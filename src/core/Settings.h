// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>

class QSettings;

#include "comp/Compositor.h"
#include "core/Types.h"

namespace wbc {

// A larger kernel costs more than it improves the picture, and an unbounded
// value read from a settings file would reach OpenCV as a kernel size.
inline constexpr int kMaxBlurRadius = 60;

struct AppSettings {
    QString cameraPath;
    BackgroundMode mode = BackgroundMode::Blur;
    int blurRadius = 15;
    QColor color{0, 177, 64};
    QString backgroundImagePath;
    QString backgroundVideoPath;
    MaskSettings mask;
    // A language code such as "ru". Empty means the choice has not been made
    // yet, and the system's language decides; see wbc::ResolveLanguage.
    QString language;

    CompositorSettings ToCompositorSettings() const;
};

void SaveSettings(QSettings& store, const AppSettings& settings);

// Values that are missing, out of range or not a known mode are replaced by
// the defaults rather than passed on. A settings file is editable by hand and
// survives upgrades, so it cannot be trusted to hold what was last written.
AppSettings LoadSettings(QSettings& store);

}  // namespace wbc
