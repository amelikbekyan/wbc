// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "camera/DeviceEnumerator.h"
#include "core/Types.h"

namespace wbc {

// Hardware tests announce exactly which device they could not find, rather
// than passing quietly on a machine with no camera. A test that succeeds
// because nothing was there is worse than no test at all.
inline std::string FirstCaptureDevicePath() {
    const auto devices = SelectCaptureDevices(EnumerateDevices(), kVirtualCameraLabel);
    return devices.empty() ? std::string{} : devices.front().path;
}

inline bool HasCaptureDevice() {
    return !FirstCaptureDevicePath().empty();
}

inline std::string LoopbackDevicePath() {
    const auto loopback = SelectLoopback(EnumerateDevices(), kVirtualCameraLabel);
    return loopback.has_value() ? loopback->path : std::string{};
}

inline bool HasLoopbackDevice() {
    return !LoopbackDevicePath().empty();
}

}  // namespace wbc
