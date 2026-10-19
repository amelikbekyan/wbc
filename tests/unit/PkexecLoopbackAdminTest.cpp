// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "sink/ModuleInstallCommand.h"
#include "sink/PkexecLoopbackAdmin.h"

namespace {

std::vector<wbc::DeviceInfo> NoDevices() { return {}; }

std::vector<wbc::DeviceInfo> OnlyACamera() {
    return {{"/dev/video0", "USB2.0 HD UVC WebCam", true, false}};
}

std::vector<wbc::DeviceInfo> CameraAndLoopback() {
    return {{"/dev/video0", "USB2.0 HD UVC WebCam", true, false},
            {"/dev/video10", wbc::kVirtualCameraLabel, false, true}};
}

wbc::CommandResult NeverRuns(const std::string&) {
    ADD_FAILURE() << "the command should not have been run";
    return {1, "the command should not have been run"};
}

constexpr const char* kHelper = "/usr/libexec/wbc/wbc-setup-loopback";

std::string InstalledHelper() { return kHelper; }
std::string NoHelper() { return {}; }
bool Sandboxed() { return true; }
bool NotSandboxed() { return false; }

// An admin whose helper is installed and whose command always exits with
// `code`, for the tests that are about what each exit code means.
struct ExitingWith {
    explicit ExitingWith(int code, std::string error = {})
        : admin{OnlyACamera, [] { return true; },
                [this, code, error](const std::string& command) {
                    commands.push_back(command);
                    return wbc::CommandResult{code, error};
                },
                wbc::kVirtualCameraLabel, 10, InstalledHelper, NotSandboxed} {}

    std::vector<std::string> commands;
    wbc::PkexecLoopbackAdmin admin;
};

}  // namespace

TEST(PkexecLoopbackAdmin, ReportsReadyWhenOurDeviceIsAlreadyThere) {
    wbc::PkexecLoopbackAdmin admin{CameraAndLoopback, [] { return true; }, NeverRuns};
    EXPECT_EQ(admin.Probe(), wbc::LoopbackState::Ready);
    EXPECT_EQ(admin.DevicePath(), "/dev/video10");
}

TEST(PkexecLoopbackAdmin, ReportsTheModuleAsNotLoadedWhenItIsInstalledButNoDeviceExists) {
    wbc::PkexecLoopbackAdmin admin{OnlyACamera, [] { return true; }, NeverRuns};
    EXPECT_EQ(admin.Probe(), wbc::LoopbackState::ModuleNotLoaded);
    EXPECT_TRUE(admin.DevicePath().empty());
}

TEST(PkexecLoopbackAdmin, ReportsTheModuleAsMissingWhenItIsNotInstalledAtAll) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return false; }, NeverRuns};
    EXPECT_EQ(admin.Probe(), wbc::LoopbackState::ModuleNotInstalled);
}

TEST(PkexecLoopbackAdmin, TheLoadCommandCarriesEveryOptionTheDeviceNeeds) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns};
    const std::string command = admin.LoadCommand();

    EXPECT_NE(command.find("pkexec"), std::string::npos);
    EXPECT_NE(command.find("modprobe v4l2loopback"), std::string::npos);
    EXPECT_NE(command.find("devices=1"), std::string::npos);
    EXPECT_NE(command.find("video_nr=10"), std::string::npos);
    EXPECT_NE(command.find(std::string{"card_label=\""} + wbc::kVirtualCameraLabel + "\""),
              std::string::npos);
    // Without this browsers and Zoom will not list the device at all.
    EXPECT_NE(command.find("exclusive_caps=1"), std::string::npos);
}

// Each package is built with its own distribution's command; the window
// must show that one, not a hard-coded apt line.
TEST(PkexecLoopbackAdmin, TheInstallCommandIsTheOneThisBuildWasConfiguredWith) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return false; }, NeverRuns};
    EXPECT_EQ(admin.InstallCommand(), WBC_MODULE_INSTALL_COMMAND);
}

TEST(PkexecLoopbackAdmin, TheInstallCommandNamesTheModuleAndDoesNotRunItself) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return false; }, NeverRuns};
    EXPECT_NE(admin.InstallCommand().find("v4l2loopback"), std::string::npos);
}

