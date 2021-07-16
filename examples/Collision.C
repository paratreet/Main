#include "Main.h"
#include "Paratreet.h"
#include "CollisionVisitor.h"
#include "GravityVisitor.h"
#include <climits>

extern bool verify;
extern int iter_start_collision;

PARATREET_REGISTER_PER_LEAF_FN(CropFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      if (part.position.lengthSquared() > 45 || part.position.z < -0.2 ||
          part.position.z > 0.2) {
        partition->deleteParticleOfOrder(part.order);
      }
    }
  }));

PARATREET_REGISTER_PER_LEAF_FN(CollisionResolveFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto best_dt = leaf.data.pps.best_dt[pi].first;

      if (best_dt < 0.01570796326) {
        auto& partB = leaf.data.pps.best_dt[pi].second;
        auto& posA = part.position;
        auto& posB = partB.position;
        auto& velA = part.velocity;
        auto& velB = partB.velocity;
        CkPrintf(
            "deleting particles of order %d and %d that collide at dt %lf. "
            "First has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf). "
            "Second has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf)\n",
            part.order, partB.order, partition->time_advanced + best_dt, posA.x,
            posA.y, posA.z, velA.x, velA.y, velA.z, posB.x, posB.y, posB.z,
            velB.x, velB.y, velB.z);
        partition->deleteParticleOfOrder(part.order);
        partition->thisProxy[partB.partition_idx].deleteParticleOfOrder(
            partB.order);
      }
    }
  }));

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    proxy_pack.partition.template startDown<GravityVisitor<0,0,0>>();
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(CropFn, CentroidData),
      CkCallbackResumeThread()
    );
    if (iter % 10000 == 0) paratreet::outputTipsy(universe, proxy_pack.partition);
    if (iter >= iter_start_collision) {
      proxy_pack.cache.resetCachedParticles(CkCallbackResumeThread());
      double start_time = CkWallTimer();
      proxy_pack.partition.template startDown<CollisionVisitor>();
      CkWaitQD();
      CkPrintf("Collision traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      // Collision is a little funky because were going to edit the mass and position of particles after a collision
      // that means were going to set the mass and position to whatever we want
      // first get minimum distance of any two particles
      start_time = CkWallTimer();
      proxy_pack.partition.callPerLeafFn(
        PARATREET_PER_LEAF_FN(CollisionResolveFn, CentroidData),
        CkCallbackResumeThread()
      );
      CkWaitQD();
      CkPrintf("Collision calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.01570796326;
  }
