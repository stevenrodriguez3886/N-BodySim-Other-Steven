#include "nbody/app/MenuModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>

using nbody::app::MenuAction;
using nbody::app::MenuEffect;
using nbody::app::MenuGroup;
using nbody::app::MenuState;
using nbody::app::menuApply;
using nbody::app::menuGroupVisible;
using nbody::app::menuIsStagedActive;
using nbody::app::menuItemCount;
using nbody::app::menuPresetOptionCount;
using nbody::app::menuSetBodies;
using nbody::app::menuSetPlanets;
using nbody::app::menuSetSeed;
using nbody::app::menuSetSeparation;
using nbody::app::menuStaged;
using nbody::app::menuStagedSummary;
using nbody::app::menuTakeStaged;
using nbody::app::menuTickedIndex;
using nbody::app::menuTogglePath;
using nbody::app::menuTogglePluto;
using nbody::core::IntegratorKind;
using nbody::core::PresetName;

namespace {

// The restart row is the LAST Simulation row; its index shifts with the
// selected preset's option-row count (spec D.1).
int restartRow(PresetName preset) {
    return 6 + nbody::app::menuPresetOptionCount(preset);
}

}  // namespace

TEST_CASE("model_apply_live_preset_and_other_live_effects") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 1}, false) ==
            MenuEffect::PresetChanged);
    REQUIRE(state.preset == PresetName::SolarSystem);
    REQUIRE(menuApply(state, {MenuGroup::Integrator, 0}, false) ==
            MenuEffect::IntegratorChanged);
    REQUIRE(state.integrator == IntegratorKind::Rk4);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 2}, false) ==
            MenuEffect::DtChanged);
    REQUIRE(state.dt == 1.0 / 120.0);
    // The Cloud rows are fields/roll buttons wired app-side (Task 4):
    // menuApply never mutates the seed or bodies through a row click.
    REQUIRE(menuApply(state, {MenuGroup::Cloud, 0}, false) == MenuEffect::None);
    REQUIRE(menuApply(state, {MenuGroup::Cloud, 1}, false) == MenuEffect::None);
    REQUIRE(menuApply(state, {MenuGroup::Cloud, 2}, false) == MenuEffect::None);
    REQUIRE(menuApply(state, {MenuGroup::Cloud, 3}, false) == MenuEffect::None);
    REQUIRE(state.seed == 42);
    REQUIRE(menuSetSeed(state, 43, false) == MenuEffect::SeedChanged);
    REQUIRE(state.seed == 43);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_apply_stages_while_paused") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Integrator, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(state.preset == PresetName::TwoBody);
    REQUIRE(state.integrator == IntegratorKind::Rk4);
    REQUIRE(menuStaged(state).preset == PresetName::FigureEight);
    REQUIRE(menuStaged(state).integrator == IntegratorKind::Euler);
}

TEST_CASE("model_stage_last_write_wins") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 2}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuStaged(state).preset == PresetName::RandomCloud);
}

TEST_CASE("model_take_staged_returns_and_clears_staged_values") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Physics, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuSetSeed(state, 43, true) == MenuEffect::Staged);
    const auto staged = menuTakeStaged(state);
    REQUIRE(staged.dt == 1.0 / 240.0);
    REQUIRE(staged.seed == 43);
    REQUIRE(menuStaged(state).empty());
    REQUIRE(menuTakeStaged(state).empty());
}

TEST_CASE("model_restart_applies_while_paused") {
    MenuState state;  // default preset two-body: restart sits at row 8.
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 8}, true) ==
            MenuEffect::SceneRestart);
    REQUIRE(state.restartTick == 1);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_restart_while_paused_preserves_staged") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Integrator, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 0}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuSetSeed(state, 1234, true) == MenuEffect::Staged);

    // state.preset is still two-body (preset change was only staged), so the
    // restart row is 6 + two-body's 2 option rows = 8.
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 8}, true) ==
            MenuEffect::SceneRestart);
    CHECK(state.restartTick == 1);
    CHECK(menuStaged(state).preset == PresetName::FigureEight);
    CHECK(menuStaged(state).integrator.has_value());
    CHECK(menuStaged(state).integrator.value_or(
              IntegratorKind::Rk4) == IntegratorKind::Euler);
    CHECK(menuStaged(state).dt == 1.0 / 30.0);
    CHECK(menuStaged(state).seed == 1234);
}

