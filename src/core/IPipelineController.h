// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <string>

#include "comp/Compositor.h"
#include "core/Types.h"

namespace wbc {

struct PipelineConfig {
    std::string cameraPath;
    std::string sinkPath;
    FormatRequest request;
    CompositorSettings compositor;
    MaskSettings mask;
    QString backgroundImagePath;
    QString backgroundVideoPath;
};

// The window talks to the pipeline only through this, so that the window can
// be tested against a fake that never touches a camera.
class IPipelineController : public QObject {
    Q_OBJECT

public:
    explicit IPipelineController(QObject* parent = nullptr) : QObject(parent) {}
    ~IPipelineController() override = default;

    virtual bool Start(const PipelineConfig& config) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
    virtual void UpdateCompositorSettings(const CompositorSettings& settings) = 0;
    // Retunes the mask chain while the pipeline runs. Like the compositor
    // settings, this is called from the GUI thread and takes effect on the
    // worker's next frame.
    virtual void UpdateMaskSettings(const MaskSettings& settings) = 0;
    virtual bool SetBackgroundImage(const QString& path) = 0;
    // Loads a background video. While the pipeline runs in video mode the new
    // one starts playing at once.
    virtual bool SetBackgroundVideo(const QString& path) = 0;
    // The window calls this once it has finished with a preview image. Until
    // it does, the pipeline drops previews instead of queueing them, so a
    // slow GUI thread sheds load rather than accumulating megabytes of frames.
    virtual void PreviewConsumed() = 0;
    virtual QString LastError() const = 0;

signals:
    void previewReady(const QImage& image);
    // The geometry and rate the camera actually gave, which may be smaller
    // than what was asked for (spec section 8, row 7).
    void formatNegotiated(int width, int height, double fps);
    void statsUpdated(double fps, double latencyMs, qulonglong dropped);
    void failed(const QString& message);
};

}  // namespace wbc
