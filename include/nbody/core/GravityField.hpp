/// @file
/// Pairwise Newtonian gravity over a body list: the single source of the
/// acceleration term in every integrator's equations of motion.

#pragma once

#include <cstddef>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Vec2.hpp"

namespace nbody::core {

/// @brief Pairwise Newtonian gravity with a softened force magnitude.
/// @details Force magnitude F = G * m_i * m_j / (d^2 + eps^2), applied along
///   the unit direction Delta / d.
/// @note The softening is deliberately NOT the Plummer (d^2+eps^2)^(3/2)
///   form: it softens the force, and the far field stays exactly Newtonian.
class GravityField {
public:
    /// @param g Gravitational constant G.
    /// @param softening Softening epsilon added to d^2 in the denominator.
    explicit GravityField(double g = 1.0, double softening = 0.05);

    /// @brief Compute every body's acceleration from pairwise gravity.
    /// @param bodies Source masses/positions; unchanged.
    /// @param outAccel Resized to bodies.size() and FULLY rewritten (no
    ///   accumulation onto stale values).
    /// @note Newton's third law is enforced pairwise: each pair (i < j)
    ///   contributes +a to i and -a to j, so the total momentum derivative
    ///   is exactly conserved.
    void computeAccelerations(const std::vector<Body>& bodies,
                              std::vector<Vec2>& outAccel) const;

private:
    double g_;          ///< gravitational constant G
    double softening_;  ///< softening epsilon of the d^2 + eps^2 denominator
    static constexpr double kZeroDist = 1e-12;  // skip pairs closer than this
};

}  // namespace nbody::core
