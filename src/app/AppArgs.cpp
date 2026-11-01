/// @file
/// Implementation of the command-line grammar declared in AppArgs.hpp:
/// per-flag value parsers and the main argument loop.

#include "nbody/app/AppArgs.hpp"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace nbody::app {
namespace {

/// @brief Shared digit-string parse for the unsigned flags.
/// @param value The flag value text after '='.
/// @param out Parsed magnitude on success; undefined on failure.
/// @param overflow Set true when the digits exceed 64-bit range (the parse
///                 still succeeds; each caller decides reject vs. clamp).
/// @return true when the value starts with a digit and is fully consumed.
/// @note A leading sign is rejected up front because strtoull would
///   happily eat one.
bool parseUnsignedLongLong(std::string_view value,
                           unsigned long long& out, bool& overflow) {
    overflow = false;
    if (value.empty() || value[0] < '0' || value[0] > '9') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const std::string copy(value);
    out = std::strtoull(copy.c_str(), &end, 10);
    if (end != copy.c_str() + copy.size()) {
        return false;  // trailing junk, e.g. "12abc"
    }
    overflow = errno == ERANGE;
    return true;
}

/// @brief Parse the --seed value: signed int64, a leading '-' allowed.
/// @param value The flag value text after '='.
/// @param out The seed on success; untouched on failure.
/// @return true when the digits (past an optional '-') are fully consumed.
/// @note Overflow clamps to the int64 bounds — a range check on the
///   strtoll result rather than silent wraparound, so an out-of-range seed
///   still names a usable (extreme) seed instead of erroring out.
bool parseSeed(std::string_view value, std::int64_t& out) {
    if (value.empty()) {
        return false;
    }
    std::size_t digits = value[0] == '-' ? 1U : 0U;
    if (digits == 1U && value.size() == 1U) {
        return false;  // bare "-"
    }
    if (value[digits] < '0' || value[digits] > '9') {
        return false;  // strtoll would eat junk like "-x" or "+5"
    }
    errno = 0;
    char* end = nullptr;
    const std::string copy(value);
    const long long v = std::strtoll(copy.c_str(), &end, 10);
    if (end != copy.c_str() + copy.size()) {
        return false;  // trailing junk, e.g. "12abc"
    }
    if (errno == ERANGE) {
        out = value[0] == '-' ? INT64_MIN : INT64_MAX;
    } else {
        out = static_cast<std::int64_t>(v);
    }
    return true;
}

/// @brief Parse the --bodies value: digits, clamped to [2, 2000].
/// @param value The flag value text after '='.
/// @param out The clamped count on success.
/// @return false only when the digits are malformed; overflow clamps high.
bool parseBodies(std::string_view value, std::uint32_t& out) {
    unsigned long long v = 0;
    bool overflow = false;
    if (!parseUnsignedLongLong(value, v, overflow)) {
        return false;
    }
    if (overflow || v > 2000ULL) {
        out = 2000U;
    } else {
        out = v < 2ULL ? 2U : static_cast<std::uint32_t>(v);
    }
    return true;
}

/// @brief Parse the --headless value: plain digits, no clamping.
/// @param value The flag value text after '='.
/// @param out The step count on success.
/// @return false on malformed digits or overflow — clamping to 2^64 steps
///   would just wedge the run.
bool parseSteps(std::string_view value, std::uint64_t& out) {
    unsigned long long v = 0;
    bool overflow = false;
    if (!parseUnsignedLongLong(value, v, overflow) || overflow) {
        return false;
    }
    out = static_cast<std::uint64_t>(v);
    return true;
}

/// @brief Report a parse error and the usage text, then (via the caller)
///   fail the parse.
/// @param arg The offending argument text, or empty for a flagless error.
/// @param reason Human-readable cause, printed before the argument.
/// @note Errors go to stderr so stdout stays reserved for program output;
///   --help still prints usage on stdout with exit 0 in main.
void fail(std::string_view arg, std::string_view reason) {
    std::cerr << "nbody: " << reason;
    if (!arg.empty()) {
        std::cerr << " '" << arg << "'";
    }
    std::cerr << '\n';
    printUsage(std::cerr);
}

}  // namespace

void printUsage(std::ostream& out) {
    out << "usage: nbody [options]\n"
        << "  --preset=<two-body|solar-system|random-cloud|figure-eight>\n"
        << "  --integrator=<euler|rk4>\n"
        << "  --seed=<int64>          seed for random-cloud (default 42,\n"
        << "                          leading '-' allowed)\n"
        << "  --bodies=<int>          random-cloud body count [2,2000] "
        << "(default 150)\n"
        << "  --dt=<float>            override the preset's fixed timestep\n"
        << "  --headless=<int>        run N physics steps, print stats, exit\n"
        << "  --smoke                 open a window for one frame, exit 0\n"
        << "  --help                  show this text and exit 0\n";
}

std::optional<AppArgs> parseArgs(int argc, char** argv) {
    AppArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "--help") {
            args.helpRequested = true;
            continue;
        }
        if (arg == "--smoke") {
            args.smokeTest = true;
            continue;
        }
        if (arg.substr(0, 2) != "--") {
            fail(arg, "unknown argument");
            return std::nullopt;
        }
        const std::string_view body = arg.substr(2);
        const std::size_t eq = body.find('=');
        if (eq == std::string_view::npos) {
            fail(arg, "flag requires a value");
            return std::nullopt;
        }
        const std::string_view name = body.substr(0, eq);
        const std::string_view value = body.substr(eq + 1);
        if (value.empty()) {
            fail(arg, "empty value");
            return std::nullopt;
        }

        if (name == "preset") {
            if (value == "two-body") {
                args.preset = core::PresetName::TwoBody;
            } else if (value == "solar-system") {
                args.preset = core::PresetName::SolarSystem;
            } else if (value == "random-cloud") {
                args.preset = core::PresetName::RandomCloud;
            } else if (value == "figure-eight") {
                args.preset = core::PresetName::FigureEight;
            } else {
                fail(value, "unknown preset");
                return std::nullopt;
            }
        } else if (name == "integrator") {
            if (value == "euler") {
                args.integrator = core::IntegratorKind::Euler;
            } else if (value == "rk4") {
                args.integrator = core::IntegratorKind::Rk4;
            } else {
                fail(value, "unknown integrator");
                return std::nullopt;
            }
        } else if (name == "seed") {
            if (!parseSeed(value, args.seed)) {
                fail(value, "invalid --seed value");
                return std::nullopt;
            }
        } else if (name == "bodies") {
            if (!parseBodies(value, args.bodies)) {
                fail(value, "invalid --bodies value");
                return std::nullopt;
            }
        } else if (name == "headless") {
            if (!parseSteps(value, args.headlessSteps)) {
                fail(value, "invalid --headless value");
                return std::nullopt;
            }
        } else if (name == "dt") {
            errno = 0;
            char* end = nullptr;
            const std::string copy(value);
            const double dt = std::strtod(copy.c_str(), &end);
            // The (0, 10] window is a sanity bound, not physics: fully
            // consumed digits, no range error, finite, positive, and at
            // most 10 — which prevents a silent NaN cascade in Release
            // builds (Debug already asserts finiteness in
            // PhysicsWorld::step).
            if (end != copy.c_str() + copy.size() || errno == ERANGE ||
                !std::isfinite(dt) || dt <= 0.0 || dt > 10.0) {
                fail(value, "--dt must be a positive finite number");
                return std::nullopt;
            }
            args.dt = dt;
        } else {
            fail(arg, "unknown flag");
            return std::nullopt;
        }
    }
    return args;
}

}  // namespace nbody::app
