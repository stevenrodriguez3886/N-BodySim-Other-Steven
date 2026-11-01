/// @file
/// Scene factories: the four simulation presets (two-body, solar system,
/// random cloud, figure-eight) plus the integrator factory. Each builder
/// returns initial bodies and a WorldConfig. All randomness is seeded
/// (std::mt19937, never rand()/time), so every scene is reproducible from
/// (preset, seed, options).

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/PhysicsWorld.hpp"

namespace nbody::core {

/// @brief Selectable initial scenes (CLI/UI spellings: two-body,
///        solar-system, random-cloud, figure-eight).
enum class PresetName { TwoBody, SolarSystem, RandomCloud, FigureEight };
/// @brief Selectable integration schemes.
enum class IntegratorKind { Euler, Rk4 };

/// @brief A fresh simulation's initial state: bodies plus gravity/timing.
struct SimulationSetup {
    std::vector<Body> bodies;  // ids assigned 1..N by the preset
    WorldConfig config;
};

/// @brief Map a full signed seed onto std::mt19937's u32 seed space.
/// @details Seeds in [0, 2^32) map to themselves, so EVERY historical
///   non-negative small-seed stream (including 42 and 7) is unchanged;
///   anything outside that window (negative or >= 2^32) goes through a
///   splitmix64 finalizer fold.
/// @param seed Caller-supplied signed seed.
/// @return The u32 seed actually handed to std::mt19937.
std::uint32_t foldSeed(std::int64_t seed);

/// @brief Two equal masses on a circular (or hyperbolic) two-body orbit.
/// @details Separation s = separation/10: positions (+-s, 0), circular
///   v = sqrt(G*m/(4s)); the default separation 10 reproduces the classic
///   unit circular orbit byte-exactly. Hyperbolic scales |v| to 1.6x
///   circular, which is unbound (1.6^2 > 2). G=1, eps=0, no RNG.
/// @param seed Unused (scene is deterministic).
/// @param separation Body separation in world units (scaled internally).
/// @param hyperbolic Unbound fly-by variant instead of the circular orbit.
/// @return The two bodies (ids 1..2) and their world config.
SimulationSetup twoBody(std::int64_t seed, double separation = 10.0,
                        bool hyperbolic = false);
/// @brief The sun plus an inside-out prefix of eight stylized planets.
/// @details Circular orbits, tangential v = sqrt(G*M_sun/a); no RNG, the
///   seed is ignored. planetCount is an inside-out prefix of the planet
///   table, clamped [0, 8]; pluto appends body id 10 (a ~= 200, circular)
///   ONLY when the clamped count == 8.
/// @param seed Unused (scene is deterministic).
/// @param planetCount Number of planets from the innermost outward.
/// @param pluto Append the distant Pluto body (full 8-planet set only).
/// @return Sun + planets and their world config.
SimulationSetup solarSystem(std::int64_t seed, int planetCount = 8,
                            bool pluto = false);
/// @brief The Chenciner-Montgomery figure-eight choreography, scaled.
/// @details Scale k applies Keplerian self-similarity: positions xk,
///   velocities /sqrt(k) (time axis xk^1.5). A merely positional x sqrt(k)
///   scaling would drift and unbind for k != 1; k = 1 is bitwise identity.
/// @param seed Unused (scene is deterministic).
/// @param scale Size multiplier of the classic choreography (1.0 = classic).
/// @return The three choreography bodies and their world config.
SimulationSetup figureEight(std::int64_t seed, double scale = 1.0);
/// @brief Isotropic Gaussian cloud, fully seeded and prefix-stable.
/// @details count clamps to [2, 2000]. Prefix contract: the shared RNG
///   stream is consumed in a fixed per-body order, so the first N bodies of
///   (seed, M) are bitwise identical to all of (seed, N) for N <= M.
/// @param seed Signed seed; reaches mt19937 through foldSeed.
/// @param count Body count, clamped [2, 2000].
/// @return The cloud bodies and their world config.
SimulationSetup randomCloud(std::int64_t seed, std::uint32_t count = 150);

/// @brief Option-free entry point: run a preset with its default options.
/// @param name Scene to build.
/// @param seed Full signed seed (option-taking builders take it too).
/// @return The preset's initial bodies and world config.
SimulationSetup createPreset(PresetName name, std::int64_t seed);

/// @brief Create the named integrator (fresh instance, no shared state).
/// @param kind Integration scheme to instantiate.
/// @return Owning pointer to the new integrator.
std::unique_ptr<IIntegrator> makeIntegrator(IntegratorKind kind);

}  // namespace nbody::core
