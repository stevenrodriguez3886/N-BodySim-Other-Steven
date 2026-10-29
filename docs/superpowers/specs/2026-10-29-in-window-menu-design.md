# Design: In-Window Options Menu (layout A) — 2026-10-29

## Intent & why

Today every choice (preset, integrator, dt, seed) is fixed at launch via CLI
flags; trying a different integrator or preset means closing the window and
restarting from a terminal. The user wants to drive those choices from inside
the running simulator: click grouped menu options, see what's available in a
dropdown, and interrupt a live simulation to change options without quitting.

Chosen layout (from three clickable mockups the user reviewed —
`docs/mockups/menu-designs.html`, layout A selected): a **top-bar menu strip**
of group headers across the top of the window; clicking a header drops its
available choices over the scene.

## Goals

- Menu is always visible during a windowed run; reachable mid-simulation.
- Four groups as headers, current value shown in each header:
  **Preset** (4 choices), **Integrator** (RK4 | Euler), **Physics** (time-step
  discrete values + "restart scene" action), **Random seed** (number cycling
  via −/+ or slider-free stepper).
- Click header → dropdown of choices; current choice marked (✓). Click a
  choice → applies per the apply-rules below. Click outside → dropdown closes.
- Gentle mid-run changes (the user's explicit ask):
  - Not paused: apply live. Preset switch swaps the scene (world reset, sim
    timer reset, camera refit). Integrator switch hot-swaps the integrator —
    it continues the *current* state (this intentionally lets you freeze
    Euler's drift damage by switching to RK4 mid-spiral). dt applies from the
    next step. Seed regenerates random-cloud immediately; other presets show
    the new seed but note it takes effect on next random-cloud load.
  - Paused (Space): preset/integrator changes are **staged**, not applied —
    shown as a dashed "staged → applies on resume" chip top-right. dt and
    seed stage trivially too. On resume (Space), all staged changes apply in
    one batch, last-write-wins per group.
- CLI unchanged and still authoritative for the initial state; the menu just
  edits what the CLI would have set. Headless mode is untouched (no menu).

## Non-goals

- Keyboard menu navigation (arrows/Enter) — mouse only this round.
- Tooltips, animations, fade transitions, theme settings.
- New physics features, trails, pause overlay beyond the existing title/HUD.
- Mobile/touch input.

## Anti-goals

- The menu must never permanently shrink the rendered scene (dropdowns overlay
  and vanish; the strip is one row tall).
- The menu must not turn the render loop into an event-driven UI framework —
  it stays a stateless-per-frame draw of a small state struct.

## Constraints

- Existing architecture stands: `nbody_core` (SFML-free, tested) /
  `nbody_app` (SFML PRIVATE) split; SFML 3 event idioms; `-Wall -Wextra
  -Wpedantic` warning-free; C++17.
- Menu geometry/hit-testing must be pure math unit-testable headlessly (same
  trick as `Camera`/`StepPlanning`): a `MenuBar` value type in `nbody::app`
  computing header/dropdown rects + hit results from window size and state —
  SimulationApp only draws and forwards clicks.
- Core additions kept minimal and additive: `PhysicsWorld::setIntegrator(...)`
  hot-swap and `setTimeStep(...)` (asserts dt > 0, finite). No physics change.
- Mouse-drag pan must not trigger when a click lands on the menu strip or an
  open dropdown (hit-test precedence: menu > camera drag).

## Decisions made in conversation (user's)

- Menu exists during a run, not a startup-only gate (startup = same menu,
  nothing is running yet — a "paused at t=0" state would have been the only
  alternative; rejected as clunkier).
- Layout A (top bar) chosen over side panel and corner dock.
- Gentle behavior for mid-run changes = stage-while-paused, apply-on-resume;
  live-apply when running.

## Decisions made by me (assistant), flagged as such

- Seed folded as its own top-level group (mockup parity) rather than nested
  in Preset's dropdown — keeps every group header/body shape identical.
- Staged changes are last-write-wins per group (not a queue).
- Physics group's dt offers discrete values (1/30, 1/60, 1/120, 1/240, 1/480)
  as a radio list rather than a continuous slider — deterministic, matches
  existing `--dt` validation (0, 10].
- Dropdown max height with scroll is out; ≤ 5 items fit, no scrolling needed.
- "Restart scene" (dropdown action AND the existing R key) restarts from
  (currentPreset, currentSeed) — the menu-edited options are the source of
  truth, exactly as if the CLI had been re-run with them. The launch-time
  body cache is replaced by the (preset, seed) pair; R's meaning updates
  accordingly. This resolves the seed-change-then-R corner consistently.
- Escape closes an open dropdown; it quits only when no dropdown is open.
  (Standard desktop convention; prevents losing a run from inside a menu.
  Space/R keep their raw meaning even with a dropdown open.)
- If the live dt is not one of the five menu values (e.g. launched with
  --dt=5), the header shows the raw value and no dropdown item is ticked.
- Seed stepper: step size 1 (plain ±), unsigned range. Regenerating
  random-cloud by seed PRESERVES the current camera view (pan/zoom kept) —
  it is the same kind of scene, not a scene swap; preset switches still
  refit the camera.
- Narrow windows: no responsive magic — headers keep fixed width and the
  strip clips/overflows past the right edge below a minimum width;
  documented, with one resize test pinning the clipping behavior.

## Left to the builder

- Exact widget layout/padding/colors, font sizes, hover/pressed visuals.
- Internal decomposition of MenuBar (rect structs, enum ids) and its tests.
- How SimulationApp routes ClickPressed vs drag-start (one mouse-press
  handler with menu precedence is enough).
