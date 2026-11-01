/// @file
/// Implementation of SimulationApp: the fixed-timestep loop, the full
/// SFML 3 event routing (menus, inline field editing with clipboard
/// support, camera pan/zoom), the planet-style body renderer, and the
/// title-bar stats readout.

#include "nbody/app/SimulationApp.hpp"

#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <utility>

#include "nbody/app/StepPlanning.hpp"
#include "nbody/app/Trace.hpp"

namespace {

/// @brief The single core::Vec2 -> sf::Vector2f conversion point.
/// Deliberately lives here, not in Camera.hpp, which stays SFML-free.
/// @param v The world-space vector to convert.
/// @return The same coordinates as single-precision SFML coordinates.
sf::Vector2f toSf(const nbody::core::Vec2& v) {
    return sf::Vector2f{static_cast<float>(v.x), static_cast<float>(v.y)};
}

// Fixed 8-color body palette, indexed by body.id % 8.
constexpr sf::Color kBodyPalette[8]{
    sf::Color(255, 200, 80),  sf::Color(120, 200, 255),
    sf::Color(255, 130, 90),  sf::Color(150, 255, 160),
    sf::Color(220, 140, 255), sf::Color(255, 255, 255),
    sf::Color(255, 100, 160), sf::Color(100, 255, 220),
};

constexpr sf::Color kBackground(10, 12, 24);
constexpr sf::Color kHudColor(220, 220, 230);
constexpr sf::Color kMenuDimColor(168, 172, 190);      // inactive header/item text
constexpr sf::Color kMenuSelectColor(150, 230, 170);   // current-item dot
constexpr sf::Color kMenuPanelColor(22, 25, 38, 242);
constexpr sf::Color kMenuBorderColor(70, 78, 100);
constexpr sf::Color kMenuAccentColor(120, 190, 255);  // focused seed field border
constexpr sf::Color kFieldSelectionColor(58, 96, 168);  // field selection band
constexpr double kMenuStripBottom = 34.0;

/// @brief Clamps a value into [0, 1].
/// fieldIndexForFraction's input contract: the app clamps the pixel ratio
/// to [0, 1] before calling it (finite inputs by construction here).
/// @param value The value to clamp.
/// @return value moved into [0, 1].
double clampUnit(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

/// @brief Loads a UI font from well-known system font locations.
/// SFML 3.0.x has no Font::getDefault() (a later 3.1/master API; verified
/// against build/_deps/sfml-src/include/SFML/Graphics/Font.hpp — its only
/// statics are glyph/texture accessors), so a short candidate list is
/// tried in order.
/// @return The loaded font, or nullopt if none opens -- in-window text
/// (menu labels, chip, seed field) is then skipped and stats stay
/// title-bar-only.
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

/// @brief Formats the title-bar stats suffix:
/// " | FPS xx | UPS xxx | N bodies | t=ss.s".
/// @param fps Frames per second over the stats window.
/// @param ups physics updates per second over the same window.
/// @param bodies current body count.
/// @param simTime accumulated simulation seconds.
/// @return The stats text appended to the window title.
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

/// @brief Renders a value with the same shortest-honest-number rule as
/// MenuModel's option labels (integers bare, fractions at 4 significant
/// digits), so the Options header readout matches its dropdown rows.
/// @param value The number to render.
/// @return The display text.
std::string formatOptionValue(double value) {
    if (std::isfinite(value) && value == std::floor(value) &&
        std::fabs(value) < 1e15) {
        return std::to_string(static_cast<long long>(value));
    }
    std::ostringstream out;
    out << std::setprecision(4) << value;
    return out.str();
}

/// @brief The header value readout for a menu group (value-first, like
/// the Cloud seed readout).
/// @param state The live menu state the readout reflects.
/// @param group The header group being rendered.
/// @return The current value's display text for that group.
std::string menuValue(const nbody::app::MenuState& state,
                      nbody::app::MenuGroup group) {
    using nbody::app::MenuGroup;
    if (group == MenuGroup::Cloud) {
        return std::to_string(state.seed);
    }
    if (group == MenuGroup::Options) {
        // Live readout (value-first, like the Cloud seed readout):
        // solar shows the planet count plus the pluto toggle state;
        // two-body/fig-8 show the separation. Random-cloud has no
        // Options header at all.
        if (state.preset == nbody::core::PresetName::SolarSystem) {
            // Plain words only: the loaded system font has no ✓/✗/·
            // glyphs (live-verified: they render as '?').
            return std::to_string(state.planetCount) + " / pluto " +
                   (state.pluto ? "on" : "off");
        }
        return formatOptionValue(state.separation);
    }
    if (group == MenuGroup::Physics) {
        const int ticked = nbody::app::menuTickedIndex(state, group);
        if (ticked >= 0) {
            return nbody::app::menuItemLabel(state, group, ticked);
        }
        std::ostringstream out;
        out << std::setprecision(12) << state.dt;
        return out.str();
    }
    const int ticked = nbody::app::menuTickedIndex(state, group);
    return nbody::app::menuItemLabel(state, group, ticked);
}

/// @brief Builds the live title prefix: "<preset> | <integrator>".
/// @param state The live menu state (ticked rows name preset and integrator).
/// @return The title text before the stats suffix.
std::string liveTitle(const nbody::app::MenuState& state) {
    using nbody::app::MenuGroup;
    return nbody::app::menuItemLabel(
               state, MenuGroup::Simulation,
               nbody::app::menuTickedIndex(state, MenuGroup::Simulation)) +
           " | " +
           nbody::app::menuItemLabel(
               state, MenuGroup::Integrator,
               nbody::app::menuTickedIndex(state, MenuGroup::Integrator));
}

}  // namespace

namespace nbody::app {

SimulationApp::SimulationApp(core::SimulationSetup setup,
                             core::PresetName preset, std::int64_t seed,
                             std::uint32_t bodies,
                             core::IntegratorKind integrator,
                             std::string_view title)
    : world_{setup.config, core::makeIntegrator(integrator)}
    , window_{sf::VideoMode(sf::Vector2u{800U, 600U}), std::string(title)}
    , font_{loadHudFont()} {
    menuState_.preset = preset;
    menuState_.integrator = integrator;
    menuState_.seed = seed;
    // Seed the bodies option from the CLI (--bodies): an R restart rebuilds
    // through the options-aware builders, so the count survives restarts
    // and a windowed restart never drops --bodies.
    menuState_.bodies = bodies;
    menuState_.dt = setup.config.timeStep;
    world_.reset(std::move(setup.bodies));
    fitCamera();
    updateStats();  // seed the title-bar stats so the HUD isn't blank pre-0.5 s
}

void SimulationApp::rebuildScene(bool refitCamera) {
    // Rebuild with the FULL current option set via the per-preset
    // builders instead of createPreset's defaults. Mapping note:
    // figure-eight scale = separation / 10.0 — the MenuState separation
    // field is shared with two-body, whose default 10 means scale 1.0,
    // i.e. the byte-identical classic choreography (pinned by the core
    // figure-eight default tests).
    core::SimulationSetup setup{};
    switch (menuState_.preset) {
        case core::PresetName::TwoBody:
            setup = core::twoBody(menuState_.seed, menuState_.separation,
                                  menuState_.hyperbolic);
            break;
        case core::PresetName::SolarSystem:
            setup = core::solarSystem(menuState_.seed, menuState_.planetCount,
                                      menuState_.pluto);
            break;
        case core::PresetName::FigureEight:
            setup = core::figureEight(menuState_.seed,
                                      menuState_.separation / 10.0);
            break;
        case core::PresetName::RandomCloud:
            setup = core::randomCloud(menuState_.seed, menuState_.bodies);
            break;
    }
    world_.setGravity(setup.config.g, setup.config.softening);
    world_.reset(std::move(setup.bodies));
    world_.setTimeStep(menuState_.dt);
    accumulator_ = 0.0;
    if (refitCamera) {
        fitCamera();
    }
}

void SimulationApp::restartScene() {
    // R-restart debug trace: dump the full rebuild inputs (env-gated,
    // see Trace.hpp; env-off cost is one predicted branch).
    if (traceEnabled()) {
        std::fprintf(stderr,
                     "[%8.3f s] [trace] restart preset=%d seed=%lld "
                     "dt=%.9f bodies=%u sep=%g hyper=%d planets=%d pluto=%d "
                     "acc=%.6f paused=%d t=%.6f steps=%llu\n",
                     traceNow(),
                     static_cast<int>(menuState_.preset),
                     static_cast<long long>(menuState_.seed), menuState_.dt,
                     menuState_.bodies, menuState_.separation,
                     menuState_.hyperbolic ? 1 : 0, menuState_.planetCount,
                     menuState_.pluto ? 1 : 0, accumulator_,
                     paused_ ? 1 : 0, world_.simulationTime(),
                     static_cast<unsigned long long>(world_.totalSteps()));
    }
    rebuildScene(true);
}

// The SeedChanged rule, shared by its three sites (applyEffect,
// commitFieldEdit, the Space staged-resume block below): random-cloud
// regenerates immediately with the camera preserved (no refit); for every
// other preset the new seed is merely stored, taking effect on the next
// random-cloud load. Keep all three sites routed through here.
void SimulationApp::applySeedEffect() {
    if (menuState_.preset == core::PresetName::RandomCloud) {
        rebuildScene(false);
    }
}

void SimulationApp::handleAction(const MenuAction& action) {
    applyEffect(menuApply(menuState_, action, paused_));
}

// The single effect-handling table (the rebuild law): live option effects
// rebuild immediately WITH camera refit, except the random-cloud Cloud
// edits (seed/bodies), which rebuild but never refit. Staged/None need no
// app action.
void SimulationApp::applyEffect(MenuEffect effect) {
    switch (effect) {
        case MenuEffect::PresetChanged:
            // A preset swap can hide the open dropdown's group (the
            // Options/Cloud slot swap): close the stale dropdown instead
            // of leaving a hidden group "open" in the state (layout and
            // hit-testing already hide it; this keeps the state honest).
            if (menuState_.openGroup &&
                !menuGroupVisible(*menuState_.openGroup, menuState_)) {
                menuState_.openGroup.reset();
            }
            rebuildScene(true);
            break;
        case MenuEffect::IntegratorChanged:
            world_.setIntegrator(core::makeIntegrator(menuState_.integrator));
            break;
        case MenuEffect::DtChanged:
            world_.setTimeStep(menuState_.dt);
            break;
        case MenuEffect::SeedChanged:
        case MenuEffect::BodiesChanged:  // Cloud-only setter: no-refit rule
            applySeedEffect();
            break;
        case MenuEffect::SeparationChanged:
        case MenuEffect::PathChanged:
        case MenuEffect::PlanetsChanged:
        case MenuEffect::PlutoChanged:
            rebuildScene(true);  // option edit on a non-cloud preset refits
            break;
        case MenuEffect::SceneRestart:
            restartScene();
            break;
        case MenuEffect::Staged:
        case MenuEffect::None:
            break;
    }
}

// Focus the shared inline editor on a field row, pre-filled with the
// field's current value rendered like its row label. The dropdown stays
// open; Enter commits, Esc ends the edit only, click-away cancels.
void SimulationApp::openFieldEditor(TextField field) {
    activeField_ = field;
    fieldText_ = {};
    fieldSelecting_ = false;
    // Cursor-position math (clicks, drags, band/caret x) runs on the '0'
    // glyph advance at the 13 px field type size, measured once here:
    // digits dominate these buffers, so one digit's advance is a
    // deliberate approximation for the whole string's layout.
    fieldAdvance_ =
        font_ ? static_cast<double>(font_->getGlyph(U'0', 13, false).advance)
              : 0.0;
    // Dwell bookkeeping: focusing a field is a click, and clicks hide
    // tooltips; the next MouseMoved re-targets.
    tooltipTarget_ = {};
    tooltipText_.clear();
    switch (field) {
        case TextField::Seed:
            fieldText_.buffer = fieldInitialSeed(menuState_.seed);
            break;
        case TextField::Bodies:
            fieldText_.buffer = fieldInitialBodies(menuState_.bodies);
            break;
        case TextField::Separation:
            fieldText_.buffer = fieldInitialSeparation(menuState_.separation);
            break;
        case TextField::Planets:
            fieldText_.buffer = fieldInitialPlanets(menuState_.planetCount);
            break;
    }
    // Prefilled editors open with caret AND anchor at the end of text.
    fieldText_.caret = fieldText_.buffer.size();
    fieldText_.anchor = fieldText_.buffer.size();
}

void SimulationApp::commitFieldEdit() {
    const TextField field = *activeField_;
    activeField_.reset();
    // The SAME staging law as every other group: live when running, staged
    // (Space-resume applies it last, after preset/integrator/dt) when
    // paused. Each field routes to its own setter; parsing (incl. the
    // int64 saturation and the empty-commits-0 law) lives in TextField.cpp.
    MenuEffect effect = MenuEffect::None;
    switch (field) {
        case TextField::Seed:
            effect = menuSetSeed(menuState_,
                                 fieldParseSeed(fieldText_.buffer), paused_);
            break;
        case TextField::Bodies:
            effect =
                menuSetBodies(menuState_, fieldParseBodies(fieldText_.buffer),
                              paused_);
            break;
        case TextField::Separation:
            effect = menuSetSeparation(
                menuState_, fieldParseSeparation(fieldText_.buffer), paused_);
            break;
        case TextField::Planets:
            effect = menuSetPlanets(
                menuState_, fieldParsePlanets(fieldText_.buffer), paused_);
            break;
    }
    fieldText_ = {};
    fieldSelecting_ = false;
    // Same dropdown-close behavior as activating any other item, whether
    // the change applied live or staged.
    menuState_.openGroup.reset();
    applyEffect(effect);
}

// Ctrl+C/X/V while a field is focused. Copy takes the selection span, or
// the whole buffer with no selection; cut copies then
// deletes the selection (whole buffer when there is none); paste runs the
// clipboard through the field grammar AT the caret, replacing the active
// selection. Any other Ctrl combo is swallowed without touching the text.
void SimulationApp::handleFieldClipboard(const sf::Event::KeyPressed& key) {
    if (!activeField_) {
        return;
    }
    using Key = sf::Keyboard::Key;
    switch (key.code) {
        case Key::C:
            sf::Clipboard::setString(fieldCopyText(fieldText_));
            break;
        case Key::X:
            sf::Clipboard::setString(fieldCopyText(fieldText_));
            if (fieldText_.hasSelection()) {
                fieldDeleteSelection(fieldText_);
            } else {
                fieldText_.buffer.clear();  // no selection: cut takes all
                fieldText_.caret = 0;
                fieldText_.anchor = 0;
            }
            break;
        case Key::V: {
            const sf::String clip = sf::Clipboard::getString();
            fieldApplyPaste(*activeField_, clip.toAnsiString(), fieldText_);
            break;
        }
        default:
            break;  // Ctrl+anything else: swallowed, never typed
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
        menuState_.openGroup, menuState_.preset);
    for (const MenuRect& row : layout.disabledRows) {
        if (row.contains(position)) {
            return;
        }
    }

    // Clicking a field row focuses the shared inline editor instead of
    // activating an action: the dropdown stays open, commit closes it,
    // Esc keeps it, click-away cancels.
    if (const std::optional<TextField> field =
            fieldForItem(hit.group, hit.index, menuState_.preset)) {
        openFieldEditor(*field);
        return;
    }

    menuState_.openGroup.reset();
    if (hit.group == MenuGroup::Cloud &&
        (hit.index == 1 || hit.index == 3)) {
        // Cloud roll buttons: fresh random_device values routed through
        // the SAME setters as the typed fields (staging law
        // included). Seed: full int64 range; bodies: uniform [2, 2000].
        std::random_device rd;
        MenuEffect effect = MenuEffect::None;
        if (hit.index == 1) {
            std::uniform_int_distribution<std::int64_t> seedDist(
                std::numeric_limits<std::int64_t>::min(),
                std::numeric_limits<std::int64_t>::max());
            effect = menuSetSeed(menuState_, seedDist(rd), paused_);
        } else {
            std::uniform_int_distribution<std::uint32_t> bodiesDist(2, 2000);
            effect = menuSetBodies(menuState_, bodiesDist(rd), paused_);
        }
        applyEffect(effect);
        return;
    }
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
    // via is<T>()/getIf<T>(). (SFML 2's waitEvent/event.type idioms are
    // defects against this API.)
    while (const std::optional<sf::Event> ev = window_.pollEvent()) {
        if (ev->is<sf::Event::Closed>()) {
            window_.close();
            continue;
        }
        // Field editor priority: the delivery ORDER of
        // TextEntered vs the matching KeyPressed varies by platform (on
        // X11, WindowImplX11 pushes KeyPressed before TextEntered) -- never
        // rely on it here. Correctness is order-independent because the two
        // editing branches are disjoint: the TextEntered branch consumes
        // only grammar-valid chars (the shared fieldInsertChar), the
        // KeyPressed branch only Enter/Escape/Backspace/Delete/caret keys/
        // Ctrl+CXV (plus swallowing all other keys while editing), so no
        // input can double-handle under either platform order.
        if (activeField_) {
            if (const auto* text = ev->getIf<sf::Event::TextEntered>()) {
                const char32_t ch = text->unicode;
                if (ch >= U' ' && ch < U'\x7f') {
                    // Typing inserts AT the caret (replacing the selection
                    // when one is active); fieldInsertChar enforces the
                    // field's input grammar.
                    fieldInsertChar(*activeField_, static_cast<char>(ch),
                                    fieldText_);
                }
                // Every other codepoint (control chars incl. Enter/Backspace
                // and the Ctrl combos' 0x03/0x18/0x16, plus >= 0x80) is
                // ignored here: Ctrl combos NEVER type into the buffer.
                continue;
            }
        }
        if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
            if (activeField_) {
                if (key->control) {
                    // Ctrl+C/X/V clipboard routing; every other Ctrl combo
                    // is swallowed (never typed, never an app shortcut).
                    handleFieldClipboard(*key);
                } else if (key->code == sf::Keyboard::Key::Escape) {
                    // Cancel the EDIT ONLY: the dropdown stays open and the
                    // app survives -- deliberately no fall-through to the
                    // dropdown-close/quit Escape law below.
                    activeField_.reset();
                    fieldText_ = {};
                    fieldSelecting_ = false;
                } else if (key->code == sf::Keyboard::Key::Backspace) {
                    fieldEditDelete(*activeField_, fieldText_,
                                    false);  // empty buffer commits 0
                } else if (key->code == sf::Keyboard::Key::Delete) {
                    fieldEditDelete(*activeField_, fieldText_, true);
                } else if (key->code == sf::Keyboard::Key::Left) {
                    // Shift extends the selection (anchor stays), plain
                    // moves collapse the selection onto the caret.
                    fieldMoveCaret(fieldText_, -1, key->shift);
                } else if (key->code == sf::Keyboard::Key::Right) {
                    fieldMoveCaret(fieldText_, 1, key->shift);
                } else if (key->code == sf::Keyboard::Key::Home) {
                    fieldMoveCaretTo(fieldText_, false, key->shift);
                } else if (key->code == sf::Keyboard::Key::End) {
                    fieldMoveCaretTo(fieldText_, true, key->shift);
                } else if (key->code == sf::Keyboard::Key::Enter) {
                    commitFieldEdit();
                }
                // Any other key (Space, R, letters, ...) is swallowed here:
                // typing must never pause (Space) or restart (R) the scene.
                continue;
            }
            if (key->code == sf::Keyboard::Key::Escape) {
                hideTooltip();  // closing a menu hides the tooltip
                if (menuState_.openGroup) {
                    menuState_.openGroup.reset();
                } else {
                    window_.close();
                }
            } else if (key->code == sf::Keyboard::Key::Space) {
                hideTooltip();  // a menu close (staged chip disappears) hides
                if (paused_) {
                    const MenuStaged staged = menuTakeStaged(menuState_);
                    // Consumption order: preset (rebuilds with the FULL
                    // current options) -> integrator -> dt -> seed ->
                    // bodies. While paused the setters only wrote the
                    // staged mirrors (live option fields untouched), so the
                    // option mirrors fold into the live state FIRST — the
                    // preset step's rebuild must already use them.
                    if (staged.bodies) {
                        menuState_.bodies = *staged.bodies;
                    }
                    if (staged.separation) {
                        menuState_.separation = *staged.separation;
                    }
                    if (staged.hyperbolic) {
                        menuState_.hyperbolic = *staged.hyperbolic;
                    }
                    if (staged.planets) {
                        menuState_.planetCount = *staged.planets;
                    }
                    if (staged.pluto) {
                        menuState_.pluto = *staged.pluto;
                    }
                    if (staged.preset) {
                        menuState_.preset = *staged.preset;
                        // Same close-on-hiding law as the live preset
                        // path: a staged swap that hides the open
                        // group's dropdown closes it.
                        if (menuState_.openGroup &&
                            !menuGroupVisible(*menuState_.openGroup,
                                              menuState_)) {
                            menuState_.openGroup.reset();
                        }
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
                        applySeedEffect();
                    }
                    // The staged option tail (bodies/separation/path/
                    // planets/pluto): any mirror set (even without a staged
                    // preset) requires a rebuild with the new option values.
                    // Refit law: a staged bodies change is a random-cloud
                    // Cloud edit and never refits; every other option edit
                    // refits.
                    if (!staged.preset &&
                        (staged.bodies || staged.separation ||
                         staged.hyperbolic || staged.planets ||
                         staged.pluto)) {
                        const bool cloudOnly =
                            staged.bodies && !staged.separation &&
                            !staged.hyperbolic && !staged.planets &&
                            !staged.pluto &&
                            menuState_.preset == core::PresetName::RandomCloud;
                        rebuildScene(!cloudOnly);
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
            hideTooltip();  // scrolling hides the tooltip
            camera_.zoomAt(core::Vec2{static_cast<double>(wheel->position.x),
                                      static_cast<double>(wheel->position.y)},
                           static_cast<double>(wheel->delta),
                           static_cast<double>(size.x),
                           static_cast<double>(size.y));
        } else if (press != nullptr) {
            hideTooltip();  // any click hides the tooltip
            const core::Vec2 position{static_cast<double>(press->position.x),
                                      static_cast<double>(press->position.y)};
            const MenuLayout layout = menuLayout(
                static_cast<double>(size.x), static_cast<double>(size.y),
                menuState_.openGroup, menuState_.preset);
            const MenuHit hit = menuHitTest(layout, position);
            // Double-press detector for the focused field (SFML 3.0 has
            // no double-click event): every left press
            // records its time+spot, and a press within 250 ms AND 4 px
            // of the previous left press reads as a double-click.
            const bool left = press->button == sf::Mouse::Button::Left;
            const long long dpx = press->position.x - fieldLastPressPos_.x;
            const long long dpy = press->position.y - fieldLastPressPos_.y;
            const bool doublePress =
                left &&
                fieldPressClock_.getElapsedTime() < sf::milliseconds(250) &&
                dpx * dpx + dpy * dpy < 16;  // 4 px radius
            if (left) {
                fieldPressClock_.restart();
                fieldLastPressPos_ = press->position;
            }
            if (activeField_) {
                // The focused field's own row (and its group header) keep
                // focus; anywhere else the edit is cancelled first, then
                // the click falls through to normal handling below -- menu
                // hit or drag start, no early return.
                const bool onFieldRow =
                    left && hit.kind == MenuHitKind::Item &&
                    fieldForItem(hit.group, hit.index, menuState_.preset) ==
                        *activeField_;
                if (onFieldRow && hit.index >= 0 &&
                    static_cast<std::size_t>(hit.index) <
                        layout.items.size()) {
                    // Left-press inside the focused row places the caret
                    // at the clicked x; a double-press selects
                    // the whole buffer. The held drag then moves the caret
                    // with the anchor fixed (MouseMoved below); releasing
                    // anywhere keeps both selection and focus.
                    const MenuRect& row =
                        layout.items[static_cast<std::size_t>(hit.index)];
                    const double inner = row.topLeft.x + 8.0;
                    const double width = fieldAdvance_ *
                                         static_cast<double>(
                                             fieldText_.buffer.size());
                    fieldDragLeft_ = inner;
                    fieldDragWidth_ = width;
                    fieldSelecting_ = true;
                    if (doublePress) {
                        fieldText_.anchor = 0;
                        fieldText_.caret = fieldText_.buffer.size();
                    } else {
                        const std::size_t index = fieldIndexForFraction(
                            clampUnit(width > 0.0
                                          ? (position.x - inner) / width
                                          : (position.x > inner ? 1.0 : 0.0)),
                            fieldText_.buffer.size());
                        fieldText_.caret = index;
                        fieldText_.anchor = index;
                    }
                    continue;  // focus stays: no menu hit, no camera drag
                }
                const bool onFieldHeader =
                    left && hit.kind == MenuHitKind::Header &&
                    menuGroupOfField(*activeField_) == hit.group;
                if (onFieldHeader) {
                    continue;  // click on the field's header: focus stays
                }
                activeField_.reset();
                fieldText_ = {};
                fieldSelecting_ = false;
            }
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
            // Ending a field drag anywhere keeps the editor focused and
            // the selection it reached (press-based focus rules unchanged).
            fieldSelecting_ = false;
        } else if (move != nullptr) {
            if (dragging_) {
                camera_.panByPixels(
                    core::Vec2{static_cast<double>(move->position.x -
                                                   lastMouse_.x),
                               static_cast<double>(move->position.y -
                                                   lastMouse_.y)});
            }
            if (activeField_ && fieldSelecting_) {
                // Drag-select: the caret follows the cursor's x while the
                // anchor stays put; positions past the field's edges or
                // off the row clamp to the buffer ends.
                const double fraction =
                    fieldDragWidth_ > 0.0
                        ? (static_cast<double>(move->position.x) -
                           fieldDragLeft_) / fieldDragWidth_
                        : 0.0;
                fieldText_.caret = fieldIndexForFraction(
                    clampUnit(fraction), fieldText_.buffer.size());
            }
            lastMouse_ = move->position;
            // Tooltip target = header/row under the cursor, recomputed on
            // every move; a target CHANGE restarts the 0.5 s dwell.
            // Hovering never touches selection/highlight (menuState_ is
            // read-only here).
            updateTooltipTarget();
        }
    }
}

namespace {
/// @brief True when both hits denote the same menu cell: the tooltip
/// dwell tracks an unchanged header/row target.
/// @param a The previously resolved hover target.
/// @param b The newly resolved hover target.
/// @return True when kind, group, and index all agree.
bool sameHit(const MenuHit& a, const MenuHit& b) {
    return a.kind == b.kind && a.group == b.group && a.index == b.index;
}
}  // namespace

void SimulationApp::updateTooltipTarget() {
    const sf::Vector2u size = window_.getSize();
    const MenuLayout layout = menuLayout(
        static_cast<double>(size.x), static_cast<double>(size.y),
        menuState_.openGroup, menuState_.preset);
    const core::Vec2 cursor{static_cast<double>(lastMouse_.x),
                            static_cast<double>(lastMouse_.y)};
    const MenuHit hit = menuHitTest(layout, cursor);
    std::string text;
    if (hit.kind == MenuHitKind::Header) {
        text = menuHeaderTooltip(hit.group);
    } else if (hit.kind == MenuHitKind::Item) {
        text = menuItemTooltip(hit.group, hit.index, menuState_);
    }
    if (!sameHit(hit, tooltipTarget_)) {
        tooltipTarget_ = hit;
        tooltipClock_.restart();  // target change restarts the dwell timer
    }
    tooltipText_ = std::move(text);
}

void SimulationApp::hideTooltip() {
    tooltipTarget_ = {};
    tooltipText_.clear();
}

void SimulationApp::update(sf::Time frameTime) {
    // Fixed-timestep loop: the frame time is clamped by
    // kMaxFrameTime (0.25 s) and at most kMaxStepsFrame (30) steps run per
    // frame, with the leftover accumulator dropped — a hang never turns into
    // a catch-up burst. Pause drains the accumulator but skips world_.step().
    const StepPlan plan =
        planSteps(accumulator_, frameTime.asSeconds(), world_.timeStep());
    if (traceEnabled()) {
        // Step-planning inputs: a frozen sim shows up here as
        // steps=0 forever (dt/acc anomaly) or steps>0 with the world
        // refusing to advance (core-side stall).
        std::fprintf(stderr,
                     "[%8.3f s] [trace] plan ft=%.6f dt=%.9f acc=%.6f -> "
                     "steps=%d rem=%.6f paused=%d t=%.6f\n",
                     traceNow(),
                     frameTime.asSeconds(), world_.timeStep(), accumulator_,
                     plan.steps, plan.remainder, paused_ ? 1 : 0,
                     world_.simulationTime());
    }
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
    const double ups =
        secs > 0.0
            ? static_cast<double>(stepsSince(steps, lastTotalSteps_)) / secs
            : 0.0;
    const double fps = secs > 0.0 ? framesInWindow_ / secs : 0.0;
    lastTotalSteps_ = steps;
    framesInWindow_ = 0;
    statsSuffix_ = formatStats(fps, ups, world_.bodies().size(),
                               world_.simulationTime());
    window_.setTitle(liveTitle(menuState_) + statsSuffix_);
    if (traceEnabled()) {
        // Stats-window outcome: the measured interval (secs) and the
        // UPS/fps the title now shows. stepsSince clamps the post-restart
        // counter reset (a raw unsigned subtract wrapped to ~2^64 and the
        // title rendered garbage UPS for one window after every R);
        // paused/acc alongside t is what distinguishes the pause,
        // catch-up and clock theories when a restart looks stalled.
        std::fprintf(stderr,
                     "[%8.3f s] [trace] stats secs=%.6f ups=%.1f fps=%.1f "
                     "bodies=%zu t=%.6f acc=%.6f paused=%d\n",
                     traceNow(), secs, ups, fps, world_.bodies().size(),
                     world_.simulationTime(), accumulator_, paused_ ? 1 : 0);
    }
}

/// @brief Draws one body in the planet style.
/// SFML 3.0.2 has no EllipseShape and no stencil/group APIs, so every
/// effect here is built from plain CircleShapes, RectangleShapes and
/// Triangle VertexArrays.
/// Draw order per body: glow halos -> ring back -> body circle -> shading
/// -> bands -> ring front.
/// @param window Target render window.
/// @param body The body to draw (position, radius, appearance).
/// @param camera Active camera mapping world space onto the window.
/// @param w Window width in pixels.
/// @param h Window height in pixels.
void drawBody(sf::RenderWindow& window, const core::Body& body,
              const Camera& camera, double w, double h) {
    constexpr double kPi = 3.14159265358979323846;
    const sf::Vector2f center = toSf(camera.toPixels(body.position, w, h));

    sf::Color bodyColor = kBodyPalette[body.id % 8U];
    if (body.appearance.color != 0U) {  // explicit 0xRRGGBB wins over palette
        const std::uint32_t c = body.appearance.color;
        bodyColor = sf::Color(static_cast<std::uint8_t>((c >> 16) & 0xFFU),
                              static_cast<std::uint8_t>((c >> 8) & 0xFFU),
                              static_cast<std::uint8_t>(c & 0xFFU));
    }

    // Minimum visible size: 3 px floor, doubled for glowing bodies
    // (suns). Cosmetic only — physical collision radii are untouched.
    const double px = std::max(body.radius * camera.scale,
                               body.appearance.glow ? 6.0 : 3.0);

    auto disc = [&](double radius, sf::Color fill, sf::Vector2f at) {
        sf::CircleShape shape{static_cast<float>(radius)};
        shape.setOrigin(sf::Vector2f{static_cast<float>(radius),
                                     static_cast<float>(radius)});
        shape.setFillColor(fill);
        shape.setPosition(at);
        window.draw(shape);
    };

    if (body.appearance.glow) {  // sun: three soft concentric halos
        // Halo radii deliberately tighter than 3.2/2.1/1.4: less overdraw
        // on aarch64, and verified visually acceptable.
        const double haloRadius[3] = {px * 2.5, px * 1.8, px * 1.3};
        const std::uint8_t haloAlpha[3] = {24, 48, 96};
        for (int i = 0; i < 3; ++i) {
            sf::Color halo = bodyColor;
            halo.a = haloAlpha[i];
            disc(haloRadius[i], halo, center);
        }
    }

    // Saturn-style ring: VertexArray triangle annulus, inner 1.45x / outer
    // 2.3x body radius, y-squashed 0.38 and rotated -18 degrees. The full
    // annulus draws behind the planet; the near half (segments whose rotated
    // y > 0, i.e. below the ring centre line on screen) is redrawn after the
    // planet so the ring appears to pass in front.
    auto drawRing = [&](bool front) {
        constexpr int kSegments = 64;
        const double rot = -18.0 * kPi / 180.0;
        const double cr = std::cos(rot);
        const double sr = std::sin(rot);
        const sf::Color ringColor(210, 190, 150,
                                  front ? std::uint8_t{200}
                                        : std::uint8_t{140});
        auto pointAt = [&](double radius, double angle) {
            const double lx = radius * std::cos(angle);
            const double ly = radius * std::sin(angle) * 0.38;  // squash
            return sf::Vector2f{
                center.x + static_cast<float>(lx * cr - ly * sr),
                center.y + static_cast<float>(lx * sr + ly * cr)};
        };
        // SFML 3.0.2's sf::VertexArray has no reserve() (only resize /
        // append -- VertexArray.hpp), so sizing it up front to the exact
        // upper bound (6 vertices per segment, none skipped) is the
        // reserve: one allocation, no append-time regrowth.
        sf::VertexArray ring{sf::PrimitiveType::Triangles,
                            static_cast<std::size_t>(kSegments) * 6U};
        std::size_t used = 0;
        for (int i = 0; i < kSegments; ++i) {
            const double a0 = 2.0 * kPi * static_cast<double>(i) /
                              static_cast<double>(kSegments);
            const double a1 = 2.0 * kPi * static_cast<double>(i + 1) /
                              static_cast<double>(kSegments);
            const double am = 0.5 * (a0 + a1);
            const double midY =
                px * std::cos(am) * sr + px * std::sin(am) * 0.38 * cr;
            if (front && midY <= 0.0) {
                continue;  // far half: already covered by the full annulus
            }
            const sf::Vector2f i0 = pointAt(px * 1.45, a0);
            const sf::Vector2f i1 = pointAt(px * 1.45, a1);
            const sf::Vector2f o0 = pointAt(px * 2.3, a0);
            const sf::Vector2f o1 = pointAt(px * 2.3, a1);
            ring[used++] = sf::Vertex{i0, ringColor};
            ring[used++] = sf::Vertex{o0, ringColor};
            ring[used++] = sf::Vertex{o1, ringColor};
            ring[used++] = sf::Vertex{i0, ringColor};
            ring[used++] = sf::Vertex{o1, ringColor};
            ring[used++] = sf::Vertex{i1, ringColor};
        }
        ring.resize(used);  // drop the segments the front pass skipped
        if (ring.getVertexCount() > 0) {
            window.draw(ring);
        }
    };

    if (body.appearance.ring) {
        drawRing(false);
    }

    disc(px, bodyColor, center);  // the body itself

    // Shading variant chosen: the offset-circle FALLBACK, not
    // the 180-degree sector. SFML 3.0.2 has no stencil, and the two-disc
    // half-overlay composite flickers; a single (0,0,0,50) disc offset
    // 0.25*px away from the world origin (the sun) reads correctly as the
    // night side in this top-down sun-centred scene, is stencil-free and
    // costs one draw call.
    if (px > 4.0) {
        const sf::Vector2f sun =
            toSf(camera.toPixels(core::Vec2{0.0, 0.0}, w, h));
        sf::Vector2f away = center - sun;  // screen direction from the sun
        const double len = std::hypot(away.x, away.y);
        if (len > 1.0) {  // body at the sun's own screen spot: no shade
            away *= static_cast<float>(0.25 * px / len);
            disc(px, sf::Color(0, 0, 0, 50), center + away);
        }
    }

    // Gas-giant bands: chord RectangleShapes fully inside the disc, no
    // stencil needed. Latitudes t = +-0.4 exactly for two-band planets,
    // else the generic t = 2k/(bands+1)-1; half-chord 0.88*r*sqrt(1-t^2),
    // height r*0.35/(bands/2), colour = 0.72x body RGB.
    if (body.appearance.bands > 0 && px >= 8.0) {
        const int bands = body.appearance.bands;
        const float bandHeight = static_cast<float>(px * 0.35 / (bands / 2.0));
        const sf::Color bandColor(
            static_cast<std::uint8_t>(bodyColor.r * 0.72F),
            static_cast<std::uint8_t>(bodyColor.g * 0.72F),
            static_cast<std::uint8_t>(bodyColor.b * 0.72F), 255);
        for (int k = 1; k <= bands; ++k) {
            // The two-band latitudes are pinned to exactly +-0.4r; the
            // generic formula would give +-1/3, so emit +-0.4*px directly
            // when bands==2 (Saturn). Jupiter (bands=3) keeps the generic
            // -0.5/0/+0.5 pattern (deviation documented, not silently off).
            const double t =
                bands == 2
                    ? (k == 1 ? -0.4 : 0.4)
                    : 2.0 * static_cast<double>(k) /
                          static_cast<double>(bands + 1) -
                          1.0;
            const double halfW =
                px * std::sqrt(std::max(0.0, 1.0 - t * t)) * 0.88;
            sf::RectangleShape band{
                sf::Vector2f{static_cast<float>(2.0 * halfW), bandHeight}};
            band.setPosition(sf::Vector2f{
                center.x - static_cast<float>(halfW),
                center.y + static_cast<float>(px * t) - bandHeight * 0.5F});
            band.setFillColor(bandColor);
            window.draw(band);
        }
    }

    if (body.appearance.ring) {
        drawRing(true);  // near half over the planet
    }
}

void SimulationApp::render() {
    const sf::Vector2u size = window_.getSize();  // live, handles resize
    const double w = static_cast<double>(size.x);
    const double h = static_cast<double>(size.y);

    window_.clear(kBackground);

    // Infinite tiled starfield first: the viewport's world rect
    // (viewportStarRect: camera_.toWorld of the corners + 2 px pad, the
    // corner mapping itself unit-tested in test_starfield.cpp) is re-queried
    // every frame, so panning slides a subset of one global grid and zoom
    // stays populated at any scale. Quads stay viewport-culled with a
    // 2 px pixel margin.
    const Rect view = viewportStarRect(camera_, w, h);
    const std::vector<Star> stars =
        starsInRect(view.x0, view.y0, view.x1, view.y1);
    for (const Star& star : stars) {
        const core::Vec2 p = camera_.toPixels(star.position, w, h);
        if (p.x < -2.0 || p.x > w + 2.0 || p.y < -2.0 || p.y > h + 2.0) {
            continue;
        }
        sf::RectangleShape dot{
            sf::Vector2f{static_cast<float>(star.sizePx),
                        static_cast<float>(star.sizePx)}};
        dot.setPosition(toSf(p));
        // dim = alpha 120 (the 0.2-0.8 alpha band); bright = FULL white:
        // deliberately brighter for visibility on bright desktops.
        dot.setFillColor(sf::Color(
            star.r, star.g, star.b,
            static_cast<std::uint8_t>(star.dim ? 120 : 255)));
        window_.draw(dot);
    }

    for (const core::Body& b : world_.bodies()) {
        drawBody(window_, b, camera_, w, h);
    }

    // No on-screen HUD by design: stats live in the title bar only
    // (updateStats). The staged chip below stays -- it is menu UI, not
    // a HUD.

    const MenuLayout layout =
        menuLayout(w, h, menuState_.openGroup, menuState_.preset);
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
            // Hidden groups are absent from the layout, so the header must
            // come from headerGroups, never from the loop index.
            const MenuGroup group = layout.headerGroups[index];
            const MenuRect& rect = layout.headers[index];
            const bool open = menuState_.openGroup == group;

            sf::Text label{*font_};
            label.setCharacterSize(12U);
            label.setFillColor(open ? kHudColor : kMenuDimColor);
            // The Options header wears the preset's short name when it
            // has one (Solar / Two-body / Fig-8); the plain group label
            // is only the fallback.
            std::string name(menuGroupLabel(group));
            if (group == MenuGroup::Options) {
                const std::string presetName(
                    menuOptionsHeaderLabel(menuState_.preset));
                if (!presetName.empty()) {
                    name = presetName;
                }
            }
            label.setString(name + ": " + menuValue(menuState_, group));
            label.setPosition(sf::Vector2f{
                static_cast<float>(rect.topLeft.x + 7.0),
                static_cast<float>(rect.topLeft.y + 7.0)});
            window_.draw(label);
            // No underline or box on the open tab: the dropdown itself already
            // shows which group is open; "open" only brightens the label.
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

                if (activeField_ &&
                    fieldForItem(*layout.openGroup, static_cast<int>(index),
                                 menuState_.preset) == *activeField_) {
                    // Focused typed field at the row
                    // rect: accent outline, selection band, buffer text,
                    // and a 1 px accent caret AT the caret index (no more
                    // end-of-buffer "|" glyph). The caret blinks on the
                    // existing statsClock_ 0.5 s period (shown for its
                    // first half, hidden for its second).
                    const double boxX = rect.topLeft.x + 2.0;
                    const double boxY = rect.topLeft.y + 2.0;
                    const double boxW = rect.size.x - 4.0;
                    const double boxH = rect.size.y - 4.0;
                    const double textLeft = rect.topLeft.x + 8.0;
                    sf::RectangleShape field{sf::Vector2f{
                        static_cast<float>(boxW), static_cast<float>(boxH)}};
                    field.setPosition(sf::Vector2f{static_cast<float>(boxX),
                                                   static_cast<float>(boxY)});
                    field.setFillColor(kMenuPanelColor);
                    field.setOutlineThickness(1.0F);
                    field.setOutlineColor(kMenuAccentColor);
                    window_.draw(field);

                    if (fieldText_.hasSelection()) {
                        // Band drawn BEFORE the text, clamped to the box.
                        const auto clampX = [boxX, boxW](double x) {
                            return std::min(std::max(x, boxX), boxX + boxW);
                        };
                        const double bandX0 = clampX(
                            textLeft + fieldAdvance_ *
                                           static_cast<double>(
                                               fieldText_.selStart()));
                        const double bandX1 = clampX(
                            textLeft + fieldAdvance_ *
                                           static_cast<double>(
                                               fieldText_.selEnd()));
                        if (bandX1 > bandX0) {
                            sf::RectangleShape band{sf::Vector2f{
                                static_cast<float>(bandX1 - bandX0),
                                static_cast<float>(boxH)}};
                            band.setPosition(
                                sf::Vector2f{static_cast<float>(bandX0),
                                            static_cast<float>(boxY)});
                            band.setFillColor(kFieldSelectionColor);
                            window_.draw(band);
                        }
                    }

                    sf::Text typed{*font_};
                    typed.setCharacterSize(13U);
                    typed.setFillColor(kHudColor);
                    typed.setString(fieldText_.buffer);
                    typed.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 8.0),
                        static_cast<float>(rect.topLeft.y + 3.0)});
                    window_.draw(typed);

                    if (statsClock_.getElapsedTime().asSeconds() < 0.25) {
                        sf::RectangleShape caret{sf::Vector2f{
                            1.0F, static_cast<float>(rect.size.y - 6.0)}};
                        caret.setPosition(sf::Vector2f{
                            static_cast<float>(
                                textLeft + fieldAdvance_ *
                                               static_cast<double>(
                                                   fieldText_.caret)),
                            static_cast<float>(rect.topLeft.y + 3.0)});
                        caret.setFillColor(kMenuAccentColor);
                        window_.draw(caret);
                    }
                    continue;
                }

