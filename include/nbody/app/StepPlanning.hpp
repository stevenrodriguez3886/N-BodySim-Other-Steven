#pragma once

#include <cstdint>

namespace nbody::app {

// Fixed-timestep accumulator math (plan D.2), kept as a free pure function
// so it is unit-testable without SFML or a display (test_step_planning.cpp).

// Worst-case frame time we are willing to simulate per frame; anything
// larger (tab-switch, debugger, first frame after clock start) is clamped so
// the sim never "catches up" in a stutter burst.
constexpr double kMaxFrameTime = 0.25;  // wall seconds

// Hard cap on physics steps executed in one frame; if hit, the leftover
// accumulator is dropped (again: no catch-up bursts).
constexpr int kMaxStepsFrame = 30;

// frameTime/accumulator/dt are in seconds, dt > 0 expected (world-owned).
// Returns {steps, remainder}: steps >= 0, remainder is the accumulator value
// the frame loop should keep.
struct StepPlan {
    int steps = 0;
    double remainder = 0.0;
};

StepPlan planSteps(double accumulator, double frameTime, double dt);

// Steps executed since the previous stats sample. world_.reset() zeroes the
// world's step counter, so after any rebuild `current` is legitimately
// below the stale `last`: clamp to 0 instead of letting the unsigned
// subtraction wrap (the D.7 R-stall probe caught the wrap rendering
// garbage UPS in the title bar for one stats window after each restart).
std::uint64_t stepsSince(std::uint64_t current, std::uint64_t last);

}  // namespace nbody::app
