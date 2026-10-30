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
    // review relay's literal "gas giants > 50.0" does not match the
    // shipped band — verified against Presets.cpp — so the 10x
    // separation ratio below is the assertion that actually holds.
    // The 10x check covers only the four rocky planets; Uranus/Neptune
    // are within 10x of Saturn (~5.6x) by design.)
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

// ---------------------------------------------------------------------------
// Task 1 (preset-option-menus): signed seeds + per-preset option parameters.
// ---------------------------------------------------------------------------

namespace {

// Literal 500-step RK4 checksums captured from the pre-change binary (main @
// 7ca1dfa) via `nbody --preset=<p> --headless=500` -- the same stats line
// runAndFormat() emulates. These pin the DEFAULT setups byte-for-byte: any
// drift in the new option parameters' default paths fails here.
constexpr const char* kPinCloudSeed42 = "44.930828866170515";
constexpr const char* kPinCloudSeed7 = "-27.350832960581805";
constexpr const char* kPinTwoBody = "0";
constexpr const char* kPinFigureEight = "3.8857805861880479e-15";
constexpr const char* kPinSolar = "-138.09795158656254";

}  // namespace

TEST_CASE("preset_checksum_pins_default_paths") {
    // Anchor regressions (spec D.3/D.8): the no-option call paths must stay
    // bit-identical to main. The 100k-step cloud anchor
    // (5.5936610862797043) is verified against the built binary in the task
    // report; these 500-step pins cover the same streams in CI.
    const SimulationSetup cloud42 = randomCloud(42);
    CHECK(runAndFormat(cloud42.bodies, cloud42.config, 500) ==
          kPinCloudSeed42);
    const SimulationSetup cloud7 = randomCloud(7);
    CHECK(runAndFormat(cloud7.bodies, cloud7.config, 500) ==
          kPinCloudSeed7);
    const SimulationSetup two = twoBody(42);
    CHECK(runAndFormat(two.bodies, two.config, 500) == kPinTwoBody);
    // The two-body checksum is 0 at ANY separation (mirror symmetry), so the
    // string pin alone cannot see geometry drift: pin the literals bitwise.
    CHECK(bitsEqual(two.bodies[0].position.x, 1.0));
    CHECK(bitsEqual(two.bodies[0].position.y, 0.0));
    CHECK(bitsEqual(two.bodies[0].velocity.y, 0.5));
    CHECK(bitsEqual(two.bodies[1].position.x, -1.0));
    CHECK(bitsEqual(two.bodies[1].velocity.y, -0.5));
    const SimulationSetup fig = figureEight(42);
    CHECK(runAndFormat(fig.bodies, fig.config, 500) ==
          kPinFigureEight);
    // Figure-eight pin likewise: literal choreography coordinates bitwise.
    CHECK(bitsEqual(fig.bodies[0].position.x, 0.9700436));
    CHECK(bitsEqual(fig.bodies[0].position.y, -0.24308753));
    CHECK(bitsEqual(fig.bodies[2].velocity.x, -2.0 * 0.4662036850));
    CHECK(bitsEqual(fig.bodies[2].velocity.y, -2.0 * 0.4323657300));
    const SimulationSetup solar = solarSystem(42);
    CHECK(runAndFormat(solar.bodies, solar.config, 500) ==
          kPinSolar);
}

TEST_CASE("preset_seed_fold_law") {
    // Spec D.3 fold: seed in [0, 2^32) maps to itself (historical streams
    // unchanged); everything else goes through the splitmix64 fold.
    CHECK(foldSeed(0) == 0U);
    CHECK(foldSeed(42) == 42U);
    CHECK(foldSeed(7) == 7U);
    CHECK(foldSeed(INT64_C(4294967295)) == 4294967295U);
    // Out-of-range seeds fold deterministically (pure function).
    CHECK(foldSeed(-1) == foldSeed(-1));
    CHECK(foldSeed(INT64_C(1099511627776)) ==
          foldSeed(INT64_C(1099511627776)));
    // 2^32 is the first folding input; it must not map to itself (0) — the
    // fold is what keeps mt19937 seeds in u32 range.
    CHECK(foldSeed(INT64_C(4294967296)) != 0U);
}

