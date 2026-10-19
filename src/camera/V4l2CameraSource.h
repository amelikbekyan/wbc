// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include <opencv2/videoio.hpp>

#include "camera/ICameraSource.h"

namespace wbc {

// Asks the device what it supports. Declared here rather than in a file of its
// own because its only purpose is to feed Open.
std::vector<FormatOption> QuerySupportedFormats(const std::string& devicePath);

// Captures through OpenCV's V4L2 backend, which unpacks MJPEG for us. The
// format is negotiated against what the device reports rather than requested
// blindly, so that the size the pipeline is configured for is the size the
// frames actually arrive in.
class V4l2CameraSource final : public ICameraSource {
public:
    bool Open(const std::string& path, const FormatRequest& request) override;
    bool IsOpen() const override;
    bool Read(cv::Mat& frameBgr) override;
    void Close() override;
    FormatOption ActualFormat() const override;
    std::string LastError() const override;

private:
    cv::VideoCapture capture_;
    FormatOption actual_{};
    std::string lastError_;
};

}  // namespace wbc
