/// @file
/// Menu state model for the in-window menu bar. MenuState holds the live
/// option values plus a hidden staged mirror (MenuStaged) that collects
/// paused edits until Space-resume applies them. The free functions here
/// are the pure mutation half of the menu: every setter follows one
/// staging law -- running applies live and clears that field's staged
/// entry, paused records into the mirror (last-write-wins per field).

#pragma once

#include "nbody/app/MenuBar.hpp"
#include "nbody/core/Presets.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace nbody::app {

/// @brief Staged mirrors of every MenuState option field.
///
/// While paused, each setter records its value here (last-write-wins per
/// field); Space-resume consumes them in the contract order
/// preset (which uses the options) -> integrator -> dt -> seed -> bodies,
/// with option rebuilds folding into the preset step.
struct MenuStaged {
    std::optional<core::PresetName> preset;          ///< Staged preset.
    std::optional<core::IntegratorKind> integrator;  ///< Staged integrator.
    std::optional<double> dt;            ///< Staged step size.
    std::optional<std::int64_t> seed;    ///< Staged scene seed.
    std::optional<std::uint32_t> bodies; ///< Staged cloud body count.
    std::optional<double> separation;    ///< Staged separation scale.
    std::optional<bool> hyperbolic;      ///< Staged path toggle.
    std::optional<int> planets;          ///< Staged planet count.
    std::optional<bool> pluto;           ///< Staged Pluto toggle.

    /// @brief True when nothing is pending: no field holds a staged value.
    bool empty() const;
};

/// @brief Identifies one dropdown row: the owning group plus the row
/// index within that group's layout (as produced by menuLayout).
struct MenuAction {
    MenuGroup group;  ///< Dropdown the activated row belongs to.
    int index;        ///< Row index within the group, layout order.
};

/// @brief What a menu interaction changed, telling the app which
/// subsystems to refresh. Staged means the change landed only in the
/// staged mirror (a paused edit); every other value is a live change.
enum class MenuEffect : std::uint8_t {
    None,              ///< Nothing changed (separator, hidden, invalid).
    PresetChanged,     ///< Preset picked live; the scene rebuilds.
    IntegratorChanged, ///< Integrator swapped live; scene untouched.
    DtChanged,         ///< Step size swapped live; scene untouched.
    SeedChanged,       ///< Seed applied live; the scene rebuilds.
    SceneRestart,      ///< Rebuild the scene from current settings.
    Staged,            ///< Value recorded in the staged mirror only.
    BodiesChanged,     ///< Cloud body count applied live.
    SeparationChanged, ///< Separation scale applied live.
    PathChanged,       ///< Orbit/fly-by toggle rebuilt the scene.
    PlanetsChanged,    ///< Solar-system planet count applied live.
    PlutoChanged       ///< Pluto toggle rebuilt the scene.
};

/// @brief Everything the menus can change: the live option values, the
/// open dropdown, a restart counter, and -- hidden behind a pimpl --
/// the staged mirror of paused edits.
///
/// Copies carry the staged mirror along with the live values, so
/// saving/restoring menu state cannot silently drop pending edits.
struct MenuState {
    core::PresetName preset = core::PresetName::TwoBody;  ///< Active scene.
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;  ///< Stepper.
    double dt = 1.0 / 120.0;  ///< Sim seconds advanced per step.
    std::int64_t seed = 42;   ///< Scene-generation seed (signed int64).
    // Preset options. The defaults reproduce the shipped scenes:
    // separation 10 is two-body's t=0 distance AND figure-eight's k=1
    // scale via the app's /10 interpretation; planets 8, pluto off,
    // circular path, cloud count 150.
    std::uint32_t bodies = 150;   ///< Cloud body count [2, 2000].
    double separation = 10.0;     ///< Separation / scale at t=0.
    bool hyperbolic = false;      ///< Fly-by path instead of orbit.
    int planetCount = 8;          ///< Solar planets, inside-out prefix.
    bool pluto = false;           ///< Extend the prefix with Pluto.
    std::optional<MenuGroup> openGroup;  ///< Dropdown currently open.
    std::uint32_t restartTick = 0;  ///< Bumped by every scene restart.

    MenuState();
    ~MenuState();
    /// @brief Deep copy: the staged mirror copies with the live values.
    MenuState(const MenuState& other);
    /// @brief Deep assign: the staged mirror assigns with the live values.
    MenuState& operator=(const MenuState& other);

private:
    struct Impl;                    ///< Holds the staged mirror only.
    std::unique_ptr<Impl> impl_;

    friend struct MenuModelAccess;  ///< Sole accessor for impl_.
};