TEST_CASE("preset_seed_negative_and_big_determinism") {
    // createPreset(RandomCloud, -1) twice equal, != seed 1 (brief).
    const SimulationSetup a = createPreset(PresetName::RandomCloud, -1);
    const SimulationSetup b = createPreset(PresetName::RandomCloud, -1);
    REQUIRE(!a.bodies.empty());
    CHECK(bodiesBitwiseEqual(a.bodies, b.bodies));
    const SimulationSetup one = createPreset(PresetName::RandomCloud, 1);
    CHECK(!bodiesBitwiseEqual(a.bodies, one.bodies));

    // A big seed (2^40) is deterministic across repeated calls too.
    const SimulationSetup c =
        createPreset(PresetName::RandomCloud, INT64_C(1099511627776));
    const SimulationSetup d =
        createPreset(PresetName::RandomCloud, INT64_C(1099511627776));
    CHECK(bodiesBitwiseEqual(c.bodies, d.bodies));

    // Folding law: the 500-step run of a folded seed equals the run of its
    // folded u32 (the engine really is mt19937(foldSeed(seed))).
    CHECK(runAndFormat(a.bodies, a.config, 500) ==
          runAndFormat(randomCloud(static_cast<std::int64_t>(foldSeed(-1)))
                           .bodies,
                       a.config, 500));
    CHECK(runAndFormat(c.bodies, c.config, 500) ==
          runAndFormat(
              randomCloud(static_cast<std::int64_t>(
                  foldSeed(INT64_C(1099511627776))))
                  .bodies,
              c.config, 500));
}

TEST_CASE("preset_cloud_prefix_contract") {
    // First 150 of (s, 2000) == all of (s, 150), bitwise: the shared RNG
    // stream is consumed in the same per-body order regardless of count.
    const SimulationSetup big = randomCloud(7, 2000);
    const SimulationSetup small = randomCloud(7, 150);
    REQUIRE(big.bodies.size() == 2000);
    REQUIRE(small.bodies.size() == 150);
    CHECK(bodiesBitwiseEqual(
        {big.bodies.begin(), big.bodies.begin() + 150}, small.bodies));
    // Default count stays 150 and matches the explicit-count call bitwise.
    const SimulationSetup def = randomCloud(7);
    CHECK(bodiesBitwiseEqual(def.bodies, small.bodies));
    // The contract also holds at other counts: first 100 of (7, 500).
    CHECK(bodiesBitwiseEqual(
        {big.bodies.begin(), big.bodies.begin() + 100},
        randomCloud(7, 100).bodies));
}

TEST_CASE("preset_cloud_count_clamps") {
    CHECK(randomCloud(7, 0).bodies.size() == 2);
    CHECK(randomCloud(7, 1).bodies.size() == 2);
    CHECK(randomCloud(7, 2).bodies.size() == 2);
    CHECK(randomCloud(7, 2000).bodies.size() == 2000);
    CHECK(randomCloud(7, 2001).bodies.size() == 2000);
    CHECK(randomCloud(7, 4000000000U).bodies.size() == 2000);
    // Clamped-to-2 output is a valid prefix: bodies[0..1] of count 2000.
    const SimulationSetup big = randomCloud(7, 2000);
    CHECK(bodiesBitwiseEqual(randomCloud(7, 0).bodies,
                             {big.bodies.begin(), big.bodies.begin() + 2}));
}

