// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <opencv2/core.hpp>

namespace wbc {

// The tensor shape the model expects for one frame: batch, channels, height,
// width.
std::array<std::int64_t, 4> PreprocessShape(cv::Size inputSize);

// Turns a BGR frame into the planar RGB float tensor the model takes: resized
// to the model's own input size, channels reordered, values scaled to [0, 1].
std::vector<float> Preprocess(const cv::Mat& bgr, cv::Size inputSize);

}  // namespace wbc
