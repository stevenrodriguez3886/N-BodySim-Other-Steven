#include "nbody/app/TextField.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>

namespace nbody::app {
namespace {

struct FieldTraits {
    int maxChars;       // buffer length cap (D.4)
    bool allowSign;     // leading '-' (seed only)
    bool allowDot;      // at most one '.' (separation only)
};

constexpr FieldTraits traitsFor(TextField field) {
    switch (field) {
        case TextField::Seed:
            return {19, true, false};
        case TextField::Bodies:
            return {4, false, false};
        case TextField::Separation:
            return {7, false, true};
        case TextField::Planets:
            return {1, false, false};
    }
    return {0, false, false};
}

// Mirror of MenuModel.cpp's formatNumber (shortest honest rendering):
// integers print bare, fractional values use 4 significant digits. Kept
// local (the model's copy is file-static); the two render the same values.
std::string formatNumber(double value) {
    if (std::isfinite(value) && value == std::floor(value) &&
        std::fabs(value) < 1e15) {
        return std::to_string(static_cast<long long>(value));
    }
    std::ostringstream out;
    out << std::setprecision(4) << value;
    return out.str();
}

// Collapse a run of leading zeros to a single "0" (today's seed-editor
// law), keeping markers: "-0005" -> "-5", "000" -> "0", "-000" -> "-0"
// (the sign survives so a later digit stays negative), "-" -> "-"
// (untouched, zero run absent), "0." -> "0." (one zero before a dot).
void trimLeadingZeros(std::string& buffer) {
    const bool signedBuffer = !buffer.empty() && buffer.front() == '-';
    const std::size_t start = signedBuffer ? 1 : 0;
    std::size_t zeros = 0;
    while (start + zeros < buffer.size() && buffer[start + zeros] == '0') {
        ++zeros;
    }
    if (zeros == 0) {
        return;  // no leading-zero run ("-", ".", or a nonzero start)
    }
    if (start + zeros == buffer.size()) {  // every digit was a zero
        buffer = signedBuffer ? "-0" : "0";
        return;
    }
    // Keep one zero only when the next char is a '.', else drop the run.
    const std::size_t keep = buffer[start + zeros] == '.' ? 1 : 0;
    buffer.erase(start, zeros - keep);
}

}  // namespace

bool fieldAppendChar(TextField field, char ch, std::string& buffer) {
    const FieldTraits traits = traitsFor(field);
    const bool digit = ch >= '0' && ch <= '9';
    const bool sign = traits.allowSign && ch == '-' && buffer.empty();
    const bool dot = traits.allowDot && ch == '.' &&
                     buffer.find('.') == std::string::npos;
    if (!digit && !sign && !dot) {
        return false;
    }
    if (buffer.size() >= static_cast<std::size_t>(traits.maxChars)) {
        return false;  // overflow rule shared with the paste filter (D.4)
    }
    buffer.push_back(ch);
    trimLeadingZeros(buffer);
    return true;
}

void fieldBackspace(TextField field, std::string& buffer) {
    (void)field;
    if (!buffer.empty()) {
        buffer.pop_back();
    }
}

std::string fieldSanitizePaste(TextField field, const std::string& pasted) {
    std::string out;
    for (const char ch : pasted) {
        fieldAppendChar(field, ch, out);
    }
    return out;
}

std::int64_t fieldParseSeed(const std::string& buffer) {
    if (buffer.empty() || buffer == "-") {
        return 0;
    }
    return std::strtoll(buffer.c_str(), nullptr, 10);  // saturates on overflow
}

std::uint32_t fieldParseBodies(const std::string& buffer) {
    if (buffer.empty()) {
        return 0;  // core clamps to the floor 2 (spec D.3)
    }
    const long long value = std::strtoll(buffer.c_str(), nullptr, 10);
    return static_cast<std::uint32_t>(value);
}

double fieldParseSeparation(const std::string& buffer) {
    // Field range clamp (spec D.2/D.3: [0.5, 500]) lives HERE: neither the
    // model setters nor twoBody clamp it. Empty commits the floor 0.5,
    // mirroring bodies' empty -> 0 -> core-floor-2 story.
    double value = 0.0;
    if (!buffer.empty() && buffer != ".") {
        value = std::strtod(buffer.c_str(), nullptr);
    }
    if (value < 0.5) {
        return 0.5;
    }
    return value > 500.0 ? 500.0 : value;
}

int fieldParsePlanets(const std::string& buffer) {
    if (buffer.empty()) {
        return 0;
    }
    return buffer[0] - '0';
}

std::string fieldInitialSeed(std::int64_t seed) {
    return std::to_string(seed);
}

std::string fieldInitialBodies(std::uint32_t bodies) {
    return std::to_string(bodies);
}

std::string fieldInitialSeparation(double separation) {
    return formatNumber(separation);
}

std::string fieldInitialPlanets(int planets) {
    return std::to_string(planets);
}

std::optional<TextField> fieldForItem(MenuGroup group, int index,
                                      core::PresetName preset) {
    switch (group) {
        case MenuGroup::Cloud:
            if (index == 0) {
                return TextField::Seed;
            }
            if (index == 2) {
                return TextField::Bodies;
            }
            return std::nullopt;  // random seed / random bodies are buttons
        case MenuGroup::Simulation:
            if (index != 5) {
                return std::nullopt;  // 0-3 presets, 4 sep, 6 toggle, restart
            }
            switch (preset) {
                case core::PresetName::TwoBody:
                case core::PresetName::FigureEight:
                    return TextField::Separation;
                case core::PresetName::SolarSystem:
                    return TextField::Planets;
                case core::PresetName::RandomCloud:
                    break;  // no option rows on Simulation (D.2)
            }
            return std::nullopt;
        case MenuGroup::Integrator:
        case MenuGroup::Physics:
        case MenuGroup::Count_:
            return std::nullopt;
    }
    return std::nullopt;
}

MenuGroup menuGroupOfField(TextField field) {
    switch (field) {
        case TextField::Seed:
        case TextField::Bodies:
            return MenuGroup::Cloud;
        case TextField::Separation:
        case TextField::Planets:
            return MenuGroup::Simulation;
    }
    return MenuGroup::Count_;  // unreachable
}

}  // namespace nbody::app