TEST(PkexecLoopbackAdmin, LoadingRunsTheCommandAndRepeatsTheProbe) {
    bool loaded = false;
    auto devices = [&loaded] { return loaded ? CameraAndLoopback() : OnlyACamera(); };
    auto run = [&loaded](const std::string&) {
        loaded = true;
        return wbc::CommandResult{0, {}};
    };

    wbc::PkexecLoopbackAdmin admin{devices, [] { return true; }, run};
    ASSERT_EQ(admin.Probe(), wbc::LoopbackState::ModuleNotLoaded);
    EXPECT_TRUE(admin.LoadModule());
    EXPECT_EQ(admin.Probe(), wbc::LoopbackState::Ready);
    EXPECT_EQ(admin.DevicePath(), "/dev/video10");
}

TEST(PkexecLoopbackAdmin, ARefusedAuthorisationIsReportedAndNotRetried) {
    int runCount = 0;
    auto run = [&runCount](const std::string&) {
        ++runCount;
        return wbc::CommandResult{1, "Request dismissed"};
    };

    wbc::PkexecLoopbackAdmin admin{OnlyACamera, [] { return true; }, run};
    EXPECT_FALSE(admin.LoadModule());
    EXPECT_EQ(runCount, 1);
    EXPECT_NE(admin.LastError().find("Request dismissed"), std::string::npos);
}

TEST(PkexecLoopbackAdmin, LoadingRefusesOutrightWhenTheModuleIsNotInstalled) {
    int runCount = 0;
    auto run = [&runCount](const std::string&) {
        ++runCount;
        return wbc::CommandResult{0, {}};
    };

    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return false; }, run};
    EXPECT_FALSE(admin.LoadModule());
    EXPECT_EQ(runCount, 0) << "asking for a password to load a module that does not exist is rude";
    EXPECT_FALSE(admin.LastError().empty());
}

TEST(PkexecLoopbackAdmin, ACommandThatSucceedsButProducesNoDeviceIsReportedAsAFailure) {
    wbc::PkexecLoopbackAdmin admin{OnlyACamera, [] { return true; }, [](const std::string&) {
                                      return wbc::CommandResult{0, {}};
                                  }};
    EXPECT_FALSE(admin.LoadModule());
    EXPECT_FALSE(admin.LastError().empty());
}

TEST(PkexecLoopbackAdmin, TheDeviceNumberAndLabelAreConfigurable) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns, "OtherLabel", 42};
    EXPECT_NE(admin.LoadCommand().find("video_nr=42"), std::string::npos);
    EXPECT_NE(admin.LoadCommand().find("card_label=\"OtherLabel\""), std::string::npos);
}

TEST(PkexecLoopbackAdmin, StripsShellMetacharactersFromTheLabel) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  "Evil\"; rm -rf /; echo \"", 10};
    const std::string command = admin.LoadCommand();

    EXPECT_EQ(command.find(';'), std::string::npos) << "a semicolon would start a new command";
    EXPECT_EQ(command.find("rm -rf"), std::string::npos);
    // Exactly two quotes remain: the ones this code puts around the label.
    EXPECT_EQ(std::count(command.begin(), command.end(), '"'), 2);
}

TEST(PkexecLoopbackAdmin, ASuccessfulLoadClearsTheErrorFromAFailedOne) {
    bool loaded = false;
    bool shouldSucceed = false;
    auto devices = [&loaded] { return loaded ? CameraAndLoopback() : OnlyACamera(); };
    auto run = [&loaded, &shouldSucceed](const std::string&) {
        if (!shouldSucceed) {
            return wbc::CommandResult{1, "Request dismissed"};
        }
        loaded = true;
        return wbc::CommandResult{0, {}};
    };

    wbc::PkexecLoopbackAdmin admin{devices, [] { return true; }, run};
    ASSERT_FALSE(admin.LoadModule());
    ASSERT_FALSE(admin.LastError().empty());

    shouldSucceed = true;
    ASSERT_TRUE(admin.LoadModule());
    EXPECT_TRUE(admin.LastError().empty()) << "the earlier failure's text outlived it";
}

