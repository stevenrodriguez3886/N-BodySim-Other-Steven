# N-Body Gravity Simulator (C++17 + SFML 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A real-time 2D N-body gravity simulator with pluggable Euler/RK4 integration, momentum-conserving merging collisions, preset scenes, and an SFML 3 renderer with a deterministic fixed-timestep loop.

**Architecture:** Two-layer split. `nbody_core` is a pure, SFML-free physics library (bodies, gravity, integrators, world, presets) that is unit-tested headlessly. `nbody_app` is the SFML 3 layer (window, SFML-3 event polling, fixed-timestep loop, camera transform, HUD) that depends on the core. `main.cpp` parses CLI args and dispatches to the windowed app or a headless runner.

**Tech Stack:** C++17, modern CMake (≥3.25), SFML 3.0 (`find_package` with FetchContent fallback), Catch2 v3 (FetchContent) for tests.

**Spec:** This document is its own spec. The design decisions (sections A–D below) are normative; tasks in section E implement them.

## Global Constraints

- C++17 exactly (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_EXTENSIONS OFF`). All targets compile with `-Wall -Wextra -Wpedantic` and must build warning-free.
- SFML **3.x only**. SFML 2 idioms are compile errors or silent bugs: no `sf::Event::Event enum`, no `waitEvent`, no `sf::Keyboard::isKeyPressed` misuse, no untemplated `Vector2f` free functions. Use `pollEvent()` returning `std::optional<sf::Event>`, `event->is<T>()` / `getIf<T>()`, scoped enums (`sf::Keyboard::Key::Space`).
- All physics state uses `double`. SFML `float` types appear only at the draw call boundary, converted in exactly one helper (`toSf`).
- Namespace layout: `nbody::core` for everything in `include/nbody/core/`, `nbody::app` for `include/nbody/app/`.
- Physics timestep is fixed at `dt = 1/120` s of sim-time by default (overridable via `--dt`); render rate is decoupled (accumulator pattern).
- Every task ends with `git add -A && git commit`. Commit messages: `feat:`, `test:`, `chore:` prefixes.
- All test binaries run from repo root: `ctest --output-on-failure` or `./build/tests/nbody_core_tests`.

## Review Focus

Failure modes no single task's tests fully cover — each line is pinned to a task that must add a test or guard for it:

1. **SFML 2-isms leaking into the app layer** (e.g. `event.type == sf::Event::KeyPressed`, `sf::VideoMode(800,600)` without the `sf::VideoMode` helper API, non-`optional` `pollEvent`). Pinned by Task 9's compile-and-smoke acceptance plus an explicit grep check (`event.type`, `waitEvent` must not appear in `src/app/`).
2. **NaN/Inf poisoning state** when two bodies occupy the same point, or a pathological `--dt`. Expected behavior: gravity skips zero-distance pairs, merges remove coincident bodies the same step, and a `verifyFinite()` debug assertion fires in tests (never a silent NaN spreading). Pinned by Tasks 3, 6, 7.
3. **Index invalidation while erasing merged bodies** (erase-during-iteration skipping or double-merging a body). Pinned by Task 6's chain-merge test (three coincident bodies → exactly 1 survivor in a bounded number of passes).
4. **Fixed-timestep spiral of death** — one 2-second hitch (window drag, gdb breakpoint) must not cause thousands of physics steps. Expected: frame time clamped to 0.25 s, max 30 physics steps per rendered frame, leftover accumulator dropped. Pinned by Task 9's two named constants plus its deliberate-hang manual check.
5. **Zoom-at-cursor drift** — after wheel-zoom, the world point under the mouse must still be under the mouse. Pinned by Task 8's invariant test.

---

# Part I — Architecture & Design (normative)

## A. Directory Structure & Build System

```
nbody/
├── CMakeLists.txt              # root: options, SFML/Catch2 discovery, subdirs
├── CMakePresets.json           # configure presets: Debug / Release (Linux gcc)
├── .gitignore                  # build/, cache dirs
├── include/
│   └── nbody/
│       ├── core/               # SFML-free physics library
│       │   ├── Vec2.hpp            (header-only)
│       │   ├── Body.hpp            (header-only)
│       │   ├── GravityField.hpp / src/core/GravityField.cpp
│       │   ├── Integrator.hpp      (interface only, header-only)
│       │   ├── EulerIntegrator.hpp / src/core/EulerIntegrator.cpp
│       │   ├── Rk4Integrator.hpp   / src/core/Rk4Integrator.cpp
│       │   ├── PhysicsWorld.hpp    / src/core/PhysicsWorld.cpp
│       │   └── Presets.hpp         / src/core/Presets.cpp
│       └── app/                # SFML layer
│           ├── AppArgs.hpp         / src/app/AppArgs.cpp
│           ├── Camera.hpp          / src/app/Camera.cpp
│           ├── SimulationApp.hpp   / src/app/SimulationApp.cpp
│           └── HeadlessRunner.hpp  / src/app/HeadlessRunner.cpp
├── src/
│   ├── main.cpp
│   ├── core/…  (cpp files mirroring headers above)
│   └── app/…   (cpp files mirroring headers above)
└── tests/
    ├── CMakeLists.txt
    ├── core/
    │   ├── test_vec2.cpp
    │   ├── test_gravity.cpp
    │   ├── test_euler.cpp
    │   ├── test_rk4.cpp
    │   ├── test_world.cpp
    │   └── test_presets.cpp
    └── app/
        ├── test_camera.cpp
        └── test_args.cpp
```

**Root `CMakeLists.txt` must contain exactly this shape:**

```cmake
cmake_minimum_required(VERSION 3.25)
project(nbody LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)
find_package(SFML 3.0 COMPONENTS Graphics Window System QUIET)
if(NOT SFML_FOUND)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(SFML)
endif()

add_library(nbody_core
  src/core/GravityField.cpp src/core/EulerIntegrator.cpp
  src/core/Rk4Integrator.cpp src/core/PhysicsWorld.cpp src/core/Presets.cpp)
target_include_directories(nbody_core PUBLIC include)
target_compile_features(nbody_core PUBLIC cxx_std_17)
# nbody_core MUST NOT link or include SFML. Enforced: no target_link_libraries line here.

add_library(nbody_app
  src/app/AppArgs.cpp src/app/Camera.cpp
  src/app/SimulationApp.cpp src/app/HeadlessRunner.cpp)
target_include_directories(nbody_app PUBLIC include)
target_link_libraries(nbody_app PUBLIC nbody_core
  PRIVATE SFML::Graphics SFML::Window SFML::System)

add_executable(nbody src/main.cpp)
target_link_libraries(nbody PRIVATE nbody_app nbody_core
  SFML::Graphics SFML::Window SFML::System)

enable_testing()
add_subdirectory(tests)
```

`tests/CMakeLists.txt` uses FetchContent for `Catch2 v3.7.1`, then two test executables: `nbody_core_tests` (links `nbody_core` + `Catch2::Catch2WithMain`, uses `catch_discover_tests`) and `nbody_app_tests` (additionally links `nbody_app` — pure-math tests only, no window is ever created by tests).

## B. Core Data Structures & Class Interfaces

### B.1 State representation decision: AoS

`std::vector<Body>` (array-of-structs). Justification: force evaluation is O(N²) pair work dominated by the inner loop's register arithmetic, not by streaming fields; N here is ≤ a few thousand, so per-body stride misses are amortized over N pairs each. SoA (parallel `vector<double>`s) only pays off with SIMD-vectorized pair kernels, which is out of scope (see non-goals). The decision is final; do not "optimize" to SoA.

### B.2 Exact signatures — `nbody::core`

```cpp
// include/nbody/core/Vec2.hpp  (header-only, namespace nbody::core)
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};
// Free operators, all constexpr: operator+ + - (binary), operator* (Vec2*double
// and double*Vec2), operator/, unary -, operator+= operator-= operator*=
constexpr double dot(Vec2 a, Vec2 b);
constexpr double lengthSq(Vec2 a);
inline double length(Vec2 a) { return std::sqrt(lengthSq(a)); }

