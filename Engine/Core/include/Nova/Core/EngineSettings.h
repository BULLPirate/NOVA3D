#pragma once

#include <Nova/Core/Input.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nova {

enum class UiLanguage { English, Russian };

struct EngineSettings {
    UiLanguage Language = UiLanguage::Russian;
    float MasterVolume = 0.0f;
    float MouseSensitivity = 1.0f;
    bool InvertY = false;
    bool EnableAudio = false;
    std::unordered_map<std::string, KeyCode> Bindings;

    static EngineSettings Defaults();
    KeyCode Binding(const std::string& action) const;
    bool IsActionDown(const Input& input, const std::string& action) const;
    bool IsActionPressed(const Input& input, const std::string& action) const;
};

std::filesystem::path EngineSettingsPath(const std::filesystem::path& projectRoot);
bool LoadEngineSettings(const std::filesystem::path& projectRoot, EngineSettings& out);
bool SaveEngineSettings(const std::filesystem::path& projectRoot, const EngineSettings& settings);

const char* KeyCodeName(KeyCode key);
bool KeyCodeFromName(const std::string& name, KeyCode& out);
std::vector<std::string> AllBindableKeyNames();

} // namespace Nova
