#include <Nova/Audio/AudioEngine.h>
#include <Nova/Core/FileSystem.h>
#include <Nova/Core/Log.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace Nova {

namespace {

constexpr int kSampleRate = 22050;

struct Clip {
    std::string DisplayName;
    bool Builtin = true;
    std::vector<float> Samples;
};

std::vector<float> Impact(float seconds, float pitch, float volume) {
    const int n = std::max(1, static_cast<int>(seconds * static_cast<float>(kSampleRate)));
    std::vector<float> out(static_cast<std::size_t>(n));
    uint32_t rng = 0x51AA5EED;
    float lp = 0.0f;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const float nse = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
        lp = lp * 0.88f + nse * 0.12f;
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float env = std::min(1.0f, static_cast<float>(i) / 40.0f) *
                          std::min(1.0f, static_cast<float>(n - i) / 500.0f);
        const float body = std::sin(6.28318530718f * pitch * t) *
                           std::exp(-t * 8.0f);
        out[static_cast<std::size_t>(i)] = (body * 0.55f + lp * 0.45f) * volume * env;
    }
    return out;
}

std::vector<float> Blade(float seconds, float volume) {
    const int n = std::max(1, static_cast<int>(seconds * static_cast<float>(kSampleRate)));
    std::vector<float> out(static_cast<std::size_t>(n));
    uint32_t rng = 0xC0FFEE11;
    float lp = 0.0f;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const float nse = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
        lp = lp * 0.82f + nse * 0.18f;
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float env = std::min(1.0f, static_cast<float>(i) / 30.0f) *
                          std::min(1.0f, static_cast<float>(n - i) / 700.0f);
        const float ring = std::sin(6.28318530718f * (520.0f + 90.0f * std::sin(30.0f * t)) * t);
        out[static_cast<std::size_t>(i)] = (ring * 0.22f + lp * 0.4f) * volume * env;
    }
    return out;
}

std::vector<float> NoiseBurst(float seconds, float volume) {
    const int n = std::max(1, static_cast<int>(seconds * static_cast<float>(kSampleRate)));
    std::vector<float> out(static_cast<std::size_t>(n));
    uint32_t rng = 0xA341316Cu;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const float nse = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
        const float env = 1.0f - static_cast<float>(i) / static_cast<float>(n);
        out[static_cast<std::size_t>(i)] = nse * volume * env;
    }
    return out;
}

bool LoadWavPcm(const std::filesystem::path& path, std::vector<float>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 44) {
        return false;
    }
    auto u16 = [&](std::size_t o) -> uint16_t {
        return static_cast<uint16_t>(static_cast<unsigned char>(bytes[o]) |
                                     (static_cast<unsigned char>(bytes[o + 1]) << 8));
    };
    auto u32 = [&](std::size_t o) -> uint32_t {
        return static_cast<uint32_t>(static_cast<unsigned char>(bytes[o]) |
                                     (static_cast<unsigned char>(bytes[o + 1]) << 8) |
                                     (static_cast<unsigned char>(bytes[o + 2]) << 16) |
                                     (static_cast<unsigned char>(bytes[o + 3]) << 24));
    };
    if (std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return false;
    }
    std::size_t pos = 12;
    uint16_t audioFormat = 1;
    uint16_t channels = 1;
    uint32_t sampleRate = kSampleRate;
    uint16_t bits = 16;
    std::size_t dataOff = 0;
    uint32_t dataSize = 0;
    while (pos + 8 <= bytes.size()) {
        char id[5] = {};
        std::memcpy(id, bytes.data() + pos, 4);
        const uint32_t sz = u32(pos + 4);
        const std::size_t payload = pos + 8;
        if (std::strcmp(id, "fmt ") == 0 && payload + 16 <= bytes.size()) {
            audioFormat = u16(payload);
            channels = u16(payload + 2);
            sampleRate = u32(payload + 4);
            bits = u16(payload + 14);
        } else if (std::strcmp(id, "data") == 0) {
            dataOff = payload;
            dataSize = sz;
            break;
        }
        pos = payload + sz + (sz & 1u);
    }
    if (dataOff == 0 || dataOff + dataSize > bytes.size()) {
        return false;
    }
    if (audioFormat != 1 || (bits != 16 && bits != 8) || channels == 0) {
        return false;
    }
    const int srcRate = sampleRate > 0 ? static_cast<int>(sampleRate) : kSampleRate;
    const int frameBytes = (bits / 8) * channels;
    if (frameBytes <= 0) {
        return false;
    }
    const int frames = static_cast<int>(dataSize / static_cast<uint32_t>(frameBytes));
    std::vector<float> mono(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        float acc = 0.0f;
        for (int c = 0; c < channels; ++c) {
            const std::size_t o =
                dataOff + static_cast<std::size_t>(i * frameBytes + c * (bits / 8));
            if (bits == 16) {
                const int16_t s = static_cast<int16_t>(
                    static_cast<unsigned char>(bytes[o]) |
                    (static_cast<unsigned char>(bytes[o + 1]) << 8));
                acc += static_cast<float>(s) / 32768.0f;
            } else {
                acc += (static_cast<float>(static_cast<unsigned char>(bytes[o])) / 128.0f) - 1.0f;
            }
        }
        mono[static_cast<std::size_t>(i)] = acc / static_cast<float>(channels);
    }
    if (srcRate == kSampleRate) {
        out = std::move(mono);
        return true;
    }
    const double ratio = static_cast<double>(srcRate) / static_cast<double>(kSampleRate);
    const int dstN = std::max(1, static_cast<int>(static_cast<double>(frames) / ratio));
    out.resize(static_cast<std::size_t>(dstN));
    for (int i = 0; i < dstN; ++i) {
        const double src = static_cast<double>(i) * ratio;
        const int a = std::min(frames - 1, static_cast<int>(src));
        const int b = std::min(frames - 1, a + 1);
        const float t = static_cast<float>(src - static_cast<double>(a));
        out[static_cast<std::size_t>(i)] =
            mono[static_cast<std::size_t>(a)] * (1.0f - t) + mono[static_cast<std::size_t>(b)] * t;
    }
    return true;
}

} // namespace