                const std::string item = menuItemLabel(
                    menuState_, *layout.openGroup, static_cast<int>(index));
                const bool selected = static_cast<int>(index) == ticked;
                sf::Text label{*font_};
                label.setCharacterSize(13U);
                label.setFillColor(selected ? kHudColor : kMenuDimColor);
                label.setString(item);
                label.setPosition(sf::Vector2f{
                    static_cast<float>(
                        rect.topLeft.x +
                        menuRowTextIndent(*layout.openGroup,
                                          static_cast<int>(index),
                                          menuState_.preset)),
                    static_cast<float>(rect.topLeft.y + 3.0)});
                window_.draw(label);

                if (selected) {  // the one selection indicator: dot + tint.
                    // A drawn circle, not a glyph: ✓ renders as tofu boxes
                    // with some system fonts.
                    sf::CircleShape dot(3.0F, 24U);
                    dot.setFillColor(kMenuSelectColor);
                    dot.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x +
                                           kMenuRowDotIndent),
                        static_cast<float>(rect.topLeft.y + rect.size.y /
                                                            2.0 -
                                                    3.0)});
                    window_.draw(dot);
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

    // Tooltips draw LAST, above the staged chip.
    renderTooltip();
}

// Hover tooltip box: bg rgb(30,34,54) alpha 240, 1 px border,
// 11 px text, at cursor+(12,18), clamped on-screen. Shown once the cursor
// has dwelled 0.5 s on an unchanged header/row target (the clock was last
// restarted by updateTooltipTarget on a target change).
void SimulationApp::renderTooltip() {
    if (!font_ || tooltipText_.empty()) {
        return;
    }
    if (tooltipClock_.getElapsedTime().asSeconds() < 0.5) {
        return;
    }
    sf::Text tip{*font_};
    tip.setCharacterSize(11U);
    tip.setFillColor(kHudColor);
    tip.setString(tooltipText_);
    const sf::FloatRect bounds = tip.getLocalBounds();
    constexpr float kPad = 5.0F;
    const sf::Vector2f boxSize{bounds.size.x + 2.0F * kPad,
                               bounds.size.y + 2.0F * kPad};
    const sf::Vector2u size = window_.getSize();
    float x = static_cast<float>(lastMouse_.x) + 12.0F;
    float y = static_cast<float>(lastMouse_.y) + 18.0F;
    x = std::min(x, static_cast<float>(size.x) - boxSize.x - 2.0F);
    y = std::min(y, static_cast<float>(size.y) - boxSize.y - 2.0F);
    x = std::max(2.0F, x);
    y = std::max(2.0F, y);
    sf::RectangleShape box{boxSize};
    box.setPosition(sf::Vector2f{x, y});
    box.setFillColor(sf::Color(30, 34, 54, 240));
    box.setOutlineThickness(1.0F);
    box.setOutlineColor(kMenuBorderColor);
    window_.draw(box);
    tip.setPosition(sf::Vector2f{x + kPad, y + kPad});
    window_.draw(tip);
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
