/// @file test_starfield.cpp
/// @brief Deterministic procedural starfield tests: purity, pan-subset
/// stability, density, cell budget, dim ratio, palette, and the viewport
/// star-rect corner math.
#include "nbody/app/Starfield.hpp"

#include <catch2/catch_test_macros.hpp>

#include "nbody/app/Camera.hpp"

#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

using nbody::app::cellsScannedInRect;
using nbody::app::kStarPalette;
using nbody::app::Star;
using nbody::app::starsInRect;
using nbody::app::viewportStarRect;

namespace {

/// @brief Compares two star vectors star by star, field by field.
/// @param a First star vector.
/// @param b Second star vector.
/// @return True only when the vectors agree on position, colour, size and
/// dim for every star (and have equal size).
bool sameStars(const std::vector<Star>& a, const std::vector<Star>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const Star& s = a[i];
        const Star& t = b[i];
        if (s.position.x != t.position.x || s.position.y != t.position.y ||
            s.r != t.r || s.g != t.g || s.b != t.b ||
            s.sizePx != t.sizePx || s.dim != t.dim) {
            return false;
        }
    }
    return true;
}

/// @brief True when the star's position lies within the closed rect.
/// @param s The star to test.
/// @param x0 Rect left edge (world units).
/// @param y0 Rect bottom edge (world units).
/// @param x1 Rect right edge (world units).
/// @param y1 Rect top edge (world units).
/// @return True when the star's position is inside (inclusive).
bool inside(const Star& s, double x0, double y0, double x1, double y1) {
    return s.position.x >= x0 && s.position.x <= x1 &&
           s.position.y >= y0 && s.position.y <= y1;
}

}  // namespace

/// @brief The same rect always yields the identical star vector.
TEST_CASE("starfield_purity") {
    // Same rect (and same level) => identical vector: splitmix64 is a pure
    // function of the cell, no clock, no cross-call state.
    const auto first = starsInRect(-123.0, -77.0, 27.0, 38.0);
    const auto second = starsInRect(-123.0, -77.0, 27.0, 38.0);
    CHECK(sameStars(first, second));
    CHECK_FALSE(first.empty());  // a nonzero field, not trivially pure
}

/// @brief A panned query reproduces the overlap stars bit-for-bit. The
/// comparison is restricted to the shared cell coverage of the two rects.
TEST_CASE("starfield_pan_subset_exact") {
    // Pan-subset stability: the cell grid is global (level depends
    // on extents alone), so stars inside the overlap of a panned view are
    // BIT-identical. Compared inside shared coverage: a query only
    // guarantees stars in cells it scans (rect + one-cell halo), so the big
    // rect must cover the shifted rect's right halo too.
    const std::vector<Star> big = starsInRect(-10.0, -10.0, 170.0, 140.0);
    const std::vector<Star> shifted = starsInRect(7.0, 13.0, 157.0, 128.0);
    std::vector<Star> filtered;
    for (const Star& s : big) {
        if (inside(s, 7.0, 13.0, 157.0, 128.0)) {
            filtered.push_back(s);
        }
    }
    CHECK_FALSE(shifted.empty());
    // Every shifted star inside the big rect's own area must be present
    // bit-identically in the big query (order is row-major cell order in
    // both, so the containment-filtered lists match exactly).
    std::vector<Star> shared;
    for (const Star& s : shifted) {
        if (inside(s, -10.0, -10.0, 170.0, 140.0)) {
            shared.push_back(s);
        }
    }
    CHECK(shared.size() > shifted.size() / 2);
    CHECK(sameStars(shared, filtered));
}

/// @brief A screen-sized rect yields a plausible star count.
TEST_CASE("starfield_density_band") {
    // A typical screen-sized rect (150 x 115 world units at the default
    // viewing scale): wide 150..1500 band, real value ~300 (the density
    // ratio kStarDensityNum/kStarDensityDenom is tunable).
    const std::vector<Star> stars = starsInRect(0.0, 0.0, 150.0, 115.0);
    CHECK(stars.size() >= 150);
    CHECK(stars.size() <= 1500);
}

/// @brief The solar-system screen scans a sane cell count within budget.
TEST_CASE("starfield_cells_fit_solar_screen") {
    // Solar-system preset screen (800x600 px, Pluto fit => scale ~1.14,
    // world rect ~700x525) must scan a sane cell count and stay inside the
    // 30000-cell budget.
    const std::size_t cells = cellsScannedInRect(0.0, 0.0, 700.0, 525.0);
    CHECK(cells >= 2400);
    CHECK(cells <= 30000);
}

