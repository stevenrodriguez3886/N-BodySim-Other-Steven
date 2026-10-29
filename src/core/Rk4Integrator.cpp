#include "nbody/core/Rk4Integrator.hpp"

namespace nbody::core {

namespace {

// Builds the Body-shaped derivative vector k = f(y) for a stage: k.position
// is the stage's velocity (dx/dt) and k.velocity is the acceleration at the
// stage's positions (dv/dt), freshly evaluated by the gravity field.
// Sets each k's velocity from computeAccelerations, then overwrites
// k.position with the stage velocity; mass/id/radius are carried over so the
// gravity evaluation sees the right masses (positions/velocities from stage).
void derivative(const std::vector<Body>& stage, const GravityField& gravity,
                std::vector<Vec2>& accel, std::vector<Body>& k) {
    k = stage;  // carries mass/id/radius through the gravity evaluation
    gravity.computeAccelerations(k, accel);
    for (std::size_t i = 0; i < k.size(); ++i) {
        k[i].velocity = accel[i];
        k[i].position = stage[i].velocity;  // dx/dt = v
    }
}

// stage = y + s * k, starting from the ORIGINAL y (never accumulates).
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
    // (Task 6 merges can shrink the vector between steps).
    if (k1_.size() != n) {
        k1_.resize(n);
        k2_.resize(n);
        k3_.resize(n);
        k4_.resize(n);
        stage_.resize(n);
    }

    // Classical RK4 on y' = f(y) (plan C.3). Every stage starts from the
    // ORIGINAL y = bodies; stages never accumulate onto each other.
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
