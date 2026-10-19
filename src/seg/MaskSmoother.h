// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <optional>

#include <opencv2/core.hpp>

#include "core/IClock.h"

namespace wbc {

// Exponential smoothing of the segmentation mask across frames.
//
// Without it the mask edge jitters from frame to frame and the picture looks
// dirty even when every individual mask is reasonable. The weight is raised to
// the power of the actual frame interval divided by the nominal one, so that
// the amount of smoothing per second stays the same when the frame rate drops;
// a fixed weight would make the mask visibly lag the person's movement.
class MaskSmoother {
public:
    explicit MaskSmoother(const IClock& clock, float alpha = 0.7F, double nominalFrameSeconds = 1.0 / 30.0);

    cv::Mat Smooth(const cv::Mat& maskF32);
    void SetAlpha(float alpha);
    float Alpha() const;
    void Reset();

private:
    const IClock& clock_;
    float alpha_;
    double nominalFrameSeconds_;
    cv::Mat previous_;
    std::optional<std::chrono::steady_clock::time_point> previousAt_;
};

}  // namespace wbc