TEST_CASE("model_set_seed_live_and_staged") {
    MenuState state;
    REQUIRE(menuSetSeed(state, 777, false) == MenuEffect::SeedChanged);
    REQUIRE(state.seed == 777);
    REQUIRE(menuStaged(state).empty());

    // Paused: staged only, state untouched; last-write-wins.
    REQUIRE(menuSetSeed(state, 999, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).seed == 999);
    REQUIRE(state.seed == 777);
    REQUIRE(menuSetSeed(state, 1000, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).seed == 1000);
    REQUIRE(state.seed == 777);

    // The typed field replaced the stepper: no seed row ever ticks.
    REQUIRE(menuTickedIndex(state, MenuGroup::Cloud) == -1);
}

// Spec D.3: seeds are signed int64; the model stores exactly what it is
// given. Negative seeds must pass through in both paths.
TEST_CASE("model_seed_accepts_negative") {
    MenuState state;
    REQUIRE(menuSetSeed(state, -5, false) == MenuEffect::SeedChanged);
    REQUIRE(state.seed == -5);
    REQUIRE(menuSetSeed(state, -7, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).seed == -7);
    REQUIRE(state.seed == -5);
}

// The model stores exactly what it is given (no clamping). Boundary values
// must pass through in both paths; saturation-at-parse is app-side.
TEST_CASE("model_seed_set_stores_zero") {
    MenuState state;
    REQUIRE(menuSetSeed(state, 0, false) == MenuEffect::SeedChanged);
    REQUIRE(state.seed == 0);
    REQUIRE(menuSetSeed(state, 0, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).seed == 0);
}

TEST_CASE("model_seed_set_stores_extremes") {
    constexpr auto maxSeed = std::numeric_limits<std::int64_t>::max();
    constexpr auto minSeed = std::numeric_limits<std::int64_t>::min();

    MenuState liveState;
    REQUIRE(menuSetSeed(liveState, maxSeed, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(liveState.seed == maxSeed);
    REQUIRE(menuSetSeed(liveState, minSeed, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(liveState.seed == minSeed);

    MenuState stagedState;
    REQUIRE(menuSetSeed(stagedState, maxSeed, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(stagedState).seed == maxSeed);
}

TEST_CASE("model_offlist_dt_has_no_tick_until_a_menu_value_is_applied") {
    MenuState state;
    state.dt = 5.0;
    REQUIRE(menuTickedIndex(state, MenuGroup::Physics) == -1);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 0}, false) ==
            MenuEffect::DtChanged);
    REQUIRE(state.dt == 1.0 / 30.0);
    REQUIRE(menuTickedIndex(state, MenuGroup::Physics) == 0);
}

TEST_CASE("model_staged_summary_lists_groups_in_contract_order") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuIsStagedActive(state));
    REQUIRE(menuStagedSummary(state) ==
            std::string("preset → figure-eight, dt → 1/240"));
}

// ---- Task 3: visibility, row tables, option setters, summary clauses ----

TEST_CASE("model_visibility_matrix_two_presets_x_four_groups") {
    MenuState state;  // preset two-body
    REQUIRE(menuGroupVisible(MenuGroup::Simulation, state));
    REQUIRE(menuGroupVisible(MenuGroup::Integrator, state));
    REQUIRE(menuGroupVisible(MenuGroup::Physics, state));
    REQUIRE(!menuGroupVisible(MenuGroup::Cloud, state));
    REQUIRE(menuItemCount(MenuGroup::Cloud, state) == 0);

    state.preset = PresetName::RandomCloud;
    REQUIRE(menuGroupVisible(MenuGroup::Simulation, state));
    REQUIRE(menuGroupVisible(MenuGroup::Integrator, state));
    REQUIRE(menuGroupVisible(MenuGroup::Physics, state));
    REQUIRE(menuGroupVisible(MenuGroup::Cloud, state));
    REQUIRE(menuItemCount(MenuGroup::Cloud, state) == 4);
}

