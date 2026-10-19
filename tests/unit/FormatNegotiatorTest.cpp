// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "camera/FormatNegotiator.h"

namespace {

wbc::FormatRequest Request(int width, int height, double fps, std::vector<std::uint32_t> preference) {
    wbc::FormatRequest request;
    request.width = width;
    request.height = height;
    request.fps = fps;
    request.fourccPreference = std::move(preference);
    return request;
}

}  // namespace

TEST(Negotiate, PicksTheExactMatchWhenItIsOffered) {
    const std::vector<wbc::FormatOption> supported{
        {wbc::kFourccYuyv, 640, 480, 30.0},
        {wbc::kFourccMjpg, 1280, 720, 30.0},
    };
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg, wbc::kFourccYuyv}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->fourcc, wbc::kFourccMjpg);
    EXPECT_EQ(chosen->width, 1280);
}

TEST(Negotiate, HonoursThePreferenceOrderWhenBothSizesExist) {
    const std::vector<wbc::FormatOption> supported{
        {wbc::kFourccMjpg, 1280, 720, 30.0},
        {wbc::kFourccYuyv, 1280, 720, 30.0},
    };
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccYuyv, wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->fourcc, wbc::kFourccYuyv);
}

TEST(Negotiate, FallsBackToTheLargestSizeThatStillFitsTheRequest) {
    const std::vector<wbc::FormatOption> supported{
        {wbc::kFourccMjpg, 640, 480, 30.0},
        {wbc::kFourccMjpg, 1024, 576, 30.0},
        {wbc::kFourccMjpg, 1920, 1080, 30.0},
    };
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->width, 1024);
    EXPECT_EQ(chosen->height, 576);
}

TEST(Negotiate, AcceptsALargerSizeOnlyWhenNothingSmallerIsOffered) {
    const std::vector<wbc::FormatOption> supported{{wbc::kFourccMjpg, 1920, 1080, 30.0}};
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->width, 1920);
}

TEST(Negotiate, AcceptsAnUnpreferredFourccRatherThanFailing) {
    const std::vector<wbc::FormatOption> supported{{wbc::kFourccYuyv, 1280, 720, 30.0}};
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->fourcc, wbc::kFourccYuyv);
}

TEST(Negotiate, PrefersTheClosestFrameRateAmongOtherwiseEqualFormats) {
    const std::vector<wbc::FormatOption> supported{
        {wbc::kFourccMjpg, 1280, 720, 15.0},
        {wbc::kFourccMjpg, 1280, 720, 30.0},
        {wbc::kFourccMjpg, 1280, 720, 60.0},
    };
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_DOUBLE_EQ(chosen->fps, 30.0);
}

TEST(Negotiate, ReturnsNothingWhenTheDeviceOffersNothing) {
    EXPECT_FALSE(wbc::Negotiate({}, Request(1280, 720, 30.0, {wbc::kFourccMjpg})).has_value());
}

TEST(Negotiate, NeverInventsAFormatTheDeviceDidNotOffer) {
    const std::vector<wbc::FormatOption> supported{{wbc::kFourccYuyv, 320, 240, 15.0}};
    const auto chosen = wbc::Negotiate(supported, Request(1280, 720, 30.0, {wbc::kFourccMjpg}));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->width, 320);
    EXPECT_EQ(chosen->height, 240);
    EXPECT_DOUBLE_EQ(chosen->fps, 15.0);
}
