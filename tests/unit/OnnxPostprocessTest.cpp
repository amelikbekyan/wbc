// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "seg/OnnxPostprocess.h"

TEST(Postprocess, ReadsChannelFirstOutput) {
    const std::vector<float> data{0.0F, 0.25F, 0.5F, 1.0F};
    const cv::Mat mask = wbc::Postprocess(data.data(), {1, 1, 2, 2});

    ASSERT_EQ(mask.size(), cv::Size(2, 2));
    EXPECT_EQ(mask.type(), CV_32FC1);
    EXPECT_FLOAT_EQ(mask.at<float>(0, 0), 0.0F);
    EXPECT_FLOAT_EQ(mask.at<float>(0, 1), 0.25F);
    EXPECT_FLOAT_EQ(mask.at<float>(1, 0), 0.5F);
    EXPECT_FLOAT_EQ(mask.at<float>(1, 1), 1.0F);
}

TEST(Postprocess, ReadsChannelLastOutput) {
    const std::vector<float> data{0.0F, 0.25F, 0.5F, 1.0F};
    const cv::Mat mask = wbc::Postprocess(data.data(), {1, 2, 2, 1});

    ASSERT_EQ(mask.size(), cv::Size(2, 2));
    EXPECT_FLOAT_EQ(mask.at<float>(1, 1), 1.0F);
}

TEST(Postprocess, ReadsOutputWithoutAChannelAxis) {
    const std::vector<float> data{0.1F, 0.2F, 0.3F, 0.4F};
    const cv::Mat mask = wbc::Postprocess(data.data(), {1, 2, 2});

    ASSERT_EQ(mask.size(), cv::Size(2, 2));
    EXPECT_FLOAT_EQ(mask.at<float>(0, 1), 0.2F);
}

TEST(Postprocess, CopiesRatherThanAliasingTheCallersBuffer) {
    std::vector<float> data{0.0F, 0.0F, 0.0F, 0.0F};
    const cv::Mat mask = wbc::Postprocess(data.data(), {1, 1, 2, 2});
    data[0] = 9.0F;
    EXPECT_FLOAT_EQ(mask.at<float>(0, 0), 0.0F);
}

TEST(Postprocess, RejectsShapesItCannotInterpret) {
    const std::vector<float> data(16, 0.0F);
    EXPECT_THROW(wbc::Postprocess(data.data(), {}), std::invalid_argument);
    EXPECT_THROW(wbc::Postprocess(data.data(), {4}), std::invalid_argument);
    EXPECT_THROW(wbc::Postprocess(data.data(), {1, 3, 2, 2}), std::invalid_argument);
    EXPECT_THROW(wbc::Postprocess(data.data(), {2, 1, 2, 2}), std::invalid_argument);
    EXPECT_THROW(wbc::Postprocess(data.data(), {1, 1, 0, 2}), std::invalid_argument);
}

TEST(Postprocess, RejectsANullBuffer) {
    EXPECT_THROW(wbc::Postprocess(nullptr, {1, 1, 2, 2}), std::invalid_argument);
}

TEST(Postprocess, RejectsAThreeDimensionalOutputWithABatchGreaterThanOne) {
    const std::vector<float> data(16, 0.0F);
    EXPECT_THROW(wbc::Postprocess(data.data(), {2, 2, 2}), std::invalid_argument);
}

TEST(Postprocess, RejectsAShapeThatMatchesBothLayoutsAtOnce) {
    const std::vector<float> data(4, 0.0F);
    // {1,1,W,1} is {1,1,H,W} with W=1 and {1,H,W,1} with H=1 at the same
    // time. Refusing it is better than silently transposing the mask.
    EXPECT_THROW(wbc::Postprocess(data.data(), {1, 1, 4, 1}), std::invalid_argument);
}

TEST(Postprocess, RejectsDegenerateWidth) {
    const std::vector<float> data(4, 0.0F);
    EXPECT_THROW(wbc::Postprocess(data.data(), {1, 1, 2, 0}), std::invalid_argument);
}
