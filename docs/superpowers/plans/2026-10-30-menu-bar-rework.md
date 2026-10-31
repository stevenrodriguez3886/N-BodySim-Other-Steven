# Menu Bar Rework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the in-window menu bar so menus hold one topic each (Simulation = presets + restart), preset options live in a per-preset "Options" header shown in the bar, every header/row has a tooltip, and inline text fields support a full caret + mouse drag-select with copy/paste.

**Architecture:** Pure layout/hit-test functions in `MenuBar.cpp` (new `MenuGroup::Options`, content-sized headers), state/label/tooltip tables in `MenuModel.cpp`, a new pure caret/selection text model in `TextField.cpp` (`FieldText` struct), and thin event/render wiring in `SimulationApp.cpp`. All behaviour-logic stays unit-testable headless (Catch2); only pixel rendering stays in the app.

**Tech Stack:** C++17, SFML 3.0.2 (FetchContent), Catch2 v3.7.1 (FetchContent), CMake 3.28.3, aarch64 host.

**Spec:** `docs/superpowers/specs/2026-10-30-menu-bar-rework-design.md`

## Global Constraints

- Base commit: work starts from `107fd21` (the two throwaway commits `8630b03`, `30fb652` are dropped by `git reset --hard 107fd21` in Task 1 — user-approved).
- SFML 3.0.2: no `sf::Uint8` (use `std::uint8_t`); `sf::Text` has no built-in selection — selection bands are drawn rectangles positioned from glyph advances.
- Menu logic must remain pure functions in `src/app/MenuBar.cpp` / `MenuModel.cpp` / `TextField.cpp` so tests run headless (`ctest --test-dir build`).
- Build: `cmake --build build -j`; tests: `ctest --test-dir build --output-on-failure`. Baseline on 107fd21: 114/114 passing. Never commit `.superpowers/`.
- Existing staging laws must not regress: paused writes go to staged mirrors; Space-resume folds staged in contract order (preset→integrator→dt→seed→bodies→options); `restart scene` keeps staged; toggles flip staged-if-staged-else-live.
- No right-click context menu anywhere. No new tooltip styling (0.5 s dwell, existing box style). No new fonts/glyphs.
- Commit messages: `feat(menu): ...` / `test(menu): ...` / `refactor(menu): ...`, one commit per task.

## Review Focus

- **Options open, preset changes:** switching preset while the Options dropdown is open must close/refresh the dropdown (no stale rows from the previous preset, no dropdown for a now-hidden group) — expected: behaves exactly like today's Simulation-preset-click.
- **Paused + Options edits:** every Options field/toggle while paused stages (chip shows `planets → N` etc.) and Space-resume applies it once — same laws as today's Simulation option rows, just a different group.
- **Click-away law with moved fields:** fields now live in Options; clicks inside the row or on the Options header keep focus, everything else cancels-then-processes as today.
- **Grammar-invalid caret edits:** insert/delete/paste that would break the field grammar leave buffer AND caret AND selection untouched (including insert mid-buffer of a char the field forbids, e.g. `-` after position 0 in seed).
- **Drag beyond the field:** mouse-drag past the field's right/left edge or off the row clamps the selection to the buffer; releasing outside still leaves the editor focused (press-based focus rules unchanged).
- **Paste overflow into a field with a selection:** sanitized paste that doesn't fit the room left by the replacement deletes only as much as fits; empty sanitized result just clears the selection.
- **Preset switch while a field editor is focused:** cancelling rules from 107fd21 still hold (field cancelled; the new Options header readout reflects the new preset immediately).

---

### Task 1: MenuBar pure layout — Options group, Simulation cleanup, content-sized headers

**Files:**
- Modify: `include/nbody/app/MenuBar.hpp` (enum, new decls)
- Modify: `src/app/MenuBar.cpp`
- Modify: `tests/app/test_menu_bar.cpp`

