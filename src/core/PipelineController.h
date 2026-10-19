// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "camera/ICameraSource.h"
#include "comp/BackgroundImage.h"
#include "comp/BackgroundVideo.h"
#include "comp/Compositor.h"
#include "core/IPipelineController.h"
#include "core/PipelineStats.h"
#include "seg/ISegmenter.h"
#include "seg/MaskRefiner.h"
#include "sink/IFrameSink.h"

namespace wbc {

// Owns the frame loop. Everything it works with is injected, so the whole loop
// runs in tests with no camera, no model and no kernel module.
//
// Prepare and ProcessOneFrame are public on purpose: driving one frame at a
// time makes the loop's behaviour assertable without racing a thread.
class PipelineController final : public IPipelineController {
public:
    PipelineController(ICameraSource& camera,
                       ISegmenter& segmenter,
                       MaskRefiner& refiner,
                       Compositor& compositor,
                       BackgroundImage& background,
                       BackgroundVideo& video,
                       IFrameSink& sink,
                       const IClock& clock,
                       QObject* parent = nullptr);
    ~PipelineController() override;

    bool Start(const PipelineConfig& config) override;
    void Stop() override;
    bool IsRunning() const override;
    void UpdateCompositorSettings(const CompositorSettings& settings) override;
    void UpdateMaskSettings(const MaskSettings& settings) override;
    bool SetBackgroundImage(const QString& path) override;
    bool SetBackgroundVideo(const QString& path) override;
    void PreviewConsumed() override;
    QString LastError() const override;

    bool Prepare(const PipelineConfig& config);
    // Returns false when the pipeline must stop; the reason has been emitted.
    bool ProcessOneFrame();
    // Safe only while the worker is not running: it reads the stats deque the
    // worker mutates, and PipelineStats::Current() evicts through a mutable
    // member, so this is a write as well as a read. The window never calls it
    // -- it takes its numbers from the statsUpdated signal. Tests call it
    // between frames, or after Stop() has joined the worker.
    PipelineStats::Snapshot Stats() const;

private:
    void Run();
    void Fail(const QString& message);
    CompositorSettings CurrentSettings() const;
    MaskSettings CurrentMaskSettings() const;

    ICameraSource& camera_;
    ISegmenter& segmenter_;
    MaskRefiner& refiner_;
    Compositor& compositor_;
    // Not guarded here on purpose: BackgroundImage owns a mutex covering Load,
    // For, IsLoaded, LastError and Clear, because the GUI thread may choose a
    // new picture while this worker is compositing with the old one. If that
    // ever stops being true, this member needs its own lock.
    BackgroundImage& background_;
    // Plays on its own thread only while the pipeline runs in video mode;
    // the controller starts and stops it as the mode and the run change.
    // Its own locking covers the pipeline's thread asking for frames.
    BackgroundVideo& video_;
    IFrameSink& sink_;
    const IClock& clock_;

    PipelineStats stats_;
    mutable std::mutex settingsMutex_;
    CompositorSettings settings_;
    // Read by the worker at the top of every frame and written by the GUI
    // thread, so it is guarded like the compositor settings beside it. The
    // refiner itself is touched only from the worker.
    MaskSettings maskSettings_;
    mutable std::mutex errorMutex_;
    QString lastError_;

    std::thread worker_;
    std::atomic<bool> running_{false};
    // Set when a preview is emitted, cleared when the window has taken it.
    std::atomic<bool> previewInFlight_{false};
};

}  // namespace wbc
