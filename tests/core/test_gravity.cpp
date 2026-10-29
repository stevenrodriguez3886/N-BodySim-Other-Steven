#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/GravityField.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;
using Catch::Approx;

namespace {

Vec2 accelAt(const std::vector<Vec2>& accel, std::size_t i) {
    REQUIRE(i < accel.size());
    return accel[i];
}

}  // namespace

TEST_CASE("gravity_third_law") {
    // Three arbitrary bodies: sum of mass-weighted accelerations (the
    // derivative of total momentum) must vanish exactly-ish.
    const std::vector<Body> bodies = {
        Body{0, 3.0, 0.1, Vec2{1.0, 2.0}, Vec2{0.0, 0.0}},
        Body{1, 1.5, 0.1, Vec2{-2.0, 0.5}, Vec2{0.0, 0.0}},
        Body{2, 0.25, 0.1, Vec2{0.75, -1.25}, Vec2{0.0, 0.0}},
    };
    const GravityField field{1.0, 0.05};
    std::vector<Vec2> accel;
    field.computeAccelerations(bodies, accel);

    REQUIRE(accel.size() == bodies.size());
    Vec2 pDot{};
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        pDot += bodies[i].mass * accelAt(accel, i);
    }
    CHECK(pDot.x == Approx(0.0).margin(1e-12));
    CHECK(pDot.y == Approx(0.0).margin(1e-12));
}

TEST_CASE("gravity_inverse_square_direction") {
    // Two bodies on the x-axis; body 0 must accelerate toward body 1 (+x),
    // body 1 toward body 0 (-x). |a0| = G*m1/(d^2+eps^2); m0 cancels.
    const double g = 2.0;
    const double eps = 0.1;
    const double m0 = 0.3;
    const double m1 = 1.7;
    const double d = 3.0;
    const std::vector<Body> bodies = {
        Body{0, m0, 0.1, Vec2{-1.0, 0.0}, Vec2{0.0, 0.0}},
        Body{1, m1, 0.1, Vec2{-1.0 + d, 0.0}, Vec2{0.0, 0.0}},
    };
    const GravityField field{g, eps};
    std::vector<Vec2> accel;
    field.computeAccelerations(bodies, accel);

    REQUIRE(accel.size() == 2);
    CHECK(accelAt(accel, 0).x > 0.0);
    CHECK(accelAt(accel, 1).x < 0.0);
    CHECK(accelAt(accel, 0).y == Approx(0.0).margin(1e-15));
    CHECK(accelAt(accel, 1).y == Approx(0.0).margin(1e-15));

    const double expected = g * m1 / (d * d + eps * eps);
    CHECK(std::abs(accelAt(accel, 0).x) == Approx(expected).epsilon(1e-12));
    // Third law on magnitudes: |a1| = |a0| * m0 / m1.
    CHECK(std::abs(accelAt(accel, 1).x) ==
          Approx(expected * m0 / m1).epsilon(1e-12));
}

TEST_CASE("gravity_softening_finite") {
    // Two bodies at identical positions: d < kZeroDist, pair skipped, so
    // accelerations are finite and exactly zero (no NaN/inf).
    const std::vector<Body> bodies = {
        Body{0, 1.0, 0.1, Vec2{0.5, -0.5}, Vec2{0.0, 0.0}},
        Body{1, 2.0, 0.1, Vec2{0.5, -0.5}, Vec2{0.0, 0.0}},
    };
    const GravityField field{1.0, 0.05};
    std::vector<Vec2> accel;
    field.computeAccelerations(bodies, accel);

    REQUIRE(accel.size() == 2);
    for (std::size_t i = 0; i < accel.size(); ++i) {
        CHECK(std::isfinite(accelAt(accel, i).x));
        CHECK(std::isfinite(accelAt(accel, i).y));
        CHECK(accelAt(accel, i).x == 0.0);
        CHECK(accelAt(accel, i).y == 0.0);
    }
}

TEST_CASE("gravity_softening_limits_force") {
    // d -> 0 (but above kZeroDist): the softening caps the force, |a| ~ G*m/eps^2.
    const double g = 1.0;
    const double eps = 0.05;
    const double m = 4.0;
    const double d = 1e-9;
    const std::vector<Body> bodies = {
        Body{0, 1.0, 0.1, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}},
        Body{1, m, 0.1, Vec2{d, 0.0}, Vec2{0.0, 0.0}},
    };
    const GravityField field{g, eps};
    std::vector<Vec2> accel;
    field.computeAccelerations(bodies, accel);

    REQUIRE(accel.size() == 2);
    const double capped = g * m / (eps * eps);
    CHECK(std::abs(accelAt(accel, 0).x) == Approx(capped).epsilon(0.01));
    CHECK(std::isfinite(accelAt(accel, 1).x));
}

TEST_CASE("gravity_out_accel_resizes_down") {
    // A previous (larger) outAccel must be fully rewritten: after a 3-body
    // call, a 1-body call leaves size 1 with entry 0 == {0,0}.
    const GravityField field{1.0, 0.05};
    std::vector<Vec2> accel;

    const std::vector<Body> three = {
        Body{0, 1.0, 0.1, Vec2{1.0, 0.0}, Vec2{0.0, 0.0}},
        Body{1, 2.0, 0.1, Vec2{-1.0, 1.0}, Vec2{0.0, 0.0}},
        Body{2, 0.5, 0.1, Vec2{0.0, -2.0}, Vec2{0.0, 0.0}},
    };
    field.computeAccelerations(three, accel);
    REQUIRE(accel.size() == 3);

    const std::vector<Body> one = {
        Body{0, 1.0, 0.1, Vec2{4.0, 4.0}, Vec2{0.0, 0.0}},
    };
    field.computeAccelerations(one, accel);

    REQUIRE(accel.size() == 1);
    CHECK(accelAt(accel, 0).x == 0.0);
    CHECK(accelAt(accel, 0).y == 0.0);
}

TEST_CASE("gravity_empty_input") {
    // n = 0: no writes, no crash; out is sized to 0.
    const GravityField field{1.0, 0.05};
    std::vector<Body> bodies;
    std::vector<Vec2> accel(2, Vec2{1.0, 1.0});
    field.computeAccelerations(bodies, accel);
    CHECK(accel.empty());
}
