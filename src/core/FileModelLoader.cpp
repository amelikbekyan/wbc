// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/FileModelLoader.h"

#include <fstream>
#include <stdexcept>

namespace wbc {

std::vector<std::uint8_t> FileModelLoader::Load(const std::string& path) const {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open the model file: " + path);
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("The model file is empty: " + path);
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {  // GCOVR_EXCL_LINE
        // Reached only if the file is truncated between tellg() and read(),
        // which is a race no test can arrange deterministically. Excluded from
        // coverage rather than removed: the partial read it guards is real.
        throw std::runtime_error("Could not read the model file: " + path);  // GCOVR_EXCL_LINE
    }
    return bytes;
}

}  // namespace wbc
