/// @file
/// The 2-D camera: an affine world <-> screen-pixel transform over plain
/// doubles, with the zoom and pan laws that keep it invertible. Deliberately
/// SFML-free (like the rest of the app's geometry) so the math is
/// unit-testable without a display; the sf::Vector2f conversion helper lives
/// at its single render call site rather than forcing an SFML include into
/// this header.

#pragma once

#include "nbody/core/Vec2.hpp"

namespace nbody::app {

/// @brief World <-> pixel transform: a center point and a pixel-per-world-unit
///   scale, mapping world space onto a window of the given size.
/// @details Convention: world y runs up while pixel y runs down, so every
///   transform flips the y axis; the window center shows `center`.
class Camera {
public:
    core::Vec2 center{0.0, 0.0};  ///< world point shown at the window center
    double scale = 50.0;  ///< pixels per world unit; always > 0 (zoomAt
                          ///< clamps it to [1e-3, 1e6])

    /// @brief World point -> screen pixel: x = (w.x - c.x) * s + W/2,
    ///        y = H/2 - (w.y - c.y) * s (world-up maps to pixel-down).
    /// @param world Point in world units.
    /// @param winW Window width in pixels.
    /// @param winH Window height in pixels.
    /// @return The pixel position, origin at the window's top-left.
    core::Vec2 toPixels(core::Vec2 world, double winW, double winH) const;

    /// @brief Screen pixel -> world point.
    /// @param pixel Pixel position, origin at the window's top-left.
    /// @param winW Window width in pixels.
    /// @param winH Window height in pixels.
    /// @return The world point that toPixels would map to `pixel`.
    /// @note Written as the algebraic inverse of toPixels but derived
    ///   independently of it, so the roundtrip test cross-checks the two
    ///   code paths rather than trusting one formula twice.
    core::Vec2 toWorld(core::Vec2 pixel, double winW, double winH) const;

    /// @brief Wheel zoom keeping the world point under the cursor fixed:
    ///   scale' = scale * 1.1^ticks (clamped), then
    ///   center' = w - (w - center) * (scale/scale'), where w is the
    ///   cursor's world position at the OLD scale.
    /// @param mousePixel Cursor position in window pixels.
    /// @param wheelTicks Wheel notches; positive zooms in.
    /// @param winW Window width in pixels.
    /// @param winH Window height in pixels.
    /// @note The [1e-3, 1e6] clamp keeps scale > 0, which every other
    ///   transform and the renderer's radius * scale rely on; 1.1 per notch
    ///   spans the 1e9-fold range in about 2200 wheel clicks.
    void zoomAt(core::Vec2 mousePixel, double wheelTicks,
                double winW, double winH);

    /// @brief Drag pan: shifts the center by the drag in world units,
    ///   y-flipped (center.x -= dx/scale, center.y += dy/scale, since pixel
    ///   y grows downward).
    /// @param dragPixels Drag delta in screen pixels.
    void panByPixels(core::Vec2 dragPixels);
};

}  // namespace nbody::app