TEST(PkexecLoopbackAdmin, ADeviceThatDisappearsClearsTheRememberedPath) {
    bool present = true;
    auto devices = [&present] { return present ? CameraAndLoopback() : OnlyACamera(); };

    wbc::PkexecLoopbackAdmin admin{devices, [] { return true; }, NeverRuns};
    ASSERT_EQ(admin.Probe(), wbc::LoopbackState::Ready);
    ASSERT_EQ(admin.DevicePath(), "/dev/video10");

    present = false;
    EXPECT_EQ(admin.Probe(), wbc::LoopbackState::ModuleNotLoaded);
    EXPECT_TRUE(admin.DevicePath().empty()) << "a path from a previous probe survived";
}

// The factory's own wiring: these are the two functions that touch the system,
// exercised through their parameters rather than through pkexec itself.

TEST(V4l2LoopbackModuleIsAvailable, IsTrueWhenTheSysfsEntryExists) {
    // "/sys/module" is present on every running Linux kernel, so this asks the
    // "already loaded" branch without depending on v4l2loopback being loaded.
    EXPECT_TRUE(wbc::V4l2LoopbackModuleIsAvailable("/sys/module", "v4l2loopback"));
}

TEST(V4l2LoopbackModuleIsAvailable, FallsBackToModinfoAndReportsAModuleThatDoesNotExist) {
    EXPECT_FALSE(wbc::V4l2LoopbackModuleIsAvailable("/sys/module/wbc_no_such_module_exists",
                                                   "wbc_no_such_module_exists"));
}

TEST(RunAuthorisedCommand, ReportsSuccessForACommandThatExitsCleanly) {
    const wbc::CommandResult result = wbc::RunAuthorisedCommand("true");
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.error.empty());
}

TEST(RunAuthorisedCommand, TreatsASilentNonZeroExitAsARefusalAndKeepsItsCode) {
    const wbc::CommandResult result = wbc::RunAuthorisedCommand("exit 3");
    EXPECT_EQ(result.exitCode, 3) << "the helper's exit code is what the admin acts on";
    EXPECT_EQ(result.error, "Authorisation was refused");
}

TEST(RunAuthorisedCommand, PrefersWhatTheCommandWroteToStandardError) {
    const wbc::CommandResult result =
        wbc::RunAuthorisedCommand("echo 'not authorised' 1>&2; exit 1");
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.error, "not authorised");
}

TEST(RunAuthorisedCommand, ReportsAProgramThatIsNotThereAsTheShellDoes) {
    // pkexec missing reaches the admin as this: 127, the same code pkexec
    // gives when it cannot authorise, which calls for the same answer.
    EXPECT_EQ(wbc::RunAuthorisedCommand("wbc_no_such_program_exists").exitCode, 127);
}

TEST(RunAuthorisedCommand, ReportsACommandThatOutlivesItsTimeout) {
    const wbc::CommandResult result = wbc::RunAuthorisedCommand("sleep 30", 100);
    EXPECT_EQ(result.exitCode, -1);
    EXPECT_EQ(result.error, "The authorisation dialog did not finish in time");
}

TEST(RunAuthorisedCommand, ReportsACommandKilledBySignal) {
    const wbc::CommandResult result = wbc::RunAuthorisedCommand("kill -SEGV $$");
    EXPECT_EQ(result.exitCode, -1);
    EXPECT_EQ(result.error, "The authorisation helper crashed");
}


// The setup helper, and what each of its outcomes and pkexec's means.

TEST(PkexecLoopbackAdmin, RunsTheInstalledHelperWithTheLabelAndNumber) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns, "OtherLabel", 42,
                                  InstalledHelper, NotSandboxed};
    EXPECT_EQ(admin.LoadCommand(), std::string{"pkexec "} + kHelper + " \"OtherLabel\" 42");
    // pkexec runs the helper itself rather than modprobe, which is what lets
    // polkit show the action's own message.
    EXPECT_EQ(admin.LoadCommand().find("modprobe"), std::string::npos);
}

