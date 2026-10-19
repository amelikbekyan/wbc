// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comp/BackgroundImage.h"

#include <QCoreApplication>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wbc {

cv::Mat CoverCrop(const cv::Mat& source, cv::Size target) {
    if (source.empty()) {
        throw std::invalid_argument("CoverCrop: empty source");
    }
    if (target.width <= 0 || target.height <= 0) {
        throw std::invalid_argument("CoverCrop: target must have positive dimensions");
    }

    // Cropping first and scaling second keeps the intermediate bounded by
    // the output. Scaling first would allocate the whole source at the
    // cover scale, which for an extreme aspect ratio is gigabytes.
    const double sourceAspect = static_cast<double>(source.cols) / source.rows;
    const double targetAspect = static_cast<double>(target.width) / target.height;

    cv::Rect region;
    if (sourceAspect > targetAspect) {
        // Source is too wide: keep full height, trim the sides.
        const int width = std::max(1, static_cast<int>(std::lround(source.rows * targetAspect)));
        region = cv::Rect((source.cols - width) / 2, 0, std::min(width, source.cols), source.rows);
    } else {
        // Source is too tall: keep full width, trim top and bottom.
        const int height = std::max(1, static_cast<int>(std::lround(source.cols / targetAspect)));
        region = cv::Rect(0, (source.rows - height) / 2, source.cols, std::min(height, source.rows));
    }

    // INTER_AREA is the right filter when shrinking and degenerates when
    // enlarging, which is what a small picture on a 720p frame does.
    const bool shrinking = region.width >= target.width && region.height >= target.height;
    cv::Mat out;
    cv::resize(source(region), out, target, 0, 0, shrinking ? cv::INTER_AREA : cv::INTER_LINEAR);
    return out;
}  // GCOVR_EXCL_LINE — gcc's exception-unwinding epilogue for the local
   // cv::Mat `out`, reached only if something throws past it. Nothing in
   // this function can throw after `out` is constructed.

bool BackgroundImage::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    const cv::Mat loaded = cv::imread(path, cv::IMREAD_COLOR);
    if (loaded.empty()) {
        lastError_ = QCoreApplication::translate("BackgroundImage",
                                                 "Could not read an image from %1")
                         .arg(QString::fromStdString(path))
                         .toStdString();
        return false;
    }
    original_ = loaded;
    cached_.release();
    cachedSize_ = cv::Size{0, 0};
    lastError_.clear();
    return true;
}

bool BackgroundImage::IsLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !original_.empty();
}

cv::Mat BackgroundImage::For(cv::Size target) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (original_.empty()) {
        return {};
    }
    // Re-cropping every frame would repeat the same resize thirty times a
    // second for a picture that never changes.
    if (cachedSize_ != target || cached_.empty()) {
        cached_ = CoverCrop(original_, target);
        cachedSize_ = target;
    }
    return cached_;
}

std::string BackgroundImage::LastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void BackgroundImage::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    original_.release();
    cached_.release();
    cachedSize_ = cv::Size{0, 0};
    lastError_.clear();
}

}  // namespace wbc
