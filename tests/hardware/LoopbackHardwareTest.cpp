// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/videoio.hpp>

#include <chrono>
#include <thread>

#include "hardware/HardwareEnvironment.h"
#include "sink/PipeWireAnnouncingSink.h"
#include "sink/V4l2LoopbackSink.h"

namespace {

class LoopbackHardware : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = wbc::LoopbackDevicePath();
        if (path_.empty()) {
            GTEST_SKIP() << "No v4l2loopback device labelled Webcam Background Changer; run: pkexec modprobe "
                            "v4l2loopback devices=1 video_nr=10 card_label=\"Webcam Background Changer\" "
                            "exclusive_caps=1";
        }
    }

    std::string path_;
};

}  // namespace

TEST_F(LoopbackHardware, OpensTheDeviceAtTheRequestedGeometry) {
    wbc::V4l2LoopbackSink sink;
    ASSERT_TRUE(sink.Open(path_, 640, 480, 30.0)) << sink.LastError();
    EXPECT_TRUE(sink.IsOpen());
}

TEST_F(LoopbackHardware, AcceptsASequenceOfFrames) {
    wbc::V4l2LoopbackSink sink;
    ASSERT_TRUE(sink.Open(path_, 640, 480, 30.0)) << sink.LastError();

    const cv::Mat frame(480, 640, CV_8UC3, cv::Scalar{30, 60, 90});
    for (int i = 0; i < 10; ++i) {
        const wbc::WriteResult result = sink.Write(frame);
        ASSERT_NE(result, wbc::WriteResult::Fatal) << sink.LastError();
    }
}

TEST_F(LoopbackHardware, WhatIsWrittenCanBeReadBackFromTheDevice) {
    wbc::V4l2LoopbackSink sink;
    ASSERT_TRUE(sink.Open(path_, 640, 480, 30.0)) << sink.LastError();

    const cv::Mat written(480, 640, CV_8UC3, cv::Scalar{200, 60, 30});

    // exclusive_caps=1 makes this node advertise Video Output until something
    // streams into it, and only then Video Capture. A reader opened before the
    // first write would find no capture device at all.
    ASSERT_NE(sink.Write(written), wbc::WriteResult::Fatal) << sink.LastError();

    cv::VideoCapture reader(path_, cv::CAP_V4L2);
    ASSERT_TRUE(reader.isOpened()) << "could not open " << path_ << " for reading";

    // The driver keeps only the most recent frame, so keep writing while reading.
    cv::Mat readBack;
    bool matched = false;
    for (int attempt = 0; attempt < 30 && !matched; ++attempt) {
        ASSERT_NE(sink.Write(written), wbc::WriteResult::Fatal) << sink.LastError();
        if (reader.read(readBack) && !readBack.empty()) {
            const cv::Vec3b pixel = readBack.at<cv::Vec3b>(240, 320);
            matched = std::abs(pixel[0] - 200) < 12 && std::abs(pixel[1] - 60) < 12 &&
                      std::abs(pixel[2] - 30) < 12;
        }
    }

    ASSERT_TRUE(matched) << "the frame read back from the loopback did not match what was written";
    EXPECT_EQ(readBack.size(), cv::Size(640, 480));
}

TEST_F(LoopbackHardware, RejectsAFrameOfTheWrongSize) {
    wbc::V4l2LoopbackSink sink;
    ASSERT_TRUE(sink.Open(path_, 640, 480, 30.0)) << sink.LastError();
    const cv::Mat wrong(240, 320, CV_8UC3, cv::Scalar{0, 0, 0});
    EXPECT_EQ(sink.Write(wrong), wbc::WriteResult::Fatal);
    EXPECT_FALSE(sink.LastError().empty());
}

TEST_F(LoopbackHardware, ReportsAnUnopenableDevice) {
    wbc::V4l2LoopbackSink sink;
    EXPECT_FALSE(sink.Open("/dev/video999", 640, 480, 30.0));
    EXPECT_FALSE(sink.LastError().empty());
}

TEST_F(LoopbackHardware, PipeWireOffersTheDeviceAsASourceOnceFramesFlow) {
    // May restart WirePlumber, which interrupts audio for a moment: that is
    // the behaviour under test.
    if (!wbc::ReadProgramOutput("pw-dump", {}).has_value()) {
        GTEST_SKIP() << "PipeWire is not running, or pw-dump is not installed";
    }

    wbc::V4l2LoopbackSink loopback;
    auto sink = wbc::MakeDefaultPipeWireAnnouncingSink(loopback);
    ASSERT_TRUE(sink->Open(path_, 640, 480, 30.0)) << sink->LastError();

    // Frames keep flowing while WirePlumber comes back, as they do in a run:
    // it probes the node again on startup and must find it capturing.
    const cv::Mat frame(480, 640, CV_8UC3, cv::Scalar{30, 60, 90});
    bool offered = false;
    for (int attempt = 0; attempt < 100 && !offered; ++attempt) {
        ASSERT_NE(sink->Write(frame), wbc::WriteResult::Fatal) << sink->LastError();
        const auto dump = wbc::ReadProgramOutput("pw-dump", {});
        offered = dump.has_value() && wbc::PipeWireHasVideoSource(*dump, path_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    sink->Close();

    EXPECT_TRUE(offered) << "PipeWire has no Video/Source for " << path_;
}