TEST_CASE("preset_two_body_separation_scale") {
    const SimulationSetup def = twoBody(0);
    REQUIRE(def.bodies.size() == 2);

    // twoBody(sep=20) positions == twoBody() positions * 2 exactly.
    const SimulationSetup dbl = twoBody(0, 20.0);
    REQUIRE(dbl.bodies.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(bitsEqual(dbl.bodies[i].position.x,
                        def.bodies[i].position.x * 2.0));
        CHECK(bitsEqual(dbl.bodies[i].position.y,
                        def.bodies[i].position.y * 2.0));
        // Masses/radii/ids untouched by scaling.
        CHECK(bitsEqual(dbl.bodies[i].mass, def.bodies[i].mass));
        CHECK(bitsEqual(dbl.bodies[i].radius, def.bodies[i].radius));
        CHECK(dbl.bodies[i].id == def.bodies[i].id);
    }
    CHECK(dbl.bodies[0].position.x == 2.0);
    CHECK(dbl.bodies[1].position.x == -2.0);

    // Circular at the new scale: per-body v = sqrt(G*m/(4s)), s = sep/10.
    const double vCirc = 0.5 / std::sqrt(2.0);
    for (const Body& b : dbl.bodies) {
        CHECK(length(b.velocity) == Approx(vCirc).epsilon(1e-12));
    }
    CHECK(std::fabs(dot(dbl.bodies[0].position, dbl.bodies[0].velocity)) <
          1e-9);  // tangential
    CHECK(dbl.bodies[0].velocity.y > 0.0);  // same orientation as default

    // Default path is bit-identical to the pre-option call (createPreset).
    CHECK(bodiesBitwiseEqual(def.bodies,
                             createPreset(PresetName::TwoBody, 0).bodies));
}

TEST_CASE("preset_two_body_hyperbolic") {
    const SimulationSetup orb = twoBody(0);
    const SimulationSetup hyp = twoBody(0, 10.0, true);
    REQUIRE(hyp.bodies.size() == 2);

    // Same geometry, equal masses; |v| = 1.6x circular, inbound fly-by.
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(bitsEqual(hyp.bodies[i].position.x, orb.bodies[i].position.x));
        CHECK(bitsEqual(hyp.bodies[i].position.y, orb.bodies[i].position.y));
        CHECK(bitsEqual(hyp.bodies[i].mass, orb.bodies[i].mass));
        // 1.6 * 0.5: exact in binary (0.5 scales cleanly).
        CHECK(length(hyp.bodies[i].velocity) ==
              Approx(1.6 * length(orb.bodies[i].velocity)).epsilon(1e-12));
    }
    // Same directions as the orbit variant (tangential, CCW pair).
    CHECK(hyp.bodies[0].velocity.y > 0.0);
    CHECK(hyp.bodies[1].velocity.y < 0.0);

    // Unbound: E = KE + PE > 0 (1.6^2 = 2.56 > escape ratio 2 for equal
    // masses).
    const double ke = 0.5 * hyp.bodies[0].mass *
                          dot(hyp.bodies[0].velocity, hyp.bodies[0].velocity) +
                      0.5 * hyp.bodies[1].mass *
                          dot(hyp.bodies[1].velocity, hyp.bodies[1].velocity);
    const double d =
        length(hyp.bodies[1].position - hyp.bodies[0].position);
    const double pe = -hyp.config.g * hyp.bodies[0].mass *
                      hyp.bodies[1].mass / d;
    CHECK(ke + pe > 0.0);

    // Scaling composes: hyperbolic at sep=20 keeps positions x2 and the
    // 1.6x rule at the scaled circular speed.
    const SimulationSetup hyp20 = twoBody(0, 20.0, true);
    CHECK(bitsEqual(hyp20.bodies[0].position.x, 2.0));
    CHECK(length(hyp20.bodies[0].velocity) ==
          Approx(1.6 * 0.5 / std::sqrt(2.0)).epsilon(1e-12));
}

