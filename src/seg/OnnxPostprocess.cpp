// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "seg/OnnxPostprocess.h"

#include <stdexcept>

namespace wbc {
namespace {

struct Spatial {
    int height;
    int width;
};

Spatial ReadSpatial(const std::vector<std::int64_t>& shape) {
    if (shape.size() == 3) {
        // {1, H, W}
        if (shape[0] != 1) {
            throw std::invalid_argument("Postprocess: only a batch of one is supported");
        }
        return {static_cast<int>(shape[1]), static_cast<int>(shape[2])};
    }
    if (shape.size() == 4) {
        if (shape[0] != 1) {
            throw std::invalid_argument("Postprocess: only a batch of one is supported");
        }
        // A shape such as {1,1,W,1} satisfies both accepted layouts at once, and
        // nothing in the tensor says which was meant. Picking one silently is the
        // transposition this function exists to prevent, so refuse instead.
        if (shape[1] == 1 && shape[3] == 1) {
            throw std::invalid_argument(
                "Postprocess: output shape is ambiguous — both the channel-first and "
                "channel-last layouts match, so the mask orientation cannot be determined");
        }
        if (shape[1] == 1) {
            return {static_cast<int>(shape[2]), static_cast<int>(shape[3])};  // {1, 1, H, W}
        }
        if (shape[3] == 1) {
            return {static_cast<int>(shape[1]), static_cast<int>(shape[2])};  // {1, H, W, 1}
        }
        throw std::invalid_argument("Postprocess: expected exactly one mask channel");
    }
    throw std::invalid_argument("Postprocess: expected a three or four dimensional output");
}

}  // namespace

cv::Mat Postprocess(const float* data, const std::vector<std::int64_t>& shape) {
    if (data == nullptr) {
        throw std::invalid_argument("Postprocess: null output buffer");
    }

    const Spatial spatial = ReadSpatial(shape);
    if (spatial.height <= 0 || spatial.width <= 0) {
        throw std::invalid_argument("Postprocess: output has a zero dimension");
    }

    // Both accepted layouts are row major over a single channel, so the bytes
    // are identical and only the shape differs. The result is cloned because
    // the tensor belongs to ONNX Runtime and is recycled on the next inference.
    const cv::Mat view(spatial.height, spatial.width, CV_32FC1, const_cast<float*>(data));
    return view.clone();
}

}  // namespace wbc
