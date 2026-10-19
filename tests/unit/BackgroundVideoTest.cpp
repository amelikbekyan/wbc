// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "comp/BackgroundVideo.h"
#include "core/SteadyClock.h"
#include "fakes/FakeClock.h"
#include "fakes/FakeVideoDecoder.h"

namespace {

using namespace std::chrono_literals;

// Frame i is a flat grey of brightness i, so which frame is on offer can be
// read off any pixel.
std::vector<cv::Mat> NumberedFrames(int count, cv::Size size = {4, 4}) {
    std::vector<cv::Mat> frames;
    for (int i = 0; i < count; ++i) {
        frames.emplace_back(size, CV_8UC3, cv::Scalar::all(10 * (i + 1)));
    }
    return frames;
}

int FrameNumber(const cv::Mat& frame) {
    return frame.at<cv::Vec3b>(0, 0)[0] / 10 - 1;
}

// A player whose decoder the test keeps a hand on, for the switches.
struct Player {
    wbc::FakeClock clock;
    wbc::FakeVideoDecoder* decoder = nullptr;
    std::vector<cv::Mat> frames;
    double fps;
    wbc::BackgroundVideo video;

    explicit Player(int count = 3, double framesPerSecond = 10.0)
        : frames(NumberedFrames(count)),
          fps(framesPerSecond),
          video(clock, [this] {
              auto made = std::make_unique<wbc::FakeVideoDecoder>(frames, fps);
              decoder = made.get();
              return made;
          }) {}

    // One frame period at 10 fps.
    void Tick() { clock.Advance(100ms); }
};

}  // namespace

TEST(BackgroundVideo, OffersNothingBeforeAVideoIsLoaded) {
    Player player;
    EXPECT_FALSE(player.video.IsLoaded());
    EXPECT_TRUE(player.video.For({8, 8}).empty());
    EXPECT_FALSE(player.video.Step());

    player.video.Play();
    EXPECT_FALSE(player.video.IsPlaying()) << "played with nothing to play";
}

TEST(BackgroundVideo, ShowsTheFirstFrameAsSoonAsItIsLoaded) {
    Player player;
    ASSERT_TRUE(player.video.Load("/clip.mp4"));

    EXPECT_TRUE(player.video.IsLoaded());
    EXPECT_EQ(player.decoder->LastPath(), "/clip.mp4");
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 0);
}

TEST(BackgroundVideo, FitsTheFrameToTheSizeAskedFor) {
    Player player;
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Play();
    player.video.Pause();

    const cv::Mat fitted = player.video.For({16, 9});
    EXPECT_EQ(fitted.size(), cv::Size(16, 9));

    // Later frames arrive already at that size, fitted off the pipeline's
    // thread: For() hands back what was published without resizing.
    player.Tick();
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(player.video.For({16, 9}).size(), cv::Size(16, 9));
    EXPECT_EQ(FrameNumber(player.video.For({16, 9})), 1);
}

TEST(BackgroundVideo, RefusesAFileThatWillNotOpenAndKeepsWhatItHad) {
    Player player;
    ASSERT_TRUE(player.video.Load("/good.mp4"));

    player.frames.clear();  // the next decoder made will have nothing to read
    EXPECT_FALSE(player.video.Load("/empty.mp4"));

    EXPECT_FALSE(player.video.LastError().empty());
    EXPECT_TRUE(player.video.IsLoaded());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 0);
}

TEST(BackgroundVideo, SaysWhyAFileWasRefused) {
    wbc::FakeClock clock;
    wbc::BackgroundVideo video(clock, [] {
        auto decoder = std::make_unique<wbc::FakeVideoDecoder>();
        decoder->SetOpenSucceeds(false);
        return decoder;
    });

    EXPECT_FALSE(video.Load("/missing.mp4"));
    EXPECT_NE(video.LastError().find("/missing.mp4"), std::string::npos);
    EXPECT_FALSE(video.IsLoaded());
}

TEST(BackgroundVideo, MovesOnOnlyWhenTheNextFrameIsDue) {
    Player player;
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Play();
    player.video.Pause();  // the frames are driven by hand from here

    player.clock.Advance(50ms);  // half a frame
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 0);

    player.clock.Advance(50ms);  // now a whole one
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
}

TEST(BackgroundVideo, StartsAgainFromTheTopWhenTheVideoEnds) {
    Player player{2};
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Play();
    player.video.Pause();

    player.Tick();
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);

    player.Tick();
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 0);
    EXPECT_EQ(player.decoder->RewindCount(), 1);
}

