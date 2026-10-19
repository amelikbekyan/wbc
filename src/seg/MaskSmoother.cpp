// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/MaskSmoother.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wbc {

MaskSmoother::MaskSmoother(const IClock& clock, float alpha, double nominalFrameSeconds)
    : clock_(clock), alpha_(std::clamp(alpha, 0.0F, 1.0F)), nominalFrameSeconds_(nominalFrameSeconds) {}

void MaskSmoother::SetAlpha(float alpha) {
    alpha_ = std::clamp(alpha, 0.0F, 1.0F);
}

float MaskSmoother::Alpha() const {
    return alpha_;
}

void MaskSmoother::Reset() {
    previous_.release();
    previousAt_.reset();
}

cv::Mat MaskSmoother::Smooth(const cv::Mat& maskF32) {
    if (maskF32.empty() || maskF32.type() != CV_32FC1) {
        throw std::invalid_argument("Smooth: mask must be a non-empty single channel float image");
    }

    const auto now = clock_.Now();

    // A first frame, a resized model output, or a clock that did not move all
    // mean there is nothing meaningful to blend with.
    const bool canBlend = previousAt_.has_value() && !previous_.empty() &&
                          previous_.size() == maskF32.size();
    const double dt = canBlend ? std::chrono::duration<double>(now - *previousAt_).count() : 0.0;

    cv::Mat result;
    if (!canBlend || dt <= 0.0 || alpha_ <= 0.0F) {
        result = maskF32.clone();
    } else {
        const double weight = std::pow(static_cast<double>(alpha_), dt / nominalFrameSeconds_);
        cv::addWeighted(previous_, weight, maskF32, 1.0 - weight, 0.0, result);
    }

    // Two clones per call, both deliberate. `result` is a fresh matrix rather
    // than a view of maskF32, and `previous_` is a fresh matrix rather than a
    // second reference to `result`. cv::Mat assignment shares the buffer, so
    // dropping either clone would leave the history and the returned mask
    // aliasing each other: whoever writes into the returned mask would be
    // rewriting the history this smoother blends against on the next frame.
    // The cost is one copy of a small mask; the saving is not worth the bug.
    previous_ = result.clone();
    previousAt_ = now;
    return result;
}  // GCOVR_EXCL_LINE — gcc's exception-unwinding epilogue for the local
   // cv::Mat `result`, reached only if something throws past it. Nothing
   // in this function can throw after `result` is constructed.

}  // namespace wbc
