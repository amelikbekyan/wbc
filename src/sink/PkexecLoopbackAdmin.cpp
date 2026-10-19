// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sink/PkexecLoopbackAdmin.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <filesystem>

namespace {

// The label is interpolated into a command string that is executed through
// a shell, so a quote or a semicolon in it would end the quoted argument
// and start a new command. Nothing today lets a user supply one — the
// factory always passes the compile-time default — but a settings field
// for renaming the virtual camera would, and sanitising here means the
// object cannot hold a dangerous label no matter who constructs it.
std::string SanitiseLabel(const std::string& label) {
    std::string safe;
    safe.reserve(label.size());
    for (const char c : label) {
        const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                             (c >= '0' && c <= '9') || c == ' ' || c == '_';
        if (allowed) {
            safe.push_back(c);
        }
    }
    return safe;
}

// The helper's path comes from the install prefix, which may hold a space
// (a home directory) and is put into a command run through a shell.
std::string QuoteForShell(const std::string& word) {
    const bool plain = std::all_of(word.begin(), word.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '/' || c == '.' || c == '_' || c == '-';
    });
    if (plain) {
        return word;
    }
    std::string quoted = "'";
    for (const char c : word) {
        quoted += c == '\'' ? std::string{"'\\''"} : std::string(1, c);
    }
    return quoted + "'";
}

// Both before asking for a password and after the helper reported it.
std::string NotInstalledMessage(const std::string& installCommand) {
    return QCoreApplication::translate("PkexecLoopbackAdmin",
                                       "The v4l2loopback kernel module is not installed on this "
                                       "system. Install it with: %1")
        .arg(QString::fromStdString(installCommand))
        .toStdString();
}

}  // namespace

namespace wbc {

PkexecLoopbackAdmin::PkexecLoopbackAdmin(DeviceListFn devices,
                                         ModuleAvailableFn moduleAvailable,
                                         RunCommandFn run,
                                         std::string label,
                                         int videoNumber,
                                         HelperPathFn helperPath,
                                         FlatpakFn inFlatpak)
    : devices_(std::move(devices)),
      moduleAvailable_(std::move(moduleAvailable)),
      run_(std::move(run)),
      label_(SanitiseLabel(label)),
      videoNumber_(videoNumber),
      helperPath_(std::move(helperPath)),
      inFlatpak_(std::move(inFlatpak)) {}

LoopbackState PkexecLoopbackAdmin::Probe() {
    const auto found = SelectLoopback(devices_(), label_);
    if (found.has_value()) {
        devicePath_ = found->path;
        return LoopbackState::Ready;
    }

    devicePath_.clear();
    return moduleAvailable_() ? LoopbackState::ModuleNotLoaded : LoopbackState::ModuleNotInstalled;
}

std::string PkexecLoopbackAdmin::DevicePath() const {
    return devicePath_;
}

std::string PkexecLoopbackAdmin::ModuleOptions() const {
    // exclusive_caps=1 is not optional: without it the node advertises capture
    // and output at once, and Chrome and Zoom then refuse to list it.
    return "devices=1 video_nr=" + std::to_string(videoNumber_) + " card_label=\"" + label_ +
           "\" exclusive_caps=1";
}

std::string PkexecLoopbackAdmin::UsableHelper() const {
    if (InFlatpak() || !helperPath_) {
        return {};
    }
    return helperPath_();
}

std::string PkexecLoopbackAdmin::LoadCommand() const {
    // The helper is preferred because it also makes the device come back at
    // every boot, and because pkexec then shows the polkit action's own
    // message rather than asking to run modprobe as root. modprobe remains for
    // a build that is run without being installed.
    const std::string helper = UsableHelper();
    if (!helper.empty()) {
        return "pkexec " + QuoteForShell(helper) + " \"" + label_ + "\" " +
               std::to_string(videoNumber_);
    }
    return "pkexec modprobe v4l2loopback " + ModuleOptions();
}

std::string PkexecLoopbackAdmin::ManualCommand() const {
    const std::string helper = UsableHelper();
    if (!helper.empty()) {
        return "sudo " + QuoteForShell(helper) + " \"" + label_ + "\" " +
               std::to_string(videoNumber_);
    }
    // What the helper does, spelled out, under the names it would use.
    return "sudo modprobe v4l2loopback " + ModuleOptions() +
           "\n"
           "echo v4l2loopback | sudo tee /etc/modules-load.d/wbc.conf\n"
           "echo 'options v4l2loopback " +
           ModuleOptions() + "' | sudo tee /etc/modprobe.d/wbc.conf";
}

bool PkexecLoopbackAdmin::InFlatpak() const {
    return inFlatpak_ && inFlatpak_();
}

bool PkexecLoopbackAdmin::NeedsManualSetup() const {
    return needsManualSetup_;
}

std::string PkexecLoopbackAdmin::InstallCommand() const {
    return "sudo apt install v4l2loopback-dkms";
}

bool PkexecLoopbackAdmin::LoadModule() {
    if (!moduleAvailable_()) {
        lastError_ = NotInstalledMessage(InstallCommand());
        return false;
    }

    const CommandResult result = run_(LoadCommand());
    switch (result.exitCode) {
        case 0:
            break;

        case kHelperModuleMissing:
            lastError_ = NotInstalledMessage(InstallCommand());
            return false;

        case kHelperModuleLoadedByOthers:
            lastError_ = QCoreApplication::translate(
                             "PkexecLoopbackAdmin",
                             "v4l2loopback is already loaded by another program, without this "
                             "program's virtual camera. It was left as it is; the virtual "
                             "camera will be there after the next reboot.")
                             .toStdString();
            return false;

        case kPkexecCannotAuthorise:
            needsManualSetup_ = true;
            lastError_ = QCoreApplication::translate(
                             "PkexecLoopbackAdmin",
                             "Could not get permission to set up the virtual camera. Run "
                             "this in a terminal instead:\n%1")
                             .arg(QString::fromStdString(ManualCommand()))
                             .toStdString();
            return false;

        case kPkexecDismissed:
            lastError_ = QCoreApplication::translate(
                             "PkexecLoopbackAdmin",
                             "Loading the virtual camera module was cancelled or failed")
                             .toStdString();
            return false;

        default:
            lastError_ = result.error.empty()
                             ? QCoreApplication::translate(
                                   "PkexecLoopbackAdmin",
                                   "Loading the virtual camera module was cancelled or failed")
                                   .toStdString()
                             : result.error;
            return false;
    }

    // A command that returns success without producing a device has failed in
    // a way the exit status did not show.
    if (Probe() != LoopbackState::Ready) {
        lastError_ = QCoreApplication::translate(
                         "PkexecLoopbackAdmin",
                         "The module was loaded but no device labelled %1 appeared")
                         .arg(QString::fromStdString(label_))
                         .toStdString();
        return false;
    }

    lastError_.clear();
    return true;
}

std::string PkexecLoopbackAdmin::LastError() const {
    return lastError_;
}

bool V4l2LoopbackModuleIsAvailable(const std::string& sysfsPath, const std::string& moduleName) {
    std::error_code error;
    if (std::filesystem::exists(sysfsPath, error)) {
        return true;
    }
    // Installed but never loaded: modinfo finds it in the module tree.
    QProcess modinfo;
    modinfo.start("modinfo", QStringList{QString::fromStdString(moduleName)});
    modinfo.waitForFinished(5000);
    return modinfo.exitStatus() == QProcess::NormalExit && modinfo.exitCode() == 0;
}

CommandResult RunAuthorisedCommand(const std::string& command, int timeoutMs) {
    QProcess process;
    process.start("/bin/sh", QStringList{"-c", QString::fromStdString(command)});
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        return {-1, "The authorisation dialog did not finish in time"};
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        return {-1, "The authorisation helper crashed"};
    }
    if (process.exitCode() != 0) {
        const QString output = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {process.exitCode(),
                output.isEmpty() ? "Authorisation was refused" : output.toStdString()};
    }
    return {0, {}};
}

