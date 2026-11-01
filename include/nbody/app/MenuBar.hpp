/// @file
/// Pure menu-bar geometry: dropdown layout, hit testing, and the label,
/// tooltip, and row-count tables shared by the renderer and the state
/// model. Everything here is display-free (window pixels in, rects out),
/// so the geometry and row-layout laws are unit-testable without any
/// rendering.

#pragma once

#include "nbody/core/Presets.hpp"
#include "nbody/core/Vec2.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nbody::app {

/// @brief The menu-bar dropdown groups, in bar order.
///
/// Simulation carries ONLY the preset list, one separator and the
/// restart row; Cloud (visible only while preset == random-cloud) carries
/// the seed and bodies fields plus their roll buttons; Options (the
/// complement of Cloud, sharing the same last bar slot) carries the
/// active preset's option rows.
enum class MenuGroup : std::uint8_t { Simulation, Integrator, Physics, Cloud,
                                    Options, Count_ };

/// @brief An axis-aligned rectangle in window pixels.
struct MenuRect {
    core::Vec2 topLeft;  ///< Top-left corner, window pixels.
    core::Vec2 size;     ///< Extent, window pixels.

    /// @brief Half-open containment: includes topLeft, excludes the right
    /// and bottom edges.
    /// @param p The point to test, window pixels.
    /// @return True when p falls inside the rect.
    bool contains(core::Vec2 p) const;
};

/// @brief What kind of element a menu hit test landed on.
enum class MenuHitKind : std::uint8_t { None, Header, Item };

/// @brief The result of testing a point against a menu layout.
struct MenuHit {
    MenuHitKind kind = MenuHitKind::None;  ///< What was hit, if anything.
    MenuGroup group{};  ///< The owning group (header or open-group item).
    int index = -1;     ///< Item row; -1 for headers and misses.
};

/// @brief A frame of the menu bar: the header rects plus the item rects
/// of the currently open dropdown (if any).
struct MenuLayout {
    std::vector<MenuRect> headers;
    // Header i belongs to headerGroups[i]; hidden groups are absent from
    // BOTH vectors (the Cloud header exists only while random-cloud is
    // selected), so header position must never be mapped to the enum
    // index.
    std::vector<MenuGroup> headerGroups;
    std::optional<MenuGroup> openGroup;  ///< The expanded dropdown.
    std::vector<MenuRect> items;         ///< Rows of the open dropdown.
    std::vector<MenuRect> disabledRows;  ///< Rows ignoring activation.
};

/// @brief Build the bar geometry for one frame.
///
/// Row counts: Simulation is 6 for every preset (4 presets, separator at
/// index 4, restart at 5); Options is the preset's option count (two-body
/// 2, solar 2, fig-8 1, cloud 0); hidden groups yield zero items and no
/// header, so hit tests never see hidden rows. Hidden groups also take no
/// bar space, so header positions never jump when presets change.
/// @param winW Window width in pixels (unused; kept for the API).
/// @param winH Window height in pixels; dropdowns flip upward if short.
/// @param openGroup The dropdown to expand, if any (hidden groups behave
///                  as if closed).
/// @param preset The selected preset, driving visibility and row counts.
/// @return The header rects plus the open dropdown's item rects.
MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup,
                      core::PresetName preset);
/// @brief Test a point against a layout: headers first, then the open
/// dropdown's rows.
/// @param layout A layout from menuLayout.
/// @param p The cursor position, window pixels.
/// @return The hit (kind, group, index); None kind on a miss.
MenuHit menuHitTest(const MenuLayout& layout, core::Vec2 p);
/// @brief The preset's option-row count in the Options group: two-body 2
/// (separation, path), solar-system 2 (planets, pluto), figure-eight 1
/// (separation scale), random-cloud 0 (its options live in Cloud).
/// @return The number of Options rows the preset contributes.
int menuPresetOptionCount(core::PresetName preset);
/// @brief Whether a group shows its header for a preset: Simulation,
/// Integrator, and Physics always; Cloud only for random-cloud; Options
/// for every preset with option rows (Cloud's complement in the last bar
/// slot).
/// @return True when the group is visible on the bar.
bool menuGroupVisible(MenuGroup group, core::PresetName preset);
/// @brief The dropdown row count for a (group, preset) pair; hidden
/// groups report zero.
/// @return The number of item rows, including separators.
int menuItemCount(MenuGroup group, core::PresetName preset);
/// @brief The group's fixed header label ("Simulation", ...).
/// @return A static string; empty for out-of-range groups.
const char* menuGroupLabel(MenuGroup group);
/// @brief The group header's hover tooltip.
/// @return A static string; empty for out-of-range groups.
const char* menuHeaderTooltip(MenuGroup group);
/// @brief The preset's lowercase menu name ("two-body", ...).
/// @return A static string; empty for out-of-range presets.
const char* menuPresetName(core::PresetName preset);
/// @brief Short preset name shown as the Options header label ("Solar",
/// "Two-body", "Fig-8").
/// @return A static string; empty when the preset has no options.
const char* menuOptionsHeaderLabel(core::PresetName preset);

/// @brief Pixel indent of the dropdown tick dots: dots draw at
/// row.x + this.
inline constexpr double kMenuRowDotIndent = 10.0;
/// @brief The ONE dropdown-row text-indent law, shared by layout and
/// render so the two cannot drift.
///
/// Dot-class rows (presets, integrator, dt) and value/field/toggle rows
/// keep dot-at-10 / text-at-30 (toggles reserve the value indent but
/// never draw a dot -- menuTickedIndex is -1 for them); PLAIN rows
/// (restart, roll buttons, separators) indent 10. renderMenu draws every
/// generic row label at row.x + menuRowTextIndent(...) and every tick dot
/// at row.x + kMenuRowDotIndent. Focused field editors use their own
/// draw pass and do not consult this.
/// @return The text indent in pixels for the row's group and index.
double menuRowTextIndent(MenuGroup group, int index, core::PresetName preset);

}  // namespace nbody::app
