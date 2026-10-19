// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QImage>
#include <QObject>
#include <QSignalSpy>

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#include "comp/BackgroundImage.h"
#include "comp/BackgroundVideo.h"
#include "comp/Compositor.h"
#include "core/PipelineController.h"
#include "fakes/FakeCameraSource.h"
#include "fakes/FakeClock.h"
#include "fakes/FakeSegmenter.h"
#include "fakes/FakeSink.h"
#include "fakes/FakeVideoDecoder.h"
#include "seg/MaskRefiner.h"
#include "sink/IFrameSink.h"

namespace {

// Everything the controller needs, assembled once so that each test reads as
// the behaviour it is checking rather than as construction.
struct Harness {
    wbc::FakeClock clock;
    wbc::FakeCameraSource camera;
    wbc::FakeSegmenter segmenter;
    wbc::MaskRefiner refiner{clock};
    wbc::Compositor compositor;
    wbc::BackgroundImage background;
    // A two-frame video of flat red then flat blue, so a composed frame says
    // which one it was built on.
    std::vector<cv::Mat> videoFrames{cv::Mat(48, 64, CV_8UC3, cv::Scalar{0, 0, 255}),
                                     cv::Mat(48, 64, CV_8UC3, cv::Scalar{255, 0, 0})};
    bool videoOpens = true;
    wbc::BackgroundVideo video{clock, [this] {
                                  auto decoder = std::make_unique<wbc::FakeVideoDecoder>(videoFrames);
                                  decoder->SetOpenSucceeds(videoOpens);
                                  return decoder;
                              }};
    wbc::FakeSink sink;
    wbc::PipelineController controller{camera, segmenter, refiner, compositor,
                                      background, video, sink, clock};

    Harness() {
        camera.PushFrame(cv::Mat(48, 64, CV_8UC3, cv::Scalar{10, 20, 30}));
        segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{1.0}));
    }

    static wbc::PipelineConfig Config() {
        wbc::PipelineConfig config;
        config.cameraPath = "/dev/fake-camera";
        config.sinkPath = "/dev/fake-loopback";
        config.compositor.mode = wbc::BackgroundMode::Color;
        config.compositor.color = cv::Scalar{0, 0, 0};
        return config;
    }
};

// FakeSink cannot be made to throw and is not changed here: it models a sink
// that reports failure through WriteResult, which is the contract. This is a
// second sink, for the other thing a real one can do. V4l2LoopbackSink::Write
// catches only std::invalid_argument, so a cv::Exception from cv::cvtColor
// inside ToI420 leaves Write and lands in the worker thread.
class ThrowingSink final : public wbc::IFrameSink {
public:
    bool Open(const std::string&, int, int, double) override { return true; }
    bool IsOpen() const override { return true; }
    wbc::WriteResult Write(const cv::Mat&) override {
        throw std::runtime_error("ThrowingSink was told to fail");
    }
    void Close() override {}
    std::string LastError() const override { return {}; }
};

}  // namespace

TEST(PipelineController, PrepareOpensBothDevicesAtTheCameraGeometry) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config())) << harness.controller.LastError().toStdString();
    EXPECT_EQ(harness.camera.OpenedPath(), "/dev/fake-camera");
    EXPECT_EQ(harness.sink.ConfiguredSize(), cv::Size(64, 48));
    // FakeCameraSource never sets a frame rate, so this is the fallback in
    // Prepare, not a rate the camera reported. Every test here takes that
    // branch, and so does any real camera whose driver enumerates no discrete
    // frame intervals -- it is a live path, not a corner.
    EXPECT_DOUBLE_EQ(harness.sink.ConfiguredFps(), 30.0);
}

TEST(PipelineController, PrepareFailsWhenTheCameraWillNotOpen) {
    Harness harness;
    harness.camera.SetOpenSucceeds(false);
    EXPECT_FALSE(harness.controller.Prepare(Harness::Config()));
    EXPECT_FALSE(harness.controller.LastError().isEmpty());
}

TEST(PipelineController, PrepareFailsWhenTheVirtualCameraWillNotOpen) {
    Harness harness;
    harness.sink.SetOpenSucceeds(false);
    EXPECT_FALSE(harness.controller.Prepare(Harness::Config()));
    EXPECT_FALSE(harness.controller.LastError().isEmpty());
}

