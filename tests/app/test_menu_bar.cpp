#include "nbody/app/MenuBar.hpp"

#include <catch2/catch_test_macros.hpp>

using nbody::app::MenuGroup;
using nbody::app::MenuHitKind;
using nbody::app::menuGroupLabel;
using nbody::app::menuHitTest;
using nbody::app::menuItemCount;
using nbody::app::menuItemLabel;
using nbody::app::menuLayout;

TEST_CASE("menu_header_geometry") {
    const auto layout = menuLayout(800.0, 600.0, std::nullopt);
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
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Physics);
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

    const auto seedLayout = menuLayout(800.0, 600.0, MenuGroup::Seed);
    REQUIRE(seedLayout.items[0].size.x == 140.0);

    const auto outsideHit = menuHitTest(
        layout, {physicsHeader.topLeft.x - 1.0, physicsHeader.topLeft.y});
    REQUIRE(outsideHit.kind == MenuHitKind::None);

    const auto outsideItemHit = menuHitTest(
        layout, {item.topLeft.x - 1.0, item.topLeft.y + item.size.y / 2.0});
    REQUIRE(outsideItemHit.kind == MenuHitKind::None);
}

TEST_CASE("menu_closed_layout_has_no_items") {
    const auto layout = menuLayout(800.0, 600.0, std::nullopt);
    REQUIRE(layout.openGroup == std::nullopt);
    REQUIRE(layout.items.empty());
    REQUIRE(layout.disabledRows.empty());
}

TEST_CASE("menu_narrow_window_clips_headers") {
    const auto layout = menuLayout(300.0, 600.0, std::nullopt);
    REQUIRE(layout.headers.size() == 4);
    REQUIRE(layout.headers[3].topLeft.x + layout.headers[3].size.x > 300.0);
}

TEST_CASE("menu_short_window_flips_items_upward") {
    const auto layout = menuLayout(800.0, 140.0, MenuGroup::Physics);
    REQUIRE(layout.items.size() == 7);
    constexpr double flippedBottom = 6.0;
    constexpr double rowHeight = 24.0;
    for (std::size_t index = 0; index < layout.items.size(); ++index) {
        const auto& item = layout.items[index];
        REQUIRE(item.size.y == rowHeight);
        REQUIRE(item.topLeft.y ==
                flippedBottom -
                    static_cast<double>(layout.items.size() - index) * rowHeight);
    }
    const auto& lastItem = layout.items.back();
    REQUIRE(lastItem.topLeft.y + lastItem.size.y == flippedBottom);
}

TEST_CASE("menu_item_labels") {
    REQUIRE(menuGroupLabel(MenuGroup::Seed) == std::string("Random seed"));
    REQUIRE(menuItemCount(MenuGroup::Preset) == 4);
    REQUIRE(menuItemCount(MenuGroup::Integrator) == 2);
    REQUIRE(menuItemCount(MenuGroup::Physics) == 7);
    REQUIRE(menuItemCount(MenuGroup::Seed) == 2);
    REQUIRE(menuItemLabel(MenuGroup::Physics, 6, 42) == "restart scene");
    REQUIRE(menuItemLabel(MenuGroup::Seed, 0, 42) == "- 1");
    REQUIRE(menuItemLabel(MenuGroup::Preset, 1, 42) == "solar-system");
}

TEST_CASE("menu_hit_disabled_row") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Physics);
    REQUIRE(layout.disabledRows.size() == 1);
    const auto& separator = layout.disabledRows[0];
    const auto hit = menuHitTest(
        layout, {separator.topLeft.x + separator.size.x / 2.0,
                 separator.topLeft.y + separator.size.y / 2.0});
    REQUIRE(hit.kind == MenuHitKind::Item);
    REQUIRE(hit.group == MenuGroup::Physics);
    REQUIRE(hit.index == 5);
}
