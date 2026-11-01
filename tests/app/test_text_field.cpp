#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>

#include "nbody/app/MenuBar.hpp"
#include "nbody/app/TextField.hpp"

using namespace nbody::app;

namespace core = nbody::core;

namespace {

// Type a whole string char by char, returning the accepted suffix.
// Append-mode typing: caret/anchor parked at the end, where
// fieldInsertChar IS the old fieldAppendChar law (that pair is now
// file-private inside TextField.cpp).
std::string typeAll(TextField field, const std::string& chars) {
    FieldText text;
    for (const char ch : chars) {
        fieldInsertChar(field, ch, text);  // caret == anchor == size()
    }
    return text.buffer;
}

}  // namespace

TEST_CASE("textfield_seed_grammar", "[app][textfield]") {
    // Digits plus at most one leading '-'; 19 chars max; leading zeros trim.
    CHECK(typeAll(TextField::Seed, "12345") == "12345");
    CHECK(typeAll(TextField::Seed, "-42") == "-42");
    CHECK(typeAll(TextField::Seed, "1-2") == "12");   // '-' only at position 0
    CHECK(typeAll(TextField::Seed, "-1-") == "-1");
    CHECK(typeAll(TextField::Seed, "a1b2c") == "12");  // letters never type
    CHECK(typeAll(TextField::Seed, ".5") == "5");     // no dot for seed
    CHECK(typeAll(TextField::Seed, "000123") == "123");
    CHECK(typeAll(TextField::Seed, "0") == "0");
    CHECK(typeAll(TextField::Seed, "000") == "0");
    CHECK(typeAll(TextField::Seed, "-0005") == "-5");
    CHECK(typeAll(TextField::Seed, "-0") == "-0");  // parses 0, sign survives
    // 19-char cap: the 20th char is refused (int64 field, spec D.4).
    const std::string typed = typeAll(TextField::Seed, "12345678901234567890");
    CHECK(typed == "1234567890123456789");
    CHECK(typed.size() == 19);
}

TEST_CASE("textfield_bodies_separation_planets_grammars", "[app][textfield]") {
    // bodies: digits, 4 chars (2..2000 clamped core-side).
    CHECK(typeAll(TextField::Bodies, "300") == "300");
    CHECK(typeAll(TextField::Bodies, "12.5") == "125");  // no dot allowed
    CHECK(typeAll(TextField::Bodies, "-5") == "5");     // no sign allowed
    CHECK(typeAll(TextField::Bodies, "12345") == "1234");
    CHECK(typeAll(TextField::Bodies, "0042") == "42");

    // separation: digits + at most one '.', 7 chars.
    CHECK(typeAll(TextField::Separation, "10.5") == "10.5");
    CHECK(typeAll(TextField::Separation, "1.2.3") == "1.23");  // one dot only
    CHECK(typeAll(TextField::Separation, ".5") == ".5");
    CHECK(typeAll(TextField::Separation, "-5") == "5");  // unsigned field
    CHECK(typeAll(TextField::Separation, "0.0") == "0.0");
    CHECK(typeAll(TextField::Separation, "12345678") == "1234567");
    CHECK(typeAll(TextField::Separation, "00.5") == "0.5");

    // planets: a single digit, nothing else.
    CHECK(typeAll(TextField::Planets, "5") == "5");
    CHECK(typeAll(TextField::Planets, "58") == "5");
    CHECK(typeAll(TextField::Planets, "-3") == "3");
    CHECK(typeAll(TextField::Planets, "1.5") == "1");
}

TEST_CASE("textfield_backspace", "[app][textfield]") {
    // Backspace == fieldEditDelete backward with the caret at the end
    // (the old buffer-only fieldBackspace is gone).
    FieldText text;
    text.buffer = typeAll(TextField::Seed, "-123");
    text.caret = text.buffer.size();
    text.anchor = text.caret;
    CHECK(fieldEditDelete(TextField::Seed, text, false));
    CHECK(text.buffer == "-12");
    CHECK(fieldEditDelete(TextField::Seed, text, false));
    CHECK(text.buffer == "-1");
    CHECK(fieldEditDelete(TextField::Seed, text, false));
    CHECK(text.buffer == "-");
    CHECK(fieldEditDelete(TextField::Seed, text, false));  // down to empty
    CHECK(text.buffer.empty());                            // (commits 0)
    CHECK(!fieldEditDelete(TextField::Seed, text, false));  // empty stays
    CHECK(text.buffer.empty());
}

TEST_CASE("textfield_paste_filter_matches_typing", "[app][textfield]") {
    // Paste = same grammar + overflow rules as typing (D.4).
    CHECK(fieldSanitizePaste(TextField::Seed, "1-2") == "12");
    CHECK(fieldSanitizePaste(TextField::Seed, "-0012ab") == "-12");
    CHECK(fieldSanitizePaste(TextField::Seed,
                             "99999999999999999999999") ==
          "9999999999999999999");  // first-come-first-fit, 19 cap
    CHECK(fieldSanitizePaste(TextField::Bodies, "1,234 bodies") == "1234");
    CHECK(fieldSanitizePaste(TextField::Separation, "  sep=10.5 ") == "10.5");
    CHECK(fieldSanitizePaste(TextField::Planets, "5 planets") == "5");
    CHECK(fieldSanitizePaste(TextField::Seed, "") == "");
    CHECK(fieldSanitizePaste(TextField::Seed, "abc") == "");
}

