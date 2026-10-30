#include "nbody/app/SimulationApp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "nbody/app/StepPlanning.hpp"

namespace {

// The single core::Vec2 -> sf::Vector2f conversion point (plan B.3 note:
// deliberately NOT in Camera.hpp, which stays SFML-free).
sf::Vector2f toSf(const nbody::core::Vec2& v) {
    return sf::Vector2f{static_cast<float>(v.x), static_cast<float>(v.y)};
}

// Fixed 8-color body palette, indexed body.id % 8 (plan D.3).
constexpr sf::Color kBodyPalette[8]{
    sf::Color(255, 200, 80),  sf::Color(120, 200, 255),
    sf::Color(255, 130, 90),  sf::Color(150, 255, 160),
    sf::Color(220, 140, 255), sf::Color(255, 255, 255),
    sf::Color(255, 100, 160), sf::Color(100, 255, 220),
};

constexpr sf::Color kBackground(10, 12, 24);
constexpr sf::Color kHudColor(220, 220, 230);
constexpr sf::Color kMenuDimColor(168, 172, 190);      // inactive header/item text
constexpr sf::Color kMenuAccentColor(120, 190, 255);   // open tab underline
constexpr sf::Color kMenuSelectColor(150, 230, 170);   // current item + tick
constexpr sf::Color kMenuPanelColor(22, 25, 38, 242);
constexpr sf::Color kMenuBorderColor(70, 78, 100);
constexpr double kMenuStripBottom = 34.0;

// SFML 3.0.x has NO Font::getDefault() (that is a later 3.1/master API;
// verified against build/_deps/sfml-src/include/SFML/Graphics/Font.hpp — the
// only statics there are glyph/texture accessors). So the plan's "belt"
// applies: try a few well-known system font files; if none loads, HUD text
// is skipped and stats stay title-bar-only.
std::optional<sf::Font> loadHudFont() {
    static const char* kCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans-CondBold.ttf",
    };
    for (const char* path : kCandidates) {
        sf::Font font;
        if (font.openFromFile(std::filesystem::path(path))) {
            return font;
        }
    }
    return std::nullopt;
}

std::string formatStats(double fps, double ups, std::size_t bodies,
                        double simTime) {
    std::ostringstream out;
    out << " | FPS " << static_cast<int>(fps) << " | UPS "
        << static_cast<int>(ups) << " | " << bodies << " bodies | t="
        << std::fixed;
    out.precision(1);
    out << simTime;
    return out.str();
}

std::string menuValue(const nbody::app::MenuState& state,
                      nbody::app::MenuGroup group) {
    using nbody::app::MenuGroup;
    if (group == MenuGroup::Seed) {
        return std::to_string(state.seed);
    }
    if (group == MenuGroup::Physics) {
        const int ticked = nbody::app::menuTickedIndex(state, group);
        if (ticked >= 0) {
            return nbody::app::menuItemLabel(group, ticked, state.seed);
        }
        std::ostringstream out;
        out << std::setprecision(12) << state.dt;
        return out.str();
    }
    const int ticked = nbody::app::menuTickedIndex(state, group);
    return nbody::app::menuItemLabel(group, ticked, state.seed);
}

std::string liveTitle(const nbody::app::MenuState& state) {
    using nbody::app::MenuGroup;
    return nbody::app::menuItemLabel(
               MenuGroup::Preset,
               nbody::app::menuTickedIndex(state, MenuGroup::Preset),
               state.seed) +
           " | " +
           nbody::app::menuItemLabel(
               MenuGroup::Integrator,
               nbody::app::menuTickedIndex(state, MenuGroup::Integrator),
               state.seed);
}

}  // namespace

