#pragma once

#include <cstdint>

namespace Nova {

/// Runtime flow owned by the engine: splash → title → load → play / pause / settings.
struct GameSession {
    enum class Phase : uint8_t { Splash = 0, Title, Loading, Playing, Paused, Settings };

    Phase Current = Phase::Splash;
    Phase AfterSettings = Phase::Title;
    float PhaseTime = 0.0f;

    void Tick(float deltaSeconds);
    void RequestPlay();
    void OpenSettings();
    void CloseSettings();
    void TogglePause();
    void QuitToTitle();

    bool BlocksWorldTick() const {
        return Current != Phase::Playing;
    }
};

} // namespace Nova
