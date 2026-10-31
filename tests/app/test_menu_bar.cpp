#include "nbody/app/MenuBar.hpp"
#include "nbody/app/MenuModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

using nbody::app::MenuGroup;
using nbody::app::MenuHitKind;
using nbody::app::MenuState;
using nbody::app::menuGroupLabel;
using nbody::app::menuGroupVisible;
using nbody::app::menuHeaderTooltip;
using nbody::app::menuHitTest;
using nbody::app::menuItemCount;
using nbody::app::menuItemLabel;
using nbody::app::menuItemTooltip;
using nbody::app::menuLayout;
using nbody::app::menuOptionsHeaderLabel;
using nbody::app::menuPresetName;
using nbody::app::menuPresetOptionCount;
using nbody::core::PresetName;

namespace {

constexpr MenuGroup kAllGroups[] = {MenuGroup::Simulation,
                                    MenuGroup::Integrator,
                                    MenuGroup::Physics, MenuGroup::Cloud,
                                    MenuGroup::Options};
constexpr PresetName kAllPresets[] = {
    PresetName::TwoBody, PresetName::SolarSystem, PresetName::RandomCloud,
    PresetName::FigureEight};

template <typename T>
bool contains(const std::vector<T>& values, T value) {
    for (const T& item : values) {
        if (item == value) {
            return true;
        }
    }
    return false;
}

// Content-sized header table (pure MenuBar layer): minimum 90 px, 7 px
// per label character + fixed value extents + 16 px padding ->
// widths {90, 90, 90, 150, 145}; gap 8 -> x = {10, 108, 206, 304}.
constexpr double kHeaderXs[] = {10.0, 108.0, 206.0, 304.0};

}  // namespace

// ---------------------------------------------------------------------------
// New-layout structure tests (menu-bar rework task 1).
// ---------------------------------------------------------------------------

TEST_CASE("group visibility: Options is the complement of Cloud") {
    for (auto p : kAllPresets) {
        CHECK(menuGroupVisible(MenuGroup::Options, p) ==
              !menuGroupVisible(MenuGroup::Cloud, p));
        CHECK(menuGroupVisible(MenuGroup::Options, p) ==
              (menuPresetOptionCount(p) > 0));
    }
}

TEST_CASE("Simulation menu is presets+separator+restart for every preset") {
    for (auto p : kAllPresets) {
        CHECK(menuItemCount(MenuGroup::Simulation, p) == 6);
    }
}

TEST_CASE("Options item counts follow the preset") {
    CHECK(menuItemCount(MenuGroup::Options, PresetName::TwoBody) == 2);
    CHECK(menuItemCount(MenuGroup::Options, PresetName::SolarSystem) == 2);
    CHECK(menuItemCount(MenuGroup::Options, PresetName::FigureEight) == 1);
    CHECK(menuItemCount(MenuGroup::Options, PresetName::RandomCloud) == 0);
}

TEST_CASE("layout: only one of Cloud/Options heads the last slot, never a "
          "6th header") {
    for (auto p : kAllPresets) {
        auto layout = menuLayout(800, 600, std::nullopt, p);
        CHECK(layout.headers.size() == 4);  // always exactly four headers
        CHECK_FALSE(layout.headerGroups.empty());
        CHECK(layout.headerGroups.back() != MenuGroup::Count_);
        bool hasCloud = contains(layout.headerGroups, MenuGroup::Cloud);
        bool hasOpts = contains(layout.headerGroups, MenuGroup::Options);
        CHECK(hasCloud != hasOpts);
    }
}

TEST_CASE("layout: no tab jumps when the preset changes") {
    // Same header x-positions across presets: Options and Cloud occupy
    // the same last slot.
    auto a = menuLayout(800, 600, std::nullopt, PresetName::RandomCloud);
    auto b = menuLayout(800, 600, std::nullopt, PresetName::SolarSystem);
    if (a.headers.size() < 4 || b.headers.size() < 4) {
        FAIL("layout must expose four headers");
    }
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(a.headers[i].topLeft.x == b.headers[i].topLeft.x);
    }
    for (auto p : kAllPresets) {
        auto c = menuLayout(800, 600, std::nullopt, p);
        for (std::size_t i = 0; i < 4 && i < c.headers.size(); ++i) {
            CHECK(c.headers[i].topLeft.x == kHeaderXs[i]);
        }
    }
}

TEST_CASE("Simulation separator row is index 4 only") {
    auto layout = menuLayout(800, 600, MenuGroup::Simulation,
                             PresetName::SolarSystem);
    CHECK(layout.disabledRows.size() == 1);
    auto opts = menuLayout(800, 600, MenuGroup::Options,
                           PresetName::SolarSystem);
    CHECK(opts.disabledRows.empty());  // Options dropdown: zero separators
}

