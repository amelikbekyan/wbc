// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "camera/DeviceEnumerator.h"
#include "core/Types.h"
#include "sink/ILoopbackAdmin.h"

namespace wbc {

// Everything that touches the outside world enters through a callable, so
// every branch can be exercised without a kernel module, a password prompt or
// a device node.
// What a command run on the user's behalf came to. The exit code is what tells
// the helper's outcomes and pkexec's own failures apart; -1 means the command
// never reached one, because it timed out or was killed.
struct CommandResult {
    int exitCode = -1;
    // Something worth showing the user when the command failed: what it wrote
    // to standard error, or a sentence saying what went wrong.
    std::string error;
};

class PkexecLoopbackAdmin final : public ILoopbackAdmin {
public:
    using DeviceListFn = std::function<std::vector<DeviceInfo>()>;
    using ModuleAvailableFn = std::function<bool()>;
    using RunCommandFn = std::function<CommandResult(const std::string&)>;
    // The installed setup helper's path, or empty when it is not installed,
    // as when running from the build directory.
    using HelperPathFn = std::function<std::string()>;
    using FlatpakFn = std::function<bool()>;

    // Without a helper or a Flatpak test, the admin behaves as it did before
    // the helper existed: it loads the module with modprobe until the next
    // reboot, and assumes it is not sandboxed.
    PkexecLoopbackAdmin(DeviceListFn devices,
                        ModuleAvailableFn moduleAvailable,
                        RunCommandFn run,
                        std::string label = kVirtualCameraLabel,
                        int videoNumber = 10,
                        HelperPathFn helperPath = {},
                        FlatpakFn inFlatpak = {});

    LoopbackState Probe() override;
    std::string DevicePath() const override;
    bool LoadModule() override;
    std::string LastError() const override;
    std::string InstallCommand() const override;
    std::string LoadCommand() const override;
    std::string ManualCommand() const override;
    bool InFlatpak() const override;
    bool NeedsManualSetup() const override;

    // The helper's exit codes, documented in packaging/wbc-setup-loopback,
    // and the two pkexec uses for its own failures.
    static constexpr int kHelperModuleMissing = 2;
    static constexpr int kHelperModuleLoadedByOthers = 3;
    static constexpr int kPkexecDismissed = 126;
    // pkexec's answer both when no authentication agent is running and when
    // authorisation is denied, as for a user who is not an administrator; it
    // does not say which. Also what the shell returns when pkexec itself is
    // not installed. All three call for the same answer.
    static constexpr int kPkexecCannotAuthorise = 127;

private:
    // The helper's path when it can be used: installed, and reachable, which
    // from inside a Flatpak sandbox it is not.
    std::string UsableHelper() const;
    std::string ModuleOptions() const;

    DeviceListFn devices_;
    ModuleAvailableFn moduleAvailable_;
    RunCommandFn run_;
    std::string label_;
    int videoNumber_;
    HelperPathFn helperPath_;
    FlatpakFn inFlatpak_;
    std::string devicePath_;
    std::string lastError_;
    bool needsManualSetup_ = false;
};

// True when v4l2loopback can be loaded: either it already is, which the
// sysfs entry shows, or modinfo finds it in the module tree. The paths are
// parameters so that both branches can be exercised without depending on
// what happens to be installed on the machine running the tests.
bool V4l2LoopbackModuleIsAvailable(const std::string& sysfsPath = "/sys/module/v4l2loopback",
                                   const std::string& moduleName = "v4l2loopback");

// Runs a command through /bin/sh and reports its exit code and what went
// wrong. The timeout is generous by default because the user has to type a
// password into pkexec's dialog; tests pass a short one.
CommandResult RunAuthorisedCommand(const std::string& command, int timeoutMs = 120000);

// The setup helper's path if an executable file is there, otherwise empty.
// The default is where this build installs it.
std::string FindSetupHelper(const std::string& path = WBC_SETUP_HELPER_PATH);

// Where the helper is looked for, in order: beside the executable in
// `applicationDirectory`, as <prefix>/bin/wbc finds <prefix>/libexec/wbc/;
// where this build installs it; and, in a build configured with
// WBC_SOURCE_TREE_HELPER, the copy in the source tree. With no directory the
// first candidate is empty, which nothing is found at.
std::vector<std::string> SetupHelperCandidates(const std::string& applicationDirectory);

// The first candidate that FindSetupHelper accepts, or empty.
std::string LocateSetupHelper(const std::vector<std::string>& candidates);

// What the application calls: the candidates for the running executable.
std::string LocateSetupHelper();

// Flatpak puts this file at the root of every sandbox it starts.
bool RunningInFlatpak(const std::string& marker = "/.flatpak-info");

// Wired to the real system: enumerates /dev, looks for the module under
// /sys/module and in the module tree, finds the helper, and runs
// pkexec through QProcess.
std::unique_ptr<PkexecLoopbackAdmin> MakeDefaultLoopbackAdmin();

}  // namespace wbc
