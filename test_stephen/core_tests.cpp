#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "nbody/core/Body.hpp"
#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/PhysicsWorld.hpp"
#include "nbody/core/Presets.hpp"
#include "nbody/core/Rk4Integrator.hpp"
#include "nbody/core/Vec2.hpp"

namespace {

using nbody::core::Body;
using nbody::core::EulerIntegrator;
using nbody::core::GravityField;
using nbody::core::IIntegrator;
using nbody::core::IntegratorKind;
using nbody::core::PhysicsWorld;
using nbody::core::Rk4Integrator;
using nbody::core::Vec2;
using nbody::core::WorldConfig;

constexpr double kTolerance = 1e-12;

void expectVecNear(Vec2 actual, Vec2 expected, double tolerance = kTolerance) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
}

void expectBodyStateEqual(const Body& actual, const Body& expected) {
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_DOUBLE_EQ(actual.mass, expected.mass);
    EXPECT_DOUBLE_EQ(actual.radius, expected.radius);
    EXPECT_DOUBLE_EQ(actual.position.x, expected.position.x);
    EXPECT_DOUBLE_EQ(actual.position.y, expected.position.y);
    EXPECT_DOUBLE_EQ(actual.velocity.x, expected.velocity.x);
    EXPECT_DOUBLE_EQ(actual.velocity.y, expected.velocity.y);
    EXPECT_EQ(actual.appearance.color, expected.appearance.color);
    EXPECT_EQ(actual.appearance.glow, expected.appearance.glow);
    EXPECT_EQ(actual.appearance.ring, expected.appearance.ring);
    EXPECT_EQ(actual.appearance.bands, expected.appearance.bands);
}

double totalMass(const std::vector<Body>& bodies) {
    double result = 0.0;
    for (const Body& body : bodies) {
        result += body.mass;
    }
    return result;
}

Vec2 totalMomentum(const std::vector<Body>& bodies) {
    Vec2 result{};
    for (const Body& body : bodies) {
        result += body.velocity * body.mass;
    }
    return result;
}

Body makeBody(std::uint64_t id, double mass, double radius, Vec2 position,
              Vec2 velocity = {}) {
    return Body{id, mass, radius, position, velocity};
}

class IntegratorTest : public ::testing::TestWithParam<bool> {};

TEST(Vec2Test, OperatorsAndMetrics) {
    constexpr Vec2 a{1.0, 2.0};
    constexpr Vec2 b{3.0, 4.0};
    static_assert((a + b).x == 4.0);
    static_assert((2.0 * a).y == 4.0);
    static_assert(nbody::core::dot(a, b) == 11.0);

    expectVecNear(a + b, {4.0, 6.0});
    expectVecNear(b - a, {2.0, 2.0});
    expectVecNear(a * 2.0, {2.0, 4.0});
    expectVecNear(2.0 * a, {2.0, 4.0});
    expectVecNear(b / 2.0, {1.5, 2.0});
    expectVecNear(-a, {-1.0, -2.0});
    EXPECT_DOUBLE_EQ(nbody::core::dot(a, b), 11.0);
    EXPECT_DOUBLE_EQ(nbody::core::lengthSq({3.0, 4.0}), 25.0);
    EXPECT_DOUBLE_EQ(nbody::core::length({3.0, 4.0}), 5.0);

    Vec2 compound = a;
    Vec2& addResult = (compound += b);
    EXPECT_EQ(&addResult, &compound);
    compound -= b;
    compound *= 3.0;
    expectVecNear(compound, {3.0, 6.0});
}

