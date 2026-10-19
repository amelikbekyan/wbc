# Engineering notes

How the pipeline came to run as fast as it does, and the faults that were
hardest to find. [README.md](../README.md) says how to use the program and
[CONTRIBUTING.md](../CONTRIBUTING.md) how to build and test it; this file is
about the decisions in between, with the numbers that drove them.

All figures are for a 1280x720 frame, measured on the laptop the program was
developed on, unless a section says otherwise. "Compute" excludes the wait
for the camera; see [Measuring latency](#measuring-latency) for why.

## Performance

### Where a frame's time went

The first working pipeline took 80.4 ms a frame, 12.4 fps:

| stage | ms |
|---|---|
| camera read | 2.9 |
| inference | 5.3 |
| mask refinement | 29.3 |
| compositing | 42.7 |

Inference -- the one stage a graphics card would have helped -- was 6.6% of
the frame. The rest was arithmetic on the processor, so that is where the
work went.

### First pass: fixed point and a smaller filter (80 ms → 30 ms)

Two changes, both arithmetic.

**The blend in fixed point.** The compositor blended in floating point, which
pushed two full-resolution `CV_32FC3` copies of the frame through memory on
every frame. The same blend in fixed point -- alpha as 0..255, products
accumulated in 16 bits, the divide by 255 riding on the final conversion --
costs 14.9 ms instead of 39.5. Channels may land one step from exact
arithmetic.

**The guided filter at half size.** The edge refiner ran the guided filter at
the full frame size. Run at half size with half the radius and scaled back
up -- the standard fast guided filter -- it costs 3.5 ms instead of 28.0. The
boundary is slightly softer, a modest price on a mask that came from a
256x256 model output.

Result: 30.2 ms a frame, 33.1 fps, against a target of 30.

### Compositing on the graphics card

Compositing was still half the frame. The same fixed-point blend runs on a
graphics device through OpenCL, which needs nothing added to the build, is
not tied to one vendor, and degrades to the processor where there is no
device. The blend is written once as a template and instantiated for
`cv::Mat` and `cv::UMat`, so the two paths cannot drift apart.

Measured on a GeForce RTX 4060 Laptop, compute only:

| | processor | graphics card |
|---|---|---|
| compositing | 14.1 ms | 6.5 ms |
| mask refinement | 6.6 ms | 11.1 ms |
| whole frame | 26.5 ms | 23.3 ms |
| ceiling | 37.7 fps | 43.0 fps |

Refining the mask gets *slower* while the device is in use, because the
guided filter stays on the processor and contends with the OpenCL runtime;
the net is still a gain. Creating the context and not using it costs nothing
measurable, which is why the window can always probe for a device.

The box is off by default and disabled, with its reason, when no device is
found. A "yes" stored on a machine that had a device is not honoured on a
machine that does not.

The path was removed after the second pass below. Measured again on the
real pipeline, the same camera and the same device, the graphics path had
become the slower one: 14.8 to 15.4 ms a frame against 12.2 to 12.9 on the
processor, at 155% of a core against 48 to 50%. The blend it moved had
fallen to 1.5 ms on the processor, which left the upload and the read-back
as most of what the device added. The extra core was NVIDIA's OpenCL
driver: after every operation a thread of its own spins on `sched_yield`
for 20 to 40 ms before it sleeps, and with a frame every 33 ms it never
does. That is the driver's design, and OpenCL offers no way to change it.

### Second pass: five processors' worth of time to half of one

With the frame at about 27 ms the program still kept roughly 500% of a
processor busy and, on the real camera, delivered only 24 frames a second at
38 ms each. After this pass it delivers all thirty at 14 ms a frame and 57%
of one processor, on one thread. Where the time went, operation by
operation:

- **OpenCV's thread pool.** OpenCV started a worker per core -- sixteen here
  -- and each sat at a quarter of a core. The work is a chain of short
  operations that a pool cannot split profitably: a frame took 35 ms on
  sixteen threads and 33 ms on two. It now runs on one
  (`src/core/OpenCvThreads.cpp`).

- **ONNX Runtime's spinning worker.** The session's second worker spun
  between inferences, keeping most of a core busy for a model that runs 7 ms
  in every 33. Spinning is off (`session.intra_op.allow_spinning=0`) and the
  session uses one thread: the second saved 2 ms of a frame and spent them on
  the processor.

- **Page faults from glibc.** glibc mapped every frame-sized buffer fresh
  from the kernel and gave it back on free, so each frame took about 11,000
  page faults -- 15 of its 38 ms. Large blocks now stay on the heap and the
  heap is not trimmed after every frame (`src/core/HeapTuning.cpp`), so a
  frame reuses the last one's pages.

- **Shrinking the frame once.** The frame was shrunk three times, to three
  sizes, for the model, the edge filter and the blur, each by area averaging
  at a ratio OpenCV handles on its slow general path: 8 ms a frame.
  `ShrinkFrame` halves exactly first, which OpenCV does in 0.4 ms, and the
  pipeline shrinks once, to the model's resolution, for all three stages.

- **The guided filter on the shared copy.** It ran at half the frame's size,
  13 ms. It now runs on that shared shrunken copy -- 455x256 at 720p -- in
  under 5: the mask it corrects never had finer detail than that.

- **The blur on a smaller copy.** The background blur ran a kernel up to 121
  pixels wide on the whole frame. It is now computed on a smaller copy with
  the sigma scaled to match, which looks the same to within two levels on
  average.

- **The blend in one pass.** The mask was scaled up in float and blended in
  six OpenCV calls through two 16-bit intermediates the size of the frame.
  It is now scaled up in eight bits and blended in a single loop over plain
  bytes that the compiler vectorises at `-O3`: 1.5 ms against 4.4, with the
  rounding exact (adding the sum's high byte before the shift divides by
  255, not 256). The OpenCL path kept the OpenCV calls, since `cv::UMat`
  cannot be walked by a plain loop.

As a consequence the refiner delivers the mask at the size of the shrunken
frame, and the compositor accepts a mask smaller than the frame, plus that
shrunken frame for a large blur.

## Threads and sanitizers

### Which ThreadSanitizer reports are ours

An early version of CONTRIBUTING said a race whose stack reaches `wbc::`
code is ours. That is wrong: OpenCV runs most operations on Intel TBB
threads, and every report about a buffer passed to that pool has `wbc::`
frames in it, because it is this project's code that called OpenCV. A run of
the pipeline and video tests produced 127 such reports.

The test that separates them is *who created the threads*. All 127 involved
a thread created inside libtbb; none was between two threads this project
started. The same reports appear in tests that start no threads of their
own, which confirms the first kind rather than assuming it. Test binaries
built for ThreadSanitizer now run OpenCV on the calling thread
(`tests/support/SerialOpenCvUnderTsan.cpp`), which removes that whole class.

One report remains that TSan cannot be taught: a `free` inside OpenCV
against a read in this project's loop, when the GUI thread replaces the
background picture while the pipeline is blending the old one. A `cv::Mat`
is shared through a reference count OpenCV changes atomically, and TSan
cannot see those atomics. The same report comes from a twenty-line program
that shares a `cv::Mat` correctly.

### The video player hands out copies

The background video decodes on a thread of its own and keeps the current
frame already fitted to the camera's size; the pipeline takes a copy.
Sharing the buffer would be safe for the reason above, but each retired
frame was reported as a race, and a copy costs about 0.3 ms at 720p. The
copy keeps the sanitizer's reports about this project worth reading.

Decoding cost belongs to the file, not the camera: a 720p clip costs a
fraction of a millisecond a frame, a noisy 1080p one several, a 4K phone
clip more than the pipeline has to spare. The decoding thread does compete
for cores -- blending alone rose from 5.3 ms to 14.6 ms under a noisy 1080p
clip, which is far above the bitrate of real footage. When the machine
cannot keep up, the video plays slower rather than bursting to catch up.

### The preview is dropped, not queued

The worker must never block on the GUI thread, and Qt does not coalesce
queued signals, so a GUI thread that falls behind would accumulate one
deep-copied image per pending frame -- 2.7 MB at 720p. A preview is emitted
only when the window has said, through `PreviewConsumed()`, that it took the
last one; otherwise it is skipped.

## Measuring latency

The status line's time was first measured from before the camera read, so
it included waiting for the next frame. While processing took 80 ms that
went unnoticed: the pipeline was slower than the camera and never waited.
Once processing fell to about 27 ms the pipeline spent most of each frame
waiting, and the number began to follow the camera instead -- 33 ms by day,
57 ms one evening, 137 ms in a dim room at 7 frames a second -- which read
as the program getting slower when the only thing that had slowed was the
webcam's exposure.

The timer now starts once the frame is in hand. The frame rate is left as it
was and still shows the camera's pace, which is what tells a user the camera
is the limit. In a dim room the status line reads 7.1 fps and 23.3 ms where
it read 7 fps and 137 ms.

The camera fake gained a hook called on each read, which moves a fake clock
on as a real camera's wait would, and two tests use it: one fails on the old
code with 100 ms of latency for no processing, the other holds the frame
rate to the camera's pace.

## Faults that were hard to find

### The locale that zeroes the model

`QApplication` calls `setlocale(LC_ALL, "")` when constructed, adopting the
user's locale. Where that locale uses a comma as the decimal separator,
ONNX Runtime evaluates the segmentation model to all zeros: the person is
never separated from the background, and the window silently shows only the
background.

`LANG` alone does not tell you whether you are affected. Desktop
environments set the regional-format categories separately, so
`LANG=en_US.UTF-8` can sit alongside `LC_NUMERIC=en_DK.UTF-8` -- which is
exactly how the fault first appeared. The test's first candidate list named
only locales the machine did not have, so it skipped on the one machine
that reproduced the bug; it now tries `en_DK` first and confirms the comma
through `localeconv()` rather than trusting a locale's name.

Forcing `LC_NUMERIC` back to `"C"` after `QApplication` is constructed
removes the fault (`wbc::UseCNumericLocale()`, `src/core/CNumericLocale.h`).
It is a small, independently testable function rather than a bare line in
`main.cpp`, which the coverage gate excludes. Qt's own number formatting
goes through `QLocale` and is unaffected.

### `exclusive_caps=1` and the camera that is only sometimes there

The loopback device is created with `exclusive_caps=1`, which is required
or Chrome and Zoom refuse to list it. The consequence is that it advertises
a capture format only while frames are being written to it. Looking for the
virtual camera in a conference application before pressing Start finds
nothing, and an application that enumerated cameras earlier keeps the list
it built without it. The window says both, and says the second half only
once streaming has started.

### PipeWire remembers the device as an output

Firefox, OBS, and every Flatpak and Snap application reach cameras only
through PipeWire and the camera portal, and to them the virtual camera did
not exist. With `exclusive_caps=1` the node advertises Video Output until a
frame is written into it, and only then Video Capture. WirePlumber probes a
V4L2 node once, when it appears, and the module is always loaded before
anything streams into it, so the node was remembered as an output and got
no `Video/Source`. Chrome, which opens V4L2 devices itself, was never
affected.

`PipeWireAnnouncingSink` wraps the loopback sink. After the first frame of
a run is delivered it asks `pw-dump` whether PipeWire has a source for the
device, and if not, restarts the user's WirePlumber, which probes the node
again and finds it capturing. The check comes first so that the restart,
which interrupts audio for a moment, happens once per loaded module rather
than once per run. It runs on a thread of its own so the frame loop never
waits for it, and a machine where `pw-dump` cannot run is left alone.

## Choices fixed early

A few decisions were pinned in the design before any code, because they
are expensive to change later:

- **v4l2loopback** as the virtual-camera transport, with `exclusive_caps=1`.
- **MediaPipe Selfie Segmentation** in ONNX, fetched at configure time and
  checked against a recorded SHA-256.
- **I420** as the output format, since OpenCV offers no conversion *to*
  YUYV.
- **Every boundary** to hardware, the model, the filesystem and the clock
  behind an interface, so that all code can be covered by tests. The
  coverage gate treats that as a requirement rather than a goal.
