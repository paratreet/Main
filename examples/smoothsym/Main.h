#ifndef EXAMPLE_MAIN_H
#define EXAMPLE_MAIN_H

#include "Main.decl.h"
#include "Paratreet.h"
#include "CentroidData.h"

class ExMain: public paratreet::Main<CentroidData> {
  virtual Real getTimestep(BoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<CentroidData>&) override;
  virtual void traversalFn(BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void postIterationFn(BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

#endif