TEST(BodyTest, ConstructorsAndAppearanceDefaults) {
    const Body defaultBody{};
    EXPECT_EQ(defaultBody.id, 0U);
    EXPECT_DOUBLE_EQ(defaultBody.mass, 1.0);
    EXPECT_DOUBLE_EQ(defaultBody.radius, 0.1);
    EXPECT_EQ(defaultBody.appearance.color, 0U);
    EXPECT_FALSE(defaultBody.appearance.glow);
    EXPECT_FALSE(defaultBody.appearance.ring);
    EXPECT_EQ(defaultBody.appearance.bands, 0U);

    Body::Appearance appearance{0x123456U, true, true, 3};
    const Body body{7, 2.0, 0.5, {1.0, 2.0}, {3.0, 4.0}, appearance};
    EXPECT_EQ(body.id, 7U);
    EXPECT_DOUBLE_EQ(body.mass, 2.0);
    EXPECT_DOUBLE_EQ(body.radius, 0.5);
    expectVecNear(body.position, {1.0, 2.0});
    expectVecNear(body.velocity, {3.0, 4.0});
    EXPECT_EQ(body.appearance.color, 0x123456U);
    EXPECT_TRUE(body.appearance.glow);
    EXPECT_TRUE(body.appearance.ring);
    EXPECT_EQ(body.appearance.bands, 3U);
}

TEST(GravityFieldTest, ComputesSoftenedPairwiseAccelerationAndClearsOutput) {
    const std::vector<Body> bodies{
        makeBody(1, 2.0, 0.1, {0.0, 0.0}),
        makeBody(2, 3.0, 0.1, {2.0, 0.0}),
    };
    GravityField gravity(1.0, 0.5);
    std::vector<Vec2> acceleration{{99.0, 99.0}, {99.0, 99.0}, {99.0, 99.0}};
    gravity.computeAccelerations(bodies, acceleration);

    const double inv = 1.0 / ((4.0 + 0.25) * 2.0);
    EXPECT_EQ(acceleration.size(), 2U);
    expectVecNear(acceleration[0], {3.0 * 2.0 * inv, 0.0});
    expectVecNear(acceleration[1], {-2.0 * 2.0 * inv, 0.0});
    EXPECT_EQ(bodies[0].position.x, 0.0);
    EXPECT_EQ(bodies[1].position.x, 2.0);
}

TEST(GravityFieldTest, HandlesEmptySingleAndCoincidentBodies) {
    GravityField gravity;
    std::vector<Vec2> acceleration{{1.0, 2.0}};
    gravity.computeAccelerations({}, acceleration);
    EXPECT_TRUE(acceleration.empty());

    const std::vector<Body> coincident{
        makeBody(1, 1.0, 0.1, {0.0, 0.0}),
        makeBody(2, 4.0, 0.1, {0.0, 0.0}),
    };
    gravity.computeAccelerations(coincident, acceleration);
    ASSERT_EQ(acceleration.size(), 2U);
    expectVecNear(acceleration[0], {});
    expectVecNear(acceleration[1], {});
}

TEST(GravityFieldTest, ConservesPairwiseMomentumDerivative) {
    const std::vector<Body> bodies{
        makeBody(1, 2.0, 0.1, {-1.0, 0.0}),
        makeBody(2, 3.0, 0.1, {1.0, 0.0}),
        makeBody(3, 5.0, 0.1, {0.0, 2.0}),
    };
    std::vector<Vec2> acceleration;
    GravityField(1.2, 0.05).computeAccelerations(bodies, acceleration);
    Vec2 momentumDerivative{};
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        momentumDerivative += acceleration[i] * bodies[i].mass;
    }
    expectVecNear(momentumDerivative, {}, 1e-13);
}

TEST_P(IntegratorTest, ReportsNameAndPreservesMetadata) {
    std::unique_ptr<IIntegrator> integrator;
    if (GetParam()) {
        integrator = std::make_unique<Rk4Integrator>();
    } else {
        integrator = std::make_unique<EulerIntegrator>();
    }
    EXPECT_EQ(integrator->name(), GetParam() ? "rk4" : "euler");

    Body body = makeBody(42, 3.0, 0.7, {1.0, 2.0}, {4.0, -1.0});
    body.appearance = Body::Appearance{0xAABBCCU, true, false, 2};
    const Body before = body;
    std::vector<Body> bodies{body};
    integrator->step(bodies, GravityField(0.0), 0.25);
    ASSERT_EQ(bodies.size(), 1U);
    EXPECT_EQ(bodies[0].id, before.id);
    EXPECT_DOUBLE_EQ(bodies[0].mass, before.mass);
    EXPECT_DOUBLE_EQ(bodies[0].radius, before.radius);
    EXPECT_EQ(bodies[0].appearance.color, before.appearance.color);
    EXPECT_TRUE(bodies[0].appearance.glow);
    expectVecNear(bodies[0].position, {2.0, 1.75});
    expectVecNear(bodies[0].velocity, before.velocity);
}

