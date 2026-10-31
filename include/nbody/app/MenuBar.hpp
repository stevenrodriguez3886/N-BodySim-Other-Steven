#pragma once

#include "nbody/core/Presets.hpp"
#include "nbody/core/Vec2.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nbody::app {

// Menu groups (menu-bar rework spec): Simulation carries ONLY the preset
// list, one separator and the restart row; Cloud (visible only while
// preset == random-cloud) carries the seed and bodies fields plus their
// roll buttons; Options (the complement of Cloud, sharing the same last
// bar slot) carries the active preset's option rows.
enum class MenuGroup : std::uint8_t { Simulation, Integrator, Physics, Cloud,
                                    Options, Count_ };

// Positions and sizes are expressed in window pixels.
struct MenuRect {
    core::Vec2 topLeft;
    core::Vec2 size;

    bool contains(core::Vec2 p) const;
};

enum class MenuHitKind : std::uint8_t { None, Header, Item };

struct MenuHit {
    MenuHitKind kind = MenuHitKind::None;
    MenuGroup group{};
    int index = -1;
};

struct MenuLayout {
    std::vector<MenuRect> headers;
    // Header i belongs to headerGroups[i]; hidden groups are absent from
    // BOTH vectors (the Cloud header exists only while random-cloud is
    // selected), so header position must never be mapped to the enum index.
    std::vector<MenuGroup> headerGroups;
    std::optional<MenuGroup> openGroup;
    std::vector<MenuRect> items;
    std::vector<MenuRect> disabledRows;
};

// Row counts: Simulation is 6 for every preset (4 presets, separator at
// index 4, restart at 5); Options is the preset's option count (two-body
// 2, solar 2, fig-8 1, cloud 0); hidden groups yield zero items and no
// header, so hit tests never see hidden rows.
MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup,
                      core::PresetName preset);
MenuHit menuHitTest(const MenuLayout& layout, core::Vec2 p);
int menuPresetOptionCount(core::PresetName preset);
bool menuGroupVisible(MenuGroup group, core::PresetName preset);
int menuItemCount(MenuGroup group, core::PresetName preset);
const char* menuGroupLabel(MenuGroup group);
const char* menuHeaderTooltip(MenuGroup group);
const char* menuPresetName(core::PresetName preset);
// Short preset name shown as the Options header label ("Solar",
// "Two-body", "Fig-8"); empty when the preset has no options.
const char* menuOptionsHeaderLabel(core::PresetName preset);

}  // namespace nbody::app
