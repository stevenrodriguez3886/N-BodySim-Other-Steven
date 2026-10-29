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

// Initial conditions per plan C.5 (exact constants). `seed` is used only by
// RandomCloud (std::mt19937, never rand()/time); other presets ignore it.
SimulationSetup createPreset(PresetName name, std::uint32_t seed);

std::unique_ptr<IIntegrator> makeIntegrator(IntegratorKind kind);

}  // namespace nbody::core
