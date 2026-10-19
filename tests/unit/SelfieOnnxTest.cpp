// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include <stdexcept>

#include "core/FileModelLoader.h"
#include "fakes/FakeModelLoader.h"
#include "seg/SelfieOnnx.h"

namespace {

wbc::SelfieOnnx MakeSegmenter() {
    static const wbc::FileModelLoader loader;
    return wbc::SelfieOnnx{loader, WBC_MODEL_PATH};
}

}  // namespace

TEST(SelfieOnnx, ReportsAUsableInputSize) {
    auto segmenter = MakeSegmenter();
    const cv::Size size = segmenter.InputSize();
    EXPECT_GT(size.width, 0);
    EXPECT_GT(size.height, 0);
}

TEST(SelfieOnnx, ReturnsASingleChannelFloatMaskAtTheModelResolution) {
    auto segmenter = MakeSegmenter();
    const cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar{120, 120, 120});

    const cv::Mat mask = segmenter.Segment(frame);
    EXPECT_EQ(mask.type(), CV_32FC1);
    EXPECT_EQ(mask.size(), segmenter.InputSize());
}

TEST(SelfieOnnx, ProducesValuesInsideTheUnitRangeBecauseTheModelEndsInASigmoid) {
    auto segmenter = MakeSegmenter();
    const cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar{90, 110, 130});

    const cv::Mat mask = segmenter.Segment(frame);
    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(mask, &minValue, &maxValue);
    EXPECT_GE(minValue, 0.0);
    EXPECT_LE(maxValue, 1.0);
}

TEST(SelfieOnnx, RepeatedInferenceOnTheSameFrameIsStable) {
    auto segmenter = MakeSegmenter();
    const cv::Mat frame(480, 640, CV_8UC3, cv::Scalar{60, 80, 100});

    const cv::Mat first = segmenter.Segment(frame);
    const cv::Mat second = segmenter.Segment(frame);
    EXPECT_LT(cv::norm(first, second, cv::NORM_INF), 1e-5);
}

TEST(SelfieOnnx, RejectsAnEmptyFrame) {
    auto segmenter = MakeSegmenter();
    EXPECT_THROW(segmenter.Segment(cv::Mat{}), std::invalid_argument);
}

TEST(SelfieOnnx, FailsToConstructWhenTheModelCannotBeRead) {
    const wbc::FileModelLoader loader;
    EXPECT_THROW((wbc::SelfieOnnx{loader, "/nonexistent/model.onnx"}), std::runtime_error);
}

TEST(SelfieOnnx, FailsToConstructWhenTheBytesAreNotAModel) {
    wbc::FakeModelLoader loader;
    loader.SetBytes(std::vector<std::uint8_t>(128, 0x41));
    EXPECT_THROW((wbc::SelfieOnnx{loader, "irrelevant.onnx"}), std::runtime_error);
}

// An idle intra-op worker spins by default, waiting for the next inference in
// a busy loop. The model runs for a few milliseconds of every 33 ms frame, so
// the spin kept a whole core busy for no work at all.
TEST(SelfieOnnx, SessionWorkersSleepInsteadOfSpinningBetweenFrames) {
    const Ort::SessionOptions options = wbc::MakeSessionOptions(2);
    ASSERT_TRUE(options.HasConfigEntry("session.intra_op.allow_spinning"));
    EXPECT_EQ(options.GetConfigEntry("session.intra_op.allow_spinning"), "0");
}
