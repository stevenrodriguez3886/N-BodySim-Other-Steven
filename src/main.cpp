// Task 1 temporary main: opens an 800x600 window and runs the SFML 3 pollEvent
// pattern. Full CLI parsing and app dispatch arrive in Task 7.
#include <SFML/Graphics.hpp>

#include <dlfcn.h>

#include <exception>
#include <iostream>
#include <optional>
#include <string_view>

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
}  // namespace

int main(int argc, char** argv) {
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--smoke") {
            smoke = true;
        }
    }

    if (smoke) {
        // Must exit 0 with or without a display: if no display can be opened
        // (SFML would abort), report headless-ok and succeed. Window ctor
        // exceptions are caught too, per plan.
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
        (void)frameClock.restart();  // frame pacing placeholder (Task 6+)
        window.display();
    }
    return 0;
}
