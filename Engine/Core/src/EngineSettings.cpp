#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/FileSystem.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace Nova {

namespace {

struct KeyEntry {
    const char* Name;
    KeyCode Code;
};

const KeyEntry kKeys[] = {
    {"A", KeyCode::A}, {"B", KeyCode::B}, {"C", KeyCode::C}, {"D", KeyCode::D},
    {"E", KeyCode::E}, {"F", KeyCode::F}, {"G", KeyCode::G}, {"H", KeyCode::H},
    {"I", KeyCode::I}, {"J", KeyCode::J}, {"K", KeyCode::K}, {"L", KeyCode::L},
    {"M", KeyCode::M}, {"N", KeyCode::N}, {"O", KeyCode::O}, {"P", KeyCode::P},
    {"Q", KeyCode::Q}, {"R", KeyCode::R}, {"S", KeyCode::S}, {"T", KeyCode::T},
    {"U", KeyCode::U}, {"V", KeyCode::V}, {"W", KeyCode::W}, {"X", KeyCode::X},
    {"Y", KeyCode::Y}, {"Z", KeyCode::Z},
    {"Space", KeyCode::Space}, {"Up", KeyCode::Up}, {"Down", KeyCode::Down},
    {"Left", KeyCode::Left}, {"Right", KeyCode::Right},
    {"LShift", KeyCode::LShift}, {"LCtrl", KeyCode::LCtrl},
};

} // namespace

EngineSettings EngineSettings::Defaults() {
    EngineSettings s;
    s.Language = UiLanguage::Russian;
    s.MasterVolume = 0.0f;
    s.EnableAudio = false;
    s.Bindings["forward"] = KeyCode::W;
    s.Bindings["back"] = KeyCode::S;
    s.Bindings["left"] = KeyCode::A;
    s.Bindings["right"] = KeyCode::D;
    s.Bindings["jump"] = KeyCode::Space;
    s.Bindings["sprint"] = KeyCode::LShift;
    s.Bindings["crouch"] = KeyCode::LCtrl;
    s.Bindings["attack"] = KeyCode::F;
    s.Bindings["dodge"] = KeyCode::Q;
    s.Bindings["interact"] = KeyCode::E;
    s.MouseSensitivity = 1.0f;
    s.InvertY = false;
    return s;
}

KeyCode EngineSettings::Binding(const std::string& action) const {
    const auto it = Bindings.find(action);
    if (it == Bindings.end()) {
        return KeyCode::Unknown;
    }
    return it->second;
}

bool EngineSettings::IsActionDown(const Input& input, const std::string& action) const {
    return input.IsKeyDown(Binding(action));
}

bool EngineSettings::IsActionPressed(const Input& input, const std::string& action) const {
    return input.IsKeyPressed(Binding(action));
}

const char* KeyCodeName(KeyCode key) {
    for (const KeyEntry& e : kKeys) {
        if (e.Code == key) {
            return e.Name;
        }
    }
    return "Unknown";
}

bool KeyCodeFromName(const std::string& name, KeyCode& out) {
    for (const KeyEntry& e : kKeys) {
        if (name == e.Name) {
            out = e.Code;
            return true;
        }
    }
    return false;
}

std::vector<std::string> AllBindableKeyNames() {
    std::vector<std::string> names;
    names.reserve(sizeof(kKeys) / sizeof(kKeys[0]));
    for (const KeyEntry& e : kKeys) {
        names.emplace_back(e.Name);
    }
    return names;
}

std::filesystem::path EngineSettingsPath(const std::filesystem::path& projectRoot) {
    return projectRoot / ".nova" / "engine.json";
}

bool LoadEngineSettings(const std::filesystem::path& projectRoot, EngineSettings& out) {
    out = EngineSettings::Defaults();
    const FileIOResult read = ReadTextFile(EngineSettingsPath(projectRoot));
    if (!read.Ok) {
        return false;
    }
    auto findValue = [&](const std::string& key) -> std::string {
        const std::string token = "\"" + key + "\"";
        const auto pos = read.Text.find(token);
        if (pos == std::string::npos) {
            return {};
        }
        const auto colon = read.Text.find(':', pos);
        if (colon == std::string::npos) {
            return {};
        }
        std::size_t i = colon + 1;
        while (i < read.Text.size() && std::isspace(static_cast<unsigned char>(read.Text[i]))) {
            ++i;
        }
        if (i < read.Text.size() && read.Text[i] == '"') {
            const auto end = read.Text.find('"', i + 1);
            if (end == std::string::npos) {
                return {};
            }
            return read.Text.substr(i + 1, end - i - 1);
        }
        std::size_t j = i;
        while (j < read.Text.size() && (std::isdigit(static_cast<unsigned char>(read.Text[j])) ||
                                        read.Text[j] == '.')) {
            ++j;
        }
        return read.Text.substr(i, j - i);
    };

    const std::string lang = findValue("language");
    if (lang == "en") {
        out.Language = UiLanguage::English;
    } else if (lang == "ru") {
        out.Language = UiLanguage::Russian;
    }
    const std::string vol = findValue("masterVolume");
    if (!vol.empty()) {
        out.MasterVolume = std::clamp(std::strtof(vol.c_str(), nullptr), 0.0f, 1.0f);
    }
    const std::string sens = findValue("mouseSensitivity");
    if (!sens.empty()) {
        out.MouseSensitivity = std::clamp(std::strtof(sens.c_str(), nullptr), 0.2f, 3.0f);
    }
    const std::string invert = findValue("invertY");
    if (invert == "true") {
        out.InvertY = true;
    } else if (invert == "false") {
        out.InvertY = false;
    }
    for (const char* action : {"forward", "back", "left", "right", "jump", "sprint", "crouch",
                               "attack", "dodge", "interact"}) {
        const std::string keyName = findValue(action);
        KeyCode code;
        if (!keyName.empty() && KeyCodeFromName(keyName, code)) {
            out.Bindings[action] = code;
        }
    }
    return true;
}

bool SaveEngineSettings(const std::filesystem::path& projectRoot, const EngineSettings& settings) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"language\": \""
         << (settings.Language == UiLanguage::English ? "en" : "ru") << "\",\n";
    json << "  \"masterVolume\": " << settings.MasterVolume << ",\n";
    json << "  \"mouseSensitivity\": " << settings.MouseSensitivity << ",\n";
    json << "  \"invertY\": \"" << (settings.InvertY ? "true" : "false") << "\",\n";
    json << "  \"forward\": \"" << KeyCodeName(settings.Binding("forward")) << "\",\n";
    json << "  \"back\": \"" << KeyCodeName(settings.Binding("back")) << "\",\n";
    json << "  \"left\": \"" << KeyCodeName(settings.Binding("left")) << "\",\n";
    json << "  \"right\": \"" << KeyCodeName(settings.Binding("right")) << "\",\n";
    json << "  \"jump\": \"" << KeyCodeName(settings.Binding("jump")) << "\",\n";
    json << "  \"sprint\": \"" << KeyCodeName(settings.Binding("sprint")) << "\",\n";
    json << "  \"crouch\": \"" << KeyCodeName(settings.Binding("crouch")) << "\",\n";
    json << "  \"attack\": \"" << KeyCodeName(settings.Binding("attack")) << "\",\n";
    json << "  \"dodge\": \"" << KeyCodeName(settings.Binding("dodge")) << "\",\n";
    json << "  \"interact\": \"" << KeyCodeName(settings.Binding("interact")) << "\"\n";
    json << "}\n";
    return WriteTextFile(EngineSettingsPath(projectRoot), json.str()).Ok;
}

} // namespace Nova
