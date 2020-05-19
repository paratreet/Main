#ifndef PARATREET_TREEPIECE_H_
#define PARATREET_TREEPIECE_H_

#include "paratreet.decl.h"
#include "common.h"
#include "templates.h"
#include "ParticleMsg.h"
#include "Node.h"
#include "Utility.h"
#include "Reader.h"
#include "CacheManager.h"
#include "Resumer.h"
#include "Traverser.h"
#include "Driver.h"
#include "OrientedBox.h"

#include <queue>
#include <vector>
#include <fstream>

extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int decomp_type;
extern Decomposition* decomposition;
extern int tree_type;
extern CProxy_Main mainProxy;

template <typename Data>
class TreePiece : public CBase_TreePiece<Data> {
public:
  std::vector<Particle> particles, incoming_particles;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> empty_leaves;

  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;

  Key tp_key; // Should be a prefix of all particle keys underneath this node
  Node<Data>* global_root; // Root of the global tree structure
  Node<Data>* local_root; // Root node of this TreePiece, TreeCanopies sit above this node

  Traverser<Data>* traverser;
  std::vector<std::pair<Node<Data>*, int>> local_travs;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data>* cm_local;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local;

  std::vector<std::vector<Node<Data>*>> interactions;
  bool cache_init;
  std::vector<Particle> flushed_particles; // For debugging

  TreePiece(const CkCallback&, int, int, TCHolder<Data>,
    CProxy_Resumer<Data>, CProxy_CacheManager<Data>, DPHolder<Data>);
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void buildTree();
  bool recursiveBuild(Node<Data>*, bool);
  void populateTree();
  inline void initCache();
  void requestNodes(Key, int);
  template<typename Visitor> void startDown();
  template<typename Visitor> void startUpAndDown();
  template<typename Visitor> void startDual(Key*, int);
  void goDown(Key);
  void processLocal(const CkCallback&);
  void interact(const CkCallback&);
  void print(Node<Data>*);
  void perturb (Real timestep, bool);
  void flush(CProxy_Reader);

  // For debugging
  void checkParticlesChanged(const CkCallback& cb) {
    bool result = true;
    if (particles.size() != flushed_particles.size()) {
      result = false;
      this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
      return;
    }
    for (int i = 0; i < particles.size(); i++) {
      if (!(particles[i] == flushed_particles[i])) {
        result = false;
        break;
      }
    }
    this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
  }
};

