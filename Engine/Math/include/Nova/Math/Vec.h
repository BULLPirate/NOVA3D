#pragma once

#include <cmath>
#include <cstdint>

namespace Nova {

// ── Vec2 ───────────────────────────────────────────────────────────────────────
struct Vec2 {
    float x = 0.0f, y = 0.0f;

    Vec2() = default;
    constexpr Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s)       const { return {x * s, y * s}; }
    Vec2 operator/(float s)       const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float LengthSq() const { return x*x + y*y; }
    float Length()   const { return std::sqrt(LengthSq()); }
    Vec2  Normalized() const { float l = Length(); return l > 0 ? *this / l : Vec2{}; }
    float Dot(const Vec2& o) const { return x*o.x + y*o.y; }
};

// ── Vec3 ───────────────────────────────────────────────────────────────────────
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s)       const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s)       const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float LengthSq() const { return x*x + y*y + z*z; }
    float Length()   const { return std::sqrt(LengthSq()); }
    Vec3  Normalized() const { float l = Length(); return l > 0 ? *this / l : Vec3{}; }
    float Dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }
    Vec3  Cross(const Vec3& o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }
};

// ── Vec4 ───────────────────────────────────────────────────────────────────────
struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    Vec4() = default;
    constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec4 operator+(const Vec4& o) const { return {x+o.x, y+o.y, z+o.z, w+o.w}; }
    Vec4 operator*(float s)       const { return {x*s, y*s, z*s, w*s}; }
    float Dot(const Vec4& o) const { return x*o.x + y*o.y + z*o.z + w*o.w; }

    Vec3 xyz() const { return {x, y, z}; }
};

} // namespace Nova
