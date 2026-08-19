#pragma once

namespace psr {

struct Vec3
{
    int x = 0;
    int y = 0;
    int z = 0;

    friend bool operator==(const Vec3&, const Vec3&) = default;

    friend Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Vec3 operator-(Vec3 v) { return {-v.x, -v.y, -v.z}; }
    friend Vec3 operator*(Vec3 v, int scalar) { return {v.x * scalar, v.y * scalar, v.z * scalar}; }
    friend Vec3 operator*(int scalar, Vec3 v) { return {v.x * scalar, v.y * scalar, v.z * scalar}; }
    friend Vec3 operator/(Vec3 v, int scalar) { return {v.x / scalar, v.y / scalar, v.z / scalar}; }
};

} // namespace psr
