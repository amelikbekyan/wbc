// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/SteadyClock.h"

namespace wbc {

std::chrono::steady_clock::time_point SteadyClock::Now() const {
    return std::chrono::steady_clock::now();
}

}  // namespace wbc
