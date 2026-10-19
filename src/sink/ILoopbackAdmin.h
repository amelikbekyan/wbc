// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace wbc {

enum class LoopbackState {
    Ready,                // our device exists and can be written to
    ModuleNotLoaded,      // v4l2loopback is installed but no device was created
    ModuleNotInstalled,   // the kernel module is not on this machine at all
};

// Finds the virtual camera device and, with the user's authorisation, creates
// it. The application never becomes root itself and never installs packages:
// it can ask pkexec to run the setup helper, and for anything more it shows
// the user the command to run.
class ILoopbackAdmin {
public:
    virtual ~ILoopbackAdmin() = default;

    virtual LoopbackState Probe() = 0;
    virtual std::string DevicePath() const = 0;
    virtual bool LoadModule() = 0;
    virtual std::string LastError() const = 0;
    virtual std::string InstallCommand() const = 0;
    virtual std::string LoadCommand() const = 0;
    // What LoadModule() does, as commands to type into a terminal, for when
    // the window cannot ask for the password itself. Unlike LoadCommand() it
    // also makes the device come back after a reboot.
    virtual std::string ManualCommand() const = 0;
    // Inside a Flatpak sandbox neither pkexec nor the helper reaches the host,
    // so the only way to set the device up is from outside.
    virtual bool InFlatpak() const = 0;
    // True once pkexec has turned out unable to get permission -- no polkit
    // agent, as under a bare i3 or Sway session, authorisation denied, or no
    // pkexec. Asking again would fail the same way.
    virtual bool NeedsManualSetup() const = 0;
};

}  // namespace wbc
