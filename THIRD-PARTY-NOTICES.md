# Third-party notices

wbc (Webcam Background Changer) is released under the GNU General Public License version 3
or later; see [LICENSE](LICENSE). It uses the following work, none of which it
includes in its own source tree.

| Component | Used for | Licence |
|---|---|---|
| [Qt 6](https://www.qt.io/) (Core, Gui, Widgets) | the window, settings, translations | LGPL-3.0 or GPL-2.0 |
| [OpenCV](https://opencv.org/) | capture, filtering, compositing | Apache-2.0 and BSD-3-Clause |
| [OpenCV contrib](https://github.com/opencv/opencv_contrib) (`ximgproc`) | the guided filter used to refine mask edges | Apache-2.0 and BSD-3-Clause |
| [ONNX Runtime](https://onnxruntime.ai/) | running the segmentation model | MIT |
| [MediaPipe Selfie Segmentation](https://huggingface.co/onnx-community/mediapipe_selfie_segmentation) | the segmentation model's weights | Apache-2.0 |
| [GoogleTest](https://github.com/google/googletest) | the tests only; not distributed | BSD-3-Clause |
| [v4l2loopback](https://github.com/umlaeute/v4l2loopback) | the kernel module providing the virtual camera | GPL-2.0 |

Qt is linked dynamically, which is what the LGPL asks of a program that is
not itself under the LGPL: anyone receiving this program can replace the Qt it
uses with their own build. A statically linked Qt would put that obligation on
whoever links it.

v4l2loopback is a kernel module. This program writes frames into the device
node it creates and never links against it.

The model's weights are downloaded during the build rather than committed.
Its licence and notice travel with this repository in
[`third_party/mediapipe-selfie-segmentation/`](third_party/mediapipe-selfie-segmentation/)
and are installed alongside the model, so a copy of the program always carries
the terms the model came with.
