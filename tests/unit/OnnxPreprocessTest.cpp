// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <stdexcept>

#include "seg/OnnxPreprocess.h"

TEST(PreprocessShape, IsBatchOneThreeChannelsHeightThenWidth) {
    const auto shape = wbc::PreprocessShape({256, 144});
    EXPECT_EQ(shape[0], 1);
    EXPECT_EQ(shape[1], 3);
    EXPECT_EQ(shape[2], 144);
    EXPECT_EQ(shape[3], 256);
}

TEST(Preprocess, ProducesOneFloatPerChannelPerPixel) {
    const cv::Mat frame(48, 64, CV_8UC3, cv::Scalar{0, 0, 0});
    const auto tensor = wbc::Preprocess(frame, {32, 16});
    EXPECT_EQ(tensor.size(), static_cast<std::size_t>(3 * 16 * 32));
}

TEST(Preprocess, ReordersBgrIntoRgbPlanes) {
    // A single flat colour makes each plane constant, so the channel order is
    // readable straight off the first element of each plane.
    const cv::Mat frame(16, 16, CV_8UC3, cv::Scalar{10, 20, 30});  // B, G, R
    const auto tensor = wbc::Preprocess(frame, {16, 16});

    const std::size_t plane = 16 * 16;
    EXPECT_FLOAT_EQ(tensor[0], 30.0F / 255.0F);              // R plane
    EXPECT_FLOAT_EQ(tensor[plane], 20.0F / 255.0F);          // G plane
    EXPECT_FLOAT_EQ(tensor[2 * plane], 10.0F / 255.0F);      // B plane
}

TEST(Preprocess, ScalesIntoTheUnitRange) {
    const cv::Mat frame(8, 8, CV_8UC3, cv::Scalar{255, 255, 255});
    const auto tensor = wbc::Preprocess(frame, {8, 8});
    for (const float value : tensor) {
        EXPECT_FLOAT_EQ(value, 1.0F);
    }
}

TEST(Preprocess, ResizesWhenTheFrameDoesNotMatchTheModelInput) {
    const cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar{0, 0, 255});
    const auto tensor = wbc::Preprocess(frame, {256, 144});
    ASSERT_EQ(tensor.size(), static_cast<std::size_t>(3 * 144 * 256));
    EXPECT_NEAR(tensor[0], 1.0F, 0.02F);  // red stays red after resizing
}

TEST(Preprocess, KeepsSpatialLayoutRowMajorWithinEachPlane) {
    cv::Mat frame(2, 2, CV_8UC3, cv::Scalar{0, 0, 0});
    frame.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 0, 255);  // top right pixel is red

    const auto tensor = wbc::Preprocess(frame, {2, 2});
    EXPECT_FLOAT_EQ(tensor[0], 0.0F);          // R at (0, 0)
    EXPECT_FLOAT_EQ(tensor[1], 1.0F);          // R at (0, 1)
}

TEST(Preprocess, RejectsInputItCannotConvert) {
    EXPECT_THROW(wbc::Preprocess(cv::Mat{}, {16, 16}), std::invalid_argument);
    const cv::Mat grey(16, 16, CV_8UC1, cv::Scalar{0});
    EXPECT_THROW(wbc::Preprocess(grey, {16, 16}), std::invalid_argument);
    const cv::Mat frame(16, 16, CV_8UC3, cv::Scalar{0, 0, 0});
    EXPECT_THROW(wbc::Preprocess(frame, {0, 16}), std::invalid_argument);
    EXPECT_THROW(wbc::Preprocess(frame, {16, 0}), std::invalid_argument);
}
