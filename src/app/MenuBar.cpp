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
// values: the Cloud seed readout (19 chars) and the Options preset
// readout (planets + pluto, 12 chars); the separation readout is 7.
constexpr double kLabelAdvance = 7.0;
constexpr double kValueExtents[] = {0.0, 0.0, 0.0, 99.0, 80.0};
constexpr const char* kGroupLabels[] = {"Simulation", "Integrator",
                                        "Physics", "Cloud", "Options"};
constexpr std::size_t kGroupCount =
    sizeof(kGroupLabels) / sizeof(kGroupLabels[0]);
static_assert(kGroupCount == static_cast<std::size_t>(MenuGroup::Count_),
              "one label per menu group");

constexpr double labelExtent(const char* label) {
    double width = 0.0;
    for (const char* p = label; *p != '\0'; ++p) {
        width += kLabelAdvance;
    }
    return width;
}

constexpr double headerWidthFor(std::size_t group) {
    const double width =
        labelExtent(kGroupLabels[group]) + kValueExtents[group] +
        kHeaderPadding;
    return width > kMinimumHeaderWidth ? width : kMinimumHeaderWidth;
}

// -> {90, 90, 90, 150, 145}: Cloud keeps its 150, Options lands at 145.
constexpr double kHeaderWidths[] = {headerWidthFor(0), headerWidthFor(1),
                                    headerWidthFor(2), headerWidthFor(3),
                                    headerWidthFor(4)};
constexpr const char* kHeaderTooltips[] = {
    "Choose the starting configuration", "", "",
    "Options for the random-cloud simulation",
    ""};  // Options header tooltip lands with the tooltips task
constexpr const char* kPresetLabels[] = {
    "two-body", "solar-system", "random-cloud", "figure-eight"};

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
    // Spec D.1/D.2: two-body = separation + path; solar-system = planets +
    // pluto; random-cloud = none (its options live in the Cloud group);
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
            // preset; the option rows live in the Options group now.
            return 6;
        case MenuGroup::Integrator:
            return 2;
        case MenuGroup::Physics:
            // dt list only: the separator + restart rows moved to
            // Simulation (7 -> 5).
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
