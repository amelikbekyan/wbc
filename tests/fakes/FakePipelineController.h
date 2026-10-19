// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/IPipelineController.h"

namespace wbc {

class FakePipelineController final : public IPipelineController {
public:
    bool Start(const PipelineConfig& config) override {
        lastConfig_ = config;
        ++startCount_;
        if (!startSucceeds_) {
            lastError_ = "FakePipelineController was told to refuse to start";
            return false;
        }
        running_ = true;
        return true;
    }

    void Stop() override {
        ++stopCount_;
        running_ = false;
    }

    bool IsRunning() const override { return running_; }

    void UpdateCompositorSettings(const CompositorSettings& settings) override {
        lastSettings_ = settings;
        ++settingsCount_;
    }

    void UpdateMaskSettings(const MaskSettings& settings) override {
        lastMaskSettings_ = settings;
        ++maskSettingsCount_;
    }

    bool SetBackgroundImage(const QString& path) override {
        lastImagePath_ = path;
        if (!imageSucceeds_) {
            lastError_ = "FakePipelineController was told to reject " + path;
            return false;
        }
        return true;
    }

    bool SetBackgroundVideo(const QString& path) override {
        lastVideoPath_ = path;
        if (!videoSucceeds_) {
            lastError_ = "FakePipelineController was told to reject " + path;
            return false;
        }
        return true;
    }

    void PreviewConsumed() override { ++previewConsumedCount_; }

    QString LastError() const override { return lastError_; }

    // Test seams: let a test drive the signals the window reacts to.
    void EmitPreview(const QImage& image) { emit previewReady(image); }
    void EmitStats(double fps, double latencyMs, qulonglong dropped) {
        emit statsUpdated(fps, latencyMs, dropped);
    }
    void EmitFailure(const QString& message) { emit failed(message); }
    void EmitFormat(int width, int height, double fps) {
        emit formatNegotiated(width, height, fps);
    }

    void SetStartSucceeds(bool value) { startSucceeds_ = value; }
    void SetImageSucceeds(bool value) { imageSucceeds_ = value; }
    void SetVideoSucceeds(bool value) { videoSucceeds_ = value; }
    const QString& LastVideoPath() const { return lastVideoPath_; }
    const PipelineConfig& LastConfig() const { return lastConfig_; }
    const CompositorSettings& LastSettings() const { return lastSettings_; }
    const MaskSettings& LastMaskSettings() const { return lastMaskSettings_; }
    const QString& LastImagePath() const { return lastImagePath_; }
    int StartCount() const { return startCount_; }
    int StopCount() const { return stopCount_; }
    int SettingsCount() const { return settingsCount_; }
    int MaskSettingsCount() const { return maskSettingsCount_; }
    int PreviewConsumedCount() const { return previewConsumedCount_; }

private:
    PipelineConfig lastConfig_;
    CompositorSettings lastSettings_;
    MaskSettings lastMaskSettings_;
    QString lastImagePath_;
    QString lastVideoPath_;
    QString lastError_;
    bool running_ = false;
    bool startSucceeds_ = true;
    bool imageSucceeds_ = true;
    bool videoSucceeds_ = true;
    int startCount_ = 0;
    int stopCount_ = 0;
    int settingsCount_ = 0;
    int maskSettingsCount_ = 0;
    int previewConsumedCount_ = 0;
};

}  // namespace wbc
