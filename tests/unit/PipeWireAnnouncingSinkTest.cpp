// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include "fakes/FakeSink.h"
#include "sink/PipeWireAnnouncingSink.h"

namespace {

// The shape pw-dump prints, cut down to what is looked at: the camera has a
// source, and the loopback is only a device, as WirePlumber leaves it when it
// probed the node before anything streamed into it.
const QByteArray kLoopbackWithoutSource = R"([
  {"id": 53, "type": "PipeWire:Interface:Device",
   "info": {"props": {"api.v4l2.path": "/dev/video10", "device.api": "v4l2"}}},
  {"id": 66, "type": "PipeWire:Interface:Node",
   "info": {"props": {"media.class": "Video/Source", "api.v4l2.path": "/dev/video0"}}},
  {"id": 70, "type": "PipeWire:Interface:Node",
   "info": {"props": {"media.class": "Audio/Source", "api.v4l2.path": "/dev/video10"}}},
  {"id": 71, "type": "PipeWire:Interface:Core", "info": null}
])";

const QByteArray kLoopbackWithSource = R"([
  {"id": 83, "type": "PipeWire:Interface:Node",
   "info": {"props": {"media.class": "Video/Source", "api.v4l2.path": "/dev/video10"}}}
])";

const cv::Mat kFrame(48, 64, CV_8UC3, cv::Scalar{10, 20, 30});

// Counts what the sink asked of PipeWire. The counters are written on the
// announcer's thread and read after Close(), which joins it.
struct PipeWire {
    std::optional<QByteArray> dump = kLoopbackWithoutSource;
    bool restartSucceeds = true;
    std::atomic<int> dumps{0};
    std::atomic<int> restarts{0};

    wbc::PipeWireAnnouncingSink::DumpFn Dump() {
        return [this] {
            ++dumps;
            return dump;
        };
    }

    wbc::PipeWireAnnouncingSink::RestartFn Restart() {
        return [this] {
            ++restarts;
            return restartSucceeds;
        };
    }
};

}  // namespace

TEST(PipeWireHasVideoSource, FindsTheSourceOfTheDevice) {
    EXPECT_TRUE(wbc::PipeWireHasVideoSource(kLoopbackWithSource, "/dev/video10"));
    EXPECT_TRUE(wbc::PipeWireHasVideoSource(kLoopbackWithoutSource, "/dev/video0"));
}

TEST(PipeWireHasVideoSource, ADeviceWithoutASourceNodeHasNone) {
    // The device object and a node of another class name the same path; only
    // a Video/Source node is something an application can open.
    EXPECT_FALSE(wbc::PipeWireHasVideoSource(kLoopbackWithoutSource, "/dev/video10"));
}

TEST(PipeWireHasVideoSource, OutputThatIsNotAnArrayOfObjectsHoldsNothing) {
    EXPECT_FALSE(wbc::PipeWireHasVideoSource("", "/dev/video10"));
    EXPECT_FALSE(wbc::PipeWireHasVideoSource("not json", "/dev/video10"));
    EXPECT_FALSE(wbc::PipeWireHasVideoSource(R"({"media.class": "Video/Source"})", "/dev/video10"));
}

TEST(PipeWireAnnouncingSink, PassesEveryCallThroughToTheDevice) {
    wbc::FakeSink inner;
    PipeWire pipewire;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    EXPECT_FALSE(sink.IsOpen());
    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 25.0));
    EXPECT_TRUE(sink.IsOpen());
    EXPECT_EQ(inner.ConfiguredSize(), cv::Size(64, 48));
    EXPECT_DOUBLE_EQ(inner.ConfiguredFps(), 25.0);
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    EXPECT_EQ(inner.Frames().size(), 1U);
    sink.Close();
    EXPECT_TRUE(inner.WasClosed());
    EXPECT_FALSE(sink.IsOpen());
}

TEST(PipeWireAnnouncingSink, ReportsTheDevicesRefusal) {
    wbc::FakeSink inner;
    inner.SetOpenSucceeds(false);
    PipeWire pipewire;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    EXPECT_FALSE(sink.Open("/dev/video10", 64, 48, 30.0));
    EXPECT_EQ(sink.LastError(), inner.LastError());
    EXPECT_FALSE(sink.LastError().empty());
}

TEST(PipeWireAnnouncingSink, RestartsWirePlumberWhenTheFirstFrameFindsNoSource) {
    wbc::FakeSink inner;
    PipeWire pipewire;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    sink.Close();

    EXPECT_EQ(pipewire.dumps, 1);
    EXPECT_EQ(pipewire.restarts, 1);
}

