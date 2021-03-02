#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <vector>

#include "Particle.h"
#include "Traverser.h"
#include "ParticleMsg.h"
#include "MultiData.h"
#include "paratreet.decl.h"
#include "LBCommon.h"

CkpvExtern(int, _lb_obj_index);
extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;

namespace paratreet {
  extern void perLeafFn(int indicator, SpatialNode<CentroidData>&);
}

template <typename Data>
struct Partition : public CBase_Partition<Data> {
  std::mutex receive_lock;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> tree_leaves;
  bool matching_decomps;

  std::unique_ptr<Traverser<Data>> traverser;
  int n_partitions;

  std::map<int, std::vector<Key>> lookup_leaf_keys;

  // filled in during traversal
  std::vector<std::vector<Node<Data>*>> interactions;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data> *cm_local;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local;

  Partition(int, CProxy_CacheManager<Data>, CProxy_Resumer<Data>, TCHolder<Data>, bool);
  Partition(CkMigrateMessage * msg){delete msg;};

  template<typename Visitor> void startDown();
  template<typename Visitor> void startUpAndDown();
  void goDown();
  void interact(const CkCallback& cb);

  void addLeaves(const std::vector<Node<Data>*>&, int);
  void receiveLeaves(std::vector<Key>, Key, int, TPHolder<Data>);
  void destroy();
  void reset();
  void perturb(TPHolder<Data>, Real, bool);
  void output(CProxy_Writer w, CkCallback cb);
  void callPerLeafFn(int indicator, const CkCallback& cb);
  void pup(PUP::er& p);
  void makeLeaves(int);
  void pauseForLB(){
    this->AtSync();
  }
  void ResumeFromSync(){
    return;
  };

private:
  struct PerturbRequest {
    bool waiting = false;
    TPHolder<Data> tp_holder;
    Real timestep = 0.;
    bool if_flush = false;
  };
  PerturbRequest saved_perturb;

private:
  void initLocalBranches();
  void erasePartition();
  void copyParticles(std::vector<Particle>& particles);
  void flush(CProxy_Reader, std::vector<Particle>&);
  void makeLeaves(const std::vector<Key>&, int);
  void doPerturb();
};

template <typename Data>
Partition<Data>::Partition(
  int np, CProxy_CacheManager<Data> cm,
  CProxy_Resumer<Data> rp, TCHolder<Data> tc_holder,
  bool matching_decomps_
  )
{
  this->usesAtSync = true;
  n_partitions = np;
  tc_proxy = tc_holder.proxy;
  r_proxy = rp;
  cm_proxy = cm;
  matching_decomps = matching_decomps_;
  initLocalBranches();
}

