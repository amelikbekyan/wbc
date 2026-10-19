// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/SelfieOnnx.h"

#include <stdexcept>

#include "seg/OnnxPostprocess.h"
#include "seg/OnnxPreprocess.h"

namespace wbc {

Ort::SessionOptions MakeSessionOptions(int threads) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(threads);
    // Between inferences an idle worker would otherwise spin, waiting for the
    // next one in a busy loop. Inference takes about 7 ms of each 33 ms frame,
    // so the spin kept most of a core busy doing nothing. Waking a sleeping
    // worker costs microseconds once per frame.
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    return options;
}

SelfieOnnx::SelfieOnnx(const IModelLoader& loader,
                       const std::string& modelPath,
                       int threads,
                       cv::Size fallbackInputSize)
    : env_(ORT_LOGGING_LEVEL_WARNING, "wbc"),
      options_(MakeSessionOptions(threads)),
      session_(nullptr),
      memoryInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    const std::vector<std::uint8_t> bytes = loader.Load(modelPath);

    // Ort::Exception does not derive from std::runtime_error, and callers
    // should not have to know about ONNX Runtime's hierarchy. Every ORT call
    // needed to bring the session up — not just session construction itself —
    // is covered by this one translating block, since an adversarial-but-
    // loadable model (e.g. zero declared inputs/outputs) can throw from the
    // name or shape lookups just as easily as from Ort::Session's constructor.
    try {
        session_ = Ort::Session(env_, bytes.data(), bytes.size(), options_);

        Ort::AllocatorWithDefaultOptions allocator;
        inputName_ = session_.GetInputNameAllocated(0, allocator).get();
        outputName_ = session_.GetOutputNameAllocated(0, allocator).get();

        const auto shape = session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        // A dynamic axis is reported as a negative extent; the model then
        // accepts whatever is offered, so a sensible default is used.
        const bool dynamic = shape.size() != 4 || shape[2] <= 0 || shape[3] <= 0;
        inputSize_ = dynamic ? fallbackInputSize
                             : cv::Size{static_cast<int>(shape[3]), static_cast<int>(shape[2])};
    } catch (const Ort::Exception& error) {
        throw std::runtime_error(std::string{"Could not load the segmentation model: "} + error.what());
    }
}

cv::Size SelfieOnnx::InputSize() const {
    return inputSize_;
}

cv::Mat SelfieOnnx::Segment(const cv::Mat& frameBgr) {
    // Preprocess's std::invalid_argument is the documented contract for a bad
    // frame and stays outside the translating block below, so it is not
    // relabelled as an inference failure.
    std::vector<float> input = Preprocess(frameBgr, inputSize_);
    const auto shape = PreprocessShape(inputSize_);

    const char* inputNames[] = {inputName_.c_str()};
    const char* outputNames[] = {outputName_.c_str()};

    try {
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo_, input.data(), input.size(), shape.data(), shape.size());

        std::vector<Ort::Value> outputs =
            session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

        if (outputs.empty()) {
            throw std::runtime_error("Inference produced no output");
        }

        const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
        return Postprocess(outputs.front().GetTensorData<float>(), outputShape);
    } catch (const Ort::Exception& error) {
        throw std::runtime_error(std::string{"Inference failed: "} + error.what());
    }
}

}  // namespace wbc