TEST_CASE("Options header label follows the preset") {
    CHECK(menuOptionsHeaderLabel(PresetName::SolarSystem) ==
          std::string("Solar"));
    CHECK(menuOptionsHeaderLabel(PresetName::TwoBody) ==
          std::string("Two-body"));
    CHECK(menuOptionsHeaderLabel(PresetName::FigureEight) ==
          std::string("Fig-8"));
    CHECK(menuOptionsHeaderLabel(PresetName::RandomCloud) == std::string(""));
    // menuGroupLabel(Options) is only the plain fallback.
    CHECK(menuGroupLabel(MenuGroup::Options) == std::string("Options"));
}

TEST_CASE("menu_options_open_layout") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Options,
                                   PresetName::SolarSystem);
    REQUIRE(layout.openGroup == MenuGroup::Options);
    REQUIRE(layout.items.size() == 2);
    REQUIRE(layout.disabledRows.empty());
    REQUIRE(layout.items[0].topLeft.x == 304.0);  // last header slot
    REQUIRE(layout.items[0].size.x == 145.0);
    REQUIRE(layout.items[0].size.y == 24.0);

    // Hidden for random-cloud: a stale open group exposes nothing.
    const auto hidden = menuLayout(800.0, 600.0, MenuGroup::Options,
                                   PresetName::RandomCloud);
    REQUIRE(hidden.openGroup == std::nullopt);
    REQUIRE(hidden.items.empty());
    const auto miss = menuHitTest(hidden, {310.0, 60.0});
    REQUIRE(miss.kind == MenuHitKind::None);
}

// ---------------------------------------------------------------------------
// Pre-existing geometry/hit tests, rewritten for the new layout.
// ---------------------------------------------------------------------------

TEST_CASE("menu_header_geometry") {
    // Every preset lays out exactly four headers: Simulation, Integrator,
    // Physics plus ONE last-slot header (Cloud while random-cloud is
    // selected, Options for the optioned presets). Widths are
    // content-sized (min 90, label + value extents + 16 padding, gap 8).
    for (PresetName preset : kAllPresets) {
        const auto layout = menuLayout(800.0, 600.0, std::nullopt, preset);
        REQUIRE(layout.headers.size() == 4);
        for (std::size_t index = 0; index < 4; ++index) {
            INFO("preset=" << static_cast<int>(preset)
                           << " header=" << index);
            REQUIRE(layout.headers[index].topLeft.x == kHeaderXs[index]);
            REQUIRE(layout.headers[index].topLeft.y == 6.0);
            REQUIRE(layout.headers[index].size.y == 28.0);
        }
        REQUIRE(layout.headerGroups[0] == MenuGroup::Simulation);
        REQUIRE(layout.headerGroups[1] == MenuGroup::Integrator);
        REQUIRE(layout.headerGroups[2] == MenuGroup::Physics);
        REQUIRE(layout.headers[0].size.x == 90.0);
        REQUIRE(layout.headers[1].size.x == 90.0);
        REQUIRE(layout.headers[2].size.x == 90.0);
        if (layout.headerGroups[3] == MenuGroup::Cloud) {
            REQUIRE(layout.headers[3].size.x == 150.0);
        } else {
            REQUIRE(layout.headerGroups[3] == MenuGroup::Options);
            REQUIRE(layout.headers[3].size.x == 145.0);
        }
    }
}

TEST_CASE("menu_hit_header_and_item") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Physics,
                                   PresetName::TwoBody);
    const auto& physicsHeader = layout.headers[2];
    REQUIRE(physicsHeader.topLeft.x == 206.0);
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
    REQUIRE(item.topLeft.x == 206.0);
    REQUIRE(item.size.x == 140.0);  // max(min item width, physics header)
    REQUIRE(item.size.y == 24.0);

    const auto cloudLayout = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                        PresetName::RandomCloud);
    REQUIRE(cloudLayout.items[0].topLeft.x == 304.0);
    REQUIRE(cloudLayout.items[0].size.x == 150.0);  // = cloud header width

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
    REQUIRE(layout.items.size() == 5);  // dt list only
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
    REQUIRE(menuGroupLabel(MenuGroup::Options) == std::string("Options"));

    // Simulation is presets + separator + restart for EVERY preset now;
    // the option rows moved to the Options group.
    for (PresetName preset : kAllPresets) {
        REQUIRE(menuItemCount(MenuGroup::Simulation, preset) == 6);
        REQUIRE(menuItemCount(MenuGroup::Options, preset) ==
                menuPresetOptionCount(preset));
    }
    REQUIRE(menuItemCount(MenuGroup::Integrator, PresetName::TwoBody) == 2);
    REQUIRE(menuItemCount(MenuGroup::Physics, PresetName::TwoBody) == 5);
    REQUIRE(menuItemCount(MenuGroup::Cloud, PresetName::TwoBody) == 0);
    REQUIRE(menuItemCount(MenuGroup::Cloud, PresetName::RandomCloud) == 4);

    REQUIRE(menuItemLabel(MenuState{}, MenuGroup::Physics, 0) == "1/30");
    REQUIRE(menuItemLabel(MenuState{}, MenuGroup::Integrator, 1) == "euler");
}

