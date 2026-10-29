#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nbody/app/Camera.hpp"
#include "nbody/core/Body.hpp"
#include "nbody/core/PhysicsWorld.hpp"
#include "nbody/core/Presets.hpp"

namespace nbody::app {

// Interactive SFML 3 application (plan D): fixed-timestep physics loop,
// camera pan/zoom, HUD in the title bar and (when a font is loadable) drawn
// on screen. All SFML event handling uses the SFML 3 std::optional<pollEvent
// + getIf<T>() style; waitEvent/event.type are defects per plan.
//
// The accumulator math itself is the pure planSteps() in StepPlanning.hpp so
// it can be unit-tested display-free.
class SimulationApp {
public:
    // Windowed branch entry point (main.cpp, after the display probe passes).
    // `baseTitle` is the "<preset> | <integrator>" prefix; run() appends the
    // live FPS/UPS/body-count/time stats to it.
    SimulationApp(core::SimulationSetup setup, core::IntegratorKind integrator,
                  std::string baseTitle);

    SimulationApp(const SimulationApp&) = delete;
    SimulationApp& operator=(const SimulationApp&) = delete;

    int run();  // returns process exit code (0)

private:
    void handleEvents();
    void update(sf::Time frameTime);
    void render();
    void updateStats();

    core::PhysicsWorld world_;
    std::vector<core::Body> presetBodies_;  // stored for the R reset key
    std::string baseTitle_;
    Camera camera_;
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    std::optional<sf::Font> font_;  // nullopt -> HUD text skipped, title only
    bool paused_ = false;
    bool dragging_ = false;
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
