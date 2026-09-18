#include <gtest/gtest.h>

#include <Nova/Audio/AudioEngine.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Script.h>

TEST(Script, CompilesPlayAndIfPlay) {
    Nova::CompiledScript compiled;
    std::string error;
    ASSERT_TRUE(Nova::CompileNovaScript(
        "on update\nif jump play jump\nif attack play sword\nend\n", compiled, error))
        << error;
    EXPECT_EQ(compiled.OnUpdate.size(), 2u);
    EXPECT_EQ(compiled.OnUpdate[0].Action, "jump");
    EXPECT_EQ(compiled.OnUpdate[1].SoundId, "sword");
}

TEST(Script, DefaultPlayerScriptCompiles) {
    Nova::CompiledScript compiled;
    std::string error;
    ASSERT_TRUE(Nova::CompileNovaScript(Nova::DefaultPlayerScript(), compiled, error)) << error;
    EXPECT_FALSE(compiled.OnStart.empty());
}

TEST(Script, RejectsUnknownCommand) {
    Nova::CompiledScript compiled;
    std::string error;
    EXPECT_FALSE(Nova::CompileNovaScript("on start\nexplode\nend\n", compiled, error));
    EXPECT_FALSE(error.empty());
}

TEST(Script, PlayableLevelHasScript) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    ASSERT_TRUE(player.IsValid());
    EXPECT_TRUE(scene.HasScript(player));
}

TEST(Script, CompilesGameConfig) {
    Nova::CompiledScript compiled;
    std::string error;
    ASSERT_TRUE(Nova::CompileNovaScript(Nova::DefaultGameScript(), compiled, error)) << error;
    EXPECT_FALSE(compiled.OnStart.empty());
}

TEST(Script, ContentScriptSpawnsGroundAndPlayer) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::Entity last{};
    std::string error;
    ASSERT_TRUE(Nova::RunContentScript(scene, "character\nground\n", error, last)) << error;
    EXPECT_TRUE(scene.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("Ground").IsValid());
}

TEST(Audio, ListsBuiltinSounds) {
    const auto sounds = Nova::AudioEngine::Get().ListSounds();
    ASSERT_GE(sounds.size(), 4u);
    bool hasJump = false;
    for (const Nova::SoundInfo& s : sounds) {
        if (s.Id == "jump") {
            hasJump = true;
        }
    }
    EXPECT_TRUE(hasJump);
}
