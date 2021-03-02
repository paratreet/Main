/**
 * \addtogroup CkLdb
*/
/*@{*/

#ifndef _PREFIXLB_H_
#define _PREFIXLB_H_

#include "CentralLB.h"
#include "PrefixLB.decl.h"

class minheap;
class maxheap;

void CreatePrefixLB();
BaseLB *AllocatePrefixLB();

class PrefixLB : public CBase_PrefixLB {
protected:
  computeInfo *computes;
  processorInfo *processors;
  minHeap *pes;
  maxHeap *computesHeap;
  int P;
  int numComputes;
  double averageLoad;

  double overLoad;

public:
  PrefixLB(const CkLBOptions &);
  PrefixLB(CkMigrateMessage *m):CBase_PrefixLB(m) { lbname = (char *)"PrefixLB"; }
  void work(LDStats* stats);
private:
  bool QueryBalanceNow(int step) { return true; }

protected:
/*
  void create(LDStats* stats, int count);
  void assign(computeInfo *c, int p);
  void assign(computeInfo *c, processorInfo *p);
  void deAssign(computeInfo *c, processorInfo *pRec);
  void computeAverage();
  double computeMax();
  int refine();
*/
};

#endif /* _REFINELB_H_ */

/*@}*/
