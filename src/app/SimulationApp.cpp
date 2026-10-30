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
constexpr sf::Color kMenuSelectColor(150, 230, 170);   // current-item dot
constexpr sf::Color kMenuPanelColor(22, 25, 38, 242);
constexpr sf::Color kMenuBorderColor(70, 78, 100);
constexpr sf::Color kMenuAccentColor(120, 190, 255);  // focused seed field border
constexpr double kMenuStripBottom = 34.0;

// SFML 3.0.x has NO Font::getDefault() (that is a later 3.1/master API;
// verified against build/_deps/sfml-src/include/SFML/Graphics/Font.hpp — the
// only statics there are glyph/texture accessors). So the plan's "belt"
// applies: try a few well-known system font files; if none loads, all
// in-window text (menu labels, chip, seed field) is skipped and stats stay
// title-bar-only.
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
    if (group == MenuGroup::Cloud) {
        return std::to_string(state.seed);
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
    // (fixes the Task-1-noted bug where windowed restart dropped --bodies).
    menuState_.bodies = bodies;
    menuState_.dt = setup.config.timeStep;
    world_.reset(std::move(setup.bodies));
    fitCamera();
    updateStats();  // seed the title-bar stats so the HUD isn't blank pre-0.5 s
}

void SimulationApp::rebuildScene(bool refitCamera) {
    // Rebuild with the FULL current option set (spec D.2/D.3) via the
    // per-preset builders instead of createPreset's defaults. Mapping note
    // (controller ruling): figure-eight scale = separation / 10.0 — the
    // MenuState separation field is shared with two-body (spec D.3), whose
    // default 10 means scale 1.0, i.e. the byte-identical classic
    // choreography (pinned by the core figure-eight default tests).
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
    // D.7 R-stall probe: dump the full rebuild inputs (env-gated, see
    // Trace.hpp; env-off cost is one predicted branch).
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

// The single effect-dispatch table (spec D.2 rebuild law): live option
// effects rebuild immediately WITH camera refit, except the random-cloud
// Cloud edits (seed/bodies), which rebuild but never refit. Staged/None
// need no app action; everything else mirrors the previous behavior.
void SimulationApp::applyEffect(MenuEffect effect) {
    switch (effect) {
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

// Focus the shared inline editor on a field row (spec D.4), pre-filled
// with the field's current value rendered like its row label. The dropdown
// stays open; Enter commits, Esc ends the edit only, click-away cancels.
void SimulationApp::openFieldEditor(TextField field) {
    activeField_ = field;
    fieldBuffer_.clear();
    // Dwell bookkeeping: focusing a field is a click, and clicks hide
    // tooltips (D.6); the next MouseMoved re-targets.
    tooltipTarget_ = {};
    tooltipText_.clear();
    switch (field) {
        case TextField::Seed:
            fieldBuffer_ = fieldInitialSeed(menuState_.seed);
            break;
        case TextField::Bodies:
            fieldBuffer_ = fieldInitialBodies(menuState_.bodies);
            break;
        case TextField::Separation:
            fieldBuffer_ = fieldInitialSeparation(menuState_.separation);
            break;
        case TextField::Planets:
            fieldBuffer_ = fieldInitialPlanets(menuState_.planetCount);
            break;
    }
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
            effect = menuSetSeed(menuState_, fieldParseSeed(fieldBuffer_),
                                 paused_);
            break;
        case TextField::Bodies:
            effect = menuSetBodies(menuState_, fieldParseBodies(fieldBuffer_),
                                   paused_);
            break;
        case TextField::Separation:
            effect = menuSetSeparation(
                menuState_, fieldParseSeparation(fieldBuffer_), paused_);
            break;
        case TextField::Planets:
            effect =
                menuSetPlanets(menuState_, fieldParsePlanets(fieldBuffer_),
                               paused_);
            break;
    }
    fieldBuffer_.clear();
    // Same dropdown-close behavior as activating any other item, whether
    // the change applied live or staged.
    menuState_.openGroup.reset();
    applyEffect(effect);
}

// Ctrl+C/X/V while a field is focused (spec D.4). Copy takes the buffer,
// cut copies then clears, paste REPLACES the buffer with the clipboard
// filtered through the field grammar (same overflow rules as typing).
// Any other Ctrl combo is swallowed without touching the buffer.
void SimulationApp::handleFieldClipboard(const sf::Event::KeyPressed& key) {
    if (!activeField_) {
        return;
    }
    using Key = sf::Keyboard::Key;
    switch (key.code) {
        case Key::C:
            sf::Clipboard::setString(fieldBuffer_);
            break;
        case Key::X:
            sf::Clipboard::setString(fieldBuffer_);
            fieldBuffer_.clear();
            break;
        case Key::V: {
            const sf::String clip = sf::Clipboard::getString();
            fieldBuffer_ =
                fieldSanitizePaste(*activeField_, clip.toAnsiString());
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
    // activating an action (spec D.4): the dropdown stays open, commit
    // closes it, Esc keeps it, click-away cancels.
    if (const std::optional<TextField> field =
            fieldForItem(hit.group, hit.index, menuState_.preset)) {
        openFieldEditor(*field);
        return;
    }

    menuState_.openGroup.reset();
    if (hit.group == MenuGroup::Cloud &&
        (hit.index == 1 || hit.index == 3)) {
        // Cloud roll buttons (spec D.2): fresh random_device values routed
        // through the SAME setters as the typed fields (staging law
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
    // via is<T>()/getIf<T>(). (The SFML 2 polling/enum-tag idioms would be a
    // defect per plan Review Focus #1.)
    while (const std::optional<sf::Event> ev = window_.pollEvent()) {
        if (ev->is<sf::Event::Closed>()) {
            window_.close();
            continue;
        }
        // Field editor priority (Review Focus 4): the delivery ORDER of
        // TextEntered vs the matching KeyPressed varies by platform (on
        // X11, WindowImplX11 pushes KeyPressed before TextEntered) -- never
        // rely on it here. Correctness is order-independent because the two
        // editing branches are disjoint: the TextEntered branch consumes
        // only grammar-valid chars (the shared fieldAppendChar), the
        // KeyPressed branch only Enter/Escape/Backspace/Ctrl+CXV (plus
        // swallowing all other keys while editing), so no input can
        // double-handle under either platform order.
        if (activeField_) {
            if (const auto* text = ev->getIf<sf::Event::TextEntered>()) {
                const char32_t ch = text->unicode;
                if (ch >= U' ' && ch < U'\x7f') {
                    fieldAppendChar(*activeField_,
                                    static_cast<char>(ch), fieldBuffer_);
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
                    fieldBuffer_.clear();
                } else if (key->code == sf::Keyboard::Key::Backspace) {
                    fieldBackspace(*activeField_, fieldBuffer_);  // empty
                    // buffer commits 0
                } else if (key->code == sf::Keyboard::Key::Enter) {
                    commitFieldEdit();
                }
                // Any other key (Space, R, letters, ...) is swallowed here:
                // typing must never pause (Space) or restart (R) the scene.
                continue;
            }
            if (key->code == sf::Keyboard::Key::Escape) {
                hideTooltip();  // menu-close hides tooltips (D.6)
                if (menuState_.openGroup) {
                    menuState_.openGroup.reset();
                } else {
                    window_.close();
                }
            } else if (key->code == sf::Keyboard::Key::Space) {
                hideTooltip();  // a menu close (staged chip disappears) hides
                if (paused_) {
                    const MenuStaged staged = menuTakeStaged(menuState_);
                    // Consumption order (spec D.3): preset (rebuilds with
                    // the FULL current options) -> integrator -> dt -> seed
                    // -> bodies. While paused the setters only wrote the
                    // staged mirrors (live option fields untouched), so the
                    // option mirrors fold into the live state FIRST — the
                    // preset step's rebuild must already use them ("option
                    // rebuilds folded into the preset step", D.3).
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
                    // Refit law per D.2: a staged bodies change is a
                    // random-cloud Cloud edit and never refits; every other
                    // option edit refits.
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
            hideTooltip();  // scroll hides tooltips (D.6)
            camera_.zoomAt(core::Vec2{static_cast<double>(wheel->position.x),
                                      static_cast<double>(wheel->position.y)},
                           static_cast<double>(wheel->delta),
                           static_cast<double>(size.x),
                           static_cast<double>(size.y));
        } else if (press != nullptr) {
            hideTooltip();  // any click hides the tooltip (D.6)
            const core::Vec2 position{static_cast<double>(press->position.x),
                                      static_cast<double>(press->position.y)};
            const MenuLayout layout = menuLayout(
                static_cast<double>(size.x), static_cast<double>(size.y),
                menuState_.openGroup, menuState_.preset);
            const MenuHit hit = menuHitTest(layout, position);
            if (activeField_) {
                // The focused field's own row (and its group header) keep
                // focus; anywhere else the edit is cancelled first, then
                // the click falls through to normal handling below -- menu
                // hit or drag start, no early return (D.4).
                const bool inField =
                    press->button == sf::Mouse::Button::Left &&
                    ((hit.kind == MenuHitKind::Item &&
                      fieldForItem(hit.group, hit.index, menuState_.preset) ==
                          *activeField_) ||
                     (hit.kind == MenuHitKind::Header &&
                      menuGroupOfField(*activeField_) == hit.group));
                if (inField) {
                    continue;  // click inside the field: focus stays, no-op
                }
                activeField_.reset();
                fieldBuffer_.clear();
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
        } else if (move != nullptr) {
            if (dragging_) {
                camera_.panByPixels(
                    core::Vec2{static_cast<double>(move->position.x -
                                                   lastMouse_.x),
                               static_cast<double>(move->position.y -
                                                   lastMouse_.y)});
            }
            lastMouse_ = move->position;
            // Tooltip target = header/row under the cursor, recomputed on
            // every move; a target CHANGE restarts the 0.5 s dwell (D.6).
            // Hovering never touches selection/highlight (menuState_ is
            // read-only here).
            updateTooltipTarget();
        }
    }
}

namespace {
// MenuHit identity for dwell bookkeeping: same cell under the cursor.
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
        tooltipClock_.restart();  // target change resets the dwell (D.6)
    }
    tooltipText_ = std::move(text);
}

void SimulationApp::hideTooltip() {
    tooltipTarget_ = {};
    tooltipText_.clear();
}

void SimulationApp::update(sf::Time frameTime) {
    // Fixed-timestep loop (plan D.2): the frame time is clamped by
    // kMaxFrameTime (0.25 s) and at most kMaxStepsFrame (30) steps run per
    // frame, with the leftover accumulator dropped — a hang never turns into
    // a catch-up burst. Pause drains the accumulator but skips world_.step().
    const StepPlan plan =
        planSteps(accumulator_, frameTime.asSeconds(), world_.timeStep());
    if (traceEnabled()) {
        // Step-planning inputs (D.7): a frozen sim shows up here as
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
        // Stats-window outcome (D.7): the measured interval (secs) and the
        // UPS/fps the title now shows. stepsSince clamps the post-restart
        // counter reset (a raw unsigned subtract wrapped to ~2^64 and the
        // title rendered garbage UPS for one window after every R);
        // paused/acc alongside t is what the controller's one-off R stall
        // needed to distinguish pause/catch-up/clock theories.
        std::fprintf(stderr,
                     "[%8.3f s] [trace] stats secs=%.6f ups=%.1f fps=%.1f "
                     "bodies=%zu t=%.6f acc=%.6f paused=%d\n",
                     traceNow(), secs, ups, fps, world_.bodies().size(),
                     world_.simulationTime(), accumulator_, paused_ ? 1 : 0);
    }
}

// Spec D.3 planet-style body renderer (Task 2). SFML 3.0.2 has NO
// EllipseShape and NO stencil/group APIs, so every effect below is built
// from plain CircleShapes, RectangleShapes and Triangle VertexArrays.
// Draw order per body: glow halos -> ring back -> body circle -> shading ->
// bands -> ring front.
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

    // Minimum visible size (D.3): 3 px floor, doubled for glowing bodies
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
        // Halo radii deliberately tighter than spec D.3's 3.2/2.1/1.4: less
        // overdraw on aarch64, and verified visually acceptable.
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

    // Shading variant chosen: the offset-circle FALLBACK from spec D.3, not
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

    // Gas-giant bands (D.3): chord RectangleShapes fully inside the disc, no
    // stencil needed. Latitudes t = +-0.4 exactly for two-band planets (spec),
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
            // Spec D.3 pins the two-band latitudes to exactly +-0.4r; the
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

    // Infinite tiled starfield first (spec D.5): the viewport's world rect
    // (viewportStarRect: camera_.toWorld of the corners + 2 px pad, the
    // corner mapping itself unit-tested in test_starfield.cpp) is re-queried
    // every frame, so panning slides a subset of one global grid and zoom
    // stays populated at any scale. Quads stay viewport-culled with the
    // same 2 px pixel margin the old fixed field used.
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
        // dim = alpha 120, spec D.3's 0.2-0.8 band; bright = FULL white:
        // a deliberate deviation for visibility on bright desktops.
        dot.setFillColor(sf::Color(
            star.r, star.g, star.b,
            static_cast<std::uint8_t>(star.dim ? 120 : 255)));
        window_.draw(dot);
    }

    for (const core::Body& b : world_.bodies()) {
        drawBody(window_, b, camera_, w, h);
    }

    // On-screen HUD duplicate deliberately removed (spec D.3): stats live
    // in the title bar only (updateStats). The staged-chip below stays --
    // it is menu UI, not a HUD.

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
            label.setString(std::string(menuGroupLabel(group)) + ": " +
                            menuValue(menuState_, group));
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
                    // Focused typed field (spec D.4) at the row rect:
                    // accent outline + buffer + caret. The caret blinks on
                    // the existing statsClock_ 0.5 s period (shown for its
                    // first half, hidden for its second).
                    sf::RectangleShape field{sf::Vector2f{
                        static_cast<float>(rect.size.x - 4.0),
                        static_cast<float>(rect.size.y - 4.0)}};
                    field.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 2.0),
                        static_cast<float>(rect.topLeft.y + 2.0)});
                    field.setFillColor(kMenuPanelColor);
                    field.setOutlineThickness(1.0F);
                    field.setOutlineColor(kMenuAccentColor);
                    window_.draw(field);

                    const bool caretOn =
                        statsClock_.getElapsedTime().asSeconds() < 0.25;
                    sf::Text typed{*font_};
                    typed.setCharacterSize(13U);
                    typed.setFillColor(kHudColor);
                    typed.setString(fieldBuffer_ + (caretOn ? "|" : " "));
                    typed.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 8.0),
                        static_cast<float>(rect.topLeft.y + 3.0)});
                    window_.draw(typed);
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
                    static_cast<float>(rect.topLeft.x + 30.0),
                    static_cast<float>(rect.topLeft.y + 3.0)});
                window_.draw(label);

                if (selected) {  // the one selection indicator: dot + tint.
                    // A drawn circle, not a glyph: ✓ renders as tofu boxes
                    // with some system fonts.
                    sf::CircleShape dot(3.0F, 24U);
                    dot.setFillColor(kMenuSelectColor);
                    dot.setPosition(sf::Vector2f{
                        static_cast<float>(rect.topLeft.x + 10.0),
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

    // Tooltips draw LAST, above the staged chip (spec D.6).
    renderTooltip();
}

// Hover tooltip box (spec D.6): bg rgb(30,34,54) alpha 240, 1 px border,
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
