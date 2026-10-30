#include "nbody/app/Starfield.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

using nbody::app::Star;
using nbody::app::starfield;

TEST_CASE("star_count_and_bounds") {
    const std::vector<Star> stars = starfield();
    REQUIRE(stars.size() == 600);
    for (const Star& star : stars) {
        CHECK(std::abs(star.position.x) <= 30000.0);
        CHECK(std::abs(star.position.y) <= 30000.0);
    }
}

TEST_CASE("star_determinism") {
    // Two calls must return identical vectors: fresh fixed-seed engine per
    // call, no wall-clock input, no shared/order-dependent state.
    const std::vector<Star> first = starfield();
    const std::vector<Star> second = starfield();
    REQUIRE(first.size() == second.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        CHECK(first[index].position.x == second[index].position.x);
        CHECK(first[index].position.y == second[index].position.y);
        CHECK(first[index].r == second[index].r);
        CHECK(first[index].g == second[index].g);
        CHECK(first[index].b == second[index].b);
        CHECK(first[index].sizePx == second[index].sizePx);
        CHECK(first[index].dim == second[index].dim);
    }
}

TEST_CASE("star_variety") {
    const std::vector<Star> stars = starfield();
    std::set<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>> colors;
    std::size_t dimmed = 0;
    bool hasSize1 = false;
    bool hasSize2 = false;
    for (const Star& star : stars) {
        colors.emplace(star.r, star.g, star.b);
        if (star.sizePx == 1) {
            hasSize1 = true;
        } else if (star.sizePx == 2) {
            hasSize2 = true;
        } else {
            FAIL("star size outside {1,2}: "
                 << static_cast<int>(star.sizePx));
        }
        if (star.dim) {
            ++dimmed;
        }
    }
    CHECK(colors.size() >= 2);
    CHECK(hasSize1);  // both sizes present
    CHECK(hasSize2);
    CHECK(dimmed >= 50);
}