TEST_CASE("preset_figure_eight_scale") {
    const SimulationSetup def = figureEight(0);
    REQUIRE(def.bodies.size() == 3);
    // Default == the classic solution (createPreset path, bitwise).
    CHECK(bodiesBitwiseEqual(
        def.bodies, createPreset(PresetName::FigureEight, 0).bodies));

    // figureEight(scale=3): positions x3, velocities x sqrt(3) (spec D.2
    // self-similarity rule), bitwise against the same scalar ops.
    const SimulationSetup s3 = figureEight(0, 3.0);
    REQUIRE(s3.bodies.size() == 3);
    const double k = std::sqrt(3.0);
    for (std::size_t i = 0; i < 3; ++i) {
        CAPTURE(i);
        CHECK(bitsEqual(s3.bodies[i].position.x,
                        def.bodies[i].position.x * 3.0));
        CHECK(bitsEqual(s3.bodies[i].position.y,
                        def.bodies[i].position.y * 3.0));
        CHECK(bitsEqual(s3.bodies[i].velocity.x,
                        def.bodies[i].velocity.x * k));
        CHECK(bitsEqual(s3.bodies[i].velocity.y,
                        def.bodies[i].velocity.y * k));
        CHECK(bitsEqual(s3.bodies[i].mass, def.bodies[i].mass));
        CHECK(bitsEqual(s3.bodies[i].radius, def.bodies[i].radius));
    }
}

TEST_CASE("preset_solar_planet_count_and_pluto") {
    const SimulationSetup full = solarSystem(0);
    REQUIRE(full.bodies.size() == 9);

    // Inside-out prefix: planets=3 == sun + first 3 planets, bitwise.
    const SimulationSetup p3 = solarSystem(0, 3);
    REQUIRE(p3.bodies.size() == 4);
    CHECK(bodiesBitwiseEqual(p3.bodies,
                             {full.bodies.begin(), full.bodies.begin() + 4}));
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(p3.bodies[i].id == i + 1);  // ids stay 1..N
    }

    // 0 = sun only; -1 clamps to 0; 9 clamps to 8 (== default, bitwise).
    CHECK(solarSystem(0, 0).bodies.size() == 1);
    CHECK(bodiesBitwiseEqual(solarSystem(0, -1).bodies,
                             solarSystem(0, 0).bodies));
    CHECK(bodiesBitwiseEqual(solarSystem(0, 9).bodies, full.bodies));

    // Pluto: extra body id 10 at a ~= 200, circular v = sqrt(g*M/a), only
    // when planets == 8.
    const SimulationSetup pl = solarSystem(0, 8, true);
    REQUIRE(pl.bodies.size() == 10);
    const Body& pluto = pl.bodies[9];
    CHECK(pluto.id == 10);
    CHECK(pluto.mass == Approx(1.0e-5).epsilon(1e-12));
    const double r = length(pluto.position);
    CHECK(r == Approx(200.0).epsilon(1e-9));
    const double vCirc = std::sqrt(pl.config.g * pl.bodies[0].mass / 200.0);
    CHECK(length(pluto.velocity) == Approx(vCirc).epsilon(1e-12));
    CHECK(std::fabs(dot(pluto.position, pluto.velocity)) < 1e-9 * 200.0 *
              vCirc);                                           // tangential
    CHECK(pluto.position.x * pluto.velocity.y -
              pluto.position.y * pluto.velocity.x > 0.0);  // prograde, CCW
    // First 9 bodies unchanged by the flag; 9 planets total (8 + Pluto).
    CHECK(bodiesBitwiseEqual({pl.bodies.begin(), pl.bodies.begin() + 9},
                             full.bodies));

    // planets < 8: the pluto flag is ignored (independent row, no-op).
    CHECK(bodiesBitwiseEqual(solarSystem(0, 3, true).bodies, p3.bodies));
    // 9 clamps to 8 first, so pluto still applies there.
    CHECK(bodiesBitwiseEqual(solarSystem(0, 9, true).bodies, pl.bodies));
}
