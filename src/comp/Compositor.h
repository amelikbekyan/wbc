// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

#include "core/Types.h"

namespace wbc {

// How many times smaller, along each side, the frame is made before a blur of
// this radius is computed on it. A Gaussian's cost grows with the area and
// with the kernel, and both shrink together; the blurred result has no detail
// a smaller copy cannot carry. The factor stops where the shrunken blur would
// be narrower than a pixel and a half, which is no longer a Gaussian worth
// the name. A radius of zero or less is no blur, and a factor of one.
int BlurDownscale(int blurRadius);

struct CompositorSettings {
    BackgroundMode mode = BackgroundMode::Blur;
    // Radius in pixels; the Gaussian kernel is 2 * radius + 1. Zero means no
    // blur at all, which is useful for seeing what the mask alone is doing.
    int blurRadius = 15;
    // Default is the green commonly used for chroma keying downstream.
    cv::Scalar color{0, 177, 64};
};

// Blends the frame over a background according to the mask:
//   out = frame * a + background * (1 - a)
// with a taken from the mask and clamped to [0, 1].
//
// The mask may be smaller than the frame, as the refiner delivers it at the
// size of the shrunken frame it worked on, and is scaled up to cover the
// frame. `smallBgr`, when given, is that shrunken frame; a large blur is taken
// from it rather than shrinking the frame again. It must be no larger than
// the frame, and is ignored where it is too small for the blur asked for.
class Compositor {
public:
    cv::Mat Compose(const cv::Mat& frameBgr,
                    const cv::Mat& maskF32,
                    const CompositorSettings& settings,
                    const cv::Mat& backgroundBgr = {},
                    const cv::Mat& smallBgr = {}) const;
};

}  // namespace wbc
