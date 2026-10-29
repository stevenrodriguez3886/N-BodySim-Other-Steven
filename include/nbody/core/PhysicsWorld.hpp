#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nbody/core/Body.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Integrator.hpp"

namespace nbody::core {

struct WorldConfig {
    double g = 1.0;
    double softening = 0.05;
    double timeStep = 1.0 / 120.0;  // fixed physics dt, sim seconds
};

// Owns the body list, the gravity field, and a polymorphic integrator.
// step() integrates one fixed dt and then resolves overlaps with perfectly
// inelastic merges (plan B.2 / C.4). Total mass and momentum are invariant
// across step(); ids are monotonic and never reused, even across resets.
class PhysicsWorld {
public:
    PhysicsWorld(WorldConfig cfg, std::unique_ptr<IIntegrator> integrator);
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void step();  // integrate one fixed dt, then merge
    void reset(std::vector<Body> bodies);
    // Effective from the next step(). Hot-swap is safe because integrators
    // are stateless across steps except scratch buffers owned by each instance.
    void setIntegrator(std::unique_ptr<IIntegrator> integrator);
    // Effective from the next step(). dt must be positive and finite.
    void setTimeStep(double dt);

    const std::vector<Body>& bodies() const;
    const GravityField& gravity() const;
    IIntegrator& integrator();
    double simulationTime() const;  // accumulated sim seconds
    double timeStep() const;        // cfg_.timeStep, needed by the app loop
    std::uint64_t totalSteps() const;
    std::uint64_t mergeCount() const;

private:
    void resolveCollisions();  // repeated merge passes, see C.4

    WorldConfig cfg_;
    GravityField gravity_;
    std::unique_ptr<IIntegrator> integrator_;
    std::vector<Body> bodies_;
    double simTime_ = 0.0;
    std::uint64_t steps_ = 0, merges_ = 0, nextId_ = 1;  // ids never reused
};

}  // namespace nbody::core
