# Visual Overhaul + Seed Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the solar system a real, visible one (sun + 8 planets that look like planets, banded gas giants, Saturn ring, sun glow) behind a world-locked deterministic starfield; delete the in-window HUD duplicate; replace the seed ± stepper with a typed number field.

**Architecture:** One additive core change (`Body::appearance`, physics-ignored), a rewritten `solarSystem()` preset, an app-layer renderer upgrade (`BodyAppearance` drawing helpers in `SimulationApp.cpp` + new `Starfield.{hpp,cpp}` pure generator unit-tested headlessly), and a seed-editor slice through `MenuBar`/`MenuModel`/`SimulationApp`.

**Tech Stack:** C++17, SFML 3.0.2 (NO EllipseShape, NO stencil groups — 3.1+ only), Catch2 v3, existing targets.

**Spec:** `docs/superpowers/specs/2026-10-29-visual-overhaul-seed-editor.md` (binding). Mockup: `docs/mockups/solar-look.html` (choice C).

## Global Constraints

- C++17; warning-free as today (`-Wall -Wextra -Wpedantic`); `nbody_core` SFML-free; `nbody_app` SFML PRIVATE.
- SFML 3 idioms only; `waitEvent`/`event.type ==` forbidden in `src/`.
- Determinism: two-body/figure-eight/random-cloud headless output byte-identical to current main; random-cloud sampling order untouched. Solar preset stays RNG-free.
- Appearance never affects physics: no branch in core step/merge paths reads it except the merge survivor copy.
- Checksum/stats line format unchanged. Title-bar HUD unchanged. Menu strip geometry/constants unchanged EXCEPT Seed item row count 2→1.
- Staging law unchanged for every group including the new seed editor (Enter commits: live or staged by pause state).
- Every task ends with a commit; never commit `.superpowers/`.

## Review Focus

1. **Appearance must be provably physics-invisible** — a test runs a merged two-body and a cloud step-for-step against appearance-free expectations; merge carries survivor appearance only.
2. **Solar preset stability** — 8 planets bounded ≤ 1.35× initial radius over 2000 steps, 0 merges, both integrators; velocities exactly circular (`v = sqrt(g·M/a)`).
3. **Starfield determinism + culling** — same 600 stars every run (constant seed), no per-frame RNG; culled points never draw off-viewport garbage.
4. **Seed editor event priority** — edit focused: Esc cancels edit (does NOT close dropdown or quit); Enter commits exactly once; digits-only incl. paste-free typing via TextEntered; overflow saturates; click-away cancels-then-processes.
5. **No HUD regression** — title stats identical format; on-screen duplicate gone; staged chip still title-independent.

---

## Task 1: Core — Body::appearance + solar-system rewrite