std::string FindSetupHelper(const std::string& path) {
    const QFileInfo helper(QString::fromStdString(path));
    return helper.isFile() && helper.isExecutable() ? path : std::string{};
}

std::vector<std::string> SetupHelperCandidates(const std::string& applicationDirectory) {
    // Returned without a local: a named vector would put an unwinding landing
    // pad on the closing brace, a line gcov counts and no test can reach.
    return {
        // Normalised, because pkexec matches the polkit action's exec.path
        // against the path as given, and /usr/bin/../libexec is not
        // /usr/libexec to it.
        applicationDirectory.empty()
            ? std::string{}
            : (std::filesystem::path{applicationDirectory} / WBC_SETUP_HELPER_FROM_BIN)
                  .lexically_normal()
                  .string(),
        WBC_SETUP_HELPER_PATH,
#ifdef WBC_SOURCE_SETUP_HELPER_PATH
        // The polkit action names only the installed path, so pkexec shows
        // its generic prompt for this one. The setup it does is the same, and
        // it lasts past a reboot, which the modprobe fallback does not.
        WBC_SOURCE_SETUP_HELPER_PATH,
#endif
    };
}

std::string LocateSetupHelper(const std::vector<std::string>& candidates) {
    for (const std::string& candidate : candidates) {
        std::string found = FindSetupHelper(candidate);
        if (!found.empty()) {
            return found;
        }
    }
    return {};
}

std::string LocateSetupHelper() {
    return LocateSetupHelper(SetupHelperCandidates(
        QCoreApplication::instance() != nullptr
            ? QCoreApplication::applicationDirPath().toStdString()
            : std::string{}));
}

bool RunningInFlatpak(const std::string& marker) {
    std::error_code error;
    return std::filesystem::exists(marker, error);
}

std::unique_ptr<PkexecLoopbackAdmin> MakeDefaultLoopbackAdmin() {
    return std::make_unique<PkexecLoopbackAdmin>(
        [] { return EnumerateDevices(); },
        [] { return V4l2LoopbackModuleIsAvailable(); },
        [](const std::string& command) { return RunAuthorisedCommand(command); },
        kVirtualCameraLabel, 10, [] { return LocateSetupHelper(); }, [] { return RunningInFlatpak(); });
}

}  // namespace wbc