TEST(PipelineController, EachProcessedFrameReachesTheVirtualCamera) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    for (int i = 0; i < 100; ++i) {
        harness.clock.Advance(std::chrono::duration<double>(1.0 / 30.0));
        ASSERT_TRUE(harness.controller.ProcessOneFrame()) << "stopped at frame " << i;
    }

    ASSERT_EQ(harness.sink.Frames().size(), 100u);
    EXPECT_EQ(harness.sink.Frames().front().size(), cv::Size(64, 48));
}

TEST(PipelineController, AFullyOpaqueMaskSendsTheCameraImageThrough) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const cv::Vec3b pixel = harness.sink.Frames().front().at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(pixel[0], 10, 2);
    EXPECT_NEAR(pixel[1], 20, 2);
    EXPECT_NEAR(pixel[2], 30, 2);
}

// The frame is shrunk once, to the model's resolution, and that copy feeds the
// model, the edge filter and the blur. A 43x32 model input on a 64x48 frame is
// the smallest the edge filter will still work at, so the shrink happens.
TEST(PipelineController, TheModelIsFedTheFrameShrunkOnceForEveryStage) {
    Harness harness;
    harness.segmenter.SetMask(cv::Mat(32, 43, CV_32FC1, cv::Scalar{1.0}));
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    EXPECT_EQ(harness.segmenter.LastFrameSize(), cv::Size(43, 32));
    ASSERT_EQ(harness.sink.Frames().front().size(), cv::Size(64, 48));
    const cv::Vec3b pixel = harness.sink.Frames().front().at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(pixel[0], 10, 2);
    EXPECT_NEAR(pixel[1], 20, 2);
    EXPECT_NEAR(pixel[2], 30, 2);
}

TEST(PipelineController, AFrameTooSmallToShrinkIsFedToTheModelWhole) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_EQ(harness.segmenter.LastFrameSize(), cv::Size(64, 48));
}

TEST(PipelineController, AnEmptyMaskSendsTheChosenBackgroundColour) {
    Harness harness;
    harness.segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{0.0}));

    wbc::PipelineConfig config = Harness::Config();
    config.compositor.color = cv::Scalar{7, 8, 9};
    ASSERT_TRUE(harness.controller.Prepare(config));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const cv::Vec3b pixel = harness.sink.Frames().front().at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(pixel[0], 7, 2);
    EXPECT_NEAR(pixel[1], 8, 2);
    EXPECT_NEAR(pixel[2], 9, 2);
}

TEST(PipelineController, EmitsAPreviewForEveryFrameTheWindowKeepsUpWith) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::previewReady);
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    ASSERT_TRUE(harness.controller.ProcessOneFrame());
    harness.controller.PreviewConsumed();
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    ASSERT_EQ(spy.count(), 2);
    const auto image = spy.at(0).at(0).value<QImage>();
    EXPECT_EQ(image.size(), QSize(64, 48));
}

TEST(PipelineController, DropsThePreviewWhenTheWindowHasNotTakenTheLastOne) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::previewReady);
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    // No PreviewConsumed() between the two: a GUI thread that has fallen
    // behind must cost the pipeline nothing. Queueing the second frame would
    // hand Qt another 2.7 MB image at 720p with nothing to shed it.
    ASSERT_TRUE(harness.controller.ProcessOneFrame());
    ASSERT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_EQ(spy.count(), 1);

    harness.controller.PreviewConsumed();
    ASSERT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_EQ(spy.count(), 2);
}

TEST(PipelineController, ACameraThatStopsDeliveringHaltsThePipelineAndSaysSo) {
    Harness harness;
    harness.camera.SetFailAfter(3);
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    EXPECT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_FALSE(harness.controller.ProcessOneFrame());

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(0).toString().isEmpty());
}

TEST(PipelineController, ADroppedFrameIsCountedAndThePipelineCarriesOn) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    harness.sink.SetResult(wbc::WriteResult::Dropped);

    EXPECT_TRUE(harness.controller.ProcessOneFrame());
    EXPECT_TRUE(harness.controller.ProcessOneFrame());

    EXPECT_EQ(harness.controller.Stats().dropped, 2u);
    EXPECT_EQ(harness.controller.Stats().delivered, 0u);
}

