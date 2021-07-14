/*
 * example of symmetric (i.e., gather-scatter) smoothing function.
 * This is a test of scattering values to the neighbors.
 */
#include "Main.h"
#include "Paratreet.h"
#include "SPHUtils.h"
#include "DensityVisitorDen.h"

extern bool verify;

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
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
    proxy_pack.partition.callPerLeafFn(0, CkCallbackResumeThread()); // calculates density, fills requests
    CkWaitQD();
    start_time = CkWallTimer();
    neighbor_list_collector.shareDensities();
    CkWaitQD();
    proxy_pack.partition.callPerLeafFn(1, CkCallbackResumeThread()); // averages pressure
    CkPrintf("Averaging densities: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
  }
  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.0002;
  }

  void ExMain::perLeafFn(int indicator, SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto& Q = leaf.data.pps.neighbors[pi];
      auto rsq = Q[0].fKey, fBall = std::sqrt(rsq);
      if (indicator == 0) { // sum up the density. requires 0ing of densities
        Real density = 0.;
        auto ih2 = 4.0/rsq;  // 1/h^2
        auto fNorm = 0.5*M_1_PI*ih2*sqrt(ih2);
        for (int i = 0; i < Q.size(); i++) {
          auto& fDist2 = Q[i].fKey;
          auto r2 = fDist2*ih2;
          auto rs = kernelM4(r2);
          rs *= fNorm;
          density += rs*Q[i].mass;
          auto nbrIt = nlc->remote_particles.find(Q[i].pKey);
          CkAssert(nbrIt != nlc->remote_particles.end());
          // scatter density
          nbrIt->second.second.density += rs*part.mass;
        }
        auto copy_part = part;
        copy_part.density = density;
        leaf.changeParticle(pi, copy_part);
      }
      else {
        auto it = nlc->remote_particles.find(part.key);
        CkAssert(it != nlc->remote_particles.end());
        auto copy_part = part;
        auto && otherDen = it->second.second.density;
        copy_part.density += otherDen;
        leaf.changeParticle(pi, copy_part);
      }
    }
  }
