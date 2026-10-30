#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/PhysicsWorld.hpp"
#include "nbody/core/Presets.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;
using Catch::Approx;

namespace {

// runHeadless output is compared as formatted text in determinism checks, so
// "identical" means bitwise-identical doubles, not merely == (-0.0 vs 0.0,
// etc. would slip through operator==).
bool bitsEqual(double a, double b) {
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

bool bodiesBitwiseEqual(const std::vector<Body>& a,
                        const std::vector<Body>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || !bitsEqual(a[i].mass, b[i].mass) ||
            !bitsEqual(a[i].radius, b[i].radius) ||
            !bitsEqual(a[i].position.x, b[i].position.x) ||
            !bitsEqual(a[i].position.y, b[i].position.y) ||
            !bitsEqual(a[i].velocity.x, b[i].velocity.x) ||
            !bitsEqual(a[i].velocity.y, b[i].velocity.y)) {
            return false;
        }
    }
    return true;
}

// Pinned checksum order, mirroring HeadlessRunner: accumulate every body's x
// component first, then every body's y, in document order. Fixing the order
// keeps the sum's floating-point rounding deterministic, which is what the
// determinism comparisons rely on.
double checksumOf(const std::vector<Body>& bodies) {
    double sum = 0.0;
    for (const Body& b : bodies) {
        sum += b.position.x;
    }
    for (const Body& b : bodies) {
        sum += b.position.y;
    }
    return sum;
}

// runHeadless-style PhysicsWorld run (runHeadless itself lives in nbody_app,
// out of reach for nbody_core_tests): RK4, `steps` steps, checksum rendered
// with setprecision(17) exactly like the stats line.
std::string runAndFormat(std::vector<Body> bodies, WorldConfig cfg,
                        std::uint64_t steps) {
    PhysicsWorld world{cfg, makeIntegrator(IntegratorKind::Rk4)};
    world.reset(std::move(bodies));
    for (std::uint64_t i = 0; i < steps; ++i) {
        world.step();
    }
    std::ostringstream out;
    out << std::setprecision(17) << checksumOf(world.bodies());
    return out.str();
}

}  // namespace

TEST_CASE("preset_two_body_orbit_period") {
    // Mirrors rk4_orbit_period_return (Task 5) but through the PUBLIC preset
    // path: if this passes, createPreset(TwoBody) is right, not just the
    // hand-written test fixture.
    SimulationSetup setup = createPreset(PresetName::TwoBody, 42);
    REQUIRE(setup.bodies.size() == 2);

    const std::vector<Body> start = setup.bodies;
    const double tPeriod = 4.0 * M_PI;  // omega = 0.5 => T = 4*pi
    setup.config.timeStep = tPeriod / 4000.0;

    PhysicsWorld world{setup.config, makeIntegrator(IntegratorKind::Rk4)};
    world.reset(std::move(setup.bodies));
    for (int i = 0; i < 4000; ++i) {
        world.step();
    }
    CHECK(world.mergeCount() == 0);

    const auto& bodies = world.bodies();
    REQUIRE(bodies.size() == 2);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        CHECK(std::isfinite(bodies[i].position.x));
        CHECK(std::isfinite(bodies[i].position.y));
        CHECK(bodies[i].position.x ==
              Approx(start[i].position.x).epsilon(0.02).margin(1e-9));
        CHECK(bodies[i].position.y ==
              Approx(start[i].position.y).epsilon(0.02).margin(1e-9));
    }
}

TEST_CASE("preset_figure_eight_period") {
    // Chenciner-Montgomery figure-eight (plan C.5), RK4 at dt = T/2400. The
    // orbit is a rigidly periodic solution, but comparing rotation-free
    // removes any residual phase drift: compare the MULTISET of the three
    // pairwise distances at t=0 and t=T, sorted, within 5%.
    SimulationSetup setup = createPreset(PresetName::FigureEight, 42);
    REQUIRE(setup.bodies.size() == 3);

    const auto pairwiseDistances = [](const std::vector<Body>& bodies) {
        std::vector<double> d;
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                d.push_back(length(bodies[j].position - bodies[i].position));
            }
        }
        std::sort(d.begin(), d.end());
        return d;
    };

    const std::vector<double> startDists = pairwiseDistances(setup.bodies);
    REQUIRE(startDists.size() == 3);

    const double tPeriod = 6.32591398;  // C.5
    setup.config.timeStep = tPeriod / 2400.0;

    PhysicsWorld world{setup.config, makeIntegrator(IntegratorKind::Rk4)};
    world.reset(std::move(setup.bodies));
    for (int i = 0; i < 2400; ++i) {
        world.step();
    }
    CHECK(world.mergeCount() == 0);
    const auto& bodies = world.bodies();
    REQUIRE(bodies.size() == 3);

    const std::vector<double> endDists = pairwiseDistances(bodies);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(endDists[i] == Approx(startDists[i]).epsilon(0.05));
    }
}

