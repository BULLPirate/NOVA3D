#pragma once

/// Convenience header — include everything from Nova::Math

#include "Vec.h"
#include "Mat4.h"

#include <cmath>

namespace Nova {

constexpr float PI      = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

inline float Radians(float degrees) { return degrees * DEG2RAD; }
inline float Degrees(float radians) { return radians * RAD2DEG; }

inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace Nova
