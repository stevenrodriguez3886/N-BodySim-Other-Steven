#include "nbody/app/MenuBar.hpp"
#include "nbody/app/MenuModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using nbody::app::MenuGroup;
using nbody::app::MenuHitKind;
using nbody::app::MenuState;
using nbody::app::menuGroupLabel;
using nbody::app::menuHeaderTooltip;
using nbody::app::menuHitTest;
using nbody::app::menuItemCount;
using nbody::app::menuItemLabel;
using nbody::app::menuItemTooltip;
using nbody::app::menuLayout;
using nbody::app::menuPresetOptionCount;
using nbody::core::PresetName;

namespace {

constexpr MenuGroup kAllGroups[] = {MenuGroup::Simulation,
                                    MenuGroup::Integrator,
                                    MenuGroup::Physics, MenuGroup::Cloud};
constexpr PresetName kAllPresets[] = {
    PresetName::TwoBody, PresetName::SolarSystem, PresetName::RandomCloud,
    PresetName::FigureEight};

}  // namespace

TEST_CASE("menu_header_geometry") {
    // The Cloud header exists only while the random-cloud preset is
    // selected (spec D.1); other presets lay out three headers.
    const auto hidden = menuLayout(800.0, 600.0, std::nullopt,
                                   PresetName::TwoBody);
    REQUIRE(hidden.headers.size() == 3);

    const auto layout = menuLayout(800.0, 600.0, std::nullopt,
                                   PresetName::RandomCloud);
    REQUIRE(layout.headers.size() == 4);

    const double expectedX[] = {10.0, 166.0, 302.0, 458.0};
    const double expectedWidth[] = {150.0, 130.0, 150.0, 110.0};
    for (std::size_t index = 0; index < layout.headers.size(); ++index) {
        REQUIRE(layout.headers[index].topLeft.x == expectedX[index]);
        REQUIRE(layout.headers[index].topLeft.y == 6.0);
        REQUIRE(layout.headers[index].size.x == expectedWidth[index]);
        REQUIRE(layout.headers[index].size.y == 28.0);
    }
}

TEST_CASE("menu_hit_header_and_item") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Physics,
                                   PresetName::TwoBody);
    const auto& physicsHeader = layout.headers[2];
    const auto headerHit = menuHitTest(
        layout, {physicsHeader.topLeft.x + physicsHeader.size.x / 2.0,
                 physicsHeader.topLeft.y + physicsHeader.size.y / 2.0});
    REQUIRE(headerHit.kind == MenuHitKind::Header);
    REQUIRE(headerHit.group == MenuGroup::Physics);
    REQUIRE(headerHit.index == -1);

    const auto& item = layout.items[2];
    const auto itemHit = menuHitTest(
        layout, {item.topLeft.x + item.size.x / 2.0,
                 item.topLeft.y + item.size.y / 2.0});
    REQUIRE(itemHit.kind == MenuHitKind::Item);
    REQUIRE(itemHit.group == MenuGroup::Physics);
    REQUIRE(itemHit.index == 2);
    REQUIRE(item.size.x == 150.0);
    REQUIRE(item.size.y == 24.0);

    const auto cloudLayout = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                        PresetName::RandomCloud);
    REQUIRE(cloudLayout.items[0].size.x == 140.0);

    const auto outsideHit = menuHitTest(
        layout, {physicsHeader.topLeft.x - 1.0, physicsHeader.topLeft.y});
    REQUIRE(outsideHit.kind == MenuHitKind::None);

    const auto outsideItemHit = menuHitTest(
        layout, {item.topLeft.x - 1.0, item.topLeft.y + item.size.y / 2.0});
    REQUIRE(outsideItemHit.kind == MenuHitKind::None);
}

TEST_CASE("menu_closed_layout_has_no_items") {
    const auto layout = menuLayout(800.0, 600.0, std::nullopt,
                                   PresetName::RandomCloud);
    REQUIRE(layout.openGroup == std::nullopt);
    REQUIRE(layout.items.empty());
    REQUIRE(layout.disabledRows.empty());
}