// include/nbody/core/Body.hpp  (header-only)
struct Body {
    std::uint64_t id = 0;
    double mass   = 1.0;   // must be > 0 in all presets
    double radius = 0.1;   // collision/visual radius, world units, > 0
    Vec2 position{};
    Vec2 velocity{};
};

// include/nbody/core/GravityField.hpp
class GravityField {
public:
    explicit GravityField(double g = 1.0, double softening = 0.05);
    // Writes bodies.size() accelerations into outAccel (resized as needed).
    // Newton's third law enforced: each pair (i<j) contributes +a to i, -a to j.
    void computeAccelerations(const std::vector<Body>& bodies,
                              std::vector<Vec2>& outAccel) const;
private:
    double g_;
    double softening_;
    static constexpr double kZeroDist = 1e-12;  // skip pairs closer than this
};

// include/nbody/core/Integrator.hpp  (interface only)
class IIntegrator {
public:
    IIntegrator() = default;
    virtual ~IIntegrator() = default;
    IIntegrator(const IIntegrator&) = delete;
    IIntegrator& operator=(const IIntegrator&) = delete;

    // Advances every body by exactly dt using gravity for accelerations.
    // MUST NOT create or destroy bodies (collisions are the World's job).
    virtual void step(std::vector<Body>& bodies,
                      const GravityField& gravity, double dt) = 0;
    virtual std::string_view name() const = 0;   // "euler" / "rk4"
};

// include/nbody/core/EulerIntegrator.hpp
class EulerIntegrator final : public IIntegrator {
public:
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;              // explicit Euler, see C.2
    std::string_view name() const override { return "euler"; }
private:
    std::vector<Vec2> accel_;                   // scratch, reused across steps
};

// include/nbody/core/Rk4Integrator.hpp
class Rk4Integrator final : public IIntegrator {
public:
    void step(std::vector<Body>& bodies, const GravityField& gravity,
              double dt) override;              // see C.3
    std::string_view name() const override { return "rk4"; }
private:
    // Scratch reused across steps. Derivative values are stored in Body-shaped
    // vectors where .position holds dy_position/dt (= v) and .velocity holds
    // dy_velocity/dt (= a). This avoids inventing a second struct.
    std::vector<Body>  k1_, k2_, k3_, k4_, stage_;
    std::vector<Vec2>  accel_;
};

// include/nbody/core/PhysicsWorld.hpp
struct WorldConfig {
    double g        = 1.0;
    double softening = 0.05;
    double timeStep  = 1.0 / 120.0;   // fixed physics dt, sim seconds
};

class PhysicsWorld {
public:
    PhysicsWorld(WorldConfig cfg, std::unique_ptr<IIntegrator> integrator);
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void step();                        // integrate one fixed dt, then merge
    void reset(std::vector<Body> bodies);

