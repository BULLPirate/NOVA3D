#pragma once

#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/Input.h>
#include <Nova/Math/Vec.h>
#include <Nova/Scene/Scene.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Nova {

enum class ScriptOp : uint8_t {
    Play,
    IfPlay,
    Gravity,
    Speed,
    Jump,
    Clear,
    Volume,
    Bind,
};

struct ScriptInstr {
    ScriptOp Op = ScriptOp::Play;
    std::string Action;
    std::string SoundId;
    Vec3 Color{0.0f, 0.0f, 0.0f};
    float Value = 0.0f;
};

struct CompiledScript {
    std::vector<ScriptInstr> OnStart;
    std::vector<ScriptInstr> OnUpdate;
};

bool CompileNovaScript(std::string_view source, CompiledScript& out, std::string& error);

void TickScripts(Scene& scene, const Input* input, EngineSettings& settings,
                 const std::filesystem::path& projectRoot, bool playJustStarted);

std::string DefaultPlayerScript();
std::string DefaultContentScript();
std::string DefaultGameScript();

bool RunContentScript(Scene& scene, std::string_view source, std::string& error, Entity& lastSpawned);

} // namespace Nova
