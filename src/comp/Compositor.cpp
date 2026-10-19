// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comp/Compositor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <stdexcept>

namespace wbc {
namespace {

// The sigma OpenCV derives for a kernel of 2 * radius + 1 when it is given
// none, so that a radius still means the blur it meant before the shrinking.
double BlurSigma(int blurRadius) {
    return 0.3 * (blurRadius - 1) + 0.8;
}

// Narrower than this, a sampled Gaussian is too few taps to be one.
constexpr double kMinShrunkSigma = 1.5;

// The frame blurred by `blurRadius`, computed on a smaller copy and scaled
// back up: the blurred result has no detail a smaller copy cannot carry, and
// a Gaussian's cost falls with the area and with the kernel together. The
// pipeline's own shrunken frame is used when it is small enough to save work
// and large enough that the blur on it is still a Gaussian; otherwise the
// frame is shrunk here by BlurDownscale. A factor of one runs the same calls
// at the frame's size, which costs two copies and saves a branch that would
// leave a second exit to cover.
cv::Mat Blur(const cv::Mat& frameBgr, int blurRadius, const cv::Mat& smallBgr) {
    const double sigma = BlurSigma(blurRadius);
    const bool smallFits =
        !smallBgr.empty() &&
        sigma * smallBgr.rows / frameBgr.rows >= kMinShrunkSigma;

    cv::Mat source;
    if (smallFits) {
        source = smallBgr;
    } else {
        const int factor = BlurDownscale(blurRadius);
        const cv::Size size(std::max(frameBgr.cols / factor, 1), std::max(frameBgr.rows / factor, 1));
        cv::resize(frameBgr, source, size, 0, 0, cv::INTER_AREA);
    }

    const double scale = static_cast<double>(source.rows) / frameBgr.rows;
    const int kernel = 2 * cvRound(blurRadius * scale) + 1;
    cv::Mat blurredSmall;
    cv::Mat blurred;
    cv::GaussianBlur(source, blurredSmall, cv::Size(kernel, kernel), sigma * scale);
    cv::resize(blurredSmall, blurred, frameBgr.size(), 0, 0, cv::INTER_LINEAR);
    return blurred;
}

cv::Mat MakeBackground(const cv::Mat& frameBgr,
                       const CompositorSettings& settings,
                       const cv::Mat& backgroundBgr,
                       const cv::Mat& smallBgr) {
    switch (settings.mode) {
        case BackgroundMode::Color:
            return cv::Mat(frameBgr.size(), CV_8UC3, settings.color);

        case BackgroundMode::Image:
        case BackgroundMode::Video:
            // Checked against the frame before this was called; by here it is
            // the picture, or the current video frame, the caller supplied.
            // To the compositor the two are the same thing.
            return backgroundBgr;

        case BackgroundMode::Blur:
            if (settings.blurRadius <= 0) {
                return frameBgr;
            }
            return Blur(frameBgr, settings.blurRadius, smallBgr);
    }
    throw std::invalid_argument("Compose: unknown background mode");
}

// The mask as eight bit alpha, one value per channel, at the frame's size.
// It arrives at the size of the shrunken frame the refiner worked on and is
// scaled up after the conversion, in eight bits rather than in float, which
// is a quarter of the memory to write. A mask that strayed outside [0, 1]
// would brighten or darken the result instead of blending it, so it is
// clamped rather than trusted: the conversion saturates, which clamps the
// top, and the max() clamps the bottom, where a negative would otherwise wrap.
cv::Mat Alpha3(const cv::Mat& maskF32, cv::Size frameSize) {
    cv::Mat alpha;
    cv::Mat alpha8;
    cv::Mat upscaled;
    cv::Mat alpha3;
    cv::max(maskF32, 0.0, alpha);
    alpha.convertTo(alpha8, CV_8UC1, 255.0);
    cv::resize(alpha8, upscaled, frameSize, 0, 0, cv::INTER_LINEAR);
    cv::cvtColor(upscaled, alpha3, cv::COLOR_GRAY2BGR);
    return alpha3;
}

// out = round((frame * a + background * (255 - a)) / 255), per channel, in
// one pass. The division is exact for every value the sum can take: adding
// the sum's high byte before shifting is division by 255 rather than by 256.
// The loop is written over plain bytes with nothing aliased so that the
// compiler can vectorise it, which it does at -O3: 1.5 ms at 1280x720,
// against 4.4 ms for the same arithmetic as six OpenCV calls, which wrote
// and reread two 16 bit intermediates the size of the frame.
void MixRow(const uchar* __restrict frame,
            const uchar* __restrict background,
            const uchar* __restrict alpha,
            uchar* __restrict out,
            int count) {
    for (int i = 0; i < count; ++i) {
        const unsigned sum = frame[i] * alpha[i] + background[i] * (255U - alpha[i]) + 128U;
        out[i] = static_cast<uchar>((sum + (sum >> 8)) >> 8);
    }
}

cv::Mat Mix(const cv::Mat& frameBgr, const cv::Mat& backgroundBgr, const cv::Mat& alpha3) {
    cv::Mat out(frameBgr.size(), CV_8UC3);
    // Row by row, because a frame cut out of a larger image is not one run of
    // memory.
    const int count = frameBgr.cols * 3;
    for (int y = 0; y < frameBgr.rows; ++y) {
        MixRow(frameBgr.ptr<uchar>(y), backgroundBgr.ptr<uchar>(y), alpha3.ptr<uchar>(y),
               out.ptr<uchar>(y), count);
    }
    return out;
}

}  // namespace

int BlurDownscale(int blurRadius) {
    if (blurRadius <= 0) {
        return 1;
    }
    return std::max(static_cast<int>(BlurSigma(blurRadius) / kMinShrunkSigma), 1);
}

cv::Mat Compositor::Compose(const cv::Mat& frameBgr,
                            const cv::Mat& maskF32,
                            const CompositorSettings& settings,
                            const cv::Mat& backgroundBgr,
                            const cv::Mat& smallBgr) const {
    if (frameBgr.empty() || frameBgr.type() != CV_8UC3) {
        throw std::invalid_argument("Compose: frame must be an 8 bit three channel BGR image");
    }
    if (maskF32.empty() || maskF32.type() != CV_32FC1) {
        throw std::invalid_argument("Compose: mask must be a single channel float image");
    }
    if (maskF32.cols > frameBgr.cols || maskF32.rows > frameBgr.rows) {
        throw std::invalid_argument("Compose: mask must be no larger than the frame");
    }
    if (!smallBgr.empty() && (smallBgr.type() != CV_8UC3 || smallBgr.cols > frameBgr.cols ||
                              smallBgr.rows > frameBgr.rows)) {
        throw std::invalid_argument(
            "Compose: the shrunken frame must be 8 bit BGR and no larger than the frame");
    }
    if (settings.mode == BackgroundMode::Image || settings.mode == BackgroundMode::Video) {
        if (backgroundBgr.empty()) {
            throw std::invalid_argument("Compose: image and video modes require a background");
        }
        if (backgroundBgr.size() != frameBgr.size() || backgroundBgr.type() != CV_8UC3) {
            throw std::invalid_argument(
                "Compose: background must match the frame in size and type");
        }
    }

    const cv::Mat background = MakeBackground(frameBgr, settings, backgroundBgr, smallBgr);
    return Mix(frameBgr, background, Alpha3(maskF32, frameBgr.size()));
}

}  // namespace wbc
