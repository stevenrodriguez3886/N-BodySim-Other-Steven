/// @file
/// The accumulator update behind the fixed-timestep frame loop. The
/// clamp/cap constants and their contracts live with the declarations in
/// StepPlanning.hpp.

#include "nbody/app/StepPlanning.hpp"

#include <algorithm>

namespace nbody::app {

StepPlan planSteps(double accumulator, double frameTime, double dt) {
    // kMaxFrameTime (0.25 s) clamps a huge frame delta (tab switch, first
    // frame); kMaxStepsFrame (30) caps the catch-up burst, and on cap the
    // leftover accumulator is dropped rather than replayed later.
    const double clamped = std::min(frameTime, kMaxFrameTime);
    double acc = accumulator + clamped;
    int steps = 0;
    if (dt > 0.0) {  // defensive: a non-positive dt would spin forever
        while (acc >= dt && steps < kMaxStepsFrame) {
            acc -= dt;
            ++steps;
        }
    }
    if (steps == kMaxStepsFrame) {
        acc = 0.0;
    }
    return StepPlan{steps, acc};
}

std::uint64_t stepsSince(std::uint64_t current, std::uint64_t last) {
    return current >= last ? current - last : 0U;
}

}  // namespace nbody::app
