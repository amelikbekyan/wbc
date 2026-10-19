# Contributing

Changes are welcome. This file is about how the project is built and checked;
[README.md](README.md) is about using it.

## The one rule

Every line of code is covered by a test, the parts that talk to hardware
included. That is not aspirational: the build has a target that fails when
coverage drops, and CI runs it. A change without tests for what it adds will
be asked for them.

## Building for development

The dependencies are the ones in the README. Every configuration is a preset
in `CMakePresets.json`, which CLion, Visual Studio Code and Qt Creator load as
build profiles and which work the same from a terminal. Each builds under
`build/<preset>` with Ninja.

| Preset | What it is for |
|---|---|
| `debug` | ordinary development |
| `release` | what users run |
| `coverage` | measuring coverage; instrumented, unoptimised |
| `asan` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `tsan` | ThreadSanitizer; the pipeline and the video decoder run on threads of their own |

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

The presets add `/opt/onnxruntime` to the search path, which is where
`ci/install-deps-ubuntu.sh` puts ONNX Runtime; where that directory does not
exist it is ignored. ONNX Runtime somewhere else goes in a
`CMakeUserPresets.json` of your own, which git ignores:

```json
{
  "version": 3,
  "configurePresets": [
    {"name": "mine", "inherits": "debug",
     "cacheVariables": {"CMAKE_PREFIX_PATH": "/where/onnxruntime/is"}}
  ]
}
```

ThreadSanitizer cannot see the atomics that thread pools inside libraries use
to hand work to their threads, so every buffer passed between a caller and
such a pool looks unsynchronised. OpenCV's Intel TBB pool was the worst of
them, at over a hundred reports a run, and in every test binary built for
ThreadSanitizer OpenCV now runs on the calling thread instead
(`tests/support/SerialOpenCvUnderTsan.cpp`); the application is unaffected.
ONNX Runtime's pool and Qt's image-scaling threads remain, so the `tsan` test
preset reports races without failing the run, and the reports are read:

- Every report names the threads involved and where each was created. If one
  was created inside `libonnxruntime` or by `QThread`, the report is that
  library's pool. A `wbc::` frame in the stack does not change that -- it is
  this project's code that called the library.
- A report between two threads this project started (`std::thread`, from
  `PipelineController::Start` or `BackgroundVideo::Play`) or between one of
  them and the main thread is ours, and is a bug.

  One exception: a race between a `free` inside `libopencv_core` and a read
  in this project's code, as when the GUI thread replaces the background
  picture while the pipeline's own loop is blending the old one. A `cv::Mat`
  is shared through a reference count that OpenCV changes atomically, and
  TSan cannot see those atomics either, so the last owner's free looks
  unordered against the other owner's earlier read. The same report comes
  from a twenty-line program that shares a `cv::Mat` correctly.

At the time of writing a full run has no report of the second kind besides
that one.

## Tests

There are three suites, run separately by label:

```bash
ctest --preset debug -L unit       # no hardware needed
ctest --preset debug -L ui         # no display needed; runs offscreen
ctest --preset debug -L hardware   # a camera and the virtual camera
```

The hardware suite needs a camera at `/dev/video0` and the v4l2loopback
device from the README. Without them it reports itself skipped -- exit code
77, which CTest shows as *Skipped* -- rather than passing with nothing tested.

## Coverage

Coverage is a gate, not a report to read. Build the `Coverage` configuration,
**run** the tests, then check:

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
cmake --build --preset coverage-check          # both gates
cmake --build --preset coverage-check-strict   # the first only
cmake --build build/coverage --target coverage # HTML report
```

gcov only records a file once an instrumented binary has run, so building
alone measures nothing, and running fewer suites than a gate needs reports
lines as uncovered that a full run covers.

There are two gates, because the code falls into two groups that can be held
to different standards honestly:

- **Strict, 100%.** Code that needs no hardware: the pipeline, the mask chain,
  compositing, settings, translations, model lookup, format negotiation, the
  I420 conversion. The exact list is in `cmake/coverage-check.sh`. This gate
  holds anywhere the unit and ui suites run, and CI enforces it.
- **Relaxed, 90%.** Code that wraps a camera, a kernel module, a privileged
  helper or the window. It can only be met with the hardware suite run on a
  machine that has a camera and the v4l2loopback device. Without them the camera and virtual-camera wrappers
  measure 0%, and the gate reports about 82%. Check it before a release.

A line in the strict group that is genuinely unreachable gets a test if one
can be written, and otherwise a `GCOVR_EXCL_LINE` with a comment saying why.
It does not get a lowered threshold. Seven lines carry the marker:

- The closing brace of five functions whose locals have non-trivial
  destructors. gcc puts an exception-unwinding landing pad on that brace,
  which gcov counts as a line, reachable only if something throws after
  those locals exist; nothing in these functions does. They are
  `CoverCrop` in `src/comp/BackgroundImage.cpp`, `LoadSettings` in
  `src/core/Settings.cpp`, `Smooth` in `src/seg/MaskSmoother.cpp`,
  `ModelSearchPath` in `src/core/ModelPath.cpp`, and `ProgramIcon` in
  `src/ui/ProgramIcon.cpp`.
- Two lines in `src/core/FileModelLoader.cpp`, the partial-read guard and the
  error it throws, reached only if the model file shrinks between `tellg()`
  and `read()` -- a race no test can arrange.

Where the same problem could be avoided by writing the code differently
rather than by excusing it, it has been: `EdgeRefiner` keeps its working
buffers at function scope, and its filter writes through an output parameter,
for exactly this reason.

`src/main.cpp` is in neither gate. It builds objects and calls `exec()`.

## Translations

Every string a user can see goes through `tr()` in the window, or
`QCoreApplication::translate` in the classes below it, and is translated in
`i18n/*.ts`. See the README's section on languages for the commands. A change
that adds or rewords a string updates all four `.ts` files; a string left
untranslated falls back to English, which is safe but is not finished.

## Continuous integration

`.github/workflows/ci.yml` builds the `release` and `coverage` presets on Ubuntu 24.04 --
the oldest release this project supports, with Qt 6.4 and OpenCV 4.6 -- runs
every suite, and checks the strict coverage gate. It installs its
dependencies with `ci/install-deps-ubuntu.sh`, the same script the README
tells people to run, so what CI builds and what a person builds cannot drift.

## Style

Match the code around the change: the names, the idioms, and the comments.
Comments here explain *why* -- what would break, what was measured, what the
alternative cost -- and a change that makes one of them untrue updates it.
