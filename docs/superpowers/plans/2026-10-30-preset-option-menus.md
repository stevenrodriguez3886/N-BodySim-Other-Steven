# Preset Option Menus Implementation Plan (2026-10-30)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development. Steps are checkboxes.

**Goal:** Per-preset option rows in the Simulation dropdown (two-body separation+path, solar planets+Pluto, fig-8 separation), Seed→Cloud group (shown only for random-cloud) with typed+pasteable fields and random-roll buttons, restart moved into Simulation, hover tooltips, signed seeds, and the tiled-starfield fix for invisible stars.

**Spec:** `docs/superpowers/specs/2026-10-30-menu-cloud-config-starfield.md` (binding).

**Environment facts (verified, don't re-derive):** SFML 3.0.2 (pollEvent/getIf idioms; no EllipseShape/stencils; sf::Clipboard::getString/setString exist in Window module); 72/72 tests; XTEST harness `docs/dev-tools/xsyn.c` (build: `gcc -o /tmp/xsyn docs/dev-tools/xsyn.c /usr/lib/aarch64-linux-gnu/libXtst.so.6 -lX11`; keys work, button clicks do NOT; window via _NET_WM_PID xprop); checksum anchor random-cloud seed42 100k = `5.5936610862797043`; build dir `build`, sanitized `build-san`; ctest from controller (subagents' CMake Tools runs are noisy).

**Global constraints:** C++17, warning-free; core SFML-free; determinism contracts per spec D.3 (defaults byte-identical, prefix contract, seed folding keeps small-seed streams); staging law applies to EVERY new control; no `.superpowers/` commits; each task commits.

## Task 1: Core APIs (signed seeds + preset options)

Files: `include/nbody/core/Presets.hpp`, `src/core/Presets.cpp`, `src/main.cpp` (--seed int64, --bodies), tests `test_presets.cpp`, `test_headless.cpp` if CLI tested.

- [ ] Tests first: seed folding equality for small seeds (createPatch: two createPsetup calls with seed 42 identical to historical — reuse existing checksum tests); negative-seed determinism (createPreset(RandomCloud, -1) twice equal, != seed 1); prefix contract (cloud seed 7 count 2000 bodies[0..149] == cloud seed 7 count 150 bitwise); twoBody(sep=20) = twoBody() positions ×2 exactly, hyperbolic variant |v| = 1.6× circular inbound; figureEight(scale=3): positions ×3 velocities ×√3 vs default; solarSystem(count=3) = first 3 planets prefix + sun; pluto flag adds body id 10 at a≈200 circular v=sqrt(gM/a) only valid with count==8 (else ignored); clamps (count 0→2? count<2 → 2, >2000 → 2000; planets -1→0, 9→8).
- [ ] Implement signatures per spec D.3 (default args keep every existing call site compiling untouched); CLI --seed parses int64 incl '-'; --bodies clamped.
- [ ] ctest green (72 + ~8); anchor checksums re-verified (paste). Commit `feat(core): signed seeds + per-preset option parameters`.

## Task 2: Tiled starfield (visible everywhere)

Files: `include/nbody/app/Starfield.hpp`/`src/app/Starfield.cpp` rewrite, `tests/app/test_starfield.cpp` rewrite, `src/app/SimulationApp.cpp` (stars_ member → per-frame starsInRect call with viewport world-rect from camera + window size).

- [ ] Tests first: purity (same rect+scale → identical vector), pan-subset stability (stars for rect R ⊂ rect R+Δ == stars of R filtered by containment, exact), density band (150×115 rect: 150 ≤ count ≤ 1500 — wide, real ~600), level cap (huge rect ≤ 30000 cells), dim ratio 30–50%, sizes ⊆ {1,2}, palette ⊆ 4 colors.
- [ ] Implement splitmix64 tiling per spec D.5; renderer computes viewport world corners via camera_.toWorld (exists? Camera.hpp has inverse toPixels — use it) and draws culled quads; delete fixed starfield()/stars_ init.
- [ ] ctest green (+5); solar headless run + note expected on-screen count math in report. Commit `fix: tiled procedural starfield visible at any zoom`.

## Task 3: MenuModel + MenuBar (options rows, Cloud group, tooltips)

Files: `include/nbody/app/{MenuBar.hpp,MenuModel.hpp}` + `src/app/{MenuBar.cpp,MenuModel.cpp}`, `tests/app/test_menu_bar.cpp`, `test_menu_model.cpp`.

- [ ] Model first: MenuState option fields + staged mirrors + menuSetBodies/menuSetSeparation/menuTogglePath/menuSetPlanets/menuTogglePluto (all `(state, value, paused)` live/staged law, MenuEffects); Space-resume consumption order preset(uses options)→integrator→dt→seed→bodies→(options already folded into rebuild via state); summary sentence extends per changed option ("planets → 5" etc. exact strings in tests); menuApply handles new row indices (Simulation: options rows dynamic — menuItemCount(Simulation, state) preset-dependent; Cloud count 4 but hidden when preset != RandomCloud via new `bool menuGroupVisible(group, state)`).
- [ ] Bar: labels/tooltips tables (spec D.6 strings verbatim); hit tests respect visibility + dynamic counts; toggle rows show current state in label (path: orbit/hyperbola, pluto: ✓/✗).
- [ ] Tests: visibility matrix (2 presets × 4 groups), Simulation row tables per preset (counts 6/8/7/7 +restart), tooltip completeness (every visible row has string or documented empty), each setter's live/staged/effect triple, summary strings, hidden-group hit None.
- [ ] ctest green (+~10). Commit `feat(menu): preset options, Cloud group, tooltips`.

## Task 4: App integration

Files: `src/app/SimulationApp.{hpp,cpp}`, tests where headless-drivable.

- [ ] Replace seedEditing_ machinery with parameterized field editor (activeField_ enum, buffers per D.4 grammar, one TextEntered/KeyPressed router, commit → correct setter); Ctrl+C/X/V via sf::Clipboard (paste filtered by field grammar); toggle rows via clicks; random-seed/random-bodies button rows (std::random_device → setter); option rows rebuild scene per staging law (live: rebuild+refit except cloud).
- [ ] Tooltips: dwell timer member (0.5 s via frameClock; target = header/row under cursor recomputed on MouseMoved), draw last (above chip), hide rules per D.6.
- [ ] Title: no format change (bodies count already shown). Staged chip must list new option changes (from summary).
- [ ] 4 presets windowed exit 124; ctest green; grep idioms empty. Commit `feat: preset options wired through menu + tooltips + clipboard fields`.

## Task 5: Anomaly probes + docs + validation

- [ ] R-stall probe: env NBODY_TRACE=1 fprintf at restart/planSteps inputs; XTEST send 'r' ×20 with 1 s gaps, capture titles between (pattern from this session: WM_PID → xprop WM_NAME); root-cause writeup (fix only with evidence; if fix: separate commit + tests).
- [ ] Resize probe: static check of Resized/default-view handling + `xwininfo` geometry vs render assumptions; findings documented in report.
- [ ] README: new menu table (rows per preset, tooltips mentioned), cloud group visibility note, paste/negative-seed docs, --bodies flag, starfield note.
- [ ] Validation: full matrix spec D.8 (double-run byte check, 8×2 preset/integrator windowed, sanitized core run, anchors).
- [ ] Commit `docs: preset options menu + anomaly findings`.
