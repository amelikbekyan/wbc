#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs both coverage gates and prints both summaries even when the first one
# fails. CMake's add_custom_target stops a multi-COMMAND target at the first
# non-zero exit, which used to hide the relaxed gate's numbers whenever the
# strict gate failed -- a gate that hides half its answer on failure is worse
# than one that prints everything. This script runs both unconditionally and
# only reports overall failure at the end.
#
# The strict gate covers only code that needs no hardware, so it holds
# anywhere the unit and ui tests run -- including CI. The relaxed gate covers
# the code that talks to a camera and a kernel module; it can only be met on
# a machine that has them, with the hardware tests run.
# `strict` checks the first alone, for CI; anything else checks both.
#
# Usage: coverage-check.sh <source-dir> <binary-dir> [strict]

set -u

src="$1"
bindir="$2"
mode="${3:-all}"

echo "=== strict gate: 100% required ==="
gcovr --root "${src}" \
      --filter "${src}/src/core/CNumericLocale.cpp" \
      --filter "${src}/src/core/OpenCvThreads.cpp" \
      --filter "${src}/src/core/HeapTuning.cpp" \
      --filter "${src}/src/core/FileModelLoader.cpp" \
      --filter "${src}/src/core/ImageConvert.cpp" \
      --filter "${src}/src/core/PipelineController.cpp" \
      --filter "${src}/src/core/PipelineStats.cpp" \
      --filter "${src}/src/core/ModelPath.cpp" \
      --filter "${src}/src/core/Settings.cpp" \
      --filter "${src}/src/core/Translations.cpp" \
      --filter "${src}/src/ui/ProgramIcon.cpp" \
      --filter "${src}/src/core/SteadyClock.cpp" \
      --filter "${src}/src/comp/BackgroundImage.cpp" \
      --filter "${src}/src/comp/BackgroundVideo.cpp" \
      --filter "${src}/src/comp/Compositor.cpp" \
      --filter "${src}/src/comp/OpenCvVideoDecoder.cpp" \
      --filter "${src}/src/seg/EdgeRefiner.cpp" \
      --filter "${src}/src/seg/MaskRefiner.cpp" \
      --filter "${src}/src/seg/MaskSmoother.cpp" \
      --filter "${src}/src/seg/OnnxPostprocess.cpp" \
      --filter "${src}/src/seg/OnnxPreprocess.cpp" \
      --filter "${src}/src/camera/FormatNegotiator.cpp" \
      --filter "${src}/src/sink/I420Converter.cpp" \
      --fail-under-line 100 \
      --print-summary
strict=$?

if [ "${mode}" = "strict" ]; then
    echo ""
    echo "strict exit=${strict} (relaxed gate not checked: it needs a camera)"
    [ "${strict}" -eq 0 ]
    exit $?
fi

echo ""
echo "=== relaxed gate: 90% required ==="
gcovr --root "${src}" \
      --filter "${src}/src/ui/MainWindow.cpp" \
      --filter "${src}/src/seg/SelfieOnnx.cpp" \
      --filter "${src}/src/camera/DeviceEnumerator.cpp" \
      --filter "${src}/src/camera/V4l2CameraSource.cpp" \
      --filter "${src}/src/sink/V4l2LoopbackSink.cpp" \
      --filter "${src}/src/sink/PkexecLoopbackAdmin.cpp" \
      --filter "${src}/src/sink/PipeWireAnnouncingSink.cpp" \
      --fail-under-line 90 \
      --print-summary
relaxed=$?

echo ""
echo "strict exit=${strict} relaxed exit=${relaxed}"

[ "${strict}" -eq 0 ] && [ "${relaxed}" -eq 0 ]
