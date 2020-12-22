#ifndef PARATREET_GRAVITYVISITOR_H_
#define PARATREET_GRAVITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>

extern CProxy_Resumer<CentroidData> centroid_resumer;

class GravityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

private:
  // note gconst = 1
  static constexpr Real theta = 0.7;
  static constexpr int  nMinParticleNode = 6;

  static void addGravity(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // Loop unrolling with a factor of 8
    static constexpr int UNROLL_FACTOR = 8;
    const Vector3D<Real>& centroid = source.data.centroid;
    const Real& sum_mass = source.data.sum_mass;
    Vector3D<Real> diff[UNROLL_FACTOR];
    Real rsq[UNROLL_FACTOR];
    Vector3D<Real> accel[UNROLL_FACTOR];

    int n_loops = target.n_particles / UNROLL_FACTOR;
    for (int i = 0; i < n_loops; i++) {
      diff[0] = centroid - target.particles()[UNROLL_FACTOR*i].position;
      diff[1] = centroid - target.particles()[UNROLL_FACTOR*i+1].position;
      diff[2] = centroid - target.particles()[UNROLL_FACTOR*i+2].position;
      diff[3] = centroid - target.particles()[UNROLL_FACTOR*i+3].position;
      diff[4] = centroid - target.particles()[UNROLL_FACTOR*i+4].position;
      diff[5] = centroid - target.particles()[UNROLL_FACTOR*i+5].position;
      diff[6] = centroid - target.particles()[UNROLL_FACTOR*i+6].position;
      diff[7] = centroid - target.particles()[UNROLL_FACTOR*i+7].position;
      rsq[0] = diff[0].lengthSquared();
      rsq[1] = diff[1].lengthSquared();
      rsq[2] = diff[2].lengthSquared();
      rsq[3] = diff[3].lengthSquared();
      rsq[4] = diff[4].lengthSquared();
      rsq[5] = diff[5].lengthSquared();
      rsq[6] = diff[6].lengthSquared();
      rsq[7] = diff[7].lengthSquared();
      accel[0] = (rsq[0] != 0) ? diff[0] * (sum_mass / (rsq[0] * sqrt(rsq[0]))) : 0;
      accel[1] = (rsq[1] != 0) ? diff[1] * (sum_mass / (rsq[1] * sqrt(rsq[1]))) : 0;
      accel[2] = (rsq[2] != 0) ? diff[2] * (sum_mass / (rsq[2] * sqrt(rsq[2]))) : 0;
      accel[3] = (rsq[3] != 0) ? diff[3] * (sum_mass / (rsq[3] * sqrt(rsq[3]))) : 0;
      accel[4] = (rsq[4] != 0) ? diff[4] * (sum_mass / (rsq[4] * sqrt(rsq[4]))) : 0;
      accel[5] = (rsq[5] != 0) ? diff[5] * (sum_mass / (rsq[5] * sqrt(rsq[5]))) : 0;
      accel[6] = (rsq[6] != 0) ? diff[6] * (sum_mass / (rsq[6] * sqrt(rsq[6]))) : 0;
      accel[7] = (rsq[7] != 0) ? diff[7] * (sum_mass / (rsq[7] * sqrt(rsq[7]))) : 0;
      target.applyAcceleration(UNROLL_FACTOR*i, accel[0]);
      target.applyAcceleration(UNROLL_FACTOR*i+1, accel[1]);
      target.applyAcceleration(UNROLL_FACTOR*i+2, accel[2]);
      target.applyAcceleration(UNROLL_FACTOR*i+3, accel[3]);
      target.applyAcceleration(UNROLL_FACTOR*i+4, accel[4]);
      target.applyAcceleration(UNROLL_FACTOR*i+5, accel[5]);
      target.applyAcceleration(UNROLL_FACTOR*i+6, accel[6]);
      target.applyAcceleration(UNROLL_FACTOR*i+7, accel[7]);
    }

    // Process remaining particles
    int remainder = target.n_particles % UNROLL_FACTOR;
    int k = 0;
    for (int i = target.n_particles - remainder; i < target.n_particles; i++, k++) {
      diff[k] = centroid - target.particles()[i].position;
      rsq[k] = diff[k].lengthSquared();
      if (rsq[k] != 0) {
        accel[k] = diff[k] * (sum_mass / (rsq[k] * sqrt(rsq[k])));
        target.applyAcceleration(i, accel[k]);
      }
    }
  }

public:
  /// @brief We've hit a leaf: N^2 interactions between all particles
  /// in the target and node.
  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
      // addGravity(source, target);
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> accel(0.0);
      for (int j = 0; j < source.n_particles; j++) {
          Vector3D<Real> diff = source.particles()[j].position - target.particles()[i].position;
          Real rsq = diff.lengthSquared();
          if (rsq != 0) {
              accel += diff * (source.particles()[j].mass / (rsq * sqrt(rsq)));
          }
      }
      target.applyAcceleration(i, accel);
    }
  }

  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles <= nMinParticleNode) return true;
    if (Space::intersect(source.data.box, target.data.box.center(), source.data.rsq))
      return true;
    // Check if any of the target balls intersect the source volume
    /*for (int i = 0; i < target.n_particles; i++) {
      if(intersect(source.data.box, target.particles()[i].position, source.data.rsq))
        return true;
    }*/
    return false;
  }

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.data.sum_mass > 0) {
      addGravity(source, target);
    }
  }

  static bool cell(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // find the closest particle in target to source's center
    // check all eight corners of bounding box
    // return true if one corner is within total_volume of node
    // else return false

    const OrientedBox<Real> box = target.data.box;
    if (box.contains(source.data.centroid)) return true;

    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 2; k++) {
          Vector3D<Real> corner;
          corner.x = (i) ? box.greater_corner.x : box.lesser_corner.x;
          corner.y = (j) ? box.greater_corner.y : box.lesser_corner.y;
          corner.z = (k) ? box.greater_corner.z : box.lesser_corner.z;
          Vector3D<Real> dr = source.data.centroid - corner;
          Real dsq = dr.lengthSquared();
          if (theta * dsq < source.data.rsq) {
            return true;
          } 
        }
      }
    }
    return false;
  }

};

#endif //PARATREET_GRAVITYVISITOR_H_