namespace nbody::app {

SimulationApp::SimulationApp(core::SimulationSetup setup,
                             core::PresetName preset, std::uint32_t seed,
                             core::IntegratorKind integrator,
                             std::string_view title)
    : world_{setup.config, core::makeIntegrator(integrator)}
    , window_{sf::VideoMode(sf::Vector2u{800U, 600U}), std::string(title)}
    , font_{loadHudFont()} {
    menuState_.preset = preset;
    menuState_.integrator = integrator;
    menuState_.seed = seed;
    menuState_.dt = setup.config.timeStep;
    world_.reset(std::move(setup.bodies));
    fitCamera();
    updateStats();  // seed the title-bar stats so the HUD isn't blank pre-0.5 s
}

void SimulationApp::rebuildScene(bool refitCamera) {
    core::SimulationSetup setup =
        core::createPreset(menuState_.preset, menuState_.seed);
    world_.setGravity(setup.config.g, setup.config.softening);
    world_.reset(std::move(setup.bodies));
    world_.setTimeStep(menuState_.dt);
    accumulator_ = 0.0;
    if (refitCamera) {
        fitCamera();
    }
}

void SimulationApp::restartScene() {
    rebuildScene(true);
}

void SimulationApp::handleAction(const MenuAction& action) {
    switch (menuApply(menuState_, action, paused_)) {
        case MenuEffect::PresetChanged:
            rebuildScene(true);
            break;
        case MenuEffect::IntegratorChanged:
            world_.setIntegrator(core::makeIntegrator(menuState_.integrator));
            break;
        case MenuEffect::DtChanged:
            world_.setTimeStep(menuState_.dt);
            break;
        case MenuEffect::SeedChanged:
            if (menuState_.preset == core::PresetName::RandomCloud) {
                rebuildScene(false);
            }
            break;
        case MenuEffect::SceneRestart:
            restartScene();
            break;
        case MenuEffect::Staged:
        case MenuEffect::None:
            break;
    }
}

void SimulationApp::handleMenuHit(const MenuHit& hit, core::Vec2 position) {
    if (hit.kind == MenuHitKind::Header) {
        if (menuState_.openGroup == hit.group) {
            menuState_.openGroup.reset();
        } else {
            menuState_.openGroup = hit.group;
        }
        return;
    }
    if (hit.kind != MenuHitKind::Item) {
        return;
    }

    const sf::Vector2u size = window_.getSize();
    const MenuLayout layout = menuLayout(
        static_cast<double>(size.x), static_cast<double>(size.y),
        menuState_.openGroup);
    for (const MenuRect& row : layout.disabledRows) {
        if (row.contains(position)) {
            return;
        }
    }

    menuState_.openGroup.reset();
    handleAction({hit.group, hit.index});
}

void SimulationApp::fitCamera() {
    const sf::Vector2u size = window_.getSize();
    const double minDim = std::min(static_cast<double>(size.x),
                                   static_cast<double>(size.y));
    double extent = 0.0;  // max bounding radius over all bodies
    for (const core::Body& b : world_.bodies()) {
        const double r =
            std::hypot(b.position.x, b.position.y) + b.radius;
        extent = std::max(extent, r);
    }
    // ~80% of the bodies' bounding radius visible. Guard extent == 0 (all
    // bodies collapsed at the origin) against the division.
    camera_.scale = extent > 0.0 ? 0.4 * minDim / extent : 50.0;
    camera_.center = core::Vec2{0.0, 0.0};
}

void SimulationApp::handleEvents() {
    // SFML 3 event style: pollEvent yields std::optional<sf::Event>; subtypes
    // via is<T>()/getIf<T>(). (The SFML 2 polling/enum-tag idioms would be a
    // defect per plan Review Focus #1.)
    while (const std::optional<sf::Event> ev = window_.pollEvent()) {
        if (ev->is<sf::Event::Closed>()) {
            window_.close();
            continue;
        }
        if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                if (menuState_.openGroup) {
                    menuState_.openGroup.reset();
                } else {
                    window_.close();
                }
            } else if (key->code == sf::Keyboard::Key::Space) {
                if (paused_) {
                    const MenuStaged staged = menuTakeStaged(menuState_);
                    if (staged.preset) {
                        menuState_.preset = *staged.preset;
                        rebuildScene(true);
                    }
                    if (staged.integrator) {
                        menuState_.integrator = *staged.integrator;
                        world_.setIntegrator(
                            core::makeIntegrator(menuState_.integrator));
                    }
                    // Apply dt after a preset rebuild so the staged value wins.
                    if (staged.dt) {
                        menuState_.dt = *staged.dt;
                        world_.setTimeStep(menuState_.dt);
                    }
                    if (staged.seed) {
                        menuState_.seed = *staged.seed;
                        if (menuState_.preset == core::PresetName::RandomCloud) {
                            rebuildScene(false);
                        }
                    }
                }
                paused_ = !paused_;  // rendering continues while paused
            } else if (key->code == sf::Keyboard::Key::R) {
                restartScene();
            }
            continue;
        }
        const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>();
        const auto* press = ev->getIf<sf::Event::MouseButtonPressed>();
        const auto* release = ev->getIf<sf::Event::MouseButtonReleased>();
        const auto* move = ev->getIf<sf::Event::MouseMoved>();
        const sf::Vector2u size = window_.getSize();
        if (wheel != nullptr) {
            camera_.zoomAt(core::Vec2{static_cast<double>(wheel->position.x),
                                      static_cast<double>(wheel->position.y)},
                           static_cast<double>(wheel->delta),
                           static_cast<double>(size.x),
                           static_cast<double>(size.y));
        } else if (press != nullptr) {
            const core::Vec2 position{static_cast<double>(press->position.x),
                                      static_cast<double>(press->position.y)};
            const MenuLayout layout = menuLayout(
                static_cast<double>(size.x), static_cast<double>(size.y),
                menuState_.openGroup);
            const MenuHit hit = menuHitTest(layout, position);
            if (press->button == sf::Mouse::Button::Left &&
                hit.kind != MenuHitKind::None) {
                handleMenuHit(hit, position);
            } else {
                menuState_.openGroup.reset();
                if (press->button == sf::Mouse::Button::Left &&
                    position.y >= kMenuStripBottom) {
                    dragging_ = true;
                    lastMouse_ = press->position;
                }
            }
        } else if (release != nullptr &&
                   release->button == sf::Mouse::Button::Left) {
            dragging_ = false;
        } else if (move != nullptr) {
            if (dragging_) {
                camera_.panByPixels(
                    core::Vec2{static_cast<double>(move->position.x -
                                                   lastMouse_.x),
                               static_cast<double>(move->position.y -
                                                   lastMouse_.y)});
            }
            lastMouse_ = move->position;
        }
    }
}

