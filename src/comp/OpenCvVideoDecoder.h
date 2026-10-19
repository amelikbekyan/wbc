// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/videoio.hpp>

#include "comp/IVideoDecoder.h"

namespace wbc {

// Decodes through whatever backend OpenCV was built with for files, which on
// the distributions this project supports is FFmpeg: anything a media player
// on the same machine opens, this opens.
class OpenCvVideoDecoder final : public IVideoDecoder {
public:
    bool Open(const std::string& path) override;
    bool Read(cv::Mat& frame) override;
    bool Rewind() override;
    double Fps() const override;
    void Close() override;
    std::string LastError() const override;

private:
    cv::VideoCapture capture_;
    std::string path_;
    std::string lastError_;
};

// The first frame of a video file, for a thumbnail; empty when the file does
// not open or yields nothing.
cv::Mat FirstFrameOf(const std::string& path);

}  // namespace wbc
