# Changes

## 0.1.0

The first release.

- Blurs the background of a webcam, or replaces it with a picture, a video
  or a solid colour, and offers the result as a virtual camera that video
  calls, OBS and browsers can choose.
- Runs on the processor: a 1280x720 frame takes about 13 ms, for about half
  of one core.
- Sets up the virtual camera, a v4l2loopback device, once, while the package
  installs or with one password prompt, and it comes back at every boot.
- In English, Russian, German, French and Spanish.
- Packages for Ubuntu 26.04, Debian 13 and Fedora 44, and a `PKGBUILD` for
  Arch Linux. How to install each is in the
  [README](https://github.com/amelikbekyan/wbc#installing); check the
  downloads against `SHA256SUMS`.