TEST(PipeWireAnnouncingSink, LeavesWirePlumberAloneWhenTheSourceIsAlreadyThere) {
    // The restart interrupts audio, so a run after the first must not repeat it.
    wbc::FakeSink inner;
    PipeWire pipewire;
    pipewire.dump = kLoopbackWithSource;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    sink.Close();

    EXPECT_EQ(pipewire.dumps, 1);
    EXPECT_EQ(pipewire.restarts, 0);
}

TEST(PipeWireAnnouncingSink, LeavesAMachineWithoutPipeWireAlone) {
    wbc::FakeSink inner;
    PipeWire pipewire;
    pipewire.dump = std::nullopt;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    sink.Close();

    EXPECT_EQ(pipewire.dumps, 1);
    EXPECT_EQ(pipewire.restarts, 0);
}

TEST(PipeWireAnnouncingSink, KeepsDeliveringFramesWhenTheRestartFails) {
    wbc::FakeSink inner;
    PipeWire pipewire;
    pipewire.restartSucceeds = false;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    sink.Close();

    EXPECT_EQ(pipewire.restarts, 1);
    EXPECT_EQ(inner.Frames().size(), 2U);
}

TEST(PipeWireAnnouncingSink, WaitsForADeliveredFrameBeforeAsking) {
    // Until a frame is delivered the node is still an output, and asking
    // PipeWire to look again would find the output again.
    wbc::FakeSink inner;
    PipeWire pipewire;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    inner.SetResult(wbc::WriteResult::Dropped);
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Dropped);
    inner.SetResult(wbc::WriteResult::Fatal);
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Fatal);
    sink.Close();
    EXPECT_EQ(pipewire.dumps, 0);

    // A write on a closed device is refused, and is not a delivered frame.
    EXPECT_EQ(sink.Write(kFrame), wbc::WriteResult::Fatal);
    sink.Close();
    EXPECT_EQ(pipewire.dumps, 0);
}

TEST(PipeWireAnnouncingSink, AsksOncePerRun) {
    wbc::FakeSink inner;
    PipeWire pipewire;
    wbc::PipeWireAnnouncingSink sink{inner, pipewire.Dump(), pipewire.Restart()};

    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    }
    sink.Close();
    EXPECT_EQ(pipewire.dumps, 1);

    // The module may have been reloaded between runs, and a new node needs
    // announcing again. Opening without closing first is the same new run.
    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
    ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    sink.Close();
    EXPECT_EQ(pipewire.dumps, 3);
}

TEST(PipeWireAnnouncingSink, DestructionWaitsForAnAnnouncementInProgress) {
    wbc::FakeSink inner;
    std::atomic<bool> finished{false};
    {
        wbc::PipeWireAnnouncingSink sink{
            inner,
            [] { return std::optional<QByteArray>{kLoopbackWithoutSource}; },
            [&finished] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                finished = true;
                return true;
            }};
        ASSERT_TRUE(sink.Open("/dev/video10", 64, 48, 30.0));
        ASSERT_EQ(sink.Write(kFrame), wbc::WriteResult::Ok);
    }
    EXPECT_TRUE(finished);
}

TEST(ReadProgramOutput, ReturnsWhatTheProgramWrote) {
    EXPECT_EQ(wbc::ReadProgramOutput("/bin/sh", {"-c", "printf hello"}), QByteArray("hello"));
}

TEST(ReadProgramOutput, ReportsAFailingProgramAsNothing) {
    EXPECT_FALSE(wbc::ReadProgramOutput("/bin/sh", {"-c", "echo partial; exit 1"}).has_value());
}

TEST(ReadProgramOutput, ReportsAMissingProgramAsNothing) {
    EXPECT_FALSE(wbc::ReadProgramOutput("/nonexistent/pw-dump", {}).has_value());
}

TEST(ReadProgramOutput, ReportsACrashedProgramAsNothing) {
    EXPECT_FALSE(wbc::ReadProgramOutput("/bin/sh", {"-c", "kill -SEGV $$"}).has_value());
}

TEST(ReadProgramOutput, ReportsAProgramThatOutlivesItsTimeoutAsNothing) {
    EXPECT_FALSE(wbc::ReadProgramOutput("/bin/sh", {"-c", "sleep 30"}, 100).has_value());
}
