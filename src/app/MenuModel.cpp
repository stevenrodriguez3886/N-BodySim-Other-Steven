#include "nbody/app/MenuModel.hpp"

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace nbody::app {

struct MenuState::Impl {
    MenuStaged staged;
    mutable std::string summary;
};

struct MenuModelAccess {
    static MenuState::Impl& get(MenuState& state) { return *state.impl_; }
    static const MenuState::Impl& get(const MenuState& state) {
        return *state.impl_;
    }
};

namespace {

struct DtOption {
    double value;
    const char* label;
};

constexpr std::array<DtOption, 5> kDtOptions = {{{1.0 / 30.0, "1/30"},
                                                {1.0 / 60.0, "1/60"},
                                                {1.0 / 120.0, "1/120"},
                                                {1.0 / 240.0, "1/240"},
                                                {1.0 / 480.0, "1/480"}}};

// Spec D.6 tooltip strings (verbatim) not covered by menuHeaderTooltip.
constexpr char kRestartTooltip[] = "Rebuild the scene from the current settings";
constexpr char kSeparationTooltip[] = "Distance between the bodies at t=0";
constexpr char kPathTooltip[] = "Circular orbit or unbound fly-by";
constexpr char kPlanetsTooltip[] = "How many planets (inside-out)";
constexpr char kPlutoTooltip[] = "Include Pluto at ~200 units";

int presetIndex(core::PresetName preset) {
    switch (preset) {
        case core::PresetName::TwoBody:
            return 0;
        case core::PresetName::SolarSystem:
            return 1;
        case core::PresetName::RandomCloud:
            return 2;
        case core::PresetName::FigureEight:
            return 3;
    }
    return -1;
}

int integratorIndex(core::IntegratorKind integrator) {
    switch (integrator) {
        case core::IntegratorKind::Rk4:
            return 0;
        case core::IntegratorKind::Euler:
            return 1;
    }
    return -1;
}

// Shortest honest rendering for option values: integers print bare
// ("separation: 10"), fractional values up to 4 significant digits
// ("separation: 10.5"). Field grammar caps input well below that.
std::string formatNumber(double value) {
    if (std::isfinite(value) && value == std::floor(value) &&
        std::fabs(value) < 1e15) {
        return std::to_string(static_cast<long long>(value));
    }
    std::ostringstream out;
    out << std::setprecision(4) << value;
    return out.str();
}

MenuEffect toggleHyperbolic(MenuState& state, bool paused) {
    auto& staged = MenuModelAccess::get(state).staged;
    if (paused) {
        // Flip the EFFECTIVE value: a pending staged flip first.
        staged.hyperbolic = !staged.hyperbolic.value_or(state.hyperbolic);
        return MenuEffect::Staged;
    }
    state.hyperbolic = !state.hyperbolic;
    staged.hyperbolic.reset();  // a live flip supersedes any staged flip
    return MenuEffect::PathChanged;
}

MenuEffect togglePluto(MenuState& state, bool paused) {
    auto& staged = MenuModelAccess::get(state).staged;
    if (paused) {
        staged.pluto = !staged.pluto.value_or(state.pluto);
        return MenuEffect::Staged;
    }
    state.pluto = !state.pluto;
    staged.pluto.reset();  // a live flip supersedes any staged flip
    return MenuEffect::PlutoChanged;
}

}  // namespace

bool MenuStaged::empty() const {
    return !preset && !integrator && !dt && !seed && !bodies &&
           !separation && !hyperbolic && !planets && !pluto;
}

MenuState::MenuState() : impl_(std::make_unique<Impl>()) {}

MenuState::~MenuState() = default;

MenuState::MenuState(const MenuState& other)
    : preset(other.preset),
      integrator(other.integrator),
      dt(other.dt),
      seed(other.seed),
      bodies(other.bodies),
      separation(other.separation),
      hyperbolic(other.hyperbolic),
      planetCount(other.planetCount),
      pluto(other.pluto),
      openGroup(other.openGroup),
      restartTick(other.restartTick),
      impl_(std::make_unique<Impl>(*other.impl_)) {}

MenuState& MenuState::operator=(const MenuState& other) {
    if (this != &other) {
        preset = other.preset;
        integrator = other.integrator;
        dt = other.dt;
        seed = other.seed;
        bodies = other.bodies;
        separation = other.separation;
        hyperbolic = other.hyperbolic;
        planetCount = other.planetCount;
        pluto = other.pluto;
        openGroup = other.openGroup;
        restartTick = other.restartTick;
        *impl_ = *other.impl_;
    }
    return *this;
}

MenuEffect menuApply(MenuState& state, const MenuAction& action,
                     bool paused) {
    auto& staged = MenuModelAccess::get(state).staged;

    switch (action.group) {
        case MenuGroup::Simulation: {
            // Row table (spec D.1): 0-3 presets, 4 separator, 5+ the
            // preset's option rows, then separator + restart.
            const int opts = menuPresetOptionCount(state.preset);
            if (action.index < 0 || action.index >= 7 + opts) {
                return MenuEffect::None;
            }
            if (action.index < 4) {
                if (paused) {
                    staged.preset =
                        static_cast<core::PresetName>(action.index);
                    return MenuEffect::Staged;
                }
                state.preset = static_cast<core::PresetName>(action.index);
                staged.preset.reset();  // live pick supersedes a staged pick
                return MenuEffect::PresetChanged;
            }
            if (action.index == 4 || action.index == 5 + opts) {
                return MenuEffect::None;  // separators
            }
            if (action.index == 6 + opts) {
                ++state.restartTick;
                return MenuEffect::SceneRestart;  // same effect as R
            }
            if (action.index == 6 && opts == 2) {
                if (state.preset == core::PresetName::TwoBody) {
                    return toggleHyperbolic(state, paused);
                }
                if (state.preset == core::PresetName::SolarSystem) {
                    return togglePluto(state, paused);
                }
            }
            // Field rows (separation/planets) focus the inline editor; the
            // app routes their edits back through the typed setters.
            return MenuEffect::None;
        }

        case MenuGroup::Integrator:
            if (action.index < 0 || action.index >= 2) {
                return MenuEffect::None;
            }
            if (paused) {
                staged.integrator = action.index == 0
                                        ? core::IntegratorKind::Rk4
                                        : core::IntegratorKind::Euler;
                return MenuEffect::Staged;
            }
            state.integrator = action.index == 0
                                   ? core::IntegratorKind::Rk4
                                   : core::IntegratorKind::Euler;
            staged.integrator.reset();  // live pick supersedes a staged pick
            return MenuEffect::IntegratorChanged;

        case MenuGroup::Physics:
            // dt list only: the separator (5) and restart (6) rows moved
            // into Simulation (spec D.1).
            if (action.index >= 0 && action.index < 5) {
                const double dt =
                    kDtOptions[static_cast<std::size_t>(action.index)].value;
                if (paused) {
                    staged.dt = dt;
                    return MenuEffect::Staged;
                }
                state.dt = dt;
                staged.dt.reset();  // live pick supersedes a staged pick
                return MenuEffect::DtChanged;
            }
            return MenuEffect::None;

        case MenuGroup::Cloud:
            // seed field / random seed / bodies field / random bodies: the
            // field commits and the roll buttons generate their values
            // app-side, then flow through menuSetSeed/menuSetBodies.
            return MenuEffect::None;

        case MenuGroup::Count_:
            return MenuEffect::None;
    }
    return MenuEffect::None;
}

MenuEffect menuSetSeed(MenuState& state, std::int64_t seed, bool paused) {
    if (paused) {
        MenuModelAccess::get(state).staged.seed = seed;
        return MenuEffect::Staged;
    }
    state.seed = seed;
    MenuModelAccess::get(state).staged.seed.reset();
    return MenuEffect::SeedChanged;
}

MenuEffect menuSetBodies(MenuState& state, std::uint32_t bodies,
                         bool paused) {
    if (paused) {
        MenuModelAccess::get(state).staged.bodies = bodies;
        return MenuEffect::Staged;
    }
    state.bodies = bodies;
    MenuModelAccess::get(state).staged.bodies.reset();
    return MenuEffect::BodiesChanged;
}

MenuEffect menuSetSeparation(MenuState& state, double separation,
                             bool paused) {
    if (paused) {
        MenuModelAccess::get(state).staged.separation = separation;
        return MenuEffect::Staged;
    }
    state.separation = separation;
    MenuModelAccess::get(state).staged.separation.reset();
    return MenuEffect::SeparationChanged;
}

MenuEffect menuSetPlanets(MenuState& state, int planets, bool paused) {
    if (paused) {
        MenuModelAccess::get(state).staged.planets = planets;
        return MenuEffect::Staged;
    }
    state.planetCount = planets;
    MenuModelAccess::get(state).staged.planets.reset();
    return MenuEffect::PlanetsChanged;
}

MenuEffect menuTogglePath(MenuState& state, bool paused) {
    return toggleHyperbolic(state, paused);
}

MenuEffect menuTogglePluto(MenuState& state, bool paused) {
    return togglePluto(state, paused);
}

bool menuGroupVisible(MenuGroup group, const MenuState& state) {
    return menuGroupVisible(group, state.preset);
}

int menuItemCount(MenuGroup group, const MenuState& state) {
    return menuItemCount(group, state.preset);
}

std::string menuItemLabel(const MenuState& state, MenuGroup group,
                          int index) {
    if (index < 0 || index >= menuItemCount(group, state)) {
        return {};
    }
    switch (group) {
        case MenuGroup::Simulation: {
            if (index < 4) {
                return menuPresetName(
                    static_cast<core::PresetName>(index));
            }
            const int opts = menuPresetOptionCount(state.preset);
            if (index == 4 || index == 5 + opts) {
                return {};  // separators
            }
            if (index == 6 + opts) {
                return "restart scene";
            }
            if (index == 5) {
                switch (state.preset) {
                    case core::PresetName::TwoBody:
                    case core::PresetName::FigureEight:
                        return "separation: " +
                               formatNumber(state.separation);
                    case core::PresetName::SolarSystem:
                        return "planets: " + std::to_string(state.planetCount);
                    case core::PresetName::RandomCloud:
                        break;
                }
                return {};
            }
            if (index == 6 && opts == 2) {
                if (state.preset == core::PresetName::TwoBody) {
                    return state.hyperbolic ? "path: hyperbola"
                                            : "path: orbit";
                }
                if (state.preset == core::PresetName::SolarSystem) {
                    return state.pluto ? "pluto: ✓" : "pluto: ✗";
                }
            }
            return {};
        }
        case MenuGroup::Integrator:
            return index == 0 ? "rk4" : "euler";
        case MenuGroup::Physics:
            return kDtOptions[static_cast<std::size_t>(index)].label;
        case MenuGroup::Cloud:
            switch (index) {
                case 0:
                    return "type a seed";  // row focuses the inline editor
                case 1:
                    return "random seed";
                case 2:
                    return "bodies: " + std::to_string(state.bodies);
                default:
                    return "random bodies";
            }
        case MenuGroup::Count_:
            return {};
    }
    return {};
}

std::string menuItemTooltip(MenuGroup group, int index,
                            const MenuState& state) {
    if (index < 0 || index >= menuItemCount(group, state)) {
        return {};
    }
    switch (group) {
        case MenuGroup::Simulation: {
            const int opts = menuPresetOptionCount(state.preset);
            if (index == 6 + opts) {
                return kRestartTooltip;
            }
            if (index < 4 || index == 4 || index == 5 + opts) {
                return {};  // preset rows and separators: none (D.6)
            }
            if (index == 5) {
                switch (state.preset) {
                    case core::PresetName::TwoBody:
                    case core::PresetName::FigureEight:
                        return kSeparationTooltip;
                    case core::PresetName::SolarSystem:
                        return kPlanetsTooltip;
                    case core::PresetName::RandomCloud:
                        break;
                }
                return {};
            }
            if (index == 6 && opts == 2) {
                if (state.preset == core::PresetName::TwoBody) {
                    return kPathTooltip;
                }
                if (state.preset == core::PresetName::SolarSystem) {
                    return kPlutoTooltip;
                }
            }
            return {};
        }
        case MenuGroup::Integrator:
        case MenuGroup::Physics:
            return {};  // dt/integrator rows have none (D.6)
        case MenuGroup::Cloud:
            switch (index) {
                case 0:
                    return "Type a seed value, press Enter";
                case 1:
                    return "Use a fresh random seed";
                case 2:
                    return "Number of bodies (2-2000)";
                default:
                    return "Use a random body count (2-2000)";
            }
        case MenuGroup::Count_:
            return {};
    }
    return {};
}

const MenuStaged& menuStaged(const MenuState& state) {
    return MenuModelAccess::get(state).staged;
}

MenuStaged menuTakeStaged(MenuState& state) {
    auto& model = MenuModelAccess::get(state);
    MenuStaged staged = model.staged;
    model.staged = {};
    model.summary.clear();
    return staged;
}

const char* menuStagedSummary(const MenuState& state) {
    const auto& model = MenuModelAccess::get(state);
    auto& summary = model.summary;
    summary.clear();
    bool hasPrevious = false;
    const auto append = [&summary, &hasPrevious](const std::string& value) {
        if (hasPrevious) {
            summary += ", ";
        }
        summary += value;
        hasPrevious = true;
    };

    // Contract order (Space-resume law): preset, integrator, dt, seed,
    // then the option clauses, restart-summary style (spec D.3).
    if (model.staged.preset) {
        append(std::string("preset → ") + menuPresetName(*model.staged.preset));
    }
    if (model.staged.integrator) {
        append(std::string("integrator → ") +
               (integratorIndex(*model.staged.integrator) == 0 ? "rk4"
                                                              : "euler"));
    }
    if (model.staged.dt) {
        for (const DtOption& option : kDtOptions) {
            if (*model.staged.dt == option.value) {
                append(std::string("dt → ") + option.label);
                break;
            }
        }
    }
    if (model.staged.seed) {
        append(std::string("seed → ") + std::to_string(*model.staged.seed));
    }
    if (model.staged.bodies) {
        append(std::string("bodies → ") + std::to_string(*model.staged.bodies));
    }
    if (model.staged.separation) {
        append(std::string("separation → ") +
               formatNumber(*model.staged.separation));
    }
    if (model.staged.hyperbolic) {
        append(std::string("path → ") +
               (*model.staged.hyperbolic ? "hyperbola" : "orbit"));
    }
    if (model.staged.planets) {
        append(std::string("planets → ") + std::to_string(*model.staged.planets));
    }
    if (model.staged.pluto) {
        append(std::string("pluto → ") + (*model.staged.pluto ? "on" : "off"));
    }
    return summary.c_str();
}

int menuTickedIndex(const MenuState& state, MenuGroup group) {
    switch (group) {
        case MenuGroup::Simulation:
            return presetIndex(state.preset);
        case MenuGroup::Integrator:
            return integratorIndex(state.integrator);
        case MenuGroup::Physics:
            for (std::size_t index = 0; index < kDtOptions.size(); ++index) {
                if (state.dt == kDtOptions[index].value) {
                    return static_cast<int>(index);
                }
            }
            return -1;
        case MenuGroup::Cloud:
            // Field/roll rows: no tick. Toggle state shows in the label
            // ("path: ...", "pluto: ..."), never as a tick row.
            return -1;
        case MenuGroup::Count_:
            return -1;
    }
    return -1;
}

bool menuIsStagedActive(const MenuState& state) {
    return !menuStaged(state).empty();
}

}  // namespace nbody::app