TEST(PipelineController, AVirtualCameraThatDisappearsHaltsThePipeline) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    harness.sink.SetResult(wbc::WriteResult::Fatal);

    EXPECT_FALSE(harness.controller.ProcessOneFrame());
    EXPECT_EQ(spy.count(), 1);
}

TEST(PipelineController, AFailingSegmenterHaltsThePipelineWithItsMessage) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    harness.segmenter.SetThrowOnNextCall(true);

    EXPECT_FALSE(harness.controller.ProcessOneFrame());
    ASSERT_EQ(spy.count(), 1);
    EXPECT_NE(spy.at(0).at(0).toString().indexOf("told to fail"), -1);
}

TEST(PipelineController, ChangedSettingsTakeEffectOnTheNextFrame) {
    Harness harness;
    harness.segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{0.0}));
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    wbc::CompositorSettings changed;
    changed.mode = wbc::BackgroundMode::Color;
    changed.color = cv::Scalar{111, 112, 113};
    harness.controller.UpdateCompositorSettings(changed);
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const cv::Vec3b pixel = harness.sink.Frames().back().at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(pixel[0], 111, 2);
}

TEST(PipelineController, RefusesABackgroundImageItCannotReadAndKeepsRunning) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    EXPECT_FALSE(harness.controller.SetBackgroundImage("/nonexistent/beach.png"));
    EXPECT_FALSE(harness.controller.LastError().isEmpty());
    EXPECT_TRUE(harness.controller.ProcessOneFrame());
}

TEST(PipelineController, StartRunsUntilStopIsCalled) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Start(Harness::Config()));
    EXPECT_TRUE(harness.controller.IsRunning());

    // Run() does no pacing at all: with a fake camera that returns at once it
    // is a busy loop, and in production the rate comes from
    // cv::VideoCapture::read blocking until the camera has a frame. So this
    // wait is not for a clock to advance -- it is simply long enough for the
    // worker thread to have been scheduled and produced something.
    for (int i = 0; i < 50 && harness.sink.Frames().empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    harness.controller.Stop();
    EXPECT_FALSE(harness.controller.IsRunning());
    EXPECT_FALSE(harness.sink.Frames().empty());
    EXPECT_TRUE(harness.sink.WasClosed());
    EXPECT_TRUE(harness.camera.WasClosed());
}

TEST(PipelineController, StoppingTwiceIsHarmless) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Start(Harness::Config()));
    harness.controller.Stop();
    harness.controller.Stop();
    EXPECT_FALSE(harness.controller.IsRunning());
}

TEST(PipelineController, StartRefusesWhenPreparationFails) {
    Harness harness;
    harness.camera.SetOpenSucceeds(false);
    EXPECT_FALSE(harness.controller.Start(Harness::Config()));
    EXPECT_FALSE(harness.controller.IsRunning());
}

TEST(PipelineController, LoadsTheBackgroundPictureNamedInTheConfig) {
    Harness harness;
    const auto path = (std::filesystem::temp_directory_path() / "wbc_pipeline_bg.png").string();
    cv::imwrite(path, cv::Mat(50, 80, CV_8UC3, cv::Scalar{5, 6, 7}));

    wbc::PipelineConfig config = Harness::Config();
    config.backgroundImagePath = QString::fromStdString(path);
    config.compositor.mode = wbc::BackgroundMode::Image;

    harness.segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{0.0}));
    ASSERT_TRUE(harness.controller.Prepare(config)) << harness.controller.LastError().toStdString();
    ASSERT_TRUE(harness.controller.ProcessOneFrame()) << harness.controller.LastError().toStdString();

    // A transparent mask means the frame is entirely the background, so the
    // picture's colour is what reaches the sink.
    const cv::Vec3b pixel = harness.sink.Frames().front().at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(pixel[0], 5, 2);
    EXPECT_NEAR(pixel[1], 6, 2);
    EXPECT_NEAR(pixel[2], 7, 2);
}

TEST(PipelineController, AcceptsABackgroundPictureChosenWhileStopped) {
    Harness harness;
    const auto path = (std::filesystem::temp_directory_path() / "wbc_pipeline_bg2.png").string();
    cv::imwrite(path, cv::Mat(50, 80, CV_8UC3, cv::Scalar{9, 9, 9}));
    EXPECT_TRUE(harness.controller.SetBackgroundImage(QString::fromStdString(path)));
    EXPECT_TRUE(harness.controller.LastError().isEmpty());
}

