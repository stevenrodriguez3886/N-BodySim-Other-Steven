/// @file
/// The integrator interface: polymorphic fixed-dt state advancement used by
/// PhysicsWorld, with "euler" and "rk4" as the concrete schemes.

#pragma once

#include <string_view>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/GravityField.hpp"

namespace nbody::core {

/// @brief Interface for fixed-dt state integrators.
/// @details Implementations must be stateless across calls except for
///   reusable scratch buffers, and MUST NOT create or destroy bodies
///   (collisions and merges are PhysicsWorld's job).
class IIntegrator {
public:
    IIntegrator() = default;
    virtual ~IIntegrator() = default;
    IIntegrator(const IIntegrator&) = delete;
    IIntegrator& operator=(const IIntegrator&) = delete;

    /// @brief Advance every body by exactly dt using gravity for
    ///        accelerations. Body count and order must be preserved.
    /// @param bodies State to advance, mutated in place.
    /// @param gravity Field providing the accelerations.
    /// @param dt Fixed step in simulation seconds (> 0).
    virtual void step(std::vector<Body>& bodies,
                      const GravityField& gravity, double dt) = 0;
    /// @brief Short lowercase name used by the UI/CLI ("euler", "rk4").
    virtual std::string_view name() const = 0;
};

}  // namespace nbody::core
