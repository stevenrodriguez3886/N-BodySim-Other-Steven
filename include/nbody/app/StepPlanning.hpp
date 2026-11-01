/// @file
/// The fixed-timestep accumulator math for the frame loop, kept as free
/// pure functions so it is unit-testable without SFML or a display.

#pragma once

#include <cstdint>

namespace nbody::app {

/// @brief Worst-case frame time we are willing to simulate per frame;
///   anything larger (tab switch, debugger, first frame after clock start)
///   is clamped so the sim never "catches up" in a stutter burst.
constexpr double kMaxFrameTime = 0.25;  ///< wall-clock seconds

/// @brief Hard cap on physics steps executed in one frame; if hit, the
///   leftover accumulator is dropped (again: no catch-up bursts).
constexpr int kMaxStepsFrame = 30;  ///< whole physics steps per frame

/// @brief What one accumulator update decides: how many physics steps to
///   run and what accumulator value the frame loop should keep.
struct StepPlan {
    int steps = 0;            ///< whole dt-steps to execute this frame
    double remainder = 0.0;   ///< accumulator to carry, seconds, in [0, dt)
};

/// @brief Advance the fixed-timestep accumulator by one frame.
/// @param accumulator Accumulator carried from the previous frame, seconds.
/// @param frameTime Wall time of the frame just ended, seconds; clamped to
///                  kMaxFrameTime before it is added.
/// @param dt The world's fixed physics timestep, seconds; dt > 0 expected
///           (world-owned), and a non-positive dt yields zero steps.
/// @return {steps, remainder} with steps >= 0; the accumulator is zeroed
///         when the kMaxStepsFrame cap is hit.
StepPlan planSteps(double accumulator, double frameTime, double dt);

/// @brief Steps executed since the previous stats sample.
/// @param current The world's step counter now.
/// @param last The counter as of the previous sample.
/// @return current - last, clamped to 0.
/// @note world_.reset() zeroes the world's step counter, so after any
///   rebuild `current` is legitimately below the stale `last`; clamping
///   prevents the unsigned subtraction from wrapping and rendering
///   garbage UPS in the title bar for one stats window after a restart.
std::uint64_t stepsSince(std::uint64_t current, std::uint64_t last);

}  // namespace nbody::app
