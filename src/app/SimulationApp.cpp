#include "nbody/app/SimulationApp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
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

}  // namespace

namespace nbody::app {

SimulationApp::SimulationApp(core::SimulationSetup setup,
                             core::IntegratorKind integrator,
                             std::string baseTitle)
    : world_{setup.config, core::makeIntegrator(integrator)}
    , presetBodies_(setup.bodies)  // copy: world_.reset() consumes below
    , baseTitle_(std::move(baseTitle))
    , window_{sf::VideoMode(sf::Vector2u{800U, 600U}), baseTitle_}
    , font_{loadHudFont()} {
    world_.reset(std::move(setup.bodies));
    fitCamera();
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
                window_.close();
            } else if (key->code == sf::Keyboard::Key::Space) {
                paused_ = !paused_;  // rendering continues while paused
            } else if (key->code == sf::Keyboard::Key::R) {
                world_.reset(presetBodies_);
                accumulator_ = 0.0;
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
        } else if (press != nullptr &&
                   press->button == sf::Mouse::Button::Left) {
            dragging_ = true;
            lastMouse_ = press->position;
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
    window_.setTitle(baseTitle_ + statsSuffix_);
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

    if (font_.has_value()) {  // HUD on screen too (belt over the title bar)
        sf::Text hud{*font_};  // SFML 3: ctor takes the Font, no default ctor
        hud.setFont(*font_);
        hud.setCharacterSize(14U);
        hud.setFillColor(kHudColor);
        hud.setString(baseTitle_ + statsSuffix_);
        hud.setPosition(sf::Vector2f{8.0F, 4.0F});
        window_.draw(hud);
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
