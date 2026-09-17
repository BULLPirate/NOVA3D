#include <gtest/gtest.h>

#include <Nova/Core/Events.h>
#include <Nova/Core/FileSystem.h>
#include <Nova/Core/Guid.h>
#include <Nova/Core/Time.h>

#include <chrono>
#include <filesystem>
#include <thread>

TEST(Guid, GenerateIsVersion4AndUnique) {
    const Nova::Guid a = Nova::Guid::Generate();
    const Nova::Guid b = Nova::Guid::Generate();
    EXPECT_FALSE(a.IsNil());
    EXPECT_NE(a, b);
    EXPECT_EQ(a.Bytes[6] & 0xf0u, 0x40u);
    EXPECT_EQ(a.Bytes[8] & 0xc0u, 0x80u);
}

TEST(Guid, ParseRoundTrip) {
    const Nova::Guid original = Nova::Guid::Generate();
    Nova::Guid parsed;
    ASSERT_TRUE(Nova::Guid::TryParse(original.ToString(), parsed));
    EXPECT_EQ(original, parsed);
}

TEST(Guid, RejectsBadString) {
    Nova::Guid parsed;
    EXPECT_FALSE(Nova::Guid::TryParse("not-a-guid", parsed));
    EXPECT_TRUE(Nova::Guid::Nil().IsNil());
}

TEST(Clock, FirstTickHasZeroDeltaThenAdvances) {
    Nova::Clock clock;
    clock.Tick();
    EXPECT_FLOAT_EQ(clock.DeltaSeconds(), 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    clock.Tick();
    EXPECT_GT(clock.DeltaSeconds(), 0.0f);
    EXPECT_GT(clock.TotalSeconds(), 0.0f);
    clock.Reset();
    EXPECT_FLOAT_EQ(clock.DeltaSeconds(), 0.0f);
}

TEST(FileSystem, WriteReadRoundTrip) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "nova_fs_test" / "note.txt";
    const Nova::FileIOResult write = Nova::WriteTextFile(path, "hello nova");
    ASSERT_TRUE(write.Ok) << write.Error;
    ASSERT_TRUE(Nova::FileExists(path));
    const Nova::FileIOResult read = Nova::ReadTextFile(path);
    ASSERT_TRUE(read.Ok) << read.Error;
    EXPECT_EQ(read.Text, "hello nova");
    std::error_code ec;
    std::filesystem::remove_all(path.parent_path(), ec);
}

TEST(EventBus, SubscribePublishUnsubscribe) {
    struct Ping {
        int Value = 0;
    };
    Nova::EventBus bus;
    int seen = 0;
    const Nova::EventBus::Token token = bus.Subscribe<Ping>([&](const Ping& ping) {
        seen += ping.Value;
    });
    bus.Publish(Ping{3});
    EXPECT_EQ(seen, 3);
    bus.Unsubscribe(token);
    bus.Publish(Ping{5});
    EXPECT_EQ(seen, 3);
}
