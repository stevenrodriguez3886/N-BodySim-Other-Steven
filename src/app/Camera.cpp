#include "nbody/app/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace nbody::app {
namespace {

// Zoom bounds enforced inside zoomAt so scale can never reach 0 (Task 9
// render relies on scale > 0 for radius*scale).
constexpr double kMinScale = 1e-3;
constexpr double kMaxScale = 1e6;
constexpr double kWheelFactor = 1.1;

}  // namespace

core::Vec2 Camera::toPixels(core::Vec2 world, double winW, double winH) const {
    return core::Vec2{(world.x - center.x) * scale + winW / 2.0,
                      winH / 2.0 - (world.y - center.y) * scale};
}

core::Vec2 Camera::toWorld(core::Vec2 pixel, double winW, double winH) const {
    // Written as the independent algebraic inverse of toPixels (plan D.3);
    // camera_roundtrip cross-checks the two implementations.
    return core::Vec2{center.x + (pixel.x - winW / 2.0) / scale,
                      center.y + (winH / 2.0 - pixel.y) / scale};
}

void Camera::zoomAt(core::Vec2 mousePixel, double wheelTicks,
                    double winW, double winH) {
    const double newScale = std::clamp(
        scale * std::pow(kWheelFactor, wheelTicks), kMinScale, kMaxScale);
    // Cursor's world position at the OLD scale, then solve for the center
    // that maps it back to the same pixel at the new scale.
    const core::Vec2 w = toWorld(mousePixel, winW, winH);
    const double ratio = scale / newScale;
    center = w - (w - center) * ratio;
    scale = newScale;
}

void Camera::panByPixels(core::Vec2 dragPixels) {
    // x: dragging right shifts the view left in world space.
    // y: pixel y grows downward, so a downward drag increases center.y.
    center.x -= dragPixels.x / scale;
    center.y += dragPixels.y / scale;
}

}  // namespace nbody::app