/// @brief Apply one dropdown row activation to the state.
///
/// Row layout: Simulation = 4 preset rows, separator, restart (moved out
/// of Physics; same effect as the R key). Options = the selected preset's
/// option rows (menuPresetOptionCount): row 0 the field row, row 1 the
/// toggle. Cloud = 4 rows, present only while preset == random-cloud (it
/// shares the Options slot); hidden groups report zero items, so their
/// rows never reach here as activations.
/// @param action The activated (group, index); out-of-range rows are no-ops.
/// @param paused Running -> apply live; paused -> record into the staged
///               mirror (last-write-wins per field).
/// @return The effect to act on: Staged while paused, otherwise the live
///         change kind (None for separators and unhandled rows).
/// @note Every live apply clears that field's staged mirror entry: a live
///       pick always supersedes a pending paused pick.
MenuEffect menuApply(MenuState& state, const MenuAction& action, bool paused);
/// @brief Set the seed; the typed field is the only path by which the
/// menu changes the seed (random rolls included, which arrive here as
/// already-computed values).
///
/// Same staging law as dt: running -> apply live (SeedChanged), paused ->
/// stage (Staged, last-write-wins). The model stores exactly the value
/// given -- parse saturation is editor-side, not here. Seeds are signed
/// int64.
/// @param seed The already-parsed seed value, stored verbatim.
/// @param paused Running -> live apply; paused -> stage.
/// @return SeedChanged when applied live, Staged when recorded.
MenuEffect menuSetSeed(MenuState& state, std::int64_t seed, bool paused);
/// @brief Set the cloud body count; the valid range is [2, 2000] and
/// clamping into it is core-side (scene generation), not here.
/// @param bodies The requested count, stored verbatim by the model.
/// @param paused Running -> live apply; paused -> stage.
/// @return BodiesChanged when applied live, Staged when recorded.
MenuEffect menuSetBodies(MenuState& state, std::uint32_t bodies,
                         bool paused);
/// @brief Set the two-body / figure-eight separation-scale field.
/// The [0.5, 500] clamp lives in the editor's commit parse
/// (fieldParseSeparation); neither this setter nor core clamps it.
/// @param separation The already-parsed and already-clamped scale.
/// @param paused Running -> live apply; paused -> stage.
/// @return SeparationChanged when applied live, Staged when recorded.
MenuEffect menuSetSeparation(MenuState& state, double separation,
                             bool paused);
/// @brief Set the solar-system planet count, an inside-out prefix of
/// [0, 8]; counts outside the range clamp core-side at scene build.
/// @param planets The requested planet count.
/// @param paused Running -> live apply; paused -> stage.
/// @return PlanetsChanged when applied live, Staged when recorded.
MenuEffect menuSetPlanets(MenuState& state, int planets, bool paused);
/// @brief Flip the two-body path (orbit <-> hyperbolic fly-by).
/// The flip targets the EFFECTIVE value: the staged mirror when a flip
/// is already staged, else the live value. Live flips rebuild the scene
/// like a preset change; a live flip also clears any staged flip.
/// @param paused Running -> flip live; paused -> flip the staged value.
/// @return PathChanged when flipped live, Staged when recorded.
MenuEffect menuTogglePath(MenuState& state, bool paused);
/// @brief Flip the solar-system Pluto toggle; same effective-value and
/// live-clears-staged laws as menuTogglePath.
/// @param paused Running -> flip live; paused -> flip the staged value.
/// @return PlutoChanged when flipped live, Staged when recorded.
MenuEffect menuTogglePluto(MenuState& state, bool paused);

/// @brief State-aware convenience over the pure (group, preset) form in
/// MenuBar.hpp: visibility depends only on the state's selected preset.
/// @return True when the group's header shows on the bar.
bool menuGroupVisible(MenuGroup group, const MenuState& state);
/// @brief State-aware row count for an open dropdown (zero if hidden).
/// @return The group's row count for the state's current preset.
int menuItemCount(MenuGroup group, const MenuState& state);
/// @brief The row's display label, reflecting current state: preset
/// names (Simulation 0-3), option rows with values/toggle state
/// ("separation: 10", "planets: 5", "path: orbit", "pluto: on"),
/// "restart scene" last, Cloud's "bodies: N".
/// @return The label text; empty for separators, out-of-range rows, and
///         value-less rows.
std::string menuItemLabel(const MenuState& state, MenuGroup group,
                          int index);
/// @brief The row's hover tooltip. Rows resolve against the selected
/// preset (option rows and the restart row move with the preset's option
/// count).
/// @return The tooltip text; empty string means no tooltip.
std::string menuItemTooltip(MenuGroup group, int index,
                            const MenuState& state);
/// @brief Read-only view of the staged mirror (does not consume it).
/// @return Reference to the state's pending paused edits.
const MenuStaged& menuStaged(const MenuState& state);
/// @brief Consume the staged mirror: return the pending edits and clear
/// them (plus the cached summary) so a resume applies each exactly once.
/// @return The staged values as of this call; the mirror is empty after.
MenuStaged menuTakeStaged(MenuState& state);
/// @brief Human-readable list of the pending staged edits, in the
/// Space-resume contract order (preset, integrator, dt, seed, options).
/// @return A static per-state buffer ("preset → solar-system, dt → 1/60")
///         valid until the next call on the same state; empty when
///         nothing is staged.
const char* menuStagedSummary(const MenuState& state);
/// @brief Which row of the group carries the current-value tick dot.
/// @return The index matching the state's value, or -1 when no row
///         should be ticked (field, roll, and toggle rows never tick:
///         toggle state shows in the label instead).
int menuTickedIndex(const MenuState& state, MenuGroup group);
/// @brief Whether any staged edit is pending a Space-resume.
/// @return True when the staged mirror holds at least one value.
bool menuIsStagedActive(const MenuState& state);

}  // namespace nbody::app
