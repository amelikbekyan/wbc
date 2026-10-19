// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include <stdexcept>

#include "sink/I420Converter.h"

namespace {

cv::Mat SolidBgr(int width, int height, cv::Scalar colour) {
    return cv::Mat(height, width, CV_8UC3, colour);
}

}  // namespace

TEST(I420Converter, OutputHasTheExactPlanarSizeV4l2Expects) {
    const cv::Mat out = wbc::ToI420(SolidBgr(64, 48, {10, 20, 30}));
    EXPECT_EQ(out.type(), CV_8UC1);
    EXPECT_EQ(out.cols, 64);
    EXPECT_EQ(out.rows, 48 * 3 / 2);
    EXPECT_TRUE(out.isContinuous());
}

TEST(I420Converter, GreyInputProducesNeutralChromaPlanes) {
    const cv::Mat out = wbc::ToI420(SolidBgr(64, 48, {128, 128, 128}));
    // The chroma planes follow the 48 rows of luma.
    const cv::Mat chroma = out.rowRange(48, out.rows);
    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(chroma, &minValue, &maxValue);
    EXPECT_NEAR(minValue, 128.0, 2.0);
    EXPECT_NEAR(maxValue, 128.0, 2.0);
}

TEST(I420Converter, ColoursSurviveARoundTripWithinToleranceOfChromaSubsampling) {
    const cv::Mat source = SolidBgr(64, 48, {200, 60, 30});
    const cv::Mat encoded = wbc::ToI420(source);

    cv::Mat decoded;
    cv::cvtColor(encoded, decoded, cv::COLOR_YUV2BGR_I420);

    ASSERT_EQ(decoded.size(), source.size());
    const cv::Vec3b actual = decoded.at<cv::Vec3b>(24, 32);
    EXPECT_NEAR(actual[0], 200, 6);
    EXPECT_NEAR(actual[1], 60, 6);
    EXPECT_NEAR(actual[2], 30, 6);
}

TEST(I420Converter, RejectsOddDimensionsBecauseChromaCannotBeSubsampled) {
    EXPECT_THROW(wbc::ToI420(SolidBgr(65, 48, {0, 0, 0})), std::invalid_argument);
    EXPECT_THROW(wbc::ToI420(SolidBgr(64, 49, {0, 0, 0})), std::invalid_argument);
}

TEST(I420Converter, RejectsEmptyAndWronglyTypedInput) {
    EXPECT_THROW(wbc::ToI420(cv::Mat{}), std::invalid_argument);
    EXPECT_THROW(wbc::ToI420(cv::Mat(48, 64, CV_8UC1, cv::Scalar{0})), std::invalid_argument);
    EXPECT_THROW(wbc::ToI420(cv::Mat(48, 64, CV_32FC3, cv::Scalar{0, 0, 0})), std::invalid_argument);
}

TEST(EnsureContinuous, RepacksAMatrixThatHasRowPadding) {
    cv::Mat parent(10, 10, CV_8UC1, cv::Scalar{7});
    const cv::Mat roi = parent(cv::Rect(0, 0, 5, 5));
    ASSERT_FALSE(roi.isContinuous()) << "the fixture is wrong, not the code";

    const cv::Mat packed = wbc::EnsureContinuous(roi);
    EXPECT_TRUE(packed.isContinuous());
    EXPECT_EQ(packed.size(), roi.size());
    EXPECT_EQ(packed.at<unsigned char>(2, 2), 7);
    EXPECT_NE(packed.data, roi.data) << "a repack must own its own buffer";
}

TEST(EnsureContinuous, ReturnsAnAlreadyPackedMatrixUntouched) {
    const cv::Mat dense(4, 4, CV_8UC1, cv::Scalar{3});
    const cv::Mat result = wbc::EnsureContinuous(dense);
    EXPECT_TRUE(result.isContinuous());
    EXPECT_EQ(result.data, dense.data) << "no copy should have been made";
}
