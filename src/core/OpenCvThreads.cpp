// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/OpenCvThreads.h"

#include <opencv2/core.hpp>

namespace wbc {

void LimitOpenCvThreads() {
    cv::setNumThreads(kOpenCvThreads);
}

}  // namespace wbc
