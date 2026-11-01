/// @file
/// Minimal 2-D vector value type plus its constexpr free-operator algebra,
/// shared by all of nbody::core. Vec2 carries no invariants beyond two
/// doubles; all geometry lives in the free functions below, every one of
/// them constexpr except length() (which needs std::sqrt).

#pragma once

#include <cmath>

namespace nbody::core {

/// @brief A 2-D vector/point in world coordinates (y increases upward).
struct Vec2 {
    double x = 0.0;  ///< x component, world units
    double y = 0.0;  ///< y component, world units
};

/// @brief Component-wise sum a + b.
constexpr Vec2 operator+(Vec2 a, Vec2 b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

/// @brief Component-wise difference a - b.
constexpr Vec2 operator-(Vec2 a, Vec2 b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

/// @brief Scalar multiplication: scales both components of a by s.
constexpr Vec2 operator*(Vec2 a, double s) {
    return Vec2{a.x * s, a.y * s};
}

/// @brief Scalar multiplication with the scalar on the left.
constexpr Vec2 operator*(double s, Vec2 a) {
    return a * s;
}

/// @brief Scalar division: scales both components of a by 1/s.
constexpr Vec2 operator/(Vec2 a, double s) {
    return Vec2{a.x / s, a.y / s};
}

/// @brief Component-wise negation.
constexpr Vec2 operator-(Vec2 a) {
    return Vec2{-a.x, -a.y};
}

/// @brief Accumulate a += b. @return Reference to a, for chaining.
constexpr Vec2& operator+=(Vec2& a, Vec2 b) {
    a = a + b;
    return a;
}

/// @brief Deaccumulate a -= b. @return Reference to a, for chaining.
constexpr Vec2& operator-=(Vec2& a, Vec2 b) {
    a = a - b;
    return a;
}

/// @brief Scale a in place by s. @return Reference to a, for chaining.
constexpr Vec2& operator*=(Vec2& a, double s) {
    a = a * s;
    return a;
}

/// @brief Euclidean dot product a.x*b.x + a.y*b.y.
/// @return The scalar projection product of a onto b.
constexpr double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

/// @brief Squared Euclidean length |a|^2; comparison-friendly (no sqrt).
constexpr double lengthSq(Vec2 a) {
    return dot(a, a);
}

/// @brief Euclidean length |a|. Not constexpr: needs std::sqrt.
inline double length(Vec2 a) {
    return std::sqrt(lengthSq(a));
}

}  // namespace nbody::core