INSTANTIATE_TEST_SUITE_P(AllIntegrators, IntegratorTest,
                         ::testing::Values(false, true));

TEST(EulerIntegratorTest, UsesOldVelocityAndCurrentAcceleration) {
    std::vector<Body> bodies{
        makeBody(1, 1.0, 0.1, {0.0, 0.0}, {2.0, 3.0}),
        makeBody(2, 1.0, 0.1, {1.0, 0.0}),
    };
    EulerIntegrator integrator;
    integrator.step(bodies, GravityField(1.0, 0.0), 0.5);
    expectVecNear(bodies[0].position, {1.0, 1.5});
    expectVecNear(bodies[0].velocity, {2.5, 3.0});
    expectVecNear(bodies[1].position, {1.0, 0.0});
    expectVecNear(bodies[1].velocity, {-0.5, 0.0});
}

TEST(Rk4IntegratorTest, ZeroGravityMatchesConstantVelocitySolution) {
    std::vector<Body> bodies{
        makeBody(1, 1.0, 0.1, {-1.0, 4.0}, {2.0, -3.0}),
        makeBody(2, 2.0, 0.1, {5.0, 2.0}, {-1.0, 1.0}),
    };
    Rk4Integrator integrator;
    integrator.step(bodies, GravityField(0.0), 0.75);
    expectVecNear(bodies[0].position, {0.5, 1.75});
    expectVecNear(bodies[0].velocity, {2.0, -3.0});
    expectVecNear(bodies[1].position, {4.25, 2.75});
    expectVecNear(bodies[1].velocity, {-1.0, 1.0});
}

TEST(PhysicsWorldTest, ResetStepCountersAndAssignsMonotonicIds) {
    WorldConfig config;
    config.timeStep = 0.25;
    PhysicsWorld world(config, std::make_unique<EulerIntegrator>());
    world.reset({makeBody(0, 1.0, 0.1, {0.0, 0.0}),
                 makeBody(0, 1.0, 0.1, {10.0, 0.0})});
    ASSERT_EQ(world.bodies().size(), 2U);
    EXPECT_EQ(world.bodies()[0].id, 1U);
    EXPECT_EQ(world.bodies()[1].id, 2U);
    world.step();
    EXPECT_EQ(world.totalSteps(), 1U);
    EXPECT_DOUBLE_EQ(world.simulationTime(), 0.25);

    world.reset({makeBody(0, 1.0, 0.1, {0.0, 0.0})});
    ASSERT_EQ(world.bodies().size(), 1U);
    EXPECT_EQ(world.bodies()[0].id, 3U);
    EXPECT_EQ(world.totalSteps(), 0U);
    EXPECT_DOUBLE_EQ(world.simulationTime(), 0.0);
    EXPECT_EQ(world.mergeCount(), 0U);
}

TEST(PhysicsWorldTest, SettersApplyConfiguration) {
    PhysicsWorld world(WorldConfig{}, std::make_unique<EulerIntegrator>());
    world.setGravity(2.0, 0.25);
    world.setTimeStep(0.5);
    world.setIntegrator(std::make_unique<Rk4Integrator>());
    EXPECT_DOUBLE_EQ(world.timeStep(), 0.5);
    EXPECT_EQ(world.integrator().name(), "rk4");

    std::vector<Body> bodies{makeBody(1, 1.0, 0.01, {0.0, 0.0}),
                             makeBody(2, 1.0, 0.01, {1.0, 0.0})};
    world.reset(bodies);
    world.step();
    EXPECT_EQ(world.totalSteps(), 1U);
    EXPECT_DOUBLE_EQ(world.simulationTime(), 0.5);
}

