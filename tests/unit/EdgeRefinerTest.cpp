// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include <stdexcept>

#include "seg/EdgeRefiner.h"

namespace {

// A guide with a hard vertical edge at x = 32 and a mask whose edge sits ten
// pixels away from it. Refinement should pull the mask towards the guide.
cv::Mat GuideWithEdge(int width, int height, int edgeX) {
    cv::Mat guide(height, width, CV_8UC3, cv::Scalar{0, 0, 0});
    guide.colRange(edgeX, width).setTo(cv::Scalar{255, 255, 255});
    return guide;
}

cv::Mat MaskWithEdge(int width, int height, int edgeX) {
    cv::Mat mask(height, width, CV_32FC1, cv::Scalar{0.0});
    mask.colRange(edgeX, width).setTo(cv::Scalar{1.0});
    return mask;
}

}  // namespace

TEST(EdgeRefiner, ProducesTheRequestedSizeAndType) {
    const wbc::EdgeRefiner refiner;
    const cv::Mat out = refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(64, 64, 32), {64, 64});
    EXPECT_EQ(out.size(), cv::Size(64, 64));
    EXPECT_EQ(out.type(), CV_32FC1);
}

TEST(EdgeRefiner, KeepsValuesInsideTheUnitRange) {
    const wbc::EdgeRefiner refiner;
    const cv::Mat out = refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(64, 64, 32), {64, 64});
    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(out, &minValue, &maxValue);
    EXPECT_GE(minValue, 0.0);
    EXPECT_LE(maxValue, 1.0);
}

TEST(EdgeRefiner, AUniformMaskStaysUniformWhateverTheGuideShows) {
    const wbc::EdgeRefiner refiner;
    const cv::Mat mask(16, 16, CV_32FC1, cv::Scalar{1.0});
    const cv::Mat out = refiner.Refine(mask, GuideWithEdge(64, 64, 32), {64, 64});
    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(out, &minValue, &maxValue);
    EXPECT_NEAR(minValue, 1.0, 0.02);
    EXPECT_NEAR(maxValue, 1.0, 0.02);
}

TEST(EdgeRefiner, ChangesTheMaskRatherThanMerelyResizingIt) {
    const wbc::EdgeRefiner refiner;
    const cv::Mat mask = MaskWithEdge(16, 16, 8);
    const cv::Mat guide = GuideWithEdge(64, 64, 40);

    cv::Mat plainResize;
    cv::resize(mask, plainResize, cv::Size(64, 64), 0, 0, cv::INTER_LINEAR);

    const cv::Mat refined = refiner.Refine(mask, guide, {64, 64});
    const double difference = cv::norm(refined, plainResize, cv::NORM_L1);
    // Measured at roughly 285 for this fixture. A threshold of 1.0 would pass
    // even if the filter did almost nothing; 50 keeps a wide margin against
    // fixture noise while still catching a filter weakened by an order of
    // magnitude.
    EXPECT_GT(difference, 50.0);
}

TEST(EdgeRefiner, RejectsAGuideLargerThanTheFrame) {
    const wbc::EdgeRefiner refiner;
    EXPECT_THROW(refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(128, 64, 16), {64, 64}),
                 std::invalid_argument);
    EXPECT_THROW(refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(64, 128, 16), {64, 64}),
                 std::invalid_argument);
}

TEST(EdgeRefiner, RejectsWronglyTypedInput) {
    const wbc::EdgeRefiner refiner;
    const cv::Mat eightBitMask(16, 16, CV_8UC1, cv::Scalar{255});
    EXPECT_THROW(refiner.Refine(eightBitMask, GuideWithEdge(64, 64, 32), {64, 64}), std::invalid_argument);

    const cv::Mat greyGuide(64, 64, CV_8UC1, cv::Scalar{255});
    EXPECT_THROW(refiner.Refine(MaskWithEdge(16, 16, 8), greyGuide, {64, 64}), std::invalid_argument);
}

TEST(EdgeRefiner, RejectsADegenerateOutputSize) {
    const wbc::EdgeRefiner refiner;
    EXPECT_THROW(refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(64, 64, 32), {0, 64}),
                 std::invalid_argument);
}

TEST(EdgeRefiner, ClampsAMaskThatArrivesOutsideTheUnitRange) {
    const wbc::EdgeRefiner refiner;
    // Refine validates the mask's type but not its range, so this is a
    // legal call. Without the clamp these values would survive filtering
    // and reach the compositor, which multiplies by them.
    cv::Mat mask(16, 16, CV_32FC1, cv::Scalar{1.5});
    mask.colRange(0, 8).setTo(cv::Scalar{-0.5});

    const cv::Mat out = refiner.Refine(mask, GuideWithEdge(64, 64, 32), {64, 64});

    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(out, &minValue, &maxValue);
    EXPECT_GE(minValue, 0.0);
    EXPECT_LE(maxValue, 1.0);
    // Deleting the clamp must fail this test, so assert it actually bit.
    EXPECT_DOUBLE_EQ(maxValue, 1.0);
    EXPECT_DOUBLE_EQ(minValue, 0.0);
}

