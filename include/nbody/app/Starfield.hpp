#pragma once

#include <cstdint>
#include <vector>

#include "nbody/core/Vec2.hpp"

namespace nbody::app {

// One world-locked background star (spec D.3). Deliberately SFML-free (like
// Camera.hpp) so the generator unit-tests run display-free in nbody_app_tests.
// Alpha is NOT stored here: the renderer draws dim stars at ~120 alpha and
// bright ones at 255.
struct Star {
    core::Vec2 position{};      // world units, |coord| <= 30000
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t sizePx = 1;    // screen-constant size, 1 or 2 px
    bool dim = false;           // ~40% of the field; dim => low alpha
};

// Generates EXACTLY 600 stars from the constant seed 0x57415A53 (mt19937,
// fresh engine per call): positions uniform in [-30000, 30000]^2, colors from
// a 4-tone palette, sizePx 1/2 weighted ~70/30, dim ~40%. No wall-clock
// input and no cross-call state, so two calls return identical vectors.
std::vector<Star> starfield();

}  // namespace nbody::app
