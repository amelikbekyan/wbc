// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/Types.h"

namespace wbc {

constexpr std::uint32_t MakeFourcc(char a, char b, char c, char d) {
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
           (static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
}

constexpr std::uint32_t kFourccMjpg = MakeFourcc('M', 'J', 'P', 'G');
constexpr std::uint32_t kFourccYuyv = MakeFourcc('Y', 'U', 'Y', 'V');
constexpr std::uint32_t kFourccYu12 = MakeFourcc('Y', 'U', '1', '2');

// Chooses the best of what the device actually offers.
//
// It never returns a format that was not in the list: a camera asked for a
// mode it does not have will either fail to open or quietly hand back
// something else, and both are worse than knowing up front what was picked.
std::optional<FormatOption> Negotiate(const std::vector<FormatOption>& supported,
                                      const FormatRequest& request);

}  // namespace wbc
