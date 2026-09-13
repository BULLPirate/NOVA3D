#include <gtest/gtest.h>
#include <Nova/Core/Log.h>

TEST(Log, InitAndShutdown) {
    Nova::Log::Init();
    ASSERT_NE(Nova::Log::GetCoreLogger(), nullptr);
    ASSERT_NE(Nova::Log::GetClientLogger(), nullptr);

    NOVA_LOG_INFO("Test log message: {}", 42);
    NOVA_LOG_GAME_INFO("Game log message: {}", "hello");

    Nova::Log::Shutdown();
    SUCCEED();
}
