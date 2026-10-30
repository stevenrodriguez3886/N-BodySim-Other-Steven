#pragma once

#include "nbody/app/MenuBar.hpp"
#include "nbody/core/Presets.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace nbody::app {

struct MenuStaged {
    std::optional<core::PresetName> preset;
    std::optional<core::IntegratorKind> integrator;
    std::optional<double> dt;
    std::optional<std::uint32_t> seed;

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
    Staged
};

struct MenuState {
    core::PresetName preset = core::PresetName::TwoBody;
    core::IntegratorKind integrator = core::IntegratorKind::Rk4;
    double dt = 1.0 / 120.0;
    std::uint32_t seed = 42;
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

MenuEffect menuApply(MenuState& state, const MenuAction& action, bool paused);
const MenuStaged& menuStaged(const MenuState& state);
MenuStaged menuTakeStaged(MenuState& state);
const char* menuStagedSummary(const MenuState& state);
int menuTickedIndex(const MenuState& state, MenuGroup group);
bool menuIsStagedActive(const MenuState& state);

}  // namespace nbody::app
