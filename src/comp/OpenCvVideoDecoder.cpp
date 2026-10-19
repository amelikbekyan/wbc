// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comp/OpenCvVideoDecoder.h"

#include <QCoreApplication>
#include <QString>

namespace wbc {

bool OpenCvVideoDecoder::Open(const std::string& path) {
    Close();
    if (!capture_.open(path)) {
        lastError_ = QCoreApplication::translate("OpenCvVideoDecoder",
                                                 "Could not open a video from %1")
                         .arg(QString::fromStdString(path))
                         .toStdString();
        return false;
    }
    path_ = path;
    lastError_.clear();
    return true;
}

bool OpenCvVideoDecoder::Read(cv::Mat& frame) {
    if (!capture_.read(frame) || frame.empty()) {
        lastError_ = QCoreApplication::translate("OpenCvVideoDecoder",
                                                 "Could not read a frame from %1")
                         .arg(QString::fromStdString(path_))
                         .toStdString();
        return false;
    }
    return true;
}

bool OpenCvVideoDecoder::Rewind() {
    // Seeking is not something every container supports, and a backend that
    // cannot seek says so by returning false. Reopening always works, so it
    // is the fallback rather than an error.
    if (capture_.set(cv::CAP_PROP_POS_FRAMES, 0)) {
        return true;
    }
    return capture_.open(path_);
}

double OpenCvVideoDecoder::Fps() const {
    return capture_.get(cv::CAP_PROP_FPS);
}

void OpenCvVideoDecoder::Close() {
    capture_.release();
}

std::string OpenCvVideoDecoder::LastError() const {
    return lastError_;
}

cv::Mat FirstFrameOf(const std::string& path) {
    OpenCvVideoDecoder decoder;
    cv::Mat frame;
    if (!decoder.Open(path) || !decoder.Read(frame)) {
        return {};
    }
    return frame;
}

}  // namespace wbc
