/// @file
/// The Body: one simulated point mass with a position, velocity, and purely
/// cosmetic rendering hints. Physics reads only id/mass/radius/position/
/// velocity; the nested Appearance struct exists for the renderer and is
/// never consulted by the simulation.

#pragma once

#include <cstdint>

#include "nbody/core/Vec2.hpp"

namespace nbody::core {

/// @brief One simulated body: point-mass state plus cosmetic look.
struct Body {
    /// @brief The renderer's per-body look; purely cosmetic.
    /// @details Physics never reads any of it — no branch in step/merge/
    ///   forces consults appearance; on a merge only the survivor object
    ///   (and thus its appearance) carries through. Default-constructed =
    ///   all-default, so 5-value brace inits keep compiling and behaving
    ///   identically.
    struct Appearance {
        std::uint32_t color = 0u;  // 0xRRGGBB; 0 = "use palette by id % 8"
        bool glow = false;         // star-style radial glow
        bool ring = false;         // Saturn-style elliptical ring
        std::uint8_t bands = 0;    // 0 none; 2-3 latitude stripes (gas giants)
    };

    // Constructors (rather than aggregate init) so existing 5-value sites
    // Body{id, mass, radius, position, velocity} stay warning-free under
    // -Wextra's -Wmissing-field-initializers; behaviour is unchanged: the
    // omitted appearance argument is the all-default Appearance.
    Body() = default;
    Body(std::uint64_t id, double mass, double radius, Vec2 position,
         Vec2 velocity)
        : Body(id, mass, radius, position, velocity, Appearance{}) {}
    Body(std::uint64_t id, double mass, double radius, Vec2 position,
         Vec2 velocity, Appearance appearance)
        : id(id),
          mass(mass),
          radius(radius),
          position(position),
          velocity(velocity),
          appearance(appearance) {}

    std::uint64_t id = 0;
    double mass = 1.0;    // must be > 0 in all presets
    double radius = 0.1;  // collision/visual radius, world units, > 0
    Vec2 position{};
    Vec2 velocity{};
    Appearance appearance;  // never read by physics
};

}  // namespace nbody::core
