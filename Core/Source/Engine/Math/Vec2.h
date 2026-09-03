#pragma once

#include <cstdlib>

namespace psr {

struct Vec2
{
    int x = 0;
    int y = 0;

    friend bool operator==(const Vec2&, const Vec2&) = default;

    // Componentwise vector arithmetic plus integer scalar scale/divide. Kept
    // minimal and inline. Scalar / truncates toward zero.
    friend Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend Vec2 operator-(Vec2 v) { return {-v.x, -v.y}; }
    friend Vec2 operator*(Vec2 v, int scalar) { return {v.x * scalar, v.y * scalar}; }
    friend Vec2 operator*(int scalar, Vec2 v) { return {v.x * scalar, v.y * scalar}; }
    friend Vec2 operator/(Vec2 v, int scalar) { return {v.x / scalar, v.y / scalar}; }
};

// Tile-grid distance (4-directional movement cost) between a and b.
inline int ManhattanDistance(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

} // namespace psr
