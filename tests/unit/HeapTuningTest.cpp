// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <sys/resource.h>

#include <cstring>
#include <memory>
#include <thread>

#include "core/HeapTuning.h"

namespace {

long MinorFaults() {
    rusage usage{};
    getrusage(RUSAGE_THREAD, &usage);
    return usage.ru_minflt;
}

// One frame's worth of what the pipeline allocates: several buffers the size
// of 720p images, alive together and freed together. Returns the page faults
// a second frame takes. Fresh pages fault on first touch; reused ones do not.
//
// Several at once, not one: glibc raises its mmap threshold to the size of a
// mapped block once it is freed, which alone would let a single buffer be
// reused. What it does not prevent is the heap being trimmed back as soon as
// a frame's worth of buffers is freed from its top, which is what the
// pipeline met.
long FaultsOnASecondFrame() {
    constexpr std::size_t kFrameBytes = 1280 * 720 * 3 * 2;
    constexpr int kBuffersPerFrame = 6;
    auto frame = [] {
        std::unique_ptr<char[]> blocks[kBuffersPerFrame];
        char sum = 0;
        for (auto& block : blocks) {
            block.reset(new char[kFrameBytes]);
            std::memset(block.get(), 1, kFrameBytes);
            // Read back so the writes cannot be optimised away.
            sum = static_cast<char>(sum + block[kFrameBytes / 2]);
        }
        return sum;
    };
    frame();
    const long before = MinorFaults();
    frame();
    return MinorFaults() - before;
}

}  // namespace

// The pipeline allocates on a thread of its own, which glibc serves from an
// arena other than the main one, so that is where it is checked.
TEST(HeapTuning, FreedFrameBuffersAreReusedRatherThanMappedAfresh) {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    // The sanitizers replace malloc with allocators of their own, which
    // mallopt does not reach, and map and poison memory on their own terms.
    GTEST_SKIP() << "glibc's allocator is replaced under a sanitizer";
#endif
    ASSERT_TRUE(wbc::KeepFrameBuffersOnTheHeap());

    long faults = 0;
    std::thread worker{[&faults] { faults = FaultsOnASecondFrame(); }};
    worker.join();

    // 33 MB of fresh pages is about 8,000 faults; reused, it is next to none.
    EXPECT_LT(faults, 50);
}
