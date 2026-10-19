// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include <stdexcept>

#include "comp/Compositor.h"

namespace {

cv::Mat Frame(int width, int height, cv::Scalar colour) {
    return cv::Mat(height, width, CV_8UC3, colour);
}

cv::Mat Mask(int width, int height, float value) {
    return cv::Mat(height, width, CV_32FC1, cv::Scalar{value});
}

}  // namespace

TEST(Compositor, AFullyOpaqueMaskLeavesTheFrameUntouched) {
    const wbc::Compositor compositor;
    const cv::Mat frame = Frame(32, 32, {10, 20, 30});
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{200, 200, 200};

    const cv::Mat out = compositor.Compose(frame, Mask(32, 32, 1.0F), settings);
    EXPECT_EQ(out.at<cv::Vec3b>(16, 16), cv::Vec3b(10, 20, 30));
}

TEST(Compositor, AFullyTransparentMaskShowsOnlyTheBackground) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{5, 6, 7};

    const cv::Mat out = compositor.Compose(Frame(32, 32, {10, 20, 30}), Mask(32, 32, 0.0F), settings);
    EXPECT_EQ(out.at<cv::Vec3b>(16, 16), cv::Vec3b(5, 6, 7));
}

TEST(Compositor, AHalfMaskBlendsHalfway) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{0, 0, 0};

    const cv::Mat out = compositor.Compose(Frame(32, 32, {100, 100, 100}), Mask(32, 32, 0.5F), settings);
    const cv::Vec3b pixel = out.at<cv::Vec3b>(16, 16);
    EXPECT_NEAR(pixel[0], 50, 1);
    EXPECT_NEAR(pixel[1], 50, 1);
    EXPECT_NEAR(pixel[2], 50, 1);
}

TEST(Compositor, EachRegionOfTheMaskIsAppliedWhereItSits) {
    const wbc::Compositor compositor;
    cv::Mat mask = Mask(32, 32, 0.0F);
    mask.colRange(0, 16).setTo(cv::Scalar{1.0});

    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{0, 0, 255};

    const cv::Mat out = compositor.Compose(Frame(32, 32, {255, 0, 0}), mask, settings);
    EXPECT_EQ(out.at<cv::Vec3b>(16, 4), cv::Vec3b(255, 0, 0));
    EXPECT_EQ(out.at<cv::Vec3b>(16, 28), cv::Vec3b(0, 0, 255));
}

TEST(Compositor, ImageModeUsesTheSuppliedBackground) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Image;

    const cv::Mat background = Frame(32, 32, {1, 2, 3});
    const cv::Mat out = compositor.Compose(Frame(32, 32, {9, 9, 9}), Mask(32, 32, 0.0F), settings, background);
    EXPECT_EQ(out.at<cv::Vec3b>(16, 16), cv::Vec3b(1, 2, 3));
}

TEST(Compositor, BlurModeKeepsTheAverageButRemovesDetail) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Blur;
    settings.blurRadius = 15;

    // A sharp edge in the frame; with the mask fully transparent, the output is
    // the blurred frame, so the edge must have softened.
    cv::Mat frame = Frame(64, 64, {0, 0, 0});
    frame.colRange(32, 64).setTo(cv::Scalar{255, 255, 255});

    const cv::Mat out = compositor.Compose(frame, Mask(64, 64, 0.0F), settings);
    const int atEdge = out.at<cv::Vec3b>(32, 32)[0];
    EXPECT_GT(atEdge, 20);
    EXPECT_LT(atEdge, 235);
}

TEST(Compositor, ABlurIsShrunkOnlyAsFarAsItsSigmaAllows) {
    // Small blurs run at the frame's own size: shrinking would leave them
    // less than a pixel and a half wide, where a Gaussian is no longer one.
    EXPECT_EQ(wbc::BlurDownscale(-5), 1);
    EXPECT_EQ(wbc::BlurDownscale(0), 1);
    EXPECT_EQ(wbc::BlurDownscale(3), 1);
    // The default radius, sigma 5.0.
    EXPECT_EQ(wbc::BlurDownscale(15), 3);
    EXPECT_EQ(wbc::BlurDownscale(60), 12);
    int previous = 1;
    for (int radius = 0; radius <= 60; ++radius) {
        const int factor = wbc::BlurDownscale(radius);
        EXPECT_GE(factor, previous) << "radius " << radius;
        previous = factor;
    }
}

