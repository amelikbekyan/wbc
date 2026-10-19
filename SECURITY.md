# Security

## Reporting a vulnerability

Please report a vulnerability privately rather than in a public issue: by
email to arthur.melikbekyan@protonmail.com, or through GitHub's private
vulnerability reporting on this repository (the *Security* tab, *Report a
vulnerability*).

Say what an attacker can do, on which version or commit, and how to
reproduce it. This is a project maintained by one person, so a reply may take
a few days. Once a fix is released the report is credited in the release
notes, unless you ask not to be named.

Only the latest release, and the `main` branch, receive fixes.

## What runs with privileges

Most of wbc runs as the user who starts it. One part does not, and it is
where a report is most valuable:

- `packaging/wbc-setup-loopback.in`, installed to `libexec/wbc/`, runs as root:
  through `pkexec` when the window sets up the virtual camera, from the
  Debian package's `postinst`, or by hand with `sudo`. It writes
  `/etc/modules-load.d/wbc.conf` and `/etc/modprobe.d/wbc.conf` and runs
  `modprobe`. Its two arguments, the device label and number, are checked
  against a fixed set of characters before they are used.
- The polkit action `org.wbc.setup-loopback` allows that helper, and only it,
  to run as root after an administrator's password, for a user at the
  machine.

Anything that lets someone run other code as root through these, write
other files, or reach them without that password is in scope. So is a way
to make the unprivileged program run code it should not, for instance
through a crafted background image, video or settings file.

Out of scope: weaknesses in the libraries wbc uses (Qt, OpenCV, ONNX
Runtime) or in v4l2loopback itself, which belong with those projects, unless
wbc uses them in a way that makes them exploitable.
