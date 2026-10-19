// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "core/Types.h"

namespace wbc {

// Where finished frames go.
//
// Write returns a result rather than a boolean because a refused frame is
// usually back pressure, which the pipeline should absorb, and only sometimes
// a dead device, which it must stop for.
class IFrameSink {
public:
    virtual ~IFrameSink() = default;

    virtual bool Open(const std::string& path, int width, int height, double fps) = 0;
    virtual bool IsOpen() const = 0;
    virtual WriteResult Write(const cv::Mat& frameBgr) = 0;
    virtual void Close() = 0;
    virtual std::string LastError() const = 0;
};

}  // namespace wbc
