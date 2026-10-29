#pragma once

#include <cstdint>

#include "nbody/core/Presets.hpp"

namespace nbody::app {

// Runs setup.bodies under a PhysicsWorld for exactly `steps` fixed steps,
// then prints one line to stdout:
//   steps=<n> time=<t> bodies=<n> merges=<n> checksum=<x>
// Used for determinism checks and long spiral-of-death-free runs. Returns 0.
int runHeadless(core::SimulationSetup setup, core::IntegratorKind kind,
                std::uint64_t steps);

}  // namespace nbody::app
