// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>

#include <gtest/gtest.h>

#include <stdexcept>

#include "fakes/FakeClock.h"
#include "seg/MaskSmoother.h"

namespace {

constexpr double kNominal = 1.0 / 30.0;

cv::Mat Mask(float value, int size = 8) {
    return cv::Mat(size, size, CV_32FC1, cv::Scalar{value});
}

float Centre(const cv::Mat& mask) {
    return mask.at<float>(mask.rows / 2, mask.cols / 2);
}

}  // namespace

TEST(MaskSmoother, TheFirstMaskPassesThroughUnchanged) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock};
    EXPECT_FLOAT_EQ(Centre(smoother.Smooth(Mask(0.25F))), 0.25F);
}

TEST(MaskSmoother, AtTheNominalFrameIntervalTheWeightIsExactlyAlpha) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    clock.Advance(std::chrono::duration<double>(kNominal));
    const cv::Mat out = smoother.Smooth(Mask(0.0F));

    // 0.7 * 1.0 + 0.3 * 0.0
    EXPECT_NEAR(Centre(out), 0.7F, 1e-4);
}

TEST(MaskSmoother, ADoubledIntervalSquaresTheWeightSoOldFramesFadeFaster) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    clock.Advance(std::chrono::duration<double>(kNominal * 2.0));
    const cv::Mat out = smoother.Smooth(Mask(0.0F));

    // 0.7^2 = 0.49
    EXPECT_NEAR(Centre(out), 0.49F, 1e-4);
}

TEST(MaskSmoother, AHalvedIntervalKeepsMoreOfThePreviousMask) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    clock.Advance(std::chrono::duration<double>(kNominal * 0.5));
    const cv::Mat out = smoother.Smooth(Mask(0.0F));

    EXPECT_NEAR(Centre(out), std::pow(0.7F, 0.5F), 1e-4);
}

TEST(MaskSmoother, AZeroIntervalSkipsSmoothingAltogether) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    const cv::Mat out = smoother.Smooth(Mask(0.0F));
    EXPECT_FLOAT_EQ(Centre(out), 0.0F);
}

TEST(MaskSmoother, AnAlphaOfZeroDisablesSmoothing) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.0F, kNominal};

    smoother.Smooth(Mask(1.0F));
    clock.Advance(std::chrono::duration<double>(kNominal));
    EXPECT_FLOAT_EQ(Centre(smoother.Smooth(Mask(0.0F))), 0.0F);
}

TEST(MaskSmoother, RepeatedFramesConvergeTowardsTheNewValue) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    cv::Mat out;
    for (int i = 0; i < 20; ++i) {
        clock.Advance(std::chrono::duration<double>(kNominal));
        out = smoother.Smooth(Mask(0.0F));
    }
    EXPECT_LT(Centre(out), 0.01F);
}

TEST(MaskSmoother, AChangeOfMaskSizeStartsOverInsteadOfFailing) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F, 8));
    clock.Advance(std::chrono::duration<double>(kNominal));
    const cv::Mat out = smoother.Smooth(Mask(0.0F, 16));

    EXPECT_EQ(out.size(), cv::Size(16, 16));
    EXPECT_FLOAT_EQ(Centre(out), 0.0F);
}

TEST(MaskSmoother, ResetForgetsThePreviousMask) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock, 0.7F, kNominal};

    smoother.Smooth(Mask(1.0F));
    smoother.Reset();
    clock.Advance(std::chrono::duration<double>(kNominal));
    EXPECT_FLOAT_EQ(Centre(smoother.Smooth(Mask(0.0F))), 0.0F);
}

TEST(MaskSmoother, AlphaIsClampedToTheUnitRange) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock};
    smoother.SetAlpha(5.0F);
    EXPECT_FLOAT_EQ(smoother.Alpha(), 1.0F);
    smoother.SetAlpha(-1.0F);
    EXPECT_FLOAT_EQ(smoother.Alpha(), 0.0F);
}

TEST(MaskSmoother, RejectsAMaskOfTheWrongType) {
    wbc::FakeClock clock;
    wbc::MaskSmoother smoother{clock};
    const cv::Mat wrong(8, 8, CV_8UC1, cv::Scalar{255});
    EXPECT_THROW(smoother.Smooth(wrong), std::invalid_argument);
    EXPECT_THROW(smoother.Smooth(cv::Mat{}), std::invalid_argument);
}
