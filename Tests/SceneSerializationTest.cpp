#include <gtest/gtest.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Core/Guid.h>

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
    EXPECT_TRUE(fromFile.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(fromFile.FindEntityByName("Ground").IsValid());
    EXPECT_TRUE(fromFile.FindPrimaryCamera().IsValid());
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

TEST(SceneSerialization, ParentLinkRoundTrip) {
    Nova::Scene scene;
    Nova::Entity root = scene.CreateEntity("Root");
    Nova::Entity child = scene.CreateEntity("Child");
    scene.GetTransform(child).Position = {0.0f, 1.0f, 0.0f};
    scene.SetParent(child, root);

    const std::string json = Nova::SerializeSceneToString(scene);
    Nova::Scene loaded;
    const Nova::SceneIOResult io = Nova::DeserializeSceneFromString(json, loaded);
    ASSERT_TRUE(io.Ok) << io.Error;

    const Nova::Entity loadedChild = loaded.FindEntityByName("Child");
    const Nova::Entity loadedRoot = loaded.FindEntityByName("Root");
    EXPECT_TRUE(loadedChild.IsValid());
    EXPECT_TRUE(loadedRoot.IsValid());
    EXPECT_EQ(loaded.GetParent(loadedChild).Id, loadedRoot.Id);
}

TEST(SceneSerialization, EnvironmentAndPointLightRoundTrip) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    scene.Settings().ClearColor = {0.2f, 0.3f, 0.4f};
    Nova::Entity lamp = scene.CreateEntity("Lamp");
    Nova::PointLightComponent light;
    light.Color = {0.1f, 0.2f, 0.3f};
    light.Intensity = 3.5f;
    light.Range = 8.0f;
    scene.AddPointLight(lamp, light);

    Nova::Scene loaded;
    ASSERT_TRUE(Nova::DeserializeSceneFromString(Nova::SerializeSceneToString(scene), loaded).Ok);
    EXPECT_TRUE(Nova::ScenesEquivalent(scene, loaded));
    EXPECT_NEAR(loaded.Settings().ClearColor.z, 0.4f, 1e-4f);
}

TEST(SceneSerialization, CloneSceneMatchesSource) {
    const Nova::Scene original = Nova::Scene::CreateDemoLevel();
    const Nova::Scene copy = Nova::CloneScene(original);
    EXPECT_TRUE(Nova::ScenesEquivalent(original, copy));
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

TEST(SceneSerialization, MeshAssetIdRoundTrip) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::Entity cube = scene.CreateEntity("GuidedCube");
    Nova::MeshRendererComponent mesh;
    mesh.AssetPath = "Assets/Models/hero.gltf";
    mesh.MeshAssetId = Nova::Guid::Generate();
    mesh.AlbedoTexturePath = "Assets/Textures/brick.png";
    mesh.AlbedoTextureId = Nova::Guid::Generate();
    scene.AddMeshRenderer(cube, mesh);

    Nova::Scene loaded;
    ASSERT_TRUE(Nova::DeserializeSceneFromString(Nova::SerializeSceneToString(scene), loaded).Ok);
    ASSERT_TRUE(loaded.HasMeshRenderer(loaded.FindEntityByName("GuidedCube")));
    const Nova::MeshRendererComponent& loadedMesh =
        loaded.GetMeshRenderer(loaded.FindEntityByName("GuidedCube"));
    EXPECT_EQ(loadedMesh.MeshAssetId, mesh.MeshAssetId);
    EXPECT_EQ(loadedMesh.AlbedoTextureId, mesh.AlbedoTextureId);
    EXPECT_TRUE(Nova::ScenesEquivalent(scene, loaded));
}
