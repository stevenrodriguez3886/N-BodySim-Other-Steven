# n-body

A small 2D n-body gravity simulator in C++17 with an SFML 3 front end:
four presets, two integrators (Euler / RK4), inelastic merge collisions,
pan/zoom camera, and a headless mode for deterministic batch runs.

## Building

Requirements: CMake >= 3.25 and git (SFML is fetched at configure time
unless a system SFML 3 is found; audio is never built — `SFML_BUILD_AUDIO=OFF`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build        # 72 tests
```

The executable is `build/nbody`.

## Command line

| Flag | Values | Default | Meaning |
| --- | --- | --- | --- |
| `--preset=` | `two-body` \| `solar-system` \| `random-cloud` \| `figure-eight` | `two-body` | Which scenario to load |
| `--integrator=` | `euler` \| `rk4` | `rk4` | Integration scheme |
| `--seed=` | integer | `42` | RNG seed (affects `random-cloud`) |
| `--dt=` | float in (0, 10] | preset's `1/120` | Override the fixed physics timestep |
| `--headless=` | integer | `0` (windowed) | Run N steps, print one stats line, exit |
| `--smoke` | — | off | Open a window for one frame, exit 0 |
| `--help` | — | — | Print usage and exit 0 |

Repeated flags: last occurrence wins. `--headless=0` (the default) means
windowed mode; when `--headless=<n>`, n > 0 takes precedence over `--smoke`
if both are given. Parse errors and usage-on-error go to **stderr** and exit
nonzero (2); `--help` usage goes to stdout and exits 0.

Headless runs print a single checksum line, e.g.
`steps=20000 time=166.66666666667942 bodies=1 merges=149 checksum=1.299524206039838`
(real output of `./build/nbody --headless=20000 --preset=random-cloud`).
With the same `--seed`, that line is byte-identical across runs
(verified: `--headless=100000 --preset=random-cloud --seed=42` twice,
identical output and md5).

## Controls (windowed mode)

| Input | Effect |
| --- | --- |
| `Esc` | Close an open dropdown; otherwise close the window |
| `Space` | Pause / resume (rendering continues; a hung-upon frame never turns into a catch-up burst — the loop is clamped to 0.25 s of frame time and 30 steps per frame) |
| `R` | Restart from the current Preset and Random seed menu options, not the launch options |
| Preset | Click the header to choose one of four presets |
| Integrator | Click the header to choose RK4 or Euler |
| Physics | Click the header to choose a time step or restart the scene |
| Random seed | Click the header, then the "type a seed" row, and type the seed directly (see the menu section) |
| Left-drag | Pan the camera |
| Mouse wheel | Zoom anchored at the cursor position |

The live FPS / UPS / body count / sim time are shown in the window title bar
only — the on-screen HUD duplicate was dropped in the visual overhaul (a
loadable system font is still used for the menu labels and the staged chip).

## In-window menu

The layout-A menu is a top bar with Preset, Integrator, Physics, and Random
seed groups. Click a group header to open its dropdown, then click an option;
click outside the dropdown to close it. The fixed-width headers can clip past
the right edge in narrow windows.

Changes apply immediately while the simulation is running. While paused,
changes are staged and shown in a pending chip. Resuming applies the pending
choices together in preset, integrator, time-step, then seed order; the last
choice made in each group wins.

| Group | While running | While paused |
| --- | --- | --- |
| Preset | Switches scenes, resets simulation time, and refits the camera | Staged until resume |
| Integrator | Switches integrator without changing the current simulation state | Staged until resume |
| Physics time step | Applies from the next simulation step | Staged until resume |
| Random seed | Regenerates random-cloud immediately, preserving the camera; for other presets, takes effect on the next random-cloud load | Staged until resume |

Physics offers time steps of `1/30`, `1/60`, `1/120`, `1/240`, and `1/480`,
plus a **Restart scene** action that uses the currently selected preset and
seed. The `R` key uses those same current menu options. Both restarts act
immediately, even while paused, and use the already-applied options — staged
choices stay pending until resume. If `--dt` sets a value
outside the menu list, the Physics header shows that raw value and no dropdown
choice is ticked. Menu navigation is mouse-only; keyboard menu navigation is not
supported.

The Random seed dropdown is a single typed field, not a ± stepper: click its
row to focus an inline editor pre-filled with the current live seed (a staged
seed, if one is pending, is replaced when you commit). Type digits — at most
10, values saturate at 4294967295 (`UINT32_MAX`) — and press Enter to commit
(live when running, staged when paused, like every other group). Escape
cancels the edit only; the dropdown stays open. Clicking anywhere else cancels
the edit and then handles that click normally (dropdown hit, or close and
drag). Mouse-wheel zoom while the editor is open deliberately does **not**
cancel the edit — zoom and typing do not interfere.

## Physics notes

**Gravity.** Pairwise Newtonian attraction with softening, applied along the
unit direction $\hat{d} = \Delta/d$:

$$F_{ij} = \frac{G\, m_i\, m_j}{d^2 + \varepsilon^2},
\qquad
\vec{a}_i \mathrel{+}= \frac{G\, m_j}{(d^2 + \varepsilon^2)\, d}\;\vec{\Delta}$$

This is *not* the Plummer $(d^2+\varepsilon^2)^{3/2}$ form. Per-preset
softening: two-body and figure-eight $\varepsilon = 0$, solar-system
$\varepsilon = 0.01$, random-cloud $\varepsilon = 0.1$; $G = 1$. Newton's
third law is enforced pair-by-pair, so total momentum is conserved exactly.

**Collisions.** Bodies merge inelastically on overlap: the more massive body
survives (ties to the lower id), taking $m = m_1 + m_2$, position at the center of mass,
$\vec{v} = (m_1 \vec{v}_1 + m_2 \vec{v}_2)/(m_1 + m_2)$, and
$r = \sqrt{r_1^2 + r_2^2}$; the loser is erased.

**Solar system.** Our sun (mass 500, rendered with a glow) plus 8 planets,
Mercury through Neptune, on circular orbits with compressed spacing and the
real mass and size ordering. Gas giants get latitude bands (Jupiter 3,
Saturn 2) and Saturn gets a ring. Fully deterministic and RNG-free: the
preset is a literal list and ignores the seed.

**Starfield.** The background is 600 fixed-seed stars locked to the world:
pan and zoom move them, and they never regenerate.

**Euler vs RK4 demo.** Run the same scene under both integrators:

```sh
./build/nbody --preset=two-body --integrator=euler   # orbit spirals outward
./build/nbody --preset=two-body --integrator=rk4     # orbit stays closed
```

Euler injects energy each orbit; RK4 does not. The test suite quantifies it:
over 40k steps at `dt=1/120` of the circular two-body setup, the relative
energy drift |dE/E| is ~1e-10 for RK4 versus ~42% for explicit Euler
(the test asserts RK4's drift is at least 100x smaller).

## Performance

Scaling is $O(N^2)$ per step (all pairs evaluated every step); Barnes–Hut
or any other broadphase is an explicit non-goal. On the development host,
`./build/nbody --headless=500000 --preset=random-cloud` (Debug build)
completed in **0.46 s** wall time — random-cloud merges down to a single
body quickly (149 merges), after which the per-step pair count is tiny.

## Non-goals

Barnes–Hut/broadphase, elastic bouncing, trails, additional integrators,
save/load, config files, gamepad, multi-window.
