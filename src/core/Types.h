// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

namespace wbc {

// The card_label the v4l2loopback device is created with, and therefore the
// name the application recognises its own output device by. It appears in the
// modprobe command, in the device list filter and in the hardware tests; one
// name so that changing it cannot half-change it.
inline constexpr const char* kVirtualCameraLabel = "Webcam Background Changer";

// Video is appended rather than inserted: the mode is stored in the settings
// file as its number, so the existing values keep their meaning.
enum class BackgroundMode { Blur, Image, Color, Video };

// A threshold of 1 would divide by zero in the remap below, and a mask that
// has been smoothed with a weight of 1 never changes again. Both stop short.
inline constexpr float kMaxMaskThreshold = 0.95F;
inline constexpr float kMaxMaskSmoothing = 0.95F;
// Past this the guided filter costs more per frame than it repays, and its
// window starts spanning whole objects rather than an edge.
inline constexpr int kMaxMaskEdgeRadius = 20;

// How the raw model output is turned into the alpha the compositor blends
// with. The defaults are the values the pipeline used before any of this was
// adjustable, so leaving every slider alone reproduces the old behaviour.
struct MaskSettings {
    // Alpha at or below this goes to zero, and what is left is stretched back
    // over the full range. Raising it removes the faint mask the model leaves
    // on furniture; raise it far enough and it removes hair and shoulders too.
    float threshold = 0.0F;
    // Weight given to the previous frame's mask, per nominal frame interval.
    // Higher is steadier and slower to follow movement.
    float smoothing = 0.7F;
    // Guided filter window, in pixels, used to pull the mask onto the edges
    // in the frame. Zero disables the filter.
    int edgeRadius = 8;
};

// One concrete capture or output format as reported by V4L2.
struct FormatOption {
    std::uint32_t fourcc = 0;
    int width = 0;
    int height = 0;
    double fps = 0.0;
};

// What the application would like, in order of preference. The negotiator is
// allowed to return something smaller; it is not allowed to invent a format
// the device never offered.
struct FormatRequest {
    int width = 1280;
    int height = 720;
    double fps = 30.0;
    std::vector<std::uint32_t> fourccPreference;
};

// A dropped frame is normal back pressure, not a failure. Fatal means the
// device is gone and the pipeline must stop.
enum class WriteResult { Ok, Dropped, Fatal };

}  // namespace wbc
