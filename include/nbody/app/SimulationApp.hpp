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
#include "nbody/app/TextField.hpp"
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
    // Seed is the full signed int64 (spec D.3); bodies seeds the random-cloud
    // option so an R restart keeps --bodies (CLI default 150).
    SimulationApp(core::SimulationSetup setup, core::PresetName preset,
                  std::int64_t seed, std::uint32_t bodies,
                  core::IntegratorKind integrator, std::string_view title);

    SimulationApp(const SimulationApp&) = delete;
    SimulationApp& operator=(const SimulationApp&) = delete;

    int run();  // returns process exit code (0)

private:
    void handleEvents();
    void handleMenuHit(const MenuHit& hit, core::Vec2 position);
    void handleAction(const MenuAction& action);
    void applyEffect(MenuEffect effect);  // the D.2 effect-dispatch table
    void openFieldEditor(TextField field);
    void commitFieldEdit();
    void handleFieldClipboard(const sf::Event::KeyPressed& key);
    void restartScene();
    void rebuildScene(bool refitCamera);
    void applySeedEffect();  // the shared SeedChanged rule (see .cpp)
    void update(sf::Time frameTime);
    void render();
    void renderTooltip();  // drawn last, above the staged chip (D.6)
    void updateTooltipTarget();  // header/row under the cursor (D.6)
    void hideTooltip();          // click/scroll/menu-close hide rules (D.6)
    void updateStats();

    core::PhysicsWorld world_;
    MenuState menuState_;
    Camera camera_;
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    std::optional<sf::Font> font_;  // nullopt -> in-window text skipped, title only
    bool paused_ = false;
    bool dragging_ = false;
    // One parameterized inline text editor shared by every field row
    // (spec D.4): nullopt = no field focused. Enter commits through the
    // staging law, Esc cancels the edit only, click-away cancels then
    // processes, Ctrl+C/X/V go to the clipboard (paste grammar-filtered).
    // The caret blink reuses statsClock_'s 0.5 s period.
    std::optional<TextField> activeField_;
    std::string fieldBuffer_;
    // Hover tooltip state (spec D.6): target last resolved on MouseMoved
    // (header or open-group row under the cursor); the dwell clock restarts
    // whenever the target changes. Empty text = nothing to show.
    MenuHit tooltipTarget_;
    std::string tooltipText_;
    sf::Clock tooltipClock_;  // dwell timer; restarted on target change
    sf::Vector2i lastMouse_{0, 0};  // tooltip anchors at cursor+(12,18)
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