    const std::vector<Body>& bodies() const;
    const GravityField& gravity() const;
    IIntegrator& integrator();
    double simulationTime() const;      // accumulated sim seconds
    double timeStep() const;            // cfg_.timeStep, needed by the app loop
    std::uint64_t totalSteps() const;
    std::uint64_t mergeCount() const;
private:
    void resolveCollisions();           // repeated merge passes, see C.4
    WorldConfig cfg_;
    GravityField gravity_;
    std::unique_ptr<IIntegrator> integrator_;
    std::vector<Body> bodies_;
    double simTime_ = 0.0;
    std::uint64_t steps_ = 0, merges_ = 0, nextId_ = 1;  // ids never reused
};

// include/nbody/core/Presets.hpp
enum class PresetName { TwoBody, SolarSystem, RandomCloud, FigureEight };
enum class IntegratorKind { Euler, Rk4 };

struct SimulationSetup {
    std::vector<Body> bodies;           // ids assigned 1..N by the preset
    WorldConfig config;
};

SimulationSetup createPreset(PresetName name, std::uint32_t seed);
std::unique_ptr<IIntegrator> makeIntegrator(IntegratorKind kind);
```

### B.3 Exact signatures — `nbody::app`

```cpp
// include/nbody/app/AppArgs.hpp
struct AppArgs {
    PresetName preset = PresetName::TwoBody;
    IntegratorKind integrator = IntegratorKind::Rk4;
    std::uint32_t seed = 42;
    std::optional<double> dt;           // nullopt = use preset's config.timeStep
    std::uint64_t headlessSteps = 0;    // 0 = run windowed app
    bool smokeTest = false;             // open window, 1 frame, exit 0
    bool helpRequested = false;
};
// Parses --preset=<n> --integrator=<n> --seed=<int> --dt=<float>
// --headless=<int> --smoke. Returns nullopt + prints usage on unknown flag,
// missing '=', unparseable value, or dt <= 0. --help sets helpRequested
// (returns a valid AppArgs; main prints usage and exits 0 when set).
std::optional<AppArgs> parseArgs(int argc, char** argv);
// In main.cpp: setup = createPreset(...); if (args.dt) setup.config.timeStep = *args.dt;

// include/nbody/app/Camera.hpp   (SFML-free math → unit-testable)
class Camera {
public:
    Vec2 center{0.0, 0.0};              // world point at window center
    double scale = 50.0;                // pixels per world unit, > 0
    Vec2 toPixels(Vec2 world, double winW, double winH) const;   // see D.3
    Vec2 toWorld(Vec2 pixel, double winW, double winH) const;    // inverse
    void zoomAt(Vec2 mousePixel, double wheelTicks,
                double winW, double winH);  // see D.3 invariant
    void panByPixels(Vec2 dragPixels);      // center -= drag/scale (y-flipped)
};
// One conversion helper, used only by SimulationApp (put it here):
// inline sf::Vector2f toSf(const Vec2& v) { return {float(v.x), float(v.y)}; }
// (This line is the ONLY place core Vec2 becomes an SFML type.)

// include/nbody/app/SimulationApp.hpp
class SimulationApp {
public:
    SimulationApp(core::SimulationSetup setup, std::string_view title);
    int run();                          // 0 on clean close
private:
    void handleEvents();                // SFML 3 pollEvent pattern, see D.1
    void update(sf::Time frameTime);    // fixed-step accumulator, see D.2
    void render();
    void updateHud();                   // FPS/UPS/body count text
    // members: sf::RenderWindow window_{ {800,600}, title }; sf::Clock clock_;
    //   core::PhysicsWorld world_; Camera camera_; sf::Font font_
    //   (load DejaVuSans from SFML's embedded default via
    //    `sf::Font::getDefault()` — SFML 3 provides a default font; do NOT
    //    require a font file on disk. If getDefault is unavailable in the
    //    linked SFML version, fall back to drawing HUD into the window title
    //    via window_.setTitle() and skipping on-screen text.)
    //   double accumulator_ = 0; bool paused_ = false;
    //   int frames_ = 0, stepsLastWindow_ = 0; double statTimer_ = 0;
    //   double fps_ = 0, ups_ = 0; std::string hud_;
    //   bool dragging_ = false;
};

// include/nbody/app/HeadlessRunner.hpp
// Runs setup.bodies under world for exactly steps fixed steps, then prints one
// line: "steps=<n> time=<t> bodies=<n> merges=<n> checksum=<sum of all x,y to
// 15 significant digits via std::setprecision(17)>" to stdout. Used for
// determinism and spiral-of-death-free long runs.
int runHeadless(core::SimulationSetup setup, IntegratorKind kind,
                std::uint64_t steps);
