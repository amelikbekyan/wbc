// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>

#include <opencv2/core.hpp>

namespace wbc {

// Converts a frame for display.
//
// The copy is deliberate: this function constructs a local cv::Mat and returns
// a QImage wrapping its buffer. Without the copy, the returned QImage would
// hold a dangling pointer to the freed local matrix. Returns a null image for
// anything that is not an 8 bit three channel BGR frame.
QImage ToQImage(const cv::Mat& bgr);

// Shrinks a BGR frame to `target` by area averaging, as cv::resize with
// INTER_AREA does, but in exact halves first while the frame is still at least
// twice the target. OpenCV averages an exact half on a fast path and any other
// ratio on a general one: at 1280x720, halving takes 0.4 ms and going straight
// to 455x256 took 3.7. The result differs from the direct resize by rounding.
// A frame already at the target size is handed back as it is, not copied.
// Throws std::invalid_argument for anything but a non-empty 8 bit three
// channel frame, or a target that is empty or larger than the frame.
cv::Mat ShrinkFrame(const cv::Mat& bgr, cv::Size target);

}  // namespace wbc