void SimulationApp::update(sf::Time frameTime) {
    // Fixed-timestep loop (plan D.2): the frame time is clamped by
    // kMaxFrameTime (0.25 s) and at most kMaxStepsFrame (30) steps run per
    // frame, with the leftover accumulator dropped — a hang never turns into
    // a catch-up burst. Pause drains the accumulator but skips world_.step().
    const StepPlan plan =
        planSteps(accumulator_, frameTime.asSeconds(), world_.timeStep());
    if (!paused_) {
        for (int i = 0; i < plan.steps; ++i) {
            world_.step();
        }
    }
    accumulator_ = plan.remainder;

    ++framesInWindow_;
    if (statsClock_.getElapsedTime() >= sf::seconds(0.5)) {
        updateStats();
    }
}

void SimulationApp::updateStats() {
    const double secs = statsClock_.restart().asSeconds();
    const std::uint64_t steps = world_.totalSteps();
    const double ups = secs > 0.0
                           ? static_cast<double>(steps - lastTotalSteps_) / secs
                           : 0.0;
    const double fps = secs > 0.0 ? framesInWindow_ / secs : 0.0;
    lastTotalSteps_ = steps;
    framesInWindow_ = 0;
    statsSuffix_ = formatStats(fps, ups, world_.bodies().size(),
                               world_.simulationTime());
    window_.setTitle(liveTitle(menuState_) + statsSuffix_);
}

