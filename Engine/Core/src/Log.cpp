#include <Nova/Core/Log.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace Nova {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

static std::string DefaultLogPath() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        std::filesystem::path dir = std::filesystem::path(home) / "Library" / "Logs";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "NOVA3D.log").string();
    }
    return "Nova3D.log";
}

void Log::Init() {
    spdlog::set_pattern("%^[%T] %n: %v%$");

    std::vector<spdlog::sink_ptr> coreSinks;
    coreSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    coreSinks[0]->set_level(spdlog::level::trace);

    const std::string logPath = DefaultLogPath();
    try {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);
        fileSink->set_level(spdlog::level::trace);
        coreSinks.emplace_back(std::move(fileSink));
    } catch (const spdlog::spdlog_ex&) {
        // Console-only if the log file cannot be created (e.g. cwd is /).
    }

    s_CoreLogger = std::make_shared<spdlog::logger>("NOVA", coreSinks.begin(), coreSinks.end());
    s_CoreLogger->set_level(spdlog::level::trace);
    s_CoreLogger->flush_on(spdlog::level::info);

    s_ClientLogger = std::make_shared<spdlog::logger>(
        "GAME", std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    s_ClientLogger->set_level(spdlog::level::trace);

    spdlog::register_logger(s_CoreLogger);
    spdlog::register_logger(s_ClientLogger);

    NOVA_LOG_INFO("Log file: {}", logPath);
}

void Log::Shutdown() {
    if (s_CoreLogger) {
        s_CoreLogger->flush();
    }
    s_CoreLogger.reset();
    s_ClientLogger.reset();
    spdlog::drop_all();
    spdlog::shutdown();
}

} // namespace Nova
