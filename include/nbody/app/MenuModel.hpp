#pragma once

#include "nbody/app/MenuBar.hpp"
#include "nbody/core/Presets.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace nbody::app {

// Staged mirrors of every MenuState option (spec D.3): while paused each
// setter records its value here (last-write-wins per field); Space-resume
// consumes them in the contract order preset(uses options) -> integrator ->
// dt -> seed -> bodies, option rebuilds folding into the preset step.
struct MenuStaged {
    std::optional<core::PresetName> preset;
    std::optional<core::IntegratorKind> integrator;
    std::optional<double> dt;
    std::optional<std::int64_t> seed;
    std::optional<std::uint32_t> bodies;
    std::optional<double> separation;
    std::optional<bool> hyperbolic;
    std::optional<int> planets;
    std::optional<bool> pluto;

    bool empty() const;
};

struct MenuAction {
    MenuGroup group;
    int index;
};

enum class MenuEffect : std::uint8_t {
    None,
    PresetChanged,
    IntegratorChanged,
    DtChanged,
    SeedChanged,
    SceneRestart,
    Staged,
    BodiesChanged,
    SeparationChanged,
    PathChanged,
    PlanetsChanged,
    PlutoChanged
};

struct MenuState {
    core::PresetName preset = core::PresetName::TwoBody;
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;
    double dt = 1.0 / 120.0;
    std::int64_t seed = 42;
    // Preset options (spec D.2/D.3). Defaults reproduce today's scenes:
    // separation 10 is two-body's today distance AND figure-eight's k=1
    // scale via the app's /10 interpretation; planets 8, pluto off,
    // circular path, cloud count 150.
    std::uint32_t bodies = 150;
    double separation = 10.0;
    bool hyperbolic = false;
    int planetCount = 8;
    bool pluto = false;
    std::optional<MenuGroup> openGroup;
    std::uint32_t restartTick = 0;

    MenuState();
    ~MenuState();
    MenuState(const MenuState& other);
    MenuState& operator=(const MenuState& other);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend struct MenuModelAccess;
};

// Row layout (spec D.1): Simulation = 4 preset rows, separator, the
// selected preset's option rows (menuPresetOptionCount), separator, restart
// (moved out of Physics; same effect as R). Cloud = 4 rows, present only
// while preset == random-cloud; hidden groups report zero items.
MenuEffect menuApply(MenuState& state, const MenuAction& action, bool paused);
// Typed seed field (spec D.4): the only way the seed changes through the
// menu (random rolls included, which come through as computed values).
// Same staging law as dt: running -> apply live (SeedChanged), paused ->
// stage (Staged, last-write-wins). The model stores exactly the value
// given; parse saturation is app-side. Seeds are signed int64 (spec D.3).
MenuEffect menuSetSeed(MenuState& state, std::int64_t seed, bool paused);
// Cloud bodies count [2, 2000] (clamping is core-side, spec D.3).
MenuEffect menuSetBodies(MenuState& state, std::uint32_t bodies,
                         bool paused);
// two-body / figure-eight separation-scale field [0.5, 500].
MenuEffect menuSetSeparation(MenuState& state, double separation,
                             bool paused);
// solar-system planet count [0, 8], inside-out prefix.
MenuEffect menuSetPlanets(MenuState& state, int planets, bool paused);
// Toggles flip the effective value: the staged mirror when one is already
// staged, else the live value. Live flips rebuild like a preset change.
MenuEffect menuTogglePath(MenuState& state, bool paused);
MenuEffect menuTogglePluto(MenuState& state, bool paused);

// State-aware conveniences over the pure (group, preset) functions in
// MenuBar.hpp.
bool menuGroupVisible(MenuGroup group, const MenuState& state);
int menuItemCount(MenuGroup group, const MenuState& state);
// Labels reflect state: preset names (Simulation 0-3), option rows with
// values/toggle state ("separation: 10", "planets: 5", "path: orbit",
// "pluto: ✓"), "restart scene" last, Cloud's "bodies: N".
std::string menuItemLabel(const MenuState& state, MenuGroup group,
                          int index);
// Hover tooltip table (spec D.6): empty string = no tooltip. Rows resolve
// against the selected preset (option rows and the restart row move with
// the preset's option count).
std::string menuItemTooltip(MenuGroup group, int index,
                            const MenuState& state);
const MenuStaged& menuStaged(const MenuState& state);
MenuStaged menuTakeStaged(MenuState& state);
const char* menuStagedSummary(const MenuState& state);
int menuTickedIndex(const MenuState& state, MenuGroup group);
bool menuIsStagedActive(const MenuState& state);

}  // namespace nbody::app
