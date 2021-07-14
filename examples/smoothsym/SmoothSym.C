/*
 * example of symmetric (i.e., gather-scatter) smoothing function.
 * This is a test of scattering values to the neighbors.
 */
#include "Main.h"
#include "Paratreet.h"
#include "SPHUtils.h"
#include "DensityVisitor.h"

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
    DensityFn fnDensity;
    proxy_pack.partition.callPerLeafFn(PerLeafRef(fnDensity), CkCallbackResumeThread()); // calculates density, fills requests
    CkWaitQD();
    start_time = CkWallTimer();
    neighbor_list_collector.shareDensities();
    CkWaitQD();
    ShareDensityFn fnShare;
    proxy_pack.partition.callPerLeafFn(PerLeafRef(fnShare),
        CkCallbackResumeThread()); // adds in scatter density
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

void DensityFn::perLeafFn(SpatialNode<CentroidData>& leaf,
      Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
        auto& part = leaf.particles()[pi];
        auto& Q = leaf.data.pps.neighbors[pi];
        auto rsq = Q[0].fKey, fBall = std::sqrt(rsq);
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
        nlc->densityFinished(part, leaf);  // get particle back to home?
    }
  }

void ShareDensityFn::perLeafFn(SpatialNode<CentroidData>& leaf,
                               Partition<CentroidData>* partition) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
        auto& part = leaf.particles()[pi];
        auto it = nlc->remote_particles.find(part.key);
        CkAssert(it != nlc->remote_particles.end());
        auto copy_part = part;
        auto && otherDen = it->second.second.density;
        copy_part.density += otherDen;
        leaf.changeParticle(pi, copy_part);
    }
  }
