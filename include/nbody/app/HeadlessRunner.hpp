/// @file
/// Window-free execution of the simulation: advance a scene by an exact
/// number of fixed physics steps and print one stats line, the basis of
/// the determinism checks and of long runs that need no display.

#pragma once

#include <cstdint>

#include "nbody/core/Presets.hpp"

namespace nbody::app {

/// @brief Run setup.bodies under a PhysicsWorld for exactly `steps` fixed
///        steps, then print one stats line to stdout:
///        steps=<n> time=<t> bodies=<n> merges=<n> checksum=<x>.
/// @param setup The scene to run; its bodies are moved into the world, and
///                the setup's config supplies the fixed timestep.
/// @param kind Which integrator to build (Euler or RK4).
/// @param steps Number of physics steps to execute; 0 reports the initial
///               state without stepping.
/// @return Always 0; the process exit code is the caller's choice.
int runHeadless(core::SimulationSetup setup, core::IntegratorKind kind,
                std::uint64_t steps);

}  // namespace nbody::app
