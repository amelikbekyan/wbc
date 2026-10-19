// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>

#include "core/IModelLoader.h"

namespace wbc {

class FakeModelLoader final : public IModelLoader {
public:
    std::vector<std::uint8_t> Load(const std::string& path) const override {
        if (!error_.empty()) {
            throw std::runtime_error(error_ + ": " + path);
        }
        return bytes_;
    }

    void SetBytes(std::vector<std::uint8_t> bytes) { bytes_ = std::move(bytes); }
    void SetError(std::string error) { error_ = std::move(error); }

private:
    std::vector<std::uint8_t> bytes_;
    std::string error_;
};

}  // namespace wbc
