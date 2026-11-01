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
// The At-variant also reports the erased run [dropStart, dropStart +
// dropCount) so caret-model callers can map indices through the trim.
void trimLeadingZerosAt(std::string& buffer, std::size_t* dropStart,
                        std::size_t* dropCount) {
    if (dropStart != nullptr) {
        *dropStart = 0;
    }
    if (dropCount != nullptr) {
        *dropCount = 0;
    }
    const bool signedBuffer = !buffer.empty() && buffer.front() == '-';
    const std::size_t start = signedBuffer ? 1 : 0;
    std::size_t zeros = 0;
    while (start + zeros < buffer.size() && buffer[start + zeros] == '0') {
        ++zeros;
    }
    if (zeros == 0) {
        return;  // no leading-zero run ("-", ".", or a nonzero start)
    }
    // All-zero runs keep one zero; otherwise one survives only when the
    // next char is a '.'.
    std::size_t keep = 1;
    if (start + zeros != buffer.size()) {
        keep = buffer[start + zeros] == '.' ? 1 : 0;
    }
    buffer.erase(start, zeros - keep);
    if (dropStart != nullptr) {
        *dropStart = start;
    }
    if (dropCount != nullptr) {
        *dropCount = zeros - keep;
    }
}

void trimLeadingZeros(std::string& buffer) {
    trimLeadingZerosAt(buffer, nullptr, nullptr);
}

// Map a caret index through a trim drop: positions at or before the drop
// start are unchanged, later ones shift left by the dropped count.
std::size_t adjustThroughDrops(std::size_t index, std::size_t dropStart,
                               std::size_t dropCount) {
    if (index <= dropStart) {
        return index;
    }
    return index - std::min(dropCount, index - dropStart);
}

// Grammar gate over COMPLETE strings: exactly the laws fieldAppendChar
// enforces one char at a time (digit anywhere; '-' only at buffer
// position 0 for signed fields; at most one '.' for dotted fields; no
// other characters; length cap). Leading-zero runs are the trim law's
// job, not this gate's: callers trim before checking.
bool grammarOk(TextField field, const std::string& text) {
    const FieldTraits traits = traitsFor(field);
    if (text.size() > static_cast<std::size_t>(traits.maxChars)) {
        return false;
    }
    int dots = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            continue;
        }
        if (c == '-') {
            if (!traits.allowSign || i != 0) {
                return false;  // sign lives at buffer position 0 only
            }
            continue;
        }
        if (c == '.') {
            if (!traits.allowDot || ++dots > 1) {
                return false;  // at most one dot
            }
            continue;
        }
        return false;  // letters and everything else never pass
    }
    return true;
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
    std::string trial = buffer;
    trial.push_back(ch);
    trimLeadingZeros(trial);
    if (!grammarOk(field, trial)) {
        return false;  // safety net: the pre-checks above already exclude it
    }
    buffer = std::move(trial);
    return true;
}

std::string fieldCopyText(const FieldText& text) {
    if (!text.hasSelection()) {
        return text.buffer;  // Ctrl+C with no selection copies everything
    }
    return text.buffer.substr(text.selStart(),
                             text.selEnd() - text.selStart());
}

void fieldDeleteSelection(FieldText& text) {
    if (!text.hasSelection()) {
        return;
    }
    const std::size_t start = text.selStart();
    text.buffer.erase(start, text.selEnd() - start);
    text.caret = start;
    text.anchor = start;
}

bool fieldInsertChar(TextField field, char ch, FieldText& text) {
    const std::size_t insertAt =
        text.hasSelection() ? text.selStart() : text.caret;
    std::string trial = text.buffer;
    trial.erase(text.selStart(), text.selEnd() - text.selStart());
    trial.insert(insertAt, 1, ch);
    std::size_t dropStart = 0;
    std::size_t dropCount = 0;
    trimLeadingZerosAt(trial, &dropStart, &dropCount);
    if (!grammarOk(field, trial)) {
        return false;  // buffer, caret AND anchor stay completely untouched
    }
    const std::size_t caret =
        adjustThroughDrops(insertAt + 1, dropStart, dropCount);
    text.buffer = std::move(trial);
    text.caret = caret;
    text.anchor = caret;
    return true;
}

