// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

namespace wbc {

// Returns a densely packed copy when `mat` has row padding, and `mat`
// itself when it does not.
//
// ToI420's result is handed straight to write() with a length computed
// from the geometry, so a padded buffer would put garbage between the rows
// on its way to the kernel.
cv::Mat EnsureContinuous(cv::Mat mat);

// Converts a BGR frame to planar I420, the format the loopback device is
// configured with. The result is a single channel matrix of height * 3 / 2
// rows, which is exactly the byte layout write() expects.
//
// Throws std::invalid_argument for input that cannot be represented: empty,
// not 8 bit three channel, or with an odd dimension, since I420 subsamples
// chroma by two in both directions.
cv::Mat ToI420(const cv::Mat& bgr);

}  // namespace wbc
