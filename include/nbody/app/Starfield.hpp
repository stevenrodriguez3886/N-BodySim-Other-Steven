/// @file
/// The infinite procedural background starfield: a pure, display-free
/// generator that derives a deterministic set of world-locked stars from
/// an axis-aligned world grid, plus the viewport query the renderer issues
/// each frame. Like Camera, everything here is SFML-free so the generator
/// unit-tests run without a display.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "nbody/app/Camera.hpp"

#include "nbody/core/Vec2.hpp"

namespace nbody::app {

/// @brief One world-locked background star in the tiled procedural field.
/// @details Alpha is deliberately absent: the renderer draws dim stars at
///   120 and bright ones at 255, so the flag below carries the intent.
struct Star {
    core::Vec2 position{};  ///< world units, anywhere in the infinite field
    std::uint8_t r = 255;   ///< red channel, 0-255, from kStarPalette
    std::uint8_t g = 255;   ///< green channel, 0-255, from kStarPalette
    std::uint8_t b = 255;   ///< blue channel, 0-255, from kStarPalette
    std::uint8_t sizePx = 1;  ///< screen-constant size, 1 or 2 px (not scaled)
    bool dim = false;  ///< ~40% of the field; dim stars render at low alpha
};

/// @brief The 4-tone star palette: white, blue-white, warm, pale blue.
/// @details Public so the unit tests can assert palette membership without
///   duplicating the literals; [index][channel], one row per tone.
inline constexpr std::uint8_t kStarPalette[4][3] = {
    {255, 255, 255},
    {200, 210, 255},
    {255, 235, 200},
    {170, 190, 255},
};

/// @brief The infinite tiled procedural starfield: query a world rect and
///   get every star inside it, generated from the grid alone.
/// @details World space is divided into an axis-aligned grid of
///   2^level * 25-unit cells. The grid is global — cell boundaries depend
///   only on the level, never on the queried rect, so panning is exact.
///   splitmix64 seeded from the cell coords and a fixed salt streams the
///   per-cell decisions: star present (~kStarDensityNum/
///   kStarDensityDenom of cells), intra-cell jitter, size 1-2 px (70/30),
///   palette color, dim ~40%.
///   The level doubles (deep zoom: halves) the cell size with the rect so
///   the scan stays within the kStarMaxCells budget, holding the visible
///   count near 600 stars at every zoom: pan moves a subset of the same
///   grid (star positions never shift), zoom pops between levels
///   (documented, acceptable).
///   Pure: the result depends on the rect alone — no clock, no RNG engine,
///   no cross-call state. Order is deterministic (cell row-major), so two
///   calls with equal extents return identical vectors.
/// @param x0 Left edge of the world rect.
/// @param y0 Bottom edge of the world rect (world y runs up).
/// @param x1 Right edge; must exceed x0.
/// @param y1 Top edge; must exceed y0.
/// @return Stars with positions inside [x0,x1]x[y0,y1]; inverted or
///         degenerate rects yield an empty vector.
std::vector<Star> starsInRect(double x0, double y0, double x1, double y1);

/// @brief Number of cells starsInRect would visit for the rect, exposed
///        for the cell-budget unit test.
/// @param x0 Left edge of the world rect.
/// @param y0 Bottom edge of the world rect.
/// @param x1 Right edge.
/// @param y1 Top edge.
/// @return Cells visited, saturated to 4 * kStarMaxCells; 0 for inverted or
///         degenerate rects. Same purity guarantees as starsInRect.
std::size_t cellsScannedInRect(double x0, double y0, double x1, double y1);

/// @brief An axis-aligned world rect, normalised so x0 <= x1 and
///        y0 <= y1. SFML-free like everything in this header.
struct Rect {
    double x0 = 0.0;  ///< min x, world units
    double y0 = 0.0;  ///< min y (bottom edge), world units
    double x1 = 0.0;  ///< max x, world units
    double y1 = 0.0;  ///< max y (top edge), world units
};

/// @brief The world rect of the whole viewport — Camera::toWorld of the
///   window corners — padded by a 2 px margin: the exact query the
///   renderer issues each frame.
/// @param camera The camera whose transform maps the window.
/// @param winW Window width in pixels.
/// @param winH Window height in pixels.
/// @return The padded world rect, normalised to x0 <= x1, y0 <= y1.
/// @note Kept here, pure and SFML-free, so the y-flipped corner mapping (x
///   from the left/right pixels, y from the bottom/top because pixel y
///   runs down while world y runs up) is unit-testable without a display.
Rect viewportStarRect(const Camera& camera, double winW, double winH);

}  // namespace nbody::app
