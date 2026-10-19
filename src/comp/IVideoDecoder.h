// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

#include <string>

namespace wbc {

// A video file, read one frame at a time. An interface so that the player's
// timing, looping and failure handling can be driven by a test that says
// exactly which frame comes next and when reading stops working.
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    virtual bool Open(const std::string& path) = 0;
    // The next frame in BGR, or false at the end of the file or on failure.
    virtual bool Read(cv::Mat& frame) = 0;
    // Back to the first frame, for looping.
    virtual bool Rewind() = 0;
    // The file's own frame rate. Zero when the container does not say.
    virtual double Fps() const = 0;
    virtual void Close() = 0;
    virtual std::string LastError() const = 0;
};

}  // namespace wbc
