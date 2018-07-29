#ifndef SIMPLE_TREEELEMENT_H_
#define SIMPLE_TREEELEMENT_H_

#include "simple.decl.h"
#include "templates.h"
#include "Node.h"
#include "CacheManager.h"
#include <unordered_set>

template<typename Data>
class CProxy_TreePiece;

template<typename Data>
class CProxy_CacheManager;

template <typename Data>
class TreeElement : public CBase_TreeElement<Data> {
private:
  Data d;
  int wait_count;
  int tp_index;
  CProxy_TreePiece<Data> tp_proxy;
  std::unordered_set<int> recipients;
public:
  TreeElement();
  void reset();
  template <typename Visitor>
  void receiveData (TPHolder<Data>, Data, int);
  void requestData(CProxy_CacheManager<Data>, int);
  void print() {
    CkPrintf("[TE %d] on PE %d from tp_index %d\n", this->thisIndex, CkMyPe(), tp_index);
  }
};

extern CProxy_Main mainProxy;

template <typename Data>
TreeElement<Data>::TreeElement() {
  d = Data();
  wait_count = -1;
}

template <typename Data>
void TreeElement<Data>::reset() {
  d = Data();
  wait_count = -1;
}

template <typename Data>
void TreeElement<Data>::requestData(CProxy_CacheManager<Data> cache_manager, int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cache_manager, cm_index);
  else {
    if (!recipients.count(cm_index)) {
      cache_manager[cm_index].restoreData(std::make_pair(this->thisIndex, d));
      recipients.insert(cm_index);
    }
    else CkPrintf("DOUBLE REQUEST FOR node %d by cm %d\n", this->thisIndex, cm_index);
  }
}

template <typename Data>
template <typename Visitor>
void TreeElement<Data>::receiveData (TPHolder<Data> tp_holderi, Data di, int tp_indexi) {
  tp_proxy = tp_holderi.tp_proxy;
  tp_index = tp_indexi;
  if (wait_count == -1) wait_count = (tp_index >= 0) ? 1 : 8; // tps need 1 message
  d = d + di;
  wait_count--;
  if (wait_count == 0) {
    Visitor v (tp_proxy, -1);
    Node<Data> node (this->thisIndex, Node<Data>::Boundary, d, 0, NULL);
    v.node(&node);
  }
}

#endif // SIMPLE_TREEELEMENT_H_
