// Camera transform tests: pure double math, no SFML, no display required.
// toPixels/toWorld must be written independently (plan D.3), so the roundtrip
// test here cross-checks the forward transform against its algebraic inverse.
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "nbody/app/Camera.hpp"

using Catch::Approx;
using nbody::app::Camera;
using nbody::core::Vec2;

namespace {

constexpr double kWinW = 800.0;
constexpr double kWinH = 600.0;
constexpr double kEps = 1e-9;

}  // namespace

TEST_CASE("camera_roundtrip") {
    // A small fixed pseudo-random spread of world points (deterministic).
    const Vec2 points[] = {
        {0.0, 0.0},       {1.0, -2.0},    {-3.5, 7.25},
        {0.1, 0.1},       {123.456, -0.007},
        {-987.6, 555.5},  {1e-4, -1e-4},
    };
    const double scales[] = {1.0, 50.0, 1000.0};
    const Vec2 centers[] = {{0.0, 0.0}, {2.5, -1.75}, {-100.0, 42.0}};
    for (const double s : scales) {
        for (const Vec2 c : centers) {
            Camera cam;
            cam.center = c;
            cam.scale = s;
            for (const Vec2 w : points) {
                const Vec2 back = cam.toWorld(cam.toPixels(w, kWinW, kWinH),
                                              kWinW, kWinH);
                CHECK(back.x == Approx(w.x).margin(kEps));
                CHECK(back.y == Approx(w.y).margin(kEps));
            }
        }
    }
}

TEST_CASE("camera_y_flipped") {
    const Camera cam;  // default: center (0,0), scale 50 (plan B.3)
    const Vec2 originPx = cam.toPixels({0.0, 0.0}, kWinW, kWinH);
    CHECK(originPx.x == Approx(kWinW / 2.0));
    CHECK(originPx.y == Approx(kWinH / 2.0));
    // +y in world must appear ABOVE center (smaller pixel y).
    const Vec2 above = cam.toPixels({0.0, 1.0}, kWinW, kWinH);
    CHECK(above.y < kWinH / 2.0);
    CHECK(above.y == Approx(kWinH / 2.0 - cam.scale));
}

TEST_CASE("camera_zoom_anchor") {
    for (const double ticks : {1.0, -1.0}) {
        Camera cam;
        cam.center = {2.5, -1.75};  // off-origin so the anchor isn't trivial
        cam.scale = 37.0;
        const Vec2 mouse{523.0, 181.0};
        const Vec2 wBefore = cam.toWorld(mouse, kWinW, kWinH);
        cam.zoomAt(mouse, ticks, kWinW, kWinH);
        CHECK(cam.scale == Approx(37.0 * std::pow(1.1, ticks)));
        const Vec2 wAfter = cam.toWorld(mouse, kWinW, kWinH);
        CHECK(wAfter.x == Approx(wBefore.x).margin(kEps));
        CHECK(wAfter.y == Approx(wBefore.y).margin(kEps));
    }
}

TEST_CASE("camera_pan") {
    Camera cam;
    cam.center = {1.0, 1.0};
    cam.scale = 4.0;
    cam.panByPixels({10.0, 0.0});
    CHECK(cam.center.x == Approx(1.0 - 10.0 / 4.0));
    CHECK(cam.center.y == Approx(1.0));
    // y-flip: dragging down (positive dy) moves the view down in world space,
    // i.e. center.y increases (plan D.3 / Task 8 dispatch).
    cam.panByPixels({0.0, 8.0});
    CHECK(cam.center.x == Approx(1.0 - 10.0 / 4.0));
    CHECK(cam.center.y == Approx(1.0 + 8.0 / 4.0));
}

TEST_CASE("camera_zoom_clamps") {
    {
        Camera cam;  // scale 50
        cam.zoomAt({400.0, 300.0}, 200.0, kWinW, kWinH);  // 50*1.1^200 >> 1e6
        CHECK(cam.scale <= 1e6);
        CHECK(cam.scale == Approx(1e6));
    }
    {
        Camera cam;
        cam.zoomAt({400.0, 300.0}, -200.0, kWinW, kWinH);  // 50/1.1^200 << 1e-3
        CHECK(cam.scale >= 1e-3);
        CHECK(cam.scale == Approx(1e-3));
    }
    {
        // Repeated single ticks must not leak past the clamp either.
        Camera cam;
        for (int i = 0; i < 200; ++i) {
            cam.zoomAt({123.0, 456.0}, 1.0, kWinW, kWinH);
        }
        CHECK(cam.scale <= 1e6);
        for (int i = 0; i < 400; ++i) {
            cam.zoomAt({123.0, 456.0}, -1.0, kWinW, kWinH);
        }
        CHECK(cam.scale >= 1e-3);
    }
}
