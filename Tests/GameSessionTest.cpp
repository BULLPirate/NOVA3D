#include <gtest/gtest.h>
#include <Nova/Scene/GameSession.h>

TEST(GameSession, SplashBecomesTitleThenPlay) {
    Nova::GameSession session;
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Splash);
    session.Tick(1.5f);
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Title);
    session.RequestPlay();
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Loading);
    EXPECT_TRUE(session.BlocksWorldTick());
    session.Tick(0.5f);
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Playing);
    EXPECT_FALSE(session.BlocksWorldTick());
}

TEST(GameSession, PauseAndSettingsRoundTrip) {
    Nova::GameSession session;
    session.Current = Nova::GameSession::Phase::Playing;
    session.TogglePause();
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Paused);
    session.OpenSettings();
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Settings);
    session.CloseSettings();
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Paused);
    session.QuitToTitle();
    EXPECT_EQ(session.Current, Nova::GameSession::Phase::Title);
}
