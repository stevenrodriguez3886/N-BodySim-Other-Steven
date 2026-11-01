/// @file
/// Classical RK4 over the coupled (x, v) system: four freshly evaluated
/// gravity stages, combined as y += (dt/6)(k1 + 2k2 + 2k3 + k4).

#include "nbody/core/Rk4Integrator.hpp"

namespace nbody::core {

namespace {

/// @brief Build the Body-shaped derivative vector k = f(y) for one stage.
/// k.position becomes the stage velocity (dx/dt) and k.velocity the
/// acceleration at the stage positions (dv/dt), freshly evaluated by the
/// gravity field. Mass/id/radius are carried over so the gravity evaluation
/// sees the right masses (positions/velocities come from the stage).
/// @param stage The stage's state (pos/vel/mass).
/// @param gravity Field evaluated at the stage positions.
/// @param accel Scratch accelerations, fully rewritten.
/// @param k Output derivative vector, filled in place.
void derivative(const std::vector<Body>& stage, const GravityField& gravity,
                std::vector<Vec2>& accel, std::vector<Body>& k) {
    k = stage;  // carries mass/id/radius through the gravity evaluation
    gravity.computeAccelerations(k, accel);
    for (std::size_t i = 0; i < k.size(); ++i) {
        k[i].velocity = accel[i];
        k[i].position = stage[i].velocity;  // dx/dt = v
    }
}

/// @brief stage = y + s * k, always starting from the ORIGINAL y (stages
/// never accumulate onto each other). Stages carry only pos/vel/mass;
/// radius/id are deliberately not propagated (gravity uses mass/position
/// only; collisions run on the world's bodies, not this stage scratch).
/// @param y The original step-start state.
/// @param k The derivative vector to scale by s.
/// @param s The stage's time offset in seconds.
/// @param stage Output stage, filled in place.
void stageFrom(const std::vector<Body>& y, const std::vector<Body>& k,
               double s, std::vector<Body>& stage) {
    for (std::size_t i = 0; i < y.size(); ++i) {
        stage[i].position = y[i].position + k[i].position * s;
        stage[i].velocity = y[i].velocity + k[i].velocity * s;
        stage[i].mass = y[i].mass;  // masses must match y for gravity
    }
}

}  // namespace

void Rk4Integrator::step(std::vector<Body>& bodies,
                        const GravityField& gravity, double dt) {
    const std::size_t n = bodies.size();
    // Resize scratch when the body count changed since the last step
    // (world merges can erase bodies between steps).
    if (k1_.size() != n) {
        k1_.resize(n);
        k2_.resize(n);
        k3_.resize(n);
        k4_.resize(n);
        stage_.resize(n);
    }

    // Classical RK4 on y' = f(y). Every stage starts from the ORIGINAL
    // y = bodies; stages never accumulate onto each other.
    derivative(bodies, gravity, accel_, k1_);

    stageFrom(bodies, k1_, 0.5 * dt, stage_);
    derivative(stage_, gravity, accel_, k2_);

    stageFrom(bodies, k2_, 0.5 * dt, stage_);
    derivative(stage_, gravity, accel_, k3_);

    stageFrom(bodies, k3_, dt, stage_);
    derivative(stage_, gravity, accel_, k4_);

    // y += (dt/6) * (k1 + 2*k2 + 2*k3 + k4), applied to BOTH position and
    // velocity with the same weighted sum.
    const double sixthDt = dt / 6.0;
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 sumPos =
            k1_[i].position + k2_[i].position * 2.0 + k3_[i].position * 2.0 +
            k4_[i].position;
        const Vec2 sumVel =
            k1_[i].velocity + k2_[i].velocity * 2.0 + k3_[i].velocity * 2.0 +
            k4_[i].velocity;
        bodies[i].position = bodies[i].position + sumPos * sixthDt;
        bodies[i].velocity = bodies[i].velocity + sumVel * sixthDt;
    }
}

}  // namespace nbody::core
