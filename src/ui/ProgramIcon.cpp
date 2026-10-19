// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ProgramIcon.h"

#include <QString>

namespace wbc {

QIcon ProgramIcon() {
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addFile(QString(":/icons/wbc-%1.png").arg(size), QSize(size, size));
    }
    return icon;
}  // GCOVR_EXCL_LINE — gcc's exception-unwinding epilogue for the local
   // QIcon, reached only if addFile() throws, which it does only when
   // memory runs out.

}  // namespace wbc
