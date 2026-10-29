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

// Shared digit-string parse: value must start with a digit (strtoull would
// happily eat a leading sign) and be fully consumed. Sets `overflow` on
// ERANGE rather than failing.
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

// --seed: overflow CLAMPS to UINT32_MAX (plan Task 7 edge case: range check
// on the strtoul result rather than silent wraparound).
bool parseSeed(std::string_view value, std::uint32_t& out) {
    unsigned long long v = 0;
    bool overflow = false;
    if (!parseUnsignedLongLong(value, v, overflow)) {
        return false;
    }
    out = (overflow || v > UINT32_MAX) ? UINT32_MAX
                                       : static_cast<std::uint32_t>(v);
    return true;
}

// --headless: overflow is a rejection (clamping to 2^64 steps would just
// wedge the run).
bool parseSteps(std::string_view value, std::uint64_t& out) {
    unsigned long long v = 0;
    bool overflow = false;
    if (!parseUnsignedLongLong(value, v, overflow) || overflow) {
        return false;
    }
    out = static_cast<std::uint64_t>(v);
    return true;
}

// Usage errors go to stderr (stdout stays reserved for program output;
// --help keeps printing usage on stdout with exit 0 in main).
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
        << "  --seed=<int>            seed for random-cloud (default 42)\n"
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
            // dt > 10 is a sanity bound, not physics: it prevents a silent
            // NaN cascade in Release builds (Debug already asserts finiteness
            // in PhysicsWorld::step).
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
