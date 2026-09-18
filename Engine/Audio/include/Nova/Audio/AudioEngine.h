#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Nova {

struct SoundInfo {
    std::string Id;
    std::string DisplayName;
    bool Builtin = true;
};

class AudioEngine {
public:
    static AudioEngine& Get();

    bool Init();
    void Shutdown();
    void SetMasterVolume(float volume);
    float MasterVolume() const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void Play(const std::string& soundId);
    std::vector<SoundInfo> ListSounds() const;
    void RefreshUserSounds(const std::filesystem::path& projectRoot);

    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

private:
    AudioEngine() = default;
    void EnsureBuiltins();
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Nova
