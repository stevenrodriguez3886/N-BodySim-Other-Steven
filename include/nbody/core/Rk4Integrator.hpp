#pragma once

#include <string_view>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/Vec2.hpp"

namespace nbody::core {

// Classical fourth-order Runge-Kutta (plan section C.3) applied to the
// coupled second-order system y = (x, v), y' = f(y) = (v, a(x)) with four
// gravity evaluations per step, each at freshly recomputed stage positions.
class Rk4Integrator final : public IIntegrator {
public:
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;
    std::string_view name() const override { return "rk4"; }

private:
    // Scratch reused across steps. Derivative values are stored in Body-
    // shaped vectors where .position holds dy_position/dt (= v) and
    // .velocity holds dy_velocity/dt (= a). This avoids inventing a second
    // struct (plan B.2).
    std::vector<Body> k1_, k2_, k3_, k4_, stage_;
    std::vector<Vec2> accel_;
};

}  // namespace nbody::core