TEST_CASE("textfield_parsing", "[app][textfield]") {
    CHECK(fieldParseSeed("42") == 42);
    CHECK(fieldParseSeed("-7") == -7);
    CHECK(fieldParseSeed("") == 0);       // empty commits 0 (law preserved)
    CHECK(fieldParseSeed("-") == 0);
    // int64 saturation on overflow (spec D.4).
    CHECK(fieldParseSeed("99999999999999999999999") ==
          std::numeric_limits<std::int64_t>::max());
    CHECK(fieldParseSeed("-99999999999999999999999") ==
          std::numeric_limits<std::int64_t>::min());
    CHECK(fieldParseBodies("") == 0);  // core clamps 0 -> floor 2
    CHECK(fieldParseBodies("300") == 300);
    CHECK(fieldParseSeparation("10.5") == Catch::Approx(10.5));
    CHECK(fieldParseSeparation("") == 0.5);  // empty commits the floor
    CHECK(fieldParseSeparation("7") == Catch::Approx(7.0));
    // Field range [0.5, 500] clamps at the parse boundary (spec D.3):
    // neither the model setters nor core clamp separation.
    CHECK(fieldParseSeparation("0") == 0.5);   // floor (also stops s=0 div)
    CHECK(fieldParseSeparation("1234567") == 500.0);  // ceiling
    CHECK(fieldParseSeparation(".") == 0.5);
    CHECK(fieldParsePlanets("0") == 0);
    CHECK(fieldParsePlanets("8") == 8);
}

TEST_CASE("textfield_initial_values_match_row_labels", "[app][textfield]") {
    CHECK(fieldInitialSeed(-7) == "-7");
    CHECK(fieldInitialBodies(150) == "150");
    CHECK(fieldInitialSeparation(10.0) == "10");     // integers bare
    CHECK(fieldInitialSeparation(10.5) == "10.5");   // 4 sig digits
    CHECK(fieldInitialPlanets(8) == "8");
}

TEST_CASE("textfield_row_mapping_matches_menu_model", "[app][textfield]") {
    // D.1/D.2 field rows: Options row 0 (separation/planets per preset),
    // Cloud rows 0 (seed) / 2 (bodies). Everything else: no field.
    CHECK(fieldForItem(MenuGroup::Options, 0, core::PresetName::TwoBody) ==
          TextField::Separation);
    CHECK(fieldForItem(MenuGroup::Options, 1, core::PresetName::TwoBody) ==
          std::nullopt);  // row 1 is the path TOGGLE, not a field
    CHECK(fieldForItem(MenuGroup::Options, 0,
                       core::PresetName::SolarSystem) == TextField::Planets);
    CHECK(fieldForItem(MenuGroup::Options, 1,
                       core::PresetName::SolarSystem) == std::nullopt);
    CHECK(fieldForItem(MenuGroup::Options, 0,
                       core::PresetName::FigureEight) ==
          TextField::Separation);
    // Simulation is presets + separator + restart only: no field rows.
    for (int row = 0; row < 6; ++row) {
        CHECK(!fieldForItem(MenuGroup::Simulation, row,
                            core::PresetName::TwoBody));
        CHECK(!fieldForItem(MenuGroup::Simulation, row,
                            core::PresetName::SolarSystem));
        CHECK(!fieldForItem(MenuGroup::Simulation, row,
                            core::PresetName::FigureEight));
    }
    CHECK(fieldForItem(MenuGroup::Cloud, 0, core::PresetName::RandomCloud) ==
          TextField::Seed);
    CHECK(fieldForItem(MenuGroup::Cloud, 1, core::PresetName::RandomCloud) ==
          std::nullopt);  // random seed is a button
    CHECK(fieldForItem(MenuGroup::Cloud, 2, core::PresetName::RandomCloud) ==
          TextField::Bodies);
    CHECK(fieldForItem(MenuGroup::Cloud, 3, core::PresetName::RandomCloud) ==
          std::nullopt);  // random bodies is a button

    // Exhaustive cross-check against the live menu model: every field row
    // must live in its group's item range, and the field's host group must
    // agree with where fieldForItem found it.
    for (const core::PresetName preset :
         {core::PresetName::TwoBody, core::PresetName::SolarSystem,
          core::PresetName::RandomCloud, core::PresetName::FigureEight}) {
        for (int g = 0; g < static_cast<int>(MenuGroup::Count_); ++g) {
            const auto group = static_cast<MenuGroup>(g);
            for (int i = 0; i < menuItemCount(group, preset); ++i) {
                const auto field = fieldForItem(group, i, preset);
                if (field) {
                    CHECK(menuGroupOfField(*field) == group);
                }
            }
        }
    }
    CHECK(menuGroupOfField(TextField::Seed) == MenuGroup::Cloud);
    CHECK(menuGroupOfField(TextField::Bodies) == MenuGroup::Cloud);
    CHECK(menuGroupOfField(TextField::Separation) == MenuGroup::Options);
    CHECK(menuGroupOfField(TextField::Planets) == MenuGroup::Options);
}
