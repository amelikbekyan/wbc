// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "fakes/FakeSink.h"

TEST(FakeSink, RecordsTheConfiguredGeometry) {
    wbc::FakeSink sink;
    ASSERT_TRUE(sink.Open("/dev/fake", 1280, 720, 30.0));
    EXPECT_EQ(sink.ConfiguredSize(), cv::Size(1280, 720));
    EXPECT_EQ(sink.OpenCount(), 1);
}

TEST(FakeSink, KeepsEveryFrameItWasGiven) {
    wbc::FakeSink sink;
    ASSERT_TRUE(sink.Open("/dev/fake", 8, 8, 30.0));
    sink.Write(cv::Mat(8, 8, CV_8UC3, cv::Scalar{1, 1, 1}));
    sink.Write(cv::Mat(8, 8, CV_8UC3, cv::Scalar{2, 2, 2}));
    ASSERT_EQ(sink.Frames().size(), 2u);
    EXPECT_EQ(sink.Frames()[1].at<cv::Vec3b>(0, 0)[0], 2);
}

TEST(FakeSink, CanBeToldToReportBackPressure) {
    wbc::FakeSink sink;
    ASSERT_TRUE(sink.Open("/dev/fake", 8, 8, 30.0));
    sink.SetResult(wbc::WriteResult::Dropped);
    EXPECT_EQ(sink.Write(cv::Mat(8, 8, CV_8UC3, cv::Scalar{0, 0, 0})), wbc::WriteResult::Dropped);
    EXPECT_TRUE(sink.Frames().empty());
}

TEST(FakeSink, WritingBeforeOpeningIsFatal) {
    wbc::FakeSink sink;
    EXPECT_EQ(sink.Write(cv::Mat(8, 8, CV_8UC3, cv::Scalar{0, 0, 0})), wbc::WriteResult::Fatal);
}
