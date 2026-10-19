// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

#include "core/IClock.h"

namespace wbc {

// Time only moves when a test says so.
class FakeClock final : public IClock {
public:
    std::chrono::steady_clock::time_point Now() const override { return now_; }

    void Advance(std::chrono::duration<double> delta) {
        now_ += std::chrono::duration_cast<std::chrono::steady_clock::duration>(delta);
    }

    void SetNow(std::chrono::steady_clock::time_point value) { now_ = value; }

private:
    std::chrono::steady_clock::time_point now_{};
};

}  // namespace wbc
