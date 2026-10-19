// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <functional>
#include <string>

#include "camera/ICameraSource.h"

namespace wbc {

class FakeCameraSource final : public ICameraSource {
public:
    bool Open(const std::string& path, const FormatRequest& request) override {
        openedPath_ = path;
        requested_ = request;
        if (!openSucceeds_) {
            lastError_ = "FakeCameraSource was told to refuse " + path;
            return false;
        }
        open_ = true;
        lastError_.clear();
        return true;
    }

    bool IsOpen() const override { return open_; }

    bool Read(cv::Mat& frameBgr) override {
        // Called first, as a real camera's wait for its next frame comes
        // before the frame: a test uses it to move a fake clock on by the
        // time a real camera would have taken.
        if (onRead_) {
            onRead_();
        }
        if (!open_) {
            lastError_ = "FakeCameraSource is not open";
            return false;
        }
        if (failAfter_ >= 0 && readCount_ >= failAfter_) {
            lastError_ = "FakeCameraSource was told to fail after " + std::to_string(failAfter_) + " frames";
            return false;
        }
        if (frames_.size() > 1) {
            last_ = frames_.front();
            frames_.pop_front();
        } else if (frames_.size() == 1) {
            last_ = frames_.front();
        }
        if (last_.empty()) {
            lastError_ = "FakeCameraSource has no frames to give";
            return false;
        }
        ++readCount_;
        frameBgr = last_.clone();
        return true;
    }

    void Close() override {
        open_ = false;
        closed_ = true;
    }

    FormatOption ActualFormat() const override { return actual_; }

    void SetOnRead(std::function<void()> onRead) { onRead_ = std::move(onRead); }
    std::string LastError() const override { return lastError_; }

    void PushFrame(cv::Mat frame) {
        actual_.width = frame.cols;
        actual_.height = frame.rows;
        frames_.push_back(std::move(frame));
    }
    void SetFailAfter(int frames) { failAfter_ = frames; }
    void SetOpenSucceeds(bool value) { openSucceeds_ = value; }
    int ReadCount() const { return readCount_; }
    bool WasClosed() const { return closed_; }
    const std::string& OpenedPath() const { return openedPath_; }

private:
    std::deque<cv::Mat> frames_;
    cv::Mat last_;
    FormatOption actual_{};
    FormatRequest requested_{};
    std::string openedPath_;
    std::string lastError_;
    std::function<void()> onRead_;
    bool open_ = false;
    bool closed_ = false;
    bool openSucceeds_ = true;
    int failAfter_ = -1;
    int readCount_ = 0;
};

}  // namespace wbc
