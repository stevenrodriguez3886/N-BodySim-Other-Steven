/// @file
/// PhysicsWorld: owns the body list, gravity field, and integrator, and
/// advances the simulation in fixed steps followed by inelastic collision
/// merges. The single simulation entry point every runner (headless or
/// windowed) drives.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Integrator.hpp"

namespace nbody::core {

/// @brief Gravity and timing settings a world is built from.
struct WorldConfig {
    double g = 1.0;               ///< gravitational constant G
    double softening = 0.05;      ///< gravity softening epsilon
    double timeStep = 1.0 / 120.0;  // fixed physics dt, sim seconds
};

/// @brief Owns the body list, gravity field, and a polymorphic integrator.
/// @details step() integrates one fixed dt and then resolves overlaps with
///   perfectly inelastic merges. Total mass and total momentum are invariant
///   across step(); ids are monotonic and never reused, even across resets.
class PhysicsWorld {
public:
    PhysicsWorld(WorldConfig cfg, std::unique_ptr<IIntegrator> integrator);
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /// @brief Integrate one fixed dt, then resolve overlap merges.
    void step();
    /// @brief Replace the body list and zero the clock/step counters.
    /// @param bodies New state; bodies with id 0 get fresh monotonic ids.
    /// @note nextId_ advances across resets, so ids are never reused.
    void reset(std::vector<Body> bodies);
    /// @brief Swap the integrator; effective from the next step().
    ///   Hot-swap is safe because integrators are stateless across steps
    ///   except for scratch buffers owned by each instance.
    void setIntegrator(std::unique_ptr<IIntegrator> integrator);
    /// @brief Replace the gravity settings; effective from the next step().
    void setGravity(double g, double softening);
    /// @brief Replace the fixed dt; effective from the next step().
    /// @param dt Must be positive and finite.
    void setTimeStep(double dt);

    const std::vector<Body>& bodies() const;       ///< live body list
    const GravityField& gravity() const;           ///< active gravity field
    IIntegrator& integrator();                     ///< active integrator
    double simulationTime() const;  ///< accumulated sim seconds
    double timeStep() const;        ///< cfg_.timeStep, needed by the app loop
    /// @brief Return kinetic plus pairwise softened gravitational energy.
    double totalEnergy() const;
    std::uint64_t totalSteps() const;  ///< step() calls since the reset
    std::uint64_t mergeCount() const;  ///< merges performed since the reset

private:
    void resolveCollisions();  // repeated pairwise overlap-merge passes

    WorldConfig cfg_;
    GravityField gravity_;
    std::unique_ptr<IIntegrator> integrator_;
    std::vector<Body> bodies_;
    double simTime_ = 0.0;
    std::uint64_t steps_ = 0, merges_ = 0, nextId_ = 1;  // ids never reused
};

}  // namespace nbody::core
