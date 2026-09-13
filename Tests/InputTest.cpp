#include <gtest/gtest.h>
#include <Nova/Core/Input.h>

using namespace Nova;

TEST(Input, KeyDownUp) {
    Input input;
    input.BeginFrame();

    EXPECT_FALSE(input.IsKeyDown(KeyCode::A));
    input.OnKeyDown(KeyCode::A);
    EXPECT_TRUE(input.IsKeyDown(KeyCode::A));
    EXPECT_TRUE(input.IsKeyPressed(KeyCode::A));

    input.BeginFrame();
    EXPECT_TRUE(input.IsKeyDown(KeyCode::A));  // still held
    EXPECT_FALSE(input.IsKeyPressed(KeyCode::A)); // not "just pressed"

    input.OnKeyUp(KeyCode::A);
    EXPECT_FALSE(input.IsKeyDown(KeyCode::A));
    EXPECT_TRUE(input.IsKeyReleased(KeyCode::A));
}

TEST(Input, MouseButton) {
    Input input;
    input.BeginFrame();

    EXPECT_FALSE(input.IsMouseButtonDown(MouseButton::Left));
    input.OnMouseButtonDown(MouseButton::Left);
    EXPECT_TRUE(input.IsMouseButtonDown(MouseButton::Left));
    EXPECT_TRUE(input.IsMouseButtonPressed(MouseButton::Left));

    input.BeginFrame();
    EXPECT_TRUE(input.IsMouseButtonDown(MouseButton::Left));
    EXPECT_FALSE(input.IsMouseButtonPressed(MouseButton::Left));

    input.OnMouseButtonUp(MouseButton::Left);
    EXPECT_FALSE(input.IsMouseButtonDown(MouseButton::Left));
}

TEST(Input, MouseMove) {
    Input input;
    input.BeginFrame();

    input.OnMouseMove(100.0f, 200.0f, 50.0f, 30.0f);
    EXPECT_FLOAT_EQ(input.GetMouseX(), 100.0f);
    EXPECT_FLOAT_EQ(input.GetMouseY(), 200.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDeltaX(), 50.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDeltaY(), 30.0f);

    input.OnMouseMove(120.0f, 210.0f, 20.0f, 10.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDeltaX(), 70.0f); // accumulated
    EXPECT_FLOAT_EQ(input.GetMouseDeltaY(), 40.0f);

    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetMouseX(), 120.0f); // position preserved
    EXPECT_FLOAT_EQ(input.GetMouseDeltaX(), 0.0f); // delta reset
}

TEST(Input, MouseScroll) {
    Input input;
    input.BeginFrame();

    input.OnMouseScroll(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(input.GetScrollY(), 1.0f);

    input.OnMouseScroll(0.0f, 2.0f);
    EXPECT_FLOAT_EQ(input.GetScrollY(), 3.0f); // accumulated

    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetScrollY(), 0.0f); // reset
}

TEST(Input, MultipleKeys) {
    Input input;
    input.BeginFrame();

    input.OnKeyDown(KeyCode::W);
    input.OnKeyDown(KeyCode::LShift);

    EXPECT_TRUE(input.IsKeyDown(KeyCode::W));
    EXPECT_TRUE(input.IsKeyDown(KeyCode::LShift));
    EXPECT_FALSE(input.IsKeyDown(KeyCode::S));

    input.BeginFrame();
    input.OnKeyUp(KeyCode::W);
    EXPECT_FALSE(input.IsKeyDown(KeyCode::W));
    EXPECT_TRUE(input.IsKeyDown(KeyCode::LShift)); // still held
}
