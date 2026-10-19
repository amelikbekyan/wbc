# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later

# Downloads the segmentation model once and verifies it byte for byte.
# A hash mismatch is a hard configuration error: silently continuing with an
# unexpected model would produce a broken mask at runtime with no explanation.

set(WBC_MODEL_URL
    "https://huggingface.co/onnx-community/mediapipe_selfie_segmentation/resolve/main/onnx/model.onnx")
set(WBC_MODEL_SHA256
    "3241ac4ad8aa35bdaf33946776db29f7c283a413aa0b0dacb9483594b4531aad")
set(WBC_MODEL_DIR "${CMAKE_SOURCE_DIR}/models")
set(WBC_MODEL_FILE "${WBC_MODEL_DIR}/selfie_segmentation.onnx")

file(MAKE_DIRECTORY "${WBC_MODEL_DIR}")

if (EXISTS "${WBC_MODEL_FILE}")
    file(SHA256 "${WBC_MODEL_FILE}" _wbc_existing_hash)
else ()
    set(_wbc_existing_hash "")
endif ()

if (NOT _wbc_existing_hash STREQUAL WBC_MODEL_SHA256)
    message(STATUS "Downloading segmentation model to ${WBC_MODEL_FILE}")
    file(DOWNLOAD "${WBC_MODEL_URL}" "${WBC_MODEL_FILE}"
         EXPECTED_HASH SHA256=${WBC_MODEL_SHA256}
         SHOW_PROGRESS
         STATUS _wbc_download_status)
    list(GET _wbc_download_status 0 _wbc_download_code)
    if (NOT _wbc_download_code EQUAL 0)
        list(GET _wbc_download_status 1 _wbc_download_message)
        message(FATAL_ERROR "Model download failed: ${_wbc_download_message}")
    endif ()
endif ()