// The blur is computed on a shrunken frame and scaled back up. What it must be
// held to is looking like the full-size Gaussian it replaces.
TEST(Compositor, AShrunkenBlurLooksLikeTheFullSizeOne) {
    cv::Mat frame(240, 320, CV_8UC3);
    cv::randu(frame, 0, 255);
    // Noise alone averages to grey under either blur; a few hard shapes give
    // the comparison edges and gradients to get wrong.
    cv::rectangle(frame, {40, 30, 100, 120}, cv::Scalar{255, 255, 255}, cv::FILLED);
    cv::circle(frame, {230, 150}, 50, cv::Scalar{0, 0, 0}, cv::FILLED);

    for (const int radius : {15, 40, 60}) {
        wbc::CompositorSettings settings;
        settings.mode = wbc::BackgroundMode::Blur;
        settings.blurRadius = radius;
        ASSERT_GT(wbc::BlurDownscale(radius), 1);

        const cv::Mat out = wbc::Compositor{}.Compose(frame, Mask(320, 240, 0.0F), settings);
        cv::Mat reference;
        const int kernel = radius * 2 + 1;
        cv::GaussianBlur(frame, reference, cv::Size(kernel, kernel), 0.0);

        cv::Mat difference;
        cv::absdiff(out, reference, difference);
        const cv::Scalar meanDifference = cv::mean(difference);
        double worst = 0.0;
        cv::minMaxLoc(difference.reshape(1), nullptr, &worst);
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_LT(meanDifference[channel], 2.0) << "radius " << radius;
        }
        EXPECT_LT(worst, 16.0) << "radius " << radius;

        // The same blur taken from a shrunken copy the caller made, the way
        // the pipeline makes one, at the 455x256-for-720p ratio. A frame of
        // 16:9, like a camera's: at the largest radius the kernel is 121
        // pixels, and on a frame much smaller than this the comparison would
        // be of how each scale reflects the border, not of the blur.
        cv::Mat wide(360, 640, CV_8UC3);
        cv::resize(frame, wide, wide.size(), 0, 0, cv::INTER_LINEAR);
        cv::Mat small;
        cv::resize(wide, small, {228, 128}, 0, 0, cv::INTER_AREA);
        const cv::Mat fromSmall =
            wbc::Compositor{}.Compose(wide, Mask(640, 360, 0.0F), settings, {}, small);
        cv::Mat wideReference;
        cv::GaussianBlur(wide, wideReference, cv::Size(kernel, kernel), 0.0);
        cv::absdiff(fromSmall, wideReference, difference);
        const cv::Scalar meanFromSmall = cv::mean(difference);
        cv::minMaxLoc(difference.reshape(1), nullptr, &worst);
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_LT(meanFromSmall[channel], 2.0) << "radius " << radius << " from the small copy";
        }
        EXPECT_LT(worst, 16.0) << "radius " << radius << " from the small copy";
    }
}

// The pipeline shrinks each frame once for the model and the edge filter, and
// a large blur reuses that copy rather than shrinking the frame again.
TEST(Compositor, ALargeBlurIsTakenFromTheSuppliedShrunkenFrame) {
    const cv::Mat frame = Frame(128, 96, {0, 0, 0});
    // Deliberately not a shrunken copy of the frame, so the output says which
    // one was blurred.
    const cv::Mat small = Frame(32, 24, {10, 200, 90});
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Blur;
    settings.blurRadius = 30;

    const cv::Mat out = wbc::Compositor{}.Compose(frame, Mask(128, 96, 0.0F), settings, {}, small);
    EXPECT_LE(cv::norm(out, Frame(128, 96, {10, 200, 90}), cv::NORM_INF), 1.0);
}

// Blurred on a copy four times smaller, a radius of 3 would have a sigma of a
// third of a pixel, so the shrunken frame is passed over.
TEST(Compositor, ASmallBlurIgnoresTheShrunkenFrame) {
    const cv::Mat frame = Frame(128, 96, {0, 0, 0});
    const cv::Mat small = Frame(32, 24, {10, 200, 90});
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Blur;
    settings.blurRadius = 3;

    const cv::Mat out = wbc::Compositor{}.Compose(frame, Mask(128, 96, 0.0F), settings, {}, small);
    EXPECT_EQ(cv::norm(out, frame, cv::NORM_INF), 0.0);
}