TEST_CASE("model_preset_option_counts_and_simulation_row_tables") {
    // Option rows per preset (spec D.1/D.2): two-body 2, solar 2, cloud 0,
    // figure-eight 1. Row count = 4 presets + separator + options +
    // separator + restart = 7 + options.
    REQUIRE(menuPresetOptionCount(PresetName::TwoBody) == 2);
    REQUIRE(menuPresetOptionCount(PresetName::SolarSystem) == 2);
    REQUIRE(menuPresetOptionCount(PresetName::RandomCloud) == 0);
    REQUIRE(menuPresetOptionCount(PresetName::FigureEight) == 1);

    const PresetName presets[] = {PresetName::TwoBody,
                                  PresetName::SolarSystem,
                                  PresetName::RandomCloud,
                                  PresetName::FigureEight};
    for (PresetName preset : presets) {
        MenuState state;
        state.preset = preset;
        const int opts = menuPresetOptionCount(preset);
        REQUIRE(menuItemCount(MenuGroup::Simulation, state) == 7 + opts);
        REQUIRE(restartRow(preset) == 6 + opts);

        // Separators and out-of-range rows never act...
        REQUIRE(menuApply(state, {MenuGroup::Simulation, 4}, false) ==
                MenuEffect::None);
        REQUIRE(menuApply(state, {MenuGroup::Simulation, 5 + opts}, false) ==
                MenuEffect::None);
        REQUIRE(menuApply(state, {MenuGroup::Simulation, 7 + opts}, false) ==
                MenuEffect::None);
        REQUIRE(menuApply(state, {MenuGroup::Simulation, -1}, false) ==
                MenuEffect::None);
        // ...while the restart row always acts, live and paused.
        REQUIRE(menuApply(state, {MenuGroup::Simulation, restartRow(preset)},
                          false) == MenuEffect::SceneRestart);
        REQUIRE(state.restartTick == 1);
    }
}

TEST_CASE("model_physics_lost_restart") {
    MenuState state;
    // Physics is the plain 5-row dt list now: the old separator (5) and
    // restart (6) rows are gone; only dt rows 0-4 act.
    REQUIRE(menuApply(state, {MenuGroup::Physics, 4}, false) ==
            MenuEffect::DtChanged);
    REQUIRE(state.dt == 1.0 / 480.0);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 5}, true) ==
            MenuEffect::None);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 6}, true) ==
            MenuEffect::None);
    REQUIRE(state.restartTick == 0);
}

TEST_CASE("model_toggle_path_row_and_setter") {
    MenuState state;  // two-body default
    REQUIRE(state.hyperbolic == false);

    // Row click (two-body option row 6 = the path toggle), live.
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 6}, false) ==
            MenuEffect::PathChanged);
    REQUIRE(state.hyperbolic == true);
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 6}, false) ==
            MenuEffect::PathChanged);
    REQUIRE(state.hyperbolic == false);

    // Paused: staged flip only.
    REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).hyperbolic == true);
    REQUIRE(state.hyperbolic == false);
    // Second paused flip must flip the STAGED value (last-write-wins).
    REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).hyperbolic == false);
    REQUIRE(state.hyperbolic == false);

    // Live setter triple.
    REQUIRE(menuTogglePath(state, false) == MenuEffect::PathChanged);
    REQUIRE(state.hyperbolic == true);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_toggle_pluto_row_and_setter") {
    MenuState state;
    state.preset = PresetName::SolarSystem;
    REQUIRE(state.pluto == false);

    // Row click (solar-system option row 6 = the pluto toggle), live.
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 6}, false) ==
            MenuEffect::PlutoChanged);
    REQUIRE(state.pluto == true);

    REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).pluto == false);  // flip of live true
    REQUIRE(state.pluto == true);
    REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).pluto == true);

    REQUIRE(menuTogglePluto(state, false) == MenuEffect::PlutoChanged);
    REQUIRE(state.pluto == false);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_option_field_rows_do_not_act_through_menuApply") {
    // Field rows (separation/planets) focus the inline editor app-side
    // (Task 4); menuApply itself never mutates.
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 5}, false) ==
            MenuEffect::None);
    state.preset = PresetName::SolarSystem;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 5}, false) ==
            MenuEffect::None);
    state.preset = PresetName::FigureEight;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 5}, false) ==
            MenuEffect::None);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_set_bodies_live_and_staged") {
    MenuState state;
    REQUIRE(state.bodies == 150);

    REQUIRE(menuSetBodies(state, 300, false) == MenuEffect::BodiesChanged);
    REQUIRE(state.bodies == 300);
    REQUIRE(menuStaged(state).empty());

    REQUIRE(menuSetBodies(state, 77, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).bodies == 77);
    REQUIRE(state.bodies == 300);
    REQUIRE(menuSetBodies(state, 78, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).bodies == 78);

    auto staged = menuTakeStaged(state);
    REQUIRE(staged.bodies == 78);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_set_separation_live_and_staged") {
    MenuState state;
    REQUIRE(state.separation == 10.0);

    REQUIRE(menuSetSeparation(state, 20.0, false) ==
            MenuEffect::SeparationChanged);
    REQUIRE(state.separation == 20.0);
    REQUIRE(menuStaged(state).empty());

    REQUIRE(menuSetSeparation(state, 0.5, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).separation == 0.5);
    REQUIRE(state.separation == 20.0);
    REQUIRE(menuSetSeparation(state, 500.0, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).separation == 500.0);
}

