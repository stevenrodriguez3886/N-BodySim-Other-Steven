# In-Window Options Menu (Layout A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An always-visible top-bar menu (Preset / Integrator / Physics / Random seed) in the SFML window that hot-changes options mid-run, staging preset/integrator changes while paused and applying them on resume.

**Architecture:** Reuse the proven two-layer split. Two new SFML-free files in `nbody::app` — `MenuBar` (pure geometry + hit-testing against window size) and `MenuModel` (pure state, actions, staging rules) — both unit-tested headlessly like `Camera`/`StepPlanning`. `SimulationApp` shrinks to glue: routes mouse/key events through the model, draws rects/text from it. `nbody::core` gains only two additive setters on `PhysicsWorld` (integrator hot-swap, dt change).

**Tech Stack:** C++17, existing CMake targets, Catch2 v3 (already wired), SFML 3.0.

**Spec:** `docs/superpowers/specs/2026-10-29-in-window-menu-design.md` (read it with this plan; it is the authority). Mockup reference: `docs/mockups/menu-designs.html` (layout A).

## Global Constraints

- C++17; `-Wall -Wextra -Wpedantic` warning-free; `nbody_core` links NOTHING and includes no SFML; `nbody_app` keeps SFML PRIVATE.
- SFML 3 idioms only (`pollEvent()` → `std::optional<sf::Event>`, `getIf<T>()`, scoped enums). Forbidden: `waitEvent`, `event.type ==` anywhere in `src/`.
- Menu geometry values (verbatim): strip at y=6, height 28, gap 6 between headers; header x starts at 10; dropdown at y = 6+28+4 = 38, item height 24, item padding 8; fixed header widths {Preset 150, Integrator 130, Physics 150, Seed 110} px — values are drawn right-clipped inside the fixed width, never resized (this makes layout a pure function of window width only).
- Menu item lists (verbatim): Preset = two-body, solar-system, random-cloud, figure-eight. Integrator = rk4, euler. Physics = 1/30, 1/60, 1/120, 1/240, 1/480, then a separator row, then "restart scene" (index 6 = restart). Seed = "− 1" (index 0), "+ 1" (index 1); seed never wraps below 0.
- Staging rule: while PAUSED, preset/integrator/dt/seed actions are staged (last-write-wins per group), never applied; resume applies all staged atomically in order preset→integrator→dt→seed. While RUNNING, actions apply immediately.
- Restart (Physics item 6 and the R key) = rebuild from the menu's CURRENT (preset, seed) via `createPreset` — replaces the old `presetBodies_` launch cache entirely.
- Escape: closes an open dropdown if one is open; otherwise quits (existing behavior). Space/R semantics unchanged and work with a dropdown open.
- Clicks hitting the strip or an open dropdown never start a camera drag (menu precedence).
- Headless path and CLI parsing: zero behavior change.
- Every task ends with a commit (feat:/test:/docs: prefixes); never commit `.superpowers/`.

## Review Focus

1. **Click routing precedence** — a press on a header/dropdown must not also begin a camera pan drag, and a press on the scene must close the dropdown. Pinning test in Task 4's manual checklist + hit-test table tests in Task 2.
2. **Staged-change leaks** — a staged change must not be applied early by R, by resume, twice, or left dangling after a second pause. Task 3's staging tests pin each path.
3. **Restart-source change (R and menu) silently altering R's old meaning** — after any menu edit, R must replay the NEW options; a test in Task 4 asserts preset switch + R yields the new preset's initial checksum.
4. **Off-list dt from CLI** (`--dt=5`) — header shows raw value, no dropdown tick; applying a menu dt snaps to a list value. Task 3 test pins the representation.
5. **Dropdowns rendering outside a tiny window** — geometry must clamp so dropdown bottoms stay within the window (grow upward when space below is short); Task 2 resize test pins clipping (headers) and the upward-flip rule (items).

---

## Task 1: Core setters — PhysicsWorld::setIntegrator / setTimeStep

**Files:**
- Modify: `include/nbody/core/PhysicsWorld.hpp` (add two public methods + doc comments)
- Modify: `src/core/PhysicsWorld.cpp` (implement)
- Test: `tests/core/test_world.cpp` (append cases; keep `world_` prefix)

