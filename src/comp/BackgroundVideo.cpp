// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comp/BackgroundVideo.h"

#include <QCoreApplication>
#include <QString>

#include <utility>

#include "comp/BackgroundImage.h"

namespace wbc {
namespace {

// What a container that does not state its frame rate is played at. Thirty is
// what almost every such file was recorded at.
constexpr double kFallbackFps = 30.0;

}  // namespace

BackgroundVideo::BackgroundVideo(const IClock& clock, DecoderFactory makeDecoder)
    : clock_(clock), makeDecoder_(std::move(makeDecoder)) {}

BackgroundVideo::~BackgroundVideo() {
    Pause();
}

bool BackgroundVideo::Load(const std::string& path) {
    Pause();

    // A fresh decoder, so that a file that fails to open leaves the one that
    // is playing untouched.
    std::unique_ptr<IVideoDecoder> candidate = makeDecoder_();
    cv::Mat first;
    if (!candidate->Open(path) || !candidate->Read(first)) {
        const std::lock_guard<std::mutex> lock(frameMutex_);
        lastError_ = candidate->LastError();
        return false;
    }

    decoder_ = std::move(candidate);
    fps_ = decoder_->Fps() > 0.0 ? decoder_->Fps() : kFallbackFps;
    {
        const std::lock_guard<std::mutex> lock(frameMutex_);
        lastError_.clear();
    }
    // The previous video's fitted frame is left in place on purpose: its
    // size is the size the pipeline wants, so the new first frame is fitted
    // to it here, not on the pipeline's thread.
    Publish(first);
    return true;
}

bool BackgroundVideo::IsLoaded() const {
    const std::lock_guard<std::mutex> lock(frameMutex_);
    return !latestRaw_.empty();
}

std::string BackgroundVideo::LastError() const {
    const std::lock_guard<std::mutex> lock(frameMutex_);
    return lastError_;
}

void BackgroundVideo::Clear() {
    Pause();
    decoder_.reset();
    const std::lock_guard<std::mutex> lock(frameMutex_);
    latestRaw_.release();
    latestFitted_.release();
}

void BackgroundVideo::Play() {
    if (playing_.load() || decoder_ == nullptr) {
        return;
    }
    // A thread that stopped on its own, because the video stopped reading,
    // has finished but not been joined; starting another over it would
    // terminate the program.
    if (thread_.joinable()) {
        thread_.join();
    }
    // The first frame is already on screen, so the next is due one frame
    // period from now rather than immediately.
    nextDue_ = clock_.Now() + FramePeriod();
    playing_.store(true);
    thread_ = std::thread(&BackgroundVideo::Run, this);
}

void BackgroundVideo::Pause() {
    {
        const std::lock_guard<std::mutex> lock(threadMutex_);
        playing_.store(false);
    }
    wake_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool BackgroundVideo::IsPlaying() const {
    return playing_.load();
}

cv::Mat BackgroundVideo::For(cv::Size target) {
    const std::lock_guard<std::mutex> lock(frameMutex_);
    if (latestRaw_.empty()) {
        return {};
    }
    // Fitted here only on the first request and when the camera's geometry
    // changes; from then on every frame arrives already fitted, because
    // Publish fits to the size last asked for.
    if (latestFitted_.size() != target) {
        latestFitted_ = CoverCrop(latestRaw_, target);
    }
    // A copy the caller owns outright, rather than a second reference to the
    // published buffer. Sharing would be safe -- cv::Mat counts references --
    // but the count is kept inside OpenCV, where ThreadSanitizer cannot see
    // it, so every frame the decoding thread retired would be reported as a
    // race on the pipeline's thread. A copy costs about 0.3 ms at 720p and
    // keeps the sanitizer's reports about this project's code meaningful.
    return latestFitted_.clone();
}

bool BackgroundVideo::Step() {
    if (decoder_ == nullptr) {
        return false;
    }
    const auto now = clock_.Now();
    if (now < nextDue_) {
        return true;
    }

    cv::Mat frame;
    if (!decoder_->Read(frame)) {
        // The end of the file, most likely: start again from the top. A
        // decoder that cannot rewind or read again is one that has stopped
        // working, which is reported rather than retried forever.
        if (!decoder_->Rewind() || !decoder_->Read(frame)) {
            const std::lock_guard<std::mutex> lock(frameMutex_);
            lastError_ = decoder_->LastError();
            return false;
        }
    }
    Publish(frame);

    nextDue_ += FramePeriod();
    // Behind by more than a frame: the machine is not keeping up. Rather than
    // decode a burst of frames nobody will see to catch up, carry on from
    // now, which plays the video slower instead of choppier.
    if (nextDue_ < now) {
        nextDue_ = now + FramePeriod();
    }
    return true;
}

void BackgroundVideo::Run() {
    while (playing_.load()) {
        if (!Step()) {
            playing_.store(false);
            return;
        }
        // Sleep until the next frame is due, or until Pause() says stop.
        // The deadline comes from the injected clock, the wait from the real
        // one; in the application they are the same clock.
        const auto remaining = nextDue_ - clock_.Now();
        std::unique_lock<std::mutex> lock(threadMutex_);
        wake_.wait_for(lock, remaining, [this] { return !playing_.load(); });
    }
}

void BackgroundVideo::Publish(const cv::Mat& frame) {
    // Fitted outside the lock, to whatever size the pipeline last asked for,
    // so that the pipeline never waits on a resize. A new matrix every time:
    // the pipeline may still be holding the previous one.
    cv::Size target;
    {
        const std::lock_guard<std::mutex> lock(frameMutex_);
        target = latestFitted_.size();
    }
    cv::Mat fitted;
    if (target.width > 0 && target.height > 0) {
        fitted = CoverCrop(frame, target);
    }

    const std::lock_guard<std::mutex> lock(frameMutex_);
    latestRaw_ = frame;
    latestFitted_ = fitted;
}

std::chrono::steady_clock::duration BackgroundVideo::FramePeriod() const {
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / fps_));
}

}  // namespace wbc
