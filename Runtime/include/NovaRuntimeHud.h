#pragma once

#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/GameSession.h>
#include <Nova/Core/EngineSettings.h>

struct SDL_Window;

namespace Nova::RuntimeHud {

void Init(SDL_Window* window, void* metalDevice);
void Shutdown();
void PassReady(void* renderPassDescriptor, void* userData);
void Overlay(void* commandBuffer, void* renderEncoder, void* userData);
void ProcessEvent(const void* sdlEvent);
void NewFrame();
void Draw(const GameplayHudSnapshot& hud, const char* title, GameSession& session,
          EngineSettings& settings);
void EndFrameWidgets();

} // namespace Nova::RuntimeHud