TEST(PipelineController, AFailingCompositorHaltsThePipelineWithItsMessage) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);

    wbc::PipelineConfig config = Harness::Config();
    // Image mode with no picture loaded: Compose refuses, and the controller
    // has to turn that into a stop with a readable reason rather than let
    // the exception escape into the worker thread.
    config.compositor.mode = wbc::BackgroundMode::Image;
    ASSERT_TRUE(harness.controller.Prepare(config));

    EXPECT_FALSE(harness.controller.ProcessOneFrame());
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(0).toString().isEmpty());
}

TEST(PipelineController, APictureNamedInTheConfigThatCannotBeReadStopsBeforeAnyDeviceOpens) {
    Harness harness;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);

    // The opposite of AStalePicturePathDoesNotBlockStartingInBlurMode: here the
    // picture is what the user asked to composite against, so it is fatal.
    wbc::PipelineConfig config = Harness::Config();
    config.compositor.mode = wbc::BackgroundMode::Image;
    config.backgroundImagePath = "/nonexistent/holiday-photo.png";

    EXPECT_FALSE(harness.controller.Prepare(config));

    // Named, and named before the camera light comes on. Swallowing this at
    // Prepare meant the user met it one frame later as the compositor's
    // "image mode requires a background", with both devices already open.
    ASSERT_EQ(spy.count(), 1);
    EXPECT_NE(spy.at(0).at(0).toString().indexOf("holiday-photo.png"), -1);
    EXPECT_EQ(harness.camera.OpenedPath(), "");
    EXPECT_EQ(harness.sink.OpenCount(), 0);
}

TEST(PipelineController, AStalePicturePathDoesNotBlockStartingInBlurMode) {
    Harness harness;
    wbc::PipelineConfig config = Harness::Config();
    config.compositor.mode = wbc::BackgroundMode::Blur;
    // Left over from a session where the user did choose a picture, and the
    // file has since gone. It has nothing to do with blurring, and the window
    // keeps the path in settings indefinitely, so refusing to start here would
    // strand the user in every mode until they found and cleared it.
    config.backgroundImagePath = "/nonexistent/gone.png";

    EXPECT_TRUE(harness.controller.Prepare(config)) << harness.controller.LastError().toStdString();
    EXPECT_TRUE(harness.controller.ProcessOneFrame());
}

TEST(PipelineController, AnExceptionFromTheSinkStopsThePipelineInsteadOfAbortingTheProcess) {
    wbc::FakeClock clock;
    wbc::FakeCameraSource camera;
    wbc::FakeSegmenter segmenter;
    wbc::MaskRefiner refiner{clock};
    wbc::Compositor compositor;
    wbc::BackgroundImage background;
    wbc::BackgroundVideo video{clock, [] { return std::make_unique<wbc::FakeVideoDecoder>(); }};
    ThrowingSink sink;
    wbc::PipelineController controller{camera, segmenter, refiner, compositor,
                                      background, video, sink, clock};
    camera.PushFrame(cv::Mat(48, 64, CV_8UC3, cv::Scalar{10, 20, 30}));
    segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{1.0}));

    QSignalSpy spy(&controller, &wbc::IPipelineController::failed);
    ASSERT_TRUE(controller.Prepare(Harness::Config()));

    // The sink write is one of the three stages that used to sit outside any
    // try. Escaping here would reach a std::thread entry and terminate.
    EXPECT_FALSE(controller.ProcessOneFrame());
    ASSERT_EQ(spy.count(), 1);
    EXPECT_NE(spy.at(0).at(0).toString().indexOf("told to fail"), -1);
}

