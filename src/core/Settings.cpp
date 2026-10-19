// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Settings.h"

#include <QSettings>

#include <algorithm>

namespace wbc {
namespace {

constexpr auto kCameraPath = "camera/path";
constexpr auto kMode = "background/mode";
constexpr auto kBlurRadius = "background/blurRadius";
constexpr auto kColor = "background/color";
constexpr auto kImagePath = "background/imagePath";
constexpr auto kVideoPath = "background/videoPath";
constexpr auto kMaskThreshold = "mask/threshold";
constexpr auto kMaskSmoothing = "mask/smoothing";
constexpr auto kMaskEdgeRadius = "mask/edgeRadius";
constexpr auto kLanguage = "ui/language";

bool IsKnownMode(int value) {
    return value == static_cast<int>(BackgroundMode::Blur) ||
           value == static_cast<int>(BackgroundMode::Image) ||
           value == static_cast<int>(BackgroundMode::Color) ||
           value == static_cast<int>(BackgroundMode::Video);
}

}  // namespace

CompositorSettings AppSettings::ToCompositorSettings() const {
    CompositorSettings converted;
    converted.mode = mode;
    converted.blurRadius = blurRadius;
    // OpenCV orders channels blue, green, red; QColor does the opposite.
    converted.color = cv::Scalar(color.blue(), color.green(), color.red());
    return converted;
}

void SaveSettings(QSettings& store, const AppSettings& settings) {
    store.setValue(kCameraPath, settings.cameraPath);
    store.setValue(kMode, static_cast<int>(settings.mode));
    store.setValue(kBlurRadius, settings.blurRadius);
    store.setValue(kColor, settings.color.name(QColor::HexRgb));
    store.setValue(kImagePath, settings.backgroundImagePath);
    store.setValue(kVideoPath, settings.backgroundVideoPath);
    store.setValue(kMaskThreshold, settings.mask.threshold);
    store.setValue(kMaskSmoothing, settings.mask.smoothing);
    store.setValue(kMaskEdgeRadius, settings.mask.edgeRadius);
    store.setValue(kLanguage, settings.language);
    store.sync();
}

AppSettings LoadSettings(QSettings& store) {
    AppSettings settings;
    settings.cameraPath = store.value(kCameraPath, QString{}).toString();

    const int mode = store.value(kMode, static_cast<int>(BackgroundMode::Blur)).toInt();
    settings.mode = IsKnownMode(mode) ? static_cast<BackgroundMode>(mode) : BackgroundMode::Blur;

    settings.blurRadius = std::clamp(store.value(kBlurRadius, 15).toInt(), 0, kMaxBlurRadius);

    const QColor colour(store.value(kColor, QString{"#00b140"}).toString());
    settings.color = colour.isValid() ? colour : QColor(0, 177, 64);

    settings.backgroundImagePath = store.value(kImagePath, QString{}).toString();
    settings.backgroundVideoPath = store.value(kVideoPath, QString{}).toString();

    const MaskSettings defaults;
    settings.mask.threshold = std::clamp(store.value(kMaskThreshold, defaults.threshold).toFloat(),
                                         0.0F, kMaxMaskThreshold);
    settings.mask.smoothing = std::clamp(store.value(kMaskSmoothing, defaults.smoothing).toFloat(),
                                         0.0F, kMaxMaskSmoothing);
    settings.mask.edgeRadius = std::clamp(
        store.value(kMaskEdgeRadius, defaults.edgeRadius).toInt(), 0, kMaxMaskEdgeRadius);

    // Left exactly as stored, including empty and including a code this build
    // does not ship: wbc::ResolveLanguage is the one place that decides what
    // such a value turns into.
    settings.language = store.value(kLanguage, QString{}).toString();
    return settings;
}  // GCOVR_EXCL_LINE — gcc's exception-unwinding epilogue for the local
   // AppSettings/QColor/QString objects, reached only if something throws
   // past them. QSettings::value() does not throw.

}  // namespace wbc
