// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

#include "core/IClock.h"
#include "core/Types.h"
#include "seg/EdgeRefiner.h"
#include "seg/MaskSmoother.h"

namespace wbc {

// Smoothing first, at the model's own resolution where it is cheap, then edge
// refinement at the frame resolution. Doing it the other way round would spend
// the expensive filter on a mask that is about to be blended away.
class MaskRefiner {
public:
    explicit MaskRefiner(const IClock& clock, float alpha = 0.7F, int radius = 8, double eps = 1e-4);

    // Delivers the mask at the guide's size; see EdgeRefiner::Refine.
    cv::Mat Refine(const cv::Mat& rawMaskF32, const cv::Mat& guideBgr, cv::Size frameSize);
    void Reset();
    MaskSmoother& Smoother();

    // Retunes the chain in place. Values outside their range are clamped
    // rather than refused: they arrive from a settings file a user can edit.
    void Apply(const MaskSettings& settings);
    MaskSettings Settings() const;

private:
    MaskSmoother smoother_;
    EdgeRefiner edges_;
    float threshold_ = 0.0F;
};

}  // namespace wbc
