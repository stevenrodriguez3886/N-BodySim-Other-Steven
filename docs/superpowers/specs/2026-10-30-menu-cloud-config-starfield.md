# Preset Option Menus + Cloud Config + Starfield Fix — Spec (2026-10-30)

Base: `main` @ `7ca1dfa`. Binding for the plan. User decisions: show/hide (not flyouts); Cloud group name; bodies 2–2000 (random rolls full range); starfield ~600 visible everywhere (density is one named constant — trivially bumpable); signed seeds incl. negatives; hover tooltips; R/resize anomalies investigated verify-first.

## D.1 Menu layout

Headers: `Simulation:` `Integrator:` `Physics:` `Cloud:` (renamed from Preset/Seed; Cloud header+dropdown visible **only while preset == random-cloud**).

**Simulation dropdown** (row list depends on selected preset):
- 0–3: two-body / solar-system / random-cloud / figure-eight (ticked dot = current)
- 4: separator
- 5+: the **selected preset's option rows** (D.2), in listed order
- then separator + `restart` row (moved out of Physics; same effect as R key). Physics loses restart + its separator (7→5 rows).

**Cloud dropdown** (random-cloud only): seed field / `random seed` / `bodies: N` field / `random bodies`.

Hidden groups: menuItemCount excludes Cloud when preset != random-cloud; hit tests and layout never see hidden rows. MenuModel stays pure/table-driven; row layout computed from MenuState (new pure fn `menuPresetOptionCount(preset)`).

## D.2 Preset options (new Simulation rows when that preset is active)

- **two-body**: `separation: [field]` (world units, [0.5, 500], default 10 = today's value); `path: orbit ⇄ hyperbola` toggle row — orbit = equal-mass circular (today); hyperbola = equal-mass inbound with v = 1.6× circular (unbound slingshot demo). Toggled by click, staged like everything else.
- **solar-system**: `planets: [0–8]` field — planet count from the inside out (1=Mercury … 8=full system; 0 = sun only); `pluto: ✓/✗` toggle — Pluto as 9th body at a ≈ 200 world units, dim brown, appears only when planets == 8 (independent toggle row regardless). Default 8, ✗ = today.
- **random-cloud**: options live in the Cloud group (seed + bodies fields & roll buttons). `random seed` = random_device value through the seed law; `random bodies` = uniform [2, 2000].
- **figure-eight**: `separation: [field]` = overall scale factor k on the classic solution (positions ×k, velocities ×√k — Newtonian self-similarity; default 1 = today).

Option changes rebuild the scene immediately-when-running / staged-when-paused with the SAME law (PresetChanged-style rebuild + camera refit, except random-cloud which never refits on its Cloud edits).

## D.3 Core API

- `twoBody(seed, double separation = 10.0, bool hyperbolic = false)`; `solarSystem(seed, int planetCount = 8, bool pluto = false)`; `figureEight(seed, double scale = 1.0)`; `randomCloud(std::int64_t seed, std::uint32_t count = 150)` (count clamped [2,2000]; **prefix contract**: first 150 of (s,2000) == all of (s,150), tested). Defaults reproduce today's byte-exact setups (checksum anchor 5.5936610862797043 for cloud/42/100k must hold; two-body/fig-8 defaults unchanged too).
- Signed seeds: core functions take `std::int64_t`; engine stays std::mt19937 with folding `u32 = seed in [0,2^32) ? uint32(seed) : splitmix64_fold(seed)` → all historical <2^32 checksums unchanged. CLI `--seed` int64 + `--bodies=N` added.
- Planet tables: existing 8 + Pluto row (mass ~1e-5, radius small, a=200, retrograde-ish inclination N/A in 2D — prograde circular like siblings). Solar with planets<8 uses prefix of the same table (id/count ordering stable).
- MenuState gains `separation`, `hyperbolic`, `planetCount`, `pluto` (+staged mirrors); MenuEffects added; staged summary sentence extends with `restart`-style clause listing each changed option (exact strings in plan). Space-resume order unchanged + option rebuilds folded into the preset step (rebuild uses current options).
- Bodies count field clamps [2,2000] (empty→2); separation fields [0.5,500] (accepts decimal point, one only); planets field [0,8].

## D.4 Text fields (shared editor, parameterized)

Fields: seed (digits + leading `-`, 19 chars, int64 saturate), bodies (digits, 4), separation (digits + one `.`, 7 chars), planets (1 digit). Laws as today (Enter commit live/staged, Esc edit-cancel only, click-away cancel-then-process, others swallowed, caret blink) **plus Ctrl+C/X/V** via sf::Clipboard (paste filtered to field grammar; overflow rules same as typing; Ctrl combos never type into the buffer).

## D.5 Starfield (fixes invisible-stars bug)

Tiled procedural: `starsInRect(x0,y0,x1,y1)` — world cells (base 25u) → splitmix64(cell ^ 0x57415A53) decides star (~25% cells) + position/size(1–2px, 70/30)/color(4-palette)/dim(~40%, α120). Cell size doubles by level so ≤ ~30k cells/frame at any zoom; pan exact, zoom pops between levels (documented). ~600 stars per typical screen at current zoom (density constant `kStarDensityNum/Denom`, one-line tunable). Pure function, unit tests: determinism, pan-stability (subset property), density band.

## D.6 Hover tooltips

`menuHeaderTooltip(group)` / `menuItemTooltip(group,row)` pure table (empty ⇒ none): Simulation "Choose the starting configuration"; restart "Rebuild the scene from the current settings"; separation "Distance between the bodies at t=0"; path "Circular orbit or unbound fly-by"; planets "How many planets (inside-out)"; pluto "Include Pluto at ~200 units"; Cloud header "Options for the random-cloud simulation"; its rows per D.1/D.2; Physics/Integrator headers as today; dt/integrator rows none.

App: 0.5 s mouse dwell over header/row → tooltip box (bg 30,34,54 α240, border 1px, 11px text) at cursor+(12,18) clamped on-screen; target change resets dwell; click/scroll/menu-close hides; hover never changes selection.

## D.7 Anomaly investigations (verify-first, fix-only-if-real)

1. **R stall** (caught once: UPS 119→3, t frozen, window alive; unreproducible after): env-gated trace of restart/accumulator/dt; XTEST 'r' via docs/dev-tools/xsyn.c loop ×20; report root cause w/ evidence.
2. **Window resize**: static analysis of Resized/default-view + programmatic `setSize` probe behind a debug env var; screenshot compare pre/post (import/xwd available? else record findings).

## D.8 Validation gates

Tests ≈ 72 → 85+; checksum anchors unchanged (cloud 42 100k = 5.5936610862797043; two-body/fig-8 defaults identical to main); 4 presets windowed exit 124; forbidden idioms empty; hidden-group hit tests; tooltips table totals; prefix contract; int64 seed fold incl. -1 and 2^40; user eyeball: stars visible in solar, paste + negative seed work, Cloud hides on other presets, option rows appear per preset.