TEST(PipelineController, TheWorkerStopsItselfWhenAFrameFails) {
    Harness harness;
    harness.camera.SetFailAfter(3);
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);

    ASSERT_TRUE(harness.controller.Start(Harness::Config()));

    // No Stop() call: the loop must leave on its own once the camera gives up.
    for (int i = 0; i < 200 && harness.controller.IsRunning(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_FALSE(harness.controller.IsRunning()) << "the worker was still running after the camera failed";
    harness.controller.Stop();
    EXPECT_GE(spy.count(), 1);
}

TEST(PipelineController, AcceptsANewBackgroundPictureWhileTheWorkerIsRunning) {
    Harness harness;
    const auto first = (std::filesystem::temp_directory_path() / "wbc_live_bg1.png").string();
    const auto second = (std::filesystem::temp_directory_path() / "wbc_live_bg2.png").string();
    cv::imwrite(first, cv::Mat(50, 80, CV_8UC3, cv::Scalar{10, 10, 10}));
    cv::imwrite(second, cv::Mat(50, 80, CV_8UC3, cv::Scalar{200, 200, 200}));

    wbc::PipelineConfig config = Harness::Config();
    config.compositor.mode = wbc::BackgroundMode::Image;
    config.backgroundImagePath = QString::fromStdString(first);
    harness.segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{0.0}));

    ASSERT_TRUE(harness.controller.Start(config));
    for (int i = 0; i < 100 && harness.sink.Frames().empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // The GUI thread swapping the picture underneath a compositing worker is
    // the exact concurrency this design has to survive.
    EXPECT_TRUE(harness.controller.SetBackgroundImage(QString::fromStdString(second)));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    harness.controller.Stop();
    EXPECT_FALSE(harness.sink.Frames().empty());
}

TEST(PipelineController, PrepareAppliesTheMaskSettingsItWasStartedWith) {
    Harness harness;
    wbc::PipelineConfig config = Harness::Config();
    config.mask.threshold = 0.3F;
    config.mask.smoothing = 0.1F;
    config.mask.edgeRadius = 2;

    ASSERT_TRUE(harness.controller.Prepare(config));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const wbc::MaskSettings applied = harness.refiner.Settings();
    EXPECT_FLOAT_EQ(applied.threshold, 0.3F);
    EXPECT_FLOAT_EQ(applied.smoothing, 0.1F);
    EXPECT_EQ(applied.edgeRadius, 2);
}

TEST(PipelineController, AMaskSettingChangedMidRunReachesTheRefinerOnTheNextFrame) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    wbc::MaskSettings changed;
    changed.threshold = 0.6F;
    changed.smoothing = 0.9F;
    changed.edgeRadius = 1;
    harness.controller.UpdateMaskSettings(changed);
    // Still the old values: the worker applies them when it next starts a
    // frame, not when the GUI thread hands them over.
    EXPECT_FLOAT_EQ(harness.refiner.Settings().threshold, 0.0F);

    harness.camera.PushFrame(cv::Mat(48, 64, CV_8UC3, cv::Scalar{10, 20, 30}));
    harness.controller.PreviewConsumed();
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const wbc::MaskSettings applied = harness.refiner.Settings();
    EXPECT_FLOAT_EQ(applied.threshold, 0.6F);
    EXPECT_FLOAT_EQ(applied.smoothing, 0.9F);
    EXPECT_EQ(applied.edgeRadius, 1);
}

// The video background. The harness's video is two flat frames, red then
// blue, and a mask of zero shows nothing but the background, so the colour
// of the composed frame says which video frame it was built on.
namespace {

wbc::PipelineConfig VideoConfig() {
    wbc::PipelineConfig config = Harness::Config();
    config.compositor.mode = wbc::BackgroundMode::Video;
    config.backgroundVideoPath = "/videos/clip.mp4";
    return config;
}

cv::Vec3b CentreOf(const cv::Mat& frame) {
    return frame.at<cv::Vec3b>(frame.rows / 2, frame.cols / 2);
}

}  // namespace

TEST(PipelineController, ComposesOverTheVideoInVideoMode) {
    Harness harness;
    harness.segmenter.SetMask(cv::Mat(12, 16, CV_32FC1, cv::Scalar{0.0}));
    ASSERT_TRUE(harness.controller.Prepare(VideoConfig()));
    ASSERT_TRUE(harness.controller.ProcessOneFrame());

    const std::vector<cv::Mat> written = harness.sink.Frames();
    ASSERT_FALSE(written.empty());
    // The sink converts to I420 and back is not ours to undo here; the fake
    // keeps the BGR it was handed.
    EXPECT_EQ(CentreOf(written.back()), cv::Vec3b(0, 0, 255)) << "not the video's first frame";
}

TEST(PipelineController, RefusesToStartInVideoModeWithAVideoThatWillNotLoad) {
    Harness harness;
    harness.videoOpens = false;
    QSignalSpy spy(&harness.controller, &wbc::IPipelineController::failed);

    EXPECT_FALSE(harness.controller.Prepare(VideoConfig()));
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(harness.controller.LastError().isEmpty());
}

