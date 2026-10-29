#include "nbody/core/Presets.hpp"

#include <cmath>
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

// Sun m=500 at rest at origin; four planets m=0.001 on circular orbits at
// r = 5, 10, 20, 40 with tangential v = (0, sqrt(G*500/r)). k=0.05; the sun's
// stored radius is NOT visually capped (the draw-time cap is Task 9's job).
SimulationSetup solarSystem() {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.01;

    constexpr double kSunMass = 500.0;
    constexpr double kPlanetMass = 0.001;
    std::vector<Body> bodies;
    bodies.emplace_back(Body{1, kSunMass, radiusFor(kSunMass, 0.05),
                             Vec2{0.0, 0.0}, Vec2{0.0, 0.0}});
    std::uint64_t id = 2;
    for (double r : {5.0, 10.0, 20.0, 40.0}) {
        const double v = std::sqrt(cfg.g * kSunMass / r);
        bodies.emplace_back(Body{id++, kPlanetMass,
                                 radiusFor(kPlanetMass, 0.05), Vec2{r, 0.0},
                                 Vec2{0.0, v}});
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
