#pragma once

#include <cstddef>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Vec2.hpp"

namespace nbody::core {

// Pairwise Newtonian gravity with the spec's softened force magnitude
// F = G * m_i * m_j / (d^2 + eps^2), applied along the unit direction
// Delta / d (plan section C.1; NOT the Plummer (d^2+eps^2)^(3/2) form).
class GravityField {
public:
    explicit GravityField(double g = 1.0, double softening = 0.05);

    // Writes bodies.size() accelerations into outAccel (resized as needed;
    // every entry is fully rewritten). Newton's third law enforced: each
    // pair (i<j) contributes +a to i and -a to j, so total momentum of the
    // derivative is exactly conserved.
    void computeAccelerations(const std::vector<Body>& bodies,
                              std::vector<Vec2>& outAccel) const;

private:
    double g_;
    double softening_;
    static constexpr double kZeroDist = 1e-12;  // skip pairs closer than this
};

}  // namespace nbody::core
