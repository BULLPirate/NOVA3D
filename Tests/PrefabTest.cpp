#include <gtest/gtest.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <filesystem>

TEST(Prefab, RoundTripSubtreeKeepsChildrenAndDropsOutsideParent) {
    Nova::Scene scene;
    Nova::Entity world = scene.CreateEntity("World");
    Nova::Entity root = scene.CreateEntity("Prop");
    Nova::Entity child = scene.CreateEntity("Child");
    scene.SetParent(root, world);
    scene.SetParent(child, root);
    scene.AddMeshRenderer(root);
    scene.GetTransform(child).Position = {0.0f, 1.0f, 0.0f};

    const std::string json = Nova::SerializePrefabToString(scene, root);
    EXPECT_EQ(json.find("World"), std::string::npos);

    Nova::Scene dest = Nova::Scene::CreateEmptyLevel();
    Nova::Entity spawned{};
    const Nova::SceneIOResult io = Nova::InstantiatePrefabFromString(json, dest, spawned);
    ASSERT_TRUE(io.Ok) << io.Error;
    ASSERT_TRUE(spawned.IsValid());
    EXPECT_EQ(dest.GetName(spawned), "Prop");
    EXPECT_FALSE(dest.GetParent(spawned).IsValid());
    EXPECT_TRUE(dest.HasMeshRenderer(spawned));

    Nova::Entity spawnedChild = dest.FindEntityByName("Child");
    ASSERT_TRUE(spawnedChild.IsValid());
    EXPECT_EQ(dest.GetParent(spawnedChild).Id, spawned.Id);
    EXPECT_FLOAT_EQ(dest.GetTransform(spawnedChild).Position.y, 1.0f);
}

TEST(Prefab, InstantiateRenamesOnCollision) {
    Nova::Scene scene;
    Nova::Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube);

    const std::string json = Nova::SerializePrefabToString(scene, cube);

    Nova::Scene dest;
    dest.CreateEntity("Cube");
    Nova::Entity spawned{};
    ASSERT_TRUE(Nova::InstantiatePrefabFromString(json, dest, spawned).Ok);
    EXPECT_NE(dest.GetName(spawned), "Cube");
    EXPECT_TRUE(dest.HasMeshRenderer(spawned));
}

TEST(Prefab, FileRoundTripPointLight) {
    Nova::Scene scene;
    Nova::Entity lamp = scene.CreateEntity("Lamp");
    Nova::PointLightComponent light;
    light.Range = 4.5f;
    light.Intensity = 2.0f;
    scene.AddPointLight(lamp, light);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "nova_prefab_point_test.prefab.json";
    ASSERT_TRUE(Nova::SavePrefabToFile(scene, lamp, path).Ok);

    Nova::Scene dest;
    Nova::Entity spawned{};
    ASSERT_TRUE(Nova::InstantiatePrefabFromFile(path, dest, spawned).Ok);
    ASSERT_TRUE(dest.HasPointLight(spawned));
    EXPECT_FLOAT_EQ(dest.GetPointLight(spawned).Range, 4.5f);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