template <typename Data>
void Partition<Data>::initLocalBranches() {
  r_local = r_proxy.ckLocalBranch();
  r_local->part_proxy = this->thisProxy;
  r_local->resume_nodes_per_part.resize(n_partitions);
  cm_local = cm_proxy.ckLocalBranch();
  cm_local->lockMaps();
  cm_local->partition_lookup.emplace(this->thisIndex, this);
  cm_local->unlockMaps();
  r_local->cm_local = cm_local;
  cm_local->r_proxy = r_proxy;
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startDown()
{
  initLocalBranches();
  interactions.resize(leaves.size());
  traverser.reset(new DownTraverser<Data, Visitor>(leaves, *this));
  traverser->start();
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startUpAndDown()
{
  initLocalBranches();
  interactions.resize(leaves.size());
  traverser.reset(new UpnDTraverser<Data, Visitor>(*this));
  traverser->start();
}

template <typename Data>
void Partition<Data>::goDown()
{
  traverser->resumeTrav();
  if (saved_perturb.waiting && traverser->isFinished()) {
    doPerturb();
  }
}

template <typename Data>
void Partition<Data>::interact(const CkCallback& cb)
{
  if (traverser) traverser->interact();
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::addLeaves(const std::vector<Node<Data>*>& leaf_ptrs, int subtree_idx) {
  receive_lock.lock();
  tree_leaves.insert(tree_leaves.end(), leaf_ptrs.begin(), leaf_ptrs.end());
  for (auto leaf : leaf_ptrs) {
    if(matching_decomps){
      // When Subtree and Partition have the same decomp type
      // the tree structures will be identical
      // reuse leaf without checks and modifications
      leaves.push_back(leaf);
    }else{
      std::vector<Particle> leaf_particles;
      for (int pi = 0; pi < leaf->n_particles; pi++) {
        if (leaf->particles()[pi].partition_idx == this->thisIndex) {
          leaf_particles.push_back(leaf->particles()[pi]);
        }
      }
      if (leaf_particles.size() == leaf->n_particles) {
        leaves.push_back(leaf);
      }
      else {
        auto particles = new Particle [leaf_particles.size()];
        std::copy(leaf_particles.begin(), leaf_particles.end(), particles);
        auto node = treespec.ckLocalBranch()->template makeNode<Data>(
          leaf->key, leaf->depth, leaf_particles.size(), particles,
          subtree_idx, subtree_idx, true, nullptr, subtree_idx
          );
        node->type = Node<Data>::Type::Leaf;
        node->home_pe = leaf->home_pe;
        node->data = Data(node->particles(), node->n_particles);
        leaves.push_back(node);
      }
    }
  }
  receive_lock.unlock();
  cm_local->num_buckets += leaf_ptrs.size();
}

template <typename Data>
void Partition<Data>::receiveLeaves(std::vector<Key> leaf_keys, Key tp_key, int subtree_idx, TPHolder<Data> tp_holder) {
  cm_local->lockMaps();
  auto && local_tps = cm_proxy.ckLocalBranch()->local_tps;
  bool found = local_tps.find(tp_key) != local_tps.end();
  if (found) {
    cm_local->unlockMaps();
    makeLeaves(leaf_keys, subtree_idx);
  }
  else {
    lookup_leaf_keys[subtree_idx] = leaf_keys;
    auto& out = cm_local->subtree_copy_started[subtree_idx];
    bool should_request = out.empty();
    out.push_back(this->thisIndex);
    cm_local->unlockMaps();
    if (should_request) {
      tp_holder.proxy[subtree_idx].requestCopy(cm_local->thisIndex, this->thisProxy);
    }
  }
}

template <typename Data>
void Partition<Data>::makeLeaves(const std::vector<Key>& keys, int subtree_idx) {
  cm_local->lockMaps();
  std::vector<Node<Data>*> leaf_ptrs;
  for (auto && k : keys) {
    auto it = cm_local->leaf_lookup.find(k);
    CkAssert(it != cm_local->leaf_lookup.end());
    leaf_ptrs.push_back(it->second);
  }
  cm_local->unlockMaps();
  addLeaves(leaf_ptrs, subtree_idx);
}

template <typename Data>
void Partition<Data>::makeLeaves(int subtree_idx) {
  auto keys = lookup_leaf_keys[subtree_idx];
  makeLeaves(keys, subtree_idx);
}

template <typename Data>
void Partition<Data>::destroy()
{
  reset();
  erasePartition();
  this->thisProxy[this->thisIndex].ckDestroy();
}

template <typename Data>
void Partition<Data>::reset()
{
  if (saved_perturb.waiting) CkAbort("never did the perturb");
  traverser.reset();
  for (int i = 0; i < leaves.size(); i++) {
    if (leaves[i] != tree_leaves[i]) {
      leaves[i]->freeParticles();
      delete leaves[i];
    }
  }
  lookup_leaf_keys.clear();
  leaves.clear();
  tree_leaves.clear();
  interactions.clear();
}

template <typename Data>
void Partition<Data>::pup(PUP::er& p)
{
  p | n_partitions;
  p | tc_proxy;
  p | cm_proxy;
  p | r_proxy;
  p | matching_decomps;
  if (p.isUnpacking()) {
    initLocalBranches();
  }
  else {
    erasePartition();
  }
}

template <typename Data>
void Partition<Data>::erasePartition() {
  cm_local->lockMaps();
  cm_local->partition_lookup.erase(this->thisIndex);
  cm_local->unlockMaps();
}

template <typename Data>
void Partition<Data>::perturb(TPHolder<Data> tp_holder, Real timestep, bool if_flush)
{
  saved_perturb.tp_holder = tp_holder;
  saved_perturb.timestep = timestep;
  saved_perturb.if_flush = if_flush;
  if (traverser && !traverser->isFinished()) {
    saved_perturb.waiting = true;
  }
  else doPerturb();
}

template <typename Data>
void Partition<Data>::doPerturb()
{
  saved_perturb.waiting = false;
  std::vector<Particle> particles;
  copyParticles(particles);
  int size = particles.size();
  r_local->countPartitionParticles(particles.size());
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) != -1) {
    void *data = this->getObjUserData(CkpvAccess(_lb_obj_index));
    LBUserData lb_data{pt, this->thisIndex, size};
    *(LBUserData *) data = lb_data;
  }
  #endif
  for (auto && p : particles) {
    p.perturb(saved_perturb.timestep, readers.ckLocalBranch()->universe.box);
  }

  if (saved_perturb.if_flush) {
    flush(readers, particles);
  }
  else {
    auto sendParticles = [&](int dest, int n_particles, Particle* particles) {
      ParticleMsg* msg = new (n_particles) ParticleMsg(particles, n_particles);
      saved_perturb.tp_holder.proxy[dest].receive(msg);
    };
    treespec.ckLocalBranch()->getSubtreeDecomposition()->flush(particles, sendParticles);
  }
}

template <typename Data>
void Partition<Data>::flush(CProxy_Reader readers, std::vector<Particle>& particles)
{
  ParticleMsg *msg = new (particles.size()) ParticleMsg(
    particles.data(), particles.size()
    );
  readers[CkMyPe()].receive(msg);
}

template <typename Data>
void Partition<Data>::callPerLeafFn(int indicator, const CkCallback& cb)
{
  for (auto && leaf : leaves) {
    paratreet::perLeafFn(indicator, *leaf);
  }
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::copyParticles(std::vector<Particle>& particles) {
  for (auto && leaf : leaves) {
    particles.insert(particles.end(), leaf->particles(), leaf->particles() + leaf->n_particles);
  }
}

template <typename Data>
void Partition<Data>::output(CProxy_Writer w, CkCallback cb)
{
  std::vector<Particle> particles;
  copyParticles(particles);

  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  int n_total_particles = readers.ckLocalBranch()->universe.n_particles;
  int particles_per_writer = n_total_particles / CkNumPes();
  if (particles_per_writer * CkNumPes() != n_total_particles)
    ++particles_per_writer;

  int particle_idx = 0;
  while (particle_idx < particles.size()) {
    int writer_idx = particles[particle_idx].order / particles_per_writer;
    int first_particle = writer_idx * particles_per_writer;
    std::vector<Particle> writer_particles;

    while (
      particles[particle_idx].order < first_particle + particles_per_writer
      && particle_idx < particles.size()
      ) {
      writer_particles.push_back(particles[particle_idx]);
      ++particle_idx;
    }

    w[writer_idx].receive(writer_particles, cb);
  }

  if (this->thisIndex != n_partitions - 1)
    this->thisProxy[this->thisIndex + 1].output(w, cb);
}

#endif /* _PARTITION_H_ */
