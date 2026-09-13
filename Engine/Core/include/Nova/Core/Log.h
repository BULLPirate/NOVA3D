#pragma once

#include <string_view>
#include <cstdio>

namespace Nova {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

/// Logging subsystem. Lightweight wrapper — full implementation in commit 2.
class Log {
public:
    static void Init();
    static void Shutdown();

    static void Write(LogLevel level, const char* file, int line, const char* fmt, ...);

    static LogLevel GetLevel() { return s_Level; }
    static void SetLevel(LogLevel level) { s_Level = level; }

private:
    static LogLevel s_Level;
};

// Convenience macros — will become spdlog-backed in commit 2
#define NOVA_LOG_TRACE(...) ::Nova::Log::Write(::Nova::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define NOVA_LOG_DEBUG(...) ::Nova::Log::Write(::Nova::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define NOVA_LOG_INFO(...)  ::Nova::Log::Write(::Nova::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define NOVA_LOG_WARN(...)  ::Nova::Log::Write(::Nova::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define NOVA_LOG_ERROR(...) ::Nova::Log::Write(::Nova::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define NOVA_LOG_FATAL(...) ::Nova::Log::Write(::Nova::LogLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__)

} // namespace Nova
