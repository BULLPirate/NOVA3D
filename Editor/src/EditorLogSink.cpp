#include <Editor/EditorLogSink.h>

#include <Nova/Core/Log.h>

#include <spdlog/sinks/callback_sink.h>

#include <mutex>
#include <cstddef>

namespace Nova::Editor {

namespace {

constexpr std::size_t kMaxLines = 250;
std::mutex g_Mutex;
std::vector<std::string> g_Lines;
std::shared_ptr<spdlog::sinks::callback_sink_mt> g_Sink;

} // namespace

void AttachEditorLogSink() {
    if (g_Sink) {
        return;
    }
    g_Sink = std::make_shared<spdlog::sinks::callback_sink_mt>([](const spdlog::details::log_msg& msg) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_Lines.emplace_back(msg.payload.begin(), msg.payload.end());
        if (g_Lines.size() > kMaxLines) {
            g_Lines.erase(g_Lines.begin(), g_Lines.begin() + static_cast<std::ptrdiff_t>(g_Lines.size() - kMaxLines));
        }
    });
    if (auto logger = Nova::Log::GetCoreLogger()) {
        logger->sinks().push_back(g_Sink);
    }
}

void DetachEditorLogSink() {
    g_Sink.reset();
}

std::vector<std::string> CopyEditorLogLines() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Lines;
}

void ClearEditorLog() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Lines.clear();
}

} // namespace Nova::Editor
