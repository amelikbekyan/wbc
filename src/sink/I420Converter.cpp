// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sink/I420Converter.h"

#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace wbc {

cv::Mat EnsureContinuous(cv::Mat mat) {
    if (mat.isContinuous()) {
        return mat;
    }
    return mat.clone();
}

cv::Mat ToI420(const cv::Mat& bgr) {
    if (bgr.empty()) {
        throw std::invalid_argument("ToI420: empty frame");
    }
    if (bgr.type() != CV_8UC3) {
        throw std::invalid_argument("ToI420: expected an 8 bit three channel BGR frame");
    }
    if (bgr.cols % 2 != 0 || bgr.rows % 2 != 0) {
        throw std::invalid_argument("ToI420: width and height must both be even");
    }

    cv::Mat i420;
    cv::cvtColor(bgr, i420, cv::COLOR_BGR2YUV_I420);
    return EnsureContinuous(i420);
}

}  // namespace wbc