**Interfaces:**
- Consumes: nothing new from 107fd21.
- Produces: `enum class MenuGroup : std::uint8_t { Simulation, Integrator, Physics, Cloud, Options, Count_ }` (Options LAST so Cloud/Options swap the same last slot: random-cloud → Cloud visible; two-body/solar/fig8 → Options visible; never both). `bool menuGroupVisible(MenuGroup, core::PresetName)` updated. `int menuItemCount(MenuGroup, core::PresetName)`: Simulation = 6 fixed (4 presets, separator at index 4, restart at 5); Options = `menuPresetOptionCount(preset)` (two-body 2, solar 2, fig8 1, cloud 0). `const char* menuOptionsHeaderLabel(core::PresetName)` → `"Solar"` / `"Two-body"` / `"Fig-8"` / `""`. `menuLayout` keeps its signature; separator rows: Simulation index 4 only (in `disabledRows`). Header widths become content-sized: `kMinimumHeaderWidth = 90.0`, width = max(90, text extent + 16) using the existing static label table + a `kValueExtent` constant per header (no font access in MenuBar.cpp — values use fixed extents: seed 19 chars, planets+pluto readout 12, sep 7 → see step 3). `kHeaderGap` 8.0.

- [ ] **Step 1: Reset the branch to the base commit**

```bash
git status --short   # must be clean; if dirty, stash nothing — the tree changes are the throwaway revert, discard them
git branch backup-discard-8630b03 30fb652   # keep the discarded work recoverable
git reset --hard 107fd21
cmake --build build -j && ctest --test-dir build --output-on-failure   # expect 114/114 PASS
```

- [ ] **Step 2: Write the failing layout tests**

In `tests/app/test_menu_bar.cpp` (Catch2, follow existing TEST_CASE style):

```cpp
TEST_CASE("group visibility: Options is the complement of Cloud") {
    const auto presets = {core::PresetName::TwoBody, core::PresetName::SolarSystem,
                          core::PresetName::RandomCloud, core::PresetName::FigureEight};
    for (auto p : presets) {
        CHECK(menuGroupVisible(MenuGroup::Options, p) ==
              !menuGroupVisible(MenuGroup::Cloud, p));
        CHECK(menuGroupVisible(MenuGroup::Options, p) ==
              (menuPresetOptionCount(p) > 0));
    }
}

TEST_CASE("Simulation menu is presets+separator+restart for every preset") {
    for (auto p : allPresets) {
        CHECK(menuItemCount(MenuGroup::Simulation, p) == 6);
    }
}

TEST_CASE("Options item counts follow the preset") {
    CHECK(menuItemCount(MenuGroup::Options, core::PresetName::TwoBody) == 2);
    CHECK(menuItemCount(MenuGroup::Options, core::PresetName::SolarSystem) == 2);
    CHECK(menuItemCount(MenuGroup::Options, core::PresetName::FigureEight) == 1);
    CHECK(menuItemCount(MenuGroup::Options, core::PresetName::RandomCloud) == 0);
}

TEST_CASE("layout: only one of Cloud/Options heads the last slot, never a 6th header") {
    for (auto p : allPresets) {
        auto layout = menuLayout(800, 600, std::nullopt, p);
        CHECK(layout.headers.size() == 4);           // always exactly four headers
        CHECK(layout.headerGroups.back() != MenuGroup::Count_);
        bool hasCloud = contains(layout.headerGroups, MenuGroup::Cloud);
        bool hasOpts  = contains(layout.headerGroups, MenuGroup::Options);
        CHECK(hasCloud != hasOpts);
    }
}

TEST_CASE("layout: no tab jumps when the preset changes") {
    // Same header x-positions across presets: Options and Cloud occupy the same slot.
    auto a = menuLayout(800, 600, std::nullopt, core::PresetName::RandomCloud);
    auto b = menuLayout(800, 600, std::nullopt, core::PresetName::SolarSystem);
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(a.headers[i].topLeft.x == b.headers[i].topLeft.x);
}

TEST_CASE("Simulation separator row is index 4 only") {
    auto layout = menuLayout(800, 600, MenuGroup::Simulation, core::PresetName::SolarSystem);
    CHECK(layout.disabledRows.size() == 1);  // Options dropdown: zero separators
    auto opts = menuLayout(800, 600, MenuGroup::Options, core::PresetName::SolarSystem);
    CHECK(opts.disabledRows.empty());
}
```