template <typename Data>
TreePiece<Data>::TreePiece(const CkCallback& cb, int n_total_particles_,
    int n_treepieces_, TCHolder<Data> tc_holder, CProxy_Resumer<Data> r_proxy_,
    CProxy_CacheManager<Data> cm_proxy_, DPHolder<Data> dp_holder) {
  n_total_particles = n_total_particles_;
  n_treepieces = n_treepieces_;
  particle_index = 0;

  tc_proxy = tc_holder.proxy;
  r_proxy = r_proxy_;
  r_local = r_proxy.ckLocalBranch();
  r_local->tp_proxy = this->thisProxy;
  cm_proxy = cm_proxy_;
  cm_local = cm_proxy.ckLocalBranch();
  r_local->cm_local = cm_local;
  cm_local->r_proxy = r_proxy;

  cache_init = false;

  n_expected = treespec.ckLocalBranch()->getDecomposition()->
      getNumExpectedParticles(n_total_particles, n_treepieces, this->thisIndex);

  if (decomp_type == OCT_DECOMP || decomp_type == SFC_DECOMP) {
    tp_key = ((SfcDecomposition*)treespec.ckLocalBranch()->getDecomposition())->getTpKey(this->thisIndex);
  }

  // Create TreeCanopies and send proxies
  auto sendProxy = [&](int dest, int tp_index) {
    tc_proxy[dest].recvProxies(TPHolder<Data>(this->thisProxy), tp_index, cm_proxy, dp_holder);
  };

  if (tree_type == OCT_TREE) {
    OctTree::buildCanopy(this->thisIndex, sendProxy);
  }

  global_root = nullptr;
  local_root = nullptr;

  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::receive(ParticleMsg* msg) {
  // Copy particles to local vector
  // TODO: Remove memcpy by just storing the pointer to msg->particles
  // and using it in tree build
  int initial_size = incoming_particles.size();
  incoming_particles.resize(initial_size + msg->n_particles);
  std::memcpy(&incoming_particles[initial_size], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
  delete msg;
}

template <typename Data>
void TreePiece<Data>::check(const CkCallback& cb) {
  if (n_expected != incoming_particles.size()) {
    CkAbort("[TP %d] ERROR! Only %zu particles out of %d received",
        this->thisIndex, particles.size(), n_expected);
  }

  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::triggerRequest() {
  readers.ckLocalBranch()->request(this->thisProxy, this->thisIndex, n_expected);
}

template <typename Data>
void TreePiece<Data>::buildTree() {
  int n_particles_saved = particles.size();
  int n_particles_received = incoming_particles.size();

  // Copy over received particles
  particles.resize(n_particles_saved + n_particles_received);
  std::copy(incoming_particles.begin(), incoming_particles.end(), particles.begin() + n_particles_saved);
  incoming_particles.clear();

  // Sort particles
  std::sort(particles.begin(), particles.end());

  // Clear existing data
  leaves.clear();
  empty_leaves.clear();
  local_travs.clear();

  // Create global root and build local tree recursively
#if DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  global_root = new Node<Data>(1, 0, particles.size(), &particles[0], 0, n_treepieces - 1, nullptr);
  recursiveBuild(global_root, false);

  // Initialize interactions vector: filled in during traversal
  interactions = std::vector<std::vector<Node<Data>*>>(leaves.size());

  // Populate the tree structure (including TreeCanopy)
  populateTree();

  // Initialize cache
  initCache();
}

template <typename Data>
bool TreePiece<Data>::recursiveBuild(Node<Data>* node, bool saw_tp_key) {
#if DEBUG
  //CkPrintf("[Level %d] created node 0x%" PRIx64 " with %d particles\n",
    //  node->depth, node->key, node->n_particles);
#endif
  // store reference to splitters
  //static std::vector<Splitter>& splitters = readers.ckLocalBranch()->splitters;

  // Check if we are inside the subtree rooted at the treepiece's key
  if (!saw_tp_key) {
    saw_tp_key = (node->key == tp_key);
    if (saw_tp_key) local_root = node;
  }

  bool is_light = (node->n_particles <= ceil(BUCKET_TOLERANCE * max_particles_per_leaf));
  bool is_prefix = Utility::isPrefix(node->key, tp_key);

  if (saw_tp_key) {
    // If we are under the TP key, we can stop going deeper if node is light
    if (is_light) {
      if (node->n_particles == 0) {
        node->type = Node<Data>::EmptyLeaf;
        empty_leaves.push_back(node);
      }
      else {
        node->type = Node<Data>::Leaf;
        leaves.push_back(node);
      }
      return true;
    }
  } else { // Still on the way to TP, or we could have diverged
    if (!is_prefix) {
      // Diverged, should be marked as are mote node
      node->type = Node<Data>::Remote;

      return false;
    }
  }

  // For all other cases, we go deeper
  /* XXX: For SFC?
  int owner_start = node->owner_tp_start;
  int owner_end = node->owner_tp_end;
  bool single_owner = (owner_start == owner_end);

  if (is_light) {
    if (saw_tp_key) {
      // we can make the node a local leaf
      if (node->n_particles == 0)
        node->type = Node<Data>::EmptyLeaf;
      else
        node->type = Node<Data>::Leaf;

        return true;
    }
    else if (!is_prefix) {
      // we have diverged from the path to the subtree rooted at the treepiece's key
      // so designate as remote
      node->type = Node<Data>::Remote;

      CkAssert(node->n_particles == 0);
      CkAssert(node->n_children == 0);

      if (single_owner) {
        int assigned = splitters[owner_start].n_particles;
        if (assigned == 0) {
          node->type = Node<Data>::RemoteEmptyLeaf;
        }
        else if (assigned <= BUCKET_TOLERANCE * max_particles_per_leaf) {
          node->type = Node<Data>::RemoteLeaf;
        }
        else {
          node->type = Node<Data>::Remote;
          node->n_children = BRANCH_FACTOR;
        }
      }
      else {
        node->type = Node<Data>::Remote;
        node->n_children = BRANCH_FACTOR;
      }

      return false;
    }
    else {
      CkAbort("Error: can a light node be an internal node (above a TP root)?");
    }
  }
  */

  // Create children
  node->n_children = BRANCH_FACTOR;
  Key child_key = node->key << LOG_BRANCH_FACTOR;
  int start = 0;
  int finish = start + node->n_particles;
  int non_local_children = 0;

  for (int i = 0; i < node->n_children; i++) {
    Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1);

    // Find number of particles in child
    int first_ge_idx;
    if (i < node->n_children - 1) {
      first_ge_idx = Utility::binarySearchGE(sibling_splitter, node->particles, start, finish);
    } else {
      first_ge_idx = finish;
    }
    int n_particles = first_ge_idx - start;

    /*
    // find owner treepieces of child
    int child_owner_start = owner_start;
    int child_owner_end;
    if (single_owner) {
      child_owner_end = child_owner_start;
    }
    else {
      if (i < node->n_children - 1) {
        int first_ge_tp = Utility::binarySearchGE(sibling_splitter, &splitters[0], owner_start, owner_end);
        child_owner_end = first_ge_tp - 1;
        owner_start = first_ge_tp;
      }
      else {
        child_owner_end = owner_end;
      }
    }
    */

    // Create child and store in vector
    Node<Data>* child = new Node<Data>(child_key, node->depth + 1, n_particles,
        node->particles + start, 0, n_treepieces - 1, node, this->thisIndex);
    /*
    Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start,
        n_particles, child_owner_start, child_owner_end, node);
    */
    node->children[i].store(child);

    // Recursive tree build
    bool local = recursiveBuild(child, saw_tp_key);
    if (!local) non_local_children++;

    start = first_ge_idx;
    child_key++;
  }

  if (non_local_children == 0) {
    // All children are local, this is an internal node
    node->type = Node<Data>::Internal;
  }
  else {
    // Some or all children are remote, this is a boundary node
    node->type = Node<Data>::Boundary;
    node->tp_index = -1;
  }

  return (non_local_children == 0);
}

template <typename Data>
void TreePiece<Data>::populateTree() {
  // Populates the global tree structure by going up the tree
  std::queue<Node<Data>*> going_up;

  for (auto leaf : leaves) {
    leaf->data = Data(leaf->particles, leaf->n_particles);
    going_up.push(leaf);
  }

  if (!leaves.size()) going_up.push(local_root);
  else for (auto empty_leaf : empty_leaves) going_up.push(empty_leaf);

  while (going_up.size()) {
    Node<Data>* node = going_up.front();
    going_up.pop();
    if (node->key == tp_key) {
      // We are at the root of the TreePiece, send accumulated data to
      // parent TreeCanopy
      tc_proxy[tp_key >> LOG_BRANCH_FACTOR].recvData(node->data);
    } else {
      // Add this node's data to the parent, and add parent to the queue
      // if all children have contributed
      Node<Data>* parent = node->parent;
      parent->data += node->data;
      parent->wait_count--;
      if (parent->wait_count == 0) going_up.push(parent);
    }
  }
}

template <typename Data>
void TreePiece<Data>::initCache() {
  if (!cache_init) {
    cm_local->connect(local_root, false);
    local_root->parent->children[tp_key % BRANCH_FACTOR].store(nullptr);
    global_root->triggerFree();
    cache_init = true;
  }
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startDown() {
  traverser = new DownTraverser<Data, Visitor>(this);
  goDown(1);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startUpAndDown() {
  if (!leaves.size()) return;
  traverser = new UpnDTraverser<Data, Visitor>(this);
  for (auto leaf : leaves) goDown(leaf->key);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startDual(Key* keys_ptr, int n) {
  std::vector<Key> keys (keys_ptr, keys_ptr + n);
  traverser = new DualTraverser<Data, Visitor>(this, keys);
  for (auto key : keys) goDown(key);
  // root needs to be the root of the searched tree, not the searching tree
}

template <typename Data>
void TreePiece<Data>::requestNodes(Key key, int cm_index) {
  Node<Data>* node = local_root->findNode(key);
  if (!node) CkPrintf("null found for key %lu on tp %d\n", key, this->thisIndex);
  cm_local->serviceRequest(node, cm_index);
}

template <typename Data>
void TreePiece<Data>::goDown(Key new_key) {
  traverser->traverse(new_key);
}

template <typename Data>
void TreePiece<Data>::processLocal(const CkCallback& cb) {
  traverser->processLocal();
  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::interact(const CkCallback& cb) {
  traverser->interact();
  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::perturb (Real timestep, bool if_flush) {
  if (if_flush) {
    for (auto leaf : leaves) {
      for (int i = 0; i < leaf->n_particles; i++) {
        leaf->particles[i].perturb(timestep, leaf->sum_forces[i], readers.ckLocalBranch()->universe.box);
      }
    }
    flush(readers);
    return;
  }

  for (int i = 0; i < leaves.size(); i++) {
    for (int j = 0; j < leaves[i]->n_particles; j++) {
//      CkPrintf("sum forces y %lf\n", leaves[i]->sum_forces[j].y);
    }
  }

  // SUM FORCES IS NAN

  std::vector<Particle> in_particles;
  std::map<int, std::vector<Particle>> out_particles;
  std::vector<int> remainders;
  Key temp = tp_key;
  while (temp >= BRANCH_FACTOR) {
    remainders.push_back(temp % BRANCH_FACTOR);
    temp /= BRANCH_FACTOR;
  }
  OrientedBox<Real> tp_box = readers.ckLocalBranch()->universe.box;
  for (int i = remainders.size()-1; i >= 0; i--) {
    for (int dim = 0; dim < 3; dim++) {
      if (remainders[i] & (1 << (2-dim))) tp_box.lesser_corner[dim] = tp_box.center()[dim];
      else tp_box.greater_corner[dim] = tp_box.center()[dim];
    }
  }

  // calculate bounding box of TP
  for (auto leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      Particle& particle = leaf->particles[i];
      Vector3D<Real> old_position = particle.position;
      particle.perturb(timestep, leaf->sum_forces[i], readers.ckLocalBranch()->universe.box);
      //CkPrintf("magitude of displacement = %lf\n", (old_position - leaf->particles[i].position).length());
      //CkPrintf("total centroid is (%lf, %lf, %lf)\n", global_root->data.getCentroid().x, global_root->data.getCentroid().y, global_root->data.getCentroid().z);
      OrientedBox<Real> curr_box = tp_box;
      Node<Data>* node = local_root;
      int remainders_index = 0;
      while (!curr_box.contains(particle.position)) {
        //CkPrintf("not under umbrella of node %d with volume %lf\n", node->key, curr_box.volume());
        if (node->parent == nullptr) CkPrintf("point (%lf, %lf, %lf) has force (%lf, %lf, %lf) and old position (%lf, %lf, %lf)\n",
                particle.position.x, particle.position.y, particle.position.z,
		leaf->sum_forces[i].x / .001, leaf->sum_forces[i].y / .001, leaf->sum_forces[i].z / .001,
		old_position.x, old_position.y, old_position.z);
	Vector3D<Real> new_point = 2 * curr_box.greater_corner - curr_box.lesser_corner;
        if (remainders[remainders_index] & 4) new_point.x = 2 * curr_box.lesser_corner.x - curr_box.greater_corner.x;
        if (remainders[remainders_index] & 2) new_point.y = 2 * curr_box.lesser_corner.y - curr_box.greater_corner.y;
        if (remainders[remainders_index] & 1) new_point.z = 2 * curr_box.lesser_corner.z - curr_box.greater_corner.z;
        curr_box.grow(new_point);
        remainders_index++;
        node = node->parent;
      }
      //if (node->tp_index >= 0) CkPrintf("node tp_index %d\n", node->tp_index);
      while (node->tp_index < 0) {
        int child = 0;
        Vector3D<Real> mean = curr_box.center();
        for (int dim = 0; dim < 3; dim++) {
          if (particle.position[dim] > mean[dim]) {
            child |= (1 << (2 - dim));
            curr_box.lesser_corner[dim] = mean[dim];
          }
          else curr_box.greater_corner[dim] = mean[dim];
        }
        if (node->type == Node<Data>::RemoteAboveTPKey || node->type == Node<Data>::Remote) {
          CkAbort("flush period too large for initial particle velocities");
        }
        Node<Data>* the_child = node->children[child].load();
        if (the_child == nullptr) std::cout << "node has type " << node->type << std::endl;
        node = node->children[child].load();
        if (!node) CkPrintf("node not legit\n");
      }

      if (node == local_root) in_particles.push_back(particle);
      else {
        std::vector<Particle>& particle_vec = out_particles[node->tp_index];
        particle_vec.push_back(particle);
      }
    }
  }
  for (auto it = out_particles.begin(); it != out_particles.end(); it++) {
    ParticleMsg* msg = new (it->second.size()) ParticleMsg (it->second.data(), it->second.size());
    this->thisProxy[it->first].receive(msg);
  } 
  particles = in_particles;
}

template <typename Data>
void TreePiece<Data>::flush(CProxy_Reader readers) {
  // debug
  flushed_particles.resize(0);
  flushed_particles.insert(flushed_particles.end(), particles.begin(), particles.end());

  ParticleMsg *msg = new (particles.size()) ParticleMsg(particles.data(), particles.size());
  readers[CkMyPe()].receive(msg);
  particles.resize(0);
  particle_index = 0;
}

template <typename Data>
void TreePiece<Data>::print(Node<Data>* node) {
  ostringstream oss;
  oss << "tree." << this->thisIndex << ".dot";
  ofstream out(oss.str().c_str());
  CkAssert(out.is_open());
  out << "digraph tree" << this->thisIndex << "{" << endl;
  node->dot(out);
  out << "}" << endl;
  out.close();
}

#endif // PARATREET_TREEPIECE_H_
