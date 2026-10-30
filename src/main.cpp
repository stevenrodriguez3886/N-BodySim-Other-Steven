// Task 9 dispatch: parseArgs -> help / headless / smoke / windowed. The
// windowed path now constructs the real SimulationApp; the display probe
// stays (SFML aborts uncatchably without an X display, and SimulationApp's
// ctor opens a window, so the probe must gate construction).
#include <SFML/Graphics.hpp>

#include <dlfcn.h>

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "nbody/app/AppArgs.hpp"
#include "nbody/app/HeadlessRunner.hpp"
#include "nbody/app/SimulationApp.hpp"
#include "nbody/core/Presets.hpp"

namespace {
constexpr sf::Vector2u kWindowSize{800U, 600U};

// SFML calls std::abort() (uncatchable) when it cannot open the X display, so
// probe availability first via dlopen'd XOpenDisplay — no hard X11 link
// dependency, and a NULL DISPLAY simply fails the probe.
bool displayReachable() {
    void* handle = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        return true;  // cannot probe; let SFML try and report
    }
    using OpenFn = void* (*)(const char*);
    using CloseFn = int (*)(void*);
    const auto openDisplay =
        reinterpret_cast<OpenFn>(dlsym(handle, "XOpenDisplay"));
    const auto closeDisplay =
        reinterpret_cast<CloseFn>(dlsym(handle, "XCloseDisplay"));
    bool reachable = true;
    if (openDisplay != nullptr) {
        void* d = openDisplay(nullptr);
        reachable = d != nullptr;
        if (d != nullptr && closeDisplay != nullptr) {
            closeDisplay(d);
        }
    }
    dlclose(handle);
    return reachable;
}

// Smoke check (nbody --smoke, the nbody_smoke ctest entry): must exit 0 with
// or without a display — if no display can be opened (SFML would abort),
// report headless-ok and succeed. Window ctor exceptions are caught too.
int runSmoke() {
    if (!displayReachable()) {
        std::cout << "SMOKE headless-ok\n";
        return 0;
    }
    try {
        sf::Window window(sf::VideoMode(kWindowSize), "nbody (smoke)");
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            }
        }
        window.close();
        std::cout << "SMOKE ok\n";
    } catch (const std::exception&) {
        std::cout << "SMOKE headless-ok\n";
    }
    return 0;
}

// CLI token spellings, reused for the windowed path's title/HUD prefix
// ("<preset> | <integrator>", per plan D.2). SimulationApp appends the live
// FPS/UPS/body/time stats to this base.
std::string windowTitle(const nbody::app::AppArgs& a) {
    std::string_view preset = "two-body";
    switch (a.preset) {
        case nbody::core::PresetName::TwoBody:
            preset = "two-body";
            break;
        case nbody::core::PresetName::SolarSystem:
            preset = "solar-system";
            break;
        case nbody::core::PresetName::RandomCloud:
            preset = "random-cloud";
            break;
        case nbody::core::PresetName::FigureEight:
            preset = "figure-eight";
            break;
    }
    const std::string_view integrator =
        a.integrator == nbody::core::IntegratorKind::Euler ? "euler" : "rk4";
    return std::string(preset) + " | " + std::string(integrator);
}
}  // namespace

int main(int argc, char** argv) {
    const std::optional<nbody::app::AppArgs> args =
        nbody::app::parseArgs(argc, argv);
    if (!args.has_value()) {
        return 2;  // usage already printed by parseArgs
    }
    if (args->helpRequested) {
        nbody::app::printUsage(std::cout);
        return 0;
    }

    // --bodies applies to random-cloud only (clamped [2,2000] by core too).
    nbody::core::SimulationSetup setup =
        args->preset == nbody::core::PresetName::RandomCloud
            ? nbody::core::randomCloud(args->seed, args->bodies)
            : nbody::core::createPreset(args->preset, args->seed);
    if (args->dt.has_value()) {
        setup.config.timeStep = *args->dt;
    }

    if (args->headlessSteps > 0) {
        return nbody::app::runHeadless(std::move(setup), args->integrator,
                                       args->headlessSteps);
    }

    if (args->smokeTest) {
        return runSmoke();
    }

    // --- Windowed path: the real interactive app (Task 9) -------------------
    // The display probe stays in main.cpp: SimulationApp's ctor opens a
    // window, and SFML abort()s uncatchably without an X display.
    if (!displayReachable()) {
        std::cout << "nbody: cannot open a display; use --headless=<n>\n";
        return 1;
    }
    const std::string title = windowTitle(*args);
    // The windowed app now carries the full signed int64 seed in MenuState
    // (spec D.3); the scene itself is already built from it via the setup
    // above.
    nbody::app::SimulationApp app{std::move(setup), args->preset, args->seed,
                                  args->bodies, args->integrator, title};
    return app.run();
}
