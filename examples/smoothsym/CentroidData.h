#ifndef PARATREET_CENTROIDDATA_H_
#define PARATREET_CENTROIDDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "Particle.h"
#include "ParticleComp.h"
#include "OrientedBox.h"

struct CentroidData {
  Vector3D<Real> moment;
  Real sum_mass;
  Vector3D<Real> centroid; // too slow to compute this on the fly
  Real max_rad = 0.0;
  Real size_sm;
  struct PerParticleStruct {
    PerParticleStruct() {}
    PerParticleStruct& operator=(const PerParticleStruct&) {return *this;}
    PerParticleStruct(const PerParticleStruct&) {}
    std::vector< CkVec<pqSmoothNode> > neighbors; // Neighbor list for knn search
    std::vector<std::pair<Real, Particle>> best_dt;
  };
  PerParticleStruct pps;
  OrientedBox<Real> box;
  int count;
  Real rsq;                     ///< Opening radius
  static constexpr const Real opening_geometry_factor_squared = 4.0 / 3.0;
  static constexpr const Real theta = 0.7;

  CentroidData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0), rsq(0.) {}

  /// Construct centroid from particles.
  CentroidData(const Particle* particles, int n_particles, int depth) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    getRadius();
    centroid = moment / sum_mass;
    count = n_particles;
    auto tmp_box = fixSize(depth);
  }

    /// The size of a node needs to be non-zero before calculating
    /// multipole moments.  Use a heuristic based on the size of the
    /// simulation to guess a non-zero size.
    OrientedBox<Real> fixSize(int depth) {
        auto tmp_box = box;
        if(size_sm == 0.0) { // Box has to have finite size for scaled multipole
            // calculations.
            auto size_universe = (thread_state_holder.ckLocalBranch()->universe.box.size()).length();
            // Approximate size by assuming a binary represented Oct tree.
            size_sm = size_universe/pow(2.0, depth/3);
            // size_sm = 1.0;  // Stopgap for now.
            tmp_box.grow(box.center() + size_sm);
        }
        return tmp_box;
    }
    
    
  
/// Calculate gravity opening radius and linear size based on box size.
  void getRadius() {
    Vector3D<Real> delta1 = centroid - box.lesser_corner;
    Vector3D<Real> delta2 = box.greater_corner - centroid;
    delta1.x = (delta1.x > delta2.x ? delta1.x : delta2.x);
    delta1.y = (delta1.y > delta2.y ? delta1.y : delta2.y);
    delta1.z = (delta1.z > delta2.z ? delta1.z : delta2.z);
    rsq = delta1.lengthSquared();
    rsq *= opening_geometry_factor_squared / (theta * theta);
    size_sm = 0.5*(box.size()).length();
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    centroid = moment / sum_mass;
    box.grow(cd.box);
    getRadius();
    auto tmp_box = fixSize(0); // XXX needs fixing
    count += cd.count;
    return *this;
  }

  CentroidData& operator=(const CentroidData&) = default;

  void widen() {
    pps.neighbors.resize(count);
    pps.best_dt.resize(count, std::make_pair(std::numeric_limits<Real>::max(), Particle{}));
  }

  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
    p | centroid;
    p | box;
    p | count;
    p | rsq;
    p | max_rad;
    p | size_sm;
  }

};

#endif // PARATREET_CENTROID_H_
