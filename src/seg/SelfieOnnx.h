// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include <onnxruntime_cxx_api.h>

#include "core/IModelLoader.h"
#include "seg/ISegmenter.h"

namespace wbc {

// The session options SelfieOnnx runs with. Separate so that what they ask of
// ONNX Runtime can be checked without loading a model.
Ort::SessionOptions MakeSessionOptions(int threads);

// Runs MediaPipe Selfie Segmentation through ONNX Runtime on the CPU.
//
// The model is fed from memory rather than by path so that the loader can be
// replaced in tests, and the input size is taken from the session rather than
// hardcoded so that swapping the model for another one does not silently
// produce a stretched mask.
//
// One thread by default: a second ran the model in 5.3 ms instead of 7.4, but
// spent the 2 ms it saved on the processor and more in handing work over,
// and the frame fits its 33 ms without it.
class SelfieOnnx final : public ISegmenter {
public:
    SelfieOnnx(const IModelLoader& loader,
               const std::string& modelPath,
               int threads = 1,
               cv::Size fallbackInputSize = {256, 256});

    cv::Mat Segment(const cv::Mat& frameBgr) override;
    cv::Size InputSize() const override;

private:
    Ort::Env env_;
    Ort::SessionOptions options_;
    Ort::Session session_;
    Ort::MemoryInfo memoryInfo_;
    std::string inputName_;
    std::string outputName_;
    cv::Size inputSize_;
};

}  // namespace wbc
