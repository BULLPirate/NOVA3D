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

TEST(Camera, ProjectWorldToViewportCenter) {
    Camera cam;
    cam.Position = {0.0f, 0.0f, 5.0f};
    cam.Target = {0.0f, 0.0f, 0.0f};
    cam.Aspect = 1.0f;

    float sx = 0.0f;
    float sy = 0.0f;
    EXPECT_TRUE(ProjectWorldToViewport(cam.GetViewProjectionMatrix(), {0.0f, 0.0f, 0.0f},
                                       800.0f, 600.0f, sx, sy));
    EXPECT_NEAR(sx, 400.0f, 2.0f);
    EXPECT_NEAR(sy, 300.0f, 2.0f);
}

TEST(Camera, OrbitChangesEyePosition) {
    Camera cam;
    cam.SetOrbit(0.0f, 2.0f, 0.0f);
    const float z0 = cam.Position.z;

    cam.SetOrbit(3.14159265f * 0.5f, 2.0f, 0.0f);
    EXPECT_NE(cam.Position.x, 0.0f);
    EXPECT_NE(cam.Position.z, z0);
}
