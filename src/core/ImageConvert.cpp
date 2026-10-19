// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/ImageConvert.h"

#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace wbc {

QImage ToQImage(const cv::Mat& bgr) {
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        return {};
    }

    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    // The stride is passed explicitly: Qt pads scan lines, and handing it a
    // matrix with a different stride shears the image. The copy is essential:
    // view wraps rgb.data, but rgb is a local destroyed at return, which frees
    // the buffer. Without the copy, the returned image holds a dangling pointer.
    const QImage view(rgb.data, rgb.cols, rgb.rows, static_cast<qsizetype>(rgb.step),
                      QImage::Format_RGB888);
    return view.copy();
}

cv::Mat ShrinkFrame(const cv::Mat& bgr, cv::Size target) {
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        throw std::invalid_argument("ShrinkFrame: expected a non-empty 8 bit three channel frame");
    }
    if (target.width <= 0 || target.height <= 0 || target.width > bgr.cols ||
        target.height > bgr.rows) {
        throw std::invalid_argument("ShrinkFrame: target must be non-empty and no larger than the frame");
    }

    cv::Mat current = bgr;
    cv::Mat halved;
    while (current.cols >= 2 * target.width && current.rows >= 2 * target.height) {
        cv::resize(current, halved, cv::Size(current.cols / 2, current.rows / 2), 0, 0, cv::INTER_AREA);
        // A fresh matrix each time: resizing into the buffer being read from
        // would make OpenCV reallocate it under the source.
        current = halved;
        halved = cv::Mat{};
    }
    cv::Mat shrunk = current;
    if (current.size() != target) {
        cv::resize(current, shrunk, target, 0, 0, cv::INTER_AREA);
    }
    return shrunk;
}

}  // namespace wbc
