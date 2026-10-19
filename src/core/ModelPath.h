// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace wbc {

// The file name the segmentation model is always stored under, wherever the
// copy happens to be.
inline constexpr const char* kModelFileName = "selfie_segmentation.onnx";

// Every place the model may be, most specific first. The copy beside the
// executable, which only the build tree has, comes last so that an installed
// copy wins for an installed binary, and a developer running from the build
// tree still finds theirs.
QStringList ModelSearchPath(const QString& environmentOverride,
                            const QString& applicationDirectory);

// The first candidate that exists, or an empty string when none does. `exists`
// is a parameter so that the order can be tested without laying files out on
// disk.
using FileExistsFn = std::function<bool(const QString&)>;
QString FindModelFile(const QStringList& candidates, const FileExistsFn& exists);

// What the application actually calls: reads the environment, asks Qt where
// the executable is, and looks on disk.
QString FindModelFile();

}  // namespace wbc