TEST_CASE("menu_narrow_window_clips_headers") {
    const auto layout = menuLayout(300.0, 600.0, std::nullopt,
                                   PresetName::RandomCloud);
    REQUIRE(layout.headers.size() == 4);
    REQUIRE(layout.headers[3].topLeft.x + layout.headers[3].size.x > 300.0);
}

TEST_CASE("menu_short_window_flips_items_upward") {
    const auto layout = menuLayout(800.0, 140.0, MenuGroup::Physics,
                                   PresetName::TwoBody);
    REQUIRE(layout.items.size() == 5);  // dt list only: restart moved out
    constexpr double flippedBottom = 6.0;
    constexpr double rowHeight = 24.0;
    for (std::size_t index = 0; index < layout.items.size(); ++index) {
        const auto& item = layout.items[index];
        REQUIRE(item.size.y == rowHeight);
        REQUIRE(item.topLeft.y ==
                flippedBottom -
                    static_cast<double>(layout.items.size() - index) *
                        rowHeight);
    }
    const auto& lastItem = layout.items.back();
    REQUIRE(lastItem.topLeft.y + lastItem.size.y == flippedBottom);
}

TEST_CASE("menu_group_labels_and_pure_counts") {
    REQUIRE(menuGroupLabel(MenuGroup::Simulation) == std::string("Simulation"));
    REQUIRE(menuGroupLabel(MenuGroup::Integrator) == std::string("Integrator"));
    REQUIRE(menuGroupLabel(MenuGroup::Physics) == std::string("Physics"));
    REQUIRE(menuGroupLabel(MenuGroup::Cloud) == std::string("Cloud"));

    REQUIRE(menuItemCount(MenuGroup::Simulation, PresetName::TwoBody) == 9);
    REQUIRE(menuItemCount(MenuGroup::Simulation, PresetName::SolarSystem) ==
            9);
    REQUIRE(menuItemCount(MenuGroup::Simulation, PresetName::RandomCloud) ==
            7);
    REQUIRE(menuItemCount(MenuGroup::Simulation, PresetName::FigureEight) ==
            8);
    REQUIRE(menuItemCount(MenuGroup::Integrator, PresetName::TwoBody) == 2);
    REQUIRE(menuItemCount(MenuGroup::Physics, PresetName::TwoBody) == 5);
    REQUIRE(menuItemCount(MenuGroup::Cloud, PresetName::TwoBody) == 0);
    REQUIRE(menuItemCount(MenuGroup::Cloud, PresetName::RandomCloud) == 4);

    REQUIRE(menuItemLabel(MenuState{}, MenuGroup::Simulation, 8) ==
            "restart scene");
    REQUIRE(menuItemLabel(MenuState{}, MenuGroup::Physics, 0) == "1/30");
    REQUIRE(menuItemLabel(MenuState{}, MenuGroup::Integrator, 1) == "euler");
}

TEST_CASE("menu_cloud_hidden_layout_and_hit") {
    MenuState state;  // preset two-body: the Cloud group is hidden.
    const auto hidden = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                   state.preset);
    REQUIRE(hidden.headers.size() == 3);  // no Cloud header at all
    REQUIRE(hidden.items.empty());
    // The spot where the Cloud header would sit hits nothing.
    const auto miss = menuHitTest(hidden, {460.0, 20.0});
    REQUIRE(miss.kind == MenuHitKind::None);
    // And even with a stale openGroup the layout never exposes rows.
    const auto missItems = menuHitTest(hidden, {460.0, 60.0});
    REQUIRE(missItems.kind == MenuHitKind::None);

    state.preset = PresetName::RandomCloud;
    const auto shown = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                  state.preset);
    REQUIRE(shown.headers.size() == 4);
    REQUIRE(shown.items.size() == 4);
    const auto& row = shown.items[0];
    const auto rowHit = menuHitTest(
        shown, {row.topLeft.x + row.size.x / 2.0,
                row.topLeft.y + row.size.y / 2.0});
    REQUIRE(rowHit.kind == MenuHitKind::Item);
    REQUIRE(rowHit.group == MenuGroup::Cloud);
    REQUIRE(rowHit.index == 0);
}

