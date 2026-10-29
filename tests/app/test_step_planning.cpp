// Task 9: pure fixed-timestep accumulator math (plan D.2). No SFML, no
// display — runs anywhere, unlike the rest of the windowed path.
#include "nbody/app/StepPlanning.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

using nbody::app::kMaxFrameTime;
using nbody::app::kMaxStepsFrame;
using nbody::app::planSteps;

TEST_CASE("StepPlanning constants match plan D.2") {
    REQUIRE(kMaxFrameTime == 0.25);
    REQUIRE(kMaxStepsFrame == 30);
}

TEST_CASE("planSteps consumes whole steps and keeps the remainder") {
    const nbody::app::StepPlan p = planSteps(0.0, 0.025, 1.0 / 120.0);
    REQUIRE(p.steps == 3);  // 0.025 / (1/120) = 3 exactly
    REQUIRE(p.remainder < 1.0 / 120.0);
}

TEST_CASE("planSteps does nothing when the accumulator is below dt") {
    const nbody::app::StepPlan p = planSteps(0.0, 0.001, 1.0 / 60.0);
    REQUIRE(p.steps == 0);
    REQUIRE(p.remainder == Approx(0.001));
}

TEST_CASE("planSteps clamps huge frame times to kMaxFrameTime") {
    // 100 s "frame" (tab switch) must still yield at most the clamp/dt steps,
    // and the 30-step cap zeroes the leftover so no catch-up debt builds.
    const nbody::app::StepPlan p = planSteps(0.0, 100.0, 1.0 / 120.0);
    REQUIRE(p.steps == kMaxStepsFrame);  // 0.25 s / (1/120) = 30
    REQUIRE(p.remainder == 0.0);
}

TEST_CASE("planSteps zeroes the accumulator when the 30-step cap is hit") {
    // dt tiny so that 0.25 s spans far more than 30 steps.
    const nbody::app::StepPlan p = planSteps(0.0, 0.25, 1.0 / 1000.0);
    REQUIRE(p.steps == kMaxStepsFrame);
    REQUIRE(p.remainder == 0.0);
}

TEST_CASE("planSteps carries the accumulator across frames without loss") {
    double acc = 0.0;
    int total = 0;
    const double dt = 1.0 / 60.0;
    for (int frame = 0; frame < 60; ++frame) {  // 60 frames x 1/30 s = 2 s
        const nbody::app::StepPlan p = planSteps(acc, 1.0 / 30.0, dt);
        acc = p.remainder;
        total += p.steps;
    }
    REQUIRE(total == 120);  // exactly 2 s of whole steps, no dropped work
    REQUIRE(acc == Approx(0.0).margin(1e-9));
}