TEST_CASE("menu_cloud_hidden_layout_and_hit") {
    MenuState state;  // preset two-body: the Cloud group is hidden.
    const auto hidden = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                   state.preset);
    // Four headers still: the last slot is held by Options, not Cloud.
    REQUIRE(hidden.headers.size() == 4);
    REQUIRE_FALSE(contains(hidden.headerGroups, MenuGroup::Cloud));
    REQUIRE(hidden.headerGroups[3] == MenuGroup::Options);
    REQUIRE(hidden.items.empty());
    // The last slot hits Options, never a hidden Cloud header.
    const auto slot = menuHitTest(hidden, {309.0, 20.0});
    REQUIRE(slot.kind == MenuHitKind::Header);
    REQUIRE(slot.group == MenuGroup::Options);
    // And even with a stale openGroup the layout never exposes rows.
    const auto missItems = menuHitTest(hidden, {309.0, 60.0});
    REQUIRE(missItems.kind == MenuHitKind::None);

    state.preset = PresetName::RandomCloud;
    const auto shown = menuLayout(800.0, 600.0, MenuGroup::Cloud,
                                  state.preset);
    REQUIRE(shown.headers.size() == 4);
    REQUIRE(shown.headerGroups[3] == MenuGroup::Cloud);
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
    // with the single separator at row 4 (options rows left Simulation).
    for (PresetName preset : kAllPresets) {
        const auto layout = menuLayout(800.0, 600.0, MenuGroup::Simulation,
                                       preset);
        REQUIRE(layout.items.size() == static_cast<std::size_t>(
                                            menuItemCount(
                                                MenuGroup::Simulation,
                                                preset)));
        REQUIRE(layout.items.size() == 6);
        REQUIRE(layout.disabledRows.size() == 1);
        REQUIRE(layout.disabledRows[0].topLeft.y ==
                layout.items[4].topLeft.y);

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
    // Simulation rows are 0-3 presets, 4 separator, 5 restart for every
    // preset. The option-row labels ("separation: ...", "planets: ...",
    // toggles) now live in the Options group; their state-aware mapping
    // is owned by the MenuModel tests (task 2 of the rework).
    for (PresetName preset : kAllPresets) {
        MenuState state;
        state.preset = preset;
        for (int row = 0; row < 4; ++row) {
            REQUIRE(menuItemLabel(state, MenuGroup::Simulation, row) ==
                    menuPresetName(static_cast<PresetName>(row)));
        }
        REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 4).empty());
        // Rows beyond the (now 6-row) menu read as empty via the count
        // guard.
        REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 6).empty());
        REQUIRE(menuItemLabel(state, MenuGroup::Simulation, 9).empty());
    }
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
    // Physics/Integrator headers keep today's (empty) tooltips; the
    // Options header tooltip arrives with the tooltips-everywhere task.
    REQUIRE(menuHeaderTooltip(MenuGroup::Physics) == std::string(""));
    REQUIRE(menuHeaderTooltip(MenuGroup::Integrator) == std::string(""));
}

TEST_CASE("menu_tooltips_row_table_verbatim") {
    // Cloud group row tooltips are untouched by the rework.
    MenuState state;
    state.preset = PresetName::RandomCloud;
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 0, state) ==
            std::string("Type a seed value, press Enter"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 1, state) ==
            std::string("Use a fresh random seed"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 2, state) ==
            std::string("Number of bodies (2-2000)"));
    REQUIRE(menuItemTooltip(MenuGroup::Cloud, 3, state) ==
            std::string("Use a random body count (2-2000)"));

    // Simulation rows 6+ are gone (options/restart rows remap in the
    // MenuModel tooltip task); out-of-range rows read empty through the
    // count guard, and the lone separator row keeps an empty tooltip.
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 4, state).empty());
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 6, state).empty());
    REQUIRE(menuItemTooltip(MenuGroup::Simulation, 8, state).empty());
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
        // The Simulation separator (row 4) is empty.
        REQUIRE(menuItemTooltip(MenuGroup::Simulation, 4, state).empty());
    }
}

TEST_CASE("menu_hit_disabled_row") {
    const auto layout = menuLayout(800.0, 600.0, MenuGroup::Simulation,
                                   PresetName::TwoBody);
    REQUIRE(layout.disabledRows.size() == 1);
    const auto& separator = layout.disabledRows[0];
    const auto hit = menuHitTest(
        layout, {separator.topLeft.x + separator.size.x / 2.0,
                 separator.topLeft.y + separator.size.y / 2.0});
    REQUIRE(hit.kind == MenuHitKind::Item);
    REQUIRE(hit.group == MenuGroup::Simulation);
    REQUIRE(hit.index == 4);
}
