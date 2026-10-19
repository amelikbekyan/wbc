// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "camera/FormatNegotiator.h"
#include "camera/V4l2CameraSource.h"
#include "hardware/HardwareEnvironment.h"

namespace {

class CameraHardware : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = wbc::FirstCaptureDevicePath();
        if (path_.empty()) {
            GTEST_SKIP() << "No V4L2 capture device found under /dev; connect a camera to run this test";
        }
    }

    std::string path_;
};

}  // namespace

TEST_F(CameraHardware, OpensTheDeviceAndReportsWhatItActuallyGot) {
    wbc::V4l2CameraSource camera;
    wbc::FormatRequest request;
    request.fourccPreference = {wbc::kFourccMjpg, wbc::kFourccYuyv};

    ASSERT_TRUE(camera.Open(path_, request)) << camera.LastError();
    const wbc::FormatOption actual = camera.ActualFormat();
    EXPECT_GT(actual.width, 0);
    EXPECT_GT(actual.height, 0);
}

TEST_F(CameraHardware, DeliversFramesMatchingTheNegotiatedFormat) {
    wbc::V4l2CameraSource camera;
    ASSERT_TRUE(camera.Open(path_, {})) << camera.LastError();

    cv::Mat frame;
    ASSERT_TRUE(camera.Read(frame)) << camera.LastError();
    EXPECT_EQ(frame.type(), CV_8UC3);
    EXPECT_EQ(frame.cols, camera.ActualFormat().width);
    EXPECT_EQ(frame.rows, camera.ActualFormat().height);
}

TEST_F(CameraHardware, DeliversASequenceOfFramesWithoutFailing) {
    wbc::V4l2CameraSource camera;
    ASSERT_TRUE(camera.Open(path_, {})) << camera.LastError();

    cv::Mat frame;
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(camera.Read(frame)) << "failed on frame " << i << ": " << camera.LastError();
        ASSERT_FALSE(frame.empty());
    }
}

TEST_F(CameraHardware, ReportsAnUnopenableDeviceInsteadOfThrowing) {
    wbc::V4l2CameraSource camera;
    EXPECT_FALSE(camera.Open("/dev/video999", {}));
    EXPECT_FALSE(camera.LastError().empty());
}

TEST_F(CameraHardware, AdvertisesAtLeastOneSupportedFormat) {
    const auto formats = wbc::QuerySupportedFormats(path_);
    EXPECT_FALSE(formats.empty());
}