(Add a small `allPresets` helper / `contains` lambda mirroring the existing file's idioms.) Also update every pre-existing test in the file whose expectations assume the old Simulation row counts — they are wrong for the new layout and should be rewritten, not deleted.

- [ ] **Step 3: Run to verify they fail** — `ctest --test-dir build -R menu_bar --output-on-failure` → FAIL (old enum/counts).

- [ ] **Step 4: Implement in MenuBar.cpp/MenuBar.hpp**

- Extend the enum with `Options` before `Count_`; `kGroupLabels` gains `"Options"`... **no** — the Options header label is preset-dependent (`menuOptionsHeaderLabel`), so `menuGroupLabel(Options)` returns `"Options"` as fallback only; `SimulationApp` prefers `menuOptionsHeaderLabel` when non-empty.
- `menuGroupVisible`: `Options` → `menuPresetOptionCount(preset) > 0`.
- `menuItemCount`: Simulation fixed 6; Options = option count; others unchanged.
- Header layout: iterate groups in enum order, skipping invisible — because Options/Cloud are mutually exclusive and last, the visible list is always 4 entries at identical x positions.
- Content-sized widths: `headerWidth(group)` = max(90.0, labelExtent + valueExtent + 16). Keep a `constexpr double kHeaderExtent[]` per group sized from the static labels + widest plausible value (Cloud seed value: 20 chars ≈ 130 at 12 px avg; keep the 150 width Cloud had; Options: 12 chars ≈ 80). This is a deterministic table, no font measurement in the pure layer. Separator row: Simulation index 4 (both `disabledRows` pushes collapse to the one).

- [ ] **Step 5: Run tests** — full `ctest` → menu_bar PASS; menu_model may still fail (Task 2 owns it). menu_bar must be green before committing.

- [ ] **Step 6: Commit** — `git add -A && git commit -m "feat(menu): Options group in pure layout; Simulation = presets + restart"`

---

### Task 2: MenuModel — Options rows, labels, apply routing, field mapping

**Files:**
- Modify: `src/app/MenuModel.cpp`
- Modify: `include/nbody/app/MenuModel.hpp` (comment only, if needed)
- Modify: `src/app/TextField.cpp` (`fieldForItem`, `menuGroupOfField`)
- Modify: `tests/app/test_menu_model.cpp`, `tests/app/test_text_field.cpp`

**Interfaces:**
- Consumes (Task 1): `MenuGroup::Options`, `menuItemCount(Options, preset)`, `menuPresetOptionCount`, `menuOptionsHeaderLabel`.
- Produces: `menuApply` accepts `MenuAction{Options, 0..1}`; `menuItemLabel(state, Options, 0)` = `"separation: <v>"` / `"planets: <n>"`, `(..., Options, 1)` = `"path: orbit|hyperbola"` / `"pluto: ✓|✗"`; `menuItemTooltip` non-empty for all Options rows (reuses `kSeparationTooltip` etc.); `menuTickedIndex(state, Options)` = -1 (no dot rows; toggle state lives in the labels, as today); `fieldForItem(Options, 0, preset)` = Separation|Planets, `(Options, 1, preset)` = nullopt (toggles are buttons); `menuGroupOfField(Separation|Planets)` = `MenuGroup::Options`; `Simulation` rows: 0-3 presets, 4 separator, 5 restart — `menuItemLabel(state, Simulation, 5)` = `"restart scene"`.

- [ ] **Step 1: Write the failing tests** in `test_menu_model.cpp`:

```cpp
TEST_CASE("Simulation rows are presets + separator + restart only") {
    MenuState s; s.preset = core::PresetName::SolarSystem;
    for (int i = 0; i < 4; ++i)
        CHECK(menuItemLabel(s, MenuGroup::Simulation, i) ==
              menuPresetName(static_cast<core::PresetName>(i)));
    CHECK(menuItemLabel(s, MenuGroup::Simulation, 4).empty());  // separator
    CHECK(menuItemLabel(s, MenuGroup::Simulation, 5) == "restart scene");
    CHECK(menuApply(s, {MenuGroup::Simulation, 5}, false) == MenuEffect::SceneRestart);
}

TEST_CASE("Options rows carry the preset option labels and effects") {
    MenuState s; s.preset = core::PresetName::SolarSystem; s.planetCount = 5;
    CHECK(menuItemLabel(s, MenuGroup::Options, 0) == "planets: 5");
    s.pluto = true;
    CHECK(menuItemLabel(s, MenuGroup::Options, 1) == "pluto: \u2713");
    CHECK(menuApply(s, {MenuGroup::Options, 1}, false) == MenuEffect::PlutoChanged);
    s.preset = core::PresetName::TwoBody;
    CHECK(menuItemLabel(s, MenuGroup::Options, 0) == "separation: 10");
    CHECK(menuApply(s, {MenuGroup::Options, 1}, false) == MenuEffect::PathChanged);
}

TEST_CASE("Options toggles keep the staging law") {
    MenuState s; s.preset = core::PresetName::SolarSystem;
    CHECK(menuApply(s, {MenuGroup::Options, 1}, true) == MenuEffect::Staged);
    CHECK(menuStaged(s).pluto == true);
    CHECK(menuApply(s, {MenuGroup::Options, 1}, true) == MenuEffect::Staged);
    CHECK(menuStaged(s).pluto == false);  // flip vs staged, last-write-wins
}

TEST_CASE("fields moved to Options") {
    CHECK(fieldForItem(MenuGroup::Options, 0, core::PresetName::SolarSystem) == TextField::Planets);
    CHECK(fieldForItem(MenuGroup::Options, 0, core::PresetName::TwoBody) == TextField::Separation);
    CHECK(!fieldForItem(MenuGroup::Options, 1, core::PresetName::SolarSystem));
    CHECK(!fieldForItem(MenuGroup::Simulation, 5, core::PresetName::SolarSystem));  // restart, no field
    CHECK(menuGroupOfField(TextField::Planets) == MenuGroup::Options);
    CHECK(menuGroupOfField(TextField::Separation) == MenuGroup::Options);
    CHECK(menuGroupOfField(TextField::Seed) == MenuGroup::Cloud);
}
```

Also rewrite pre-existing tests that click option rows via `MenuGroup::Simulation` (they move to Options), and extend the existing staged-fold-order test to include an Options-edited field.

- [ ] **Step 2: Verify they fail** — `ctest --test-dir build -R menu_model --output-on-failure`.

- [ ] **Step 3: Implement** — move the Simulation option-row branches (labels, tooltips, `menuApply` case rows 5/6) into the Options group in `menuApply`/`menuItemLabel`/`menuItemTooltip`/`menuTickedIndex`; Simulation becomes the fixed 6-row table. In `TextField.cpp`, rewrite `fieldForItem` (Cloud unchanged; Options row 0 = field, row 1 = toggle) and `menuGroupOfField`. Check every `openGroup`-dependent path in `MenuModel.cpp` for exhaustive-switch warnings (GCC `-Wall -Wextra` is on; add Options cases, no `default:` swallowing `Count_`).

- [ ] **Step 4: Verify** — `ctest` → all green (menu_model + previously green suites; app-level SimulationApp row-index assumptions are fixed in Task 3 — if `SimulationApp.cpp` still compiles green tests, good; if a compile error appears from the enum, fix the switch there minimally, no behaviour yet).

- [ ] **Step 5: Commit** — `git commit -am "feat(menu): option rows moved from Simulation to Options group"`

---

### Task 3: Tooltips everywhere + app-side Options header wiring

**Files:**
- Modify: `src/app/MenuBar.cpp` (`kHeaderTooltips`)
- Modify: `src/app/MenuModel.cpp` (`menuItemTooltip` tables)
- Modify: `src/app/SimulationApp.cpp` (header label/value rendering incl. Options readout; open-group close on preset change)
- Modify: `tests/app/test_menu_model.cpp`, `tests/app/test_menu_bar.cpp`

**Interfaces:**
- Consumes (Tasks 1-2): Options group end-to-end.
- Produces: `menuHeaderTooltip` non-empty for all 5 groups; `menuItemTooltip` non-empty for every non-separator row of every group (preset rows get one-line scene descriptions); `SimulationApp::menuValue(state, Options)` = `"8 · pluto ✓"`-style readout for solar, `"2.5"`-style for two-body/fig-8 (builder: append the toggle word for two-body path only if it fits; value-first, live model).

- [ ] **Step 1: Write the failing coverage test** — a table-driven test asserting non-empty tooltips:

```cpp
TEST_CASE("every visible header and row has a tooltip") {
    for (auto p : allPresets) {
        MenuState s; s.preset = p;
        CHECK(!menuHeaderTooltip(MenuGroup::Simulation).empty());
        CHECK(!menuHeaderTooltip(MenuGroup::Integrator).empty());
        CHECK(!menuHeaderTooltip(MenuGroup::Physics).empty());
        CHECK(!menuHeaderTooltip(MenuGroup::Cloud).empty());
        CHECK(!menuHeaderTooltip(MenuGroup::Options).empty());
        for (auto g : {MenuGroup::Simulation, MenuGroup::Integrator,
                       MenuGroup::Physics, MenuGroup::Cloud, MenuGroup::Options}) {
            if (!menuGroupVisible(g, s)) continue;
            for (int i = 0; i < menuItemCount(g, s); ++i) {
                const bool separator = g == MenuGroup::Simulation && i == 4;
                if (separator) continue;
                CHECK(!menuItemTooltip(g, i, s).empty());
            }
        }
    }
}
```

- [ ] **Step 2: Verify fail**, then **Step 3: Implement**: header tooltips for Integrator ("Choose the integration method"), Physics ("Choose how much time each step advances"), Options ("Options for the selected simulation"); preset row tooltips (`kTwoBodySceneTooltip` … reuse the wording style from the discarded commit: short scene descriptions, verified fresh here, not restored from it); Integrator/Physics row tooltips (rk4/euler, one per dt fraction e.g. "1/30 — 30 steps per second"); Options row tooltips reuse `kSeparationTooltip`/`kPathTooltip`/`kPlanetsTooltip`/`kPlutoTooltip`. App side: `menuValue` gains the Options branch (solar: `std::to_string(planetCount) + " · pluto " + (pluto ? "✓" : "✗")` — check `font_` glyph availability at runtime; if the font lacks ✓/✗ use the existing dot convention by rendering `pluto on/off`, decided in the live app during Step 5); header label uses `menuOptionsHeaderLabel`; opening Options renders fields/toggles via existing row machinery with Task 2's `fieldForItem`. Preset change while Options open: close `openGroup` (same code path as Cloud hiding).

- [ ] **Step 4: Verify** — `ctest` green; then run the live app on DISPLAY :1 (`./build/nbody`) and eyeball: Options header appears for solar/two-body/fig-8, disappears for random-cloud without the other tabs moving; hover shows tooltips on every row. Fix tooltip-anchor edge cases (tooltip near right/bottom window edge flips side — mirror existing renderTooltip clamps).

- [ ] **Step 5: Commit** — `git commit -am "feat(menu): tooltips on every header and row; Options header readout"`

---

### Task 4: TextField pure caret model (`FieldText`)

**Files:**
- Modify: `include/nbody/app/TextField.hpp`
- Modify: `src/app/TextField.cpp`
- Create: `tests/app/test_text_field_caret.cpp` (registered automatically by catch_discover_tests)

**Interfaces:**
- Consumes: existing grammar laws (`fieldAppendChar` semantics, `trimLeadingZeros`, clamps, `fieldSanitizePaste`, parse/initial fns — all UNCHANGED).
- Produces:

```cpp
struct FieldText {
    std::string buffer;
    std::size_t caret = 0;    // 0..buffer.size()
    std::size_t anchor = 0;   // selection anchor; caret != anchor == selecting
    bool hasSelection() const { return caret != anchor; }
    std::size_t selStart() const;  // min(anchor, caret)
    std::size_t selEnd() const;    // max(anchor, caret)
};
std::string fieldCopyText(const FieldText& t);       // selection, else whole buffer
void fieldDeleteSelection(FieldText& t);             // collapses to caret
bool fieldInsertChar(TextField f, char ch, FieldText& t);
    // selection first (replace), then insert at caret; grammar-checked on the
    // RESULTING string; false + untouched if invalid. Grammar laws identical
    // to fieldAppendChar (sign only at buffer start; one dot; leading-zero trim;
    // length cap) — e.g. inserting '-' at caret>0 in Seed is rejected.
bool fieldEditDelete(TextField f, FieldText& t, bool forward);
    // true if anything changed: selection deleted, else one char
    // right (forward) / left (backspace) of caret. Backspace keeps today's
    // empty-stays-empty law.
bool fieldMoveCaret(FieldText& t, int delta, bool select);  // Left/Right (+select=shift-select)
void fieldMoveCaretTo(FieldText& t, bool toEnd, bool select);  // Home/End
bool fieldApplyPaste(TextField f, const std::string& pasted, FieldText& t);
    // replace selection (grammar-sanitize the pasted text AND the splice
    // result against the field grammar; overflow trims the paste, never the
    // buffer); caret lands after the inserted text; false if nothing changed.
std::size_t fieldIndexForFraction(double fraction, std::size_t length);
    // click x within the field -> index: clamp(round(fraction * length)); pure
    // so the pixel math stays in the app but the clamp law is unit-tested.
```

`FieldText` starts from the existing `fieldInitial*` prefills (caret/anchor = length = end, as today's caret display implies).

- [ ] **Step 1: Write failing tests** covering: insert at 0/middle/end; insert rejected leaves buffer+caret+anchor untouched; sign rule mid-buffer (`fieldInsertChar(Seed,'-',t at caret 2)` false); dot rule; leading-zero trim still fires on mid-buffer insert; selection replace on type; delete selection vs char both directions; empty-buffer backspace no-op; move/home/end with and without select; copy text = selection else buffer; paste replaces selection, paste overflow trims paste not buffer (e.g. Bodies buffer "12", full selection, paste "3456" → "345" grammar room 4 → "345"?? — pin the exact law the test demands: **buffer max length wins; paste truncated to remaining room**; `fieldIndexForFraction` clamping incl. negative/NaN-free contract (input is finite by contract, caller clamps).

- [ ] **Step 2: Verify fail** → **Step 3: Implement** (reuse `trimLeadingZeros`; write a shared `bool grammarOk(TextField, const std::string&)` used by insert/paste paths; keep `fieldAppendChar` as a thin wrapper for existing callers until Task 5 removes them — then delete it in Task 5's commit).
- [ ] **Step 4: Verify** `ctest -R text_field` green → **Step 5: Commit** `git commit -am "feat(menu): pure caret/selection model for field editors"`.

---

### Task 5: SimulationApp — mouse selection, caret keys, selection-aware clipboard

**Files:**
- Modify: `include/nbody/app/SimulationApp.hpp` (members: `FieldText fieldText_`, drag state `bool fieldSelecting_`, `double fieldAdvance_`)
- Modify: `src/app/SimulationApp.cpp`
- Modify: `tests/app/test_text_field.cpp` (call-site updates only)

**Interfaces:**
- Consumes (Task 4): `FieldText`, `fieldInsertChar`, `fieldEditDelete`, `fieldMoveCaret`, `fieldMoveCaretTo`, `fieldApplyPaste`, `fieldCopyText`, `fieldIndexForFraction`.
- Produces: user-visible behaviour (verified by hand in the live app — this task's "test" is the manual checklist + the pure laws already pinned in Task 4).

- [ ] **Step 1: Replace `fieldBuffer_` with `fieldText_`** everywhere (`openFieldEditor` prefills with caret/anchor = end; `commitFieldEdit`, `handleFieldClipboard` copy = `fieldCopyText(fieldText_)`, cut = copy + `fieldDeleteSelection` (or clear all when no selection), paste = `fieldApplyPaste`). Remove `fieldAppendChar`/`fieldBackspace` from the header once call sites are gone.

- [ ] **Step 2: Keyboard** — while a field is focused route Left/Right/Home/End (with Shift held = select, via `sf::Keyboard::isKeyPressed(Shift)`) and Delete in addition to Backspace through the Task-4 fns. Typing (TextEntered) = `fieldInsertChar`. Every one of these clears nothing on its own that Task 4 laws don't clear.

- [ ] **Step 3: Mouse** — in the field-focused MouseDown handler: left-press inside the focused field row → index = `fieldIndexForFraction((x - fieldInnerLeft) / fieldInnerWidth, buffer.size())` (compute `fieldInnerWidth` from `fieldAdvance_ * buffer.size()` — measure once at editor open as `space-advance × 0.6`... builder: measure with the real font via `font_->getGlyph('0', 13, false).advance` averaged at open; store `fieldAdvance_`); sets caret=anchor=index, `fieldSelecting_ = true`; MouseMoved while held → caret = index, anchor stays; MouseUp → `fieldSelecting_ = false` (selection kept). Pressed elsewhere = today's click-away law unchanged. Double-press (`sf::Event::MouseButtonPressed::firstButton`/clickCount via two-press window — SFML 3.0 has no double-click event: detect manually with last-click time+position < 250 ms / 4 px) → select all.

- [ ] **Step 4: Render** — inside the focused-field block: draw selection band rectangle(s) between `fieldAdvance_ * selStart` and `* selEnd` (color `kFieldSelectionColor(58, 96, 168, 255)`, row height minus 4 px inset, drawn BEFORE the text); caret glyph drawn at `fieldAdvance_ * caret` (replace the end-of-buffer "|" append — the typed string draws from the buffer only, caret is a 1 px accent-colored RectangleShape, same blink law).

- [ ] **Step 5: Manual verify on DISPLAY :1** — checklist: click mid-number moves caret; type inserts there; drag highlights; double-click selects all; Ctrl+C/V round-trips into the title-bar-free clipboard (verify by pasting into seed field from a terminal-set clipboard via `xclip -sel clip`); selection + type replaces span; arrows move caret; no right-click menu anywhere; drag off the row and release = still focused. Fix fallout, keep pure laws as the source of truth for any disagreement.

- [ ] **Step 6: Commit** — `git commit -am "feat(menu): caret + mouse text selection in field editors"`.

---

### Task 6: Spacing polish + full verification pass

**Files:**
- Modify: `src/app/MenuBar.cpp` (final width table), `src/app/SimulationApp.cpp` (row padding constants)
- Modify: `tests/app/test_menu_bar.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: final bar/row geometry: header x = prev right + 8; dropdown rows: plain-row text x = row left + 10; dot rows (toggles/presets/integrator/physics) dot at +10, text at +30; field rows render per Task 5; Options dropdown rows same.

- [ ] **Step 1: Failing layout tests** asserting gap = 8 between adjacent headers, min width 90, and that the Options dropdown width uses the Options slot width (>= 140 item min like the others).
- [ ] **Step 2: Implement**, run full `ctest` (expect 114 + new, all green, zero new warnings), then full manual pass against the spec's four asks: (1) tooltip on every header/row of all menus; (2) Simulation = 4 presets + restart only; (3) Options header shows planets/pluto/sep like Cloud shows seed, per preset; (4) mouse select/copy/paste in every field incl. seed. Verify staged-chip + Space-resume still behave for an Options edit while paused (Review Focus line 2).
- [ ] **Step 3: Commit** — `git commit -am "feat(menu): even header spacing; final geometry"`.