TEST(Compositor, RejectsAShrunkenFrameItCannotUse) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    const cv::Mat frame = Frame(32, 32, {0, 0, 0});
    const cv::Mat mask = Mask(32, 32, 1.0F);
    EXPECT_THROW(compositor.Compose(frame, mask, settings, {}, Frame(64, 16, {0, 0, 0})),
                 std::invalid_argument);
    EXPECT_THROW(compositor.Compose(frame, mask, settings, {}, Frame(16, 64, {0, 0, 0})),
                 std::invalid_argument);
    EXPECT_THROW(compositor.Compose(frame, mask, settings, {}, cv::Mat(16, 16, CV_8UC1)),
                 std::invalid_argument);
}

TEST(Compositor, ABlurRadiusOfZeroLeavesTheBackgroundSharp) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Blur;
    settings.blurRadius = 0;

    cv::Mat frame = Frame(64, 64, {0, 0, 0});
    frame.colRange(32, 64).setTo(cv::Scalar{255, 255, 255});

    const cv::Mat out = compositor.Compose(frame, Mask(64, 64, 0.0F), settings);
    EXPECT_EQ(out.at<cv::Vec3b>(32, 31)[0], 0);
    EXPECT_EQ(out.at<cv::Vec3b>(32, 33)[0], 255);
}

TEST(Compositor, RejectsAMaskLargerThanTheFrame) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(64, 32, 1.0F), settings),
                 std::invalid_argument);
    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(32, 64, 1.0F), settings),
                 std::invalid_argument);
}

// The refiner delivers the mask at the size of the shrunken frame it worked
// on. Scaling it up is left to here, where it is done in eight bits.
TEST(Compositor, AMaskSmallerThanTheFrameIsScaledUpToCoverIt) {
    cv::Mat mask = Mask(16, 16, 0.0F);
    mask.colRange(0, 8).setTo(cv::Scalar{1.0});

    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{0, 0, 255};
    const cv::Mat out = wbc::Compositor{}.Compose(Frame(64, 64, {255, 0, 0}), mask, settings);

    ASSERT_EQ(out.size(), cv::Size(64, 64));
    EXPECT_EQ(out.at<cv::Vec3b>(32, 4), cv::Vec3b(255, 0, 0));
    EXPECT_EQ(out.at<cv::Vec3b>(32, 59), cv::Vec3b(0, 0, 255));
    // Across the boundary the scaled-up mask is a ramp, not a step.
    const cv::Vec3b middle = out.at<cv::Vec3b>(32, 32);
    EXPECT_GT(middle[0], 0);
    EXPECT_LT(middle[0], 255);
}

// One blend in a single pass, against the formula it implements, over every
// combination a channel can meet: the rounding must be exact, not close.
TEST(Compositor, TheBlendRoundsExactlyAsTheFormulaSays) {
    cv::Mat frame(256, 256, CV_8UC3);
    cv::Mat mask(256, 256, CV_32FC1);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            frame.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uchar>(x), static_cast<uchar>(y),
                                                  static_cast<uchar>(255 - x));
            mask.at<float>(y, x) = static_cast<float>((x + y) % 256) / 255.0F;
        }
    }
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{37, 128, 250};

    const cv::Mat out = wbc::Compositor{}.Compose(frame, mask, settings);

    int mismatches = 0;
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const int a = (x + y) % 256;
            for (int c = 0; c < 3; ++c) {
                const int f = frame.at<cv::Vec3b>(y, x)[c];
                const int b = static_cast<int>(settings.color[c]);
                const int expected = cvRound((f * a + b * (255 - a)) / 255.0);
                mismatches += out.at<cv::Vec3b>(y, x)[c] != expected ? 1 : 0;
            }
        }
    }
    EXPECT_EQ(mismatches, 0);
}