TEST(PkexecLoopbackAdmin, FallsBackToModprobeWhenTheHelperIsNotInstalled) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10, NoHelper, NotSandboxed};
    EXPECT_NE(admin.LoadCommand().find("pkexec modprobe v4l2loopback"), std::string::npos);
}

TEST(PkexecLoopbackAdmin, QuotesAHelperPathTheShellWouldSplit) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10,
                                  [] { return std::string{"/home/a b/it's/wbc-setup-loopback"}; },
                                  NotSandboxed};
    EXPECT_EQ(admin.LoadCommand(),
              std::string{"pkexec '/home/a b/it'\\''s/wbc-setup-loopback' \""} +
                  wbc::kVirtualCameraLabel + "\" 10");
}

TEST(PkexecLoopbackAdmin, TheManualCommandIsTheSudoFormOfTheHelperCall) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10, InstalledHelper, NotSandboxed};
    EXPECT_EQ(admin.ManualCommand(), std::string{"sudo "} + kHelper + " \"" +
                                         wbc::kVirtualCameraLabel + "\" 10");
}

TEST(PkexecLoopbackAdmin, WithoutTheHelperTheManualCommandSpellsOutWhatItWouldDo) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10, NoHelper, NotSandboxed};
    const std::string command = admin.ManualCommand();
    EXPECT_NE(command.find("sudo modprobe v4l2loopback devices=1 video_nr=10"), std::string::npos);
    EXPECT_NE(command.find("exclusive_caps=1"), std::string::npos);
    // The same two files the helper writes, so that the device comes back
    // after a reboot and a later run of the helper finds them already right.
    EXPECT_NE(command.find("echo v4l2loopback | sudo tee /etc/modules-load.d/wbc.conf"),
              std::string::npos);
    EXPECT_NE(command.find(std::string{"echo 'options v4l2loopback devices=1 video_nr=10 "
                                       "card_label=\""} +
                           wbc::kVirtualCameraLabel +
                           "\" exclusive_caps=1' | sudo tee /etc/modprobe.d/wbc.conf"),
              std::string::npos);
}

TEST(PkexecLoopbackAdmin, InFlatpakTheHelperIsIgnoredBecauseItIsNotOnTheHost) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10, InstalledHelper, Sandboxed};
    EXPECT_TRUE(admin.InFlatpak());
    EXPECT_EQ(admin.ManualCommand().find(kHelper), std::string::npos);
    EXPECT_NE(admin.ManualCommand().find("/etc/modprobe.d/wbc.conf"), std::string::npos);
}

TEST(PkexecLoopbackAdmin, WithoutAFlatpakTestItAssumesItIsNotSandboxed) {
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns};
    EXPECT_FALSE(admin.InFlatpak());
    EXPECT_FALSE(admin.NeedsManualSetup());
}

TEST(PkexecLoopbackAdmin, AHelperThatReportsTheModuleMissingSaysWhatToInstall) {
    ExitingWith run{wbc::PkexecLoopbackAdmin::kHelperModuleMissing};
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_NE(run.admin.LastError().find(WBC_MODULE_INSTALL_COMMAND), std::string::npos);
    EXPECT_FALSE(run.admin.NeedsManualSetup());
}

TEST(PkexecLoopbackAdmin, AModuleLoadedByAnotherProgramIsExplainedAsWaitingForAReboot) {
    ExitingWith run{wbc::PkexecLoopbackAdmin::kHelperModuleLoadedByOthers, "helper's own words"};
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_NE(run.admin.LastError().find("reboot"), std::string::npos);
    EXPECT_FALSE(run.admin.NeedsManualSetup());
}

TEST(PkexecLoopbackAdmin, ADismissedPasswordDialogIsACancellationNotAnError) {
    ExitingWith run{wbc::PkexecLoopbackAdmin::kPkexecDismissed,
                    "Error executing command as another user: Request dismissed"};
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_NE(run.admin.LastError().find("cancelled"), std::string::npos);
    EXPECT_FALSE(run.admin.NeedsManualSetup()) << "the user may simply try again";
}

