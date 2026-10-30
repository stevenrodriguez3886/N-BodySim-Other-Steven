# Visual Overhaul + Seed Editor — Design Spec

Date: 2026-09-29 (feature follow-up to the in-window menu). Status: approved by user via mockup choices (docs/mockups/solar-look.html): style **C (banded gas giants)**, **compressed orbit spacing**, **world-locked starfield**.

## Problem statements (user-reported)

1. The solar-system preset "shows only one body" — diagnosis: physics is correct (5 bodies, 0 merges at 20k steps); planets render sub-pixel at scene scale, so only the sun is visible.
2. The on-screen HUD text duplicates the window title — remove it.
3. The Random-seed dropdown's ± buttons are clunky — replace with a typed numeric field.
4. The solar system is generic (sun + 4 identical dots) — model OUR solar system (sun + Mercury..Neptune), make planets look like planets, add stars in the background.

## Design decisions (binding)

### D.1 Core: per-body appearance (additive, physics-ignored)
`Body` gains a sixth member:
```cpp
struct Appearance {
    std::uint32_t color = 0u;  // 0xRRGGBB; 0 = "use palette by id % 8" (clouds unchanged)
    bool glow = false;         // star-style radial glow
    bool ring = false;         // Saturn-style elliptical ring
    std::uint8_t bands = 0;    // 0 none; 2–3 latitude stripes (gas giants)
};
Appearance appearance;
```
- Default-constructed = all-default → existing presets/tests keep compiling and behaving identically (aggregate inits with 5 initializers still valid).
- Physics never reads it. Collision/merge: the merge SURVIVOR's appearance is carried (survivor = larger body; tie → lower id). Mass-weighted color blending is out of scope.
- `checksum`/stats/headless output: unchanged (appearance excluded from checksum by construction — checksum sums position only).

### D.2 Solar-system preset rewrite (compressed spacing, chosen)
Sun (mass 500, glow) + 8 planets on circular orbits, masses 1e-4..8e-3 (Jupiter heaviest, all ≤ 1e-2·sun so orbits stay clean), orbital radii compressed: a = 6·(1, 1.55, 2.05, 2.6, 3.9, 5.0, 6.4, 7.7) ⇒ Mercury..Neptune, sqrt-style compression of real AU, ordering preserved. v = sqrt(g·M_sun/a) tangential (+y), bodies start at angles 0..360 spread (fixed offsets per planet, deterministic literal list — no RNG, preset stays seed-free). Radii for collisions stay `radiusFor(mass, 0.05)` (tiny; guarantees no merges at these orbits). Colors (0x hex): Sun ffd75e, Mercury 9c8e82, Venus e6c47a, Earth 5f8dd6, Mars c96a4a, Jupiter d8a568 (bands 3), Saturn e3ce9a (bands 2, ring), Uranus 9ad6d2, Neptune 5a7bd6.
Existing test `preset_solar_planets_bounded` must be updated to 8 planets and stay green (bounded = position radius ≤ 1.35× initial for 2000 steps).

### D.3 Renderer (app layer; SFML 3.0.2 constraint rulings)
SFML 3.0.2 has NO EllipseShape and NO stencil/group APIs (both are 3.1+). Rulings:
- **Glow**: 3 concentric CircleShapes (alpha 24/48/96, radii 3.2/2.1/1.4× body radius, body color).
- **Ring**: VertexArray triangle-strip annulus (inner 1.45×, outer 2.3× body radius), y-squash 0.38, rotated −18°, drawn BEHIND the planet; front arc (points below ring center line) redrawn after the planet. One static vertex layout, transformed per body.
- **Shading (day/night)**: 180° CircleShape sector, alpha 60, radius 0.98×, rotated to point away from the dominant attractor (world origin — the sun). At planet sizes (4–16 px) the straight terminator is imperceptible.
- **Bands**: chord RectangleShapes (width 2·√(r²−d²)·0.88 at latitudes d = ±0.4r), fully inside the circle — no stencil needed.
- **Minimum visible size**: drawn radius = max(physical radius × scale, 3.0 px); suns keep 2× that floor. Physical collisions unaffected.
- **Starfield**: 600 stars, world-locked (chosen): fixed-seed (constant 0x5741_5A53) mt19937 generates positions uniform in ±30 000 world units, sizes 1–2 px screen-constant, colors from 4-tone palette, alpha 0.2–0.8. Drawn as VertexArray::Points + small quads, culled to the current viewport, before bodies. Pan and zoom move them (world-locked); they never regenerate.
- **HUD text**: the on-screen duplicate of the title is deleted; title-bar stats stay exactly as-is. The paused staged-chip stays (it is title-independent). Menu/dropdown drawing unchanged (already polished on menu-polish branch).

### D.4 Seed editor (replaces ± stepper)
- MenuBar: Seed dropdown becomes ONE row (an input row; menuItemCount(Seed): 2→1; label "type a seed"). Hit semantics unchanged (Item, index 0).
- MenuModel: `MenuEffect menuSetSeed(MenuState&, std::uint32_t seed, bool paused)` — same staging law: running → apply immediately (SeedChanged), paused → stage (Staged, last-write-wins). Old (Seed, 0/1) stepper actions are removed; menuTickedIndex(Seed) → -1 always.
- App: clicking the seed row focuses an inline text field (dropdown-style box under the seed header, caret blink, digits only, max 10 digits, parsed with saturation to UINT32_MAX). Enter commits (apply or stage by pause state, camera preserved per existing SeedChanged rules); Esc cancels the edit — **edit-cancel takes priority over dropdown-close and quit** when an edit is focused. Any scene/header click also cancels the edit (then processes the click). While paused with a staged seed, the chip shows it as before.
- CLI `--seed=` unchanged. Seed steppers in docs/README updated.

## Non-goals
Moons, asteroid belt, textures/shaders, orbital trails, elliptical orbits, realistic scale ratios, mouse wheel over menus, seed history UI.

## Validation
Determinism contract for random-cloud + figure-eight + two-body byte-identical. Solar preset has no RNG — double-run check still applies. 8×2 windowed matrix, sanitized core suite (ASan/UBSan) for the new preset math, updated unit tests (appearance carry-through on merge, preset stability, seed model replacement, MenuBar 1-row seed layout), interactive pass for the editor + visuals.
