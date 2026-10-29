#pragma once

#include "nbody/core/Vec2.hpp"

namespace nbody::app {

// World <-> pixel transform (plan B.3/D.3). Deliberately SFML-free: all math
// is plain doubles over core::Vec2 so it is unit-testable without a display
// (tests/app/test_camera.cpp). The sf::Vector2f conversion helper `toSf` is
// intentionally NOT here — it would force an SFML include into this header;
// Task 9's SimulationApp.cpp defines it at its sole call site instead.
class Camera {
public:
    core::Vec2 center{0.0, 0.0};  // world point shown at window center
    double scale = 50.0;          // pixels per world unit, kept > 0 (zoomAt
                                  // clamps to [1e-3, 1e6])

    // pixel.x = (w.x - c.x) * s + W/2 ; pixel.y = H/2 - (w.y - c.y) * s
    core::Vec2 toPixels(core::Vec2 world, double winW, double winH) const;
    // Algebraic inverse of toPixels, written independently of it so the
    // roundtrip test cross-checks both code paths.
    core::Vec2 toWorld(core::Vec2 pixel, double winW, double winH) const;

    // Wheel zoom keeping the world point under the cursor fixed: s' =
    // s * 1.1^ticks clamped, center' = w - (w - center) * (s/s'), where w is
    // the cursor's world position at the OLD scale.
    void zoomAt(core::Vec2 mousePixel, double wheelTicks,
                double winW, double winH);

    // Drag pan (y-flipped): center.x -= dx/scale, center.y += dy/scale.
    void panByPixels(core::Vec2 dragPixels);
};

}  // namespace nbody::app
