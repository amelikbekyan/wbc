// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sink/V4l2LoopbackSink.h"

#include <QCoreApplication>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "sink/I420Converter.h"

namespace wbc {

V4l2LoopbackSink::~V4l2LoopbackSink() {
    Close();
}

bool V4l2LoopbackSink::Open(const std::string& path, int width, int height, double fps) {
    Close();

    fd_ = ::open(path.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd_ < 0) {
        lastError_ = QCoreApplication::translate("V4l2LoopbackSink",
                                                 "Could not open the virtual camera %1: %2")
                         .arg(QString::fromStdString(path), QString::fromUtf8(std::strerror(errno)))
                         .toStdString();
        return false;
    }

    v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = static_cast<std::uint32_t>(width);
    format.fmt.pix.height = static_cast<std::uint32_t>(height);
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    format.fmt.pix.bytesperline = static_cast<std::uint32_t>(width);
    format.fmt.pix.sizeimage = static_cast<std::uint32_t>(width * height * 3 / 2);
    format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    if (::ioctl(fd_, VIDIOC_S_FMT, &format) < 0) {
        lastError_ = QCoreApplication::translate("V4l2LoopbackSink",
                                                 "The virtual camera refused the format: %1")
                         .arg(QString::fromUtf8(std::strerror(errno)))
                         .toStdString();
        Close();
        return false;
    }

    v4l2_streamparm parameters{};
    parameters.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parameters.parm.output.timeperframe.numerator = 1;
    parameters.parm.output.timeperframe.denominator =
        fps > 0.0 ? static_cast<std::uint32_t>(fps) : 30U;
    // A driver that will not take a frame rate still works; the rate is
    // advisory for a loopback.
    ::ioctl(fd_, VIDIOC_S_PARM, &parameters);

    width_ = width;
    height_ = height;
    lastError_.clear();
    return true;
}

bool V4l2LoopbackSink::IsOpen() const {
    return fd_ >= 0;
}

WriteResult V4l2LoopbackSink::Write(const cv::Mat& frameBgr) {
    if (fd_ < 0) {
        lastError_ = QCoreApplication::translate("V4l2LoopbackSink", "The virtual camera is not open")
                         .toStdString();
        return WriteResult::Fatal;
    }
    if (frameBgr.cols != width_ || frameBgr.rows != height_) {
        lastError_ = QCoreApplication::translate(
                         "V4l2LoopbackSink",
                         "The frame does not match the geometry the virtual camera was opened with")
                         .toStdString();
        return WriteResult::Fatal;
    }

    cv::Mat i420;
    try {
        i420 = ToI420(frameBgr);
    } catch (const std::invalid_argument& error) {
        lastError_ = QCoreApplication::translate("V4l2LoopbackSink", "Could not convert the frame: %1")
                         .arg(QString::fromUtf8(error.what()))
                         .toStdString();
        return WriteResult::Fatal;
    }

    const std::size_t expected = static_cast<std::size_t>(width_) * height_ * 3 / 2;
    // EINTR means a signal arrived before a single byte moved, which says
    // nothing about the reader -- retrying delivers the frame. Counting it as
    // a drop threw away a frame that would have gone through and blurred the
    // dropped counter, whose entire meaning is "the reader is falling behind".
    ssize_t written = 0;
    do {
        written = ::write(fd_, i420.data, expected);
    } while (written < 0 && errno == EINTR);

    if (written == static_cast<ssize_t>(expected)) {
        return WriteResult::Ok;
    }

    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Nobody is reading fast enough. Dropping is correct for live video.
        return WriteResult::Dropped;
    }

    const QString reason =
        written < 0 ? QString::fromUtf8(std::strerror(errno))
                    : QCoreApplication::translate("V4l2LoopbackSink", "short write");
    lastError_ = QCoreApplication::translate("V4l2LoopbackSink",
                                             "Writing to the virtual camera failed: %1")
                     .arg(reason)
                     .toStdString();
    return WriteResult::Fatal;
}

void V4l2LoopbackSink::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    width_ = 0;
    height_ = 0;
}

std::string V4l2LoopbackSink::LastError() const {
    return lastError_;
}

}  // namespace wbc
