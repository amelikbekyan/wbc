// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/HeapTuning.h"

#include <malloc.h>

namespace wbc {
namespace {

// glibc's ceiling for the threshold on a 64 bit system. A 1080p frame's
// 16 bit three channel intermediate is 12 MB, comfortably under it.
constexpr int kMmapThreshold = 32 * 1024 * 1024;
// Freed memory beyond this at the top of the heap is returned to the kernel.
// Larger than the pipeline's working set, so a frame's buffers survive to
// the next frame; small enough that a burst of something else is given back.
constexpr int kTrimThreshold = 128 * 1024 * 1024;

}  // namespace

bool KeepFrameBuffersOnTheHeap() {
    const bool mmapSet = mallopt(M_MMAP_THRESHOLD, kMmapThreshold) == 1;
    const bool trimSet = mallopt(M_TRIM_THRESHOLD, kTrimThreshold) == 1;
    return mmapSet && trimSet;
}

}  // namespace wbc
