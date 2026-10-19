// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/EdgeRefiner.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <stdexcept>

#if WBC_HAVE_XIMGPROC
#include <opencv2/ximgproc/edge_filter.hpp>
#endif

namespace wbc {

EdgeRefiner::EdgeRefiner(int radius, double eps) : radius_(std::max(radius, 0)), eps_(eps) {}

void EdgeRefiner::SetRadius(int radius) {
    radius_ = std::max(radius, 0);
}

int EdgeRefiner::Radius() const {
    return radius_;
}

namespace {

// Below this the shrunken image is too small for the filter's window to mean
// anything, and the saving would be a fraction of a millisecond anyway.
constexpr int kMinWorkingSide = 32;

}  // namespace

cv::Size EdgeRefiner::WorkingSize(cv::Size maskSize, cv::Size outSize) {
    // The larger of the two ratios, so that neither axis drops below the
    // mask's own resolution.
    const double scale =
        std::max(static_cast<double>(maskSize.width) / outSize.width,
                 static_cast<double>(maskSize.height) / outSize.height);
    const cv::Size working(cvRound(outSize.width * scale), cvRound(outSize.height * scale));
    const bool shrinks = scale < 1.0;
    const bool largeEnough = working.width >= kMinWorkingSide && working.height >= kMinWorkingSide;
    return shrinks && largeEnough ? working : outSize;
}

void EdgeRefiner::Filter(const cv::Mat& guideBgr, const cv::Mat& maskF32, int radius,
                         cv::Mat& out) const {
#if WBC_HAVE_XIMGPROC
    cv::ximgproc::guidedFilter(guideBgr, maskF32, out, radius, eps_);
#else
    // sigmaColor is expressed in mask units, where the whole range is [0, 1].
    cv::bilateralFilter(maskF32, out, radius * 2 + 1, 0.1, static_cast<double>(radius));
#endif
}

cv::Mat EdgeRefiner::Refine(const cv::Mat& maskF32, const cv::Mat& guideBgr, cv::Size frameSize) const {
    if (maskF32.empty() || maskF32.type() != CV_32FC1) {
        throw std::invalid_argument("Refine: mask must be a non-empty single channel float image");
    }
    if (guideBgr.empty() || guideBgr.type() != CV_8UC3) {
        throw std::invalid_argument("Refine: guide must be an 8 bit three channel BGR image");
    }
    if (frameSize.width <= 0 || frameSize.height <= 0) {
        throw std::invalid_argument("Refine: frame size must have positive dimensions");
    }
    if (guideBgr.cols > frameSize.width || guideBgr.rows > frameSize.height) {
        throw std::invalid_argument("Refine: guide must be no larger than the frame");
    }

    // Every working buffer lives at function scope. Declared inside the
    // branches they would each leave gcc an unreachable unwinding epilogue on
    // a closing brace, which is a coverage hole nothing can reach.
    cv::Mat resized;
    cv::Mat filtered;
    cv::resize(maskF32, resized, guideBgr.size(), 0, 0, cv::INTER_LINEAR);
    if (radius_ == 0) {
        // Radius zero means the caller asked for no edge work at all: the
        // guided filter would reject it and the bilateral fallback would be
        // handed a 1x1 kernel.
        filtered = resized;
    } else {
        const double scale = static_cast<double>(guideBgr.rows) / frameSize.height;
        Filter(guideBgr, resized, std::max(cvRound(radius_ * scale), 1), filtered);
    }

    // Both filters can overshoot slightly at a hard boundary, and the
    // compositor is entitled to a mask that means what it says.
    cv::Mat clamped;
    cv::min(cv::max(filtered, 0.0), 1.0, clamped);
    return clamped;
}

}  // namespace wbc
