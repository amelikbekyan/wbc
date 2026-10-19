// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/CNumericLocale.h"

#include <clocale>

namespace wbc {

std::string UseCNumericLocale() {
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string previousLocale = previous != nullptr ? previous : "";
    std::setlocale(LC_NUMERIC, "C");
    return previousLocale;
}

}  // namespace wbc
