// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "sink/IFrameSink.h"

namespace wbc {

// Makes the virtual camera visible to PipeWire.
//
// exclusive_caps=1 makes the loopback node advertise Video Output until a
// frame is written into it, and only then Video Capture. WirePlumber probes a
// V4L2 node once, when it appears, and never again: a node created before
// anything streamed into it -- which is every node, because the module is
// loaded before the pipeline starts -- is remembered as an output and gets no
// Video/Source. Firefox, OBS, and every Flatpak and Snap application reach
// cameras only through PipeWire and the camera portal, so to them the virtual
// camera does not exist. Chrome's own V4L2 path is not affected.
//
// So after the first frame of a run is delivered, this asks PipeWire whether
// it has a source for the device, and if it has not, restarts WirePlumber,
// which probes the node again and finds it capturing now. The check runs
// first so that the restart -- which interrupts audio for a moment -- happens
// once per loaded module, not once per run. It runs on its own thread so that
// the frame loop never waits for it, and a machine without PipeWire, where
// pw-dump cannot run, is left alone.
//
// Everything that touches the outside world enters through a callable, so
// every branch can be exercised without PipeWire.
class PipeWireAnnouncingSink final : public IFrameSink {
public:
    // The output of pw-dump, or nothing when it could not be run.
    using DumpFn = std::function<std::optional<QByteArray>()>;
    // Returns true when WirePlumber was restarted.
    using RestartFn = std::function<bool()>;

    PipeWireAnnouncingSink(IFrameSink& inner, DumpFn dump, RestartFn restart);
    ~PipeWireAnnouncingSink() override;

    PipeWireAnnouncingSink(const PipeWireAnnouncingSink&) = delete;
    PipeWireAnnouncingSink& operator=(const PipeWireAnnouncingSink&) = delete;

    bool Open(const std::string& path, int width, int height, double fps) override;
    bool IsOpen() const override;
    WriteResult Write(const cv::Mat& frameBgr) override;
    // Waits for an announcement still in progress before closing the device.
    void Close() override;
    std::string LastError() const override;

private:
    void Announce(const std::string& path) const;
    void JoinAnnouncement();

    IFrameSink& inner_;
    DumpFn dump_;
    RestartFn restart_;
    std::string path_;
    bool announced_ = false;
    std::thread announcer_;
};

// True when the pw-dump output holds a Video/Source node for the V4L2 device
// at `devicePath`. Output that is not a pw-dump array holds no such node.
bool PipeWireHasVideoSource(const QByteArray& pwDump, const std::string& devicePath);

// Runs a program without a shell and returns what it wrote to standard
// output, or nothing when it could not be started, did not finish within
// `timeoutMs`, crashed, or exited with a non-zero status.
std::optional<QByteArray> ReadProgramOutput(const QString& program, const QStringList& arguments,
                                            int timeoutMs = 10000);

// Wired to the real system: pw-dump, and systemctl restarting the user's
// WirePlumber service.
std::unique_ptr<PipeWireAnnouncingSink> MakeDefaultPipeWireAnnouncingSink(IFrameSink& inner);

}  // namespace wbc
