// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <linux/videodev2.h>

#include <cstring>
#include <filesystem>
#include <fstream>

#include "camera/DeviceEnumerator.h"

namespace {

v4l2_capability MakeCapability(const char* card, std::uint32_t caps, bool useDeviceCaps) {
    v4l2_capability capability{};
    std::strncpy(reinterpret_cast<char*>(capability.card), card, sizeof(capability.card) - 1);
    if (useDeviceCaps) {
        capability.capabilities = V4L2_CAP_DEVICE_CAPS | caps;
        capability.device_caps = caps;
    } else {
        capability.capabilities = caps;
    }
    return capability;
}

}  // namespace

TEST(DescribeDevice, ReadsTheCardNameAndThePath) {
    const auto capability = MakeCapability("USB2.0 HD UVC WebCam", V4L2_CAP_VIDEO_CAPTURE, false);
    const wbc::DeviceInfo info = wbc::DescribeDevice(capability, "/dev/video0");
    EXPECT_EQ(info.path, "/dev/video0");
    EXPECT_EQ(info.name, "USB2.0 HD UVC WebCam");
    EXPECT_TRUE(info.canCapture);
    EXPECT_FALSE(info.canOutput);
}

TEST(DescribeDevice, PrefersDeviceCapsOverTheDriverWideCapabilities) {
    // A driver may advertise both capture and output across all its nodes
    // while this particular node only outputs. Reading the wrong field is how
    // a loopback ends up offered as a camera.
    v4l2_capability capability = MakeCapability("Webcam Background Changer", V4L2_CAP_VIDEO_OUTPUT, true);
    capability.capabilities |= V4L2_CAP_VIDEO_CAPTURE;

    const wbc::DeviceInfo info = wbc::DescribeDevice(capability, "/dev/video10");
    EXPECT_TRUE(info.canOutput);
    EXPECT_FALSE(info.canCapture);
}

TEST(DescribeDevice, FallsBackToTheDriverCapabilitiesWhenDeviceCapsAreAbsent) {
    const auto capability = MakeCapability("Old Driver", V4L2_CAP_VIDEO_CAPTURE, false);
    EXPECT_TRUE(wbc::DescribeDevice(capability, "/dev/video3").canCapture);
}

TEST(DescribeDevice, ReadsACardNameThatFillsTheFieldWithoutATerminator) {
    v4l2_capability capability{};
    // Every one of the 32 bytes occupied, no NUL. The V4L2 header promises
    // no terminator, so a std::string built from the bare pointer would run
    // off the end of the struct.
    std::memset(capability.card, 'A', sizeof(capability.card));
    capability.capabilities = V4L2_CAP_VIDEO_CAPTURE;

    const wbc::DeviceInfo info = wbc::DescribeDevice(capability, "/dev/video0");
    EXPECT_EQ(info.name.size(), sizeof(capability.card));
    EXPECT_EQ(info.name, std::string(sizeof(capability.card), 'A'));
}

TEST(SelectCaptureDevices, KeepsOnlyCaptureNodes) {
    const std::vector<wbc::DeviceInfo> all{
        {"/dev/video0", "Webcam", true, false},
        {"/dev/video1", "Webcam metadata", false, false},
        {"/dev/video10", "Webcam Background Changer", false, true},
    };
    const auto capture = wbc::SelectCaptureDevices(all, "Webcam Background Changer");
    ASSERT_EQ(capture.size(), 1u);
    EXPECT_EQ(capture.front().path, "/dev/video0");
}

TEST(SelectCaptureDevices, ExcludesOurOwnLoopbackEvenWhenItAdvertisesCapture) {
    // With exclusive_caps the loopback presents a capture node once something
    // is writing to it. Offering the user their own output as an input would
    // create a feedback loop.
    const std::vector<wbc::DeviceInfo> all{
        {"/dev/video0", "Webcam", true, false},
        {"/dev/video10", "Webcam Background Changer", true, false},
    };
    const auto capture = wbc::SelectCaptureDevices(all, "Webcam Background Changer");
    ASSERT_EQ(capture.size(), 1u);
    EXPECT_EQ(capture.front().name, "Webcam");
}

TEST(SelectCaptureDevices, ReturnsNothingWhenThereAreNoDevices) {
    EXPECT_TRUE(wbc::SelectCaptureDevices({}, "Webcam Background Changer").empty());
}

TEST(SelectLoopback, FindsTheOutputNodeCarryingOurLabel) {
    const std::vector<wbc::DeviceInfo> all{
        {"/dev/video0", "Webcam", true, false},
        {"/dev/video10", "Webcam Background Changer", false, true},
    };
    const auto loopback = wbc::SelectLoopback(all, "Webcam Background Changer");
    ASSERT_TRUE(loopback.has_value());
    EXPECT_EQ(loopback->path, "/dev/video10");
}

TEST(SelectLoopback, IgnoresALabelMatchThatCannotOutput) {
    const std::vector<wbc::DeviceInfo> all{{"/dev/video10", "Webcam Background Changer", true, false}};
    EXPECT_FALSE(wbc::SelectLoopback(all, "Webcam Background Changer").has_value());
}

TEST(SelectLoopback, ReturnsNothingWhenTheLabelIsAbsent) {
    const std::vector<wbc::DeviceInfo> all{{"/dev/video10", "OBS Virtual Camera", false, true}};
    EXPECT_FALSE(wbc::SelectLoopback(all, "Webcam Background Changer").has_value());
}

TEST(ListVideoDevicePaths, ReturnsVideoNodesInNumericOrderAndIgnoresOtherEntries) {
    const auto directory = std::filesystem::temp_directory_path() / "wbc_devices";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    for (const char* name : {"video10", "video2", "video0", "radio0", "notes.txt"}) {
        std::ofstream(directory / name) << "";
    }

    const auto paths = wbc::ListVideoDevicePaths(directory.string());
    ASSERT_EQ(paths.size(), 3u);
    EXPECT_EQ(paths[0], (directory / "video0").string());
    EXPECT_EQ(paths[1], (directory / "video2").string());
    EXPECT_EQ(paths[2], (directory / "video10").string());
}

TEST(ListVideoDevicePaths, ReturnsNothingForAMissingDirectory) {
    EXPECT_TRUE(wbc::ListVideoDevicePaths("/nonexistent/dev").empty());
}

TEST(ListVideoDevicePaths, SkipsVideoNodesWithOversizedNumbers) {
    const auto directory = std::filesystem::temp_directory_path() / "wbc_devices_overflow";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    // A name that is all digits but overflows int. It should be skipped without throwing.
    std::ofstream(directory / "video99999999999999") << "";
    std::ofstream(directory / "video0") << "";

    const auto paths = wbc::ListVideoDevicePaths(directory.string());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], (directory / "video0").string());
}
