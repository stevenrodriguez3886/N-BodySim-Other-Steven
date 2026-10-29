#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/EulerIntegrator.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Integrator.hpp"
#include "nbody/core/PhysicsWorld.hpp"
#include "nbody/core/Presets.hpp"
#include "nbody/core/Rk4Integrator.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;
using Catch::Approx;

namespace {

// WorldConfig with an effectively inert gravity field: a huge softening makes
// every pairwise acceleration ~G*m/eps^2 -> negligible, so a single step()
// isolates the merge math from any gravitational displacement.
WorldConfig inertConfig(double dt = 1.0 / 120.0) {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 1e9;  // a_i ~ 1 / 1e18, displacement over one step ~ 0
    cfg.timeStep = dt;
    return cfg;
}

}  // namespace

TEST_CASE("world_merge_conserves_mass_momentum") {
    // m1=2, v=(1,0); m2=1, v=(0,1). One step must leave exactly one body of
    // mass 3 carrying the exact initial total momentum (2, 1).
    // The bodies start coincident and the inert field cannot separate them
    // (identical ~zero displacement), so detection after integration fires.
    PhysicsWorld world{inertConfig(), std::make_unique<EulerIntegrator>()};
    world.reset({
        Body{1, 2.0, 0.5, Vec2{0.0, 0.0}, Vec2{1.0, 0.0}},
        Body{2, 1.0, 0.3, Vec2{0.0, 0.0}, Vec2{0.0, 1.0}},
    });

    const Vec2 p0{2.0 * 1.0 + 1.0 * 0.0, 2.0 * 0.0 + 1.0 * 1.0};
    world.step();

    REQUIRE(world.bodies().size() == 1);
    const Body& survivor = world.bodies()[0];
    CHECK(survivor.mass == Approx(3.0).margin(1e-12));
    CHECK(survivor.id == 1);  // larger mass survives
    CHECK(world.mergeCount() == 1);
    // Momentum after: m*v must equal p0 = (2, 1).
    CHECK(survivor.mass * survivor.velocity.x == Approx(p0.x).margin(1e-12));
    CHECK(survivor.mass * survivor.velocity.y == Approx(p0.y).margin(1e-12));
}

TEST_CASE("world_merge_position_radius") {
    // Overlapping (d=0.4 < r1+r2=0.8) pair at dt=0: survivor position must be
    // the momentum-weighted center of mass, radius sqrt(r1^2+r2^2).
    PhysicsWorld world{inertConfig(0.0), std::make_unique<EulerIntegrator>()};
    world.reset({
        Body{1, 2.0, 0.5, Vec2{0.0, 0.0}, Vec2{1.0, 0.0}},
        Body{2, 1.0, 0.3, Vec2{0.4, 0.0}, Vec2{0.0, 1.0}},
    });

    world.step();

    REQUIRE(world.bodies().size() == 1);
    const Body& survivor = world.bodies()[0];
    // x = (2*(0,0) + 1*(0.4,0)) / 3 = (0.4/3, 0)
    CHECK(survivor.position.x == Approx((2.0 * 0.0 + 1.0 * 0.4) / 3.0).margin(1e-9));
    CHECK(survivor.position.y == Approx(0.0).margin(1e-9));
    CHECK(survivor.radius == Approx(std::sqrt(0.5 * 0.5 + 0.3 * 0.3)).margin(1e-9));
}

