#include "nbody/core/EulerIntegrator.hpp"

namespace nbody::core {

void EulerIntegrator::step(std::vector<Body>& bodies,
                           const GravityField& gravity, double dt) {
    // One acceleration evaluation at the CURRENT positions (explicit Euler,
    // plan C.2). computeAccelerations resizes accel_ to exactly
    // bodies.size() and fully rewrites every entry, which also covers the
    // edge case of the body count changing between steps (Task 6 merges).
    gravity.computeAccelerations(bodies, accel_);

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        Body& body = bodies[i];
        // Order matters: position uses the PRE-UPDATE velocity (velocity is
        // only modified on the next statement, so in-place is safe here).
        // Semi-implicit Euler (v first, then x with the new v) is NOT to be
        // substituted — it would defeat the Euler-vs-RK4 demo.
        body.position += body.velocity * dt;
        body.velocity += accel_[i] * dt;
    }
}

}  // namespace nbody::core
