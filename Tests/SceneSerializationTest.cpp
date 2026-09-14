#include <gtest/gtest.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path DemoScenePath() {
    return std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Scenes/demo.scene.json";
}

} // namespace

TEST(SceneSerialization, RoundTripDemoLevelString) {
    const Nova::Scene original = Nova::Scene::CreateDemoLevel();
    const std::string json = Nova::SerializeSceneToString(original);

    Nova::Scene loaded;
    const Nova::SceneIOResult io = Nova::DeserializeSceneFromString(json, loaded);
    ASSERT_TRUE(io.Ok) << io.Error;
    EXPECT_TRUE(Nova::ScenesEquivalent(original, loaded));
}

TEST(SceneSerialization, RoundTripDemoLevelFile) {
    const Nova::Scene original = Nova::Scene::CreateDemoLevel();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "nova_scene_roundtrip_test.json";

    const Nova::SceneIOResult save = Nova::SaveSceneToFile(original, path);
    ASSERT_TRUE(save.Ok) << save.Error;

    Nova::Scene loaded;
    const Nova::SceneIOResult load = Nova::LoadSceneFromFile(path, loaded);
    ASSERT_TRUE(load.Ok) << load.Error;
    EXPECT_TRUE(Nova::ScenesEquivalent(original, loaded));

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(SceneSerialization, BundledDemoSceneMatchesBuiltin) {
    ASSERT_TRUE(std::filesystem::exists(DemoScenePath()));

    Nova::Scene fromFile;
    const Nova::SceneIOResult load = Nova::LoadSceneFromFile(DemoScenePath(), fromFile);
    ASSERT_TRUE(load.Ok) << load.Error;

    const Nova::Scene builtin = Nova::Scene::CreateDemoLevel();
    EXPECT_TRUE(Nova::ScenesEquivalent(builtin, fromFile, 1e-3f));
}

TEST(SceneSerialization, RejectsBadVersion) {
    const std::string json = R"({
      "format": "nova.scene",
      "version": 999,
      "entities": []
    })";

    Nova::Scene scene;
    const Nova::SceneIOResult io = Nova::DeserializeSceneFromString(json, scene);
    EXPECT_FALSE(io.Ok);
    EXPECT_EQ(scene.EntityCount(), 0u);
}

TEST(SceneSerialization, RejectsUnknownPrimitive) {
    const std::string json = R"({
      "format": "nova.scene",
      "version": 1,
      "entities": [{
        "name": "BadMesh",
        "meshRenderer": { "primitive": "NotARealMesh" }
      }]
    })";

    Nova::Scene scene;
    const Nova::SceneIOResult io = Nova::DeserializeSceneFromString(json, scene);
    EXPECT_FALSE(io.Ok);
    EXPECT_EQ(scene.EntityCount(), 0u);
}
