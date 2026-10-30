#include "nbody/core/Presets.hpp"

#include <cmath>
#include <iterator>
#include <random>
#include <utility>

#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/Rk4Integrator.hpp"

namespace nbody::core {
namespace {

// Radius convention from plan C.5: r = k * sqrt(m), with k per preset.
double radiusFor(double mass, double k) {
    return k * std::sqrt(mass);
}

// G=1, eps=0; m=1 at (+-1, 0), v=(0, +-0.5): exact circular orbit with
// r_orbit=1, v_rel=1, period T = 4*pi. k=0.1.
SimulationSetup twoBody() {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;
    return SimulationSetup{
        {
            Body{1, 1.0, radiusFor(1.0, 0.1), Vec2{1.0, 0.0}, Vec2{0.0, 0.5}},
            Body{2, 1.0, radiusFor(1.0, 0.1), Vec2{-1.0, 0.0},
                 Vec2{0.0, -0.5}},
        },
        cfg};
}

// OUR solar system (spec D.2): sun m=500 at rest at origin + 8 planets
// (Mercury..Neptune) on circular orbits, tangential v = sqrt(G*M_sun/a),
// start angles from a fixed literal list — NO RNG, the preset ignores the
// seed. Radii a = 6*(1, 1.55, 2.05, 2.6, 3.9, 5.0, 6.4, 7.7): a sqrt-style
// compression of real AU, order preserved. Masses 1e-4..8e-3 (Jupiter
// heaviest, every planet <= 1e-2 * sun so orbits stay clean). Collision
// radii stay k=0.05 (tiny): no merges at these spacings. The sun's stored
// radius is NOT visually capped (draw-time floors are Task 2's job).
// Rocky-planet masses are floored into the spec band (real ratios would put
// them ~3 orders of magnitude below it); ordering follows reality.
SimulationSetup solarSystem() {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.01;

    constexpr double kSunMass = 500.0;
    constexpr double kDegToRad = 0.017453292519943295;  // pi/180

    struct PlanetDef {
        double mass;
        double a;        // orbital radius (start position radius), world units
        double angleDeg;  // fixed start angle, counter-clockwise orbit
        std::uint32_t color;
        std::uint8_t bands;
        bool ring;
    };
    constexpr PlanetDef kPlanets[] = {
        {1.0e-4, 6.0, 0.0, 0x9C8E82u, 0, false},     // Mercury
        {1.6e-4, 9.3, 40.0, 0xE6C47Au, 0, false},    // Venus
        {2.0e-4, 12.3, 80.0, 0x5F8DD6u, 0, false},   // Earth
        {1.2e-4, 15.6, 130.0, 0xC96A4Au, 0, false},  // Mars
        {8.0e-3, 23.4, 170.0, 0xD8A568u, 3, false},  // Jupiter
        {2.4e-3, 30.0, 215.0, 0xE3CE9Au, 2, true},   // Saturn
        {3.7e-4, 38.4, 260.0, 0x9AD6D2u, 0, false},  // Uranus
        {4.3e-4, 46.2, 305.0, 0x5A7BD6u, 0, false},  // Neptune
    };

    std::vector<Body> bodies;
    bodies.reserve(1 + std::size(kPlanets));

    Body sun{1, kSunMass, radiusFor(kSunMass, 0.05), Vec2{0.0, 0.0},
            Vec2{0.0, 0.0}};
    sun.appearance.color = 0xFFD75Eu;
    sun.appearance.glow = true;  // star-style glow (renderer, Task 2)
    bodies.push_back(sun);

    std::uint64_t id = 2;
    for (const PlanetDef& p : kPlanets) {
        const double theta = p.angleDeg * kDegToRad;
        const double v = std::sqrt(cfg.g * kSunMass / p.a);
        Body body{id++, p.mass, radiusFor(p.mass, 0.05),
                  Vec2{p.a * std::cos(theta), p.a * std::sin(theta)},
                  Vec2{-v * std::sin(theta), v * std::cos(theta)}};
        body.appearance.color = p.color;
        body.appearance.bands = p.bands;
        body.appearance.ring = p.ring;
        bodies.push_back(body);
    }
    return SimulationSetup{std::move(bodies), cfg};
}

// N=150 Gaussian cloud, fully seeded (plan C.5): positions ~ N(0, 6) per
// axis (isotropic => disc), velocities ~ N(0, 0.2), mass ~ U[0.1, 1.0],
// k=0.08. The per-body sampling ORDER below (px, py, vx, vy, m) is part of
// the seeded-determinism contract: never reshuffle it.
SimulationSetup randomCloud(std::uint32_t seed) {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.1;

    std::mt19937 rng(seed);  // seeded, never rand(), never time-based
    std::normal_distribution<double> posDist(0.0, 6.0);
    std::normal_distribution<double> velDist(0.0, 0.2);
    std::uniform_real_distribution<double> massDist(0.1, 1.0);

    constexpr int kCount = 150;
    std::vector<Body> bodies;
    bodies.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        const double px = posDist(rng);
        const double py = posDist(rng);
        const double vx = velDist(rng);
        const double vy = velDist(rng);
        const double m = massDist(rng);
        bodies.emplace_back(Body{static_cast<std::uint64_t>(i) + 1, m,
                                 radiusFor(m, 0.08), Vec2{px, py},
                                 Vec2{vx, vy}});
    }
    return SimulationSetup{std::move(bodies), cfg};
}

// Chenciner-Montgomery figure-eight, G=1, eps=0, period T ~ 6.32591398.
// The figure-eight velocity block below deviates from plan C.5 (whose literal
// v-block carries net angular momentum L ~ 0.839 with these positions); the
// shipped L=0 mirrored variant is the published Chenciner-Montgomery
// choreography — see the plan erratum note (2026-10-29).
// k=0.1 is free per C.5 (equal masses) and keeps the merge threshold (0.2) a
// factor 3.4 below the orbit's minimum pairwise separation (0.690, measured,
// dt-converged) — the choreography must never merge.
SimulationSetup figureEight() {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;

    constexpr double kX = 0.9700436;
    constexpr double kY = 0.24308753;
    constexpr double kVX = 0.4662036850;
    constexpr double kVY = 0.4323657300;
    return SimulationSetup{
        {
            Body{1, 1.0, radiusFor(1.0, 0.1), Vec2{kX, -kY},
                 Vec2{kVX, kVY}},
            Body{2, 1.0, radiusFor(1.0, 0.1), Vec2{-kX, kY},
                 Vec2{kVX, kVY}},
            Body{3, 1.0, radiusFor(1.0, 0.1), Vec2{0.0, 0.0},
                 Vec2{-2.0 * kVX, -2.0 * kVY}},
        },
        cfg};
}

}  // namespace

SimulationSetup createPreset(PresetName name, std::uint32_t seed) {
    switch (name) {
    case PresetName::TwoBody:
        return twoBody();
    case PresetName::SolarSystem:
        return solarSystem();
    case PresetName::RandomCloud:
        return randomCloud(seed);
    case PresetName::FigureEight:
        return figureEight();
    }
    return {};  // unreachable for valid enum values; keeps -Wreturn-type quiet
}

std::unique_ptr<IIntegrator> makeIntegrator(IntegratorKind kind) {
    switch (kind) {
    case IntegratorKind::Euler:
        return std::make_unique<EulerIntegrator>();
    case IntegratorKind::Rk4:
        return std::make_unique<Rk4Integrator>();
    }
    return {};  // unreachable for valid enum values
}

}  // namespace nbody::core