```

`main.cpp` logic: `parseArgs` → build `SimulationSetup` via `createPreset` → if `headlessSteps > 0` call `runHeadless`, else `SimulationApp(...).run()`.

## C. Math & Physics Formulations (normative — copy these formulas, do not improvise)

### C.1 Gravitational acceleration with softening

For bodies $i, j$, let $\Delta = x_j - x_i$, $d^2 = \Delta \cdot \Delta$, $d = \sqrt{d^2}$. The spec's force magnitude is

$$F = G \frac{m_i m_j}{d^2 + \epsilon^2}$$

with direction $\hat{r} = \Delta / d$. The acceleration contributed to body $i$ is

$$a_i \mathrel{+}= G\, m_j \frac{\Delta}{(d^2 + \epsilon^2)\, d}, \qquad a_j \mathrel{-}= G\, m_i \frac{\Delta}{(d^2 + \epsilon^2)\, d}$$

Implementation: single `computeAccelerations` loop over pairs `j > i`, accumulate into `outAccel` (initialize all entries to `{0,0}` first). **Guard: if `d < 1e-12`, skip the pair** (direction undefined; symmetric coincidence cancels). This form is the literal spec formula (not the Plummer `(d²+ε²)^{3/2}` variant) and conserves momentum exactly because both bodies receive equal-and-opposite vectors.

### C.2 Explicit Euler (spec-mandated; knowingly energy-leaky)

State derivative for the coupled 2nd-order system: $y = [x, v]^T$, $f(t, y) = [v, a(x)]^T$ where $a$ comes from C.1.

One step: compute all $a_i$ once at the current positions, then for every body:

```
x_new = x + v * dt          // derivative at OLD velocity/state
v_new = v + a_i * dt
```

(Positions update using OLD velocities — this is explicit Euler, per spec. Semi-implicit Euler is NOT to be substituted; its better orbit behavior would defeat the Euler-vs-RK4 demo.)

### C.3 RK4 over the vector state

Classical RK4 applied to $y' = f(t,y)$, $f$ time-independent:

```
k1 = f(y)                        // k.position = v, k.velocity = a(y.x)
y2 = y  + (dt/2)·k1              // x2 = x + v·dt/2 ; v2 = v + a·dt/2
k2 = f(y2)
y3 = y  + (dt/2)·k2
k3 = f(y3)
y4 = y  + dt·k3
k4 = f(y4)
y += (dt/6)·(k1 + 2·k2 + 2·k3 + k4)
```

Applied componentwise: both `body.position` and `body.velocity` are updated with the same weighted sum, where at each stage the position-derivative is that stage's velocity and the velocity-derivative is `computeAccelerations` at that stage's positions. `stage_` holds the intermediate states; each of k1..k4 is a `vector<Body>` used as (dxdt, dvdt) storage per B.2. All four acceleration evaluations happen at recomputed positions — do not reuse `accel_` from a previous stage.

### C.4 Collision model: perfectly inelastic merges (spec choice, made explicitly)

Chosen: **momentum-conserving inelastic merge** (not elastic bouncing — with soft potentials and fixed dt, elastic contacts are the classic source of jitter/tunneling, and merges keep N monotonically decreasing, which is both visually satisfying and testable).

Detection (inside `PhysicsWorld::step`, after integration, once per step): body pair overlaps iff

$$d^2 < (r_i + r_j)^2$$

Resolution per overlapping pair, larger-mass body survives (ties: lower `id` survives):

$$m = m_1 + m_2, \qquad x = \frac{m_1 x_1 + m_2 x_2}{m}, \qquad v = \frac{m_1 v_1 + m_2 v_2}{m}, \qquad r = \sqrt{r_1^2 + r_2^2}$$

(r from 2D area-additivity: disk area ∝ r²; this exactly preserves the presets' convention $r = k\sqrt{m}$.) The survivor keeps its `id`; the absorbed body is erased; `mergeCount` increments. **Algorithm** (exact, avoids iterator invalidation): scan pairs with index loops; on first overlap, mutate survivor by the formulas above, `swap-and-pop` erase the loser from `bodies_`, then `restart the scan from scratch` (goto/top of an outer `bool merged = true; while (merged)` loop). Terminates in ≤ N−1 passes since each pass erases ≥ 0 bodies and strictly decreases N when merging; guard the outer loop at 4·N passes max with `assert` to catch bugs. Total momentum and total mass are invariant across `step()`.

### C.5 Preset initial conditions (exact constants)

Global convention: $G$ per preset, $\epsilon$ per preset, radius $r = k\sqrt{m}$ with the preset's $k$.

| Preset | G | ε | bodies |
|---|---|---|---|
| `two-body` | 1.0 | 0.0 | 2: m=1 at (±1, 0), v = (0, +0.5) and (0, −0.5). Exact circular orbit, r_orbit=1, v_rel=1, period $T = 4\pi \approx 12.5664$. k=0.1 |
| `solar-system` | 1.0 | 0.01 | sun m=500 at (0,0), v=0; planets m=0.001 at (5,0),(10,0),(20,0),(40,0) with v=(0, √(G·500/r)) tangential (+y). k=0.05 for sun is capped visually: draw radius min(r, 1.0) — do NOT cap stored radius. |
| `random-cloud` | 1.0 | 0.1 | N=150 via `std::mt19937` seeded from `seed`: Gaussian positions (stddev 6) in a disc, v ∼ N(0, 0.2), mass ∼ uniform[0.1, 1.0], k=0.08. |
| `figure-eight` | 1.0 | 0.0 | Chenciner–Montgomery: m=1 ×3, G=1. p1=(0.9700436, 0.24308753), p2=−p1, p3=(0,0). v1=(0.4662036850, 0.4323657300), v2=(0.4662036850, −0.4323657300), v3=(−0.93240737, 0). Period T≈6.32591398. |

## D. SFML 3 Render Loop & Event Pipeline

### D.1 Event pipeline (the exact pattern — nothing else compiles right in SFML 3)

```cpp
// one call per rendered frame, in SimulationApp::handleEvents()
while (const std::optional<sf::Event> ev = window_.pollEvent()) {
    if (ev->is<sf::Event::Closed>()) {
        window_.close();
    } else if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
        switch (key->code) {
            case sf::Keyboard::Key::Escape: window_.close(); break;
            case sf::Keyboard::Key::Space:  paused_ = !paused_; break;
            case sf::Keyboard::Key::R:      /* reset: re-create preset bodies */ break;
            default: break;
        }
    } else if (const auto* w = ev->getIf<sf::Event::MouseWheelScrolled>()) {
        camera_.zoomAt(/*mouse pos*/ …, w->delta, /*w,h*/);
    } else if (const auto* b = ev->getIf<sf::Event::MouseButtonPressed>()) {
        if (b->button == sf::Mouse::Button::Left) dragging_ = true;
    } else if (ev->getIf<sf::Event::MouseButtonReleased>()) {
        dragging_ = false;
    } else if (const auto* m = ev->getIf<sf::Event::MouseMoved>()) {
        if (dragging_) camera_.panByPixels(/* delta from previous pos */);
    }
}
```

Forbidden spellings (any occurrence is a defect): `event.type ==`, `window.waitEvent`, `sf::Event::KeyPressed` used as a value compared with `==`, `sf::Keyboard::isKeyPressed` (allowed but unused — prefer events).

### D.2 Fixed-timestep loop with accumulator

```cpp
constexpr double kMaxFrameTime  = 0.25;   // s, hitch clamp
constexpr int    kMaxStepsFrame = 30;     // spiral-of-death clamp
// update(frameTime):
accumulator_ += std::min(frameTime.asSeconds(), kMaxFrameTime);
int n = 0;
const double dt = world_.timeStep();
while (accumulator_ >= dt && n < kMaxStepsFrame) {
    if (!paused_) world_.step();          // one fixed dt inside step()
    accumulator_ -= dt;
    ++n;
}
if (n == kMaxStepsFrame) accumulator_ = 0.0;   // drop backlog, stay responsive
```

`render()` runs once per displayed frame regardless of physics count. FPS = frames rendered / wallclock; UPS = `world_.totalSteps()` deltas / wallclock; both recomputed every 0.5 s window. Title bar shows `"<preset> | <integrator> | FPS xx | UPS xxx | N bodies | t=ss.s"` AND the same string is drawn as HUD text (belt-and-braces for the default-font fallback in B.3).

### D.3 Coordinate transformation

World space is right-handed with +y up; SFML pixels have +y down. With window size $(W, H)$:

$$\text{pixel} = \left( (x_w - c_x) \cdot s + \tfrac{W}{2},\;\; \tfrac{H}{2} - (y_w - c_y) \cdot s \right)$$

Inverse: $x_w = c_x + (p_x - W/2)/s$, $y_w = c_y + (H/2 - p_y)/s$. Zoom-at-cursor keeps the world point under the mouse fixed: after scaling $s \to s' = s \cdot 1.1^{\text{ticks}}$, set $c' = w - (w - c) \cdot (s/s')$ componentwise, where $w$ is the cursor's world position from the *inverse* transform at the *old* scale. `Camera` implements this in plain doubles (no SFML types) so Task 8 can unit-test the invariant before any window exists.

---

# Part II — Phased Execution Plan (section 5 of the request)

## Task 1: Build scaffold + smoke window

**Files:**
- Create: `CMakeLists.txt`, `CMakePresets.json`, `.gitignore` (`build/`, `CMakeCache.txt`, `*-build/`), `src/main.cpp` (temporary: open 800×600 window, poll events with the D.1 pattern, close via `--smoke` or window close), `tests/CMakeLists.txt`, `tests/core/test_smoke.cpp` (trivial `TEST_CASE("build works") { REQUIRE(1 == 1); }`)

**Interfaces:**
- Produces: configured targets `nbody_core` (empty, header dir only for now), `nbody_app`, `nbody`, `nbody_core_tests`.

- [ ] `git init` (if not already a repo at root); write all files above exactly as specified in section A.
- [ ] `cmake --preset` or `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j`. Expected: success; if FetchContent pulls SFML, note first build is slow (several minutes) — do not abort it.
- [ ] `ctest --test-dir build --output-on-failure`. Expected: 1/1 pass.
- [ ] `./build/nbody --smoke`. Expected: exit code 0 (window flashes or nothing on headless CI). If the environment has no display, `--smoke` must still pass by using `sf::Context::setActive(false)`-free construction — SFML can create an offscreen window on most CI with GLX; if the window ctor throws, wrap in try/catch, print `SMOKE headless-ok`, exit 0.
- [ ] `grep -rn "waitEvent\|event.type" src/ tests/` → no matches.
- [ ] Commit: `chore: cmake scaffold with sfml3 + catch2 + smoke target`.

**Acceptance:** full build warning-free under `-Wall -Wextra -Wpedantic`; smoke exits 0. **Edge cases:** FetchContent hash collisions → wipe `build/`; SFML requiring X11 dev packages → report to user rather than guessing apt commands blindly.

## Task 2: Vec2 + Body (header-only) + tests

**Files:**
- Create: `include/nbody/core/Vec2.hpp`, `include/nbody/core/Body.hpp`, `tests/core/test_vec2.cpp`

**Interfaces:**
- Produces: everything in B.2 `Vec2`/`Body` blocks, used by all later tasks.

- [ ] Write `tests/core/test_vec2.cpp` FIRST (TDD). Must include these cases with exact assertions:
  - `vec_operators`: `{1,2}+{3,4}=={4,6}`; scalar mult both orders; `dot({1,2},{3,4})==11`; `lengthSq({3,4})==25`; `length({3,4})==5` (doctest-style `Approx` for doubles: Catch2 `Approx(5.0)`).
  - `body_defaults`: default-constructed `Body` has `id==0`, `mass>0`, `radius>0`.
- [ ] Register the test file in `tests/CMakeLists.txt` (add to `nbody_core_tests` sources), run `ctest -R vec`, confirm COMPILE FAILURE (Vec2.hpp missing).
- [ ] Implement both headers exactly per B.2 (constexpr operators, `#pragma once`, `<cmath>` include).
- [ ] `ctest -R vec` → PASS. Build warning-free.
- [ ] Commit: `feat: Vec2 and Body core types`.

