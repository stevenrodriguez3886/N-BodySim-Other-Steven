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
constexpr const char* kPresetLabels[] = {
    "two-body", "solar-system", "random-cloud", "figure-eight"};
constexpr const char* kIntegratorLabels[] = {"rk4", "euler"};
constexpr const char* kPhysicsLabels[] = {
    "1/30", "1/60", "1/120", "1/240", "1/480", "", "restart scene"};

bool validGroup(MenuGroup group) {
    return static_cast<unsigned int>(group) <
           static_cast<unsigned int>(MenuGroup::Count_);
}

}  // namespace

bool MenuRect::contains(core::Vec2 p) const {
    return p.x >= topLeft.x && p.x < topLeft.x + size.x &&
           p.y >= topLeft.y && p.y < topLeft.y + size.y;
}

MenuLayout menuLayout(double winW, double winH,
                      std::optional<MenuGroup> openGroup) {
    (void)winW;
    MenuLayout layout;
    layout.openGroup = openGroup;
    layout.headers.reserve(4);

    double x = kHeaderX;
    for (const double width : kHeaderWidths) {
        layout.headers.push_back({{x, kHeaderY}, {width, kHeaderHeight}});
        x += width + kHeaderGap;
    }

    if (!openGroup || !validGroup(*openGroup)) {
        return layout;
    }

    const auto groupIndex = static_cast<std::size_t>(*openGroup);
    const int count = menuItemCount(*openGroup);
    const double height = static_cast<double>(count) * kItemHeight;
    const double y = winH - kDropdownY < height
                         ? kDropdownBottom - height
                         : kDropdownY;
    const double width = kHeaderWidths[groupIndex] > kMinimumItemWidth
                             ? kHeaderWidths[groupIndex]
                             : kMinimumItemWidth;
    const double itemX = layout.headers[groupIndex].topLeft.x;
    layout.items.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const MenuRect item{{itemX, y + static_cast<double>(index) * kItemHeight},
                            {width, kItemHeight}};
        layout.items.push_back(item);
        if (*openGroup == MenuGroup::Physics && index == 5) {
            layout.disabledRows.push_back(item);
        }
    }

    return layout;
}

MenuHit menuHitTest(const MenuLayout& layout, core::Vec2 p) {
    for (std::size_t index = 0; index < layout.headers.size(); ++index) {
        if (layout.headers[index].contains(p)) {
            return {MenuHitKind::Header, static_cast<MenuGroup>(index), -1};
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

int menuItemCount(MenuGroup group) {
    switch (group) {
        case MenuGroup::Preset:
            return 4;
        case MenuGroup::Integrator:
            return 2;
        case MenuGroup::Physics:
            return 7;
        case MenuGroup::Seed:
            return 2;
        case MenuGroup::Count_:
            return 0;
    }
    return 0;
}

const char* menuGroupLabel(MenuGroup group) {
    switch (group) {
        case MenuGroup::Preset:
            return "Preset";
        case MenuGroup::Integrator:
            return "Integrator";
        case MenuGroup::Physics:
            return "Physics";
        case MenuGroup::Seed:
            return "Random seed";
        case MenuGroup::Count_:
            return "";
    }
    return "";
}

std::string menuItemLabel(MenuGroup group, int index, std::uint32_t) {
    if (index < 0 || index >= menuItemCount(group)) {
        return {};
    }
    switch (group) {
        case MenuGroup::Preset:
            return kPresetLabels[index];
        case MenuGroup::Integrator:
            return kIntegratorLabels[index];
        case MenuGroup::Physics:
            return kPhysicsLabels[index];
        case MenuGroup::Seed:
            return index == 0 ? "- 1" : "+ 1";
        case MenuGroup::Count_:
            return {};
    }
    return {};
}

}  // namespace nbody::app
