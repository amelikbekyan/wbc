// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "core/SteadyClock.h"
#include "fakes/FakeClock.h"

using namespace std::chrono_literals;

TEST(SteadyClock, NowDoesNotGoBackwards) {
    wbc::SteadyClock clock;
    const auto first = clock.Now();
    const auto second = clock.Now();
    EXPECT_GE(second, first);
}

TEST(FakeClock, StartsAtEpochOfSteadyClock) {
    wbc::FakeClock clock;
    EXPECT_EQ(clock.Now(), std::chrono::steady_clock::time_point{});
}

TEST(FakeClock, AdvanceMovesTimeForwardByExactlyTheGivenAmount) {
    wbc::FakeClock clock;
    const auto before = clock.Now();
    clock.Advance(250ms);
    const auto elapsed = std::chrono::duration<double>(clock.Now() - before).count();
    EXPECT_DOUBLE_EQ(elapsed, 0.25);
}

TEST(FakeClock, AdvanceAccumulates) {
    wbc::FakeClock clock;
    clock.Advance(100ms);
    clock.Advance(150ms);
    const auto elapsed = std::chrono::duration<double>(clock.Now() - std::chrono::steady_clock::time_point{}).count();
    EXPECT_DOUBLE_EQ(elapsed, 0.25);
}
