// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace wbc {

// Keeps the frame-sized buffers the pipeline allocates on every frame in
// memory the process already owns.
//
// glibc hands an allocation above its mmap threshold straight to the kernel
// and gives it back on free. A 720p frame, its float mask and the blend's
// 16 bit intermediates are all above it, so every frame mapped fresh pages,
// and the kernel zeroed each one on first touch: about 11,000 page faults a
// frame. That was 15 of the 38 ms a frame took at 1280x720, and the processor
// time to go with it. With large blocks kept on the heap, and the heap not
// trimmed back as soon as they are freed, the next frame reuses the last
// one's pages.
//
// Call once, before the pipeline starts. Returns false if glibc refused
// either setting, which leaves the process working as before, only slower.
bool KeepFrameBuffersOnTheHeap();

}  // namespace wbc
