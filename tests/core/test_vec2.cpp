#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nbody/core/Body.hpp"
#include "nbody/core/Vec2.hpp"

using namespace nbody::core;

// Compile-time proof that the free operators are constexpr.
static_assert(dot(Vec2{1.0, 2.0}, Vec2{3.0, 4.0}) == 11.0);
static_assert(lengthSq(Vec2{3.0, 4.0}) == 25.0);
static_assert((Vec2{1.0, 2.0} + Vec2{3.0, 4.0}).x == 4.0);
static_assert((2.0 * Vec2{1.0, 2.0}).y == 4.0);

TEST_CASE("vec_operators") {
    const Vec2 a{1.0, 2.0};
    const Vec2 b{3.0, 4.0};

    SECTION("addition") {
        const Vec2 s = a + b;
        CHECK(s.x == 4.0);
        CHECK(s.y == 6.0);
    }

    SECTION("subtraction") {
        const Vec2 d = b - a;
        CHECK(d.x == 2.0);
        CHECK(d.y == 2.0);
    }

    SECTION("scalar multiplication both orders") {
        const Vec2 m1 = a * 2.0;
        CHECK(m1.x == 2.0);
        CHECK(m1.y == 4.0);

        const Vec2 m2 = 2.0 * a;
        CHECK(m2.x == 2.0);
        CHECK(m2.y == 4.0);
    }

    SECTION("division") {
        const Vec2 q = b / 2.0;
        CHECK(q.x == 1.5);
        CHECK(q.y == 2.0);
    }

    SECTION("unary minus") {
        const Vec2 n = -a;
        CHECK(n.x == -1.0);
        CHECK(n.y == -2.0);
    }

    SECTION("compound assignment") {
        Vec2 p = a;
        p += b;
        CHECK(p.x == 4.0);
        CHECK(p.y == 6.0);
        p -= b;
        CHECK(p.x == 1.0);
        CHECK(p.y == 2.0);
        p *= 3.0;
        CHECK(p.x == 3.0);
        CHECK(p.y == 6.0);
    }

    SECTION("dot") {
        CHECK(dot(a, b) == 11.0);
    }

    SECTION("lengthSq") {
        CHECK(lengthSq(Vec2{3.0, 4.0}) == 25.0);
    }

    SECTION("length") {
        CHECK(length(Vec2{3.0, 4.0}) == Catch::Approx(5.0));
    }
}

TEST_CASE("body_defaults") {
    const Body body{};
    CHECK(body.id == 0U);
    CHECK(body.mass > 0.0);
    CHECK(body.radius > 0.0);
}