TEST(PkexecLoopbackAdmin, WhenPkexecCannotGetPermissionTheUserIsGivenTheCommandToRunThemselves) {
    ExitingWith run{wbc::PkexecLoopbackAdmin::kPkexecCannotAuthorise};
    ASSERT_FALSE(run.admin.NeedsManualSetup());
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_TRUE(run.admin.NeedsManualSetup());
    EXPECT_NE(run.admin.LastError().find("terminal"), std::string::npos);
    // 127 is also a denied authorisation, so the message must not claim to
    // know that no dialog appeared: the user may have just seen one.
    EXPECT_EQ(run.admin.LastError().find("dialog"), std::string::npos);
    EXPECT_NE(run.admin.LastError().find(run.admin.ManualCommand()), std::string::npos);
    EXPECT_EQ(run.commands.size(), 1U);
    EXPECT_EQ(run.commands.front().rfind("pkexec ", 0), 0U);
}

TEST(PkexecLoopbackAdmin, AnyOtherFailureShowsWhatTheHelperSaid) {
    ExitingWith run{1, "wbc-setup-loopback: modprobe could not load v4l2loopback"};
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_EQ(run.admin.LastError(), "wbc-setup-loopback: modprobe could not load v4l2loopback");
}

TEST(PkexecLoopbackAdmin, AnyOtherSilentFailureStillSaysSomething) {
    ExitingWith run{-1};
    EXPECT_FALSE(run.admin.LoadModule());
    EXPECT_FALSE(run.admin.LastError().empty());
}

