// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "sink/IFrameSink.h"

namespace wbc {

// Writes frames into a v4l2loopback output node as planar I420.
//
// The device is opened non-blocking so that an application reading slowly
// cannot stall the pipeline; a full buffer comes back as EAGAIN and the frame
// is dropped, which is the right trade for live video.
class V4l2LoopbackSink final : public IFrameSink {
public:
    ~V4l2LoopbackSink() override;

    bool Open(const std::string& path, int width, int height, double fps) override;
    bool IsOpen() const override;
    WriteResult Write(const cv::Mat& frameBgr) override;
    void Close() override;
    std::string LastError() const override;

private:
    int fd_ = -1;
    int width_ = 0;
    int height_ = 0;
    std::string lastError_;
};

}  // namespace wbc
