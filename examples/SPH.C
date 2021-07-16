#include "Main.h"
#include "Paratreet.h"
#include "SPHUtils.h"
//#include "PressureVisitor.h"
#include "DensityVisitor.h"

extern bool verify;
using namespace paratreet;

PARATREET_REGISTER_PER_LEAF_FN(DensityFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto& Q = leaf.data.pps.neighbors[pi];
      auto rsq = Q[0].fKey, fBall = std::sqrt(rsq);
      // sum up the density. requires 0ing of densities
      Real density = 0.;
      auto ih2 = 4.0 / rsq;  // 1/h^2
      for (int i = 0; i < Q.size(); i++) {
        auto& fDist2 = Q[i].fKey;
        auto r2 = fDist2 * ih2;
        auto rs = kernelM4(r2);
        density += rs * Q[i].mass;
      }
      Real r_cubed = rsq * fBall;
      density /= (0.125 * M_PI * r_cubed);
      auto copy_part = part;
      copy_part.density = density;
      leaf.changeParticle(pi, copy_part);
      nlc->densityFinished(part, leaf);
    }
  }));

PARATREET_REGISTER_PER_LEAF_FN(ForceFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& Q = leaf.data.pps.neighbors[pi];
      auto rsq = Q[0].fKey, fBall = std::sqrt(rsq);
      for (int i = 0; i < Q.size(); i++) {
        auto nbrIt = nlc->remote_particles.find(Q[i].pKey);
        CkAssert(nbrIt != nlc->remote_particles.end());
        doSPHCalc(leaf, pi, fBall, nbrIt->second.second);
      }
    }
  }));

PARATREET_REGISTER_PER_LEAF_FN(SymmetricForceFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto it = nlc->remote_particles.find(part.key);
      CkAssert(it != nlc->remote_particles.end());
      auto copy_part = part;
      auto&& otherAcc = it->second.second.acceleration;
      copy_part.acceleration = (otherAcc + part.acceleration) / 2;
      auto otherWork = it->second.second.pressure_dVolume;
      copy_part.pressure_dVolume = (otherWork + part.pressure_dVolume) / 2;
      leaf.changeParticle(pi, copy_part);
    }
  }));

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    neighbor_list_collector.reset(CkCallbackResumeThread());
    double start_time = CkWallTimer();
    proxy_pack.partition.template startUpAndDown<DensityVisitor>();
    CkWaitQD();
    CkPrintf("K-nearest neighbors traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    // by now, all density requests have gone out
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(DensityFn, CentroidData), // calculates density, fills requests
      CkCallbackResumeThread()
    );
    CkWaitQD();
    CkPrintf("Density calculations and sharing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(ForceFn, CentroidData),  // calculates pressure
      CkCallbackResumeThread()
    );
    CkPrintf("Pressure calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    neighbor_list_collector.shareAccelerations();
    CkWaitQD();
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(SymmetricForceFn, CentroidData),  // averages pressure
      CkCallbackResumeThread()
    );
    CkPrintf("Averaging pressures: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.0002;
  }
