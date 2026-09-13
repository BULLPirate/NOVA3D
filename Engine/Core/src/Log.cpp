#include <Nova/Core/Log.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Nova {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init() {
    // Pattern: [timestamp] [level] logger_name: message (file:line)
    spdlog::set_pattern("%^[%T] %n: %v%$ (%s:%#)");

    // Core logger — console + file
    std::vector<spdlog::sink_ptr> coreSinks;
    coreSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    coreSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Nova3D.log", true));
    coreSinks[0]->set_level(spdlog::level::trace);
    coreSinks[1]->set_level(spdlog::level::trace);

    s_CoreLogger = std::make_shared<spdlog::logger>("NOVA", coreSinks.begin(), coreSinks.end());
    s_CoreLogger->set_level(spdlog::level::trace);

    // Client logger — console only
    s_ClientLogger = std::make_shared<spdlog::logger>("GAME", std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    s_ClientLogger->set_level(spdlog::level::trace);

    spdlog::register_logger(s_CoreLogger);
    spdlog::register_logger(s_ClientLogger);
}

void Log::Shutdown() {
    s_CoreLogger.reset();
    s_ClientLogger.reset();
    spdlog::drop_all();
    spdlog::shutdown();
}

} // namespace Nova
