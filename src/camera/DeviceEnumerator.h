// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <linux/videodev2.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wbc {

struct DeviceInfo {
    std::string path;
    std::string name;
    bool canCapture = false;
    bool canOutput = false;
};

// Interpreting a capability structure is separated from obtaining one so that
// the interpretation, which is where the mistakes live, can be tested against
// structures built by hand.
DeviceInfo DescribeDevice(const v4l2_capability& capability, std::string path);

// Capture nodes the user may select, with our own loopback removed: under
// exclusive_caps it presents a capture node once something writes to it, and
// offering that back as an input would close a feedback loop. Our loopback
// is the node named `excludeName`.
std::vector<DeviceInfo> SelectCaptureDevices(const std::vector<DeviceInfo>& all,
                                             std::string_view excludeName);

// The first output node carrying `label`.
std::optional<DeviceInfo> SelectLoopback(const std::vector<DeviceInfo>& all,
                                         std::string_view label);

// Video nodes in numeric order; /dev/video10 sorts after /dev/video2, which
// lexicographic order gets wrong.
std::vector<std::string> ListVideoDevicePaths(const std::string& directory);

// Opens every video node and asks it what it is. Nodes that cannot be opened
// are skipped rather than reported: another application holding a camera is
// normal.
std::vector<DeviceInfo> EnumerateDevices(const std::string& directory = "/dev");

}  // namespace wbc