**Files:** Modify `include/nbody/core/Body.hpp` (Appearance struct + member), `src/core/PhysicsWorld.cpp` (merge survivor carries appearance — verify merge copies survivor's other fields; adjust), `src/core/Presets.cpp` (solarSystem rewrite), `tests/core/test_world.cpp`, `tests/core/test_presets.cpp`.

**Interfaces:** Produces `Body::Appearance{uint32_t color; bool glow; bool ring; uint8_t bands;}` (defaults 0/false/false/0). Consumed by Tasks 2–3 renderer and unchanged physics.

- [ ] Tests first: `world_merge_keeps_survivor_appearance` (small vs big body, survivor = big, its color/glow flags survive, merged body has survivor's appearance); `preset_solar_eight_bodies_circular` (8 bodies, sun first with glow flag + 0xFFD75E-ish color nonzero, each planet |v| == sqrt(gM/a) within 1e-12, positions on radii {6, 9.3, 12.3, 15.6, 23.4, 30, 38.4, 46.2}±1e-9, all angles distinct per the literal list); update `preset_solar_planets_bounded` for 8 planets (≤1.35× at 2000 steps) and assert `merges == 0`; add `preset_solar_no_rng` (createPreset(SolarSystem, 7) == createPreset(SolarSystem, 999) body-for-body).
- [ ] Implement per spec D.1/D.2 (angle literals: planets 0..7 offset 0, 40, 80, 130, 170, 215, 260, 305 degrees; color constants exactly as spec list).
- [ ] Full `ctest` green (count grows by 3). Warning-free. Commit `feat: per-body appearance + real solar system preset`.

## Task 2: Renderer — glow/ring/shading/bands/min-size + starfield + HUD removal

**Files:** Modify `src/app/SimulationApp.cpp` (body draw loop → `drawBody()` helper, remove on-screen HUD text block); create `include/nbody/app/Starfield.hpp` + `src/app/Starfield.cpp` (pure: `std::vector<Star> starfield()` returning 600 world-space stars from constant seed 0x57415A53; `struct Star{Vec2 pos; sf-free color bytes; uint8_t sizePx;}`), register + test in `tests/app/test_starfield.cpp`.

- [ ] `star_` tests: `star_count_and_bounds` (600, |coord| ≤ 30000), `star_determinism` (two calls identical), `star_variety` (≥ 2 distinct colors, sizes ∈ {1,2}, at least 50 dimmed alpha < 0.5).
- [ ] Body drawing per spec D.3 exactly (min px floor 3.0/6.0, glow 3 circles alpha 24/48/96, ring VertexArray annulus 1.45×–2.3× y-squash 0.38 rot −18° behind + front arc after, shading sector alpha 60 pointing away from origin, band chords at ±0.4r width 0.88·chord).
- [ ] Delete the on-screen HUD text block (keep title update path in updateStats). Starfield drawn first (Points + quads, viewport-culled), under bodies.
- [ ] `ctest` green; `--headless=100000 --preset=random-cloud --seed=42` byte-identical to main (paste before/after). 4 presets windowed (timeout 3, exit 124). Commit `feat: planet-style rendering, world-locked starfield; drop on-screen HUD`.

## Task 3: Seed number editor (MenuBar/MenuModel/app)

**Files:** `src/app/MenuBar.cpp`+tests (Seed count 2→1, label "type a seed"), `include/nbody/app/MenuModel.hpp`/`.cpp`+tests (remove Seed stepper actions → `menuSetSeed(MenuState&, uint32_t, bool paused)`; menuTickedIndex(Seed)→-1; summary unchanged), `src/app/SimulationApp.{hpp,cpp}` (edit-buffer members: `bool seedEditing_ = false; std::string seedBuffer_;` TextEntered/KeyEntered routing, caret draw on 0.5 s blink, commit/cancel paths), `src/main.cpp` untouched.

- [ ] `model_set_seed_live_and_staged` (running → SeedChanged + state.seed set, paused → Staged + staged.seed set, state untouched; last-write-wins twice). `menu_seed_single_row` (menuItemCount(Seed)==1, label verbatim, hit row → Item 0). Update the two old stepper tests (`model_seed_stepper_floors_at_zero`/`saturates_high`) to call `menuSetSeed` (floor case → set 0 works; add saturate-on-parse guard is app-side, model just takes the value).
- [ ] App priority law (Review Focus 4): when `seedEditing_`: TextEntered digit → append (cap 10 chars, leading zeros trimmed), Backspace deletes, Enter → parse (strtoull saturate to UINT32_MAX) → menuSetSeed + close editor (apply-or-stage by paused_), Esc → cancel edit ONLY (dropdown stays open, app survives); else existing Escape law. Click outside the field → cancel edit, then normal click handling (menu/drag). While editing, Space/R/other keys go to the editor (typing 'r'/' ' is digits-only so Space inserts nothing; R must NOT restart while editing — comment it).
- [ ] Field renders under the Seed header at the dropdown slot geometry (reuse MenuRect row), border accent color, caret `|` blinking via existing stats clock.
- [ ] `ctest` green; grep forbidden idioms empty. Commit `feat: typed seed field replaces stepper`.

## Task 4: Docs + validation

- [ ] README: menu section — seed row now typed (Enter/Esc semantics + priority note), solar preset description (8 planets, colors, ring, bands), starfield note (deterministic, world-locked), controls table: HUD line notes title-bar-only, R/Escape rows unchanged.
- [ ] Matrix: determinism double-run byte-identical; 8×2 windowed exit 124; sanitized (build-san ASan/UBSan) `nbody_core_tests` clean (solar preset math); live: `--preset=solar-system` shows 8 visible bodies + stars, seed editor open/commit/cancel paths, Escape priority.
- [ ] Commit `docs: visual overhaul + seed editor`.