TEST(PhysicsWorldTest, MergesOverlapWithMassMomentumAndAppearanceRules) {
    WorldConfig config;
    config.g = 0.0;
    config.timeStep = 0.1;
    PhysicsWorld world(config, std::make_unique<EulerIntegrator>());
    Body survivor = makeBody(10, 3.0, 0.5, {0.0, 0.0}, {2.0, 0.0});
    survivor.appearance = Body::Appearance{0x111111U, true, false, 1};
    Body loser = makeBody(20, 1.0, 0.5, {0.5, 0.0}, {0.0, 4.0});
    loser.appearance = Body::Appearance{0x222222U, false, true, 3};
    world.reset({survivor, loser});
    world.step();

    ASSERT_EQ(world.bodies().size(), 1U);
    const Body& merged = world.bodies().front();
    EXPECT_EQ(merged.id, 10U);
    EXPECT_DOUBLE_EQ(merged.mass, 4.0);
    EXPECT_DOUBLE_EQ(merged.radius, std::sqrt(0.5));
    expectVecNear(merged.position, {0.275, 0.1});
    expectVecNear(merged.velocity, {1.5, 1.0});
    EXPECT_EQ(merged.appearance.color, 0x111111U);
    EXPECT_TRUE(merged.appearance.glow);
    EXPECT_EQ(world.mergeCount(), 1U);
}

TEST(PhysicsWorldTest, EqualMassCollisionUsesLowerIdAndConservesTotals) {
    WorldConfig config;
    config.g = 0.0;
    config.timeStep = 0.01;
    PhysicsWorld world(config, std::make_unique<EulerIntegrator>());
    std::vector<Body> bodies{
        makeBody(9, 1.0, 1.0, {0.0, 0.0}, {1.0, 0.0}),
        makeBody(4, 1.0, 1.0, {0.5, 0.0}, {-1.0, 0.0}),
    };
    const double mass = totalMass(bodies);
    const Vec2 momentum = totalMomentum(bodies);
    world.reset(bodies);
    world.step();
    ASSERT_EQ(world.bodies().size(), 1U);
    EXPECT_EQ(world.bodies()[0].id, 4U);
    EXPECT_DOUBLE_EQ(totalMass(world.bodies()), mass);
    expectVecNear(totalMomentum(world.bodies()), momentum);
}

TEST(PresetSeedTest, FoldsSeedsDeterministicallyAndPreservesSmallSeeds) {
    EXPECT_EQ(nbody::core::foldSeed(42), 42U);
    EXPECT_EQ(nbody::core::foldSeed(0), 0U);
    EXPECT_EQ(nbody::core::foldSeed(4294967295LL), 4294967295U);
    EXPECT_EQ(nbody::core::foldSeed(-1), nbody::core::foldSeed(-1));
    EXPECT_EQ(nbody::core::foldSeed(4294967296LL),
              nbody::core::foldSeed(4294967296LL));
    EXPECT_NE(nbody::core::foldSeed(-1), nbody::core::foldSeed(0));
}

TEST(TwoBodyPresetTest, BuildsCircularAndHyperbolicVariants) {
    const auto circular = nbody::core::twoBody(42);
    ASSERT_EQ(circular.bodies.size(), 2U);
    EXPECT_DOUBLE_EQ(circular.config.g, 1.0);
    EXPECT_DOUBLE_EQ(circular.config.softening, 0.0);
    expectVecNear(circular.bodies[0].position, {1.0, 0.0});
    expectVecNear(circular.bodies[1].position, {-1.0, 0.0});
    expectVecNear(circular.bodies[0].velocity, {0.0, 0.5});
    expectVecNear(circular.bodies[1].velocity, {0.0, -0.5});

    const auto hyperbolic = nbody::core::twoBody(7, 10.0, true);
    EXPECT_DOUBLE_EQ(hyperbolic.bodies[0].velocity.y, 0.8);
    EXPECT_DOUBLE_EQ(hyperbolic.bodies[1].velocity.y, -0.8);
}

