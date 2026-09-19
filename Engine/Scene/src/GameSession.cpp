#include <Nova/Scene/GameSession.h>

namespace Nova {

void GameSession::Tick(float deltaSeconds) {
    if (deltaSeconds < 0.0f) {
        deltaSeconds = 0.0f;
    }
    PhaseTime += deltaSeconds;
    if (Current == Phase::Splash && PhaseTime >= 1.4f) {
        Current = Phase::Title;
        PhaseTime = 0.0f;
    } else if (Current == Phase::Loading && PhaseTime >= 0.4f) {
        Current = Phase::Playing;
        PhaseTime = 0.0f;
    }
}

void GameSession::RequestPlay() {
    Current = Phase::Loading;
    PhaseTime = 0.0f;
}

void GameSession::OpenSettings() {
    AfterSettings = Current == Phase::Paused ? Phase::Paused : Phase::Title;
    Current = Phase::Settings;
}

void GameSession::CloseSettings() {
    Current = AfterSettings;
    PhaseTime = 0.0f;
}

void GameSession::TogglePause() {
    if (Current == Phase::Playing) {
        Current = Phase::Paused;
        PhaseTime = 0.0f;
    } else if (Current == Phase::Paused) {
        Current = Phase::Playing;
        PhaseTime = 0.0f;
    }
}

void GameSession::QuitToTitle() {
    Current = Phase::Title;
    PhaseTime = 0.0f;
}

} // namespace Nova
