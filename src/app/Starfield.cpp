#include "nbody/app/Starfield.hpp"

#include <random>

namespace nbody::app {

namespace {

// Constant seed (spec D.3): never clock-derived, so the field is identical
// every run and across calls.
constexpr std::mt19937::result_type kStarSeed = 0x57415A53U;
constexpr std::size_t kStarCount = 600;
constexpr double kStarExtent = 30000.0;  // world units, uniform +-this

// 4-tone star palette (spec D.3): white, blue-white, warm, pale blue.
constexpr std::uint8_t kStarPalette[4][3] = {
    {255, 255, 255},
    {200, 210, 255},
    {255, 235, 200},
    {170, 190, 255},
};

}  // namespace

std::vector<Star> starfield() {
    std::mt19937 rng(kStarSeed);  // fresh engine: call-order-independent
    std::uniform_real_distribution<double> extent(-kStarExtent, kStarExtent);
    std::uniform_int_distribution<int> paletteIndex(0, 3);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    std::vector<Star> stars;
    stars.reserve(kStarCount);
    for (std::size_t i = 0; i < kStarCount; ++i) {
        // Fixed draw order per star (x, y, color, size, dim): any change to
        // this order changes the field, so it stays exactly as written.
        Star star;
        star.position = core::Vec2{extent(rng), extent(rng)};
        const int color = paletteIndex(rng);
        star.r = kStarPalette[color][0];
        star.g = kStarPalette[color][1];
        star.b = kStarPalette[color][2];
        star.sizePx = unit(rng) < 0.70 ? std::uint8_t{1} : std::uint8_t{2};
        star.dim = unit(rng) < 0.40;
        stars.push_back(star);
    }
    return stars;
}

}  // namespace nbody::app