**Interfaces:**
- Consumes: existing `PhysicsWorld`, `makeIntegrator(IntegratorKind)`, `IIntegrator::step`.
- Produces: `void PhysicsWorld::setIntegrator(std::unique_ptr<IIntegrator> integrator);` (asserts non-null, takes ownership, effective next `step()`) and `void PhysicsWorld::setTimeStep(double dt);` (asserts `dt > 0 && std::isfinite(dt)`, effective next `step()`). Task 3/4 call both.

- [ ] **Step 1: Append failing tests to `tests/core/test_world.cpp`:**
  - `world_set_integrator_swaps_midflight`: start an RK4 world on the two-body scene, run 100 steps, `setIntegrator(makeIntegrator(IntegratorKind::Euler))`, run 1 more step, assert `integrator().name() == "euler"` and that the just-completed step moved positions exactly as explicit Euler would from the pre-step state (positions advanced by old-v·dt first — copy the ordering-assert pattern from `test_euler.cpp`'s `euler_explicit_order_check`).
  - `world_set_timestep_changes_advance`: `setTimeStep(1.0/60.0)`; after one `step()`, `simulationTime()` advanced by exactly 1/60 and `timeStep()` returns 1/60.
- [ ] **Step 2:** `ctest --test-dir build -R world` → the two new cases FAIL to compile (no members). Record this output.
- [ ] **Step 3:** Implement both setters (5 lines each: asserts + assignment; `timeStep_` is `cfg_.timeStep_` per existing member naming — read the file first). Header comments: state "effective from the next step(); hot-swap is safe because IIntegrator implementations are stateless across steps except scratch buffers, which the new instance owns."
- [ ] **Step 4:** `ctest --test-dir build` → all pass (expect 46/46). Build warning-free.
- [ ] **Step 5: Commit** `git add -A && git commit -m "feat: hot-swap setters on PhysicsWorld"`

**Edge cases:** `setIntegrator(nullptr)` → assert fires (Debug test not required; comment it). `setTimeStep(0/NaN/neg)` → assert (document; Release builds rely on the existing parseArgs bound — this is defense in depth).

## Task 2: MenuBar — pure geometry + hit-testing

**Files:**
- Create: `include/nbody/app/MenuBar.hpp`, `src/app/MenuBar.cpp` (add to `nbody_app` sources in root `CMakeLists.txt`)
- Create: `tests/app/test_menu_bar.cpp` (add to `nbody_app_tests` in `tests/CMakeLists.txt`)

**Interfaces:**
- Consumes: `nbody::core::Vec2` (pixel coords in a `Vec2` — documented convention, no SFML types anywhere in this file pair).
- Produces (namespace `nbody::app`):
```cpp
enum class MenuGroup : std::uint8_t { Preset, Integrator, Physics, Seed, Count_ };
struct MenuRect { core::Vec2 topLeft; core::Vec2 size; bool contains(core::Vec2 p) const; };
enum class MenuHitKind : std::uint8_t { None, Header, Item };
struct MenuHit { MenuHitKind kind = MenuHitKind::None; MenuGroup group{}; int index = -1; };
struct MenuLayout { std::vector<MenuRect> headers;                 // always 4, in enum order
                    std::optional<MenuGroup> openGroup;
                    std::vector<MenuRect> items;                   // empty unless openGroup
                    std::vector<MenuRect> disabledRows; };         // separators (hit like Item, index -1)
MenuLayout menuLayout(double winW, double winH, std::optional<MenuGroup> openGroup);
MenuHit menuHitTest(const MenuLayout&, core::Vec2 p);
int menuItemCount(MenuGroup);   // Preset 4, Integrator 2, Physics 7, Seed 2
const char* menuGroupLabel(MenuGroup);           // "Preset" etc.
std::string menuItemLabel(MenuGroup, int index, std::uint32_t seed);  // e.g. "1/120", "restart scene", "- 1"
```
  All `constexpr`-where-possible constants from Global Constraints. Includes: none beyond `<vector>`, `<optional>`, `<string>`, `<cstdint>`, Vec2.hpp. `menuLayout` rules: fixed header widths {150,130,150,110} laid out left-to-right from x=10, gap 6; headers past `winW` are still emitted (clipped by the renderer — pin in test); dropdown item width = header width of its group, min 140; if `winH − 38 < items·24`, items flip upward: last row's bottom = 6 (grow above the strip) — pin in test.
- Test file includes ONLY Catch2 + MenuBar.hpp (compile-proves SFML-free).

- [ ] **Step 1: Write failing tests** (`menu_` prefix): `menu_header_geometry` (4 headers, x/widths verbatim at winW=800); `menu_hit_header_and_item` (center-points hit expected group/index, 1px outside → None); `menu_closed_layout_has_no_items`; `menu_narrow_window_clips_headers` (winW=300: all 4 emitted, 4th extends past 300); `menu_short_window_flips_items_upward` (winH=140 with Physics open: item rects have top < 38 and bottoms ≤ 34); `menu_item_labels` (menuItemLabel(Physics, 6) == "restart scene", (Seed,0) == "- 1", (Preset,1) == "solar-system"); `menu_hit_disabled_row` (separator row index 6 of Physics… actually separator is index 5 = row between 1/480 and restart: define Physics rows: 0-4 dt values, 5 separator (disabled), 6 restart; a hit on the separator returns kind Item with index 5 and is in `disabledRows` — caller ignores it).
- [ ] **Step 2:** Register, run `-R menu` → compile fail; record.
- [ ] **Step 3:** Implement per rules above (pure arithmetic; ~80 lines).
- [ ] **Step 4:** `ctest --test-dir build` → all pass (53/53 expected). Warning-free.
- [ ] **Step 5: Commit** `feat: pure menu layout and hit-testing`

## Task 3: MenuModel — state, actions, staging

**Files:**
- Create: `include/nbody/app/MenuModel.hpp`, `src/app/MenuModel.cpp` (register in `nbody_app`)
- Create: `tests/app/test_menu_model.cpp` (register in `nbody_app_tests`)

**Interfaces:**
- Consumes: `MenuGroup` (Task 2), `core::PresetName`, `core::IntegratorKind`.
- Produces (namespace `nbody::app`):
```cpp
struct MenuState {
    core::PresetName preset = core::PresetName::TwoBody;
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;
    double dt = 1.0 / 120.0;            // may be off-list (CLI); then no dt item is ticked
    std::uint32_t seed = 42;
    std::optional<MenuGroup> openGroup; // which dropdown is open
    std::uint32_t restartTick = 0;      // incremented by the restart action; app watches it
};
struct MenuAction { MenuGroup group; int index; };
enum class MenuEffect : std::uint8_t { None, PresetChanged, IntegratorChanged, DtChanged,
                                      SeedChanged, SceneRestart, Staged };
// Running: preset item -> PresetChanged (value already in state), integrator ->
// IntegratorChanged, dt -> DtChanged, seed +/- -> SeedChanged. Paused: all four
// return Staged (value only in staged slots).
// Paused => preset/integrator/dt/seed actions STAGE (last-write-wins per group,
// restartTick NOT bumped); restart action applies instantly even paused (it is
// a scene action, and staging a restart makes no sense). Running => apply now.
MenuEffect menuApply(MenuState& s, const MenuAction& a, bool paused);
struct MenuStaged {   // what resume will apply
    std::optional<core::PresetName> preset; std::optional<core::IntegratorKind> integrator;
    std::optional<double> dt; std::optional<std::uint32_t> seed;
    bool empty() const; };
const MenuStaged& menuStaged(const MenuState&);
MenuStaged menuTakeStaged(MenuState&);   // resume path: returns + clears staged
const char* menuStagedSummary(const MenuState&);  // "" or e.g. "preset → solar-system" (for the chip/HUD)
int menuTickedIndex(const MenuState&, MenuGroup);  // -1 when off-list/unmatched; Physics off-list dt → -1; Physics 5/6 never ticked
bool menuIsStagedActive(const MenuState&);
```
  Internal staged fields + summary live in the .cpp with a fixed sentence format: `"preset → X, integrator → Y, dt → 1/Z, seed → N"` (only non-empty groups, comma-separated).
- Produces test hooks needed by Task 4: everything above is the app↔model boundary.

- [ ] **Step 1: Write failing tests** (`model_` prefix):
  - `model_apply_live_preset`: running, apply (Preset,1) → `preset==SolarSystem`, effect PresetChanged, staged empty. Mirror cases for the other three live effects: (Integrator,0)→IntegratorChanged, (Physics,2)→DtChanged with dt==1/120, (Seed,1)→SeedChanged.
  - `model_apply_stages_while_paused`: paused, apply (Preset,3) → staged().preset==FigureEight, `s.preset` UNCHANGED, effect Staged.
  - `model_stage_last_write_wins`: paused, (Preset,1) then (Preset,2) → staged().preset==RandomCloud (single slot).
  - `model_take_staged`: after staging dt and seed, `menuTakeStaged` returns both AND clears (`menuStaged(s).empty()`); staged state survives a second pause→no-resume without duplication.
  - `model_restart_applies_paused`: paused, (Physics,6) → restartTick incremented, effect SceneRestart, NOT staged.
  - `model_seed_stepper`: seed=0 + (Seed,0) → seed stays 0 (no wrap); seed=7 + (Seed,1) → 8.
  - `model_offlist_dt`: dt=5.0 → `menuTickedIndex(Physics) == -1`; after applying (Physics,0) dt==1/30 and tick==0.
  - `model_staged_summary`: staged preset+dt → summary string exactly `"preset → figure-eight, dt → 1/240"` form (pick one concrete pair and pin the literal).
- [ ] **Step 2:** Register; `-R model` compile fail; record.
- [ ] **Step 3:** Implement (~120 lines; preset dt list `{1/30,1/60,1/120,1/240,1/480}` as a static array; Physics index 5 = ignore/None effect).
- [ ] **Step 4:** `ctest --test-dir build` → 61/61. Warning-free.
- [ ] **Step 5: Commit** `feat: menu state model with staging semantics`

## Task 4: SimulationApp integration

**Files:**
- Modify: `include/nbody/app/SimulationApp.hpp` (replace `presetBodies_` with `MenuState menuState_`; add `void handleMenuHit(...)` / draw members: `sf::Font* font_` already exists)
- Modify: `src/app/SimulationApp.cpp` (event routing, menu draw, apply/staged execution, restart semantics, live title)
- Modify: `src/main.cpp` (pass preset name + seed into the app ctor so MenuState can be seeded from CLI)
- Test: none new (integration is manual + existing suites must stay green; existing 44 physics tests untouched)

**Interfaces:**
- Consumes: Tasks 1–3 exact signatures (`menuLayout`, `menuHitTest`, `menuApply`, `menuTakeStaged`, `menuStagedSummary`, `menuTickedIndex`, `setIntegrator`, `setTimeStep`, `createPreset`, `makeIntegrator`).
- Produces: ctor `SimulationApp(core::SimulationSetup setup, core::PresetName preset, std::uint32_t seed, core::IntegratorKind kind, std::string_view title)` seeding `MenuState` (dt from `setup.config.timeStep`, preset/integrator/seed from CLI args); `run()` unchanged externally.

- [ ] **Step 1: Wiring.** main.cpp constructs the new ctor args from `AppArgs`. In SimulationApp: `menuState_` initialized; REMOVE `presetBodies_`; `restartScene()` = `createPreset(menuState_.preset, menuState_.seed)` → apply config g/softening stay from preset, `world_.reset(bodies)`, refit camera, `accumulator_ = 0`, bump nothing else. R key and (Physics,6) both call `restartScene()`.
- [ ] **Step 2: Event routing (order inside the pollEvent loop matters).** MouseButtonPressed: compute `hit = menuHitTest(menuLayout(w,h,menuState_.openGroup), mousePx)`; if Header → toggle `openGroup` (same header closes); if Item (and not in `disabledRows`) → `menuState_.openGroup = std::nullopt` then `handleAction(action)`; if None → close dropdown AND (existing button==Left) start drag as today. MouseButtonReleased/MouseMoved: unchanged drag logic. KeyPressed: Escape → if `openGroup` clear it ELSE close window (existing); Space → if paused: `resume()`: `auto st = menuTakeStaged(menuState_); apply preset/integrator/dt/seed to world_` in that order (preset swap → `restartScene()`-style rebuild WITHOUT bumping restartTick… preset rebuild IS the application; integrator → `world_.setIntegrator(makeIntegrator(...))`; dt → `setTimeStep`; seed → if preset is random-cloud regenerate via restartScene-without-refit (preserve camera, spec decision), else nothing until next load); `paused_ = !paused_` last. R → `restartScene()`.
- [ ] **Step 3: handleAction (mouse item path).** `auto fx = menuApply(menuState_, action, paused_);` switch: PresetChanged → rebuild world from (new preset, current seed) + refit camera (integrator object unchanged — it hot-continues the new scene); IntegratorChanged → `world_.setIntegrator(makeIntegrator(menuState_.integrator))`; DtChanged → `world_.setTimeStep(menuState_.dt)`; SeedChanged → if `menuState_.preset == RandomCloud`: rebuild bodies via `createPreset` + `world_.reset` WITHOUT refit (camera preserved, spec decision); else state-only. SceneRestart → `restartScene()`; Staged/None → nothing (draw path shows chip if staged).
- [ ] **Step 4: Render (after bodies, before HUD).** Strip bg rect (dark, 60% alpha, height 28, full width), headers (label: `"<Group>: <value>"` via `menuGroupLabel` + current value string; open header highlighted), if `openGroup`: dropdown panel + rows with ✓ from `menuTickedIndex`, separator row from `disabledRows`. While paused AND `menuIsStagedActive`: top-right chip text `menuStagedSummary`. Reuse existing font load; if no font, skip menu text (title-bar HUD already belt-and-braces).
- [ ] **Step 5: Live title/HUD.** Title/stats prefix now built from `menuState_` each 0.5 s window, not the ctor-frozen string.
- [ ] **Step 6:** `ctest --test-dir build` → 61/61 (all pre-existing green — physics untouched). `grep -rn "waitEvent\|event.type" src/` → empty (paste). `./build/nbody --smoke` exit 0; `./build/nbody --headless=100 --preset=solar-system` unchanged stats line.
- [ ] **Step 7: Interactive checklist (this host HAS a display — actually run the app; kill via timeout or its own close).** Verify and mark: [ ] dropdown opens/closes on header click; [ ] preset click mid-run swaps scene, R afterward restarts the NEW preset (watch body count); [ ] pause → change integrator → chip appears → resume applies (HUD integrator updates); [ ] pause → two preset changes → chip shows the LAST one; [ ] click on empty scene closes dropdown without panning (drag still works on scene); [ ] Escape with dropdown open closes it (app survives), Escape twice quits; [ ] Physics → restart scene works while paused; [ ] `--dt=5` launch shows raw 5 un-ticked, menu 1/60 applies.
- [ ] **Step 8: Commit** `feat: in-window top-bar menu (layout A) with staged changes`

**Edge cases:** click that opens a dropdown must not also hit `dragging_` (close branch only when hit==None); resize while open (layout recomputes per frame from live size — no cached rects); seed stepper at 0 floors; applying staged preset+dt where the staged rebuild already resets dt (order preset→…→dt makes dt final — pin in code comment).

## Task 5: README + final validation

**Files:**
- Modify: `README.md` (menu section: layout-A description, grouping, click behavior, staged-while-paused rules table, R-meaning change note, non-goals: mouse-only)
- No code changes (fixes only if validation fails)

- [ ] README: new "In-window menu" section (controls table gains the four groups; R row updated: restarts from CURRENT menu options).
- [ ] Regression: full `ctest` green; determinism double-run `--headless=100000 --preset=random-cloud --seed=42` byte-identical (menu must not perturb headless).
- [ ] Matrix: 8 windowed runs (4 presets × 2 integrators, `timeout 5`) exit 124.
- [ ] One live menu smoke: launch figure-eight, open a dropdown, switch preset, quit — no crash, finite positions (HUD sanity).
- [ ] Commit `docs: menu usage + final validation`.

---

## Explicit non-goals (do not build)

Keyboard menu navigation, tooltips/animation, theming, scrollable dropdowns, touch input, config files, headless menu, multiple dropdowns open at once.
