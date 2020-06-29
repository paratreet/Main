#ifndef PARATREET_TREECANOPY_H_
#define PARATREET_TREECANOPY_H_

#include "paratreet.decl.h"
#include "templates.h"
#include "Node.h"
#include "CacheManager.h"

template<typename Data>
class CProxy_TreePiece;

template<typename Data>
class CProxy_CacheManager;

template <typename Data>
class TreeCanopy : public CBase_TreeCanopy<Data> {
private:
  SpatialNode<Data> my_sn;
  int recv_count = 0;
  int tp_index; // If -1, sits above TreePieces
  CProxy_TreePiece<Data> tp_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CProxy_Driver<Data> d_proxy;
public:
  TreeCanopy() = default;
  void reset();
  void recvProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>, DPHolder<Data>);
  void recvData(SpatialNode<Data>, int);
  void requestData(int);
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
}

template <typename Data>
void TreeCanopy<Data>::recvData(SpatialNode<Data> child, int branch_factor) {
  // Accumulate data received from TreePiece or children TreeCanopies
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

#endif // PARATREET_TREECANOPY_H_