TEST_CASE("world_chain_merge_terminates") {
    // Three coincident bodies: the scan-restart loop must terminate with one
    // survivor (id 2, the largest mass) and exactly 2 recorded merges.
    PhysicsWorld world{inertConfig(), std::make_unique<EulerIntegrator>()};
    world.reset({
        Body{1, 1.0, 0.5, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{2, 5.0, 0.4, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{3, 2.0, 0.6, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
    });

    world.step();  // one step is enough; each pass restarts the scan on a merge

    REQUIRE(world.bodies().size() == 1);
    CHECK(world.bodies()[0].id == 2);
    CHECK(world.bodies()[0].mass == Approx(8.0).margin(1e-12));
    CHECK(world.mergeCount() == 2);
}

TEST_CASE("world_chain_merge_terminates_rk4") {
    // Same chain merge under RK4: additionally exercises the RK4 scratch
    // resize-on-shrink path (4 bodies -> 1 between steps).
    PhysicsWorld world{inertConfig(), std::make_unique<Rk4Integrator>()};
    world.reset({
        Body{1, 1.0, 0.5, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{2, 5.0, 0.4, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{3, 2.0, 0.6, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{4, 3.0, 0.45, Vec2{0.1, 0.0}, Vec2{0.0, 0.0}},
    });

    for (int i = 0; i < 3; ++i) {
        world.step();  // must not hang or assert (4N pass cap)
    }

    REQUIRE(world.bodies().size() == 1);
    CHECK(world.mergeCount() == 3);
}

TEST_CASE("world_ids_never_reused") {
    // ids are monotonic across resets: nextId_ never rewinds, and bodies
    // arriving with id==0 get ids above everything ever issued.
    PhysicsWorld world{inertConfig(), std::make_unique<EulerIntegrator>()};
    world.reset({
        Body{0, 2.0, 0.5, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},  // id assigned 1
        Body{0, 1.0, 0.5, Vec2{0.1, 0.0}, Vec2{0.0, 0.0}},  // id assigned 2
    });
    const std::uint64_t idA = world.bodies()[0].id;
    const std::uint64_t idB = world.bodies()[1].id;
    REQUIRE(idA == 1);
    REQUIRE(idB == 2);

    world.step();                       // merges to one survivor
    REQUIRE(world.bodies().size() == 1);

    world.reset({
        Body{0, 1.0, 0.2, Vec2{10.0, 0.0}, Vec2{0.0, 0.0}},  // must be > 2
        Body{0, 1.0, 0.2, Vec2{-10.0, 0.0}, Vec2{0.0, 0.0}},
    });
    const std::uint64_t idC = world.bodies()[0].id;
    const std::uint64_t idD = world.bodies()[1].id;
    CHECK(idC > std::max({idA, idB}));
    CHECK(idD > std::max({idA, idB}));

    // And a merge of the post-reset bodies keeps an already-issued id.
    world.reset({
        Body{0, 3.0, 0.5, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{0, 1.0, 0.5, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
    });
    CHECK(world.bodies()[0].id > std::max({idA, idB}));
    CHECK(world.bodies()[1].id > std::max({idA, idB}));
    CHECK(world.bodies()[0].id != world.bodies()[1].id);
    world.step();
    REQUIRE(world.bodies().size() == 1);
    CHECK(world.bodies()[0].id > 0);  // survivor keeps its id, never a fresh one
    world.reset({Body{42, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}}});
    CHECK(world.bodies()[0].id == 42);  // preset-provided ids are kept as-is
    world.reset({Body{0, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}}});
    CHECK(world.bodies()[0].id > 42);  // monotonic even past a preset's explicit ids
}

TEST_CASE("world_step_advances_time") {
    PhysicsWorld world{inertConfig(0.25), std::make_unique<EulerIntegrator>()};
    world.reset({Body{1, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}}});
    CHECK(world.totalSteps() == 0);
    CHECK(world.simulationTime() == Approx(0.0).margin(0.0));
    CHECK(world.timeStep() == Approx(0.25));

    for (int i = 1; i <= 10; ++i) {
        world.step();
        CHECK(world.totalSteps() == static_cast<std::uint64_t>(i));
        CHECK(world.simulationTime() == Approx(0.25 * i).margin(1e-12));
    }

    world.reset({Body{7, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}}});
    CHECK(world.totalSteps() == 0);
    CHECK(world.simulationTime() == Approx(0.0).margin(0.0));
}

TEST_CASE("world_no_collision_stable_pair") {
    // Exact circular two-body orbit (plan C.5 two-body preset geometry) with
    // radii 0.05: separation stays ~2 across 5000 steps, so the body count
    // must remain 2 with zero merges. Euler (cheaper) per instructions.
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;  // preset value: no softening
    cfg.timeStep = 1.0 / 120.0;
    PhysicsWorld world{cfg, std::make_unique<EulerIntegrator>()};
    world.reset({
        Body{1, 1.0, 0.05, Vec2{1.0, 0.0}, Vec2{0.0, 0.5}},
        Body{2, 1.0, 0.05, Vec2{-1.0, 0.0}, Vec2{0.0, -0.5}},
    });

    for (int i = 0; i < 5000; ++i) {
        world.step();
        REQUIRE(world.bodies().size() == 2);
    }
    CHECK(world.mergeCount() == 0);
    CHECK(world.totalSteps() == 5000);
}

TEST_CASE("world_set_integrator_swaps_midflight") {
    WorldConfig cfg;
    cfg.g = 1.0;
    cfg.softening = 0.0;
    cfg.timeStep = 1.0 / 120.0;
    PhysicsWorld world{cfg, makeIntegrator(IntegratorKind::Rk4)};
    world.reset({
        Body{1, 1.0, 0.05, Vec2{1.0, 0.0}, Vec2{0.0, 0.5}},
        Body{2, 1.0, 0.05, Vec2{-1.0, 0.0}, Vec2{0.0, -0.5}},
    });
    for (int i = 0; i < 100; ++i) {
        world.step();
    }

    const std::vector<Body> beforeStep = world.bodies();
    world.setIntegrator(makeIntegrator(IntegratorKind::Euler));
    world.step();

    REQUIRE(world.integrator().name() == "euler");
    REQUIRE(world.bodies().size() == beforeStep.size());
    for (std::size_t i = 0; i < beforeStep.size(); ++i) {
        CHECK(world.bodies()[i].position.x ==
              Approx(beforeStep[i].position.x + beforeStep[i].velocity.x * cfg.timeStep)
                  .margin(1e-12));
        CHECK(world.bodies()[i].position.y ==
              Approx(beforeStep[i].position.y + beforeStep[i].velocity.y * cfg.timeStep)
                  .margin(1e-12));
    }
}

TEST_CASE("world_set_timestep_changes_advance") {
    PhysicsWorld world{inertConfig(), std::make_unique<EulerIntegrator>()};
    world.reset({Body{1, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}}});

    world.setTimeStep(1.0 / 60.0);
    world.step();

    CHECK(world.simulationTime() == Approx(1.0 / 60.0).margin(1e-12));
    CHECK(world.timeStep() == Approx(1.0 / 60.0).margin(0.0));
}