TEST_CASE("model_set_planets_live_and_staged") {
    MenuState state;
    REQUIRE(state.planetCount == 8);

    REQUIRE(menuSetPlanets(state, 5, false) == MenuEffect::PlanetsChanged);
    REQUIRE(state.planetCount == 5);
    REQUIRE(menuStaged(state).empty());

    REQUIRE(menuSetPlanets(state, 0, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).planets == 0);
    REQUIRE(state.planetCount == 5);
    REQUIRE(menuSetPlanets(state, 8, true) == MenuEffect::Staged);
    REQUIRE(menuStaged(state).planets == 8);
}

TEST_CASE("model_take_staged_carries_option_mirrors") {
    MenuState state;
    REQUIRE(menuSetSeparation(state, 25.0, true) == MenuEffect::Staged);
    REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
    REQUIRE(menuSetPlanets(state, 3, true) == MenuEffect::Staged);
    REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
    REQUIRE(menuSetBodies(state, 400, true) == MenuEffect::Staged);
    const auto staged = menuTakeStaged(state);
    REQUIRE(staged.separation == 25.0);
    REQUIRE(staged.hyperbolic == true);
    REQUIRE(staged.planets == 3);
    REQUIRE(staged.pluto == true);
    REQUIRE(staged.bodies == 400);
    REQUIRE(menuStaged(state).empty());
    // Every staged option counts as staged activity.
    REQUIRE(menuSetPlanets(state, 4, true) == MenuEffect::Staged);
    REQUIRE(menuIsStagedActive(state));
}

TEST_CASE("model_staged_summary_option_clauses") {
    {
        MenuState state;
        REQUIRE(menuSetSeparation(state, 20.0, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("separation → 20"));
    }
    {
        MenuState state;
        REQUIRE(menuSetSeparation(state, 10.5, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("separation → 10.5"));
    }
    {
        MenuState state;
        REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("path → hyperbola"));
        REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("path → orbit"));
    }
    {
        MenuState state;
        REQUIRE(menuSetPlanets(state, 5, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("planets → 5"));
    }
    {
        MenuState state;
        REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("pluto → on"));
        REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("pluto → off"));
    }
    {
        MenuState state;
        REQUIRE(menuSetBodies(state, 300, true) == MenuEffect::Staged);
        REQUIRE(menuStagedSummary(state) == std::string("bodies → 300"));
    }
}

TEST_CASE("model_staged_summary_full_contract_order") {
    // preset → integrator → dt → seed → bodies → separation → path →
    // planets → pluto (mirrors the Space-resume consumption order).
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Integrator, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuSetSeed(state, 7, true) == MenuEffect::Staged);
    REQUIRE(menuSetBodies(state, 300, true) == MenuEffect::Staged);
    REQUIRE(menuSetSeparation(state, 20.0, true) == MenuEffect::Staged);
    REQUIRE(menuTogglePath(state, true) == MenuEffect::Staged);
    REQUIRE(menuSetPlanets(state, 5, true) == MenuEffect::Staged);
    REQUIRE(menuTogglePluto(state, true) == MenuEffect::Staged);
    REQUIRE(menuStagedSummary(state) ==
            std::string("preset → solar-system, integrator → euler, "
                        "dt → 1/60, seed → 7, bodies → 300, "
                        "separation → 20, path → hyperbola, "
                        "planets → 5, pluto → on"));
}

TEST_CASE("model_ticked_indices_with_renamed_groups") {
    MenuState state;
    REQUIRE(menuTickedIndex(state, MenuGroup::Simulation) == 0);
    REQUIRE(menuApply(state, {MenuGroup::Simulation, 2}, false) ==
            MenuEffect::PresetChanged);
    REQUIRE(menuTickedIndex(state, MenuGroup::Simulation) == 2);
    REQUIRE(menuTickedIndex(state, MenuGroup::Cloud) == -1);
    state.preset = PresetName::SolarSystem;
    REQUIRE(menuTickedIndex(state, MenuGroup::Simulation) == 1);
}
