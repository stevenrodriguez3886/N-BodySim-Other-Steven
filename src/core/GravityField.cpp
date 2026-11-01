/// @file
/// O(n^2) pairwise gravity loop with Newton's third law applied symmetrically
/// per pair, a softened d^2 + eps^2 denominator, and a coincidence guard.

#include "nbody/core/GravityField.hpp"

#include <cmath>

namespace nbody::core {

GravityField::GravityField(double g, double softening)
    : g_(g), softening_(softening) {}

void GravityField::computeAccelerations(const std::vector<Body>& bodies,
                                        std::vector<Vec2>& outAccel) const {
    const std::size_t n = bodies.size();
    outAccel.resize(n);  // shrinks or grows to exactly n
    // Full rewrite: resize alone keeps stale values in pre-existing entries.
    for (std::size_t i = 0; i < n; ++i) {
        outAccel[i] = Vec2{0.0, 0.0};
    }
    const double eps2 = softening_ * softening_;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const Vec2 delta = bodies[j].position - bodies[i].position;
            const double d2 = lengthSq(delta);
            const double d = std::sqrt(d2);
            if (d < kZeroDist) {
                continue;  // direction undefined; symmetric coincidence cancels
            }
            // a_i += G*m_j*Delta/((d^2+eps^2)*d), a_j -= G*m_i*Delta/((d^2+eps^2)*d)
            const double inv = g_ / ((d2 + eps2) * d);
            outAccel[i] += delta * (inv * bodies[j].mass);
            outAccel[j] -= delta * (inv * bodies[i].mass);
        }
    }
}

}  // namespace nbody::core
