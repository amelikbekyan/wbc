// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include <stdexcept>

#include "core/ImageConvert.h"

TEST(ToQImage, ProducesAnImageOfTheSameSizeInRgb) {
    const cv::Mat frame(48, 64, CV_8UC3, cv::Scalar{10, 20, 30});
    const QImage image = wbc::ToQImage(frame);
    EXPECT_EQ(image.width(), 64);
    EXPECT_EQ(image.height(), 48);
    EXPECT_EQ(image.format(), QImage::Format_RGB888);
}

TEST(ToQImage, SwapsTheChannelOrderBecauseOpenCvIsBlueFirst) {
    const cv::Mat frame(8, 8, CV_8UC3, cv::Scalar{10, 20, 30});  // B, G, R
    const QImage image = wbc::ToQImage(frame);
    const QColor pixel = image.pixelColor(4, 4);
    EXPECT_EQ(pixel.red(), 30);
    EXPECT_EQ(pixel.green(), 20);
    EXPECT_EQ(pixel.blue(), 10);
}

TEST(ToQImage, ReturnsAnImageThatOutlivesTheFunctionsLocalBuffer) {
    cv::Mat frame(8, 8, CV_8UC3, cv::Scalar{0, 0, 0});
    const QImage image = wbc::ToQImage(frame);
    frame.setTo(cv::Scalar{255, 255, 255});
    // This is a weak check by construction. The image and the caller's frame
    // were never the same buffer — cvtColor allocates a fresh one — so nothing
    // here distinguishes a real copy from an alias. What .copy() actually
    // guards is the lifetime of the local matrix, which is freed when this
    // function returns; without it the returned QImage would dangle, and a
    // freed buffer usually still reads as its old contents. The sanitizer
    // build is what enforces this, not this assertion.
    EXPECT_EQ(image.pixelColor(4, 4).red(), 0);
}

TEST(ToQImage, HandlesAWidthThatIsNotAMultipleOfFour) {
    // Qt pads scan lines to four bytes; a copy that ignores that produces a
    // sheared picture.
    const cv::Mat frame(5, 5, CV_8UC3, cv::Scalar{0, 0, 255});
    const QImage image = wbc::ToQImage(frame);
    ASSERT_EQ(image.width(), 5);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            EXPECT_EQ(image.pixelColor(x, y).red(), 255) << "at " << x << ", " << y;
        }
    }
}

TEST(ToQImage, ReturnsANullImageForInputItCannotConvert) {
    EXPECT_TRUE(wbc::ToQImage(cv::Mat{}).isNull());
    const cv::Mat grey(8, 8, CV_8UC1, cv::Scalar{255});
    EXPECT_TRUE(wbc::ToQImage(grey).isNull());
}

namespace {

// Noise softened into something with gradients and edges, like a camera
// frame, so that the comparisons below have structure to get wrong.
cv::Mat CameraLikeFrame(int width, int height) {
    cv::Mat frame(height, width, CV_8UC3);
    cv::randu(frame, 0, 255);
    cv::GaussianBlur(frame, frame, {5, 5}, 0.0);
    cv::rectangle(frame, {width / 8, height / 8, width / 3, height / 2}, cv::Scalar{255, 255, 255},
                  cv::FILLED);
    return frame;
}

}  // namespace

// Halving first and finishing with one resize is a shortcut to the same
// average a single area resize computes; it has to land on the same picture.
TEST(ShrinkFrame, MatchesASingleAreaResize) {
    const cv::Mat frame = CameraLikeFrame(1280, 720);
    for (const cv::Size target : {cv::Size(455, 256), cv::Size(640, 360), cv::Size(160, 90),
                                  cv::Size(256, 256)}) {
        const cv::Mat shrunk = wbc::ShrinkFrame(frame, target);
        ASSERT_EQ(shrunk.size(), target);
        ASSERT_EQ(shrunk.type(), CV_8UC3);

        cv::Mat direct;
        cv::resize(frame, direct, target, 0, 0, cv::INTER_AREA);
        cv::Mat difference;
        cv::absdiff(shrunk, direct, difference);
        // Both are area averages, but halving rounds to eight bits in between
        // and weighs the last step's partial pixels differently. On this
        // fixture's pixel-level noise, far coarser than a camera's, that came
        // to 1.3 to 1.8 levels on average where the last step is not itself
        // an exact half, and nothing where it is. A shrink that sampled
        // instead of averaging would be off by tens.
        const cv::Scalar meanDifference = cv::mean(difference);
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_LT(meanDifference[channel], 2.5) << target;
        }
    }
}

TEST(ShrinkFrame, IsExactWhereEveryStepIsAHalf) {
    const cv::Mat frame = CameraLikeFrame(1280, 720);
    cv::Mat direct;
    cv::resize(frame, direct, {320, 180}, 0, 0, cv::INTER_AREA);
    EXPECT_LE(cv::norm(wbc::ShrinkFrame(frame, {320, 180}), direct, cv::NORM_INF), 1.0);
}

TEST(ShrinkFrame, HandsBackTheFrameItselfWhenItIsAlreadyTheTargetSize) {
    const cv::Mat frame = CameraLikeFrame(64, 48);
    const cv::Mat same = wbc::ShrinkFrame(frame, frame.size());
    EXPECT_EQ(same.data, frame.data);
}

TEST(ShrinkFrame, RejectsWhatItCannotShrink) {
    const cv::Mat frame = CameraLikeFrame(64, 48);
    EXPECT_THROW(wbc::ShrinkFrame(cv::Mat{}, {32, 24}), std::invalid_argument);
    EXPECT_THROW(wbc::ShrinkFrame(cv::Mat(48, 64, CV_8UC1), {32, 24}), std::invalid_argument);
    EXPECT_THROW(wbc::ShrinkFrame(frame, {0, 24}), std::invalid_argument);
    EXPECT_THROW(wbc::ShrinkFrame(frame, {128, 24}), std::invalid_argument);
    EXPECT_THROW(wbc::ShrinkFrame(frame, {32, 96}), std::invalid_argument);
}
