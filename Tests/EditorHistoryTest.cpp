#include <Editor/EditorHistory.h>

#include <gtest/gtest.h>

TEST(EditorHistory, UndoRestoresDestroyedEntity) {
    Nova::Scene scene = Nova::Scene::CreateDemoLevel();
    Nova::Editor::EditorHistory history;
    history.Push(scene);

    const Nova::Entity cube = scene.FindEntityByName("Cube");
    ASSERT_TRUE(cube.IsValid());
    scene.DestroyEntity(cube);
    EXPECT_FALSE(scene.FindEntityByName("Cube").IsValid());

    ASSERT_TRUE(history.Undo(scene));
    EXPECT_TRUE(scene.FindEntityByName("Cube").IsValid());
}

TEST(EditorHistory, RedoReappliesChange) {
    Nova::Scene scene = Nova::Scene::CreateDemoLevel();
    Nova::Editor::EditorHistory history;
    history.Push(scene);
    scene.GetTransform(scene.FindEntityByName("Cube")).Position.x = 4.0f;
    ASSERT_TRUE(history.Undo(scene));
    EXPECT_NEAR(scene.GetTransform(scene.FindEntityByName("Cube")).Position.x, 0.0f, 0.01f);
    ASSERT_TRUE(history.Redo(scene));
    EXPECT_NEAR(scene.GetTransform(scene.FindEntityByName("Cube")).Position.x, 4.0f, 0.01f);
}