/// @brief Huge rects stay inside the cell budget via level doubling.
TEST_CASE("starfield_cell_budget_huge_rect") {
    // The level doubling keeps huge rects affordable: <= 30000 cells.
    CHECK(cellsScannedInRect(0.0, 0.0, 1.0e9, 1.0e9) <= 30000);
    CHECK(cellsScannedInRect(-5.0e11, 2.0e11, 5.0e11, 8.0e11) <= 30000);
    // starsInRect can never out-scan its own cell budget.
    const std::vector<Star> stars = starsInRect(0.0, 0.0, 1.0e9, 1.0e9);
    CHECK(stars.size() <=
          cellsScannedInRect(0.0, 0.0, 1.0e9, 1.0e9));
}

/// @brief Roughly 40% of stars carry the dim flag.
TEST_CASE("starfield_dim_ratio") {
    // dim ~40% by design; measured on two disjoint rects, band 30..50%.
    const auto small = starsInRect(0.0, 0.0, 150.0, 115.0);
    const auto large = starsInRect(4.0e7, 9.0e7, 4.007e7, 9.0115e7);
    CHECK(small.size() >= 100);
    CHECK(large.size() >= 100);
    std::size_t dimmed = 0;
    for (const Star& s : small) {
        if (s.dim) {
            ++dimmed;
        }
    }
    for (const Star& s : large) {
        if (s.dim) {
            ++dimmed;
        }
    }
    const double ratio = static_cast<double>(dimmed) /
                         static_cast<double>(small.size() + large.size());
    CHECK(ratio >= 0.30);
    CHECK(ratio <= 0.50);
}

/// @brief Stars are 1-2 px and drawn only in palette tones.
TEST_CASE("starfield_sizes_and_palette") {
    const std::vector<Star> stars = starsInRect(0.0, 0.0, 150.0, 115.0);
    std::set<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>> colors;
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
    }
    CHECK(hasSize1);  // both sizes present
    CHECK(hasSize2);
    // Colors must all come from the public palette (exactly four tones),
    // and the field must actually show more than one of them.
    std::set<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>> palette;
    for (const auto& rgb : kStarPalette) {
        palette.emplace(rgb[0], rgb[1], rgb[2]);
    }
    CHECK(palette.size() == 4);
    CHECK(colors.size() >= 2);
    CHECK(colors.size() <= palette.size());
    for (const auto& color : colors) {
        CHECK(palette.count(color) == 1);
    }
}

/// @brief Degenerate or inverted rects return no stars, never a hang.
TEST_CASE("starfield_empty_rect") {
    // Degenerate/inverted rects are safe (no infinite scan).
    CHECK(starsInRect(5.0, 5.0, 5.0, 5.0).empty());
    CHECK(starsInRect(10.0, 10.0, 0.0, 0.0).empty());
}

/// @brief The viewport star rect spans the whole screen, corners chosen
/// with the y-flip in mind, and stars reach its right/top edges.
TEST_CASE("starfield_viewport_rect_spans_screen") {
    // Regression pin for SimulationApp::render's corner math: with y-flipped
    // toWorld, the viewport rect is (tl.x, br.y) .. (br.x, tl.y) -- using
    // topLeft.x for x1 or bottomRight.y for y1 collapses it to a sliver and
    // the field vanishes. Mirrors the default solar camera (800x600,
    // extent 46.2 => scale 0.4*600/46.2 ~ 5.195, world ~154x115).
    nbody::app::Camera cam;
    cam.center = {0.0, 0.0};
    cam.scale = 5.195;
    const nbody::app::Rect view = viewportStarRect(cam, 800.0, 600.0);
    CHECK(view.x0 < view.x1);  // sanity: not inverted
    CHECK(view.y0 < view.y1);
    CHECK(view.x1 - view.x0 > 150.0);  // full 800 px width at scale ~5.2
    CHECK(view.y1 - view.y0 > 110.0);  // full 600 px height
    const auto stars = starsInRect(view.x0, view.y0, view.x1, view.y1);
    CHECK(stars.size() >= 150);   // a full screen, not a sliver
    CHECK(stars.size() <= 1500);
    // Coverage must reach the RIGHT and TOP edges: a sliver rect built from
    // the wrong corner pair still yields ~600 stars (the level is
    // extent-driven), all of them bunched on one edge.
    std::size_t rightBand = 0;
    std::size_t topBand = 0;
    for (const Star& s : stars) {
        if (s.position.x >= view.x1 - 5.0 && s.position.x <= view.x1) {
            ++rightBand;
        }
        if (s.position.y >= view.y1 - 5.0 && s.position.y <= view.y1) {
            ++topBand;
        }
    }
    CHECK(rightBand > 0);
    CHECK(topBand > 0);
}
