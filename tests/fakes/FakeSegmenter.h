// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>

#include "seg/ISegmenter.h"

namespace wbc {

class FakeSegmenter final : public ISegmenter {
public:
    FakeSegmenter() : mask_(cv::Mat(16, 16, CV_32FC1, cv::Scalar{1.0})), inputSize_(16, 16) {}

    cv::Mat Segment(const cv::Mat& frameBgr) override {
        ++callCount_;
        lastFrameSize_ = frameBgr.size();
        if (throwOnNextCall_) {
            throwOnNextCall_ = false;
            throw std::runtime_error("FakeSegmenter was told to fail");
        }
        if (frameBgr.empty()) {
            throw std::invalid_argument("FakeSegmenter: empty frame");
        }
        return mask_.clone();
    }

    cv::Size InputSize() const override { return inputSize_; }

    void SetMask(cv::Mat mask) {
        mask_ = std::move(mask);
        inputSize_ = mask_.size();
    }
    void SetInputSize(cv::Size size) { inputSize_ = size; }
    void SetThrowOnNextCall(bool value) { throwOnNextCall_ = value; }
    int CallCount() const { return callCount_; }
    cv::Size LastFrameSize() const { return lastFrameSize_; }

private:
    cv::Mat mask_;
    cv::Size inputSize_;
    bool throwOnNextCall_ = false;
    int callCount_ = 0;
    cv::Size lastFrameSize_;
};

}  // namespace wbc
