// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "core/Types.h"

namespace wbc {

// A source of BGR frames.
//
// Failures are reported by return value with a readable reason rather than by
// exception: a camera being unplugged or taken by another application is
// ordinary, and the pipeline has to show the user what happened rather than
// unwind.
class ICameraSource {
public:
    virtual ~ICameraSource() = default;

    virtual bool Open(const std::string& path, const FormatRequest& request) = 0;
    virtual bool IsOpen() const = 0;
    virtual bool Read(cv::Mat& frameBgr) = 0;
    virtual void Close() = 0;
    virtual FormatOption ActualFormat() const = 0;
    virtual std::string LastError() const = 0;
};

}  // namespace wbc
