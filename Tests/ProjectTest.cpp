#include <Nova/Project/Project.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace {

std::filesystem::path TempProjectDir() {
    return std::filesystem::temp_directory_path() / "nova_project_test";
}

void RemoveTree(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

} // namespace

TEST(Project, InitializeAndLoadRoundTrip) {
    const std::filesystem::path root = TempProjectDir();
    RemoveTree(root);

    ASSERT_TRUE(Nova::InitializeNewProject(root, "Test Game").Ok);
    ASSERT_TRUE(Nova::IsNovaProjectRoot(root));

    Nova::ProjectDescriptor loaded;
    ASSERT_TRUE(Nova::LoadProject(root, loaded).Ok);
    EXPECT_EQ(loaded.Name, "Test Game");
    EXPECT_EQ(loaded.StartupScene.generic_string(), "Assets/Scenes/main.scene.json");

    loaded.LastOpenedScene = "Assets/Scenes/custom.scene.json";
    ASSERT_TRUE(Nova::SaveProject(loaded).Ok);

    Nova::ProjectDescriptor again;
    ASSERT_TRUE(Nova::LoadProject(root, again).Ok);
    EXPECT_EQ(again.LastOpenedScene.generic_string(), "Assets/Scenes/custom.scene.json");

    RemoveTree(root);
}

TEST(Project, MakeProjectRelativePath) {
    Nova::ProjectDescriptor project;
    project.Root = std::filesystem::path("/game/root");

    const auto rel = Nova::MakeProjectRelativePath(project, "/game/root/Assets/Scenes/a.json");
    EXPECT_EQ(rel.generic_string(), "Assets/Scenes/a.json");
}

TEST(Project, BundledRepoIsValidProject) {
    ASSERT_TRUE(std::filesystem::exists(
        std::filesystem::path(NOVA_SOURCE_DIR) / ".nova/project.json"));

    Nova::ProjectDescriptor project;
    ASSERT_TRUE(Nova::LoadProject(std::filesystem::path(NOVA_SOURCE_DIR), project).Ok);
    EXPECT_TRUE(std::filesystem::exists(project.LastOpenedSceneAbsolute()));
}
