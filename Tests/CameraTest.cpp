#include <gtest/gtest.h>
#include <Nova/Renderer/Camera.h>

using namespace Nova;

TEST(Camera, ViewProjectionNotIdentity) {
    Camera cam;
    cam.SetOrbit(0.0f, 3.0f, 0.0f);
    Mat4 vp = cam.GetViewProjectionMatrix();
    Mat4 I = Mat4::Identity();
    EXPECT_NE(vp.m[0][0], I.m[0][0]);
}

TEST(Camera, OrbitChangesEyePosition) {
    Camera cam;
    cam.SetOrbit(0.0f, 2.0f, 0.0f);
    const float z0 = cam.Position.z;

    cam.SetOrbit(3.14159265f * 0.5f, 2.0f, 0.0f);
    EXPECT_NE(cam.Position.x, 0.0f);
    EXPECT_NE(cam.Position.z, z0);
}
