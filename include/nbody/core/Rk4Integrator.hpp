/// @file
/// Classical fourth-order Runge-Kutta integration: four gravity evaluations
/// per step, dramatically better energy behaviour than explicit Euler at
/// roughly four times the force cost.

#pragma once

#include <string_view>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/Vec2.hpp"

namespace nbody::core {

/// @brief Classical fourth-order Runge-Kutta integrator.
/// @details Applied to the coupled second-order system y = (x, v),
///   y' = f(y) = (v, a(x)), with four gravity evaluations per step, each at
///   freshly recomputed stage positions (stages never accumulate).
class Rk4Integrator final : public IIntegrator {
public:
    /// @brief Advance every body by dt via one classical RK4 step.
    /// @param bodies State to advance, mutated in place.
    /// @param gravity Field re-evaluated at each of the four stages.
    /// @param dt Fixed step in simulation seconds (> 0).
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;
    /// @brief Integrator name shown in the UI/CLI: "rk4".
    std::string_view name() const override { return "rk4"; }

private:
    // Scratch reused across steps. Derivative values are stored in Body-
    // shaped vectors where .position holds dy_position/dt (= v) and
    // .velocity holds dy_velocity/dt (= a), which avoids inventing a second
    // state struct.
    std::vector<Body> k1_, k2_, k3_, k4_, stage_;
    std::vector<Vec2> accel_;
};

}  // namespace nbody::core
