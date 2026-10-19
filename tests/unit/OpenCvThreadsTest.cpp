// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "core/OpenCvThreads.h"

TEST(OpenCvThreads, LimitsOpenCvToItsOwnSmallPool) {
    const int previous = cv::getNumThreads();

    wbc::LimitOpenCvThreads();
    const int limited = cv::getNumThreads();

    // Shared by every test in this binary, so it is put back first.
    cv::setNumThreads(previous);
    EXPECT_EQ(limited, wbc::kOpenCvThreads);
}
