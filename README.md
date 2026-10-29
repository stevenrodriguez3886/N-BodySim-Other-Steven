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
ctest --test-dir build        # 44 tests
```

The executable is `build/nbody`.

## Command line

| Flag | Values | Default | Meaning |
| --- | --- | --- | --- |
| `--preset=` | `two-body` \| `solar-system` \| `random-cloud` \| `figure-eight` | `two-body` | Which scenario to load |
| `--integrator=` | `euler` \| `rk4` | `rk4` | Integration scheme |
| `--seed=` | integer | `42` | RNG seed (affects `random-cloud`) |
| `--dt=` | float > 0 | preset's `1/120` | Override the fixed physics timestep |
| `--headless=` | integer | `0` (windowed) | Run N steps, print one stats line, exit |
| `--smoke` | — | off | Open a window for one frame, exit 0 |
| `--help` | — | — | Print usage and exit 0 |

Repeated flags: last occurrence wins.

Headless runs print a single checksum line, e.g.
`steps=20000 time=166.667 bodies=5 merges=0 checksum=13.17`.
With the same `--seed`, that line is byte-identical across runs
(verified: `--headless=100000 --preset=random-cloud --seed=42` twice,
identical output and md5).

## Controls (windowed mode)

| Input | Effect |
| --- | --- |
| `Esc` | Close the window |
| `Space` | Pause / resume (rendering continues; a hung-upon frame never turns into a catch-up burst — the loop is clamped to 0.25 s of frame time and 30 steps per frame) |
| `R` | Reset to the preset's initial bodies (reset assigns fresh monotonic ids, so body palette colors may remap) |
| Left-drag | Pan the camera |
| Mouse wheel | Zoom anchored at the cursor position |

The live FPS / UPS / body count / sim time are shown in the title bar (and
on-screen when a system font is loadable).

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
