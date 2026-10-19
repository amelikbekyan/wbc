// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "fakes/FakeCameraSource.h"

TEST(FakeCameraSource, OpensAndClosesLikeTheRealThing) {
    wbc::FakeCameraSource camera;
    EXPECT_FALSE(camera.IsOpen());
    EXPECT_TRUE(camera.Open("/dev/fake", {}));
    EXPECT_TRUE(camera.IsOpen());
    camera.Close();
    EXPECT_FALSE(camera.IsOpen());
    EXPECT_TRUE(camera.WasClosed());
}

TEST(FakeCameraSource, CanBeToldToRefuseToOpen) {
    wbc::FakeCameraSource camera;
    camera.SetOpenSucceeds(false);
    EXPECT_FALSE(camera.Open("/dev/fake", {}));
    EXPECT_FALSE(camera.LastError().empty());
}

TEST(FakeCameraSource, HandsBackThePushedFramesInOrderThenRepeatsTheLast) {
    wbc::FakeCameraSource camera;
    camera.PushFrame(cv::Mat(4, 4, CV_8UC3, cv::Scalar{1, 1, 1}));
    camera.PushFrame(cv::Mat(4, 4, CV_8UC3, cv::Scalar{2, 2, 2}));
    ASSERT_TRUE(camera.Open("/dev/fake", {}));

    cv::Mat frame;
    ASSERT_TRUE(camera.Read(frame));
    EXPECT_EQ(frame.at<cv::Vec3b>(0, 0)[0], 1);
    ASSERT_TRUE(camera.Read(frame));
    EXPECT_EQ(frame.at<cv::Vec3b>(0, 0)[0], 2);
    ASSERT_TRUE(camera.Read(frame));
    EXPECT_EQ(frame.at<cv::Vec3b>(0, 0)[0], 2);
    EXPECT_EQ(camera.ReadCount(), 3);
}

TEST(FakeCameraSource, StopsDeliveringAfterTheConfiguredNumberOfFrames) {
    wbc::FakeCameraSource camera;
    camera.PushFrame(cv::Mat(4, 4, CV_8UC3, cv::Scalar{1, 1, 1}));
    camera.SetFailAfter(2);
    ASSERT_TRUE(camera.Open("/dev/fake", {}));

    cv::Mat frame;
    EXPECT_TRUE(camera.Read(frame));
    EXPECT_TRUE(camera.Read(frame));
    EXPECT_FALSE(camera.Read(frame));
    EXPECT_FALSE(camera.LastError().empty());
}

TEST(FakeCameraSource, ReadingBeforeOpeningFails) {
    wbc::FakeCameraSource camera;
    cv::Mat frame;
    EXPECT_FALSE(camera.Read(frame));
}