bool fieldEditDelete(TextField field, FieldText& text, bool forward) {
    if (text.hasSelection()) {
        fieldDeleteSelection(text);  // span first, either direction
        return true;
    }
    if (forward && text.caret >= text.buffer.size()) {
        return false;  // Delete at the right edge
    }
    if (!forward && text.caret == 0) {
        return false;  // Backspace at 0 (empty stays empty law included)
    }
    std::string trial = text.buffer;
    trial.erase(forward ? text.caret : text.caret - 1, 1);
    std::size_t caret = forward ? text.caret : text.caret - 1;
    std::size_t dropStart = 0;
    std::size_t dropCount = 0;
    trimLeadingZerosAt(trial, &dropStart, &dropCount);  // e.g. "100" -> "0"
    caret = adjustThroughDrops(caret, dropStart, dropCount);
    if (!grammarOk(field, trial)) {
        return false;  // safety net: deletions only remove characters
    }
    text.buffer = std::move(trial);
    text.caret = caret;
    text.anchor = caret;
    return true;
}

bool fieldMoveCaret(FieldText& text, int delta, bool select) {
    const long long size = static_cast<long long>(text.buffer.size());
    const long long raw = static_cast<long long>(text.caret) + delta;
    const long long clamped = std::max(0LL, std::min(size, raw));
    const std::size_t caret = static_cast<std::size_t>(clamped);
    const std::size_t anchor = select ? text.anchor : caret;
    if (caret == text.caret && anchor == text.anchor) {
        return false;  // position AND anchor unchanged
    }
    text.caret = caret;
    text.anchor = anchor;
    return true;
}

void fieldMoveCaretTo(FieldText& text, bool toEnd, bool select) {
    const std::size_t caret = toEnd ? text.buffer.size() : 0;
    text.caret = caret;
    if (!select) {
        text.anchor = caret;  // same anchor law as fieldMoveCaret
    }
}

bool fieldApplyPaste(TextField field, const std::string& pasted,
                     FieldText& text) {
    FieldText trial = text;
    fieldDeleteSelection(trial);  // paste replaces the span when one is active
    std::size_t pos = trial.caret;
    for (const char ch : pasted) {
        std::string candidate = trial.buffer;
        candidate.insert(pos, 1, ch);
        std::size_t dropStart = 0;
        std::size_t dropCount = 0;
        trimLeadingZerosAt(candidate, &dropStart, &dropCount);
        if (!grammarOk(field, candidate)) {
            continue;  // per-char sanitize: this pasted char is dropped
        }
        pos = adjustThroughDrops(pos + 1, dropStart, dropCount);
        trial.buffer = std::move(candidate);
    }
    trial.caret = pos;
    trial.anchor = pos;
    if (trial.buffer == text.buffer && trial.caret == text.caret &&
        trial.anchor == text.anchor) {
        return false;  // nothing changed (incl. empty paste, no selection)
    }
    text = std::move(trial);
    return true;
}

std::size_t fieldIndexForFraction(double fraction, std::size_t length) {
    if (std::isnan(fraction) || !(fraction > 0.0)) {
        return 0;  // NaN (defensive), negatives, -inf
    }
    if (fraction >= 1.0) {
        return length;  // overs clamp to the end (+inf; inputs are finite by contract)
    }
    return static_cast<std::size_t>(
        std::llround(fraction * static_cast<double>(length)));
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
            // Presets, separator, restart only (spec D.1): no field rows.
            return std::nullopt;
        case MenuGroup::Options:
            if (index != 0) {
                return std::nullopt;  // row 1 is a toggle button
            }
            switch (preset) {
                case core::PresetName::TwoBody:
                case core::PresetName::FigureEight:
                    return TextField::Separation;
                case core::PresetName::SolarSystem:
                    return TextField::Planets;
                case core::PresetName::RandomCloud:
                    break;  // no option rows (D.2)
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
            return MenuGroup::Options;
    }
    return MenuGroup::Count_;  // unreachable
}

}  // namespace nbody::app
