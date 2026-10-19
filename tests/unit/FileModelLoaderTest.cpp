// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "core/FileModelLoader.h"

TEST(FileModelLoader, ReadsTheDownloadedModelWhole) {
    const wbc::FileModelLoader loader;
    const auto bytes = loader.Load(WBC_MODEL_PATH);
    EXPECT_EQ(bytes.size(), 462352u);
}

TEST(FileModelLoader, ReportsAMissingFileByPath) {
    const wbc::FileModelLoader loader;
    try {
        loader.Load("/nonexistent/model.onnx");
        FAIL() << "expected a runtime_error";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("/nonexistent/model.onnx"), std::string::npos);
    }
}

TEST(FileModelLoader, RefusesAnEmptyFileRatherThanHandingOutZeroBytes) {
    const auto path = (std::filesystem::temp_directory_path() / "wbc_empty_model.onnx").string();
    std::filesystem::remove(path);
    { std::ofstream file(path); }

    const wbc::FileModelLoader loader;
    EXPECT_THROW(loader.Load(path), std::runtime_error);
}
