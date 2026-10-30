#include "nbody/core/PhysicsWorld.hpp"

#include <cassert>
#include <cmath>
#include <utility>

#include "nbody/core/Vec2.hpp"

namespace nbody::core {

PhysicsWorld::PhysicsWorld(WorldConfig cfg, std::unique_ptr<IIntegrator> integrator)
    : cfg_(cfg), gravity_(cfg.g, cfg.softening), integrator_(std::move(integrator)) {
    assert(integrator_ != nullptr);
}

void PhysicsWorld::step() {
#ifndef NDEBUG
    // NaN poisoning guard (review focus #2): a non-finite state before a step
    // means something upstream already broke; catch it at the source.
    for (const Body& body : bodies_) {
        assert(std::isfinite(body.position.x) && std::isfinite(body.position.y));
        assert(std::isfinite(body.velocity.x) && std::isfinite(body.velocity.y));
    }
#endif
    integrator_->step(bodies_, gravity_, cfg_.timeStep);
    resolveCollisions();
    ++steps_;
    simTime_ += cfg_.timeStep;
}

void PhysicsWorld::reset(std::vector<Body> bodies) {
    bodies_ = std::move(bodies);
    simTime_ = 0.0;
    steps_ = 0;
    // nextId_ is monotonic across resets: ids are never reused. Track the max
    // id already present (presets assign 1..N or explicit ids themselves); a
    // body arriving with id 0 means the caller left ids to the world.
    std::uint64_t maxId = 0;
    for (const Body& body : bodies_) {
        maxId = body.id > maxId ? body.id : maxId;
    }
    if (maxId + 1 > nextId_) {
        nextId_ = maxId + 1;
    }
    for (Body& body : bodies_) {
        if (body.id == 0) {
            body.id = nextId_++;
        }
    }
}

void PhysicsWorld::setIntegrator(std::unique_ptr<IIntegrator> integrator) {
    assert(integrator != nullptr);
    integrator_ = std::move(integrator);
}

void PhysicsWorld::setGravity(double g, double softening) {
    cfg_.g = g;
    cfg_.softening = softening;
    gravity_ = GravityField(g, softening);
}

void PhysicsWorld::setTimeStep(double dt) {
    assert(dt > 0.0 && std::isfinite(dt));
    cfg_.timeStep = dt;
}

const std::vector<Body>& PhysicsWorld::bodies() const {
    return bodies_;
}

const GravityField& PhysicsWorld::gravity() const {
    return gravity_;
}

IIntegrator& PhysicsWorld::integrator() {
    return *integrator_;
}

double PhysicsWorld::simulationTime() const {
    return simTime_;
}

double PhysicsWorld::timeStep() const {
    return cfg_.timeStep;
}

std::uint64_t PhysicsWorld::totalSteps() const {
    return steps_;
}

std::uint64_t PhysicsWorld::mergeCount() const {
    return merges_;
}

void PhysicsWorld::resolveCollisions() {
    // Plan C.4: overlap iff d^2 < (r_i + r_j)^2. Larger mass survives (tie:
    // lower id); survivor gets m=m1+m2, x=COM, v=momentum-weighted, r=sqrt
    // (r1^2+r2^2) and keeps its id; loser is swap-and-pop erased. Every merge
    // restarts the whole scan, so mid-vector erasures are handled by
    // construction. Each merging pass strictly decreases N, so <= N-1 merging
    // passes suffice; the 4N cap below only fires on an implementation bug.
    const std::size_t n0 = bodies_.size();
    std::uint32_t passes = 0;
    bool merged = true;
    while (merged) {
        assert(passes <= 4u * n0 && "resolveCollisions: pass cap exceeded");
        ++passes;
        merged = false;
        for (std::size_t i = 0; i < bodies_.size() && !merged; ++i) {
            for (std::size_t j = i + 1; j < bodies_.size() && !merged; ++j) {
                const Body& a = bodies_[i];
                const Body& b = bodies_[j];
                const Vec2 delta = b.position - a.position;
                const double sumR = a.radius + b.radius;
                if (!(lengthSq(delta) < sumR * sumR)) {
                    continue;
                }
                // First overlap: larger mass survives, ties go to lower id.
                const bool iSurvives =
                    a.mass > b.mass || (a.mass == b.mass && a.id < b.id);
                const std::size_t s = iSurvives ? i : j;
                const std::size_t l = iSurvives ? j : i;
                Body& survivor = bodies_[s];
                const Body& loser = bodies_[l];
                const double m = survivor.mass + loser.mass;
                const double wSurv = survivor.mass / m;
                const double wLoss = loser.mass / m;
                survivor.position = survivor.position * wSurv + loser.position * wLoss;
                survivor.velocity = survivor.velocity * wSurv + loser.velocity * wLoss;
                survivor.radius = std::sqrt(survivor.radius * survivor.radius +
                                            loser.radius * loser.radius);
                survivor.mass = m;
                // Body::appearance is deliberately untouched here: physics
                // never reads it, and the survivor BODY object carries its
                // appearance through both the in-place update above and the
                // swap below (the loser's is erased with it). No field-by-
                // field copy exists, so there is nothing to keep in sync.
                // Swap-and-pop: the survivor lands at the loser's slot; the
                // scan restart clears any staleness by construction.
                if (l != bodies_.size() - 1) {
                    std::swap(bodies_[l], bodies_[bodies_.size() - 1]);
                }
                bodies_.pop_back();
                ++merges_;
                merged = true;  // restart the scan from scratch
            }
        }
    }
}

}  // namespace nbody::core
