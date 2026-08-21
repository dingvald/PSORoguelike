#pragma once

namespace psr {

struct Vec2f
{
    float x = 0.0f;
    float y = 0.0f;

    friend bool operator==(const Vec2f&, const Vec2f&) = default;

    // Componentwise vector arithmetic plus float scalar scale. Kept minimal
    // and inline, mirroring Vec2's shape for the float-space (sub-tile
    // offset) case.
    friend Vec2f operator+(Vec2f a, Vec2f b) { return {a.x + b.x, a.y + b.y}; }
    friend Vec2f operator-(Vec2f a, Vec2f b) { return {a.x - b.x, a.y - b.y}; }
    friend Vec2f operator-(Vec2f v) { return {-v.x, -v.y}; }
    friend Vec2f operator*(Vec2f v, float scalar) { return {v.x * scalar, v.y * scalar}; }
    friend Vec2f operator*(float scalar, Vec2f v) { return {v.x * scalar, v.y * scalar}; }
};

} // namespace psr
