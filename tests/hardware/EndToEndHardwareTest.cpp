// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "camera/V4l2CameraSource.h"
#include "comp/BackgroundImage.h"
#include "comp/BackgroundVideo.h"
#include "comp/OpenCvVideoDecoder.h"
#include "comp/Compositor.h"
#include "core/FileModelLoader.h"
#include "core/PipelineController.h"
#include "core/SteadyClock.h"
#include "hardware/HardwareEnvironment.h"
#include "seg/MaskRefiner.h"
#include "seg/SelfieOnnx.h"
#include "sink/V4l2LoopbackSink.h"

namespace {

class EndToEnd : public ::testing::Test {
protected:
    void SetUp() override {
        cameraPath_ = wbc::FirstCaptureDevicePath();
        loopbackPath_ = wbc::LoopbackDevicePath();
        if (cameraPath_.empty()) {
            GTEST_SKIP() << "No V4L2 capture device found under /dev; connect a camera to run this test";
        }
        if (loopbackPath_.empty()) {
            GTEST_SKIP() << "No v4l2loopback device labelled Webcam Background Changer; run: pkexec modprobe "
                            "v4l2loopback devices=1 video_nr=10 card_label=\"Webcam Background Changer\" "
                            "exclusive_caps=1";
        }
    }

    std::string cameraPath_;
    std::string loopbackPath_;
};

}  // namespace

TEST_F(EndToEnd, AFrameWithAReplacedBackgroundReachesTheVirtualCamera) {
    wbc::SteadyClock clock;
    wbc::FileModelLoader loader;
    wbc::SelfieOnnx segmenter{loader, WBC_MODEL_PATH};
    wbc::MaskRefiner refiner{clock};
    wbc::Compositor compositor;
    wbc::BackgroundImage background;
    wbc::BackgroundVideo video{clock, [] { return std::make_unique<wbc::OpenCvVideoDecoder>(); }};
    wbc::V4l2CameraSource camera;
    wbc::V4l2LoopbackSink sink;
    wbc::PipelineController controller{camera, segmenter, refiner, compositor,
                                      background, video, sink, clock};

    wbc::PipelineConfig config;
    config.cameraPath = cameraPath_;
    config.sinkPath = loopbackPath_;
    // A solid colour makes the check unambiguous: whatever the mask decides,
    // every background pixel must be this exact colour.
    config.compositor.mode = wbc::BackgroundMode::Color;
    config.compositor.color = cv::Scalar{255, 0, 0};

    ASSERT_TRUE(controller.Prepare(config)) << controller.LastError().toStdString();

    // exclusive_caps=1 makes this node advertise Video Output until something
    // streams into it, and only then Video Capture. A reader opened before the
    // first frame would find no capture device at all.
    ASSERT_TRUE(controller.ProcessOneFrame()) << controller.LastError().toStdString();

    cv::VideoCapture reader(loopbackPath_, cv::CAP_V4L2);
    ASSERT_TRUE(reader.isOpened()) << "could not open " << loopbackPath_ << " for reading";

    cv::Mat readBack;
    bool gotAFrame = false;
    for (int i = 0; i < 60 && !gotAFrame; ++i) {
        ASSERT_TRUE(controller.ProcessOneFrame()) << controller.LastError().toStdString();
        gotAFrame = reader.read(readBack) && !readBack.empty();
    }

    ASSERT_TRUE(gotAFrame) << "nothing could be read back from the virtual camera";
    EXPECT_EQ(readBack.size(), cv::Size(camera.ActualFormat().width, camera.ActualFormat().height));
    EXPECT_GT(controller.Stats().delivered, 0u);

    // The check the comment above promises. Whatever the mask decides, every
    // pixel it does not claim is the background colour, and no real scene is
    // entirely foreground -- so a substantial part of the frame must be that
    // colour. A sink writing garbage, or a background that was never
    // replaced, fails this. The tolerance is for the BGR -> I420 -> BGR round
    // trip the loopback performs, the same allowance LoopbackHardwareTest
    // makes for the same reason.
    int backgroundPixels = 0;
    for (int y = 0; y < readBack.rows; ++y) {
        for (int x = 0; x < readBack.cols; ++x) {
            const cv::Vec3b pixel = readBack.at<cv::Vec3b>(y, x);
            if (std::abs(pixel[0] - 255) < 16 && std::abs(pixel[1] - 0) < 16 &&
                std::abs(pixel[2] - 0) < 16) {
                ++backgroundPixels;
            }
        }
    }
    EXPECT_GT(backgroundPixels, static_cast<int>(readBack.total() / 10))
        << "only " << backgroundPixels << " of " << readBack.total()
        << " pixels carried the background colour that was asked for";
}

// Under a sanitizer everything runs several times slower -- and under
// ThreadSanitizer OpenCV runs on one thread as well -- so a rate that would
// mean something is wrong in an ordinary build means nothing there. The
// sanitized builds ask only that the pipeline ran.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr unsigned kFewestFrames = 0;
constexpr double kLowestRate = 0.0;
#else
constexpr unsigned kFewestFrames = 10;
constexpr double kLowestRate = 5.0;
#endif

TEST_F(EndToEnd, TheThreadedPipelineRunsAndStopsCleanly) {
    wbc::SteadyClock clock;
    wbc::FileModelLoader loader;
    wbc::SelfieOnnx segmenter{loader, WBC_MODEL_PATH};
    wbc::MaskRefiner refiner{clock};
    wbc::Compositor compositor;
    wbc::BackgroundImage background;
    wbc::BackgroundVideo video{clock, [] { return std::make_unique<wbc::OpenCvVideoDecoder>(); }};
    wbc::V4l2CameraSource camera;
    wbc::V4l2LoopbackSink sink;
    wbc::PipelineController controller{camera, segmenter, refiner, compositor,
                                      background, video, sink, clock};

    wbc::PipelineConfig config;
    config.cameraPath = cameraPath_;
    config.sinkPath = loopbackPath_;

    ASSERT_TRUE(controller.Start(config)) << controller.LastError().toStdString();
    // Read before Stop(): closing the camera clears the negotiated format.
    const wbc::FormatOption negotiated = camera.ActualFormat();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    controller.Stop();

    // Read after Stop(), not before: Stats() -> Current() -> Evict() mutates
    // the stats deque through a mutable member, and while the worker is still
    // running it calls RecordDelivered() on that same deque from its own
    // thread. Reading here, after the worker has been joined by Stop(), keeps
    // this a single-threaded read instead of a data race that only the test
    // creates. The one-second sliding window easily survives the few
    // milliseconds Stop() takes, so the assertions below still hold.
    const auto snapshot = controller.Stats();

    // The spec's target is 30 fps at 1280x720 (section 12, risk 2). This is
    // deliberately NOT asserted: a loaded machine would fail a gate it has no
    // business failing. The number is printed instead, so that a human running
    // the hardware suite sees what the pipeline actually managed and can
    // compare it against the target.
    std::cout << "[   RATE   ] " << snapshot.delivered << " frames delivered in two seconds, "
              << snapshot.fps << " fps measured at " << negotiated.width << "x"
              << negotiated.height << " (the spec's target is 30 at 1280x720)"
              << std::endl;

    EXPECT_FALSE(controller.IsRunning());
    EXPECT_GT(snapshot.delivered, kFewestFrames) << "too few frames in two seconds";
    // Not a hard performance gate, but a rate this low means something is wrong.
    EXPECT_GT(snapshot.fps, kLowestRate);
}