TEST(BackgroundVideo, PlaysSlowerRatherThanBurstingWhenItFallsBehind) {
    Player player{5};
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Play();
    player.video.Pause();

    // Five frame periods pass with nobody decoding.
    player.clock.Advance(500ms);
    ASSERT_TRUE(player.video.Step());
    // One frame on, not four: the backlog is dropped, not decoded.
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
    EXPECT_EQ(player.decoder->ReadCount(), 2);

    // And the next frame is due one period from now, not immediately.
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
}

TEST(BackgroundVideo, PlaysAFileThatStatesNoFrameRateAtThirty) {
    Player player{3, 0.0};
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Play();
    player.video.Pause();

    player.clock.Advance(34ms);  // just over a thirtieth of a second
    ASSERT_TRUE(player.video.Step());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
}

TEST(BackgroundVideo, HoldsTheLastGoodFrameWhenTheVideoCannotBeRewound) {
    Player player{2};
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.decoder->SetRewindSucceeds(false);
    player.video.Play();
    player.video.Pause();

    player.Tick();
    ASSERT_TRUE(player.video.Step());
    player.Tick();
    EXPECT_FALSE(player.video.Step());

    EXPECT_FALSE(player.video.LastError().empty());
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
}

TEST(BackgroundVideo, HoldsTheLastGoodFrameWhenReadingStopsWorking) {
    Player player{3};
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.decoder->FailReadsAfter(2);
    player.video.Play();
    player.video.Pause();

    player.Tick();
    ASSERT_TRUE(player.video.Step());
    player.Tick();
    EXPECT_FALSE(player.video.Step()) << "a rewind that cannot read again is a failure";
    EXPECT_EQ(FrameNumber(player.video.For({4, 4})), 1);
}

TEST(BackgroundVideo, ForgetsTheVideoWhenCleared) {
    Player player;
    ASSERT_TRUE(player.video.Load("/clip.mp4"));
    player.video.Clear();

    EXPECT_FALSE(player.video.IsLoaded());
    EXPECT_TRUE(player.video.For({4, 4}).empty());
    EXPECT_FALSE(player.video.Step());
}

// The thread itself, on the real clock. These wait for something to happen
// rather than for a fixed time, and give up after two seconds.
namespace {

template <typename Condition>
bool WaitFor(Condition condition) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

}  // namespace

TEST(BackgroundVideo, PlaysOnItsOwnThreadUntilPaused) {
    wbc::SteadyClock clock;
    const std::vector<cv::Mat> frames = NumberedFrames(5);
    wbc::BackgroundVideo video(clock,
                              [&] { return std::make_unique<wbc::FakeVideoDecoder>(frames, 100.0); });
    ASSERT_TRUE(video.Load("/clip.mp4"));

    video.Play();
    EXPECT_TRUE(video.IsPlaying());
    EXPECT_TRUE(WaitFor([&] { return FrameNumber(video.For({4, 4})) != 0; }))
        << "the thread never moved past the first frame";

    video.Pause();
    EXPECT_FALSE(video.IsPlaying());
}

TEST(BackgroundVideo, CanBePlayedAgainAfterItsThreadStoppedOnItsOwn) {
    wbc::SteadyClock clock;
    const std::vector<cv::Mat> frames = NumberedFrames(2);
    wbc::BackgroundVideo video(clock, [&] {
        auto decoder = std::make_unique<wbc::FakeVideoDecoder>(frames, 100.0);
        decoder->FailReadsAfter(1);  // the first frame, then nothing
        return decoder;
    });
    ASSERT_TRUE(video.Load("/clip.mp4"));

    video.Play();
    ASSERT_TRUE(WaitFor([&] { return !video.IsPlaying(); }));

    // The finished thread has to be joined before another starts, or the
    // program terminates here.
    video.Play();
    EXPECT_TRUE(WaitFor([&] { return !video.IsPlaying(); }));
}

TEST(BackgroundVideo, LoadingAnotherVideoStopsTheOneThatIsPlaying) {
    wbc::SteadyClock clock;
    const std::vector<cv::Mat> frames = NumberedFrames(3);
    wbc::BackgroundVideo video(clock,
                              [&] { return std::make_unique<wbc::FakeVideoDecoder>(frames, 100.0); });
    ASSERT_TRUE(video.Load("/first.mp4"));
    video.Play();

    ASSERT_TRUE(video.Load("/second.mp4"));
    EXPECT_FALSE(video.IsPlaying());
}

TEST(BackgroundVideo, CanBeDestroyedWhilePlaying) {
    wbc::SteadyClock clock;
    const std::vector<cv::Mat> frames = NumberedFrames(3);
    auto video = std::make_unique<wbc::BackgroundVideo>(
        clock, [&] { return std::make_unique<wbc::FakeVideoDecoder>(frames, 100.0); });
    ASSERT_TRUE(video->Load("/clip.mp4"));
    video->Play();
    video.reset();  // joins rather than leaving a thread running into freed memory
    SUCCEED();
}