TEST(PipelineController, IgnoresABrokenVideoWhenNotInVideoMode) {
    Harness harness;
    harness.videoOpens = false;
    wbc::PipelineConfig config = VideoConfig();
    config.compositor.mode = wbc::BackgroundMode::Color;

    EXPECT_TRUE(harness.controller.Prepare(config));
}

TEST(PipelineController, PlaysTheVideoOnlyWhileRunningInVideoMode) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Start(VideoConfig()));
    EXPECT_TRUE(harness.video.IsPlaying());

    harness.controller.Stop();
    EXPECT_FALSE(harness.video.IsPlaying());
}

TEST(PipelineController, StartsAndStopsTheVideoAsTheModeChanges) {
    Harness harness;
    wbc::PipelineConfig config = VideoConfig();
    config.compositor.mode = wbc::BackgroundMode::Blur;
    ASSERT_TRUE(harness.controller.Start(config));
    EXPECT_FALSE(harness.video.IsPlaying()) << "decoding a video nobody is shown";

    wbc::CompositorSettings video = config.compositor;
    video.mode = wbc::BackgroundMode::Video;
    harness.controller.UpdateCompositorSettings(video);
    EXPECT_TRUE(harness.video.IsPlaying());

    wbc::CompositorSettings blur = config.compositor;
    blur.mode = wbc::BackgroundMode::Blur;
    harness.controller.UpdateCompositorSettings(blur);
    EXPECT_FALSE(harness.video.IsPlaying());

    harness.controller.Stop();
}

TEST(PipelineController, DoesNotPlayTheVideoForAModeChangeWhileStopped) {
    Harness harness;
    wbc::CompositorSettings video;
    video.mode = wbc::BackgroundMode::Video;
    ASSERT_TRUE(harness.controller.SetBackgroundVideo("/videos/clip.mp4"));

    harness.controller.UpdateCompositorSettings(video);
    EXPECT_FALSE(harness.video.IsPlaying());
}

TEST(PipelineController, PlaysANewlyChosenVideoAtOnceWhenShowingVideo) {
    Harness harness;
    ASSERT_TRUE(harness.controller.Start(VideoConfig()));

    // Loading stops playback; the controller has to start it again.
    ASSERT_TRUE(harness.controller.SetBackgroundVideo("/videos/other.mp4"));
    EXPECT_TRUE(harness.video.IsPlaying());

    harness.controller.Stop();
}

TEST(PipelineController, ReportsAVideoThatWillNotLoad) {
    Harness harness;
    harness.videoOpens = false;
    EXPECT_FALSE(harness.controller.SetBackgroundVideo("/videos/broken.mp4"));
    EXPECT_FALSE(harness.controller.LastError().isEmpty());
}

// The time the window reports is the time spent processing a frame, as the
// spec asks. Waiting for the camera to deliver it is not processing: once the
// pipeline was faster than the camera, counting the wait made the number
// follow the camera -- 33 ms by day, 137 ms in a dim room -- and read as the
// program slowing down.
TEST(PipelineController, ReportsProcessingTimeWithoutTheWaitForTheCamera) {
    using namespace std::chrono_literals;
    Harness harness;
    // A webcam in a dim room: 100 ms for each frame. Processing, on the fake
    // clock, takes no time at all.
    harness.camera.SetOnRead([&] { harness.clock.Advance(100ms); });
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(harness.controller.ProcessOneFrame());
        harness.controller.PreviewConsumed();
    }

    EXPECT_LT(harness.controller.Stats().latencyMs, 1.0) << "the wait for the camera was counted";
}

TEST(PipelineController, TheFrameRateStillShowsThePaceTheCameraSets) {
    using namespace std::chrono_literals;
    Harness harness;
    harness.camera.SetOnRead([&] { harness.clock.Advance(100ms); });
    ASSERT_TRUE(harness.controller.Prepare(Harness::Config()));

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(harness.controller.ProcessOneFrame());
        harness.controller.PreviewConsumed();
    }

    // With processing out of the latency, the frame rate is what tells a
    // user the camera is the limit: ten frames a second here.
    EXPECT_NEAR(harness.controller.Stats().fps, 10.0, 0.5);
}