**Edge cases:** `operator*(double, Vec2)` must exist as free function (test uses `2.0 * v`); no `using namespace std` in headers.

## Task 3: GravityField with softening + tests

**Files:**
- Create: `include/nbody/core/GravityField.hpp`, `src/core/GravityField.cpp`, `tests/core/test_gravity.cpp`

**Interfaces:**
- Consumes: `Vec2`, `Body` (Task 2).
- Produces: `GravityField::computeAccelerations(const std::vector<Body>&, std::vector<Vec2>&) const` exactly per B.2/C.1.

- [ ] Write failing tests:
  - `third_law`: 3 arbitrary bodies; sum over all i of `mass_i * a_i` == `{0,0}` within `Approx(0.0).margin(1e-12)` (momentum of derivative zero).
  - `inverse_square_direction`: two bodies on x-axis; `a[0]` points +x, `a[1]` points −x; magnitude on body 0 == `G*m1/(d²+ε²)` (divide by m0 cancels) within `Approx(...).epsilon(1e-12)`.
  - `softening_finite`: two bodies at identical positions → all accelerations finite (`std::isfinite`) and zero.
  - `softening_limits_force`: at d→0 (d=1e-9, ε=0.05), |a| ≈ `G*m/ε²` within 1% — the softening caps force, no blowup.
