// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>

namespace wbc {

// The program's icon, from PNGs embedded in the binary at each size a desktop
// asks for. PNG rather than the SVG they are rendered from, so that showing
// the icon does not depend on Qt's SVG support being installed.
QIcon ProgramIcon();

}  // namespace wbc