TEST_CASE("menu_simulation_layout_rows_and_separators") {
    // Layout item counts must agree with menuItemCount for every preset,
    // with separators at rows 4 and 5+options.
    for (PresetName preset : kAllPresets) {
        MenuState state;
        state.preset = preset;
        const int opts = menuPresetOptionCount(preset);
        const auto layout = menuLayout(800.0, 600.0, MenuGroup::Simulation,
                                       preset);
        REQUIRE(layout.items.size() ==
                static_cast<std::size_t>(
                    menuItemCount(MenuGroup::Simulation, state)));
        REQUIRE(layout.items.size() == static_cast<std::size_t>(7 + opts));
        REQUIRE(layout.disabledRows.size() == 2);
        REQUIRE(layout.disabledRows[0].topLeft.y ==
                layout.items[4].topLeft.y);
        REQUIRE(layout.disabledRows[1].topLeft.y ==
                layout.items[5 + opts].topLeft.y);

        const auto& separator = layout.items[4];
        const auto hit = menuHitTest(
            layout, {separator.topLeft.x + separator.size.x / 2.0,
                     separator.topLeft.y + separator.size.y / 2.0});
        REQUIRE(hit.kind == MenuHitKind::Item);
        REQUIRE(hit.group == MenuGroup::Simulation);
        REQUIRE(hit.index == 4);
    }
}

TEST_CASE("menu_simulation_labels_are_state_aware") {
    MenuState state;  // two-body
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 0) == "two-body");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 4).empty());
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5) ==
            "separation: 10");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6) == "path: orbit");
    state.hyperbolic = true;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6) ==
            "path: hyperbola");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 7).empty());
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 8) == "restart scene");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 9).empty());
    state.separation = 10.5;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5) ==
            "separation: 10.5");

    state = MenuState{};
    state.preset = PresetName::SolarSystem;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5) == "planets: 8");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6) == "pluto: ✗");
    state.pluto = true;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6) == "pluto: ✓");
    state.planetCount = 5;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5) == "planets: 5");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 8) == "restart scene");

    state = MenuState{};
    state.preset = PresetName::RandomCloud;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5).empty());
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6) == "restart scene");

    state = MenuState{};
    state.preset = PresetName::FigureEight;
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 5) ==
            "separation: 10");
    REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 7) == "restart scene");
}

TEST_CASE("menu_cloud_group_rows") {
    MenuState state;
    state.preset = PresetName::RandomCloud;
    REQUIRE(menuItemCount(MenuGroup::Cloud, state) == 4);
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 0) == "type a seed");
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 1) == "random seed");
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 2) == "bodies: 150");
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 3) == "random bodies");
    state.bodies = 300;
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 2) == "bodies: 300");
    REQUIRE(menuItemLabel(state, MenuGroup::Cloud, 4).empty());

    // The seed field keeps its row-0 hit semantics from the old Seed group.
    const auto closed = menuLayout(800.0, 600.0, std::nullopt,
                                   PresetName::RandomCloud);
    const auto& cloudHeader = closed.headers[3];
    const auto headerHit = menuHitTest(
        closed, {cloudHeader.topLeft.x + cloudHeader.size.x / 2.0,
                 cloudHeader.topLeft.y + cloudHeader.size.y / 2.0});
    REQUIRE(headerHit.kind == MenuHitKind::Header);
    REQUIRE(headerHit.group == MenuGroup::Cloud);
    REQUIRE(headerHit.index == -1);
}

TEST_CASE("menu_tooltips_header_table") {
    REQUIRE(menuHeaderTooltip(MenuGroup::Simulation) ==
            std::string("Choose the starting configuration"));
    REQUIRE(menuHeaderTooltip(MenuGroup::Cloud) ==
            std::string("Options for the random-cloud simulation"));
    // Physics/Integrator headers keep today's (empty) tooltips.
    REQUIRE(menuHeaderTooltip(MenuGroup::Physics) == std::string(""));
    REQUIRE(menuHeaderTooltip(MenuGroup::Integrator) == std::string(""));
}

