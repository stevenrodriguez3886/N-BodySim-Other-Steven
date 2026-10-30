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
using nbody::app::menuIsStagedActive;
using nbody::app::menuStaged;
using nbody::app::menuStagedSummary;
using nbody::app::menuTakeStaged;
using nbody::app::menuTickedIndex;
using nbody::core::IntegratorKind;
using nbody::core::PresetName;

TEST_CASE("model_apply_live_preset_and_other_live_effects") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Preset, 1}, false) ==
            MenuEffect::PresetChanged);
    REQUIRE(state.preset == PresetName::SolarSystem);
    REQUIRE(menuApply(state, {MenuGroup::Integrator, 0}, false) ==
            MenuEffect::IntegratorChanged);
    REQUIRE(state.integrator == IntegratorKind::Rk4);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 2}, false) ==
            MenuEffect::DtChanged);
    REQUIRE(state.dt == 1.0 / 120.0);
    REQUIRE(menuApply(state, {MenuGroup::Seed, 1}, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(state.seed == 43);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_apply_stages_while_paused") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Preset, 3}, true) ==
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
    REQUIRE(menuApply(state, {MenuGroup::Preset, 1}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Preset, 2}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuStaged(state).preset == PresetName::RandomCloud);
}

TEST_CASE("model_take_staged_returns_and_clears_staged_values") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Physics, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Seed, 1}, true) == MenuEffect::Staged);
    const auto staged = menuTakeStaged(state);
    REQUIRE(staged.dt == 1.0 / 240.0);
    REQUIRE(staged.seed == 43);
    REQUIRE(menuStaged(state).empty());
    REQUIRE(menuTakeStaged(state).empty());
}

TEST_CASE("model_restart_applies_while_paused") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Physics, 6}, true) ==
            MenuEffect::SceneRestart);
    REQUIRE(state.restartTick == 1);
    REQUIRE(menuStaged(state).empty());
}

TEST_CASE("model_restart_while_paused_preserves_staged") {
    MenuState state;
    REQUIRE(menuApply(state, {MenuGroup::Preset, 3}, true) ==
            MenuEffect::Staged);

    REQUIRE(menuApply(state, {MenuGroup::Physics, 6}, true) ==
            MenuEffect::SceneRestart);
    CHECK(state.restartTick == 1);
    CHECK(menuStaged(state).preset == PresetName::FigureEight);
}

TEST_CASE("model_seed_stepper_floors_at_zero") {
    MenuState state;
    state.seed = 0;
    REQUIRE(menuApply(state, {MenuGroup::Seed, 0}, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(state.seed == 0);
    state.seed = 7;
    REQUIRE(menuApply(state, {MenuGroup::Seed, 1}, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(state.seed == 8);
}

TEST_CASE("model_seed_stepper_saturates_high") {
    constexpr auto maxSeed = std::numeric_limits<std::uint32_t>::max();

    MenuState liveState;
    liveState.seed = maxSeed;
    REQUIRE(menuApply(liveState, {MenuGroup::Seed, 1}, false) ==
            MenuEffect::SeedChanged);
    REQUIRE(liveState.seed == maxSeed);

    MenuState stagedState;
    stagedState.seed = maxSeed;
    REQUIRE(menuApply(stagedState, {MenuGroup::Seed, 1}, true) ==
            MenuEffect::Staged);
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
    REQUIRE(menuApply(state, {MenuGroup::Preset, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuApply(state, {MenuGroup::Physics, 3}, true) ==
            MenuEffect::Staged);
    REQUIRE(menuIsStagedActive(state));
    REQUIRE(menuStagedSummary(state) ==
            std::string("preset → figure-eight, dt → 1/240"));
}