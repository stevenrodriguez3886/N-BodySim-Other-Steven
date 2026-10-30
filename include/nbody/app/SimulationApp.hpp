#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nbody/app/Camera.hpp"
#include "nbody/app/MenuBar.hpp"
#include "nbody/app/MenuModel.hpp"
#include "nbody/app/Starfield.hpp"
#include "nbody/core/Body.hpp"
#include "nbody/core/PhysicsWorld.hpp"
#include "nbody/core/Presets.hpp"

namespace nbody::app {

// Interactive SFML 3 application (plan D): fixed-timestep physics loop,
// camera pan/zoom and stats in the title bar only (the on-screen HUD
// duplicate was dropped in the visual overhaul, spec D.3; a loadable font
// is still used for menu labels and the staged chip). All SFML event
// handling uses the SFML 3 std::optional<pollEvent
// + getIf<T>() style; waitEvent/event.type are defects per plan.
//
// The accumulator math itself is the pure planSteps() in StepPlanning.hpp so
// it can be unit-tested display-free.
class SimulationApp {
public:
    // Windowed branch entry point (main.cpp, after the display probe passes).
    SimulationApp(core::SimulationSetup setup, core::PresetName preset,
                  std::uint32_t seed, core::IntegratorKind integrator,
                  std::string_view title);

    SimulationApp(const SimulationApp&) = delete;
    SimulationApp& operator=(const SimulationApp&) = delete;

    int run();  // returns process exit code (0)

private:
    void handleEvents();
    void handleMenuHit(const MenuHit& hit, core::Vec2 position);
    void handleAction(const MenuAction& action);
    void commitSeedEdit();
    void restartScene();
    void rebuildScene(bool refitCamera);
    void update(sf::Time frameTime);
    void render();
    void updateStats();

    core::PhysicsWorld world_;
    MenuState menuState_;
    Camera camera_;
    std::vector<Star> stars_{starfield()};  // world-locked, generated once
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    std::optional<sf::Font> font_;  // nullopt -> HUD text skipped, title only
    bool paused_ = false;
    bool dragging_ = false;
    // Inline typed seed field of the Seed dropdown (spec D.4): focused by
    // clicking its single row. Digits only (max 10), Enter commits through
    // the staging law, Esc cancels the edit only, click-away cancels then
    // processes. The caret blink reuses statsClock_'s 0.5 s period.
    bool seedEditing_ = false;
    std::string seedBuffer_;
    sf::Vector2i lastMouse_{0, 0};
    double accumulator_ = 0.0;

    // 0.5 s stats window (plan D.2): frames counted here, steps via
    // world_.totalSteps() deltas.
    sf::Clock statsClock_;
    int framesInWindow_ = 0;
    std::uint64_t lastTotalSteps_ = 0;
    std::string statsSuffix_;  // " | FPS xx | UPS xxx | N bodies | t=ss.s"

    void fitCamera();  // initial zoom to ~80% of bodies' bounding radius
};

}  // namespace nbody::app
