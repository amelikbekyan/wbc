// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

// Under ThreadSanitizer, OpenCV runs on the calling thread instead of its
// Intel TBB pool, in every test binary.
//
// TBB hands work to its threads with atomics of its own that the sanitizer
// cannot see, so every buffer an OpenCV call shares with the pool is reported
// as a race -- over a hundred reports a run, none of them this project's, and
// enough to bury one that is. With the pool gone those reports go with it,
// while the threads this project starts -- the pipeline's worker and the
// video decoder -- are exactly as they were, and any report left is about
// them. The application itself is unaffected: this file is linked into the
// tests only, and does nothing in any other build.

#if defined(__SANITIZE_THREAD__)

#include <opencv2/core/utility.hpp>

namespace {

// A static initialiser, so it runs before the first test and needs no
// change to the test binaries' own main functions. OpenCV is a shared
// library and is initialised before this executable's statics.
const bool kOpenCvIsSerial = [] {
    cv::setNumThreads(1);
    return true;
}();

}  // namespace

#endif
