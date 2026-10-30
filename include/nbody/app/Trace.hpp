#ifndef NBODY_APP_TRACE_HPP
#define NBODY_APP_TRACE_HPP

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace nbody::app {

// Seconds since the first traced call (steady clock). Only ever touched
// behind traceEnabled(), so env-off cost is unaffected.
inline double traceNow() {
    static const std::chrono::steady_clock::time_point base =
        std::chrono::steady_clock::now();
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - base)
        .count();
}

// Env-gated anomaly tracing (spec D.7): run the app with NBODY_TRACE=1 to
// get fprintf diagnostics at the restart and step-planning inputs (stderr).
// Off by default. The gate is a function-local static initialised once from
// getenv, so with the env var unset the per-call cost is one well-predicted
// branch at each call site — no stream, formatting, or syscall work.
inline bool traceEnabled() {
    static const bool on = std::getenv("NBODY_TRACE") != nullptr;
    return on;
}

}  // namespace nbody::app

#endif  // NBODY_APP_TRACE_HPP
