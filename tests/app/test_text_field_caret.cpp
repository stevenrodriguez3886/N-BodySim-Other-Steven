#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "nbody/app/TextField.hpp"

using namespace nbody::app;

namespace {

// Build a FieldText with an explicit caret (and optional selection anchor;
// anchor defaults to the caret, i.e. no selection).
FieldText makeText(const std::string& buffer, std::size_t caret,
                   std::size_t anchor) {
    FieldText text;
    text.buffer = buffer;
    text.caret = caret;
    text.anchor = anchor;
    return text;
}

FieldText caretAt(const std::string& buffer, std::size_t caret) {
    return makeText(buffer, caret, caret);
}

}  // namespace

TEST_CASE("textfield_caret_insert_at_start_middle_end", "[app][textfield]") {
    FieldText text = caretAt("23", 0);
    CHECK(fieldInsertChar(TextField::Bodies, '1', text));
    CHECK(text.buffer == "123");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    text = caretAt("13", 1);
    CHECK(fieldInsertChar(TextField::Bodies, '2', text));
    CHECK(text.buffer == "123");
    CHECK(text.caret == 2);

    text = caretAt("12", 2);
    CHECK(fieldInsertChar(TextField::Bodies, '3', text));
    CHECK(text.buffer == "123");
    CHECK(text.caret == 3);
}

TEST_CASE("textfield_caret_insert_seed_sign_rule", "[app][textfield]") {
    // '-' is legal at buffer position 0 only (resulting-string law).
    FieldText text = caretAt("42", 0);
    CHECK(fieldInsertChar(TextField::Seed, '-', text));
    CHECK(text.buffer == "-42");
    CHECK(text.caret == 1);

    text = caretAt("42", 2);
    CHECK_FALSE(fieldInsertChar(TextField::Seed, '-', text));
    CHECK(text.buffer == "42");
    CHECK(text.caret == 2);
    CHECK(text.anchor == 2);

    text = caretAt("42", 1);
    CHECK_FALSE(fieldInsertChar(TextField::Seed, '-', text));
    CHECK(text.buffer == "42");
    CHECK(text.caret == 1);

    // A second '-' is rejected even at caret 0 (would read "--4").
    text = caretAt("-4", 0);
    CHECK_FALSE(fieldInsertChar(TextField::Seed, '-', text));
    CHECK(text.buffer == "-4");
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);
}

TEST_CASE("textfield_caret_insert_dot_and_char_rules", "[app][textfield]") {
    FieldText text = caretAt("15", 1);
    CHECK(fieldInsertChar(TextField::Separation, '.', text));
    CHECK(text.buffer == "1.5");
    CHECK(text.caret == 2);

    // Second dot anywhere is refused, everything untouched.
    text = makeText("1.5", 0, 0);
    CHECK_FALSE(fieldInsertChar(TextField::Separation, '.', text));
    CHECK(text.buffer == "1.5");
    text = makeText("1.5", 3, 3);
    CHECK_FALSE(fieldInsertChar(TextField::Separation, '.', text));
    CHECK(text.buffer == "1.5");

    // Seed has no dot; letters never pass.
    text = caretAt("12", 1);
    CHECK_FALSE(fieldInsertChar(TextField::Seed, '.', text));
    CHECK(text.buffer == "12");
    CHECK_FALSE(fieldInsertChar(TextField::Bodies, 'a', text));
    CHECK(text.buffer == "12");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);
}

TEST_CASE("textfield_caret_insert_overflow", "[app][textfield]") {
    FieldText text = caretAt("", 0);
    CHECK(fieldInsertChar(TextField::Planets, '5', text));
    CHECK(text.buffer == "5");
    CHECK(text.caret == 1);
    CHECK_FALSE(fieldInsertChar(TextField::Planets, '8', text));
    CHECK(text.buffer == "5");
    CHECK(text.caret == 1);

    text = caretAt("1234", 4);
    CHECK_FALSE(fieldInsertChar(TextField::Bodies, '5', text));
    CHECK(text.buffer == "1234");

    text = caretAt("1234567890123456789", 19);  // 19-digit seed at cap
    CHECK_FALSE(fieldInsertChar(TextField::Seed, '0', text));
    CHECK(text.buffer.size() == 19);
    CHECK(text.caret == 19);
}

