// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sink/PipeWireAnnouncingSink.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtGlobal>

#include <utility>

namespace wbc {

PipeWireAnnouncingSink::PipeWireAnnouncingSink(IFrameSink& inner, DumpFn dump, RestartFn restart)
    : inner_(inner), dump_(std::move(dump)), restart_(std::move(restart)) {}

PipeWireAnnouncingSink::~PipeWireAnnouncingSink() {
    JoinAnnouncement();
}

bool PipeWireAnnouncingSink::Open(const std::string& path, int width, int height, double fps) {
    JoinAnnouncement();
    path_ = path;
    announced_ = false;
    return inner_.Open(path, width, height, fps);
}

bool PipeWireAnnouncingSink::IsOpen() const {
    return inner_.IsOpen();
}

WriteResult PipeWireAnnouncingSink::Write(const cv::Mat& frameBgr) {
    const WriteResult result = inner_.Write(frameBgr);
    // Only a delivered frame turns the node into a capture device; asking
    // PipeWire to look at it before then would find the output again.
    if (result == WriteResult::Ok && !announced_) {
        announced_ = true;
        announcer_ = std::thread(&PipeWireAnnouncingSink::Announce, this, path_);
    }
    return result;
}

void PipeWireAnnouncingSink::Close() {
    JoinAnnouncement();
    inner_.Close();
}

std::string PipeWireAnnouncingSink::LastError() const {
    return inner_.LastError();
}

void PipeWireAnnouncingSink::Announce(const std::string& path) const {
    const std::optional<QByteArray> dump = dump_();
    if (!dump.has_value() || PipeWireHasVideoSource(*dump, path)) {
        return;
    }
    if (!restart_()) {
        qWarning("PipeWire has no source for %s and WirePlumber could not be restarted; "
                 "applications that use PipeWire will not see the virtual camera",
                 path.c_str());
    }
}

void PipeWireAnnouncingSink::JoinAnnouncement() {
    if (announcer_.joinable()) {
        announcer_.join();
    }
}

bool PipeWireHasVideoSource(const QByteArray& pwDump, const std::string& devicePath) {
    const QString wanted = QString::fromStdString(devicePath);
    const QJsonArray objects = QJsonDocument::fromJson(pwDump).array();
    for (const QJsonValue& object : objects) {
        const QJsonObject props = object["info"]["props"].toObject();
        if (props["media.class"].toString() == "Video/Source" &&
            props["api.v4l2.path"].toString() == wanted) {
            return true;
        }
    }
    return false;
}

std::optional<QByteArray> ReadProgramOutput(const QString& program, const QStringList& arguments,
                                            int timeoutMs) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(timeoutMs)) {
        // Also the answer for a program that never started, whose wait
        // returns at once.
        process.kill();
        process.waitForFinished(5000);
        return std::nullopt;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return std::nullopt;
    }
    return process.readAllStandardOutput();
}

std::unique_ptr<PipeWireAnnouncingSink> MakeDefaultPipeWireAnnouncingSink(IFrameSink& inner) {
    return std::make_unique<PipeWireAnnouncingSink>(
        inner, [] { return ReadProgramOutput("pw-dump", {}); },
        [] {
            return ReadProgramOutput("systemctl", {"--user", "restart", "wireplumber"})
                .has_value();
        });
}

}  // namespace wbc
