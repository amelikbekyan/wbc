# wbc — Webcam Background Changer

[![CI](https://github.com/amelikbekyan/wbc/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/amelikbekyan/wbc/actions/workflows/ci.yml)

Replaces the background of a webcam and presents the result as a second
camera, which Zoom, Google Meet, Microsoft Teams, OBS and any browser can
select like any other. The person stays; the room behind them is blurred,
or replaced by a picture, a video or a solid colour.

It runs on Linux, on the processor, with no graphics card required. On the
laptop it was developed on, an AMD Ryzen 7 7435HS, a 1280x720 frame takes
about 13 ms, well inside the 33 ms a 30 fps camera allows: all thirty frames a
second arrive, for about half of one processor core.

## Why

On Linux, blurring or replacing the background of a webcam is left to each
video-calling application: some offer it, some do not, and those that do each
have their own settings. The general answer is OBS with a background-removal
plugin and its virtual camera, which works well, but is a whole streaming
studio for one feature. The tools made for exactly this task, listed below,
are driven from the command line.

wbc does that one thing, in a window. It takes the real camera, blurs or
replaces the background using the MediaPipe Selfie Segmentation model, and
offers the result as a virtual camera that any application can choose. Open
it and press Start.

## How it compares

| | wbc | [Linux-Fake-Background-Webcam](https://github.com/fangfufu/Linux-Fake-Background-Webcam) | [backscrub](https://github.com/floe/backscrub) | OBS with [obs-backgroundremoval](https://github.com/royshil/obs-backgroundremoval) |
|---|---|---|---|---|
| Made for | the webcam's background | the webcam's background | the webcam's background | streaming and recording; background removal is a plugin |
| Used through | a window | the command line or a configuration file | the command line | OBS's studio window: scenes, filters, virtual camera |
| Written in | C++20, Qt 6 | Python | C++ | C++ |
| Segmentation | MediaPipe Selfie Segmentation, ONNX Runtime | MediaPipe | TensorFlow Lite models | a choice of models, ONNX Runtime |
| Virtual camera | v4l2loopback, made visible to PipeWire applications | v4l2loopback | v4l2loopback | OBS's virtual camera, v4l2loopback |

There are no timings for the others here. Each was built with its own aims
and defaults, and figures taken on one machine with one set of choices would
say more about those choices than about the programs. Corrections to the
table are welcome.

## How it works

A small neural network ([MediaPipe Selfie Segmentation][model]) marks which
pixels belong to the person. That mask is steadied over time so it does not
flicker, pulled onto the real edges in the frame so it does not leave a halo,
and used to blend the camera over the chosen background. The result is
written to a [v4l2loopback][v4l2loopback] device, which other applications
see as an ordinary camera named **Webcam Background Changer**.

[model]: https://huggingface.co/onnx-community/mediapipe_selfie_segmentation
[v4l2loopback]: https://github.com/umlaeute/v4l2loopback

## Installing

There are no prebuilt packages yet; it is built from source.

### Ubuntu 24.04

Ubuntu 24.04 does not package ONNX Runtime, so the script that CI uses fetches
the official release, checks it against a pinned hash, and installs it in
`/opt/onnxruntime`. It installs everything else from Ubuntu's own archive:

```bash
./ci/install-deps-ubuntu.sh
sudo apt install v4l2loopback-dkms

cmake --preset release
cmake --build --preset release
sudo cmake --install build/release
```

### Ubuntu 26.04

ONNX Runtime is packaged here, so everything comes from the archive:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-tools-dev-tools \
                 libopencv-dev libopencv-contrib-dev libonnxruntime-dev \
                 v4l2loopback-dkms

cmake --preset release
cmake --build --preset release
sudo cmake --install build/release
```

### What the build needs

- CMake 3.21 or newer, Ninja, and a C++20 compiler
- Qt 6.4 or newer (Core, Gui, Widgets)
- OpenCV 4.6 or newer; `opencv_contrib`'s `ximgproc` is recommended, and
  without it the edges of the mask are softer
- ONNX Runtime; 1.23 is the version this is built and tested with
- `lrelease`, for the translations; without it the program speaks English only
- a network connection the first time CMake runs, to download the model. It is
  checked against a SHA-256 recorded in `cmake/FetchModel.cmake`

To install it for yourself alone, without `sudo`, into `~/.local`:

```bash
cmake --install build/release --prefix ~/.local
```

`cmake --install` puts the program in the menu under *Webcam Background Changer*. It
can also be run from the build directory without installing:
`./build/release/wbc`.

### Debian package

A `.deb` is built with CPack. It installs under `/usr`, sets up the virtual
camera at install time, recommends `v4l2loopback-dkms`, and removes its
module configuration on purge:

```bash
cmake --preset release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build --preset release
(cd build/release && cpack -G DEB)
sudo apt install ./build/release/wbc_*.deb
```

Build it on Ubuntu 26.04, where ONNX Runtime is a package the `.deb` can
depend on. On 24.04 it builds, but neither declares nor carries the copy in
`/opt/onnxruntime`, so it only runs where that copy is present too.

### Removing it

Installed from the Debian package:

```bash
sudo apt purge wbc
```

`purge` also deletes `/etc/modules-load.d/wbc.conf` and
`/etc/modprobe.d/wbc.conf`, so the virtual camera is no longer created at
boot; `apt remove` keeps them. Installed with `cmake --install`, delete what
it installed, as listed in the build directory, and the same two files:

```bash
xargs -d '\n' sudo rm -f < build/release/install_manifest.txt
sudo rm -f /etc/modules-load.d/wbc.conf /etc/modprobe.d/wbc.conf
```

For an installation in `~/.local`, run the first line without `sudo`. Either
way the module stays loaded until the next reboot, or until
`sudo modprobe -r v4l2loopback` while nothing is using it. Your settings are
in `~/.config/wbc/`.

## Setting up the virtual camera

The program needs a v4l2loopback device labelled `Webcam Background Changer`.
Setting it up is a one-time step: the module is then loaded at every boot, so
the device survives reboots and no password is asked for again.

- **Installed from the Debian package**, it is done at install time. If
  v4l2loopback has not been built yet when the package is configured, the
  setting is saved all the same and the device appears after the next reboot.
- **Otherwise** the window says the virtual camera is not set up and offers a
  button. It asks for your password once, in the desktop's usual dialog, and
  runs `wbc-setup-loopback`, which writes `/etc/modules-load.d/wbc.conf` and
  `/etc/modprobe.d/wbc.conf` and loads the module.
- **Where permission cannot be had that way** -- a desktop with no polkit
  agent, such as a bare i3 or Sway, an account that is refused, or inside
  Flatpak -- the window shows the command to run in a terminal instead, with a
  button to copy it.

If v4l2loopback is already loaded by another program, OBS's virtual camera
for instance, it is not unloaded: the setting is saved and the device appears
after the next reboot.

The password dialog names this program only when it was installed under
`/usr`: by the Debian package, or from a build configured with
`-DCMAKE_INSTALL_PREFIX=/usr`. polkit reads its actions from
`/usr/share/polkit-1/actions` alone, and the action names the helper by the
path the build was configured with, so a `--prefix` given to `cmake --install`
is not enough. Installed anywhere else, under `/usr/local` or `~/.local`, the
prompt is the generic one and the setup works all the same: the program finds
its helper beside itself.

A Debug build run from the IDE or the build directory, without installing
it, uses `packaging/wbc-setup-loopback` from the source tree, also with the
generic prompt. A Release build does not, as it is the one that gets
installed; `-DWBC_SOURCE_TREE_HELPER` decides.

To set it up by hand, with the program installed:

```bash
sudo /usr/libexec/wbc/wbc-setup-loopback "Webcam Background Changer" 10
```

or without it, which is what that does:

```bash
sudo modprobe v4l2loopback devices=1 video_nr=10 \
     card_label="Webcam Background Changer" exclusive_caps=1
echo v4l2loopback | sudo tee /etc/modules-load.d/wbc.conf
echo 'options v4l2loopback devices=1 video_nr=10 card_label="Webcam Background Changer" exclusive_caps=1' \
    | sudo tee /etc/modprobe.d/wbc.conf
```

`exclusive_caps=1` is required. Without it the device claims to be a camera
and a display at the same time, and Chrome and Zoom refuse to list it.

Every application that shows the virtual camera needs the module. Zoom,
Discord, Telegram and Chrome read V4L2 devices directly, so for them the
v4l2loopback device is the virtual camera; Firefox, OBS and Flatpak or Snap
applications reach the same device through PipeWire.

## Using it

1. Start Webcam Background Changer, choose your camera and a background.
2. Press **Start**.
3. In the other application, choose the camera named **Webcam Background Changer**.

A video background plays in a loop, at its own speed whatever the camera's,
and without sound -- a camera carries none. It is decoded on a thread of its
own, so a large file slows the rest of the program only by the processor time
it takes to decode, not by holding up each frame. If the machine cannot keep
up, the video plays slower rather than jerkily.

The order matters. Because of `exclusive_caps=1`, the device only presents
itself as a camera while this program is sending frames to it. An application
that was already open when you pressed Start built its camera list without
it, and needs restarting before it will see it. The window says the same.

The window fits screens down to 1024x600. Starting and stopping, choosing the
camera and the messages are always in view; the settings sit in two tabs,
which scroll on a screen too short to show them whole. The window opens where
it was last left, or at a size that suits the screen.

### The mask controls

The model is not perfect, and what it gets wrong depends on the room, the
light and the camera, so the three settings that matter are on screen:

- **Cut off background below** removes the faint outline the model sometimes
  leaves on furniture and walls. Start around 20-35%. Too high, and it starts
  removing hair and the edges of your shoulders as well.
- **Steady the mask over time** stops it flickering. Higher is steadier, and
  slower to follow when you move.
- **Follow the edges in the frame** pulls the mask onto the real outline of
  the person. If parts of the room show through along their own edges, lower
  it. Zero turns it off.

All three take effect immediately and are remembered.

## Languages

The window is available in English, Russian, German, French and Spanish, and
so are the error messages that come from the camera, the virtual camera and
the kernel module helper -- those are built inside classes that predate the
window, so they ask `QCoreApplication::translate` rather than `tr`.

The language is chosen from the box at the bottom of the window and takes
effect immediately. With no choice made, the system's language is used if it
is one that ships; otherwise English.

The German, French and Spanish translations have not been reviewed by a
native speaker. Corrections are welcome and are a single file each.

### Adding or correcting a language

Translations live in `i18n/` as Qt Linguist `.ts` files, which are the source
of truth; the `.qm` files the application loads are compiled during the build
and embedded in the binary, so nothing has to be installed beside it.

```bash
# after changing any translatable string in src/
lupdate -recursive src -ts i18n/wbc_ru.ts

# then edit i18n/wbc_ru.ts, by hand or in Qt Linguist
linguist i18n/wbc_ru.ts
```

A new language needs its code added to `WBC_LANGUAGES` in `CMakeLists.txt` and
to `AvailableLanguages()` in `src/core/Translations.cpp`, with its own name in
`LanguageDisplayName`.

`lrelease` is looked for at configure time and is optional: without it the
build succeeds and the application speaks English only.

## Troubleshooting

**Another application says the camera is busy.** A camera can be read by one
program at a time. wbc holds your real camera while it runs, so other programs
should select *Webcam Background Changer*, not the webcam itself. If the error
is about Webcam Background Changer, check that nothing else — often a browser tab
left open — still holds the real camera: `fuser -v /dev/video*` names it.

**Webcam Background Changer does not appear in the other application.** Press Start
first, then restart the other application; see [Using it](#using-it).

**Only the background is shown; the person is gone.** See the note on locales
below.

**Reading the status line.** It shows frames a second, the time spent
processing each frame, and how many the virtual camera dropped. The time does
not include waiting for the camera, so a low frame rate beside a small time --
7 frames a second at 23 ms, say -- means the camera is what sets the pace, not
this program.

**The frame rate halves in a dim room.** Many webcams lengthen their
exposure in poor light and deliver fewer frames to do it -- one tested here
went from 30 to about 17 frames a second after dark. Nothing in this program
can make the camera faster; its own setting can, at the cost of a darker and
noisier picture:

```bash
v4l2-ctl -d /dev/video0 -c exposure_dynamic_framerate=0
```

**The model was not found.** The window lists every place it looked. Set
`WBC_MODEL` to the path of a copy to use that one instead.

### A note on locales

If the preview and the virtual camera show only the chosen background — the
person is never separated from it, as if the segmentation mask were zero
everywhere — check the process locale before anything else.

`QApplication` calls `setlocale(LC_ALL, "")` when it is constructed, which
adopts whatever locale the environment specifies. Where that locale uses a
comma as the decimal separator, ONNX Runtime then evaluates the segmentation
model to all zeros.

`LANG` alone does not tell you whether you are affected. Desktop environments
set the regional-format categories separately, so `LANG=en_US.UTF-8` can sit
alongside `LC_NUMERIC=en_DK.UTF-8` — which is exactly how this fault first
appeared here. Ask for the separator itself rather than reading locale names:

```bash
locale -k decimal_point
```

Forcing `LC_NUMERIC` back to `"C"` after `QApplication` is constructed removes
the fault; `main.cpp` does this via `wbc::UseCNumericLocale()`
(`src/core/CNumericLocale.h`). Qt's own number formatting goes through
`QLocale` and is unaffected by `LC_NUMERIC`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for building the test configurations,
the coverage gates, and how translations are kept, and
[docs/engineering-notes.md](docs/engineering-notes.md) for the decisions
behind the code: where a frame's time went, the threads and sanitizers, and
the faults that were hardest to find.

## Licence

wbc is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version. See [LICENSE](LICENSE).

The segmentation model is Google's, under the Apache License 2.0, and is
downloaded during the build rather than included here. It and the libraries
this program uses are listed, with their licences, in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