TEST_CASE("textfield_caret_insert_replaces_selection", "[app][textfield]") {
    FieldText text = makeText("1234", 4, 0);  // caret > anchor
    CHECK(fieldInsertChar(TextField::Bodies, '9', text));
    CHECK(text.buffer == "9");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    text = makeText("1234", 1, 4);  // anchor > caret
    CHECK(fieldInsertChar(TextField::Bodies, '9', text));
    CHECK(text.buffer == "19");  // span [1,4) replaced at selStart
    CHECK(text.caret == 2);
    CHECK(text.anchor == 2);

    // Grammar-invalid insert keeps buffer AND the selection untouched.
    text = makeText("1234", 4, 0);
    CHECK_FALSE(fieldInsertChar(TextField::Bodies, 'a', text));
    CHECK(text.buffer == "1234");
    CHECK(text.caret == 4);
    CHECK(text.anchor == 0);

    text = makeText("1.23", 3, 2);  // span excludes the dot -> second dot
    CHECK_FALSE(fieldInsertChar(TextField::Separation, '.', text));
    CHECK(text.buffer == "1.23");
    CHECK(text.caret == 3);
    CHECK(text.anchor == 2);
}

TEST_CASE("textfield_caret_leading_zero_trim_law", "[app][textfield]") {
    // Mid-buffer insert never breaks the no-leading-zero-run invariant.
    FieldText text = caretAt("10", 1);
    CHECK(fieldInsertChar(TextField::Bodies, '0', text));
    CHECK(text.buffer == "100");
    CHECK(text.caret == 2);

    // "0" + digit appended at the end -> trim collapses; the caret tracks
    // the erased run: caret 2 after insert, one drop before it -> 1.
    text = caretAt("0", 1);
    CHECK(fieldInsertChar(TextField::Bodies, '5', text));
    CHECK(text.buffer == "5");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    // Inserting '0' into "0" collapses back to "0": grammar-valid insert,
    // accepted, caret lands at 1 (2 minus the one dropped zero).
    text = caretAt("0", 1);
    CHECK(fieldInsertChar(TextField::Bodies, '0', text));
    CHECK(text.buffer == "0");
    CHECK(text.caret == 1);

    // Splice at the very front is a plain insert, no trim fires.
    text = caretAt("0", 0);
    CHECK(fieldInsertChar(TextField::Bodies, '5', text));
    CHECK(text.buffer == "50");
    CHECK(text.caret == 1);

    // Signed seed buffer: "-0" + '5' -> "-05" -> "-5", caret 3 - 1 = 2.
    text = caretAt("-0", 2);
    CHECK(fieldInsertChar(TextField::Seed, '5', text));
    CHECK(text.buffer == "-5");
    CHECK(text.caret == 2);
}

TEST_CASE("textfield_caret_delete_char", "[app][textfield]") {
    FieldText text = caretAt("123", 1);
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/true));
    CHECK(text.buffer == "13");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    // Delete at the right edge is a no-op.
    text = caretAt("123", 3);
    CHECK_FALSE(fieldEditDelete(TextField::Bodies, text, /*forward=*/true));
    CHECK(text.buffer == "123");
    CHECK(text.caret == 3);

    // Backspace deletes left of the caret (the char at index 1 here).
    text = caretAt("123", 2);
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/false));
    CHECK(text.buffer == "13");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    // Backspace at 0 / on an empty buffer: false, untouched.
    text = caretAt("123", 0);
    CHECK_FALSE(fieldEditDelete(TextField::Bodies, text, /*forward=*/false));
    CHECK(text.buffer == "123");
    CHECK(text.caret == 0);
    text = caretAt("", 0);
    CHECK_FALSE(fieldEditDelete(TextField::Bodies, text, /*forward=*/false));
    CHECK_FALSE(fieldEditDelete(TextField::Bodies, text, /*forward=*/true));
    CHECK(text.buffer.empty());

    // Deleting the front digit can expose a zero run; the trim law fires.
    text = caretAt("100", 1);
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/false));
    CHECK(text.buffer == "0");  // "00" collapsed by the trim law
    CHECK(text.caret == 0);

    // Backspacing a lone leading '-': sign goes, caret lands at 0.
    text = caretAt("-5", 1);
    CHECK(fieldEditDelete(TextField::Seed, text, /*forward=*/false));
    CHECK(text.buffer == "5");
    CHECK(text.caret == 0);
}

