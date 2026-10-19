// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

#include <mutex>
#include <string>

namespace wbc {

// Scales the source so that it covers the target completely and crops the
// overflow from the centre. The alternative, stretching to fit, distorts faces
// in the background and looks wrong immediately.
cv::Mat CoverCrop(const cv::Mat& source, cv::Size target);

// Holds the user's chosen background picture and serves it at whatever size
// the current camera format needs.
class BackgroundImage {
public:
    // Returns false and records the reason rather than throwing: a bad path is
    // a normal thing for a user to pick, not an exceptional condition. On
    // failure the previously loaded image is kept.
    bool Load(const std::string& path);

    bool IsLoaded() const;

    // Returns an empty matrix when nothing is loaded. The returned matrix
    // shares the cache's buffer and must be treated as read-only.
    cv::Mat For(cv::Size target) const;

    std::string LastError() const;
    void Clear();

private:
    mutable std::mutex mutex_;
    cv::Mat original_;
    mutable cv::Mat cached_;
    mutable cv::Size cachedSize_{0, 0};
    std::string lastError_;
};

}  // namespace wbc
