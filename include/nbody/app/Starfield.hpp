#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "nbody/app/Camera.hpp"

#include "nbody/core/Vec2.hpp"

namespace nbody::app {

// One world-locked background star (spec D.5, tiled procedural field).
// Deliberately SFML-free (like Camera.hpp) so the generator unit-tests run
// display-free in nbody_app_tests. Alpha is NOT stored here: the renderer
// draws dim stars at 120 alpha and bright ones at 255.
struct Star {
    core::Vec2 position{};      // world units, anywhere in the infinite field
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t sizePx = 1;    // screen-constant size, 1 or 2 px
    bool dim = false;           // ~40% of the field; dim => low alpha
};

// The 4-tone star palette (spec D.5): white, blue-white, warm, pale blue.
// Public so the unit tests can assert palette membership without duplicating
// the literals.
inline constexpr std::uint8_t kStarPalette[4][3] = {
    {255, 255, 255},
    {200, 210, 255},
    {255, 235, 200},
    {170, 190, 255},
};

// Infinite tiled procedural starfield (spec D.5). World space is divided in
// an axis-aligned grid of 2^level * 25-unit cells (global grid: cell
// boundaries depend only on the level, never on the queried rect, so panning
// is exact). splitmix64(cell ^ 0x57415A53) streams the per-cell decisions:
// star present (~kStarDensityNum/kStarDensityDenom of cells), intra-cell
// jitter, size 1-2 px (70/30), palette color, dim ~40%.
//
// The level doubles (deep zoom: halves) the cell size with the rect so the
// scan stays within the kStarMaxCells budget, holding the visible count
// ~=600 stars at every zoom: pan moves a subset of the same grid (star
// positions never shift), zoom pops between levels (documented, acceptable).
//
// Pure: the result depends on the rect alone — no clock, no RNG engine, no
// cross-call state. Returns stars whose positions lie inside [x0,x1]x[y0,y1]
// (inverted/degenerate rects yield empty). Order is deterministic (cell
// row-major), so two calls with equal extents return identical vectors.
std::vector<Star> starsInRect(double x0, double y0, double x1, double y1);

// Number of cells starsInRect would visit for the rect: exposed for the
// cell-budget unit test. Same purity guarantees as starsInRect.
std::size_t cellsScannedInRect(double x0, double y0, double x1, double y1);

// Axis-aligned rect (x0 <= x1, y0 <= y1), SFML-free like everything here.
struct Rect {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

// The world rect of the whole viewport (camera_.toWorld of the window
// corners) padded by a 2 px margin — the exact query SimulationApp::render
// issues each frame. Lives here, pure and SFML-free, so the y-flipped corner
// mapping (x from left/right pixels, y from bottom/top because pixel y runs
// down while world y runs up) is unit-testable without a display.
Rect viewportStarRect(const Camera& camera, double winW, double winH);

}  // namespace nbody::app