TEST_CASE("textfield_caret_delete_with_selection", "[app][textfield]") {
    FieldText text = makeText("1234", 3, 1);
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/true));
    CHECK(text.buffer == "14");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    text = makeText("1234", 1, 3);  // anchor > caret, either direction
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/false));
    CHECK(text.buffer == "14");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    text = makeText("12", 2, 0);  // whole-buffer selection
    CHECK(fieldEditDelete(TextField::Bodies, text, /*forward=*/true));
    CHECK(text.buffer.empty());
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);
}

TEST_CASE("textfield_caret_copy_text", "[app][textfield]") {
    CHECK(fieldCopyText(caretAt("42", 1)) == "42");  // no selection: buffer
    CHECK(fieldCopyText(makeText("1234", 3, 1)) == "23");
    CHECK(fieldCopyText(makeText("1234", 1, 3)) == "23");
    CHECK(fieldCopyText(caretAt("", 0)).empty());
}

TEST_CASE("textfield_caret_move_and_select", "[app][textfield]") {
    FieldText text = caretAt("1234", 0);
    CHECK(fieldMoveCaret(text, 1, /*select=*/false));
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    CHECK(fieldMoveCaret(text, 1, /*select=*/true));  // extend
    CHECK(text.caret == 2);
    CHECK(text.anchor == 1);

    CHECK(fieldMoveCaret(text, -1, /*select=*/true));  // anchor survives
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    // Clamped moves that do not change the position report false.
    text = caretAt("1234", 0);
    CHECK_FALSE(fieldMoveCaret(text, -3, /*select=*/false));
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);
    text = makeText("1234", 4, 0);
    CHECK_FALSE(fieldMoveCaret(text, 2, /*select=*/true));
    CHECK(text.caret == 4);
    CHECK(text.anchor == 0);  // anchor law: unchanged on a no-move

    // A plain (select=false) move collapses an active selection.
    text = makeText("1234", 4, 2);
    CHECK(fieldMoveCaret(text, -1, /*select=*/false));
    CHECK(text.caret == 3);
    CHECK(text.anchor == 3);
}

TEST_CASE("textfield_caret_home_end", "[app][textfield]") {
    FieldText text = caretAt("1234", 2);
    fieldMoveCaretTo(text, /*toEnd=*/false, /*select=*/false);
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);
    fieldMoveCaretTo(text, /*toEnd=*/false, /*select=*/false);  // idempotent
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);

    fieldMoveCaretTo(text, /*toEnd=*/true, /*select=*/true);  // Home..End
    CHECK(text.caret == 4);
    CHECK(text.anchor == 0);

    fieldMoveCaretTo(text, /*toEnd=*/false, /*select=*/true);
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);

    fieldMoveCaretTo(text, /*toEnd=*/true, /*select=*/false);
    CHECK(text.caret == 4);
    CHECK(text.anchor == 4);
}

TEST_CASE("textfield_delete_selection_helper", "[app][textfield]") {
    FieldText text = makeText("1234", 3, 0);
    fieldDeleteSelection(text);
    CHECK(text.buffer == "4");
    CHECK(text.caret == 0);  // caret lands at selStart, before the survivor
    CHECK(text.anchor == 0);

    text = makeText("1234", 1, 3);  // anchor > caret
    fieldDeleteSelection(text);
    CHECK(text.buffer == "14");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    text = caretAt("1234", 2);  // no selection: untouched
    fieldDeleteSelection(text);
    CHECK(text.buffer == "1234");
    CHECK(text.caret == 2);
    CHECK(text.anchor == 2);
}