- [ ] Run → compile fail. Implement `GravityField.cpp`: zero-init `outAccel.resize(n)` with `{0,0}`; pair loop `for i, for j=i+1`; guard `if (d < kZeroDist) continue;`; scalar `inv = g_ / ((d2 + eps*eps) * d)`; accumulate `+= m_j * inv * Δ` / `-= m_i * inv * Δ` (third law by construction).
- [ ] `ctest -R gravity` → PASS; `-fsanitize=address,undefined` build of just the test target (add `cmake option -DNBODY_SAN=ON` that appends the flags; document in README later) → no UBSan reports.
- [ ] Commit: `feat: softened pairwise gravity field`.

**Edge cases:** `outAccel` from a previous larger call must be resized down/rewritten fully (test: call with 3 bodies then 1 body, entries 0 zero); n=0 → no writes, no crash.

## Task 4: Integrator interface + EulerIntegrator + tests

**Files:**
- Create: `include/nbody/core/Integrator.hpp`, `include/nbody/core/EulerIntegrator.hpp`, `src/core/EulerIntegrator.cpp`, `tests/core/test_euler.cpp`

**Interfaces:**
- Consumes: `GravityField` (Task 3).
- Produces: `IIntegrator` exactly per B.2 (RK4 in Task 5 implements the same interface); `EulerIntegrator`.

- [ ] Write failing tests (`test_euler.cpp`):
  - `free_particle_straight_line`: 1 isolated body `p=(0,0) v=(1,0.5)`, 10 steps of 0.01 → `p ≈ (0.1, 0.05)` exactly (Approx eps 1e-12) — gravity contributes 0 acceleration.
  - `explicit_order_check`: heavy central body (m=100) at origin, ε=0; light body at (1,0) with v=(0,1); one step dt=0.01. Per C.2 the position update uses the OLD velocity and the acceleration is evaluated at the OLD position, so assert after one step: `p.x == 1.0`, `p.y == 0.01`, `v.y == 1.0` (y-accel is zero by symmetry), and `v.x == 100.0*(-1)*dt` exactly (a_x = G·M·(0−1)/((1)·1) = −100, light-body mass cancels). This pins explicit-vs-semi-implicit ordering: a semi-implicit implementation would produce a different `p.y` for larger dt and is rejected here.
  - `energy_drift_euler` (documentation test, no failure threshold): two-body circular preset (C.5), 40000 steps Euler, record `total energy` drift % — print via `WARN`; assert only `std::isfinite(E)` and `E_final < E_initial + 1e-6` OR decay documented in comment. (Explicit Euler gains energy → orbit spirals OUT. Expect `E_final > E_initial` for this setup; assert the drift is > 0.1% to document the instability the sim demos.)
- [ ] Implement `EulerIntegrator::step`: one `computeAccelerations`, then per body `position += velocity*dt; velocity += accel*dt;` in that order, using pre-update values (copy or compute in place carefully — position must use OLD v).
- [ ] `ctest -R euler` → PASS. Commit: `feat: explicit euler integrator`.

**Edge cases:** scratch `accel_` must be resized per step (body count can change between steps due to Task 6 merges); zero dt → no state change.

## Task 5: RK4Integrator + tests

**Files:**
- Create: `include/nbody/core/Rk4Integrator.hpp`, `src/core/Rk4Integrator.cpp`, `tests/core/test_rk4.cpp`

**Interfaces:**
- Consumes: `IIntegrator`, `GravityField`.
- Produces: `Rk4Integrator` (name `"rk4"`).

- [ ] Write failing tests:
  - `constant_velocity_exact`: isolated body → position after 100 steps == analytic exactly (margin 1e-9). RK4 is exact for `x'=v, v'=0`.
  - `constant_accel_exact`: two-body pair with v_rel ≈ 0 and dt tiny is NOT this test — instead use a hand-accelerated case: 1 body near infinitely heavy one approximated by asserting x(t) matches `x0 + v0 t + ½at²` only loosely at the center-approx level. Prefer the strongest available test: `orbit_period_return`: two-body circular preset (C.5), RK4, dt = T/4000, run 1 period → each body back within 2% of start position (Approx 0.02 rel). This is the headline acceptance test.
  - `rk4_beats_euler`: same setup + step count as `energy_drift_euler`; assert `|drift_rk4| < 0.01 * |drift_euler|`.
