/// @file
/// Explicit (forward) Euler integration: the cheap, knowingly energy-leaky
/// scheme, kept as the foil for the RK4 integrator's energy-behaviour demo.

#pragma once

#include <vector>

#include "nbody/core/Integrator.hpp"

namespace nbody::core {

/// @brief Explicit (forward) Euler integrator — knowingly energy-leaky.
/// @details All accelerations are evaluated once at the current positions;
///   positions advance with the OLD velocities, then velocities advance with
///   those accelerations. The energy drift versus higher-order schemes is
///   the point of the euler-vs-rk4 demo, not a bug to fix.
class EulerIntegrator final : public IIntegrator {
public:
    /// @brief Advance every body by dt with a single acceleration evaluation.
    /// @param bodies Mutated in place: x += v*dt, then v += a*dt.
    /// @param gravity Field providing accelerations at the current positions.
    /// @param dt Fixed time step in simulation seconds (> 0).
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;
    /// @brief Integrator name shown in the UI/CLI: "euler".
    std::string_view name() const override { return "euler"; }

private:
    std::vector<Vec2> accel_;  ///< scratch, reused across steps
};

}  // namespace nbody::core
