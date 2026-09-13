#pragma once

#include "Vec.h"
#include "Mat4.h"
#include "Quat.h"

namespace Nova {

/// Convenience transform: position + rotation + scale -> Mat4
struct Transform {
    Vec3  Position{0, 0, 0};
    Quat  Rotation = Quat::Identity();
    Vec3  Scale{1, 1, 1};

    Transform() = default;
    Transform(const Vec3& pos, const Quat& rot = Quat::Identity(), const Vec3& scale = {1,1,1})
        : Position(pos), Rotation(rot), Scale(scale) {}

    Mat4 ToMatrix() const {
        Mat4 S = Mat4::Scale(Scale);
        Mat4 R = Rotation.ToMat4();
        Mat4 T = Mat4::Translate(Position);
        return T * R * S;  // TRS order
    }
};

} // namespace Nova