void SimulationApp::render() {
    const sf::Vector2u size = window_.getSize();  // live, handles resize
    const double w = static_cast<double>(size.x);
    const double h = static_cast<double>(size.y);

    window_.clear(kBackground);
    for (const core::Body& b : world_.bodies()) {
        sf::CircleShape body;
        const double radiusPx = b.radius * camera_.scale;
        body.setRadius(static_cast<float>(radiusPx));
        body.setOrigin(sf::Vector2f{static_cast<float>(radiusPx),
                                    static_cast<float>(radiusPx)});  // center
        body.setFillColor(kBodyPalette[b.id % 8U]);
        body.setPosition(toSf(camera_.toPixels(b.position, w, h)));
        window_.draw(body);
    }

    if (font_.has_value()) {  // HUD on screen too (belt over the title bar).
        // Drawn before the menu so an open dropdown overlays it cleanly.
        sf::Text hud{*font_};  // SFML 3: ctor takes the Font, no default ctor
        hud.setCharacterSize(14U);
        hud.setFillColor(kHudColor);
        hud.setString(liveTitle(menuState_) + statsSuffix_);
        hud.setPosition(sf::Vector2f{8.0F, 40.0F});
        window_.draw(hud);
    }

    const MenuLayout layout = menuLayout(w, h, menuState_.openGroup);
    sf::RectangleShape menuBackground{sf::Vector2f{static_cast<float>(w), 28.0F}};
    menuBackground.setFillColor(sf::Color(10, 12, 24, 200));
    window_.draw(menuBackground);

    if (layout.openGroup && !layout.items.empty()) {
        const MenuRect& first = layout.items.front();
        const float rowHeight = static_cast<float>(first.size.y);
        const float panelHeight = rowHeight *
                                  static_cast<float>(layout.items.size());
        const float panelWidth = static_cast<float>(first.size.x);
        sf::RectangleShape panel{
            sf::Vector2f{panelWidth, panelHeight}};
        panel.setPosition(sf::Vector2f{static_cast<float>(first.topLeft.x),
                                       static_cast<float>(first.topLeft.y)});
        panel.setFillColor(kMenuPanelColor);
        panel.setOutlineThickness(1.0F);
        panel.setOutlineColor(kMenuBorderColor);
        window_.draw(panel);
    }

    if (font_.has_value()) {
        for (std::size_t index = 0; index < layout.headers.size(); ++index) {
            const MenuGroup group = static_cast<MenuGroup>(index);
            const MenuRect& rect = layout.headers[index];
            const bool open = menuState_.openGroup == group;

            sf::Text label{*font_};
            label.setCharacterSize(12U);
            label.setFillColor(open ? kHudColor : kMenuDimColor);
            label.setString(std::string(menuGroupLabel(group)) + ": " +
                            menuValue(menuState_, group));
            label.setPosition(sf::Vector2f{
                static_cast<float>(rect.topLeft.x + 7.0),
                static_cast<float>(rect.topLeft.y + 7.0)});
            window_.draw(label);

            if (open) {  // tab indicator: accent underline, no filled box
                sf::RectangleShape underline{sf::Vector2f{
                    static_cast<float>(rect.size.x - 14.0), 2.0F}};
                underline.setPosition(sf::Vector2f{
                    static_cast<float>(rect.topLeft.x + 7.0),
                    static_cast<float>(rect.topLeft.y) + 18.0F});
                underline.setFillColor(kMenuAccentColor);
                window_.draw(underline);
            }
        }

        if (layout.openGroup) {
            const int ticked = menuTickedIndex(menuState_, *layout.openGroup);
            for (std::size_t index = 0; index < layout.items.size(); ++index) {
                const MenuRect& rect = layout.items[index];
                const bool disabled = std::any_of(
                    layout.disabledRows.begin(), layout.disabledRows.end(),
                    [&rect](const MenuRect& row) {
                        return row.topLeft.y == rect.topLeft.y;
                    });
                if (disabled) {
                    sf::RectangleShape separator{sf::Vector2f{
                        static_cast<float>(rect.size.x - 16.0), 1.0F}};
                    separator.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 8.0),
                        static_cast<float>(rect.topLeft.y +
                                           rect.size.y / 2.0)});
                    separator.setFillColor(sf::Color(105, 110, 125));
                    window_.draw(separator);
                    continue;
                }

                const std::string item = menuItemLabel(
                    *layout.openGroup, static_cast<int>(index), menuState_.seed);
                const bool selected = static_cast<int>(index) == ticked;
                sf::Text label{*font_};
                label.setCharacterSize(13U);
                label.setFillColor(selected ? kHudColor : kMenuDimColor);
                label.setString(item);
                label.setPosition(sf::Vector2f{
                    static_cast<float>(rect.topLeft.x + 30.0),
                    static_cast<float>(rect.topLeft.y + 3.0)});
                window_.draw(label);

                if (selected) {  // the one selection indicator: tick + tint
                    sf::Text tick{*font_};
                    tick.setCharacterSize(13U);
                    tick.setFillColor(kMenuSelectColor);
                    tick.setString("\xE2\x9C\x93");
                    tick.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 8.0),
                        static_cast<float>(rect.topLeft.y + 3.0)});
                    window_.draw(tick);
                }
            }
        }

        if (paused_ && menuIsStagedActive(menuState_)) {
            sf::Text chip{*font_};
            chip.setCharacterSize(12U);
            chip.setFillColor(kHudColor);
            chip.setString(menuStagedSummary(menuState_));
            const float chipWidth = chip.getLocalBounds().size.x + 16.0F;
            const float chipX = std::max(
                8.0F, static_cast<float>(size.x) - chipWidth - 8.0F);
            sf::RectangleShape chipBackground{
                sf::Vector2f{chipWidth, 24.0F}};
            chipBackground.setPosition(sf::Vector2f{chipX, 4.0F});
            chipBackground.setFillColor(sf::Color(40, 46, 66, 235));
            window_.draw(chipBackground);
            chip.setPosition(sf::Vector2f{chipX + 8.0F, 6.0F});
            window_.draw(chip);
        }
    }
}

int SimulationApp::run() {
    while (window_.isOpen()) {
        handleEvents();
        update(frameClock_.restart());
        render();
        window_.display();
    }
    return 0;
}

}  // namespace nbody::app
