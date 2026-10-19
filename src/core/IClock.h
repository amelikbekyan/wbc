// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

namespace wbc {

// Time is injected rather than read directly so that frame pacing, frames per
// second accounting and mask smoothing can be tested without sleeping.
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::steady_clock::time_point Now() const = 0;
};

}  // namespace wbc
