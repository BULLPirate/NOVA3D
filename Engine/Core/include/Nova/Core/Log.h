#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>

namespace Nova {

class Log {
public:
    static void Init();
    static void Shutdown();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
};

// ── Core engine macros ─────────────────────────────────────────────────────────
#define NOVA_LOG_TRACE(...)    ::Nova::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define NOVA_LOG_DEBUG(...)    ::Nova::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define NOVA_LOG_INFO(...)     ::Nova::Log::GetCoreLogger()->info(__VA_ARGS__)
#define NOVA_LOG_WARN(...)     ::Nova::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define NOVA_LOG_ERROR(...)    ::Nova::Log::GetCoreLogger()->error(__VA_ARGS__)
#define NOVA_LOG_FATAL(...)    ::Nova::Log::GetCoreLogger()->critical(__VA_ARGS__)

// ── Client/game macros ─────────────────────────────────────────────────────────
#define NOVA_LOG_GAME_TRACE(...) ::Nova::Log::GetClientLogger()->trace(__VA_ARGS__)
#define NOVA_LOG_GAME_DEBUG(...) ::Nova::Log::GetClientLogger()->debug(__VA_ARGS__)
#define NOVA_LOG_GAME_INFO(...)  ::Nova::Log::GetClientLogger()->info(__VA_ARGS__)
#define NOVA_LOG_GAME_WARN(...)  ::Nova::Log::GetClientLogger()->warn(__VA_ARGS__)
#define NOVA_LOG_GAME_ERROR(...) ::Nova::Log::GetClientLogger()->error(__VA_ARGS__)
#define NOVA_LOG_GAME_FATAL(...) ::Nova::Log::GetClientLogger()->critical(__VA_ARGS__)

} // namespace Nova