TEST(SolarSystemPresetTest, ClampsCountAndAddsPlutoOnlyToFullSystem) {
    EXPECT_EQ(nbody::core::solarSystem(1, -3).bodies.size(), 1U);
    EXPECT_EQ(nbody::core::solarSystem(1, 2).bodies.size(), 3U);
    EXPECT_EQ(nbody::core::solarSystem(1, 99).bodies.size(), 9U);
    EXPECT_EQ(nbody::core::solarSystem(1, 8, true).bodies.size(), 10U);
    EXPECT_EQ(nbody::core::solarSystem(1, 7, true).bodies.size(), 8U);
    const auto setup = nbody::core::solarSystem(1, 8, true);
    EXPECT_TRUE(setup.bodies.front().appearance.glow);
    EXPECT_EQ(setup.bodies[5].appearance.bands, 3U);
    EXPECT_TRUE(setup.bodies[6].appearance.ring);
    EXPECT_DOUBLE_EQ(setup.bodies.back().position.x, 200.0);
}

TEST(RandomCloudPresetTest, IsDeterministicPrefixStableAndClamped) {
    const auto small = nbody::core::randomCloud(42, 3);
    const auto large = nbody::core::randomCloud(42, 10);
    ASSERT_EQ(small.bodies.size(), 3U);
    ASSERT_EQ(large.bodies.size(), 10U);
    for (std::size_t i = 0; i < small.bodies.size(); ++i) {
        expectBodyStateEqual(small.bodies[i], large.bodies[i]);
    }
    const auto repeat = nbody::core::randomCloud(42, 3);
    for (std::size_t i = 0; i < repeat.bodies.size(); ++i) {
        expectBodyStateEqual(repeat.bodies[i], small.bodies[i]);
        EXPECT_GE(repeat.bodies[i].mass, 0.1);
        EXPECT_LE(repeat.bodies[i].mass, 1.0);
        EXPECT_GT(repeat.bodies[i].radius, 0.0);
    }
    EXPECT_EQ(nbody::core::randomCloud(42, 1).bodies.size(), 2U);
    EXPECT_EQ(nbody::core::randomCloud(42, 3000).bodies.size(), 2000U);
}

TEST(FigureEightPresetTest, AppliesKeplerianScale) {
    const auto unit = nbody::core::figureEight(1, 1.0);
    const auto scaled = nbody::core::figureEight(2, 4.0);
    ASSERT_EQ(unit.bodies.size(), 3U);
    ASSERT_EQ(scaled.bodies.size(), 3U);
    for (std::size_t i = 0; i < unit.bodies.size(); ++i) {
        expectVecNear(scaled.bodies[i].position, unit.bodies[i].position * 4.0);
        expectVecNear(scaled.bodies[i].velocity, unit.bodies[i].velocity * 0.5);
    }
}

TEST(PresetFactoryTest, DispatchesPresetsAndIntegrators) {
    EXPECT_EQ(nbody::core::createPreset(nbody::core::PresetName::TwoBody, 1)
                  .bodies.size(),
              2U);
    EXPECT_EQ(nbody::core::createPreset(nbody::core::PresetName::SolarSystem, 1)
                  .bodies.size(),
              9U);
    EXPECT_EQ(nbody::core::createPreset(nbody::core::PresetName::RandomCloud, 1)
                  .bodies.size(),
              150U);
    EXPECT_EQ(nbody::core::createPreset(nbody::core::PresetName::FigureEight, 1)
                  .bodies.size(),
              3U);
    EXPECT_EQ(nbody::core::makeIntegrator(IntegratorKind::Euler)->name(), "euler");
    EXPECT_EQ(nbody::core::makeIntegrator(IntegratorKind::Rk4)->name(), "rk4");
}

}  // namespace
