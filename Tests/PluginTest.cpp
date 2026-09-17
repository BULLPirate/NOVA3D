#include <gtest/gtest.h>

#include <Nova/Plugins/BuiltinPlugins.h>
#include <Nova/Plugins/PluginRegistry.h>
#include <Nova/Plugins/ServiceHub.h>

#include <filesystem>

TEST(Plugins, BuiltinAiAndStorageRegister) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nova_plugin_storage";
    Nova::PluginRegistry registry;
    Nova::ServiceHub services;
    Nova::RegisterBuiltinPlugins(registry, services, root);

    ASSERT_EQ(registry.Count(), 2u);
    ASSERT_NE(registry.Find("nova.ai.null"), nullptr);
    ASSERT_NE(registry.Find("nova.storage.file"), nullptr);
    ASSERT_NE(services.AI, nullptr);
    ASSERT_NE(services.Storage, nullptr);

    EXPECT_FALSE(services.AI->IsAvailable());
    const Nova::ProviderTextResult ai = services.AI->GenerateText("hi");
    EXPECT_FALSE(ai.Ok);

    const Nova::StorageResult save = services.Storage->Save("save/slot1.json", "{\"hp\":10}");
    ASSERT_TRUE(save.Ok) << save.Error;
    EXPECT_TRUE(services.Storage->Exists("save/slot1.json"));
    const Nova::StorageResult load = services.Storage->Load("save/slot1.json");
    ASSERT_TRUE(load.Ok) << load.Error;
    EXPECT_EQ(load.Data, "{\"hp\":10}");

    EXPECT_FALSE(services.Storage->Save("../escape", "no").Ok);
    EXPECT_TRUE(registry.SetEnabled("nova.ai.null", false));
    EXPECT_FALSE(registry.Find("nova.ai.null")->IsEnabled());
    EXPECT_TRUE(registry.Unload("nova.ai.null"));
    EXPECT_EQ(registry.Find("nova.ai.null"), nullptr);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
