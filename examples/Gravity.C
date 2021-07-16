#include "Main.h"
#include "GravityVisitor.h"

extern bool verify;
extern bool dual_tree;
extern bool periodic;

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (dual_tree) proxy_pack.subtree.startDual<GravityVisitor<0,0,0>>();
    if (dual_tree && periodic) CkAbort("Not sure about this -- dual_tree and periodic both set");
    proxy_pack.partition.template startDown<GravityVisitor<0,0,0>>();
    if (periodic) {
      CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,-1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,-1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,-1,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,0,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,0,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,0,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<-1,1,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,-1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,-1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,-1,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,0,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,0,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<0,1,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,-1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,-1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,-1,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,0,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,0,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,0,1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,1,-1>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,1,0>>(); CkWaitQD();
      proxy_pack.partition.template startDown<GravityVisitor<1,1,1>>(); CkWaitQD();
    }
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
    else if (periodic) {
      // do ewald
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    return universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  }
