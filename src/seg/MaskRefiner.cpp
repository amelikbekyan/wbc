// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/MaskRefiner.h"

#include <algorithm>

namespace wbc {

MaskRefiner::MaskRefiner(const IClock& clock, float alpha, int radius, double eps)
    : smoother_(clock, alpha), edges_(radius, eps) {}

cv::Mat MaskRefiner::Refine(const cv::Mat& rawMaskF32, const cv::Mat& guideBgr, cv::Size frameSize) {
    const cv::Mat smoothed = smoother_.Smooth(rawMaskF32);
    const cv::Mat refined = edges_.Refine(smoothed, guideBgr, frameSize);
    if (threshold_ <= 0.0F) {
        return refined;
    }

    // Everything at or below the threshold becomes background, and what
    // survives is stretched back over the full range so that the person does
    // not turn translucent as the threshold rises. A hard cut would do the
    // first half and lose the soft boundary the guided filter just earned.
    cv::Mat stretched = (refined - threshold_) * (1.0F / (1.0F - threshold_));
    cv::Mat clamped;
    cv::min(cv::max(stretched, 0.0), 1.0, clamped);
    return clamped;
}

void MaskRefiner::Apply(const MaskSettings& settings) {
    threshold_ = std::clamp(settings.threshold, 0.0F, kMaxMaskThreshold);
    smoother_.SetAlpha(settings.smoothing);
    edges_.SetRadius(std::min(settings.edgeRadius, kMaxMaskEdgeRadius));
}

MaskSettings MaskRefiner::Settings() const {
    MaskSettings settings;
    settings.threshold = threshold_;
    settings.smoothing = smoother_.Alpha();
    settings.edgeRadius = edges_.Radius();
    return settings;
}

void MaskRefiner::Reset() {
    smoother_.Reset();
}

MaskSmoother& MaskRefiner::Smoother() {
    return smoother_;
}

}  // namespace wbc
