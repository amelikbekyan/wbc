// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "fakes/FakeClock.h"
#include "seg/MaskRefiner.h"

namespace {

cv::Mat Mask(float value, int size = 16) {
    return cv::Mat(size, size, CV_32FC1, cv::Scalar{value});
}

cv::Mat Guide(int width, int height) {
    return cv::Mat(height, width, CV_8UC3, cv::Scalar{40, 40, 40});
}

}  // namespace

TEST(MaskRefiner, DeliversTheMaskAtTheFrameSize) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};
    const cv::Mat out = refiner.Refine(Mask(1.0F), Guide(128, 96), {128, 96});
    EXPECT_EQ(out.size(), cv::Size(128, 96));
    EXPECT_EQ(out.type(), CV_32FC1);
}

TEST(MaskRefiner, DeliversTheMaskAtTheGuidesSizeWhenTheGuideIsShrunk) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};
    const cv::Mat out = refiner.Refine(Mask(1.0F), Guide(64, 48), {256, 192});
    EXPECT_EQ(out.size(), cv::Size(64, 48));
    EXPECT_EQ(out.type(), CV_32FC1);
}

TEST(MaskRefiner, BlendsAcrossFramesThroughTheFacade) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock, 0.7F};

    refiner.Refine(Mask(1.0F), Guide(128, 96), {128, 96});
    clock.Advance(std::chrono::duration<double>(1.0 / 30.0));
    const cv::Mat out = refiner.Refine(Mask(0.0F), Guide(128, 96), {128, 96});

    // This does not prove the smoothing-then-refinement ORDER, but do not
    // conclude from that that the order is free. An earlier comment here
    // argued the two orders produce identical output because guidedFilter is
    // linear in its input for a fixed guide and resize is linear too. That
    // argument holds for ONE frame with a FIXED guide, and fails across
    // frames: MaskSmoother is stateful and the guide is a different frame
    // every time. Under the current order the carried mask is re-filtered
    // against the CURRENT frame's edges; under the reverse it would stay
    // attached to the previous frame's. Those differ whenever the scene
    // moves, which is always. The current order is the correct one — it is
    // also the cheaper one, since refining first would run the expensive
    // filter on a mask about to be blended away — and reversing it is a
    // behaviour change, not an optimisation.
    //
    // A flat guide leaves the smoothed value essentially intact, so the blend
    // is still observable through the facade.
    EXPECT_NEAR(out.at<float>(48, 64), 0.7F, 0.05F);
}

TEST(MaskRefiner, ResetClearsTheSmoothingHistory) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock, 0.7F};

    refiner.Refine(Mask(1.0F), Guide(128, 96), {128, 96});
    refiner.Reset();
    clock.Advance(std::chrono::duration<double>(1.0 / 30.0));
    const cv::Mat out = refiner.Refine(Mask(0.0F), Guide(128, 96), {128, 96});

    EXPECT_NEAR(out.at<float>(48, 64), 0.0F, 0.05F);
}

TEST(MaskRefiner, ExposesItsSmootherSoTheAlphaCanBeTuned) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock, 0.7F};
    refiner.Smoother().SetAlpha(0.2F);
    EXPECT_FLOAT_EQ(refiner.Smoother().Alpha(), 0.2F);
}

TEST(MaskRefiner, DefaultsToTheValuesThePipelineUsedBeforeAnyOfThisWasAdjustable) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};

    const wbc::MaskSettings settings = refiner.Settings();
    EXPECT_FLOAT_EQ(settings.threshold, 0.0F);
    EXPECT_FLOAT_EQ(settings.smoothing, 0.7F);
    EXPECT_EQ(settings.edgeRadius, 8);
}

TEST(MaskRefiner, ApplyReachesTheSmootherAndTheEdgeFilter) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};

    wbc::MaskSettings settings;
    settings.threshold = 0.25F;
    settings.smoothing = 0.1F;
    settings.edgeRadius = 3;
    refiner.Apply(settings);

    EXPECT_FLOAT_EQ(refiner.Settings().threshold, 0.25F);
    EXPECT_FLOAT_EQ(refiner.Smoother().Alpha(), 0.1F);
    EXPECT_EQ(refiner.Settings().edgeRadius, 3);
}

TEST(MaskRefiner, ClampsSettingsThatCameFromAnEditedFile) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};

    wbc::MaskSettings settings;
    settings.threshold = 4.0F;
    settings.smoothing = -1.0F;
    settings.edgeRadius = 500;
    refiner.Apply(settings);

    EXPECT_FLOAT_EQ(refiner.Settings().threshold, wbc::kMaxMaskThreshold);
    EXPECT_FLOAT_EQ(refiner.Smoother().Alpha(), 0.0F);
    EXPECT_EQ(refiner.Settings().edgeRadius, wbc::kMaxMaskEdgeRadius);
}

TEST(MaskRefiner, AThresholdSendsWeakAlphaToTheBackgroundAndKeepsStrongAlphaOpaque) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};

    wbc::MaskSettings settings;
    settings.threshold = 0.5F;
    settings.smoothing = 0.0F;
    settings.edgeRadius = 0;
    refiner.Apply(settings);

    // The faint alpha the model leaves on furniture.
    const cv::Mat weak = refiner.Refine(Mask(0.3F), Guide(32, 32), {32, 32});
    EXPECT_FLOAT_EQ(weak.at<float>(16, 16), 0.0F);

    // The person, who must stay fully opaque rather than turn translucent as
    // the threshold rises.
    const cv::Mat strong = refiner.Refine(Mask(1.0F), Guide(32, 32), {32, 32});
    EXPECT_FLOAT_EQ(strong.at<float>(16, 16), 1.0F);

    // Halfway between threshold and full: stretched back over the range.
    const cv::Mat middle = refiner.Refine(Mask(0.75F), Guide(32, 32), {32, 32});
    EXPECT_NEAR(middle.at<float>(16, 16), 0.5F, 1e-5F);
}

TEST(MaskRefiner, ThresholdZeroLeavesTheMaskExactlyAsTheEdgeFilterProducedIt) {
    wbc::FakeClock clock;
    wbc::MaskRefiner refiner{clock};

    wbc::MaskSettings settings;
    settings.threshold = 0.0F;
    settings.smoothing = 0.0F;
    settings.edgeRadius = 0;
    refiner.Apply(settings);

    const cv::Mat out = refiner.Refine(Mask(0.3F), Guide(32, 32), {32, 32});
    EXPECT_FLOAT_EQ(out.at<float>(16, 16), 0.3F);
}
