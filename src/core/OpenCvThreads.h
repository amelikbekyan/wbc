// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace wbc {

// How many threads OpenCV may run its own parallel loops on.
//
// Left alone, OpenCV starts one worker per core. The per-frame work is a
// chain of small operations on a shrunken frame, each too short for a pool to
// split profitably, so the extra workers barely shortened a frame: at
// 1280x720 on 16 cores a frame took 35 ms with all of them and 33 ms with
// two, while the process used 480% of a processor instead of 200%. Since the
// frame was shrunk once for every stage, the whole of it fits in the 33 ms a
// frame lasts on one core, and one thread does the same work with none of
// the handing over.
inline constexpr int kOpenCvThreads = 1;

// Call once, before the pipeline starts.
void LimitOpenCvThreads();

}  // namespace wbc
