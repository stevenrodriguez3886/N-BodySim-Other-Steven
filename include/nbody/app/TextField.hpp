/// @file
/// The inline menu editors: the editable-field identities, their input
/// grammar, a caret/selection text model, commit parsing, and the
/// row-to-field mapping. All of it is pure and display-free -- the
/// editor state itself lives in SimulationApp -- so the grammar laws are
/// unit-testable without rendering (same rationale as the other pure
/// app layers).

#pragma once

#include "nbody/app/MenuBar.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nbody::app {

/// @brief One parameterized inline editor: Simulation's
/// separation/planets field rows and Cloud's seed/bodies field rows all
/// share this enum, one input grammar, and the same edit laws (Enter
/// commit, Esc edit-cancel, click-away cancel-then-process, Ctrl+C/X/V
/// clipboard).
///
/// @note The caret model below subsumes plain append/backspace: typing
/// with caret == anchor == size() IS append, and Backspace is
/// fieldEditDelete with forward = false. A file-static fieldAppendChar
/// survives in TextField.cpp only as fieldSanitizePaste's per-char
/// grammar helper.
enum class TextField : std::uint8_t { Seed, Bodies, Separation, Planets };

/// @brief Paste filter: keeps only the characters that fit the field's
/// grammar, applying the SAME overflow rules as typing
/// (first-come-first-fit; e.g. pasting "1-2" into seed keeps "12",
/// "-0012ab" keeps "-12").
/// @param field The target field, which selects the grammar.
/// @param pasted The raw clipboard text.
/// @return The maximal grammar-valid prefix-by-filtering of pasted.
std::string fieldSanitizePaste(TextField field, const std::string& pasted);

/// @brief The full text-box model the app's inline editors run on: a
/// real caret index plus a selection anchor.
///
/// caret and anchor are indices into buffer in [0, size()];
/// caret != anchor means a selection spans [selStart, selEnd). Editors
/// seed this from the fieldInitial* prefills with caret = anchor = size
/// (end of text).
/// @note Every mutator is grammar-checked against the SAME laws as the
/// paste filter (sign at buffer position 0 only, at most one dot,
/// leading-zero trim, length cap): a mutation that would break the
/// grammar leaves buffer, caret AND anchor completely untouched and
/// returns false.
struct FieldText {
    std::string buffer;     ///< The field's current text.
    std::size_t caret = 0;  ///< Insertion index, [0, buffer.size()].
    std::size_t anchor = 0; ///< Selection anchor; caret != anchor selecting

    /// @brief Whether a selection span is active (caret != anchor).
    bool hasSelection() const { return caret != anchor; }
    /// @brief The selection span's left endpoint (min of caret/anchor).
    std::size_t selStart() const { return std::min(anchor, caret); }
    /// @brief The selection span's right endpoint (max of caret/anchor).
    std::size_t selEnd() const { return std::max(anchor, caret); }
};

/// @brief Ctrl+C payload: the selection span when one is active, else
/// the whole buffer.
/// @param text The editor's text state.
/// @return The text to place on the clipboard.
std::string fieldCopyText(const FieldText& text);

/// @brief Erase [selStart, selEnd): the caret lands at selStart and the
/// anchor collapses onto it. No-op without a selection.
/// @param text The editor's text state, modified in place.
void fieldDeleteSelection(FieldText& text);

/// @brief Type a char: an active selection is replaced first (erased at
/// selStart), then ch inserts at that index. Grammar-checked on the
/// RESULTING string, so e.g. '-' at caret > 0 in Seed fails and the
/// selection stays put.
/// @param field The target field, which selects the grammar.
/// @param ch The typed character.
/// @param text The editor's text state; untouched on failure.
/// @return True when accepted; then the caret sits after the inserted
///         char (tracked through any leading-zero trim) and the anchor
///         collapses onto it.
bool fieldInsertChar(TextField field, char ch, FieldText& text);

/// @brief Backspace (forward = false) deletes left of the caret, Delete
/// right of it; an active selection is deleted instead, either way.
/// @param field The target field (grammar safety check).
/// @param text The editor's text state; untouched on failure.
/// @param forward True for Delete, false for Backspace.
/// @return False -- leaving everything untouched -- on an empty buffer or
///         when the caret sits at the edge in the deletion direction. A
///         deletion exposing a leading-zero run trims it (invariant:
///         buffers never contain one) and adjusts the caret.
bool fieldEditDelete(TextField field, FieldText& text, bool forward);

/// @brief Left/Right caret motion (delta +/-1), clamped to
/// [0, buffer.size()].
/// @param text The editor's text state.
/// @param delta The caret shift in characters.
/// @param select True keeps the anchor (shift-select grows/shrinks the
///               span); false collapses it onto the new caret.
/// @return False only when caret AND anchor are unchanged.
bool fieldMoveCaret(FieldText& text, int delta, bool select);

