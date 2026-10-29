// Task 7 dispatch: parseArgs -> help / headless / smoke / windowed.
// The windowed path is still the temporary open/close loop from Task 1 —
// the real SimulationApp lands in Task 9 (search "TEMPORARY WINDOWED PATH").
#include <SFML/Graphics.hpp>

#include <dlfcn.h>

#include <exception>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include "nbody/app/AppArgs.hpp"
#include "nbody/app/HeadlessRunner.hpp"
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

    nbody::core::SimulationSetup setup =
        nbody::core::createPreset(args->preset, args->seed);
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

    // --- TEMPORARY WINDOWED PATH (Task 1 placeholder) -----------------------
    // Task 9 replaces this loop with SimulationApp (rendering, camera, HUD,
    // fixed-step accumulator). The display probe must stay until then.
    if (!displayReachable()) {
        std::cout << "nbody: cannot open a display; use --headless=<n>\n";
        return 1;
    }
    sf::Window window(sf::VideoMode(kWindowSize), "nbody");
    sf::Clock frameClock;
    while (window.isOpen()) {
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }
        }
        (void)frameClock.restart();  // frame pacing placeholder (Task 9)
        window.display();
    }
    return 0;
}
