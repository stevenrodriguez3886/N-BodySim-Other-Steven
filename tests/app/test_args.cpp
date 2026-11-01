/// @file test_args.cpp
/// @brief Command-line argument parsing tests for parseArgs/AppArgs.
/// nbody_app_tests must never construct an SFML window: args parsing is pure
/// code; the windowed path is exercised by the nbody_smoke add_test instead.
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "nbody/app/AppArgs.hpp"

using nbody::app::AppArgs;
using nbody::app::parseArgs;
using nbody::core::IntegratorKind;
using nbody::core::PresetName;

namespace {

/// @brief Build a C-style argv (NUL-terminated at argv[argc]) from flag
/// strings and run parseArgs over it.
/// @param flags The command-line flags, excluding the program name.
/// @return The parsed args, or nullopt when the flags are invalid.
std::optional<AppArgs> parse(const std::vector<std::string>& flags) {
    std::vector<std::string> storage;
    storage.reserve(flags.size());
    for (const std::string& f : flags) {
        storage.push_back(f);
    }
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("nbody"));
    for (std::string& s : storage) {
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);
    return parseArgs(static_cast<int>(storage.size()) + 1, argv.data());
}

}  // namespace

/// @brief Known flags fill the parsed fields; the rest stay at defaults.
TEST_CASE("args_parses_known_flags") {
    const auto args =
        parse({"--preset=solar-system", "--integrator=euler", "--seed=99"});
    REQUIRE(args.has_value());
    CHECK(args->preset == PresetName::SolarSystem);
    CHECK(args->integrator == IntegratorKind::Euler);
    CHECK(args->seed == 99U);
    // Untouched defaults.
    CHECK_FALSE(args->dt.has_value());
    CHECK(args->headlessSteps == 0);
    CHECK_FALSE(args->smokeTest);
    CHECK_FALSE(args->helpRequested);
}

/// @brief Unknown or malformed flags are rejected outright.
TEST_CASE("args_unknown_flag_rejected") {
    CHECK_FALSE(parse({"--bogus"}).has_value());
    CHECK_FALSE(parse({"not-a-flag"}).has_value());
    CHECK_FALSE(parse({"--preset"}).has_value());  // missing '='
    CHECK_FALSE(parse({"--preset="}).has_value());  // empty value
}

/// @brief --dt accepts only positive numeric values.
TEST_CASE("args_bad_dt_rejected") {
    CHECK_FALSE(parse({"--dt=abc"}).has_value());
    CHECK_FALSE(parse({"--dt=0"}).has_value());
    CHECK_FALSE(parse({"--dt=-1"}).has_value());
    CHECK(parse({"--dt=0.001"}).has_value());
}

/// @brief --help parses to a valid args struct with the help flag set.
TEST_CASE("args_help_sets_flag") {
    const auto args = parse({"--help"});
    REQUIRE(args.has_value());  // valid AppArgs, not a rejection
    CHECK(args->helpRequested);
}

/// @brief --headless=N parses into headlessSteps.
TEST_CASE("args_headless_steps_parsed") {
    const auto args = parse({"--headless=1000"});
    REQUIRE(args.has_value());
    CHECK(args->headlessSteps == 1000);
}
