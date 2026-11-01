/// @file
/// Concrete initial conditions for the four presets. Every constant here is
/// load-bearing: presets are regression anchors (bitwise reproducible from
/// seed), and tests pin orbits, energies, and stream stability against them.

#include "nbody/core/Presets.hpp"

#include <cmath>
#include <iterator>
#include <random>
#include <utility>

#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/Rk4Integrator.hpp"

namespace nbody::core {
namespace {

/// @brief Radius convention: r = k * sqrt(mass), with a scale factor k per
/// preset (area grows with mass; k sets how big bodies look on screen).
/// @param mass The body's mass.
/// @param k The preset's radius scale factor.
/// @return The collision/visual radius in world units.
double radiusFor(double mass, double k) {
    return k * std::sqrt(mass);
}

/// @brief Splitmix64 finalizer (Steele et al.) over the two's-complement
/// bits of the int64 seed; the high 32 bits become the mt19937 seed. Only
/// reached for seeds OUTSIDE [0, 2^32) — small non-negative seeds pass
/// through unchanged (see foldSeed), which keeps every historical stream
/// intact.
/// @param seed The out-of-window signed seed to fold.
/// @return The folded u32 seed for std::mt19937.
std::uint32_t splitmix64Fold(std::int64_t seed) {
    std::uint64_t z = static_cast<std::uint64_t>(seed);
    z += UINT64_C(0x9E3779B97F4A7C15);
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    z = z ^ (z >> 31);
    return static_cast<std::uint32_t>(z >> 32);
}

}  // namespace

std::uint32_t foldSeed(std::int64_t seed) {
    // Identity on the historical window: 0 <= seed < 2^32 -> uint32(seed).
    if (seed >= 0 && seed < INT64_C(4294967296)) {
        return static_cast<std::uint32_t>(seed);
    }
    return splitmix64Fold(seed);
}

// G=1, eps=0; m=1 at (+-s, 0), v=(0, +-0.5/sqrt(s)) with s = separation/10:
// at the default separation 10 (s=1) this is the exact circular orbit
// (r_orbit=1, v_rel=1, period T = 4*pi); larger s is the same orbit scaled
// (circular speed sqrt(G*m/(4s)) follows from the two-body scaling law).
// hyperbolic = same geometry, |v| = 1.6x circular: unbound (1.6^2 > 2).
SimulationSetup twoBody(std::int64_t /*seed*/, double separation,
                        bool hyperbolic) {
    const double s = separation / 10.0;
    const double v = (hyperbolic ? 1.6 : 1.0) * (0.5 / std::sqrt(s));
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;
    return SimulationSetup{
        {
            Body{1, 1.0, radiusFor(1.0, 0.1), Vec2{s, 0.0}, Vec2{0.0, v}},
            Body{2, 1.0, radiusFor(1.0, 0.1), Vec2{-s, 0.0}, Vec2{0.0, -v}},
        },
        cfg};
}

// A stylized solar system: sun m=500 at rest at the origin + 8 planets
// (Mercury..Neptune) on circular orbits, tangential v = sqrt(G*M_sun/a),
// start angles from a fixed literal list — NO RNG, the preset ignores the
// seed. Radii a = 6*(1, 1.55, 2.05, 2.6, 3.9, 5.0, 6.4, 7.7): a sqrt-style
// compression of real AU, order preserved. Masses 1e-4..8e-3 (Jupiter
// heaviest, every planet <= 1e-2 * sun so orbits stay clean). Collision
// radii stay k=0.05 (tiny): no merges at these spacings. The sun's stored
// radius is not visually capped (the renderer applies draw-time floors).
// Rocky-planet masses are floored into the 1e-4..8e-3 band (real ratios
// would put them ~3 orders of magnitude below it); ordering follows reality.
// planetCount is an inside-out prefix of the table, clamped
// [0, 8]; pluto adds body id 10 (a ~= 200, circular, prograde like the
// siblings) only when the CLAMPED count == 8, else it is ignored.
SimulationSetup solarSystem(std::int64_t /*seed*/, int planetCount,
                            bool pluto) {
    if (planetCount < 0) {
        planetCount = 0;
    } else if (planetCount > 8) {
        planetCount = 8;
    }
    const bool addPluto = pluto && planetCount == 8;
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
    bodies.reserve(1 + static_cast<std::size_t>(planetCount) +
                    (addPluto ? 1U : 0U));

    Body sun{1, kSunMass, radiusFor(kSunMass, 0.05), Vec2{0.0, 0.0},
            Vec2{0.0, 0.0}};
    sun.appearance.color = 0xFFD75Eu;
    sun.appearance.glow = true;  // star-style glow, honored by the renderer
    bodies.push_back(sun);

    std::uint64_t id = 2;
    for (int i = 0; i < planetCount; ++i) {
        const PlanetDef& p = kPlanets[static_cast<std::size_t>(i)];
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
    if (addPluto) {
        // Pluto at a ~= 200, circular v = sqrt(G*M_sun/a), prograde
        // (2D: no inclination), dim brown, mass ~1e-5, k=0.05 radius law.
        constexpr double kPlutoA = 200.0;
        constexpr double kPlutoMass = 1.0e-5;
        const double v = std::sqrt(cfg.g * kSunMass / kPlutoA);
        Body plutoBody{id++, kPlutoMass, radiusFor(kPlutoMass, 0.05),
                       Vec2{kPlutoA, 0.0}, Vec2{0.0, v}};
        plutoBody.appearance.color = 0x8A6A52u;  // dim brown
        bodies.push_back(plutoBody);
    }
    return SimulationSetup{std::move(bodies), cfg};
}

// Gaussian cloud, fully seeded: positions ~ N(0, 6) per axis
// (isotropic => disc), velocities ~ N(0, 0.2), mass ~ U[0.1, 1.0], k=0.08.
// The per-body sampling ORDER below (px, py, vx, vy, m) is part of the
// seeded-determinism contract: never reshuffle it. It also gives the prefix
// contract: body i consumes the stream identically for any total
// count, so the first N of (seed, M) are bitwise all of (seed, N).
// count clamps to [2, 2000]; the seed reaches mt19937 through
// foldSeed, so historical <2^32 streams are unchanged.
SimulationSetup randomCloud(std::int64_t seed, std::uint32_t count) {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.1;

    if (count < 2U) {
        count = 2U;
    } else if (count > 2000U) {
        count = 2000U;
    }

    std::mt19937 rng(foldSeed(seed));  // seeded, never rand(), never time
    std::normal_distribution<double> posDist(0.0, 6.0);
    std::normal_distribution<double> velDist(0.0, 0.2);
    std::uniform_real_distribution<double> massDist(0.1, 1.0);

    std::vector<Body> bodies;
    bodies.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
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
// The velocity block below is the published L=0 mirrored choreography; the
// naive v-block paired with these positions carries net angular momentum
// L ~ 0.839 and would not stay on the figure-eight.
// k=0.1 (free for equal masses) keeps the merge threshold (0.2) a factor
// 3.4 below the orbit's minimum pairwise separation (0.690, measured,
// dt-converged) — the choreography must never merge.
// scale: positions xk, velocities /sqrt(k) — the Keplerian self-similarity
// of Newtonian gravity with G fixed (a scaled copy traverses the same shape
// on a time axis xk^1.5). A merely positional x sqrt(k) scaling drifts and
// unbinds for k != 1. At k = 1.0 the scaling is bitwise identity
// (sqrt(1.0) == 1.0 and division by 1.0 is exact).
SimulationSetup figureEight(std::int64_t /*seed*/, double scale) {
    const double vk = std::sqrt(scale);
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;

    constexpr double kX = 0.9700436;
    constexpr double kY = 0.24308753;
    constexpr double kVX = 0.4662036850;
    constexpr double kVY = 0.4323657300;
    return SimulationSetup{
        {
            Body{1, 1.0, radiusFor(1.0, 0.1), Vec2{kX * scale, -kY * scale},
                 Vec2{kVX / vk, kVY / vk}},
            Body{2, 1.0, radiusFor(1.0, 0.1), Vec2{-kX * scale, kY * scale},
                 Vec2{kVX / vk, kVY / vk}},
            Body{3, 1.0, radiusFor(1.0, 0.1), Vec2{0.0, 0.0},
                 Vec2{-2.0 * kVX / vk, -2.0 * kVY / vk}},
        },
        cfg};
}

SimulationSetup createPreset(PresetName name, std::int64_t seed) {
    switch (name) {
    case PresetName::TwoBody:
        return twoBody(seed);
    case PresetName::SolarSystem:
        return solarSystem(seed);
    case PresetName::RandomCloud:
        return randomCloud(seed);
    case PresetName::FigureEight:
        return figureEight(seed);
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
