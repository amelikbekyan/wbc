// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sink/ILoopbackAdmin.h"

namespace wbc {

class FakeLoopbackAdmin final : public ILoopbackAdmin {
public:
    LoopbackState Probe() override {
        ++probeCount_;
        return state_;
    }

    std::string DevicePath() const override {
        return state_ == LoopbackState::Ready ? devicePath_ : std::string{};
    }

    bool LoadModule() override {
        ++loadCount_;
        if (!loadSucceeds_) {
            lastError_ = "FakeLoopbackAdmin was told to fail";
            needsManualSetup_ = failureNeedsManualSetup_;
            return false;
        }
        state_ = LoopbackState::Ready;
        return true;
    }

    std::string LastError() const override { return lastError_; }
    std::string InstallCommand() const override { return "sudo apt install v4l2loopback-dkms"; }
    std::string LoadCommand() const override { return "pkexec modprobe v4l2loopback"; }
    std::string ManualCommand() const override {
        return "sudo /usr/libexec/wbc/wbc-setup-loopback \"Webcam Background Changer\" 10";
    }
    bool InFlatpak() const override { return inFlatpak_; }
    bool NeedsManualSetup() const override { return needsManualSetup_; }

    void SetState(LoopbackState state) { state_ = state; }
    void SetDevicePath(std::string path) { devicePath_ = std::move(path); }
    void SetLoadSucceeds(bool value) { loadSucceeds_ = value; }
    // A failed load then behaves as pkexec unable to get permission does.
    void SetFailureNeedsManualSetup(bool value) { failureNeedsManualSetup_ = value; }
    void SetInFlatpak(bool value) { inFlatpak_ = value; }
    int ProbeCount() const { return probeCount_; }
    int LoadCount() const { return loadCount_; }

private:
    LoopbackState state_ = LoopbackState::Ready;
    std::string devicePath_ = "/dev/video10";
    std::string lastError_;
    bool loadSucceeds_ = true;
    bool failureNeedsManualSetup_ = false;
    bool needsManualSetup_ = false;
    bool inFlatpak_ = false;
    int probeCount_ = 0;
    int loadCount_ = 0;
};

}  // namespace wbc
