#pragma once

#include "nbody/app/MenuBar.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nbody::app {

// One parameterized inline editor (spec D.4) replaces the old seed-only
// seedEditing_ machinery: Simulation's separation/planets field rows and
// Cloud's seed/bodies field rows all share this grammar + the same edit
// laws (Enter commit, Esc edit-cancel, click-away cancel-then-process,
// Ctrl+C/X/V clipboard). The editor state itself lives in SimulationApp;
// the functions here are the pure half so the grammar is unit-testable
// display-free (same rationale as StepPlanning/Starfield).
enum class TextField : std::uint8_t { Seed, Bodies, Separation, Planets };

// Typing rule (D.4): a char is accepted only if the result stays within
// the field's grammar (seed: digits + one leading '-', 19 chars; bodies:
// digits, 4; separation: digits + at most one '.', 7; planets: 1 digit).
// Accepted digits trim leading zeros, with the same laws as today's seed
// editor (a lone '-' or '.' stays, an all-zero run collapses to "0").
// Returns false and leaves the buffer untouched when the char overflows
// the grammar.
bool fieldAppendChar(TextField field, char ch, std::string& buffer);

// Backspace deletes one character (empty stays empty; an empty buffer
// commits as the field's empty value, see fieldParseValue).
void fieldBackspace(TextField field, std::string& buffer);

// Paste filter (D.4): keeps only the characters that fit the field's
// grammar, applying the SAME overflow rules as typing (first-come-first-
// fit; e.g. pasting "1-2" into seed keeps "12", "-0012ab" keeps "-12").
std::string fieldSanitizePaste(TextField field, const std::string& pasted);

// Commit parsing. Empty buffer parses as 0 (same as today's seed editor).
// Seed: int64, saturating on overflow (strtoll errno/endpoint behavior).
// Bodies: unsigned digits; core clamps [2, 2000] (an empty field commits
// 0 and lands on the floor 2, spec D.3). Separation: parsed AND clamped
// to [0.5, 500] right here (empty commits the floor 0.5); neither the
// model setters nor core clamp it. Planets: a single digit; core clamps
// the prefix count [0, 8] (a typed 9 lands on 8).
std::int64_t fieldParseSeed(const std::string& buffer);
std::uint32_t fieldParseBodies(const std::string& buffer);
double fieldParseSeparation(const std::string& buffer);
int fieldParsePlanets(const std::string& buffer);

// Editor prefill: the current value rendered exactly like the model's row
// label renders it (integers bare, fractional via shortest 4-significant-
// digit form; seed/bodies/planets plain integers).
std::string fieldInitialSeed(std::int64_t seed);
std::string fieldInitialBodies(std::uint32_t bodies);
std::string fieldInitialSeparation(double separation);
std::string fieldInitialPlanets(int planets);

// Which editable field a menu row focuses, or nullopt for action/toggle/
// button rows. Derived from the D.1 row tables — Simulation rows 5/6 of
// the two-body (separation, path), figure-eight (separation) and solar-
// system (planets, pluto) layouts, Cloud rows 0/2 (seed, bodies); preset
// rows, separators, restart, the random rolls and the other groups have
// no field. Pure so tests can cross-check it against the live menu model.
std::optional<TextField> fieldForItem(MenuGroup group, int index,
                                       core::PresetName preset);

// The dropdown group that hosts a field (seed/bodies live in Cloud,
// separation/planets in Simulation). Used by the click-away law: clicks
// on the field's row OR its group header keep editor focus.
MenuGroup menuGroupOfField(TextField field);

}  // namespace nbody::app