TEST_CASE("preset_cloud_determinism") {
    // Same seed => bitwise-identical body vectors ...
    const SimulationSetup a = createPreset(PresetName::RandomCloud, 7);
    const SimulationSetup b = createPreset(PresetName::RandomCloud, 7);
    REQUIRE(!a.bodies.empty());
    CHECK(bodiesBitwiseEqual(a.bodies, b.bodies));
    CHECK(bitsEqual(a.config.g, b.config.g));
    CHECK(bitsEqual(a.config.softening, b.config.softening));
    CHECK(bitsEqual(a.config.timeStep, b.config.timeStep));

    // ... and two 500-step world runs from them produce identical checksum
    // text (the exact string the headless stats line would print).
    CHECK(runAndFormat(a.bodies, a.config, 500) ==
          runAndFormat(b.bodies, b.config, 500));
}

TEST_CASE("preset_solar_planets_bounded") {
    // Spec D.2: 2000 steps; every planet must stay within a bound times its
    // initial sun-distance (no ejections) and nothing may collide (compressed
    // spacings are still >> collision radii). Run under BOTH integrators,
    // three arms (all measured on this machine, bodies deterministic):
    //  - RK4 at the preset dt: the spec's 1.35 bound, with huge room (max
    //    ratio 1.0001).
    //  - Euler at the preset dt: a documented looser 1.5 bound. Mercury
    //    spirals to 1.442 by step 2000 -- Euler's O(dt) energy injection IS
    //    the README "Euler spirals outward" demo, so 1.35 legitimately does
    //    not hold at this dt; the system is still bounded, 0 merges.
    //  - Euler dt-converged at 1/480: the SAME 1.35 bound holds with room
    //    (max 1.040), proving the drift is a dt artifact, not an unstable
    //    preset.
    SimulationSetup setup = createPreset(PresetName::SolarSystem, 42);
    REQUIRE(setup.bodies.size() == 9);
    const std::vector<Body> start = setup.bodies;

    struct Run {
        IntegratorKind kind;
        double dt;
        double bound;
    };
    const Run runs[] = {
        {IntegratorKind::Rk4, setup.config.timeStep, 1.35},
        {IntegratorKind::Euler, setup.config.timeStep, 1.5},
        {IntegratorKind::Euler, 1.0 / 480.0, 1.35},
    };
    for (const Run& run : runs) {
        CAPTURE(run.kind, run.dt);
        WorldConfig cfg = setup.config;
        cfg.timeStep = run.dt;
        PhysicsWorld world{cfg, makeIntegrator(run.kind)};
        std::vector<Body> bodies0 = setup.bodies;
        world.reset(std::move(bodies0));
        for (int i = 0; i < 2000; ++i) {
            world.step();
        }

        CHECK(world.mergeCount() == 0);
        const auto& bodies = world.bodies();
        REQUIRE(bodies.size() == 9);  // no merges => document order preserved
        for (std::size_t i = 1; i < bodies.size(); ++i) {
            const double r0 =
                length(start[i].position - start[0].position);
            const double r1 = length(bodies[i].position - bodies[0].position);
            CHECK(std::isfinite(r1));
            CHECK(r1 < run.bound * r0);
        }
    }
}

