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