TEST_CASE("menu_tooltips_row_table_verbatim") {
    MenuState state;  // two-body
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 5, state) ==
            std::string("Distance between the bodies at t=0"));
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 6, state) ==
            std::string("Circular orbit or unbound fly-by"));
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 8, state) ==
            std::string("Rebuild the scene from the current settings"));

    state.preset = PresetName::SolarSystem;
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 5, state) ==
            std::string("How many planets (inside-out)"));
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 6, state) ==
            std::string("Include Pluto at ~200 units"));
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 8, state) ==
            std::string("Rebuild the scene from the current settings"));

    state.preset = PresetName::RandomCloud;
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 6, state) ==
            std::string("Rebuild the scene from the current settings"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 0, state) ==
            std::string("Type a seed value, press Enter"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 1, state) ==
            std::string("Use a fresh random seed"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 2, state) ==
            std::string("Number of bodies (2-2000)"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 3, state) ==
            std::string("Use a random body count (2-2000)"));

    state.preset = PresetName::FigureEight;
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 5, state) ==
            std::string("Distance between the bodies at t=0"));
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 7, state) ==
            std::string("Rebuild the scene from the current settings"));
}

TEST_CASE("menu_tooltips_complete_over_every_visible_row") {
    // Every visible (group, row) has either a documented string or a
    // documented empty entry; nothing falls outside the table. dt and
    // integrator rows have none (spec D.6).
    const std::set<std::string> documented = {
        "",
        "Choose the starting configuration",
        "Options for the random-cloud simulation",
        "Rebuild the scene from the current settings",
        "Distance between the bodies at t=0",
        "Circular orbit or unbound fly-by",
        "How many planets (inside-out)",
        "Include Pluto at ~200 units",
        "Type a seed value, press Enter",
        "Use a fresh random seed",
        "Number of bodies (2-2000)",
        "Use a random body count (2-2000)"};

    for (PresetName preset : kAllPresets) {
        MenuState state;
        state.preset = preset;
        for (MenuGroup group : kAllGroups) {
            const int count = menuItemCount(group, state);
            for (int row = 0; row < count; ++row) {
                const std::string tip = menuItemTooltip(group, row, state);
                INFO("preset=" << static_cast<int>(preset)
                               << " group=" << static_cast<int>(group)
                               << " row=" << row);
                REQUIRE(documented.count(tip) == 1);
            }
        }
        // Preset/dt/integrator action rows deliberately have no tooltips.
        for (int row = 0; row < 4; ++row) {
            REQUIRE(menuItemTooltip(MenuGroup::Simulation, row, state)
                        .empty());
        }
        for (int row = 0; row < 2; ++row) {
            REQUIRE(
                menuItemTooltip(MenuGroup::Integrator, row, state).empty());
        }
        for (int row = 0; row < 5; ++row) {
            REQUIRE(menuItemTooltip(MenuGroup::Physics, row, state).empty());
        }
        // Separators are empty.
        REQUIRE(menuItemTooltip(MenuGroup::Simulation, 4, state).empty());
        REQUIRE(menuItemTooltip(MenuGroup::Simulation,
                                5 + menuPresetOptionCount(preset), state)
                    .empty());
    }
}

TEST_CASE("menu_hit_disabled_row") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Simulation,
                                   PresetName::TwoBody);
    REQUIRE(layout.disabledRows.size() == 2);
    const auto& separator = layout.disabledRows[0];
    const auto hit = menuHitTest(
        layout, {separator.topLeft.x + separator.size.x / 2.0,
                 separator.topLeft.y + separator.size.y / 2.0});
    REQUIRE(hit.kind == MenuHitKind::Item);
    REQUIRE(hit.group == MenuGroup::Simulation);
    REQUIRE(hit.index == 4);
}