TEST_CASE("preset_solar_eight_bodies_circular") {
    // Spec D.2 literals: sun + 8 planets (Mercury..Neptune), circular
    // velocities v = sqrt(g*M_sun/a), start angles from the fixed list.
    const SimulationSetup setup = createPreset(PresetName::SolarSystem, 42);
    REQUIRE(setup.bodies.size() == 9);

    const Body& sun = setup.bodies[0];
    CHECK(sun.mass == Approx(500.0).margin(1e-12));
    CHECK(sun.appearance.glow);
    CHECK(sun.appearance.color == 0xFFD75Eu);

    const double kRadii[8] = {6.0, 9.3, 12.3, 15.6, 23.4, 30.0, 38.4, 46.2};
    const double kAnglesDeg[8] = {0.0, 40.0, 80.0, 130.0,
                                  170.0, 215.0, 260.0, 305.0};
    const std::uint32_t kColors[8] = {
        0x9C8E82u, 0xE6C47Au, 0x5F8DD6u, 0xC96A4Au,
        0xD8A568u, 0xE3CE9Au, 0x9AD6D2u, 0x5A7BD6u};
    constexpr double kDegToRad = 0.017453292519943295;  // pi/180

    for (int i = 0; i < 8; ++i) {
        CAPTURE(i);
        const Body& p = setup.bodies[static_cast<std::size_t>(i) + 1];
        CHECK(p.id == static_cast<std::uint64_t>(i) + 2);
        CHECK(p.mass > 0.0);
        CHECK(p.mass <= 1e-2 * sun.mass);  // planets never destabilise orbits

        const double a = kRadii[i];
        const Vec2 rel = p.position - sun.position;
        // Start position on the literal orbital radius, at the literal angle.
        CHECK(length(rel) == Approx(a).epsilon(1e-12).margin(1e-9));
        const double ang = std::atan2(rel.y, rel.x);
        const double expected = kAnglesDeg[i] * kDegToRad;
        const double diff =
            std::atan2(std::sin(ang - expected), std::cos(ang - expected));
        CHECK(std::fabs(diff) < 1e-9);

        // Circular speed v = sqrt(g*M_sun/a), tangential, counter-clockwise.
        const Vec2 vel = p.velocity - sun.velocity;
        const double vCirc = std::sqrt(setup.config.g * sun.mass / a);
        CHECK(length(vel) == Approx(vCirc).epsilon(1e-12));
        CHECK(std::fabs(dot(rel, vel)) < 1e-9 * a * vCirc);  // perpendicular
        CHECK(rel.x * vel.y - rel.y * vel.x > 0.0);          // CCW

        // Per-planet appearance per spec (renderer contract, Tasks 2-3).
        CHECK(p.appearance.color == kColors[i]);
        CHECK(p.appearance.glow == false);
        CHECK(p.appearance.ring == (i == 5));    // Saturn only
        CHECK(p.appearance.bands == (i == 4 ? 3 : (i == 5 ? 2 : 0)));
    }

    // Mass bands (T1 review minor): spec D.2's band is 1e-4..8e-3. The
    // rocky inner planets stay tiny (floored at the 1e-4 spec floor, far
    // below 1.0); both gas giants sit clearly above every inner planet;
    // and the heaviest planet is Jupiter at index 4 (bodies[5]). (The
    // review note's literal "gas giants > 50.0" does not match the
    // shipped band — verified against Presets.cpp — so the 10x
    // separation ratio below is the assertion that actually holds.)
    for (int i = 0; i < 4; ++i) {
        CAPTURE(i);
        const double m = setup.bodies[static_cast<std::size_t>(i) + 1].mass;
        CHECK(m >= 1e-4);
        CHECK(m < 1.0);
    }
    const double jupiter = setup.bodies[5].mass;
    const double saturn = setup.bodies[6].mass;
    CHECK(jupiter > 1.0e-3);
    CHECK(saturn > 1.0e-3);
    CHECK(saturn < jupiter);  // Jupiter is the heaviest planet ...
    for (int i = 0; i < 4; ++i) {
        const double m = setup.bodies[static_cast<std::size_t>(i) + 1].mass;
        CHECK(jupiter > 10.0 * m);
        CHECK(saturn > 10.0 * m);
    }
    std::size_t heaviest = 5;  // ... proven at the index level, too
    for (std::size_t i = 1; i < setup.bodies.size(); ++i) {
        if (setup.bodies[i].mass > setup.bodies[heaviest].mass) {
            heaviest = i;
        }
    }
    CHECK(heaviest == 5);  // bodies[5] == planets[4] == Jupiter
}

TEST_CASE("preset_solar_no_rng") {
    // The solar preset must stay seed-free: two different seeds produce the
    // exact same setup, body-for-body, including appearance.
    const SimulationSetup a = createPreset(PresetName::SolarSystem, 7);
    const SimulationSetup b = createPreset(PresetName::SolarSystem, 999);
    REQUIRE(!a.bodies.empty());
    CHECK(bodiesBitwiseEqual(a.bodies, b.bodies));
    REQUIRE(a.bodies.size() == b.bodies.size());
    for (std::size_t i = 0; i < a.bodies.size(); ++i) {
        CAPTURE(i);
        CHECK(a.bodies[i].appearance.color == b.bodies[i].appearance.color);
        CHECK(a.bodies[i].appearance.glow == b.bodies[i].appearance.glow);
        CHECK(a.bodies[i].appearance.ring == b.bodies[i].appearance.ring);
        CHECK(a.bodies[i].appearance.bands == b.bodies[i].appearance.bands);
    }
    CHECK(bitsEqual(a.config.g, b.config.g));
    CHECK(bitsEqual(a.config.softening, b.config.softening));
    CHECK(bitsEqual(a.config.timeStep, b.config.timeStep));
}

TEST_CASE("preset_presets_valid") {
    const std::pair<const char*, PresetName> all[] = {
        {"two-body", PresetName::TwoBody},
        {"solar-system", PresetName::SolarSystem},
        {"random-cloud", PresetName::RandomCloud},
        {"figure-eight", PresetName::FigureEight},
    };
    for (const auto& [label, name] : all) {
        CAPTURE(label);
        const SimulationSetup setup = createPreset(name, 42);
        REQUIRE_FALSE(setup.bodies.empty());
        for (std::size_t i = 0; i < setup.bodies.size(); ++i) {
            CAPTURE(i);
            CHECK(setup.bodies[i].mass > 0.0);
            CHECK(setup.bodies[i].radius > 0.0);
            CHECK(std::isfinite(setup.bodies[i].position.x));
            CHECK(std::isfinite(setup.bodies[i].position.y));
            CHECK(std::isfinite(setup.bodies[i].velocity.x));
            CHECK(std::isfinite(setup.bodies[i].velocity.y));
            // Presets assign ids 1..N in document order (PhysicsWorld::reset
            // keeps caller-provided nonzero ids).
            CHECK(setup.bodies[i].id == i + 1);
        }
    }
}
