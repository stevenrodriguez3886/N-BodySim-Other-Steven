/// @file
/// Environment-gated anomaly tracing: a steady timestamp helper and the
/// gate that keeps the off path essentially free. Run the app with
/// NBODY_TRACE set in the environment to get stderr diagnostics at the
/// restart and step-planning inputs; tracing is off by default.

#ifndef NBODY_APP_TRACE_HPP
#define NBODY_APP_TRACE_HPP

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace nbody::app {

/// @brief Seconds since the first traced call, on the steady clock.
/// @return A monotonic timestamp in seconds.
/// @note The static base initialises on the first call, so timestamps are
///   relative to when tracing first ran, not to process start. Only ever
///   called behind traceEnabled(), so the env-off cost is unaffected.
inline double traceNow() {
    static const std::chrono::steady_clock::time_point base =
        std::chrono::steady_clock::now();
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - base)
        .count();
}

/// @brief Whether NBODY_TRACE is present in the environment.
/// @return true when anomaly tracing is enabled for this process.
/// @note The gate is a function-local static initialised once from getenv,
///   so with the env var unset the per-call cost is one well-predicted
///   branch at each call site — no stream, formatting, or syscall work.
inline bool traceEnabled() {
    static const bool on = std::getenv("NBODY_TRACE") != nullptr;
    return on;
}

}  // namespace nbody::app

#endif  // NBODY_APP_TRACE_HPP
