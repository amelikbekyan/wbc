// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wbc {

// Reads model bytes. Injected so that inference can be tested against
// deliberately malformed input without writing files.
class IModelLoader {
public:
    virtual ~IModelLoader() = default;

    // Throws std::runtime_error naming the path when the model cannot be read.
    virtual std::vector<std::uint8_t> Load(const std::string& path) const = 0;
};

}  // namespace wbc
