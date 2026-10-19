// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

namespace wbc {

// Produces a foreground mask for a frame. The mask comes back at the model's
// own resolution, not the frame's: upscaling belongs to the refiner, which has
// the frame available as a guide.
class ISegmenter {
public:
    virtual ~ISegmenter() = default;

    // Returns CV_32FC1 with values in [0, 1]. Throws std::invalid_argument on
    // an empty or wrongly typed frame.
    virtual cv::Mat Segment(const cv::Mat& frameBgr) = 0;

    virtual cv::Size InputSize() const = 0;
};

}  // namespace wbc
