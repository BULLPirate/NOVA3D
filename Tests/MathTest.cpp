#include <gtest/gtest.h>
#include <Nova/Math/Math.h>

using namespace Nova;

TEST(Vec3, AddSubtract) {
    Vec3 a{1, 2, 3};
    Vec3 b{4, 5, 6};
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3, CrossProduct) {
    Vec3 x{1, 0, 0};
    Vec3 y{0, 1, 0};
    Vec3 z = x.Cross(y);
    EXPECT_FLOAT_EQ(z.x, 0.0f);
    EXPECT_FLOAT_EQ(z.y, 0.0f);
    EXPECT_FLOAT_EQ(z.z, 1.0f);
}

TEST(Vec3, DotProduct) {
    Vec3 a{1, 2, 3};
    Vec3 b{4, 5, 6};
    EXPECT_FLOAT_EQ(a.Dot(b), 32.0f);
}

TEST(Vec3, Normalize) {
    Vec3 a{3, 0, 0};
    Vec3 n = a.Normalized();
    EXPECT_FLOAT_EQ(n.x, 1.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(Vec3, Length) {
    Vec3 a{3, 4, 0};
    EXPECT_FLOAT_EQ(a.Length(), 5.0f);
}

TEST(Mat4, Identity) {
    Mat4 I = Mat4::Identity();
    EXPECT_FLOAT_EQ(I.m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(I.m[1][1], 1.0f);
    EXPECT_FLOAT_EQ(I.m[2][2], 1.0f);
    EXPECT_FLOAT_EQ(I.m[3][3], 1.0f);
    EXPECT_FLOAT_EQ(I.m[0][1], 0.0f);
}

TEST(Mat4, Translate) {
    Mat4 T = Mat4::Translate({10, 20, 30});
    EXPECT_FLOAT_EQ(T.m[3][0], 10.0f);
    EXPECT_FLOAT_EQ(T.m[3][1], 20.0f);
    EXPECT_FLOAT_EQ(T.m[3][2], 30.0f);
}

TEST(Mat4, MultiplyIdentity) {
    Mat4 A = Mat4::Translate({1, 2, 3});
    Mat4 R = A * Mat4::Identity();
    EXPECT_FLOAT_EQ(R.m[3][0], 1.0f);
    EXPECT_FLOAT_EQ(R.m[3][1], 2.0f);
    EXPECT_FLOAT_EQ(R.m[3][2], 3.0f);
}

TEST(Mat4, OrthographicMetalMapsDepthToZeroOne) {
    Mat4 ortho = Mat4::OrthographicMetal(-1.0f, 1.0f, -1.0f, 1.0f, 0.5f, 10.0f);
    Vec4 viewPoint{0.0f, 0.0f, -5.0f, 1.0f};
    Vec4 clip = ortho * viewPoint;
    EXPECT_NEAR(clip.w, 1.0f, 1e-5f);
    const float ndcZ = clip.z / clip.w;
    EXPECT_GT(ndcZ, 0.0f);
    EXPECT_LT(ndcZ, 1.0f);
}

TEST(Mat4, Perspective) {
    Mat4 P = Mat4::Perspective(Radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
    // m[0][0] should be ~0.9743 for 16:9 @ 60°
    EXPECT_NEAR(P.m[0][0], 0.9743f, 0.01f);
    // m[2][3] = -1 (perspective divide flag)
    EXPECT_FLOAT_EQ(P.m[2][3], -1.0f);
}

TEST(Mat4, Vec4Multiply) {
    Mat4 T = Mat4::Translate({5, 0, 0});
    Vec4 p{0, 0, 0, 1};
    Vec4 r = T * p;
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
    EXPECT_FLOAT_EQ(r.z, 0.0f);
    EXPECT_FLOAT_EQ(r.w, 1.0f);
}

TEST(MathUtils, RadiansDegrees) {
    EXPECT_FLOAT_EQ(Radians(180.0f), PI);
    EXPECT_FLOAT_EQ(Degrees(PI), 180.0f);
}

TEST(MathUtils, Clamp) {
    EXPECT_FLOAT_EQ(Clamp(5.0f, 0.0f, 10.0f), 5.0f);
    EXPECT_FLOAT_EQ(Clamp(-1.0f, 0.0f, 10.0f), 0.0f);
    EXPECT_FLOAT_EQ(Clamp(15.0f, 0.0f, 10.0f), 10.0f);
}

TEST(MathUtils, Lerp) {
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 1.0f), 10.0f);
}

// ── Quat ───────────────────────────────────────────────────────────────────────

TEST(Quat, Identity) {
    Quat q = Quat::Identity();
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

TEST(Quat, FromAxisAngle90) {
    // 90° around Y: (0,0,-1) should rotate to (1,0,0)
    Quat q = Quat::FromAxisAngle({0, 1, 0}, Radians(90.0f));
    Vec3 forward{0, 0, -1};
    Vec3 result = q.Rotate(forward);
    EXPECT_NEAR(result.x, -1.0f, 0.001f);
    EXPECT_NEAR(result.y,  0.0f, 0.001f);
    EXPECT_NEAR(result.z,  0.0f, 0.001f);
}

TEST(Quat, Normalize) {
    Quat q{1, 2, 3, 4};
    Quat n = q.Normalized();
    EXPECT_NEAR(n.Length(), 1.0f, 0.001f);
}

TEST(Quat, ConjugateInverse) {
    Quat q = Quat::FromAxisAngle({0, 1, 0}, Radians(45.0f));
    Quat inv = q.Inverse();
    Quat product = q * inv;
    EXPECT_NEAR(product.x, 0.0f, 0.001f);
    EXPECT_NEAR(product.y, 0.0f, 0.001f);
    EXPECT_NEAR(product.z, 0.0f, 0.001f);
    EXPECT_NEAR(product.w, 1.0f, 0.001f);
}

TEST(Quat, ToMat4Identity) {
    Quat q = Quat::Identity();
    Mat4 m = q.ToMat4();
    Mat4 I = Mat4::Identity();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_NEAR(m.m[c][r], I.m[c][r], 0.001f);
}

TEST(Quat, ShortestRotationMapsDirection) {
    const Quat q = Quat::ShortestRotation({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const Vec3 rotated = q.Rotate({1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(rotated.x, 0.0f, 0.01f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.01f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.01f);
}

TEST(Quat, EulerRoundTrip) {
    const Quat original =
        Quat::FromEulerYXZRadians({Radians(15.0f), Radians(-30.0f), Radians(45.0f)});
    const Vec3 euler = original.ToEulerYXZRadians();
    const Quat restored = Quat::FromEulerYXZRadians(euler);
    const float dot = std::abs(original.Dot(restored));
    EXPECT_NEAR(dot, 1.0f, 0.01f);
}

// ── Transform ──────────────────────────────────────────────────────────────────

TEST(Mat4, InverseRoundTripTranslate) {
    const Mat4 t = Mat4::Translate({2.0f, -3.0f, 4.0f});
    const Vec3 p = t.Inverse().TransformPoint({2.0f, -3.0f, 4.0f});
    EXPECT_NEAR(p.x, 0.0f, 0.001f);
    EXPECT_NEAR(p.y, 0.0f, 0.001f);
    EXPECT_NEAR(p.z, 0.0f, 0.001f);
}

TEST(Transform, TranslateOnly) {
    Transform t;
    t.Position = {10, 20, 30};
    Mat4 m = t.ToMatrix();
    EXPECT_NEAR(m.m[3][0], 10.0f, 0.001f);
    EXPECT_NEAR(m.m[3][1], 20.0f, 0.001f);
    EXPECT_NEAR(m.m[3][2], 30.0f, 0.001f);
}

TEST(Transform, ScaleOnly) {
    Transform t;
    t.Scale = {2, 3, 4};
    Mat4 m = t.ToMatrix();
    EXPECT_NEAR(m.m[0][0], 2.0f, 0.001f);
    EXPECT_NEAR(m.m[1][1], 3.0f, 0.001f);
    EXPECT_NEAR(m.m[2][2], 4.0f, 0.001f);
}

TEST(Transform, TRSCombined) {
    Transform t;
    t.Position = {5, 0, 0};
    t.Rotation = Quat::FromAxisAngle({0, 1, 0}, Radians(90.0f));
    t.Scale = {1, 1, 1};
    Mat4 m = t.ToMatrix();
    // Rotating (0,0,-1) by 90° around Y and translating by (5,0,0)
    Vec4 p{0, 0, -1, 1};
    Vec4 r = m * p;
    EXPECT_NEAR(r.x, 4.0f, 0.01f); // 5 + (-1)
    EXPECT_NEAR(r.y, 0.0f, 0.01f);
    EXPECT_NEAR(r.z, 0.0f, 0.01f);
}
