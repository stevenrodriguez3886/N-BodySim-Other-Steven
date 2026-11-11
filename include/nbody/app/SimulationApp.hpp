/// @file
/// Declares SimulationApp, the interactive SFML 3 frontend for the n-body
/// core: a fixed-timestep physics loop, an in-window menu bar with inline
/// text fields, camera pan/zoom, an energy-error HUD, title-bar stats and
/// hover tooltips.

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

/// @brief The interactive application window and its main loop.
///
/// Runs a fixed-timestep physics loop: each frame's wall time is
/// accumulated and consumed in whole physics steps, with clamps so a
/// hang never turns into a catch-up burst. Camera pan (left-drag on the
/// scene) and zoom (mouse wheel) navigate the view; FPS/UPS/body/time
/// stats live in the window title bar, while the energy-error HUD, menu
/// labels and staged chip render in-window.
///
/// Menu/field interaction: a top bar of headers opens dropdowns of
/// actions, toggles and inline text fields sharing one editor. Option
/// changes apply live while running; while paused they land in staged
/// mirrors and fold into the live simulation on the next Space resume,
/// in a fixed consumption order (preset, integrator, dt, seed, then the
/// option tail).
///
/// Hover tooltips on headers and rows appear after a dwell and draw
/// last, above every other overlay including the staged chip.
///
/// @note All SFML event handling uses the SFML 3 std::optional pollEvent
/// + getIf<T>() style; waitEvent and the SFML 2 event.type tags are
/// defects in this codebase.
/// @note The accumulator math itself is the pure planSteps() in
/// StepPlanning.hpp so it can be unit-tested display-free.
class SimulationApp {
public:
    /// @brief Windowed branch entry point (main.cpp, after the display
    /// probe passes).
    /// @param setup Initial world config and bodies for the chosen scene.
    /// @param preset Preset the menu bar starts on.
    /// @param seed Full signed int64 seed for the scene generators.
    /// @param bodies Body count seeding the random-cloud option so an R
    /// restart keeps the CLI --bodies value (default 150).
    /// @param integrator Integrator the simulation starts with.
    /// @param title Window title; stats are appended to it at runtime.
    SimulationApp(core::SimulationSetup setup, core::PresetName preset,
                  std::int64_t seed, std::uint32_t bodies,
                  core::IntegratorKind integrator, std::string_view title);

    SimulationApp(const SimulationApp&) = delete;
    SimulationApp& operator=(const SimulationApp&) = delete;

    /// @brief Runs the main loop (events, update, render, display) until
    /// the window closes.
    /// @return The process exit code (0).
    int run();

private:
    /// @brief Polls and handles every pending window event.
    /// While a field is focused, TextEntered characters go to the editor
    /// first and KeyPressed covers Enter/Escape/Backspace/Delete/caret
    /// keys/Ctrl+C/X/V (all other keys are swallowed). Otherwise Escape
    /// closes the dropdown or quits, Space toggles pause (applying staged
    /// changes on resume), R restarts, and mouse events drive wheel zoom,
    /// menu hits, camera drags and field caret placement.
    /// @note TextEntered vs KeyPressed delivery order varies by platform
    /// (X11 delivers KeyPressed first); correctness is order-independent
    /// because the two editing branches consume disjoint inputs.
    /// @note On Space-resume, staged values are consumed in a fixed
    /// order: preset first (its rebuild already sees the folded-in option
    /// mirrors), then integrator, dt, seed, then the option tail.
    void handleEvents();
    /// @brief Routes a hit-tested header or item to its behavior.
    /// @param hit The header or item that was clicked.
    /// @param position Window-pixel point, used to reject disabled rows.
    /// Headers toggle their dropdown; field rows focus the inline editor;
    /// Cloud roll buttons roll fresh random values through the same
    /// setters as typed fields; remaining items activate as actions.
    void handleMenuHit(const MenuHit& hit, core::Vec2 position);
    /// @brief Applies a menu action and executes the returned effect.
    /// @param action The group and row index that was activated.
    void handleAction(const MenuAction& action);
    /// @brief Executes one effect from the menu model's action table.
    /// @param effect The rebuild/reconfigure action to perform.
    /// @note Rebuild law: live option effects rebuild immediately with a
    /// camera refit, except Cloud (seed/bodies) edits on random-cloud,
    /// which rebuild without refitting. Staged and None do nothing here.
    void applyEffect(MenuEffect effect);
    /// @brief Focuses the shared inline editor on a field row.
    /// @param field The field row that was clicked.
    /// The buffer is prefilled with the field's current value, with caret
    /// and selection anchor at the end; the dropdown stays open.
    void openFieldEditor(TextField field);
    /// @brief Commits the focused field's buffer through its setter.
    /// The same staging law as every other group applies (live when
    /// running, staged when paused); then the dropdown closes.
    void commitFieldEdit();
    /// @brief Handles Ctrl+C/X/V while a field is focused.
    /// @param key The control-combo key event to interpret.
    /// Copy takes the selection (whole buffer if none), cut deletes what
    /// it copied, paste runs the clipboard through the field's grammar at
    /// the caret. Any other Ctrl combo is swallowed without typing.
    void handleFieldClipboard(const sf::Event::KeyPressed& key);
    /// @brief R/restart: rebuilds from the full current option set and
    /// refits the camera.
    void restartScene();
    /// @brief Rebuilds the world via the per-preset builders.
    /// @param refitCamera Whether to refit the zoom to the new bodies.
    /// Uses the full current option set (not createPreset's defaults) and
    /// zeroes the step accumulator.
    void rebuildScene(bool refitCamera);
    /// @brief Applies the shared SeedChanged rule (see .cpp).
    /// Random-cloud regenerates immediately with the camera preserved;
    /// other presets just store the new seed for a future cloud load.
    void applySeedEffect();
    /// @brief Consumes one frame's time into whole physics steps.
    /// @param frameTime Wall-clock time since the previous frame.
    /// @note Frame time and per-frame step count are clamped, so a hang
    /// never becomes a catch-up burst; pausing drains the accumulator
    /// without stepping the world.
    void update(sf::Time frameTime);
    /// @brief Draws one frame: starfield, bodies, menu bar and dropdown,
    /// staged chip; tooltips draw last via renderTooltip().
    void render();
    /// @brief Draws the hover tooltip box at the cursor, above the
    /// staged chip.
    void renderTooltip();
    /// @brief Re-resolves the header or row under the cursor.
    /// A target change restarts the dwell clock; hovering never mutates
    /// menu state.
    void updateTooltipTarget();
    /// @brief Clears the tooltip (the click/scroll/menu-close hide rules).
    void hideTooltip();
    /// @brief Recomputes FPS/UPS over the stats window and refreshes the
    /// window title.
    void updateStats();

