#include "nbody/app/StepPlanning.hpp"

#include <algorithm>

namespace nbody::app {

StepPlan planSteps(double accumulator, double frameTime, double dt) {
    // Constants live in the header verbatim: kMaxFrameTime (0.25 s) clamps a
    // huge frame delta (tab switch, first frame), kMaxStepsFrame (30) caps
    // the catch-up burst; on cap the leftover accumulator is dropped.
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

}  // namespace nbody::app
