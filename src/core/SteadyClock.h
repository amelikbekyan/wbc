// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/IClock.h"

namespace wbc {

class SteadyClock final : public IClock {
public:
    std::chrono::steady_clock::time_point Now() const override;
};

}  // namespace wbc
