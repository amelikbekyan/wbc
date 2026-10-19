// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/IModelLoader.h"

namespace wbc {

class FileModelLoader final : public IModelLoader {
public:
    std::vector<std::uint8_t> Load(const std::string& path) const override;
};

}  // namespace wbc