/// @brief Home (toEnd = false) / End (toEnd = true); same anchor law as
/// fieldMoveCaret (select keeps the anchor, else it collapses).
/// @param text The editor's text state.
/// @param toEnd True jumps to the end, false to the start.
/// @param select Whether the anchor stays put.
void fieldMoveCaretTo(FieldText& text, bool toEnd, bool select);

/// @brief Ctrl+V at the caret: an active selection is replaced, then
/// each pasted char passes the same per-char grammar as
/// fieldSanitizePaste, checked against the RESULTING string and inserted
/// at the running caret while the field's max length allows. Overflow
/// truncates the PASTE, never the buffer.
/// @param field The target field, which selects the grammar.
/// @param pasted The raw clipboard text.
/// @param text The editor's text state; untouched on failure.
/// @return True when anything changed; the caret then lands after the
///         inserted text. An empty sanitized paste with a selection
///         still clears the span (true); nothing changed -> false.
bool fieldApplyPaste(TextField field, const std::string& pasted,
                     FieldText& text);

/// @brief Map a click/drag x-position within a field to a caret index:
/// clamp(round(fraction * length)).
/// @param fraction The cursor's fraction along the field, finite by
///                 contract (the app clamps the pixel ratio to [0, 1]);
///                 NaN still yields 0 defensively.
/// @param length The buffer's character count.
/// @return The caret index in [0, length].
std::size_t fieldIndexForFraction(double fraction, std::size_t length);

// Commit parsing for the four fields: an empty buffer parses as 0 for
// every field. The range-safety split: Seed saturates here (int64
// overflow via strtoll endpoint behavior); Bodies and Planets clamp in
// core at scene build; Separation alone clamps in this file -- see each
// function for where its clamp lives.

/// @brief Seed: signed int64, saturating on overflow (strtoll endpoint
/// behavior; a lone "-" parses as 0). No clamping anywhere.
/// @param buffer The committed text.
/// @return The parsed seed the model setter receives verbatim.
std::int64_t fieldParseSeed(const std::string& buffer);
/// @brief Bodies: unsigned digits; core clamps the count to [2, 2000]
/// (an empty field commits 0 and lands on the floor 2).
/// @param buffer The committed digit text.
/// @return The raw parsed count; core-side clamp applies later.
std::uint32_t fieldParseBodies(const std::string& buffer);
/// @brief Separation: parsed AND clamped to [0.5, 500] right here --
/// neither the model setters nor core clamp it (clamp ownership lives in
/// this file; an empty field commits the floor 0.5).
/// @param buffer The committed text ("." alone parses as the floor).
/// @return The value clamped into [0.5, 500].
double fieldParseSeparation(const std::string& buffer);
/// @brief Planets: a single digit (the grammar caps input at one char);
/// core clamps the inside-out prefix count to [0, 8] (a typed 9 would
/// land on 8, though the grammar blocks it first).
/// @param buffer The committed text; empty parses as 0.
/// @return The digit's value.
int fieldParsePlanets(const std::string& buffer);

/// @brief Editor prefills: each renders the current value exactly like
/// the model's row label renders it -- integers bare, fractional values
/// via the shortest 4-significant-digit form; seed/bodies/planets are
/// plain integers. Editors seed caret = anchor = size() from these.
/// @param value The field's current model value.
/// @return The prefill text for the editor buffer.
std::string fieldInitialSeed(std::int64_t seed);
/// @brief Prefill for the bodies field (plain integer form).
std::string fieldInitialBodies(std::uint32_t bodies);
/// @brief Prefill for the separation field (shortest honest form).
std::string fieldInitialSeparation(double separation);
/// @brief Prefill for the planets field (plain integer form).
std::string fieldInitialPlanets(int planets);

/// @brief Which editable field a menu row focuses, or nullopt for
/// action/toggle/button rows. Derived from the menu's row tables:
/// Options row 0 of the two-body (separation), figure-eight (separation)
/// and solar-system (planets) layouts, and Cloud rows 0/2 (seed,
/// bodies); preset rows, separators, restart, the random rolls and the
/// other groups have no field.
/// @param group The dropdown group of the clicked row.
/// @param index The row index within the group.
/// @param preset The selected preset (option rows are preset-specific).
/// @return The field the row edits, or nullopt.
std::optional<TextField> fieldForItem(MenuGroup group, int index,
                                       core::PresetName preset);

/// @brief The dropdown group that hosts a field: seed/bodies live in
/// Cloud, separation/planets in Options. Used by the click-away law:
/// clicks on the field's row OR its group header keep editor focus.
/// @param field The focused field.
/// @return The owning menu group.
MenuGroup menuGroupOfField(TextField field);

}  // namespace nbody::app
