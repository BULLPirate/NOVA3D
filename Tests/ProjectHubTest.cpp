#include <Nova/Project/Workspace.h>
#include <Nova/Project/ProjectTemplate.h>
#include <Nova/Project/Project.h>
#include <Nova/Scene/SaveGame.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/SceneSerialization.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace {

std::filesystem::path TempRoot(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void RemoveTree(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

} // namespace

TEST(Workspace, DefaultProjectsDirectoryIsUnderHome) {
    const auto dir = Nova::DefaultProjectsDirectory();
    EXPECT_FALSE(dir.empty());
    EXPECT_NE(dir.generic_string().find("NOVA3D"), std::string::npos);
}

TEST(ProjectTemplate, EmptyProjectHasNoHero) {
    const std::filesystem::path root = TempRoot("nova_empty_game");
    RemoveTree(root);
    ASSERT_TRUE(Nova::CreateGameProject(root, "Empty", Nova::ProjectTemplateKind::Empty).Ok);
    Nova::Scene scene;
    ASSERT_TRUE(Nova::LoadSceneFromFile(root / "Assets/Scenes/main.scene.json", scene).Ok);
    EXPECT_FALSE(scene.FindEntityByName("Player").IsValid());
    EXPECT_FALSE(scene.Settings().EnableWaves);
    EXPECT_TRUE(std::filesystem::is_directory(root / "Saves"));
    RemoveTree(root);
}

TEST(ProjectTemplate, ThirdPersonWritesPlayableScene) {
    const std::filesystem::path root = TempRoot("nova_tpp_game");
    RemoveTree(root);
    ASSERT_TRUE(Nova::CreateGameProject(root, "TPP", Nova::ProjectTemplateKind::ThirdPerson).Ok);
    Nova::Scene scene;
    ASSERT_TRUE(Nova::LoadSceneFromFile(root / "Assets/Scenes/main.scene.json", scene).Ok);
    EXPECT_TRUE(scene.FindEntityByName("Player").IsValid());
    EXPECT_FALSE(scene.Settings().EnableWaves);
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
    RemoveTree(root);
}

TEST(ProjectTemplate, KnightBanditsIsCombatGame) {
    const std::filesystem::path root = TempRoot("nova_knight_game");
    RemoveTree(root);
    ASSERT_TRUE(Nova::CreateGameProject(root, "Knight Bandits",
                                        Nova::ProjectTemplateKind::KnightBandits, NOVA_SOURCE_DIR)
                    .Ok);
    Nova::Scene scene;
    ASSERT_TRUE(Nova::LoadSceneFromFile(root / "Assets/Scenes/main.scene.json", scene).Ok);
    EXPECT_TRUE(scene.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(scene.Settings().EnableWaves);
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
    int bandits = 0;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            ++bandits;
        }
    });
    EXPECT_GE(bandits, 4);
    EXPECT_EQ(scene.GetMeshRenderer(scene.FindEntityByName("Player")).AssetPath,
              "Assets/Characters/knight_armed.obj");
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Assets/Characters/knight_armed.obj"));
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Assets/Characters/bandit.obj"));
    RemoveTree(root);
}

TEST(ProjectTemplate, EnsureRepairsSandboxIntoKnightBandits) {
    const std::filesystem::path root = TempRoot("nova_knight_repair");
    RemoveTree(root);
    ASSERT_TRUE(Nova::CreateGameProject(root, "Wrong", Nova::ProjectTemplateKind::ThirdPerson).Ok);
    ASSERT_TRUE(Nova::EnsureGameProject(root, "Knight Bandits",
                                        Nova::ProjectTemplateKind::KnightBandits, NOVA_SOURCE_DIR)
                    .Ok);
    Nova::Scene scene;
    ASSERT_TRUE(Nova::LoadSceneFromFile(root / "Assets/Scenes/main.scene.json", scene).Ok);
    EXPECT_TRUE(scene.Settings().EnableWaves);
    EXPECT_TRUE(scene.FindEntityByName("Bandit 1").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("House A").IsValid());
    RemoveTree(root);
}

TEST(SaveGame, SlotRoundTrip) {
    const std::filesystem::path root = TempRoot("nova_save_game");
    RemoveTree(root);
    ASSERT_TRUE(Nova::CreateGameProject(root, "Save", Nova::ProjectTemplateKind::Empty).Ok);
    Nova::ProjectDescriptor project;
    ASSERT_TRUE(Nova::LoadProject(root, project).Ok);
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    scene.CreateEntity("Marker");
    ASSERT_TRUE(Nova::SaveGameSlot(project, "slot1", scene).Ok);
    Nova::Scene loaded;
    ASSERT_TRUE(Nova::LoadGameSlot(project, "slot1", loaded).Ok);
    EXPECT_TRUE(loaded.FindEntityByName("Marker").IsValid());
    const auto slots = Nova::ListSaveSlots(project);
    ASSERT_EQ(slots.size(), 1u);
    EXPECT_EQ(slots[0], "slot1");
    RemoveTree(root);
}
