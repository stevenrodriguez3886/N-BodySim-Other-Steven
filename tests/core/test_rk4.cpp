/// @file
/// Tests for the classic RK4 integrator: exact free-particle motion,
/// one-period return on the two-body circular orbit, and an energy-drift
/// comparison against explicit Euler on the identical setup.

#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/Rk4Integrator.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;
using Catch::Approx;

namespace {

/// @brief Total energy, same softened-potential convention as the Euler
/// tests: E = sum 1/2 m v^2 - sum_{i<j} G m_i m_j / sqrt(d^2 + eps^2).
/// @param bodies Bodies whose kinetic and pairwise potential energy is summed.
/// @param g Gravitational constant.
/// @param eps Softening length used in the pairwise potential.
/// @return The total mechanical energy of the system.
double totalEnergy(const std::vector<Body>& bodies, double g, double eps) {
    double e = 0.0;
    for (const Body& b : bodies) {
        e += 0.5 * b.mass * lengthSq(b.velocity);
    }
    const double eps2 = eps * eps;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vec2 delta = bodies[j].position - bodies[i].position;
            const double d = std::sqrt(lengthSq(delta) + eps2);
            e -= g * bodies[i].mass * bodies[j].mass / d;
        }
    }
    return e;
}

/// @brief Runs a system and reports its relative total-energy drift,
/// |E1 - E0| / |E0|, over the given number of steps.
/// @param integrator The integrator to advance the system with.
/// @param bodies The initial state (taken by value; the caller's vector is
///               untouched).
/// @param field The gravity field to step under.
/// @param dt Time step per integration step.
/// @param steps Number of integration steps to take.
/// @param g Gravitational constant matching `field`, for the energy formula.
/// @param eps Softening length matching `field`, for the energy formula.
/// @return The relative change in total energy over the run.
double energyDrift(IIntegrator& integrator, std::vector<Body> bodies,
                   const GravityField& field, double dt, int steps,
                   double g, double eps) {
    const double e0 = totalEnergy(bodies, g, eps);
    for (int i = 0; i < steps; ++i) {
        integrator.step(bodies, field, dt);
    }
    const double e1 = totalEnergy(bodies, g, eps);
    return std::abs(e1 - e0) / std::abs(e0);
}

/// @brief The two-body circular orbit: m = 1 at (+-1, 0), v = (0, +-0.5)
/// with the counter-clockwise pairing (v_y = +GM/(2*|x|) = +0.5 at x = +1),
/// the same setup as euler_energy_drift_euler. G = 1, eps = 0. Circular
/// speed derivation: for the equal-mass pair sharing the separation, the
/// orbit condition gives omega = v/r = 0.5, so T = 2*pi/omega = 4*pi.
/// @return A fresh two-body system at t = 0.
std::vector<Body> twoBodyCircular() {
    return {
        Body{1, 1.0, 0.1, Vec2{1.0, 0.0}, Vec2{0.0, 0.5}},
        Body{2, 1.0, 0.1, Vec2{-1.0, 0.0}, Vec2{0.0, -0.5}},
    };
}

}  // namespace

/// @brief An isolated body under RK4 follows the analytic straight line to
/// machine precision (margin 1e-9).
TEST_CASE("rk4_constant_velocity_exact") {
    // Zero acceleration: RK4's weighted sum collapses to x += v*dt at every
    // stage.
    Rk4Integrator rk4;
    CHECK(rk4.name() == "rk4");

    std::vector<Body> bodies = {
        Body{1, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{1.0, 0.5}},
    };
    const GravityField field{1.0, 0.05};
    const double dt = 0.01;
    for (int i = 0; i < 100; ++i) {
        rk4.step(bodies, field, dt);
    }
    const double t = 100 * dt;
    CHECK(bodies[0].position.x == Approx(1.0 * t).margin(1e-9));
    CHECK(bodies[0].position.y == Approx(0.5 * t).margin(1e-9));
    CHECK(bodies[0].velocity.x == Approx(1.0).margin(1e-9));
    CHECK(bodies[0].velocity.y == Approx(0.5).margin(1e-9));
}

/// @brief After exactly one period T = 4*pi, integrated with RK4 at
/// dt = T/4000, each body of the two-body circular orbit must return within
/// 2% relative of its start position.
TEST_CASE("rk4_orbit_period_return") {
    Rk4Integrator rk4;
    std::vector<Body> bodies = twoBodyCircular();
    const std::vector<Body> start = bodies;

    const double tPeriod = 4.0 * M_PI;  // omega = 0.5 => T = 4*pi
    const double dt = tPeriod / 4000.0;
    const GravityField field{1.0, 0.0};
    for (int i = 0; i < 4000; ++i) {
        rk4.step(bodies, field, dt);
    }

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        CHECK(std::isfinite(bodies[i].position.x));
        CHECK(std::isfinite(bodies[i].position.y));
        // 2% relative; the margin covers the y components whose start value
        // is exactly 0 (relative epsilon is vacuous there — observed
        // return error is ~1e-12).
        CHECK(bodies[i].position.x ==
              Approx(start[i].position.x).epsilon(0.02).margin(1e-9));
        CHECK(bodies[i].position.y ==
              Approx(start[i].position.y).epsilon(0.02).margin(1e-9));
    }
}

/// @brief On the same two-body setup and step count as the Euler drift test
/// (40000 steps at dt = 1/120), RK4's relative energy drift must be at least
/// 100x smaller than explicit Euler's.
TEST_CASE("rk4_beats_euler") {
    const double g = 1.0;
    const double eps = 0.0;
    const double dt = 1.0 / 120.0;
    const int steps = 40000;
    const GravityField field{g, eps};

    Rk4Integrator rk4;
    EulerIntegrator euler;
    const double driftRk4 =
        energyDrift(rk4, twoBodyCircular(), field, dt, steps, g, eps);
    const double driftEuler =
        energyDrift(euler, twoBodyCircular(), field, dt, steps, g, eps);

    WARN("energy drift after 40000 steps at dt=1/120 — rk4: "
         << driftRk4 * 100.0 << "% | euler: " << driftEuler * 100.0 << "%");

    CHECK(driftRk4 < 0.01 * driftEuler);
}