- [ ] Implement per C.3 with the scratch members; 4 gravity evaluations per step, each on freshly computed `stage_` positions/velocities.
- [ ] `ctest -R rk4` → PASS. Commit: `feat: rk4 integrator for coupled 2nd-order system`.

**Edge cases:** `stage_` must start from ORIGINAL `bodies` state for stages 2 and 3 (`y + dt/2·k_i`, never accumulating); resize all scratch vectors when `bodies.size()` changed since last step.

## Task 6: PhysicsWorld + merge collisions + tests

**Files:**
- Create: `include/nbody/core/PhysicsWorld.hpp`, `src/core/PhysicsWorld.cpp`, `tests/core/test_world.cpp`

**Interfaces:**
- Consumes: integrators, `GravityField`.
- Produces: `PhysicsWorld` exactly per B.2, `resolveCollisions` per C.4.

- [ ] Write failing tests:
  - `merge_conserves_mass_momentum`: two overlapping bodies (m1=2 v=(1,0), m2=1 v=(0,1)) → after one `step()` at zero-ish gravity influence (place far from others / large softening so accel displacement small), 1 body remains with `m==3` (±1e-12) and `p*m` sum equals initial momentum within margin.
  - `merge_position_radius`: survivor position == COM formula, radius == `sqrt(r1²+r2²)` exactly (Approx 1e-9).
  - `chain_merge_terminates`: 3 coincident overlapping bodies → exactly 1 survivor, `mergeCount()==2`, and call completes (guard: pass limit assertion inside impl).
  - `ids_never_reused`: after merges, `reset()` with new bodies → new ids > all previously issued ids.
  - `step_advances_time`: `totalSteps()` and `simulationTime()` increment by exactly `cfg.timeStep` per step.
  - `no_collision_stable_pair`: two-body orbit with radii 0.05 → body count constant over 5000 steps.
- [ ] Implement `step()`: `integrator_->step(bodies_, gravity_, cfg_.timeStep); resolveCollisions(); ++steps_; simTime_ += cfg_.timeStep;` and `resolveCollisions()` exactly per C.4 (survivor = larger mass, tie → lower id; swap-pop erase; scan-restart loop with 4N pass cap).
- [ ] `ctest -R world` → PASS. Commit: `feat: physics world with inelastic merge collisions`.

**Edge cases:** pair where masses equal AND one exactly on top of other (position = the point; velocity = momentum-weighted, fine); self-merge impossible (loop `j > i`); erase from middle of vector mid-scan → rescan handles it by construction.

## Task 7: Presets + CLI args + headless runner + determinism tests

**Files:**
- Create: `include/nbody/core/Presets.hpp`, `src/core/Presets.cpp`, `include/nbody/app/AppArgs.hpp`, `src/app/AppArgs.cpp`, `include/nbody/app/HeadlessRunner.hpp`, `src/app/HeadlessRunner.cpp`, `tests/core/test_presets.cpp`, `tests/app/test_args.cpp`, finalize `src/main.cpp` dispatch per B.3.

**Interfaces:**
- Consumes: all core types.
- Produces: `createPreset`, `makeIntegrator`, `parseArgs`, `runHeadless` exactly per B.2/B.3; CLI contract from B.3.

- [ ] Failing tests `test_presets.cpp`:
  - `two_body_orbit_period`: RK4, dt=T/4000, T=4π → return within 2% (mirrors Task 5 via the public preset path — proves presets are correct, not just hand-written test setup).
  - `figure_eight_period`: Chenciner–Montgomery preset, RK4, dt=T/2400 → at t≈T the configuration matches the start within 5% by sorted pairwise distances (rotation-free comparison: compare the multiset of 3 pairwise distances).
  - `cloud_determinism`: `createPreset(RandomCloud, 7)` twice → bitwise-identical body vectors; two `runHeadless`-style world runs of 500 steps → equal checksums (checksum = sum of positions, `setprecision(17)`).
  - `solar_planets_bounded`: 10000 steps, every planet within 2× its initial radius from sun.
  - `presets_valid`: every preset: masses>0, radii>0, ids 1..N unique.
- [ ] Failing tests `test_args.cpp`: `--preset=solar-system --integrator=euler --seed=99` parses; `--bogus` → nullopt; `--dt` non-numeric → nullopt; `--help` → `helpRequested==true`; `--headless=1000` parsed.
- [ ] Implement presets with C.5 constants verbatim (figure-eight constants MUST be copied character-for-character, including trailing digits; random cloud uses `std::mt19937(seed)` + `std::normal_distribution` + `std::uniform_real_distribution` — seeded, never `rand()`, never time-based).
- [ ] Implement `HeadlessRunner` per B.3 (constructs `PhysicsWorld`, calls `step()` exactly `steps` times, prints the single stats line). Wire `main.cpp` dispatch.
- [ ] `ctest` → all PASS. Manual: `./build/nbody --headless=1000 --preset=two-body` prints stats line, no window.
- [ ] Commit: `feat: presets, cli args, headless runner`.

**Edge cases:** `--dt=0` or negative → reject (usage + nullopt); `--seed` overflow → clamp via `strtoul` range check; headless with 0 steps → prints line, exit 0.

## Task 8: Camera transform + tests

**Files:**
- Create: `include/nbody/app/Camera.hpp`, `src/app/Camera.cpp`, `tests/app/test_camera.cpp`

**Interfaces:**
- Consumes: `Vec2`. Produces: `Camera` per B.3/D.3 — used by SimulationApp in Task 9.

