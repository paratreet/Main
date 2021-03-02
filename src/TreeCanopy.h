#ifndef PARATREET_TREECANOPY_H_
#define PARATREET_TREECANOPY_H_

#include "paratreet.decl.h"
#include "templates.h"
#include "Node.h"
#include "CacheManager.h"
#include "LBCommon.h"

CkpvExtern(int, _lb_obj_index);
template<typename Data>
class CProxy_Subtree;

template<typename Data>
class CProxy_CacheManager;

template <typename Data>
class TreeCanopy : public CBase_TreeCanopy<Data> {
private:
  SpatialNode<Data> my_sn;
  int recv_count = 0;
  int tp_index; // If -1, sits above Subtrees
  CProxy_Subtree<Data> tp_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CProxy_Driver<Data> d_proxy;
public:
  TreeCanopy() = default;
  TreeCanopy(CkMigrateMessage * msg){
    delete msg;
  };
  void reset();
  void recvProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>, DPHolder<Data>);
  void recvData(SpatialNode<Data>, int);
  void requestData(int);
  void pup(PUP::er& p);
};

template <typename Data>
void TreeCanopy<Data>::reset() {
  Data empty_data;
  my_sn = SpatialNode<Data>(empty_data, 0, false, nullptr, -1);
  recv_count = 0;
}

template <typename Data>
void TreeCanopy<Data>::recvProxies(TPHolder<Data> tp_holder, int tp_index_,
                                   CProxy_CacheManager<Data> cm_proxy_, DPHolder<Data> dp_holder) {
  tp_proxy = tp_holder.proxy;
  tp_index = tp_index_;
  cm_proxy = cm_proxy_;
  d_proxy = dp_holder.proxy;

  this->setMigratable(false);

  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) != -1) {
    void *data = this->getObjUserData(CkpvAccess(_lb_obj_index));
    LBUserData lb_data{cp};
    *(LBUserData *) data = lb_data;
    //CkPrintf("[C %d] \n", this->thisIndex);
  }
  #endif
}

template <typename Data>
void TreeCanopy<Data>::recvData(SpatialNode<Data> child, int branch_factor) {
  // Accumulate data received from Subtree or children TreeCanopies
  my_sn.data += child.data;
  my_sn.n_particles += child.n_particles;
  my_sn.depth = child.depth - 1;

  // If data from all children has been received, send the accumulated data
  // to Driver and to the parent TreeCanopy
  if (++recv_count == branch_factor) {
    d_proxy.recvTC(std::make_pair(this->thisIndex, my_sn));

    if (this->thisIndex == 1) {
      //cm_proxy.restoreData(std::make_pair(1, data));
    } else {
      this->thisProxy[this->thisIndex / branch_factor].recvData(my_sn, branch_factor);
    }

    reset();
  }
}

template <typename Data>
void TreeCanopy<Data>::requestData(int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cm_index);
  else cm_proxy[cm_index].restoreData(std::make_pair(this->thisIndex, my_sn));
}

template <typename Data>
void TreeCanopy<Data>::pup(PUP::er& p) {
  p | tp_proxy;
  p | tp_index;
  p | cm_proxy;
  p | d_proxy;
  p | my_sn;
  p | recv_count;
}
#endif // PARATREET_TREECANOPY_H_
