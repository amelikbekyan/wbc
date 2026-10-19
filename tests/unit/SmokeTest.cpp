// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <filesystem>

TEST(BuildHarness, ModelFileWasDownloadedAndIsNotEmpty) {
    const std::filesystem::path model{WBC_MODEL_PATH};
    ASSERT_TRUE(std::filesystem::exists(model)) << "missing: " << model;
    EXPECT_EQ(std::filesystem::file_size(model), 462352u);
}
