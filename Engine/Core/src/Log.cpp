#include <Nova/Core/Log.h>
#include <cstdarg>

namespace Nova {

LogLevel Log::s_Level = LogLevel::Trace;

void Log::Init() {
    // Stub — spdlog integration in commit 2
}

void Log::Shutdown() {
    // Stub
}

void Log::Write(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < s_Level) return;

    static const char* levelStr[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };

    std::fprintf(stderr, "[%s] ", levelStr[static_cast<int>(level)]);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);

    std::fprintf(stderr, " (%s:%d)\n", file, line);
}

} // namespace Nova
