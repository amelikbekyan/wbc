// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camera/DeviceEnumerator.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace wbc {
namespace {

std::optional<int> VideoNodeNumber(const std::string& filename) {
    constexpr std::string_view prefix = "video";
    if (filename.size() <= prefix.size() || filename.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    const std::string digits = filename.substr(prefix.size());
    if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
        return std::nullopt;
    }
    // The digit check bounds the character class, not the magnitude. A name
    // like video99999999999999 is all digits and still overflows int.
    try {
        return std::stoi(digits);
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

}  // namespace

DeviceInfo DescribeDevice(const v4l2_capability& capability, std::string path) {
    // device_caps describes this node; capabilities describes everything the
    // driver exposes. Reading the latter is how an output-only node ends up
    // offered as a camera.
    const std::uint32_t caps = (capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0
                                   ? capability.device_caps
                                   : capability.capabilities;

    DeviceInfo info;
    info.path = std::move(path);
    // card is a fixed 32-byte field with no NUL-termination contract, so the
    // name ends at the first NUL or at the end of the field, whichever comes
    // first. std::find keeps this to standard C++; strnlen is POSIX, which
    // <cstring> does not actually guarantee.
    const char* card = reinterpret_cast<const char*>(capability.card);
    const char* end = std::find(card, card + sizeof(capability.card), '\0');
    info.name.assign(card, end);
    info.canCapture = (caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
    info.canOutput = (caps & V4L2_CAP_VIDEO_OUTPUT) != 0;
    return info;
}

std::vector<DeviceInfo> SelectCaptureDevices(const std::vector<DeviceInfo>& all,
                                             std::string_view excludeName) {
    std::vector<DeviceInfo> capture;
    for (const DeviceInfo& info : all) {
        if (info.canCapture && info.name != excludeName) {
            capture.push_back(info);
        }
    }
    return capture;
}

std::optional<DeviceInfo> SelectLoopback(const std::vector<DeviceInfo>& all,
                                         std::string_view label) {
    for (const DeviceInfo& info : all) {
        if (info.canOutput && info.name == label) {
            return info;
        }
    }
    return std::nullopt;
}

std::vector<std::string> ListVideoDevicePaths(const std::string& directory) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    if (error) {
        return {};
    }

    std::vector<std::pair<int, std::string>> nodes;
    for (const auto& entry : iterator) {
        const std::string filename = entry.path().filename().string();
        if (const auto number = VideoNodeNumber(filename)) {
            nodes.emplace_back(*number, entry.path().string());
        }
    }

    std::sort(nodes.begin(), nodes.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });

    std::vector<std::string> paths;
    paths.reserve(nodes.size());
    for (auto& node : nodes) {
        paths.push_back(std::move(node.second));
    }
    return paths;
}

std::vector<DeviceInfo> EnumerateDevices(const std::string& directory) {
    std::vector<DeviceInfo> devices;
    for (const std::string& path : ListVideoDevicePaths(directory)) {
        const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        v4l2_capability capability{};
        if (::ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0) {
            devices.push_back(DescribeDevice(capability, path));
        }
        ::close(fd);
    }
    return devices;
}

}  // namespace wbc
