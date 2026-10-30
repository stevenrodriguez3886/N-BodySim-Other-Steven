#include "nbody/app/MenuBar.hpp"

namespace nbody::app {
namespace {

constexpr double kHeaderX = 10.0;
constexpr double kHeaderY = 6.0;
constexpr double kHeaderHeight = 28.0;
constexpr double kHeaderGap = 6.0;
constexpr double kDropdownY = 38.0;
constexpr double kDropdownBottom = 6.0;
constexpr double kItemHeight = 24.0;
constexpr double kMinimumItemWidth = 140.0;
constexpr double kHeaderWidths[] = {150.0, 130.0, 150.0, 110.0};
constexpr const char* kGroupLabels[] = {"Simulation", "Integrator",
                                        "Physics", "Cloud"};
constexpr const char* kHeaderTooltips[] = {
    "Choose the starting configuration", "", "",
    "Options for the random-cloud simulation"};
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
            // 4 presets + separator + options + separator + restart (D.1).
            return 7 + menuPresetOptionCount(preset);
        case MenuGroup::Integrator:
            return 2;
        case MenuGroup::Physics:
            // dt list only: the separator + restart rows moved to
            // Simulation (7 -> 5).
            return 5;
        case MenuGroup::Cloud:
            return 4;
        case MenuGroup::Count_:
            return 0;
    }
    return 0;
}

MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup,
                      core::PresetName preset) {
    (void)winW;
    MenuLayout layout;

    double x = kHeaderX;
    for (std::size_t group = 0; group < 4; ++group) {
        const auto groupEnum = static_cast<MenuGroup>(group);
        const double width = kHeaderWidths[group];
        if (menuGroupVisible(groupEnum, preset)) {
            layout.headers.push_back({{x, kHeaderY}, {width, kHeaderHeight}});
            layout.headerGroups.push_back(groupEnum);
        }
        x += width + kHeaderGap;
    }

    if (!openGroup ||
        static_cast<unsigned int>(*openGroup) >=
            static_cast<unsigned int>(MenuGroup::Count_) ||
        !menuGroupVisible(*openGroup, preset)) {
        // Hidden groups are invisible to the layout and the hit test.
        return layout;
    }
    layout.openGroup = openGroup;

    const auto groupIndex = static_cast<std::size_t>(*openGroup);
    const int count = menuItemCount(*openGroup, preset);
    const double height = static_cast<double>(count) * kItemHeight;
    const double y = winH - kDropdownY < height
                         ? kDropdownBottom - height
                         : kDropdownY;
    const double width = kHeaderWidths[groupIndex] > kMinimumItemWidth
                             ? kHeaderWidths[groupIndex]
                             : kMinimumItemWidth;
    const double itemX = layout.headers[groupIndex].topLeft.x;
    layout.items.reserve(static_cast<std::size_t>(count));

    const int separatorLast =
        *openGroup == MenuGroup::Simulation
            ? 5 + menuPresetOptionCount(preset)
            : -1;
    for (int index = 0; index < count; ++index) {
        const MenuRect item{
            {itemX, y + static_cast<double>(index) * kItemHeight},
            {width, kItemHeight}};
        layout.items.push_back(item);
        if (*openGroup == MenuGroup::Simulation &&
            (index == 4 || index == separatorLast)) {
            layout.disabledRows.push_back(item);
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
    return index < 4 ? kGroupLabels[index] : "";
}

const char* menuHeaderTooltip(MenuGroup group) {
    const auto index = static_cast<std::size_t>(group);
    return index < 4 ? kHeaderTooltips[index] : "";
}

const char* menuPresetName(core::PresetName preset) {
    const int index = presetIndex(preset);
    return index >= 0 ? kPresetLabels[index] : "";
}

}  // namespace nbody::app