TEST_CASE("textfield_paste_replaces_selection", "[app][textfield]") {
    // Buffer max length wins; the paste is truncated, never the buffer.
    FieldText text = makeText("12", 2, 0);  // select-all, cap 4
    CHECK(fieldApplyPaste(TextField::Bodies, "3456", text));
    CHECK(text.buffer == "3456");
    CHECK(text.caret == 4);
    CHECK(text.anchor == 4);

    text = makeText("1.5", 3, 0);  // separation cap 7
    CHECK(fieldApplyPaste(TextField::Separation, "1234567890", text));
    CHECK(text.buffer == "1234567");
    CHECK(text.caret == 7);

    // No room at all (cap 4, buffer already 4): nothing fits -> untouched.
    text = caretAt("1234", 4);
    CHECK_FALSE(fieldApplyPaste(TextField::Bodies, "56", text));
    CHECK(text.buffer == "1234");
    CHECK(text.caret == 4);

    text = caretAt("12", 1);  // splice mid-buffer, room for 2 of 5
    CHECK(fieldApplyPaste(TextField::Bodies, "34567", text));
    CHECK(text.buffer == "1342");  // "3","4" fit; "5","6","7" truncated
    CHECK(text.caret == 3);

    // Paste at the tail with partial room.
    text = caretAt("123", 3);
    CHECK(fieldApplyPaste(TextField::Bodies, "456", text));
    CHECK(text.buffer == "1234");
    CHECK(text.caret == 4);
}

TEST_CASE("textfield_paste_grammar", "[app][textfield]") {
    // Per-char grammar matches fieldSanitizePaste against the result.
    FieldText text = caretAt("", 0);
    CHECK(fieldApplyPaste(TextField::Seed, "1-2", text));
    CHECK(text.buffer == "12");
    CHECK(text.caret == 2);

    text = caretAt("", 0);
    CHECK(fieldApplyPaste(TextField::Seed, "-0012ab", text));
    CHECK(text.buffer == "-12");
    CHECK(text.caret == 3);

    // Splice grammar: '-' lands at buffer position 0 of the result.
    text = caretAt("42", 0);
    CHECK(fieldApplyPaste(TextField::Seed, "-", text));
    CHECK(text.buffer == "-42");
    CHECK(text.caret == 1);

    // Fully-invalid paste, no selection: false + untouched.
    text = caretAt("12", 1);
    CHECK_FALSE(fieldApplyPaste(TextField::Bodies, "-", text));
    CHECK(text.buffer == "12");
    CHECK(text.caret == 1);
    CHECK(text.anchor == 1);

    // Fully-invalid paste WITH a selection: the span is still cleared.
    text = makeText("12", 2, 0);
    CHECK(fieldApplyPaste(TextField::Bodies, "-", text));
    CHECK(text.buffer.empty());
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);

    // Empty paste: no selection -> false; selection -> cleared, true.
    text = caretAt("12", 1);
    CHECK_FALSE(fieldApplyPaste(TextField::Bodies, "", text));
    CHECK(text.buffer == "12");
    text = makeText("12", 2, 0);
    CHECK(fieldApplyPaste(TextField::Bodies, "", text));
    CHECK(text.buffer.empty());
    CHECK(text.caret == 0);
    CHECK(text.anchor == 0);

    // Splice into the middle, everything fits.
    text = caretAt("14", 1);
    CHECK(fieldApplyPaste(TextField::Bodies, "23", text));
    CHECK(text.buffer == "1234");
    CHECK(text.caret == 3);
}

TEST_CASE("textfield_index_for_fraction", "[app][textfield]") {
    CHECK(fieldIndexForFraction(0.0, 4) == 0);
    CHECK(fieldIndexForFraction(1.0, 4) == 4);
    CHECK(fieldIndexForFraction(0.5, 4) == 2);
    CHECK(fieldIndexForFraction(0.5, 5) == 3);  // rounds half away from zero
    CHECK(fieldIndexForFraction(0.1, 5) == 1);  // 0.5 -> 1
    CHECK(fieldIndexForFraction(-0.3, 4) == 0);
    CHECK(fieldIndexForFraction(1.7, 4) == 4);  // clamps to length
    CHECK(fieldIndexForFraction(std::nan(""), 4) == 0);
    CHECK(fieldIndexForFraction(-1.0, 4) == 0);
    CHECK(fieldIndexForFraction(0.5, 0) == 0);
    CHECK(fieldIndexForFraction(0.999, 1) == 1);
}
