// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

namespace wbc {

// Lifts the small mask produced by the model up to the frame size and pulls
// its boundary onto the real contours of the person.
//
// A plain resize leaves a soft halo wherever the model's low resolution cut a
// corner. A guided filter, with the frame itself as the guide, snaps the mask
// back onto the edges the camera actually recorded. Where opencv_contrib is
// unavailable the build falls back to a bilateral filter, which softens the
// boundary without aligning it and therefore looks worse, but still works.
class EdgeRefiner {
public:
    explicit EdgeRefiner(int radius = 8, double eps = 1e-4);

    // Refines the mask against the guide and delivers it at the guide's size.
    // The guide may be a shrunken copy of the frame, no larger than
    // `frameSize`; the radius is in the frame's pixels and is scaled down with
    // it. Scaling the result up to the frame is left to the compositor.
    cv::Mat Refine(const cv::Mat& maskF32, const cv::Mat& guideBgr, cv::Size frameSize) const;

    // Negative radii are clamped to zero, which turns the filter off and
    // leaves the plain upscale.
    void SetRadius(int radius);
    int Radius() const;

    // The size worth shrinking a frame of `outSize` to for a mask of
    // `maskSize`: the frame scaled down until the mask's resolution is reached
    // along one axis, or the frame's own size when that would not shrink it,
    // or would shrink it too far for the filter's window. At 1280x720 with
    // the model's 256x256 mask that is 455x256, and the guided filter costs
    // a third of what it did at half the frame's size. What it costs is a
    // softer boundary, since the filter never sees the finest detail in the
    // frame -- but the mask being corrected never had detail finer than this.
    static cv::Size WorkingSize(cv::Size maskSize, cv::Size outSize);

private:
    // The edge-aware filter itself, wherever it is being run and at whatever
    // size. Kept apart so that the choice of working resolution does not have
    // to be written twice around the build-time choice of filter. It writes
    // through `out` rather than returning, so that it owns no local of its
    // own: a returned cv::Mat leaves gcc an unwinding epilogue on the closing
    // brace that nothing can reach and coverage cannot excuse.
    void Filter(const cv::Mat& guideBgr, const cv::Mat& maskF32, int radius, cv::Mat& out) const;

    int radius_;
    double eps_;
};

}  // namespace wbc
