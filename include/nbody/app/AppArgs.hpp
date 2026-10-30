#pragma once

#include <cstdint>
#include <optional>
#include <ostream>

#include "nbody/core/Presets.hpp"

namespace nbody::app {

// Parsed command line (plan B.3). parseArgs returns nullopt + prints usage
// on an unknown flag, missing '=', unparseable value, or dt <= 0. --help
// sets helpRequested and still returns a valid AppArgs; main then prints
// usage and exits 0.
struct AppArgs {
    core::PresetName preset = core::PresetName::TwoBody;
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;
    std::int64_t seed = 42;           // signed since spec D.3 (fold law in
                                      // core; see nbody::core::foldSeed)
    std::uint32_t bodies = 150;       // random-cloud count, clamped [2,2000]
    std::optional<double> dt;         // nullopt = use preset's config.timeStep
    std::uint64_t headlessSteps = 0;  // 0 = run windowed app
    bool smokeTest = false;           // open window, 1 frame, exit 0
    bool helpRequested = false;
};

// Recognized: --preset=<two-body|solar-system|random-cloud|figure-eight>
//            --integrator=<euler|rk4> --seed=<int64> --bodies=<int>
//            --dt=<float> --headless=<int> --smoke --help
// --seed accepts a leading '-' (int64, saturating on overflow); --bodies
// clamps to [2, 2000]. Repeated flags: last occurrence wins.
std::optional<AppArgs> parseArgs(int argc, char** argv);

// Usage text shared by the parse-error path and main's --help path.
void printUsage(std::ostream& out);

}  // namespace nbody::app
