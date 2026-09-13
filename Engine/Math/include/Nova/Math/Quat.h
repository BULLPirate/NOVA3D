#pragma once

#include "Vec.h"
#include "Mat4.h"
#include <cmath>

namespace Nova {

/// Unit quaternion for rotation. Stored as (x, y, z, w).
struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Quat() = default;
    constexpr Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static Quat Identity() { return {0, 0, 0, 1}; }

    /// Create quaternion from axis-angle (radians)
    static Quat FromAxisAngle(const Vec3& axis, float radians) {
        float half = radians * 0.5f;
        float s = std::sin(half);
        Vec3 n = axis.Normalized();
        return {n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    static Quat FromEuler(float pitch, float yaw, float roll) {
        float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
        float cy = std::cos(yaw   * 0.5f), sy = std::sin(yaw   * 0.5f);
        float cr = std::cos(roll  * 0.5f), sr = std::sin(roll  * 0.5f);
        return {
            sp * cy * cr - cp * sy * sr,
            cp * sy * cr + sp * cy * sr,
            cp * cy * sr - sp * sy * cr,
            cp * cy * cr + sp * sy * sr
        };
    }

    Quat operator*(const Quat& q) const {
        return {
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w,
            w*q.w - x*q.x - y*q.y - z*q.z
        };
    }

    float LengthSq() const { return x*x + y*y + z*z + w*w; }
    float Length()   const { return std::sqrt(LengthSq()); }

    Quat Normalized() const {
        float l = Length();
        return l > 0 ? Quat{x/l, y/l, z/l, w/l} : Identity();
    }

    Quat Conjugate() const { return {-x, -y, -z, w}; }

    Quat Inverse() const {
        float ls = LengthSq();
        return ls > 0 ? Quat{-x/ls, -y/ls, -z/ls, w/ls} : Identity();
    }

    float Dot(const Quat& q) const { return x*q.x + y*q.y + z*q.z + w*q.w; }

    /// Rotate a vector by this quaternion
    Vec3 Rotate(const Vec3& v) const {
        Quat vq{v.x, v.y, v.z, 0};
        Quat result = *this * vq * Conjugate();
        return {result.x, result.y, result.z};
    }

    /// Convert to 4x4 rotation matrix
    Mat4 ToMat4() const {
        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;

        Mat4 result;
        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);
        result.m[3][3] = 1.0f;
        return result;
    }
};

} // namespace Nova