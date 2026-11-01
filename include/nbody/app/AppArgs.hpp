/// @file
/// Command-line parsing for the nbody executable: the parsed-argument
/// container and its parser, kept free of any SFML or windowing
/// dependencies so the accepted grammar can be unit-tested directly.

#pragma once

#include <cstdint>
#include <optional>
#include <ostream>

#include "nbody/core/Presets.hpp"

namespace nbody::app {

/// @brief The parsed command line, preinitialised to the CLI defaults.
/// @details parseArgs returns std::nullopt — after printing usage on
///   stderr — for an unknown flag, a missing '=', an unparseable value,
///   or a non-positive dt. --help sets helpRequested and still yields a
///   valid AppArgs; main then prints usage and exits 0.
struct AppArgs {
    core::PresetName preset = core::PresetName::TwoBody;   ///< scene to load
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;  ///< stepper
    ///< Seed for the random-cloud preset. Signed so negative values are
    ///< accepted; core folds it to its internal width
    ///< (nbody::core::foldSeed).
    std::int64_t seed = 42;
    std::uint32_t bodies = 150;  ///< random-cloud count, clamped to [2, 2000]
    std::optional<double> dt;    ///< nullopt = use the preset's config.timeStep
    std::uint64_t headlessSteps = 0;  ///< 0 = run the windowed app
    bool smokeTest = false;      ///< open a window, draw one frame, exit 0
    bool helpRequested = false;  ///< --help seen; main prints usage, exits 0
};

/// @brief Parse argv into an AppArgs, or report the error and give up.
/// @param argc Argument count, as handed to main.
/// @param argv Argument vector, as handed to main; null entries are
///             treated as the empty string.
/// @return The parsed args, or std::nullopt after printing an error and
///         the usage text on stderr.
/// @details Recognized:
///   --preset=<two-body|solar-system|random-cloud|figure-eight>
///   --integrator=<euler|rk4> --seed=<int64> --bodies=<int>
///   --dt=<float> --headless=<int> --smoke --help
///   --seed accepts a leading '-' (int64, saturating on overflow);
///   --bodies clamps to [2, 2000]. Repeated flags: last one wins.
std::optional<AppArgs> parseArgs(int argc, char** argv);

/// @brief Print the usage text (shared by the parse-error path on stderr
///        and main's --help path on stdout).
/// @param out Stream to write the usage block to.
void printUsage(std::ostream& out);

}  // namespace nbody::app
