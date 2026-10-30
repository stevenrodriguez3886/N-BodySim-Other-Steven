#include "nbody/app/MenuModel.hpp"

#include <array>
#include <limits>
#include <string>

namespace nbody::app {
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

}  // namespace

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

bool MenuStaged::empty() const {
    return !preset && !integrator && !dt && !seed;
}

MenuState::MenuState() : impl_(std::make_unique<Impl>()) {}

MenuState::~MenuState() = default;

MenuState::MenuState(const MenuState& other)
    : preset(other.preset),
      integrator(other.integrator),
      dt(other.dt),
      seed(other.seed),
      openGroup(other.openGroup),
      restartTick(other.restartTick),
      impl_(std::make_unique<Impl>(*other.impl_)) {}

MenuState& MenuState::operator=(const MenuState& other) {
    if (this != &other) {
        preset = other.preset;
        integrator = other.integrator;
        dt = other.dt;
        seed = other.seed;
        openGroup = other.openGroup;
        restartTick = other.restartTick;
        *impl_ = *other.impl_;
    }
    return *this;
}

MenuEffect menuApply(MenuState& state, const MenuAction& action, bool paused) {
    auto& staged = MenuModelAccess::get(state).staged;

    switch (action.group) {
        case MenuGroup::Preset:
            if (action.index < 0 || action.index >= 4) {
                return MenuEffect::None;
            }
            if (paused) {
                staged.preset = static_cast<core::PresetName>(action.index);
                return MenuEffect::Staged;
            }
            state.preset = static_cast<core::PresetName>(action.index);
            return MenuEffect::PresetChanged;

        case MenuGroup::Integrator:
            if (action.index < 0 || action.index >= 2) {
                return MenuEffect::None;
            }
            if (paused) {
                staged.integrator = action.index == 0 ? core::IntegratorKind::Rk4
                                                      : core::IntegratorKind::Euler;
                return MenuEffect::Staged;
            }
            state.integrator = action.index == 0 ? core::IntegratorKind::Rk4
                                                 : core::IntegratorKind::Euler;
            return MenuEffect::IntegratorChanged;

        case MenuGroup::Physics:
            if (action.index >= 0 && action.index < 5) {
                const double dt =
                    kDtOptions[static_cast<std::size_t>(action.index)].value;
                if (paused) {
                    staged.dt = dt;
                    return MenuEffect::Staged;
                }
                state.dt = dt;
                return MenuEffect::DtChanged;
            }
            if (action.index == 6) {
                ++state.restartTick;
                return MenuEffect::SceneRestart;
            }
            return MenuEffect::None;

        case MenuGroup::Seed: {
            if (action.index != 0 && action.index != 1) {
                return MenuEffect::None;
            }
            std::uint32_t seed = paused && staged.seed ? *staged.seed : state.seed;
            if (action.index == 0) {
                if (seed > 0) {
                    --seed;
                }
            } else {
                if (seed < std::numeric_limits<std::uint32_t>::max()) {
                    ++seed;
                }
            }
            if (paused) {
                staged.seed = seed;
                return MenuEffect::Staged;
            }
            state.seed = seed;
            return MenuEffect::SeedChanged;
        }

        case MenuGroup::Count_:
            return MenuEffect::None;
    }
    return MenuEffect::None;
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

    if (model.staged.preset) {
        append(std::string("preset → ") +
               menuItemLabel(MenuGroup::Preset,
                             presetIndex(*model.staged.preset), 0));
    }
    if (model.staged.integrator) {
        append(std::string("integrator → ") +
               menuItemLabel(MenuGroup::Integrator,
                             integratorIndex(*model.staged.integrator), 0));
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
    return summary.c_str();
}

int menuTickedIndex(const MenuState& state, MenuGroup group) {
    switch (group) {
        case MenuGroup::Preset:
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
        case MenuGroup::Seed:
        case MenuGroup::Count_:
            return -1;
    }
    return -1;
}

bool menuIsStagedActive(const MenuState& state) {
    return !menuStaged(state).empty();
}

}  // namespace nbody::app
