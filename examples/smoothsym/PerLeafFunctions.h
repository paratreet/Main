#ifndef EXAMPLES_PER_LEAF_FNS_H_
#define EXAMPLES_PER_LEAF_FNS_H_

#include "Paratreet.h"
#include "CentroidData.h"

using PerLeafBase = paratreet::PerLeafAble<CentroidData>;
using PerLeafRef = CkReference<PerLeafBase>;

class DensityFn : public PerLeafBase {
 public:
  DensityFn(void) = default;
  DensityFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(DensityFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override;
};

class ShareDensityFn : public PerLeafBase {
 public:
  ShareDensityFn(void) = default;
  ShareDensityFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(ShareDensityFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override;
};

#endif
