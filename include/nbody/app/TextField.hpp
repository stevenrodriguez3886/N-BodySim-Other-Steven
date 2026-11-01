#pragma once

#include "nbody/app/MenuBar.hpp"

#include <algorithm>
#include <cstddef>
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

// (The old buffer-only fieldAppendChar/fieldBackspace live on privately
// inside TextField.cpp: the caret model below subsumes them -- typing with
// caret == anchor == size() IS append, and Backspace is fieldEditDelete
// with forward = false.)

// Paste filter (D.4): keeps only the characters that fit the field's
// grammar, applying the SAME overflow rules as typing (first-come-first-
// fit; e.g. pasting "1-2" into seed keeps "12", "-0012ab" keeps "-12").
std::string fieldSanitizePaste(TextField field, const std::string& pasted);

// --- Caret/selection model (spec Goal 4) -----------------------------------
// The full text-box model the app's inline editors run on: a real caret
// index plus a selection anchor. caret/anchor are indices into buffer in
// [0, size()]; caret != anchor means a selection spans [selStart, selEnd).
// Editors seed this from the fieldInitial* prefills with caret = anchor =
// size (end of text). Every mutator below is grammar-checked against the
// SAME laws as the paste filter (sign at buffer position 0 only, one dot,
// leading-zero trim, length cap): a mutation that would break the grammar
// leaves buffer, caret AND anchor completely untouched and returns false.
struct FieldText {
    std::string buffer;
    std::size_t caret = 0;   // insertion index, [0, buffer.size()]
    std::size_t anchor = 0;  // selection anchor; caret != anchor == selecting

    bool hasSelection() const { return caret != anchor; }
    std::size_t selStart() const { return std::min(anchor, caret); }
    std::size_t selEnd() const { return std::max(anchor, caret); }
};

// Ctrl+C payload: the selection span when one is active, else whole buffer.
std::string fieldCopyText(const FieldText& text);

// Erase [selStart, selEnd); caret lands at selStart, anchor collapses.
// No-op without a selection.
void fieldDeleteSelection(FieldText& text);

// Type a char: an active selection is replaced first (erased at
// selStart), then ch inserts at that index. Grammar-checked on the
// RESULTING string, so e.g. '-' at caret > 0 in Seed fails and the
// selection stays put. Caret lands after the inserted char (tracked
// through any leading-zero trim); anchor collapses onto it.
bool fieldInsertChar(TextField field, char ch, FieldText& text);

// Backspace (forward = false) deletes left of the caret, Delete right of
// it; a selection is deleted instead when one is active. Empty buffer or
// caret at the edge in the deletion direction -> false, untouched. A
// deletion that exposes a leading-zero run trims it (invariant: buffers
// never contain one), adjusting the caret.
bool fieldEditDelete(TextField field, FieldText& text, bool forward);

// Left/Right (delta +/-1) clamped to [0, size()]. select = true keeps the
// anchor (shift-select grows/shrinks the span); select = false collapses
// it onto the new caret. False only when caret AND anchor are unchanged.
bool fieldMoveCaret(FieldText& text, int delta, bool select);

// Home (toEnd = false) / End (toEnd = true); same anchor law as
// fieldMoveCaret.
void fieldMoveCaretTo(FieldText& text, bool toEnd, bool select);

// Ctrl+V at the caret: an active selection is replaced, then each pasted
// char passes the same per-char grammar as fieldSanitizePaste checked
// against the RESULTING string, appended at the caret while the field's
// max length allows. Overflow truncates the PASTE, never the buffer.
// Caret lands after the inserted text. Empty sanitized paste with a
// selection still clears the span (true); nothing changed -> false.
bool fieldApplyPaste(TextField field, const std::string& pasted,
                     FieldText& text);

// Click/drag x within a field -> caret index: clamp(round(fraction *
// length)). Inputs are finite by contract (the app clamps the pixel
// ratio to [0, 1]); NaN still yields 0 defensively.
std::size_t fieldIndexForFraction(double fraction, std::size_t length);

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
