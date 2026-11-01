/// @file
/// The pure menu-bar geometry tables and layout math: header sizing,
/// dropdown row counts, visibility per preset, and the shared row-indent
/// law. All sizes are deterministic (no font metrics), keeping layout
/// unit-testable and the renderer's job to drawing rects.

#include "nbody/app/MenuBar.hpp"

namespace nbody::app {
namespace {

constexpr double kHeaderX = 10.0;
constexpr double kHeaderY = 6.0;
constexpr double kHeaderHeight = 28.0;
constexpr double kHeaderGap = 8.0;
constexpr double kDropdownY = 38.0;
constexpr double kDropdownBottom = 6.0;
constexpr double kItemHeight = 24.0;
constexpr double kMinimumItemWidth = 140.0;
constexpr double kMinimumHeaderWidth = 90.0;
constexpr double kHeaderPadding = 16.0;
// Deterministic content sizing (no font access in the pure layer): a
// fixed label advance plus a per-group value extent for the widest
// plausible header readout, floored at kMinimumHeaderWidth. Widest
// values: the Cloud seed readout (19 chars), the Simulation preset name
// ("figure-eight", 12) and the Options planets+pluto readout (12).
// Every header renders "label: value", so no group gets a zero value
// extent -- a zero here would let the text overflow into the
// neighbouring tab, which this table exists to prevent.
/// @brief Pixels assumed per label character when sizing headers.
constexpr double kLabelAdvance = 7.0;
/// @brief Reserved width for each group's widest "label: value" readout.
constexpr double kValueExtents[] = {98.0, 49.0, 56.0, 99.0, 80.0};
constexpr const char* kGroupLabels[] = {"Simulation", "Integrator",
                                        "Physics", "Cloud", "Options"};
constexpr std::size_t kGroupCount =
    sizeof(kGroupLabels) / sizeof(kGroupLabels[0]);
static_assert(kGroupCount == static_cast<std::size_t>(MenuGroup::Count_),
              "one label per menu group");

/// @brief The fixed-advance width of a header label (kLabelAdvance per
/// character).
/// @param label NUL-terminated header label text.
/// @return The label's width in pixels.
constexpr double labelExtent(const char* label) {
    double width = 0.0;
    for (const char* p = label; *p != '\0'; ++p) {
        width += kLabelAdvance;
    }
    return width;
}

/// @brief One header's content width: label + value extent + padding,
/// raised to kMinimumHeaderWidth.
/// @param group Menu group index into the label/extent tables.
/// @return The header's width in pixels.
constexpr double headerWidthFor(std::size_t group) {
    const double width =
        labelExtent(kGroupLabels[group]) + kValueExtents[group] +
        kHeaderPadding;
    return width > kMinimumHeaderWidth ? width : kMinimumHeaderWidth;
}

// -> {184, 135, 121, 150, 145}: every header is wide enough for its
// "label: value" readout to fit without touching the neighbouring tab.
constexpr double kHeaderWidths[] = {headerWidthFor(0), headerWidthFor(1),
                                    headerWidthFor(2), headerWidthFor(3),
                                    headerWidthFor(4)};
/// @brief Per-group header tooltips, indexed by MenuGroup.
constexpr const char* kHeaderTooltips[] = {
    "Choose the starting configuration",
    "Choose the integration method",
    "Choose how much time each step advances",
    "Options for the random-cloud simulation",
    "Options for the selected simulation"};
/// @brief Preset menu names, indexed by enum value (= Simulation row).
constexpr const char* kPresetLabels[] = {
    "two-body", "solar-system", "random-cloud", "figure-eight"};

/// @brief The preset's enum value as an int, for table lookups.
/// @param preset The preset to index.
/// @return 0-3 for a valid preset, -1 otherwise.
int presetIndex(core::PresetName preset) {
    switch (preset) {
        case core::PresetName::TwoBody:
            return 0;
        case core::PresetName::SolarSystem:
            return 1;
        case core::PresetName::RandomCloud:
            return 2;
        case core::PresetName::FigureEight:
            return 3;
    }
    return -1;
}

}  // namespace

bool MenuRect::contains(core::Vec2 p) const {
    return p.x >= topLeft.x && p.x < topLeft.x + size.x &&
           p.y >= topLeft.y && p.y < topLeft.y + size.y;
}

int menuPresetOptionCount(core::PresetName preset) {
    // Two-body = separation + path; solar-system = planets + pluto;
    // random-cloud = none (its options live in the Cloud group);
    // figure-eight = separation (scale) only.
    switch (preset) {
        case core::PresetName::TwoBody:
        case core::PresetName::SolarSystem:
            return 2;
        case core::PresetName::RandomCloud:
            return 0;
        case core::PresetName::FigureEight:
            return 1;
    }
    return 0;
}

bool menuGroupVisible(MenuGroup group, core::PresetName preset) {
    switch (group) {
        case MenuGroup::Simulation:
        case MenuGroup::Integrator:
        case MenuGroup::Physics:
            return true;
        case MenuGroup::Cloud:
            return preset == core::PresetName::RandomCloud;
        case MenuGroup::Options:
            // Complement of Cloud: the optioned presets show Options in
            // the same last slot; random-cloud shows Cloud instead.
            return menuPresetOptionCount(preset) > 0;
        case MenuGroup::Count_:
            return false;
    }
    return false;
}

int menuItemCount(MenuGroup group, core::PresetName preset) {
    if (!menuGroupVisible(group, preset)) {
        return 0;
    }
    switch (group) {
        case MenuGroup::Simulation:
            // 4 presets + separator + restart, identical for every
            // preset; the option rows belong to the Options group.
            return 6;
        case MenuGroup::Integrator:
            return 2;
        case MenuGroup::Physics:
            // dt list only: the separator and restart rows belong to
            // Simulation.
            return 5;
        case MenuGroup::Cloud:
            return 4;
        case MenuGroup::Options:
            return menuPresetOptionCount(preset);
        case MenuGroup::Count_:
            return 0;
    }
    return 0;
}

const char* menuOptionsHeaderLabel(core::PresetName preset) {
    switch (preset) {
        case core::PresetName::TwoBody:
            return "Two-body";
        case core::PresetName::SolarSystem:
            return "Solar";
        case core::PresetName::RandomCloud:
            return "";  // no Options header for random-cloud
        case core::PresetName::FigureEight:
            return "Fig-8";
    }
    return "";
}

double menuRowTextIndent(MenuGroup group, int index, core::PresetName preset) {
    // The ONE definition of the dropdown indent law (see MenuBar.hpp);
    // renderMenu consumes it, so layout and render cannot drift.
    constexpr double kPlainRowIndent = 10.0;
    constexpr double kMarkedRowIndent = 30.0;  // dot-at-10 + text-at-30
    if (index < 0 || index >= menuItemCount(group, preset)) {
        return kPlainRowIndent;  // hidden / out-of-range rows are defined
    }
    switch (group) {
        case MenuGroup::Simulation:
            // 0-3 preset rows are dot-style; the separator (4) and the
            // restart action (5) are plain.
            return index < 4 ? kMarkedRowIndent : kPlainRowIndent;
        case MenuGroup::Integrator:
        case MenuGroup::Physics:
            return kMarkedRowIndent;  // dot rows
        case MenuGroup::Cloud:
            // Field rows (even) keep the 30 px value indent; the roll
            // buttons (odd) are plain action rows.
            return (index % 2 == 0) ? kMarkedRowIndent : kPlainRowIndent;
        case MenuGroup::Options:
            // Field row + toggle row: both keep the value indent.
            return kMarkedRowIndent;
        case MenuGroup::Count_:
            return kPlainRowIndent;
    }
    return kPlainRowIndent;
}

MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup,
                      core::PresetName preset) {
    (void)winW;
    MenuLayout layout;

    // Cloud and Options are mutually exclusive and last in enum order,
    // so exactly four headers land at identical x positions for every
    // preset (hidden groups take no bar space: no tab jumps).
    double x = kHeaderX;
    for (std::size_t group = 0;
         group < static_cast<std::size_t>(MenuGroup::Count_); ++group) {
        const auto groupEnum = static_cast<MenuGroup>(group);
        if (!menuGroupVisible(groupEnum, preset)) {
            continue;
        }
        layout.headers.push_back(
            {{x, kHeaderY}, {kHeaderWidths[group], kHeaderHeight}});
        layout.headerGroups.push_back(groupEnum);
        x += kHeaderWidths[group] + kHeaderGap;
    }

    if (!openGroup ||
        static_cast<unsigned int>(*openGroup) >=
            static_cast<unsigned int>(MenuGroup::Count_) ||
        !menuGroupVisible(*openGroup, preset)) {
        // Hidden groups are invisible to the layout and the hit test.
        return layout;
    }
    layout.openGroup = openGroup;

    const int count = menuItemCount(*openGroup, preset);
    const double height = static_cast<double>(count) * kItemHeight;
    // Vertical placement: hang below the bar at kDropdownY; when the
    // dropdown would run past the window bottom, flip it upward so its
    // bottom edge rests kDropdownBottom above the window bottom.
    const double y = winH - kDropdownY < height
                         ? kDropdownBottom - height
                         : kDropdownY;
    // Header positions must never be mapped to the enum index: find the
    // open group's own header rect.
    double itemX = kHeaderX;
    double headerWidth = kMinimumItemWidth;
    for (std::size_t index = 0; index < layout.headerGroups.size(); ++index) {
        if (layout.headerGroups[index] == *openGroup) {
            itemX = layout.headers[index].topLeft.x;
            headerWidth = layout.headers[index].size.x;
        }
    }
    const double width =
        headerWidth > kMinimumItemWidth ? headerWidth : kMinimumItemWidth;
    layout.items.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const MenuRect item{
            {itemX, y + static_cast<double>(index) * kItemHeight},
            {width, kItemHeight}};
        layout.items.push_back(item);
        if (*openGroup == MenuGroup::Simulation && index == 4) {
            layout.disabledRows.push_back(item);  // presets / restart
        }
    }

    return layout;
}

MenuHit menuHitTest(const MenuLayout& layout, core::Vec2 p) {
    for (std::size_t index = 0; index < layout.headers.size(); ++index) {
        if (layout.headers[index].contains(p)) {
            return {MenuHitKind::Header, layout.headerGroups[index], -1};
        }
    }

    if (!layout.openGroup) {
        return {};
    }
    for (std::size_t index = 0; index < layout.items.size(); ++index) {
        if (layout.items[index].contains(p)) {
            return {MenuHitKind::Item, *layout.openGroup,
                    static_cast<int>(index)};
        }
    }
    return {};
}

const char* menuGroupLabel(MenuGroup group) {
    const auto index = static_cast<std::size_t>(group);
    return index < kGroupCount ? kGroupLabels[index] : "";
}

const char* menuHeaderTooltip(MenuGroup group) {
    const auto index = static_cast<std::size_t>(group);
    return index < kGroupCount ? kHeaderTooltips[index] : "";
}

const char* menuPresetName(core::PresetName preset) {
    const int index = presetIndex(preset);
    return index >= 0 ? kPresetLabels[index] : "";
}

}  // namespace nbody::app
