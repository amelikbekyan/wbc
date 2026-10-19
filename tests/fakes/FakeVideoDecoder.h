// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <vector>

#include "comp/IVideoDecoder.h"

namespace wbc {

// A video made of the frames a test hands it. Every way reading can go wrong
// is a switch the test can throw.
class FakeVideoDecoder final : public IVideoDecoder {
public:
    explicit FakeVideoDecoder(std::vector<cv::Mat> frames = {}, double fps = 30.0)
        : frames_(std::move(frames)), fps_(fps) {}

    bool Open(const std::string& path) override {
        ++openCount_;
        lastPath_ = path;
        if (!openSucceeds_) {
            lastError_ = "FakeVideoDecoder was told to refuse " + path;
            return false;
        }
        next_ = 0;
        return true;
    }

    bool Read(cv::Mat& frame) override {
        ++readCount_;
        if (failReadsFrom_ >= 0 && readCount_ > failReadsFrom_) {
            lastError_ = "FakeVideoDecoder was told to stop reading";
            return false;
        }
        if (next_ >= frames_.size()) {
            lastError_ = "end of the fake video";
            return false;
        }
        frame = frames_[next_++].clone();
        return true;
    }

    bool Rewind() override {
        ++rewindCount_;
        if (!rewindSucceeds_) {
            return false;
        }
        next_ = 0;
        return true;
    }

    double Fps() const override { return fps_; }
    void Close() override { ++closeCount_; }
    std::string LastError() const override { return lastError_; }

    void SetOpenSucceeds(bool value) { openSucceeds_ = value; }
    void SetRewindSucceeds(bool value) { rewindSucceeds_ = value; }
    // Reads after the first `count` fail, whatever the position.
    void FailReadsAfter(int count) { failReadsFrom_ = count; }

    int OpenCount() const { return openCount_; }
    int ReadCount() const { return readCount_; }
    int RewindCount() const { return rewindCount_; }
    int CloseCount() const { return closeCount_; }
    const std::string& LastPath() const { return lastPath_; }

private:
    std::vector<cv::Mat> frames_;
    double fps_;
    std::size_t next_ = 0;
    bool openSucceeds_ = true;
    bool rewindSucceeds_ = true;
    int failReadsFrom_ = -1;
    int openCount_ = 0;
    int readCount_ = 0;
    int rewindCount_ = 0;
    int closeCount_ = 0;
    std::string lastPath_;
    std::string lastError_;
};

}  // namespace wbc
