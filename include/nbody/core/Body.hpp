#pragma once

#include <cstdint>

#include "nbody/core/Vec2.hpp"

namespace nbody::core {

struct Body {
    std::uint64_t id = 0;
    double mass = 1.0;    // must be > 0 in all presets
    double radius = 0.1;  // collision/visual radius, world units, > 0
    Vec2 position{};
    Vec2 velocity{};
};

}  // namespace nbody::core
