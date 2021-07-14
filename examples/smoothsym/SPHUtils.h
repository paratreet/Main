#include <cmath>


namespace paratreet {

/* Standard M_4 Kernel */
/**
 * @brief kernelM4 is a scaled version of the standard M4 cubic SPH kernel
 * 
 * This returns a scaled version of the standard SPH kernel (W) of Monaghan 1992
 * The kernel W(q) is defined as:
 *  W(q) = (1/(pi h^3)) (1 - 1.5q^2 + 0.75q^3)       for 0 < q < 1
 *  W(q) = (1/(4 pi h^3)) (2 - q)^3                  for 1 < q < 2
 * for q = |dx|/h
 * 
 * NOTE: This function returns a scaled version of W(q)
 * @param ar2 = q^2 = (|dx|/h)^2
 * @return (pi h^3) W
 */
    static Real kernelM4(Real ar2) {
        Real ak;
        ak = 2.0 - sqrt(ar2);
        if (ar2 < 1.0) ak = (1.0 - 0.75*ak*ar2);
        else ak = 0.25*ak*ak*ak;
        return ak;
    }
/**
 * @brief dkernelM4 returns a scaled gradient of the M4 SPH kernel.
 * 
 * This returns a scaled version of the gradient of the Monaghan 1992 kernel
 * The kernel gradient gradW is defined as:
 *  gradW(q) = (1/(pi h^5)) (-3 + 9q/4) dx            for 0 < q < 1
 *  gradW(q) = -(1/(pi h^5)) (3/4) [(2-q)^2 /q] dx    for 1 < q < 2
 * For q = |dx|/h and dx is the particle separation (a vector)
 * NOTE: This function returns a scaled (and scalar) version of gradW
 * 
 * @param ar2 = q^2 = (|dx|/h)^2
 * @return (pi h^5/|dx|^2) (dx.dot.gradW)
 */
  static Real dkernelM4(Real ar2) {
    Real adk = sqrt(ar2);
    if (ar2 < 1.0) {
      adk = -3 + 2.25*adk;
    }
    else {
      adk = -0.75*(2.0-adk)*(2.0-adk)/adk;
    }
    return adk;
  }

}

