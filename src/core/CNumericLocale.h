// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace wbc {

// Forces the process's numeric locale to "C".
//
// Qt calls setlocale(LC_ALL, "") when a QApplication is constructed, which
// adopts the user's locale. On a machine whose locale uses a comma as the
// decimal separator, ONNX Runtime then evaluates the segmentation model to
// all zeros, so the person is never separated from the background and the
// application silently produces only the background. Forcing LC_NUMERIC
// back to "C" removes the fault. Qt's own number formatting uses QLocale
// and is unaffected.
//
// Call once, immediately after constructing the QApplication and before
// loading the model. Returns the locale that was in effect before.
std::string UseCNumericLocale();

}  // namespace wbc
