# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later

# Ubuntu's libonnxruntime-dev provides no CMake package config, so locate the
# pieces directly and expose them as an imported target.

find_path(OnnxRuntime_INCLUDE_DIR
          NAMES onnxruntime_cxx_api.h
          PATH_SUFFIXES onnxruntime onnxruntime/core/session)

find_library(OnnxRuntime_LIBRARY NAMES onnxruntime)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OnnxRuntime
                                  REQUIRED_VARS OnnxRuntime_LIBRARY OnnxRuntime_INCLUDE_DIR)

if (OnnxRuntime_FOUND AND NOT TARGET OnnxRuntime::OnnxRuntime)
    add_library(OnnxRuntime::OnnxRuntime UNKNOWN IMPORTED)
    set_target_properties(OnnxRuntime::OnnxRuntime PROPERTIES
                          IMPORTED_LOCATION "${OnnxRuntime_LIBRARY}"
                          INTERFACE_INCLUDE_DIRECTORIES "${OnnxRuntime_INCLUDE_DIR}")
endif ()

mark_as_advanced(OnnxRuntime_INCLUDE_DIR OnnxRuntime_LIBRARY)
