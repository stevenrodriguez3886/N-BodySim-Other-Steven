#include "nbody/app/Starfield.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "nbody/app/Camera.hpp"

namespace nbody::app {
namespace {

// Spec D.5 constants. The density pair is the one-line tunable: raising the
// numerator (or lowering the denominator) thickens the field everywhere.
constexpr std::uint64_t kStarHashSeed = 0x57415A53ULL;  // "STARS"
constexpr double kStarBaseCell = 25.0;  // world-unit cell at level 0
// Star presence: kStarDensityNum/kStarDensityDenom of the scanned cells
// carry a star (~25%, spec D.5).
constexpr std::uint64_t kStarDensityNum = 1;
constexpr std::uint64_t kStarDensityDenom = 4;
// Cells per query: the level (up OR down, powers of two) is always chosen
// so the scan fits this budget, which pins the on-screen star count to
// ~kStarMaxCells * num/denom ~= 600-1200 at EVERY zoom. One-line tunable
// together with the density pair; 1/6 of the 30000-cell/frame budget.
constexpr std::uint64_t kStarMaxCells = 4800;

// Canonical splitmix64 finalizer (Steele/Lea/Venson): a pure 64->64-bit
// bijection; iterating it with the increment forms a deterministic stream.
std::uint64_t splitmix64(std::uint64_t& x) {
    x += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Cell edge for a rect's extents: the cell of the smallest level (which may
// be negative, shrinking below the base cell at deep zoom) whose rect scan
// fits kStarMaxCells. The level depends on the extents ALONE, so panning
// (equal extents) always re-queries the same global grid.
double cellSizeFor(double dx, double dy) {
    if (!(dx > 0.0) || !(dy > 0.0)) {
        return kStarBaseCell;
    }
    auto cellsAt = [&](int l) {
        const double cell = kStarBaseCell * std::ldexp(1.0, l);
        return (dx / cell + 1.0) * (dy / cell + 1.0);
    };
    // level = ceil(log2(sqrt(cellsAt(0) / kStarMaxCells))); the frexp-free
    // estimate is corrected by the refinement loops so it stays minimal.
    const double cellsAt0 = (dx / kStarBaseCell + 1.0) *
                            (dy / kStarBaseCell + 1.0);
    double guess = std::log2(std::sqrt(cellsAt0 /
                                       static_cast<double>(kStarMaxCells)));
    if (!(guess > -64.0)) {
        guess = 64.0;  // absurd input: keep the int cast well-defined
    }
    if (!(guess < 64.0)) {
        guess = 64.0;
    }
    int level = static_cast<int>(std::ceil(guess));
    while (level > -20 &&
           cellsAt(level - 1) <= static_cast<double>(kStarMaxCells)) {
        --level;  // a finer grid still fits the budget
    }
    while (cellsAt(level) > static_cast<double>(kStarMaxCells)) {
        ++level;  // budget blown (log/float slop)
    }
    return kStarBaseCell * std::ldexp(1.0, level);
}

// Cells starsInRect visits along one axis: the covered range plus a one-cell
// halo, since a star jitters strictly inside its own cell (offset in [0,1)
// cell widths).
bool cellRange(double x0, double x1, double cell, std::int64_t& first,
               std::int64_t& last) {
    if (!(x1 > x0)) {
        return false;
    }
    first = static_cast<std::int64_t>(std::floor(x0 / cell)) - 1;
    last = static_cast<std::int64_t>(std::floor(x1 / cell)) + 1;
    return last >= first;
}

struct CellGrid {
    double cell = kStarBaseCell;
    std::int64_t xFirst = 0;
    std::int64_t xLast = -1;
    std::int64_t yFirst = 0;
    std::int64_t yLast = -1;
};

bool gridForRect(double x0, double y0, double x1, double y1, CellGrid& out) {
    if (!(x1 > x0) || !(y1 > y0)) {
        return false;
    }
    out.cell = cellSizeFor(x1 - x0, y1 - y0);
    return cellRange(x0, x1, out.cell, out.xFirst, out.xLast) &&
           cellRange(y0, y1, out.cell, out.yFirst, out.yLast);
}

}  // namespace

std::size_t cellsScannedInRect(double x0, double y0, double x1, double y1) {
    CellGrid grid;
    if (!gridForRect(x0, y0, x1, y1, grid)) {
        return 0;
    }
    const std::uint64_t nx =
        static_cast<std::uint64_t>(grid.xLast - grid.xFirst + 1);
    const std::uint64_t ny =
        static_cast<std::uint64_t>(grid.yLast - grid.yFirst + 1);
    // cellSizeFor bounds the product; saturate to stay overflow-safe.
    return nx > kStarMaxCells * 4 || ny > kStarMaxCells * 4
               ? static_cast<std::size_t>(kStarMaxCells) * 4
               : static_cast<std::size_t>(nx * ny);
}

Rect viewportStarRect(const Camera& camera, double winW, double winH) {
    // 2 px cull margin, same as the per-star pixel cull in render, in world
    // units (scale = pixels per world unit).
    const double pad = 2.0 / camera.scale;
    const core::Vec2 tl = camera.toWorld(core::Vec2{0.0, 0.0}, winW, winH);
    const core::Vec2 br = camera.toWorld(core::Vec2{winW, winH}, winW, winH);
    // tl = left/top pixel = MIN x, MAX y (world y runs up, pixel y runs
    // down); br = MAX x, MIN y. Min corner (tl.x, br.y), max corner
    // (br.x, tl.y) -- pairing tl.x with tl.y yields a left-edge sliver.
    return Rect{tl.x - pad, br.y - pad, br.x + pad, tl.y + pad};
}

std::vector<Star> starsInRect(double x0, double y0, double x1, double y1) {
    std::vector<Star> stars;
    CellGrid grid;
    if (!gridForRect(x0, y0, x1, y1, grid)) {
        return stars;
    }
    const double cell = grid.cell;
    stars.reserve(static_cast<std::size_t>(
        (grid.xLast - grid.xFirst + 1) * (grid.yLast - grid.yFirst + 1) /
        kStarDensityDenom + 1));
    for (std::int64_t cy = grid.yFirst; cy <= grid.yLast; ++cy) {
        for (std::int64_t cx = grid.xFirst; cx <= grid.xLast; ++cx) {
            // Coordinate mix is the canonical cant pair (asymmetric
            // multipliers), then the D.5 constant folds in.
            std::uint64_t s = (static_cast<std::uint64_t>(cx) *
                                   0x9E3779B97F4A7C15ULL) ^
                              (static_cast<std::uint64_t>(cy) *
                                   0xC2B2AE3D27D4EB4FULL) ^
                              kStarHashSeed;
            // Fixed field order (present, jx, jy, color, size, dim): any
            // change reshuffles the sky, so it stays as written.
            if (splitmix64(s) % kStarDensityDenom >= kStarDensityNum) {
                continue;  // empty cell (~75%)
            }
            const std::uint64_t jx = splitmix64(s);
            const std::uint64_t jy = splitmix64(s);
            const std::uint64_t colorPick = splitmix64(s);
            const std::uint64_t sizePick = splitmix64(s);
            const std::uint64_t dimPick = splitmix64(s);

            Star star;
            // Canonical splitmix64 next_double: top 53 bits over 2^53 gives
            // a jitter in [0,1) cell widths (2^20 would let a star escape
            // its cell and break the halo bound).
            star.position = core::Vec2{
                static_cast<double>(cx) * cell +
                    static_cast<double>(jx >> 11) / 9007199254740992.0 * cell,
                static_cast<double>(cy) * cell +
                    static_cast<double>(jy >> 11) / 9007199254740992.0 * cell};
            const std::uint8_t color =
                static_cast<std::uint8_t>(colorPick & 3U);
            star.r = kStarPalette[color][0];
            star.g = kStarPalette[color][1];
            star.b = kStarPalette[color][2];
            star.sizePx = (sizePick % 10U) < 7U ? std::uint8_t{1}
                                                : std::uint8_t{2};
            star.dim = (dimPick & 31U) < 13U;  // ~40%
            if (star.position.x < x0 || star.position.x > x1 ||
                star.position.y < y0 || star.position.y > y1) {
                continue;  // halo cell, star outside the rect
            }
            stars.push_back(star);
        }
    }
    return stars;
}

}  // namespace nbody::app
