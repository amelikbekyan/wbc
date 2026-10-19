// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "comp/IVideoDecoder.h"
#include "core/IClock.h"

namespace wbc {

// Plays the user's chosen background video, in a loop, on a thread of its own.
//
// Decoding is kept off the frame pipeline's thread because its cost depends on
// the file, not on the camera: a 720p clip costs well under a millisecond a
// frame, but a 4K one from a phone costs more than the pipeline has to spare.
// The pipeline asks for the current frame with For() and gets the one already
// decoded and fitted, which costs it nothing.
//
// The video plays at its own frame rate, not the camera's. If the machine
// cannot keep up it plays slower rather than skipping frames to catch up:
// for a background, smooth and slightly slow is better than jerky and exact.
// There is no sound -- a camera carries none.
class BackgroundVideo {
public:
    using DecoderFactory = std::function<std::unique_ptr<IVideoDecoder>()>;

    BackgroundVideo(const IClock& clock, DecoderFactory makeDecoder);
    ~BackgroundVideo();

    BackgroundVideo(const BackgroundVideo&) = delete;
    BackgroundVideo& operator=(const BackgroundVideo&) = delete;

    // Stops playback, opens `path` and reads its first frame, so that a file
    // that opens but holds nothing playable is refused here rather than later.
    // On failure the previous video, if any, stays loaded. Either way playback
    // is left stopped; Play() resumes it.
    bool Load(const std::string& path);
    bool IsLoaded() const;
    std::string LastError() const;
    // Stops playback and forgets the video.
    void Clear();

    // Starts the decoding thread, from wherever playback had reached. Does
    // nothing when nothing is loaded or it is already playing.
    void Play();
    // Stops the decoding thread and waits for it. The last frame stays on
    // offer, so a paused video shows a still.
    void Pause();
    bool IsPlaying() const;

    // The current frame, fitted to `target` by cropping from the centre.
    // Empty when nothing is loaded. The returned matrix is the caller's own
    // copy, shared with nothing.
    cv::Mat For(cv::Size target);

    // One unit of the decoding thread's work: if the next frame is due at the
    // clock's current time, decode it and publish it. Public so that a test
    // can drive playback frame by frame with a clock it controls. Returns
    // false once the video can no longer be read, in which case the last good
    // frame stays on offer and LastError() says why.
    bool Step();

private:
    void Run();
    void Publish(const cv::Mat& frame);
    std::chrono::steady_clock::duration FramePeriod() const;

    const IClock& clock_;
    DecoderFactory makeDecoder_;

    // Touched by Load, Clear, Play and Step only. Load and Clear pause first,
    // and Play starts the thread only after setting them, so the decoding
    // thread is the one user of these while it runs.
    std::unique_ptr<IVideoDecoder> decoder_;
    double fps_ = 0.0;
    std::chrono::steady_clock::time_point nextDue_{};

    // Shared between the decoding thread and the pipeline's.
    mutable std::mutex frameMutex_;
    cv::Mat latestRaw_;
    cv::Mat latestFitted_;
    std::string lastError_;

    // The thread and how it is told to stop.
    std::mutex threadMutex_;
    std::condition_variable wake_;
    std::thread thread_;
    std::atomic<bool> playing_{false};
};

}  // namespace wbc