struct AudioEngine::Impl {
    bool Ready = false;
    bool Enabled = false;
    float MasterVolume = 0.0f;
    SDL_AudioStream* Stream = nullptr;
    std::unordered_map<std::string, Clip> Clips;
    std::filesystem::path UserAudioDir;
};

AudioEngine& AudioEngine::Get() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::~AudioEngine() {
    Shutdown();
}

void AudioEngine::EnsureBuiltins() {
    if (!m_Impl) {
        m_Impl = std::make_unique<Impl>();
    }
    auto& clips = m_Impl->Clips;
    if (clips.count("confirm")) {
        return;
    }
    clips["confirm"] = {"Confirm", true, Impact(0.16f, 180.0f, 0.28f)};
    clips["jump"] = {"Jump", true, NoiseBurst(0.14f, 0.22f)};
    clips["click"] = {"Click", true, Impact(0.06f, 140.0f, 0.22f)};
    clips["error"] = {"Error", true, Impact(0.2f, 70.0f, 0.35f)};
    clips["footstep"] = {"Footstep", true, NoiseBurst(0.08f, 0.2f)};
    clips["pickup"] = {"Pickup", true, Impact(0.12f, 240.0f, 0.25f)};
    clips["attack"] = {"Attack", true, Blade(0.22f, 0.45f)};
    clips["sword"] = {"Sword", true, Blade(0.26f, 0.5f)};
    clips["hit"] = {"Hit", true, Impact(0.18f, 90.0f, 0.4f)};
    clips["death"] = {"Death", true, Impact(0.32f, 55.0f, 0.38f)};
    clips["whoosh"] = {"Dodge", true, NoiseBurst(0.14f, 0.24f)};
    clips["land"] = {"Land", true, Impact(0.12f, 80.0f, 0.32f)};
    clips["interact"] = {"Interact", true, Impact(0.1f, 160.0f, 0.24f)};
}

bool AudioEngine::Init() {
    EnsureBuiltins();
    if (m_Impl->Ready) {
        return true;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        NOVA_LOG_WARN("Audio init failed: {}", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec{};
    spec.freq = kSampleRate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    m_Impl->Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr,
                                               nullptr);
    if (!m_Impl->Stream) {
        NOVA_LOG_WARN("Audio device failed: {}", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(m_Impl->Stream);
    m_Impl->Ready = true;
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_Impl) {
        return;
    }
    if (m_Impl->Stream) {
        SDL_DestroyAudioStream(m_Impl->Stream);
        m_Impl->Stream = nullptr;
    }
    m_Impl->Ready = false;
}

void AudioEngine::SetMasterVolume(float volume) {
    EnsureBuiltins();
    m_Impl->MasterVolume = std::clamp(volume, 0.0f, 1.0f);
}

float AudioEngine::MasterVolume() const {
    return m_Impl ? m_Impl->MasterVolume : 0.0f;
}

void AudioEngine::SetEnabled(bool enabled) {
    EnsureBuiltins();
    m_Impl->Enabled = enabled;
}

bool AudioEngine::IsEnabled() const {
    return m_Impl && m_Impl->Enabled;
}

void AudioEngine::Play(const std::string& soundId) {
    EnsureBuiltins();
    if (!m_Impl->Enabled || !m_Impl->Ready || !m_Impl->Stream) {
        return;
    }
    const auto it = m_Impl->Clips.find(soundId);
    if (it == m_Impl->Clips.end() || it->second.Samples.empty()) {
        return;
    }
    std::vector<float> scaled = it->second.Samples;
    const float vol = m_Impl->MasterVolume;
    for (float& s : scaled) {
        s *= vol;
    }
    if (vol <= 0.001f) {
        return;
    }
    SDL_PutAudioStreamData(m_Impl->Stream, scaled.data(),
                           static_cast<int>(scaled.size() * sizeof(float)));
}

std::vector<SoundInfo> AudioEngine::ListSounds() const {
    const_cast<AudioEngine*>(this)->EnsureBuiltins();
    std::vector<SoundInfo> list;
    list.reserve(m_Impl->Clips.size());
    for (const auto& [id, clip] : m_Impl->Clips) {
        list.push_back({id, clip.DisplayName, clip.Builtin});
    }
    std::sort(list.begin(), list.end(), [](const SoundInfo& a, const SoundInfo& b) {
        return a.Id < b.Id;
    });
    return list;
}

void AudioEngine::RefreshUserSounds(const std::filesystem::path& projectRoot) {
    EnsureBuiltins();
    for (auto it = m_Impl->Clips.begin(); it != m_Impl->Clips.end();) {
        if (!it->second.Builtin) {
            it = m_Impl->Clips.erase(it);
        } else {
            ++it;
        }
    }
    const std::filesystem::path dir = projectRoot / "Assets" / "Audio";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".wav") {
            continue;
        }
        std::vector<float> samples;
        if (!LoadWavPcm(entry.path(), samples)) {
            NOVA_LOG_WARN("Could not load wav {}", entry.path().string());
            continue;
        }
        const std::string id = entry.path().stem().string();
        Clip clip;
        clip.DisplayName = id;
        clip.Builtin = false;
        clip.Samples = std::move(samples);
        m_Impl->Clips[id] = std::move(clip);
    }
}

} // namespace Nova
