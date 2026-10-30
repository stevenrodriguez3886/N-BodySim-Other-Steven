#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/PhysicsWorld.hpp"

namespace nbody::core {

enum class PresetName { TwoBody, SolarSystem, RandomCloud, FigureEight };
enum class IntegratorKind { Euler, Rk4 };

struct SimulationSetup {
    std::vector<Body> bodies;  // ids assigned 1..N by the preset
    WorldConfig config;
};

// Seed law (spec D.3): core preset functions take std::int64_t seeds, but
// the engine stays std::mt19937 (u32 seed). Seeds in [0, 2^32) map to
// themselves, so EVERY historical non-negative small-seed stream (incl. 42
// and 7) is unchanged; anything outside that window (negative or >= 2^32)
// goes through a splitmix64 finalizer fold.
std::uint32_t foldSeed(std::int64_t seed);

// Individual preset builders (spec D.3). Initial conditions per plan C.5
// (exact constants); `seed` is used only by randomCloud (std::mt19937,
// never rand()/time). Defaults reproduce the pre-option setups byte-exactly:
//  - twoBody: separation scales today's solution (s = separation/10:
//    positions +-s, circular v = sqrt(G*m/(4s))); hyperbolic = 1.6x circular
//    (unbound fly-by).
//  - solarSystem: planetCount = inside-out prefix of the 8-planet table,
//    clamped [0, 8]; pluto appends body id 10 (a ~= 200, circular) ONLY
//    when the clamped count == 8.
//  - figureEight: scale k on the classic choreography (positions xk,
//    velocities /sqrt(k) per Keplerian self-similarity, time axis xk^1.5;
//    supersedes spec D.2's x sqrt(k), which drifts).
//  - randomCloud: count clamped [2, 2000], default 150. Prefix contract:
//    the shared RNG stream is consumed in the same per-body order, so the
//    first N bodies of (seed, M) equal all of (seed, N) for N <= M.
SimulationSetup twoBody(std::int64_t seed, double separation = 10.0,
                        bool hyperbolic = false);
SimulationSetup solarSystem(std::int64_t seed, int planetCount = 8,
                            bool pluto = false);
SimulationSetup figureEight(std::int64_t seed, double scale = 1.0);
SimulationSetup randomCloud(std::int64_t seed, std::uint32_t count = 150);

// Default-options dispatch (every option-less call site stays source-
// compatible; only the seed parameter widened from uint32_t to int64_t).
SimulationSetup createPreset(PresetName name, std::int64_t seed);

std::unique_ptr<IIntegrator> makeIntegrator(IntegratorKind kind);

}  // namespace nbody::core
