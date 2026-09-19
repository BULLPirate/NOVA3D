#include <NovaRuntimeHud.h>

#include <imgui.h>
#include <imgui_impl_metal.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL_events.h>

#import <Metal/Metal.h>

#include <algorithm>
#include <filesystem>

namespace Nova::RuntimeHud {
namespace {

void LoadFont(ImGuiIO& io) {
    const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial Unicode.ttf",
    };
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.PixelSnapH = true;
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    for (const char* path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        if (io.Fonts->AddFontFromFileTTF(path, 18.0f, &cfg, ranges)) {
            io.FontDefault = io.Fonts->Fonts.back();
            return;
        }
    }
}

void CenteredWindow(const char* id, float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                                   vp->WorkPos.y + vp->WorkSize.y * 0.42f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize);
}

} // namespace

void Init(SDL_Window* window, void* metalDevice) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    LoadFont(io);
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForMetal(window);
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)metalDevice);
}

void Shutdown() {
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void PassReady(void* renderPassDescriptor, void*) {
    ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)renderPassDescriptor);
}

void Overlay(void* commandBuffer, void* renderEncoder, void*) {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData) {
        return;
    }
    ImGui_ImplMetal_RenderDrawData(drawData, (__bridge id<MTLCommandBuffer>)commandBuffer,
                                   (__bridge id<MTLRenderCommandEncoder>)renderEncoder);
}

void ProcessEvent(const void* sdlEvent) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdlEvent));
}

void NewFrame() {
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Draw(const GameplayHudSnapshot& hud, const char* title, GameSession& session,
          EngineSettings& settings) {
    const char* gameTitle = title && title[0] ? title : "NOVA3D";

    if (session.Current == GameSession::Phase::Splash) {
        CenteredWindow("##splash", 420.0f);
        ImGui::TextUnformatted("NOVA3D");
        ImGui::Separator();
        ImGui::TextUnformatted(gameTitle);
        ImGui::TextDisabled("загрузка движка…");
        ImGui::End();
        return;
    }

    if (session.Current == GameSession::Phase::Loading) {
        CenteredWindow("##loading", 420.0f);
        ImGui::TextUnformatted("Загрузка сцены…");
        ImGui::ProgressBar(std::min(1.0f, session.PhaseTime / 0.4f), ImVec2(-1.0f, 18.0f));
        ImGui::End();
        return;
    }

    if (session.Current == GameSession::Phase::Title) {
        CenteredWindow("##title", 360.0f);
        ImGui::TextUnformatted(gameTitle);
        ImGui::Separator();
        if (ImGui::Button("Играть", ImVec2(-1.0f, 36.0f))) {
            session.RequestPlay();
        }
        if (ImGui::Button("Настройки", ImVec2(-1.0f, 32.0f))) {
            session.OpenSettings();
        }
        ImGui::TextDisabled("Esc — выход в меню, F6 — пауза");
        ImGui::End();
        return;
    }

    if (session.Current == GameSession::Phase::Settings) {
        CenteredWindow("##settings", 400.0f);
        ImGui::TextUnformatted("Настройки");
        ImGui::SliderFloat("Чувствительность мыши", &settings.MouseSensitivity, 0.2f, 2.5f);
        ImGui::Checkbox("Инвертировать Y", &settings.InvertY);
        if (ImGui::Button("Назад", ImVec2(-1.0f, 32.0f))) {
            session.CloseSettings();
        }
        ImGui::End();
        return;
    }

    if (session.Current == GameSession::Phase::Paused) {
        CenteredWindow("##paused", 360.0f);
        ImGui::TextUnformatted("Пауза");
        if (ImGui::Button("Продолжить", ImVec2(-1.0f, 32.0f))) {
            session.TogglePause();
        }
        if (ImGui::Button("Настройки", ImVec2(-1.0f, 32.0f))) {
            session.OpenSettings();
        }
        if (ImGui::Button("В меню", ImVec2(-1.0f, 32.0f))) {
            session.QuitToTitle();
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.74f);
    ImGui::Begin("##game_hud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(gameTitle);
    const float pmax = hud.PlayerMaxHealth > 1.0f ? hud.PlayerMaxHealth : 1.0f;
    if (hud.PlayerMaxHealth > 0.0f) {
        ImGui::ProgressBar(hud.PlayerHealth / pmax, ImVec2(280.0f, 20.0f));
        ImGui::Text("HP  %.0f / %.0f", hud.PlayerHealth, hud.PlayerMaxHealth);
    }
    if (hud.ShowCombatHud) {
        ImGui::Text("Волна %d   Очки %d   Бандиты: %d", hud.Wave, hud.Score, hud.AliveEnemies);
        if (hud.TargetMaxHealth > 0.0f && hud.Phase == GameplayPhase::Combat) {
            ImGui::Text("Цель HP  %.0f / %.0f", hud.TargetHealth, hud.TargetMaxHealth);
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted(GameplayHudStatusLine(hud));
    ImGui::TextUnformatted("Esc — меню   F6 — пауза   F8/F9 — сейв");
    ImGui::End();
}

void EndFrameWidgets() {
    ImGui::Render();
}

} // namespace Nova::RuntimeHud
