#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;
using Catch::Approx;

namespace {

// Total energy of the system: E = sum 1/2 m v^2 - sum_{i<j} G m_i m_j /
// sqrt(d^2 + eps^2), matching the sim's softened-potential convention.
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

}  // namespace

TEST_CASE("euler_free_particle_straight_line") {
    // One isolated body: gravity contributes zero acceleration, so explicit
    // Euler must reproduce the straight-line trajectory exactly.
    EulerIntegrator euler;
    CHECK(euler.name() == "euler");

    std::vector<Body> bodies = {
        Body{1, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{1.0, 0.5}},
    };
    const GravityField field{1.0, 0.05};
    for (int i = 0; i < 10; ++i) {
        euler.step(bodies, field, 0.01);
    }
    CHECK(bodies[0].position.x == Approx(0.1).epsilon(1e-12));
    CHECK(bodies[0].position.y == Approx(0.05).epsilon(1e-12));
    CHECK(bodies[0].velocity.x == Approx(1.0).epsilon(1e-12));
    CHECK(bodies[0].velocity.y == Approx(0.5).epsilon(1e-12));
}

TEST_CASE("euler_explicit_order_check") {
    // Pins explicit (not semi-implicit) ordering per plan section C.2:
    // position uses the OLD velocity; acceleration is evaluated at the OLD
    // positions. A semi-implicit step would update v first and move p.y with
    // the modified velocity, which this check rejects.
    const double dt = 0.01;
    std::vector<Body> bodies = {
        Body{1, 100.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},  // heavy, fixed-ish
        Body{2, 1.0, 0.1, Vec2{1.0, 0.0}, Vec2{0.0, 1.0}},    // light orbitand
    };
    const GravityField field{1.0, 0.0};  // eps = 0 for this check
    EulerIntegrator euler;
    euler.step(bodies, field, dt);

    Body& light = bodies[1];
    // Position advanced by the ORIGINAL velocity (0, 1) * dt.
    CHECK(light.position.x == 1.0);
    CHECK(light.position.y == 0.01);
    // Velocity then advanced by a*dt; a_x = G*M*(0-1)/((1^2+0)*1) = -100
    // (the light body's own mass cancels), a_y = 0 by symmetry.
    CHECK(light.velocity.y == 1.0);
    CHECK(light.velocity.x == 100.0 * (-1.0) * dt);
}

TEST_CASE("euler_energy_drift_euler") {
    // Documentation test for the spec's Euler-vs-RK4 demo: the two-body
    // circular preset (plan C.5: m=1 at (+-1, 0), v = (0, +-0.5), G=1,
    // eps=0; radii are irrelevant here) under 40000 explicit-Euler steps.
    // Explicit Euler is known to GAIN energy (orbit spirals out); we record
    // the drift and assert the documented instability rather than a tight
    // conservation bound.
    const double g = 1.0;
    const double eps = 0.0;
    const double dt = 1.0 / 120.0;  // the sim's fixed time step
    std::vector<Body> bodies = {
        Body{1, 1.0, 0.1, Vec2{1.0, 0.0}, Vec2{0.0, 0.5}},
        Body{2, 1.0, 0.1, Vec2{-1.0, 0.0}, Vec2{0.0, -0.5}},
    };
    const GravityField field{g, eps};
    EulerIntegrator euler;

    const double e0 = totalEnergy(bodies, g, eps);
    for (int i = 0; i < 40000; ++i) {
        euler.step(bodies, field, dt);
    }
    const double e1 = totalEnergy(bodies, g, eps);
    const double driftPct = std::abs(e1 - e0) / std::abs(e0) * 100.0;
    WARN("explicit Euler energy drift after 40000 steps at dt=1/120: "
         << driftPct << "% (E0=" << e0 << ", E1=" << e1 << ")");

    CHECK(std::isfinite(e1));
    // Explicit Euler gains energy on this setup (E1 > E0); a drift past 0.1%
    // documents the instability the sim demos against RK4.
    CHECK(driftPct > 0.1);
}
