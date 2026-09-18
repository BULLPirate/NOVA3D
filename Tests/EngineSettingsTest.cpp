#include <gtest/gtest.h>

#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/FileSystem.h>

#include <filesystem>

TEST(EngineSettings, DefaultsAndRoundTrip) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nova_engine_settings_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / ".nova", ec);

    Nova::EngineSettings settings = Nova::EngineSettings::Defaults();
    settings.Language = Nova::UiLanguage::English;
    settings.MasterVolume = 0.4f;
    settings.Bindings["jump"] = Nova::KeyCode::F;
    ASSERT_TRUE(Nova::SaveEngineSettings(root, settings));

    Nova::EngineSettings loaded;
    ASSERT_TRUE(Nova::LoadEngineSettings(root, loaded));
    EXPECT_EQ(loaded.Language, Nova::UiLanguage::English);
    EXPECT_NEAR(loaded.MasterVolume, 0.4f, 1e-3f);
    EXPECT_EQ(loaded.Binding("jump"), Nova::KeyCode::F);
    EXPECT_EQ(loaded.Binding("forward"), Nova::KeyCode::W);

    std::filesystem::remove_all(root, ec);
}

TEST(EngineSettings, ActionUsesBinding) {
    Nova::EngineSettings settings = Nova::EngineSettings::Defaults();
    settings.Bindings["forward"] = Nova::KeyCode::I;
    Nova::Input input;
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::I);
    EXPECT_TRUE(settings.IsActionDown(input, "forward"));
    EXPECT_FALSE(settings.IsActionDown(input, "jump"));
}