TEST(EdgeRefiner, ClampsANegativeRadiusToZeroRatherThanHandingItToTheFilter) {
    wbc::EdgeRefiner refiner{-5};
    EXPECT_EQ(refiner.Radius(), 0);

    refiner.SetRadius(6);
    EXPECT_EQ(refiner.Radius(), 6);

    refiner.SetRadius(-1);
    EXPECT_EQ(refiner.Radius(), 0);
}

TEST(EdgeRefiner, RadiusZeroUpscalesWithoutFiltering) {
    wbc::EdgeRefiner refiner{0};
    const cv::Mat mask = MaskWithEdge(16, 16, 8);
    const cv::Mat guide = GuideWithEdge(64, 64, 40);

    const cv::Mat out = refiner.Refine(mask, guide, {64, 64});

    ASSERT_EQ(out.size(), cv::Size(64, 64));
    EXPECT_EQ(out.type(), CV_32FC1);
    // A plain resize keeps the mask's own edge at x = 32 and ignores the
    // guide's at x = 40, which is exactly what "no edge work" means.
    EXPECT_LT(out.at<float>(32, 10), 0.1F);
    EXPECT_GT(out.at<float>(32, 54), 0.9F);
    double low = 0.0;
    double high = 0.0;
    cv::minMaxLoc(out, &low, &high);
    EXPECT_GE(low, 0.0);
    EXPECT_LE(high, 1.0);
}

// The pipeline shrinks the frame once and hands the refiner that copy as the
// guide. The refiner works at the guide's size and delivers the mask there;
// scaling it up to the frame is the compositor's job.

TEST(EdgeRefiner, WorksAtTheResolutionOfTheMaskItIsRefining) {
    // The model's 256x256 output on a 720p frame: the side that fills the
    // model's height sets the scale, so the filter sees no less detail than
    // the mask carries along either axis.
    EXPECT_EQ(wbc::EdgeRefiner::WorkingSize({256, 256}, {1280, 720}), cv::Size(455, 256));
    EXPECT_EQ(wbc::EdgeRefiner::WorkingSize({64, 64}, {256, 256}), cv::Size(64, 64));
    // A mask already as large as the output has nothing to shrink to.
    EXPECT_EQ(wbc::EdgeRefiner::WorkingSize({256, 256}, {200, 100}), cv::Size(200, 100));
    // Shrunk below 32 pixels the filter's window would cover the image.
    EXPECT_EQ(wbc::EdgeRefiner::WorkingSize({16, 16}, {64, 64}), cv::Size(64, 64));
}

TEST(EdgeRefiner, DeliversTheMaskAtTheGuidesSizeWhenTheGuideIsShrunk) {
    const wbc::EdgeRefiner refiner{8};
    const cv::Mat out = refiner.Refine(MaskWithEdge(16, 16, 8), GuideWithEdge(64, 64, 32), {256, 256});
    EXPECT_EQ(out.size(), cv::Size(64, 64));
    EXPECT_EQ(out.type(), CV_32FC1);
}

// The radius is in the frame's pixels, which is what the user set it in, so
// on a guide a quarter of the frame's size the filter's window is a quarter
// as wide.
TEST(EdgeRefiner, ScalesTheRadiusFromTheFrameToTheGuide) {
    const cv::Mat mask = MaskWithEdge(64, 64, 32);
    const cv::Mat guide = GuideWithEdge(64, 64, 36);

    const cv::Mat onShrunkGuide = wbc::EdgeRefiner{8}.Refine(mask, guide, {256, 256});
    const cv::Mat atGuideScale = wbc::EdgeRefiner{2}.Refine(mask, guide, {64, 64});
    const cv::Mat unscaled = wbc::EdgeRefiner{8}.Refine(mask, guide, {64, 64});

    EXPECT_EQ(cv::norm(onShrunkGuide, atGuideScale, cv::NORM_INF), 0.0);
    EXPECT_GT(cv::norm(onShrunkGuide, unscaled, cv::NORM_INF), 0.01);
}

TEST(EdgeRefiner, PullsTheMaskOntoAShrunkGuide) {
    const wbc::EdgeRefiner refiner{8};
    // A 256x256 frame shrunk to the mask's 64x64. A guided filter aligns a
    // boundary within its own window, so the two edges are a window apart
    // rather than across the frame.
    const cv::Mat mask = MaskWithEdge(64, 64, 32);
    const cv::Mat guide = GuideWithEdge(64, 64, 35);

    const cv::Mat out = refiner.Refine(mask, guide, {256, 256});

    // Between the mask's edge and the guide's the mask says "fully
    // foreground". The filter has to pull that down towards the guide.
    EXPECT_FLOAT_EQ(mask.at<float>(32, 33), 1.0F);
    EXPECT_LT(out.at<float>(32, 33), 0.9F) << "boundary did not move towards the guide";
    // Far from either edge nothing should have changed.
    EXPECT_GT(out.at<float>(32, 55), 0.9F);
    EXPECT_LT(out.at<float>(32, 5), 0.1F);
}
