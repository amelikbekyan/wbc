// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/PipelineController.h"

#include <chrono>

#include "core/ImageConvert.h"
#include "seg/EdgeRefiner.h"

namespace wbc {

PipelineController::PipelineController(ICameraSource& camera,
                                       ISegmenter& segmenter,
                                       MaskRefiner& refiner,
                                       Compositor& compositor,
                                       BackgroundImage& background,
                                       BackgroundVideo& video,
                                       IFrameSink& sink,
                                       const IClock& clock,
                                       QObject* parent)
    : IPipelineController(parent),
      camera_(camera),
      segmenter_(segmenter),
      refiner_(refiner),
      compositor_(compositor),
      background_(background),
      video_(video),
      sink_(sink),
      clock_(clock),
      stats_(clock) {}

PipelineController::~PipelineController() {
    Stop();
}

void PipelineController::Fail(const QString& message) {
    {
        const std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = message;
    }
    emit failed(message);
}

QString PipelineController::LastError() const {
    const std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

CompositorSettings PipelineController::CurrentSettings() const {
    const std::lock_guard<std::mutex> lock(settingsMutex_);
    return settings_;
}

MaskSettings PipelineController::CurrentMaskSettings() const {
    const std::lock_guard<std::mutex> lock(settingsMutex_);
    return maskSettings_;
}

void PipelineController::UpdateCompositorSettings(const CompositorSettings& settings) {
    BackgroundMode previous;
    {
        const std::lock_guard<std::mutex> lock(settingsMutex_);
        previous = settings_.mode;
        settings_ = settings;
    }
    // The video decodes only while it is being shown. Switching to it mid-run
    // starts it; switching away stops it, so a video nobody is watching does
    // not keep a core busy.
    if (previous != BackgroundMode::Video && settings.mode == BackgroundMode::Video) {
        if (IsRunning()) {
            video_.Play();
        }
    } else if (previous == BackgroundMode::Video && settings.mode != BackgroundMode::Video) {
        video_.Pause();
    }
}

void PipelineController::UpdateMaskSettings(const MaskSettings& settings) {
    const std::lock_guard<std::mutex> lock(settingsMutex_);
    maskSettings_ = settings;
}

bool PipelineController::SetBackgroundImage(const QString& path) {
    if (!background_.Load(path.toStdString())) {
        const QString message = QString::fromStdString(background_.LastError());
        const std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = message;
        return false;
    }
    return true;
}

bool PipelineController::SetBackgroundVideo(const QString& path) {
    // Load stops playback whether or not it succeeds, so it is restarted here
    // if the pipeline is showing the video: a new video starts at once, and
    // a refused one leaves the previous one playing.
    const bool loaded = video_.Load(path.toStdString());
    if (IsRunning() && CurrentSettings().mode == BackgroundMode::Video) {
        video_.Play();
    }
    if (!loaded) {
        const QString message = QString::fromStdString(video_.LastError());
        const std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = message;
        return false;
    }
    return true;
}

bool PipelineController::Prepare(const PipelineConfig& config) {
    UpdateCompositorSettings(config.compositor);
    UpdateMaskSettings(config.mask);

    if (!config.backgroundImagePath.isEmpty()) {
        const bool loaded = SetBackgroundImage(config.backgroundImagePath);
        if (!loaded && config.compositor.mode == BackgroundMode::Image) {
            // Spec section 8, row 10: a picture that will not load is named to
            // the user and the mode is left alone. Reported here, before a
            // single device is opened, because swallowing it meant the user
            // met the failure one frame later as the compositor's internal
            // complaint instead -- with the camera light already on.
            Fail(QString::fromStdString(background_.LastError()));
            return false;
        }
        // In any other mode a path left over from an earlier session is not
        // this run's problem. It is loaded anyway when it can be, so that
        // switching to picture mode later is instant, and its failure is
        // silent: refusing to blur because of a picture nobody asked for
        // would strand the user with a setting they cannot see or clear.
    }

    // The same rule for the video, for the same reasons.
    if (!config.backgroundVideoPath.isEmpty()) {
        const bool loaded = SetBackgroundVideo(config.backgroundVideoPath);
        if (!loaded && config.compositor.mode == BackgroundMode::Video) {
            Fail(QString::fromStdString(video_.LastError()));
            return false;
        }
    }

    if (!camera_.Open(config.cameraPath, config.request)) {
        Fail(QString::fromStdString(camera_.LastError()));
        return false;
    }

    const FormatOption actual = camera_.ActualFormat();
    // A driver that reports no discrete frame intervals leaves this at zero,
    // which is not a rate anything can be opened at. This fallback is taken on
    // every such camera, not only in tests.
    const double negotiatedFps = actual.fps > 0.0 ? actual.fps : 30.0;

    // The virtual camera is opened at the geometry the camera actually gave us,
    // not the one that was asked for.
    if (!sink_.Open(config.sinkPath, actual.width, actual.height, negotiatedFps)) {
        Fail(QString::fromStdString(sink_.LastError()));
        camera_.Close();
        return false;
    }

    // Spec section 8, row 7: the user is told what the camera actually gave,
    // which is the only way to notice a camera that quietly fell back to
    // 640x480 when 720p was asked for.
    emit formatNegotiated(actual.width, actual.height, negotiatedFps);

    refiner_.Reset();
    stats_.Reset();
    // A preview left in flight by a previous run would otherwise suppress every
    // preview of this one.
    previewInFlight_.store(false, std::memory_order_release);
    return true;
}

bool PipelineController::ProcessOneFrame() {
    // Run() is a std::thread entry, so anything that escapes this function
    // calls std::terminate and takes the whole application with it. Every
    // stage is inside this try for that reason, not only the two that are
    // known to throw: a cv::Exception from any OpenCV call in the loop -- the
    // camera read, the composite, the I420 conversion inside the sink's write,
    // the conversion for the preview -- has to become a stop with a message,
    // never an abort.
    try {
        cv::Mat frame;
        if (!camera_.Read(frame)) {
            Fail(QString::fromStdString(camera_.LastError()));
            return false;
        }

        // Timed from here, once the frame is in hand, not from before the
        // read: the read waits for the camera, and a camera in a dim room
        // takes 100 ms or more to deliver. Counting the wait made the
        // reported time follow the camera rather than the processing, and a
        // faster pipeline looked slower. How often frames arrive is the
        // frame rate's to say.
        const auto started = clock_.Now();

        // Applied here rather than when the setting arrives: the refiner is
        // the worker's own, and retuning it from the GUI thread mid-frame
        // would race the filter reading the value it is being given.
        refiner_.Apply(CurrentMaskSettings());
        // Shrunk once, to the model's resolution, for every stage that works
        // below the frame's: the model, the edge filter and a large blur.
        // Each used to shrink the full frame itself, to a size of its own,
        // and the three took 8 ms of every frame at 720p. The mask stays at
        // this size until the compositor scales it up.
        const cv::Mat small =
            ShrinkFrame(frame, EdgeRefiner::WorkingSize(segmenter_.InputSize(), frame.size()));
        const cv::Mat mask = refiner_.Refine(segmenter_.Segment(small), small, frame.size());

        const CompositorSettings settings = CurrentSettings();
        const cv::Mat background = settings.mode == BackgroundMode::Video
                                       ? video_.For(frame.size())
                                       : background_.For(frame.size());
        const cv::Mat composed = compositor_.Compose(frame, mask, settings, background, small);

        const WriteResult result = sink_.Write(composed);
        if (result == WriteResult::Fatal) {
            Fail(QString::fromStdString(sink_.LastError()));
            return false;
        }

        const double latencyMs =
            std::chrono::duration<double, std::milli>(clock_.Now() - started).count();
        if (result == WriteResult::Ok) {
            stats_.RecordDelivered(latencyMs);
        } else {
            stats_.RecordDropped();
        }

        // ToQImage copies; see ImageConvert.h for why -- the QImage it returns
        // would otherwise wrap the buffer of a cv::Mat local to that function.
        //
        // The worker must never block on the GUI thread, and Qt does not
        // coalesce queued signals, so a GUI thread that falls behind would
        // accumulate one deep-copied image per pending frame (2.7 MB at 720p).
        // A preview is therefore dropped rather than queued until the window
        // says, through PreviewConsumed(), that it has taken the last one.
        if (!previewInFlight_.exchange(true)) {
            emit previewReady(ToQImage(composed));
        }

        const PipelineStats::Snapshot snapshot = stats_.Current();
        emit statsUpdated(snapshot.fps, snapshot.latencyMs,
                          static_cast<qulonglong>(snapshot.dropped));
        return true;
    } catch (const std::exception& error) {
        Fail(QString::fromUtf8(error.what()));
        return false;
    }
}

void PipelineController::PreviewConsumed() {
    previewInFlight_.store(false, std::memory_order_release);
}

void PipelineController::Run() {
    while (running_.load(std::memory_order_acquire)) {
        if (!ProcessOneFrame()) {
            running_.store(false, std::memory_order_release);
            break;
        }
    }
}

bool PipelineController::Start(const PipelineConfig& config) {
    Stop();

    if (!Prepare(config)) {
        return false;
    }

    running_.store(true, std::memory_order_release);
    if (CurrentSettings().mode == BackgroundMode::Video) {
        video_.Play();
    }
    worker_ = std::thread(&PipelineController::Run, this);
    return true;
}

void PipelineController::Stop() {
    running_.store(false, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
    video_.Pause();
    // Closing after the join, never before: the worker is still reading from
    // these right up to the moment it leaves the loop.
    sink_.Close();
    camera_.Close();
}

bool PipelineController::IsRunning() const {
    return running_.load(std::memory_order_acquire);
}

PipelineStats::Snapshot PipelineController::Stats() const {
    return stats_.Current();
}

}  // namespace wbc
