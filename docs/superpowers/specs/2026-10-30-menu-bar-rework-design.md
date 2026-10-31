# Menu Bar Rework — Design (2026-10-30)

Supersedes the menu aspects of the 2026-10-30 preset-options work. The two
commits from the previous session (`8630b03`, `30fb652`) are thrown away: the
branch is reset to `107fd21` (docs-only tip of the prior run) before this work
starts, so no code from those commits survives. This design is written fresh
against that base.

## Intent and the why

The user plays with the simulator through the in-window menu bar and finds it
hard to use: tooltips appear only in parts of the menus, spacing looks uneven,
solar-system options (planets, pluto) are hidden inside the Simulation
dropdown instead of being visible in the bar the way the cloud seed is, and
the seed text box gives no way to select/copy/paste text with the mouse.
They also want the menus to stay clean: each menu holds one topic only.

## Goals

1. **Tooltips everywhere.** Every visible header AND every visible menu row
   (excluding separator rows) has a hover tooltip, on every menu: Simulation,
   Integrator, Physics, Cloud, and the new Options header. Same dwell time and
   rendering as today's tooltips (0.5 s hover, existing tooltip box style).
2. **Menu separation by topic.** The Simulation menu contains ONLY the four
   presets and `restart scene` (plus one separator between them). Nothing
   else. No option rows, no field editors inside Simulation.
3. **Per-preset Options header.** A fifth header appears in the bar only when
   the selected preset has options, styled exactly like the Cloud header
   (label + live value readout, click opens a dropdown):
   - solar-system → header `Solar: 8 · pluto ✓` (or `· pluto ✗`);
     dropdown rows: `planets: <n>` (inline text field) and
     `pluto: ✓/✗` (toggle).
   - two-body → header `Two-body: <sep>`; dropdown rows: `separation: <v>`
     (inline text field) and `path: orbit/hyperbola` (toggle).
   - figure-eight → header `Fig-8: <sep>`; dropdown rows: `separation: <v>`
     (inline text field) only.
   - random-cloud → no Options header (its options live in Cloud, unchanged).
   Header value updates immediately on change.
   Position: LAST in the bar, after Cloud (builder's call from builder
   review: Options and Cloud are mutually exclusive per preset —
   random-cloud shows Cloud, optioned presets show Options — so swapping
   them in the same last slot means no tab ever jumps when presets change;
   the enum gains Options after Cloud so existing index-based layout keeps
   working). Staging-while-paused semantics
   (staged mirrors, Space-resume fold-in, restart keeps staged) carry over
   unchanged from the current MenuModel behaviour; the options just move
   from MenuGroup::Simulation to a new MenuGroup::Options.
4. **Mouse selection in text fields.** In any inline field editor (seed,
   bodies, separation, planets, and the new Options fields):
   - Click places a caret; drag highlights a span with a visible selection
     band (distinct color, behind glyphs); double-click selects the whole
     buffer.
   - Full caret model (user's choice): the editor has a real caret index.
     Left/Right move it, Home/End jump to the ends, typing inserts AT the
     caret, Backspace deletes left of the caret, Delete right of it. When a
     selection is active, typing or deleting replaces the span first.
   - Ctrl+C copies the selection (whole buffer if none); Ctrl+X copies then
     deletes the selection (or whole buffer if none); Ctrl+V pastes AT the
     caret, replacing the selection when one is active (user confirmed full
     text-box behaviour, so paste replaces the span, not the whole buffer).
     Keyboard-only clipboard, no popup menu.
   - NO right-click context menu (explicitly removed / never added).
   - The focused field keeps the existing accent outline + blinking caret so
     it is obvious the box is editable.
5. **Even spacing (my call, listed under "calls made").** Headers are laid
   out with content-sized widths and a uniform gap; dropdown rows use a
   uniform left padding; no reserved blank column where a tick dot is absent.

## Non-goals

- No changes to rendering, physics, presets' numbers, CLI args, or headless
  mode.
- No keyboard navigation of menus (arrows/Enter); mouse stays the only menu
  input, as today.
- No new fonts, glyphs, or tooltip styling.
- Cloud menu content is unchanged (seed field, random seed, bodies field,
  random bodies, seed readout in the bar).

## Anti-goals

- Menus that mix topics again (options smuggled back into Simulation).
- Silent selection state (a selection the user cannot see).
- Extra mouse affordances nobody asked for (right-click menus, scrollbars,
  hover submenus).

## Constraints

- SFML 3.0.2 (no `sf::Uint8`; SFML 3.0.x has no built-in text-selection —
  selection is drawn manually from glyph positions via
  `sf::Font::getGlyph`/`findGlyphsInRange` or per-char advance widths).
  Note for implementer: if glyph-level measurement proves troublesome, an
  isometric fallback (constant advance = space glyph advance × string
  length, matching how text is already positioned) is acceptable for the
  field editors, which hold at most ~19 short chars.
- Existing behaviour that must not regress (locked by current tests):
  pause-staging (`*` chip), Space-resume fold-in order, restart semantics,
  field clamps (separation [0.5,500] field-side, planets [0,8], bodies
  [2,2000] core-side), seed display in Cloud header, title-bar stats.
- Host: aarch64, cmake 3.28.3, FetchContent SFML+Catch2; build
  `cmake --build build -j`, tests `ctest --test-dir build`.
- Menu hit-testing stays pure functions in MenuBar.cpp (layout computed from
  window size + open group + preset), so tests stay headless.

## The parts the user decided

- Throw away both previous-session commits' approach; rebuild fresh.
- Simulation menu = presets + restart only.
- Options get their own header, appearing/disappearing with the selected
  preset (per-preset options header), shown in the bar like the seed is.
- Tooltips on everything; mouse text selection; pluto/planets visible in
  the bar.

## Left to the builder

- Exact tooltip strings for headers/rows (short, plain, matching existing
  tone; user cares about coverage, not wording).
- Exact colors: selection band and Options-header styling must be visually
  distinct from hover/selection row tints already in use; builder picks
  values consistent with the current palette.
- Header width rule: builder's call, but must be uniform in method across
  all five headers (text width + fixed horizontal padding, uniform gap), and
  must not jitter when values change length (e.g. clamp to a minimum width
  per header so opening solar-system doesn't shift neighbouring tabs more
  than once).
- Whether the Options header label uses the preset's short name
  ("Solar"/"Two-body"/"Fig-8") — builder's call; defaults proposed above.
- Selection band metrics (height, inset) — builder's call.

## Calls made by the builder (this design)

- Reset base to `107fd21` (drop the two commits entirely, per user's "throw
  the two commits'"; their test additions go too — new tests written fresh).
- Dropdown row indent: 10 px left padding for plain rows; rows with a tick
  dot keep dot-at-10/text-at-30 (so dot rows and plain rows both look
  intentional, no dangling indent).
- Header gap 8 px, header height 28 px unchanged; min header width 90 px.
- Double-click = select whole buffer (simplest useful mouse affordance;
  drag = span select; no word-boundary logic).
- Middle-click paste: not added.
- Paste-with-selection replaces the span (conventional; follows from the
  full-text-box model the user chose).
- Options header sits last in the bar (see above; zero-jump rationale).
- Caret model is the full one (user's explicit choice in review).
