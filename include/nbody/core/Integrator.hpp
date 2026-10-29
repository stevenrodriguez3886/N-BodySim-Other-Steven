#pragma once

#include <string_view>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/GravityField.hpp"

namespace nbody::core {

// Interface for fixed-dt state integrators (plan section B.2). Implementations
// must be stateless across calls except for reusable scratch buffers, and
// MUST NOT create or destroy bodies (collisions are the World's job).
class IIntegrator {
public:
    IIntegrator() = default;
    virtual ~IIntegrator() = default;
    IIntegrator(const IIntegrator&) = delete;
    IIntegrator& operator=(const IIntegrator&) = delete;

    // Advances every body by exactly dt using gravity for accelerations.
    virtual void step(std::vector<Body>& bodies,
                      const GravityField& gravity, double dt) = 0;
    virtual std::string_view name() const = 0;  // "euler" / "rk4"
};

}  // namespace nbody::core