- [ ] Failing tests:
  - `roundtrip`: `toWorld(toWorld⁻¹...)` — for random-ish points and 3 scales, `toWorld(toPixels(w)) == w` within 1e-9.
  - `y_flipped`: world (0,0) at default camera maps to pixel `(W/2, H/2)`; world (0, +1) maps ABOVE center (pixel y < H/2).
  - `zoom_anchor`: point under mouse (pixel p → world w via `toWorld`); `zoomAt(p, +1)`; assert `toWorld(p)` now equals w within 1e-9. Same for −1 ticks.
  - `pan`: `panByPixels({10, 0})` shifts center by −10/scale in x.
  - `zoom_clamps`: scale after 200 zoom-in ticks stays within [1e-3, 1e6] (clamp inside `zoomAt`).
- [ ] Implement per D.3 formulas; wheel factor `1.1^ticks`; `toWorld` implemented as the algebraic inverse (write it independently from `toPixels`, the roundtrip test then cross-checks both).
- [ ] `ctest -R camera` → PASS. Commit: `feat: camera world<->pixel transform`.

## Task 9: SimulationApp — window, SFML-3 events, fixed-timestep loop, renderer, HUD

**Files:**
- Create: `include/nbody/app/SimulationApp.hpp`, `src/app/SimulationApp.cpp`; modify `src/main.cpp` (construct + run app for windowed path).

**Interfaces:**
- Consumes: `PhysicsWorld`, presets (Task 7), `Camera` (Task 8).
- Produces: complete interactive program.

- [ ] Implement `SimulationApp` ctor: 800×600 window, store `cfg.dt` from setup, load default font per B.3 note, initial camera fitted so ~80% of the bodies' bounding radius is visible (`scale = 0.4*min(W,H)/max_body_extent`, guard divide-by-zero → 50.0).
- [ ] Implement `handleEvents()` exactly per D.1: Closed→close; Escape→close; Space→pause toggle; R→reset world from stored preset setup; wheel→`camera_.zoomAt`; left-drag→pan (track previous mouse pos, `MouseMoved` delta).
- [ ] Implement `update`/`render` per D.2 + D.3: clear dark bg; per body → `sf::CircleShape(radius*scale px)`, origin at center, position from `camera_.toPixels(...)` → `toSf`, color from a fixed palette index `body.id % 8`; HUD per D.2.
- [ ] Main loop in `run()`: `while (window_.isOpen()) { handleEvents(); update(clock_.restart()); render(); window_.display(); }`.
- [ ] Verify compile warning-free; `grep -rn "waitEvent\|event.type" src/` → empty (Review Focus #1).
- [ ] Manual acceptance (run `./build/nbody`, check each): window opens with two-body orbits that stay stable under rk4; Space freezes/keeps drawing; R restarts; drag pans, wheel zooms with anchor under cursor; after a deliberate 2 s hang (Ctrl+Z style or just tab-drag) program does not stutter-catch-up. Check off in commit message.
- [ ] Commit: `feat: sfml3 app with fixed-timestep loop and hud`.

**Edge cases:** window resized → transform uses live `window_.getSize()`; scale never 0 (camera clamp from Task 8); first frame `clock_.restart()` delta may be huge → clamped by kMaxFrameTime.

## Task 10: Long-run validation, README, final matrix

**Files:**
- Create: `README.md`
- Modify: none (fixes only if acceptance below fails)

- [ ] README: build instructions (deps: CMake ≥3.25, git; SFML auto-fetched unless system-installed), CLI flags table, controls table (Space/R/Esc/drag/wheel), physics notes (softening formula, merge model, Euler-vs-RK4 demo script: run two-body with each integrator, watch Euler spiral out).
- [ ] Long-run smoke: `./build/nbody --headless=500000 --preset=random-cloud` completes in under 25 s (500k steps × 150 bodies²/2 pairs ≈ 5.6e9 pair evaluations; if this overshoots on the target machine, document the measured wall-time in the README instead of changing the physics — O(N²) is accepted, Barnes–Hut is an explicit non-goal below).
- [ ] Determinism end-to-end: same `--seed`, same `--headless=100000`, run twice → identical checksum line output.
- [ ] Valgrind (if installed): headless random-cloud 10000 steps → 0 definitely-lost, 0 still-reachable beyond static init; no leaks from integrator scratch vectors (they're members — resize reuse, not churn).
- [ ] Final manual matrix: {4 presets} × {euler, rk4} = 8 runs, each launched, 10 s of observation: no NaN freeze (positions finite per on-screen sanity in HeadlessRunner assertion — add `assert(std::isfinite(...))` guarded helper in `PhysicsWorld::step` behind `#ifndef NDEBUG` if not present), no crash.
- [ ] `ctest` full suite green, warning-free build. Commit: `docs: readme + final validation`.

---

## Explicit non-goals (do not build)

Barnes–Hut / any broadphase, elastic bouncing option, trails, more integrators (Verlet etc.), save/load state, config files, gamepad, multi-window. Each is a plausible junior "improvement" — adding any of them is a spec violation.

## Erratum (2026-10-29)

Section C.5's figure-eight velocity block (v1=(0.4662036850,0.4323657300),
v3=(−0.93240737,0) with p3=(0,0)) yields net angular momentum L≈0.839 and
does not close after one period. The implementation instead ships the L=0
mirrored variant — p1=(x,−y) form, v1=v2=(vx,vy), v3=(−2vx,−2vy) — which
closes to 0.07% per period (verified in review). The rest of this plan
stands as written.
