#pragma once

#include "Vec.h"
#include <cstring>
#include <cmath>

namespace Nova {

/// Column-major 4x4 matrix — same layout as Metal/OpenGL.
/// m[col][row]
struct Mat4 {
    float m[4][4] = {};

    Mat4() = default;

    /// Identity matrix
    static Mat4 Identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 Translate(const Vec3& t) {
        Mat4 r = Identity();
        r.m[3][0] = t.x;
        r.m[3][1] = t.y;
        r.m[3][2] = t.z;
        return r;
    }

    static Mat4 Scale(const Vec3& s) {
        Mat4 r;
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        r.m[2][2] = s.z;
        r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 RotateX(float radians) {
        Mat4 r = Identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[1][1] =  c; r.m[2][1] = s;
        r.m[1][2] = -s; r.m[2][2] = c;
        return r;
    }

    static Mat4 RotateY(float radians) {
        Mat4 r = Identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0][0] =  c; r.m[2][0] = -s;
        r.m[0][2] =  s; r.m[2][2] =  c;
        return r;
    }

    static Mat4 RotateZ(float radians) {
        Mat4 r = Identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0][0] =  c; r.m[1][0] = s;
        r.m[0][1] = -s; r.m[1][1] = c;
        return r;
    }

    static Mat4 Perspective(float fovYRadians, float aspect, float near, float far) {
        Mat4 r;
        float tanHalf = std::tan(fovYRadians * 0.5f);
        r.m[0][0] = 1.0f / (aspect * tanHalf);
        r.m[1][1] = 1.0f / tanHalf;
        r.m[2][2] = -(far + near) / (far - near);
        r.m[2][3] = -1.0f;
        r.m[3][2] = -(2.0f * far * near) / (far - near);
        return r;
    }

    /// Perspective for Metal / D3D clip space (Z in [0, 1] after divide).
    static Mat4 PerspectiveMetal(float fovYRadians, float aspect, float near, float far) {
        Mat4 r;
        float tanHalf = std::tan(fovYRadians * 0.5f);
        r.m[0][0] = 1.0f / (aspect * tanHalf);
        r.m[1][1] = 1.0f / tanHalf;
        r.m[2][2] = far / (near - far);
        r.m[2][3] = -1.0f;
        r.m[3][2] = (near * far) / (near - far);
        return r;
    }

    /// Orthographic projection for Metal clip space (Z in [0, 1] after divide).
    static Mat4 OrthographicMetal(float left, float right, float bottom, float top,
                                  float nearPlane, float farPlane) {
        Mat4 r = Identity();
        r.m[0][0] = 2.0f / (right - left);
        r.m[1][1] = 2.0f / (top - bottom);
        r.m[2][2] = -1.0f / (farPlane - nearPlane);
        r.m[3][0] = -(right + left) / (right - left);
        r.m[3][1] = -(top + bottom) / (top - bottom);
        r.m[3][2] = -nearPlane / (farPlane - nearPlane);
        return r;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& worldUp) {
        Vec3 f = (target - eye).Normalized();
        Vec3 r = f.Cross(worldUp).Normalized();
        Vec3 u = r.Cross(f);

        Mat4 result = Identity();
        result.m[0][0] =  r.x; result.m[1][0] =  r.y; result.m[2][0] =  r.z;
        result.m[0][1] =  u.x; result.m[1][1] =  u.y; result.m[2][1] =  u.z;
        result.m[0][2] = -f.x; result.m[1][2] = -f.y; result.m[2][2] = -f.z;
        result.m[3][0] = -r.Dot(eye);
        result.m[3][1] = -u.Dot(eye);
        result.m[3][2] =  f.Dot(eye);
        return result;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += m[k][row] * b.m[c][k];
                r.m[c][row] = sum;
            }
        return r;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z + m[3][0]*v.w,
            m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z + m[3][1]*v.w,
            m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z + m[3][2]*v.w,
            m[0][3]*v.x + m[1][3]*v.y + m[2][3]*v.z + m[3][3]*v.w
        };
    }

    Vec3 TransformPoint(const Vec3& p) const {
        const Vec4 r = *this * Vec4{p.x, p.y, p.z, 1.0f};
        if (std::fabs(r.w) > 1e-8f) {
            return {r.x / r.w, r.y / r.w, r.z / r.w};
        }
        return {r.x, r.y, r.z};
    }

    Vec3 TransformVector(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z,
            m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z,
            m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z
        };
    }

    /// Gauss-Jordan inverse. Returns Identity if the matrix is singular.
    Mat4 Inverse() const {
        float a[4][8] = {};
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                a[r][c] = m[c][r];
            }
            a[c][c + 4] = 1.0f;
        }

        for (int col = 0; col < 4; ++col) {
            int pivot = col;
            float best = std::fabs(a[col][col]);
            for (int row = col + 1; row < 4; ++row) {
                const float v = std::fabs(a[row][col]);
                if (v > best) {
                    best = v;
                    pivot = row;
                }
            }
            if (best < 1e-8f) {
                return Identity();
            }
            if (pivot != col) {
                for (int k = 0; k < 8; ++k) {
                    const float tmp = a[col][k];
                    a[col][k] = a[pivot][k];
                    a[pivot][k] = tmp;
                }
            }
            const float invPivot = 1.0f / a[col][col];
            for (int k = 0; k < 8; ++k) {
                a[col][k] *= invPivot;
            }
            for (int row = 0; row < 4; ++row) {
                if (row == col) continue;
                const float factor = a[row][col];
                for (int k = 0; k < 8; ++k) {
                    a[row][k] -= factor * a[col][k];
                }
            }
        }

        Mat4 inv;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                inv.m[c][r] = a[r][c + 4];
            }
        }
        return inv;
    }

    const float* Data() const { return &m[0][0]; }
};

} // namespace Nova