// A frame cut out of a larger image has rows that do not follow one another
// in memory, and the blend walks it row by row for that reason.
TEST(Compositor, BlendsAFrameWhoseRowsAreNotContiguous) {
    cv::Mat larger(48, 64, CV_8UC3, cv::Scalar{0, 0, 0});
    larger(cv::Rect(8, 8, 32, 32)).setTo(cv::Scalar{100, 100, 100});
    const cv::Mat frame = larger(cv::Rect(8, 8, 32, 32));
    ASSERT_FALSE(frame.isContinuous());

    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{0, 0, 0};
    const cv::Mat out = wbc::Compositor{}.Compose(frame, Mask(32, 32, 0.5F), settings);

    EXPECT_EQ(cv::norm(out, cv::Mat(32, 32, CV_8UC3, cv::Scalar{50, 50, 50}), cv::NORM_INF), 0.0);
}

TEST(Compositor, RejectsWronglyTypedInput) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    const cv::Mat eightBitMask(32, 32, CV_8UC1, cv::Scalar{255});
    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), eightBitMask, settings), std::invalid_argument);
    const cv::Mat floatFrame(32, 32, CV_32FC3, cv::Scalar{0, 0, 0});
    EXPECT_THROW(compositor.Compose(floatFrame, Mask(32, 32, 1.0F), settings), std::invalid_argument);
}

TEST(Compositor, RejectsImageModeWithoutABackground) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Image;
    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(32, 32, 1.0F), settings, cv::Mat{}),
                 std::invalid_argument);
}

TEST(Compositor, RejectsABackgroundOfADifferentSize) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Image;
    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(32, 32, 1.0F), settings, Frame(16, 16, {0, 0, 0})),
                 std::invalid_argument);
}

TEST(Compositor, MaskValuesOutsideTheUnitRangeAreClamped) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.color = cv::Scalar{0, 0, 0};

    const cv::Mat out = compositor.Compose(Frame(32, 32, {100, 100, 100}), Mask(32, 32, 4.0F), settings);
    EXPECT_EQ(out.at<cv::Vec3b>(16, 16), cv::Vec3b(100, 100, 100));
}

TEST(Compositor, RejectsABackgroundModeItDoesNotKnow) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    // Well defined for a scoped enum with a fixed underlying type. This is
    // the only way to reach the guard after the switch, and the guard has
    // to stay reachable because this directory is gated at 100% coverage.
    settings.mode = static_cast<wbc::BackgroundMode>(99);

    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(32, 32, 1.0F), settings),
                 std::invalid_argument);
}

TEST(Compositor, RejectsAnImageBackgroundOfTheWrongType) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Image;
    const cv::Mat floatBackground(32, 32, CV_32FC3, cv::Scalar{0, 0, 0});

    EXPECT_THROW(compositor.Compose(Frame(32, 32, {0, 0, 0}), Mask(32, 32, 1.0F), settings, floatBackground),
                 std::invalid_argument);
}

TEST(Compositor, ANegativeBlurRadiusIsTreatedLikeNoBlur) {
    const wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Blur;
    settings.blurRadius = -5;

    cv::Mat frame = Frame(64, 64, {0, 0, 0});
    frame.colRange(32, 64).setTo(cv::Scalar{255, 255, 255});

    const cv::Mat out = compositor.Compose(frame, Mask(64, 64, 0.0F), settings);
    EXPECT_EQ(out.at<cv::Vec3b>(32, 31)[0], 0);
    EXPECT_EQ(out.at<cv::Vec3b>(32, 33)[0], 255);
}

// To the compositor a video frame is a picture that happens to change.
TEST(Compositor, VideoModeUsesTheSuppliedFrameLikeAPicture) {
    const cv::Mat frame(8, 8, CV_8UC3, cv::Scalar{10, 20, 30});
    const cv::Mat mask(8, 8, CV_32FC1, cv::Scalar{0.0});
    const cv::Mat videoFrame(8, 8, CV_8UC3, cv::Scalar{200, 100, 50});

    wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Video;
    const cv::Mat out = compositor.Compose(frame, mask, settings, videoFrame);

    EXPECT_EQ(out.at<cv::Vec3b>(4, 4), cv::Vec3b(200, 100, 50));
}

TEST(Compositor, VideoModeRefusesToRunWithoutAFrame) {
    const cv::Mat frame(8, 8, CV_8UC3, cv::Scalar{10, 20, 30});
    const cv::Mat mask(8, 8, CV_32FC1, cv::Scalar{0.0});

    wbc::Compositor compositor;
    wbc::CompositorSettings settings;
    settings.mode = wbc::BackgroundMode::Video;
    EXPECT_THROW(compositor.Compose(frame, mask, settings), std::invalid_argument);
}
