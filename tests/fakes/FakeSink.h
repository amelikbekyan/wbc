// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "sink/IFrameSink.h"

namespace wbc {

// Task 16's test polls Frames() from the test thread while the pipeline's
// worker thread calls Write(), so every access to this fake's state goes
// through mutex_. Frames() hands back a copy rather than a reference: a
// reference into a vector another thread may reallocate at any moment would
// still be a data race even with the lock held only during the call.
class FakeSink final : public IFrameSink {
public:
    bool Open(const std::string& path, int width, int height, double fps) override {
        std::lock_guard<std::mutex> lock(mutex_);
        path_ = path;
        size_ = {width, height};
        fps_ = fps;
        ++openCount_;
        open_ = openSucceeds_;
        if (!open_) {
            lastError_ = "FakeSink was told to refuse " + path;
        }
        return open_;
    }

    bool IsOpen() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return open_;
    }

    WriteResult Write(const cv::Mat& frameBgr) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_) {
            lastError_ = "FakeSink is not open";
            return WriteResult::Fatal;
        }
        if (result_ == WriteResult::Ok) {
            frames_.push_back(frameBgr.clone());
        }
        return result_;
    }

    void Close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        open_ = false;
        closed_ = true;
    }

    std::string LastError() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastError_;
    }

    void SetResult(WriteResult result) {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = result;
    }

    void SetOpenSucceeds(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        openSucceeds_ = value;
    }

    std::vector<cv::Mat> Frames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

    int OpenCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return openCount_;
    }

    bool WasClosed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    cv::Size ConfiguredSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    double ConfiguredFps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fps_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<cv::Mat> frames_;
    std::string path_;
    std::string lastError_;
    cv::Size size_{0, 0};
    double fps_ = 0.0;
    WriteResult result_ = WriteResult::Ok;
    bool open_ = false;
    bool openSucceeds_ = true;
    bool closed_ = false;
    int openCount_ = 0;
};

}  // namespace wbc
