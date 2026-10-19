// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/OnnxPreprocess.h"

#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace wbc {

std::array<std::int64_t, 4> PreprocessShape(cv::Size inputSize) {
    return {1, 3, inputSize.height, inputSize.width};
}

std::vector<float> Preprocess(const cv::Mat& bgr, cv::Size inputSize) {
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        throw std::invalid_argument("Preprocess: expected a non-empty 8 bit three channel BGR frame");
    }
    if (inputSize.width <= 0 || inputSize.height <= 0) {
        throw std::invalid_argument("Preprocess: input size must have positive dimensions");
    }

    cv::Mat resized;
    if (bgr.size() == inputSize) {
        resized = bgr;
    } else {
        cv::resize(bgr, resized, inputSize, 0, 0, cv::INTER_AREA);
    }

    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    cv::Mat scaled;
    rgb.convertTo(scaled, CV_32FC3, 1.0 / 255.0);

    // ONNX wants the channels as separate planes; OpenCV stores them
    // interleaved, so split straight into the destination buffer.
    std::vector<float> tensor(static_cast<std::size_t>(3) * inputSize.area());
    const std::size_t plane = static_cast<std::size_t>(inputSize.area());
    std::array<cv::Mat, 3> planes{
        cv::Mat(inputSize, CV_32FC1, tensor.data()),
        cv::Mat(inputSize, CV_32FC1, tensor.data() + plane),
        cv::Mat(inputSize, CV_32FC1, tensor.data() + 2 * plane),
    };
    cv::split(scaled, planes.data());
    return tensor;
}

}  // namespace wbc
