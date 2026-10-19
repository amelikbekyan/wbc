// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include <opencv2/core.hpp>

namespace wbc {

// Turns the model's output tensor into a single channel float mask.
//
// The layout is read from the shape rather than assumed, because a converted
// model may emit the channel axis first or last, or omit it entirely, and
// guessing wrong produces a mask that is transposed rather than one that
// visibly fails.
cv::Mat Postprocess(const float* data, const std::vector<std::int64_t>& shape);

}  // namespace wbc