    core::PhysicsWorld world_;  ///< live world: bodies, integrator, dt, step/time counters
    MenuState menuState_;  ///< menu options (live values + staged mirrors), open dropdown
    Camera camera_;        ///< world<->pixel transform (center + scale)
    sf::RenderWindow window_;  ///< the single application window
    sf::Clock frameClock_;  ///< measures the wall time between frames
    std::optional<sf::Font> font_;  ///< nullopt -> in-window text skipped, title only
    bool paused_ = false;   ///< Space toggles; rendering continues while paused
    bool dragging_ = false;  ///< left button held on the scene: camera panning
    // One parameterized inline text editor shared by every field row:
    // nullopt = no field focused. Enter commits through the staging law,
    // Esc cancels the edit only, click-away cancels then processes,
    // Ctrl+C/X/V go to the clipboard (paste grammar-filtered). The caret
    // blink reuses statsClock_'s 0.5 s period; the buffer carries a real
    // caret + selection anchor: clicks inside the row place the caret,
    // held drags extend the selection (fieldSelecting_), and Ctrl+C/X/V
    // use the FieldText clipboard laws.
    std::optional<TextField> activeField_;  ///< nullopt = no field focused
    FieldText fieldText_;  ///< editor buffer with caret + selection anchor
    bool fieldSelecting_ = false;  ///< left button held inside the field row
    // Pixels per character inside the focused field: the '0' glyph
    // advance at the 13 px type size, measured at editor open (digits
    // dominate these fields; a deliberate approximation for click/drag
    // math and the selection-band/caret positions).
    double fieldAdvance_ = 0.0;
    double fieldDragLeft_ = 0.0;   ///< focused row's text-start x (drag map)
    double fieldDragWidth_ = 0.0;  ///< fieldAdvance_ * buffer size at press
    // Double-press detector (SFML 3.0 has no double-click event): a left
    // press within 250 ms AND 4 px of the previous left press selects all.
    sf::Clock fieldPressClock_;  ///< time since the previous left press
    sf::Vector2i fieldLastPressPos_{-1000000, -1000000};  ///< previous press spot (px)
    // Hover tooltip state: the target (header or open-group row under
    // the cursor) is re-resolved on every MouseMoved; the dwell clock
    // restarts whenever the target changes. Empty text = nothing to show.
    MenuHit tooltipTarget_;   ///< header/row last resolved under the cursor
    std::string tooltipText_;  ///< empty = nothing to show
    sf::Clock tooltipClock_;  ///< dwell timer; restarted on target change
    sf::Vector2i lastMouse_{0, 0};  ///< cursor px; tooltip anchors at cursor+(12,18)
    double accumulator_ = 0.0;  ///< frame time (s) not yet spent on whole steps

    // 0.5 s stats window: frames counted here, steps via
    // world_.totalSteps() deltas.
    sf::Clock statsClock_;  ///< restarted at every stats refresh
    int framesInWindow_ = 0;  ///< frames drawn in the current stats window
    std::uint64_t lastTotalSteps_ = 0;  ///< totalSteps() at the last refresh
    std::string statsSuffix_;  // " | FPS xx | UPS xxx | N bodies | t=ss.s"
    double initialEnergy_ = 0.0;  ///< total energy at the current scene's start

    /// @brief Sets the zoom so ~80% of the bodies' bounding radius is
    /// visible, centred on the origin.
    void fitCamera();
};

}  // namespace nbody::app
