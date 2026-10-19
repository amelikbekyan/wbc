// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "core/PipelineStats.h"
#include "fakes/FakeClock.h"

using namespace std::chrono_literals;

TEST(PipelineStats, ReportsZeroBeforeAnyFrame) {
    wbc::FakeClock clock;
    const wbc::PipelineStats stats{clock};
    const auto snapshot = stats.Current();
    EXPECT_DOUBLE_EQ(snapshot.fps, 0.0);
    EXPECT_EQ(snapshot.delivered, 0u);
    EXPECT_EQ(snapshot.dropped, 0u);
}

TEST(PipelineStats, ThirtyFramesInOneSecondReportThirtyFps) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock};
    for (int i = 0; i < 30; ++i) {
        stats.RecordDelivered(5.0);
        clock.Advance(std::chrono::duration<double>(1.0 / 30.0));
    }
    EXPECT_NEAR(stats.Current().fps, 30.0, 0.5);
}

TEST(PipelineStats, FramesOlderThanTheWindowStopCounting) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock, 1.0};
    for (int i = 0; i < 10; ++i) {
        stats.RecordDelivered(5.0);
    }
    clock.Advance(2s);
    stats.RecordDelivered(5.0);
    EXPECT_NEAR(stats.Current().fps, 1.0, 0.5);
}

TEST(PipelineStats, LatencyIsTheMeanOverTheWindow) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock};
    stats.RecordDelivered(10.0);
    clock.Advance(100ms);
    stats.RecordDelivered(20.0);
    EXPECT_DOUBLE_EQ(stats.Current().latencyMs, 15.0);
}

TEST(PipelineStats, DroppedFramesAreCountedSeparatelyAndNeverAffectFps) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock};
    stats.RecordDelivered(5.0);
    stats.RecordDropped();
    stats.RecordDropped();
    const auto snapshot = stats.Current();
    EXPECT_EQ(snapshot.delivered, 1u);
    EXPECT_EQ(snapshot.dropped, 2u);
}

TEST(PipelineStats, ResetClearsEverything) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock};
    stats.RecordDelivered(5.0);
    stats.RecordDropped();
    stats.Reset();
    const auto snapshot = stats.Current();
    EXPECT_EQ(snapshot.delivered, 0u);
    EXPECT_EQ(snapshot.dropped, 0u);
    EXPECT_DOUBLE_EQ(snapshot.fps, 0.0);
}

TEST(PipelineStats, ABurstAtASingleInstantIsReportedAsItsCountNotAsOneFrame) {
    wbc::FakeClock clock;
    wbc::PipelineStats stats{clock};
    for (int i = 0; i < 50; ++i) {
        stats.RecordDelivered(1.0);
    }
    // No interval exists between these, so the rate is the count over the window.
    EXPECT_DOUBLE_EQ(stats.Current().fps, 50.0);
    EXPECT_EQ(stats.Current().delivered, 50u);
}
