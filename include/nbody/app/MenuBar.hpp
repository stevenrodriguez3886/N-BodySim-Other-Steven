#pragma once

#include "nbody/core/Vec2.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nbody::app {

enum class MenuGroup : std::uint8_t { Preset, Integrator, Physics, Seed, Count_ };

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
    std::optional<MenuGroup> openGroup;
    std::vector<MenuRect> items;
    std::vector<MenuRect> disabledRows;
};

MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup);
MenuHit menuHitTest(const MenuLayout& layout, core::Vec2 p);
int menuItemCount(MenuGroup group);
const char* menuGroupLabel(MenuGroup group);
std::string menuItemLabel(MenuGroup group, int index, std::uint32_t seed);

}  // namespace nbody::app
