// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camera/V4l2CameraSource.h"

#include <QCoreApplication>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "camera/FormatNegotiator.h"

namespace wbc {
namespace {

// V4L2 reports frame rates as intervals; a zero denominator would be a broken
// driver, and dividing by it is worse than ignoring the entry.
double IntervalToFps(const v4l2_fract& interval) {
    if (interval.numerator == 0) {
        return 0.0;
    }
    return static_cast<double>(interval.denominator) / static_cast<double>(interval.numerator);
}

void CollectIntervals(int fd, std::uint32_t fourcc, std::uint32_t width, std::uint32_t height,
                      std::vector<FormatOption>& out) {
    v4l2_frmivalenum interval{};
    interval.pixel_format = fourcc;
    interval.width = width;
    interval.height = height;

    bool any = false;
    for (interval.index = 0; ::ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &interval) == 0; ++interval.index) {
        if (interval.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
            break;
        }
        out.push_back({fourcc, static_cast<int>(width), static_cast<int>(height),
                       IntervalToFps(interval.discrete)});
        any = true;
    }

    // A driver that reports no discrete intervals still offers the size.
    if (!any) {
        out.push_back({fourcc, static_cast<int>(width), static_cast<int>(height), 0.0});
    }
}

}  // namespace

std::vector<FormatOption> QuerySupportedFormats(const std::string& devicePath) {
    const int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return {};
    }

    std::vector<FormatOption> formats;
    v4l2_fmtdesc description{};
    description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (description.index = 0; ::ioctl(fd, VIDIOC_ENUM_FMT, &description) == 0; ++description.index) {
        v4l2_frmsizeenum size{};
        size.pixel_format = description.pixelformat;
        for (size.index = 0; ::ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0; ++size.index) {
            if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
                break;
            }
            CollectIntervals(fd, description.pixelformat, size.discrete.width, size.discrete.height, formats);
        }
    }

    ::close(fd);
    return formats;
}

bool V4l2CameraSource::Open(const std::string& path, const FormatRequest& request) {
    Close();

    FormatRequest effective = request;
    if (effective.fourccPreference.empty()) {
        // MJPEG first: at 720p a USB 2.0 camera cannot sustain thirty frames a
        // second in raw YUYV, so asking for YUYV first silently halves the rate.
        effective.fourccPreference = {kFourccMjpg, kFourccYuyv};
    }

    const auto supported = QuerySupportedFormats(path);
    const auto chosen = Negotiate(supported, effective);
    if (!chosen.has_value()) {
        lastError_ = QCoreApplication::translate("V4l2CameraSource",
                                                 "The device offered no usable capture format: %1")
                         .arg(QString::fromStdString(path))
                         .toStdString();
        return false;
    }

    if (!capture_.open(path, cv::CAP_V4L2)) {
        lastError_ =
            QCoreApplication::translate("V4l2CameraSource",
                                        "Could not open %1; it may be in use by another application")
                .arg(QString::fromStdString(path))
                .toStdString();
        return false;
    }

    capture_.set(cv::CAP_PROP_FOURCC, static_cast<double>(chosen->fourcc));
    capture_.set(cv::CAP_PROP_FRAME_WIDTH, chosen->width);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, chosen->height);
    if (chosen->fps > 0.0) {
        capture_.set(cv::CAP_PROP_FPS, chosen->fps);
    }

    // What was asked for and what was granted are not always the same, and
    // every buffer downstream is sized from this.
    actual_.fourcc = static_cast<std::uint32_t>(capture_.get(cv::CAP_PROP_FOURCC));
    actual_.width = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
    actual_.height = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
    actual_.fps = capture_.get(cv::CAP_PROP_FPS);

    lastError_.clear();
    return true;
}

bool V4l2CameraSource::IsOpen() const {
    return capture_.isOpened();
}

bool V4l2CameraSource::Read(cv::Mat& frameBgr) {
    if (!capture_.isOpened()) {
        lastError_ =
            QCoreApplication::translate("V4l2CameraSource", "The camera is not open").toStdString();
        return false;
    }
    if (!capture_.read(frameBgr) || frameBgr.empty()) {
        lastError_ = QCoreApplication::translate(
                         "V4l2CameraSource",
                         "The camera stopped delivering frames; it may have been disconnected")
                         .toStdString();
        return false;
    }
    return true;
}

void V4l2CameraSource::Close() {
    if (capture_.isOpened()) {
        capture_.release();
    }
    actual_ = {};
}

FormatOption V4l2CameraSource::ActualFormat() const {
    return actual_;
}

std::string V4l2CameraSource::LastError() const {
    return lastError_;
}

}  // namespace wbc
