/// @file
/// The window-free stats run: fixed-step physics followed by the pinned
/// stdout stats line (see the header for the output format).

#include "nbody/app/HeadlessRunner.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "nbody/core/PhysicsWorld.hpp"

namespace nbody::app {

int runHeadless(core::SimulationSetup setup, core::IntegratorKind kind,
                std::uint64_t steps) {
    core::PhysicsWorld world{setup.config, core::makeIntegrator(kind)};
    world.reset(std::move(setup.bodies));  // keeps the preset's ids 1..N
    for (std::uint64_t i = 0; i < steps; ++i) {
        world.step();
    }

    // Checksum accumulation order is PINNED: every body's x component first,
    // then every body's y, in document order. Floating-point addition is not
    // associative, so any order works only as long as it never changes —
    // determinism tests compare this text across runs.
    double checksum = 0.0;
    for (const core::Body& b : world.bodies()) {
        checksum += b.position.x;
    }
    for (const core::Body& b : world.bodies()) {
        checksum += b.position.y;
    }

    std::ostringstream line;
    line << "steps=" << world.totalSteps() << " time=" << std::setprecision(17)
         << world.simulationTime() << " bodies=" << world.bodies().size()
         << " merges=" << world.mergeCount() << " checksum=" << checksum;
    std::cout << line.str() << '\n';
    return 0;
}

}  // namespace nbody::app