TEST(FindSetupHelper, FindsAnExecutableFile) {
    QTemporaryDir directory;
    const std::string path = directory.filePath("wbc-setup-loopback").toStdString();
    std::ofstream{path} << "#!/bin/sh\n";
    ASSERT_TRUE(QFile::setPermissions(QString::fromStdString(path),
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    EXPECT_EQ(wbc::FindSetupHelper(path), path);
}

TEST(FindSetupHelper, IgnoresAFileThatCannotBeRun) {
    QTemporaryDir directory;
    const std::string path = directory.filePath("wbc-setup-loopback").toStdString();
    std::ofstream{path} << "#!/bin/sh\n";
    EXPECT_TRUE(wbc::FindSetupHelper(path).empty());
}

TEST(FindSetupHelper, IgnoresADirectoryAndAMissingPath) {
    QTemporaryDir directory;
    EXPECT_TRUE(wbc::FindSetupHelper(directory.path().toStdString()).empty());
    EXPECT_TRUE(wbc::FindSetupHelper(directory.filePath("absent").toStdString()).empty());
}

TEST(FindSetupHelper, LooksWhereThisBuildInstallsIt) {
    // Whether it is there depends on the machine; where it is looked for
    // does not.
    EXPECT_EQ(wbc::FindSetupHelper(), wbc::FindSetupHelper(WBC_SETUP_HELPER_PATH));
}

TEST(SetupHelperCandidates, LooksBesideTheExecutableFirst) {
    const std::vector<std::string> candidates = wbc::SetupHelperCandidates("/opt/app/bin");
    ASSERT_GE(candidates.size(), 2U);
    // Normalised: pkexec would not match /opt/app/bin/../libexec against the
    // polkit action's path.
    EXPECT_EQ(candidates[0], "/opt/app/libexec/wbc/wbc-setup-loopback");
    EXPECT_EQ(candidates[1], WBC_SETUP_HELPER_PATH);
}

TEST(SetupHelperCandidates, BesideAnExecutableInThePrefixIsTheInstalledPath) {
    // What an installed program asks; the two lookups must agree, or the
    // polkit action would not match the path the first one finds.
    const std::string bin =
        std::filesystem::path{WBC_SETUP_HELPER_PATH}.parent_path().parent_path().parent_path() /
        "bin";
    EXPECT_EQ(wbc::SetupHelperCandidates(bin).front(), WBC_SETUP_HELPER_PATH);
}

TEST(SetupHelperCandidates, WithoutADirectoryTheFirstCandidateIsNothing) {
    const std::vector<std::string> candidates = wbc::SetupHelperCandidates({});
    ASSERT_GE(candidates.size(), 2U);
    EXPECT_TRUE(candidates[0].empty());
    EXPECT_TRUE(wbc::FindSetupHelper(candidates[0]).empty());
}

TEST(SetupHelperCandidates, ReachesIntoTheSourceTreeOnlyWhenConfiguredTo) {
    const std::vector<std::string> candidates = wbc::SetupHelperCandidates("/opt/app/bin");
#ifdef WBC_SOURCE_SETUP_HELPER_PATH
    // Last, and being the file in the repository, there to be found.
    ASSERT_EQ(candidates.size(), 3U);
    EXPECT_EQ(candidates.back(), WBC_SOURCE_SETUP_HELPER_PATH);
    EXPECT_FALSE(wbc::FindSetupHelper(WBC_SOURCE_SETUP_HELPER_PATH).empty())
        << "packaging/wbc-setup-loopback lost its executable bit";
#else
    EXPECT_EQ(candidates.size(), 2U) << "a release build reached into the source tree";
#endif
}

TEST(LocateSetupHelper, TakesTheFirstCandidateThatCanBeRun) {
    QTemporaryDir directory;
    const std::string installed = directory.filePath("installed").toStdString();
    const std::string source = directory.filePath("source").toStdString();
    for (const std::string& path : {installed, source}) {
        std::ofstream{path} << "#!/bin/sh\n";
        ASSERT_TRUE(QFile::setPermissions(QString::fromStdString(path),
                                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    EXPECT_EQ(wbc::LocateSetupHelper({installed, source}), installed);

    // The installed one gone, as when running from the IDE: the source tree's.
    ASSERT_TRUE(QFile::remove(QString::fromStdString(installed)));
    EXPECT_EQ(wbc::LocateSetupHelper({installed, source}), source);

    ASSERT_TRUE(QFile::remove(QString::fromStdString(source)));
    EXPECT_TRUE(wbc::LocateSetupHelper({installed, source}).empty());
    EXPECT_TRUE(wbc::LocateSetupHelper({}).empty());
}

TEST(PkexecLoopbackAdmin, RunsTheSourceTreeHelperWhenThatIsTheOneFound) {
    // pkexec shows its generic prompt for this path, as the polkit action
    // names only the installed one; what runs, and what it sets up, is the same.
    const std::string source = "/src/wbc/packaging/wbc-setup-loopback";
    wbc::PkexecLoopbackAdmin admin{NoDevices, [] { return true; }, NeverRuns,
                                  wbc::kVirtualCameraLabel, 10, [source] { return source; },
                                  NotSandboxed};
    EXPECT_EQ(admin.LoadCommand(),
              "pkexec " + source + " \"" + wbc::kVirtualCameraLabel + "\" 10");
    EXPECT_EQ(admin.ManualCommand(),
              "sudo " + source + " \"" + wbc::kVirtualCameraLabel + "\" 10");
}

TEST(RunningInFlatpak, IsTrueWhenTheMarkerFileExists) {
    QTemporaryDir directory;
    const std::string marker = directory.filePath(".flatpak-info").toStdString();
    EXPECT_FALSE(wbc::RunningInFlatpak(marker));
    std::ofstream{marker} << "[Application]\n";
    EXPECT_TRUE(wbc::RunningInFlatpak(marker));
}

TEST(MakeDefaultLoopbackAdmin, IsWiredToThisMachine) {
    // Nothing is loaded or run; this only checks that the factory hands back
    // an admin whose answers agree with the functions it was built from.
    const auto admin = wbc::MakeDefaultLoopbackAdmin();
    EXPECT_EQ(admin->InFlatpak(), wbc::RunningInFlatpak());
    const std::string helper = wbc::LocateSetupHelper();
    if (helper.empty()) {
        EXPECT_NE(admin->LoadCommand().find("modprobe"), std::string::npos);
    } else {
        EXPECT_NE(admin->LoadCommand().find(helper), std::string::npos);
    }
    // Probing only reads /dev and the module tree. Probed first, on a line of
    // its own: the two arguments of a comparison are evaluated in no fixed
    // order, and DevicePath() asked before Probe() is always empty.
    const bool ready = admin->Probe() == wbc::LoopbackState::Ready;
    EXPECT_EQ(ready, !admin->DevicePath().empty());
}
