#pragma once

#include <cmath>

namespace nbody::core {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

constexpr Vec2 operator-(Vec2 a, Vec2 b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

constexpr Vec2 operator*(Vec2 a, double s) {
    return Vec2{a.x * s, a.y * s};
}

constexpr Vec2 operator*(double s, Vec2 a) {
    return a * s;
}

constexpr Vec2 operator/(Vec2 a, double s) {
    return Vec2{a.x / s, a.y / s};
}

constexpr Vec2 operator-(Vec2 a) {
    return Vec2{-a.x, -a.y};
}

constexpr Vec2& operator+=(Vec2& a, Vec2 b) {
    a = a + b;
    return a;
}

constexpr Vec2& operator-=(Vec2& a, Vec2 b) {
    a = a - b;
    return a;
}

constexpr Vec2& operator*=(Vec2& a, double s) {
    a = a * s;
    return a;
}

constexpr double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

constexpr double lengthSq(Vec2 a) {
    return dot(a, a);
}

inline double length(Vec2 a) {
    return std::sqrt(lengthSq(a));
}

}  // namespace nbody::core
