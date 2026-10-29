#pragma once

#include <vector>

#include "nbody/core/Integrator.hpp"

namespace nbody::core {

// Explicit (forward) Euler, spec-mandated and knowingly energy-leaky
// (plan section C.2): all accelerations are evaluated once at the current
// positions, positions advance with the OLD velocities, then velocities
// advance with those accelerations.
class EulerIntegrator final : public IIntegrator {
public:
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;
    std::string_view name() const override { return "euler"; }

private:
    std::vector<Vec2> accel_;  // scratch, reused across steps
};

}  // namespace nbody::core
