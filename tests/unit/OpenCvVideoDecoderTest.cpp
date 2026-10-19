// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <opencv2/videoio.hpp>

#include <string>

#include "comp/OpenCvVideoDecoder.h"

namespace {

// A real file, written with Motion JPEG so that it needs nothing beyond what
// OpenCV itself can encode. Frame i is a flat grey of brightness 60 * (i + 1),
// far enough apart that JPEG's rounding cannot confuse them.
std::string WriteClip(QTemporaryDir& directory, int frames, double fps) {
    const std::string path = directory.filePath("clip.avi").toStdString();
    cv::VideoWriter writer(path, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, {32, 24});
    EXPECT_TRUE(writer.isOpened()) << "this OpenCV cannot write Motion JPEG";
    for (int i = 0; i < frames; ++i) {
        writer.write(cv::Mat(24, 32, CV_8UC3, cv::Scalar::all(60 * (i + 1))));
    }
    return path;
}

int FrameNumber(const cv::Mat& frame) {
    return (frame.at<cv::Vec3b>(12, 16)[0] + 30) / 60 - 1;
}

}  // namespace

TEST(OpenCvVideoDecoder, ReadsARealFileFrameByFrame) {
    QTemporaryDir directory;
    const std::string path = WriteClip(directory, 3, 10.0);

    wbc::OpenCvVideoDecoder decoder;
    ASSERT_TRUE(decoder.Open(path)) << decoder.LastError();
    EXPECT_NEAR(decoder.Fps(), 10.0, 0.01);

    cv::Mat frame;
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(decoder.Read(frame));
        EXPECT_EQ(frame.size(), cv::Size(32, 24));
        EXPECT_EQ(FrameNumber(frame), i);
    }
    EXPECT_FALSE(decoder.Read(frame)) << "read past the end";
    EXPECT_NE(decoder.LastError().find(path), std::string::npos);
}

TEST(OpenCvVideoDecoder, RewindsToTheFirstFrame) {
    QTemporaryDir directory;
    const std::string path = WriteClip(directory, 2, 10.0);

    wbc::OpenCvVideoDecoder decoder;
    ASSERT_TRUE(decoder.Open(path));
    cv::Mat frame;
    ASSERT_TRUE(decoder.Read(frame));
    ASSERT_TRUE(decoder.Read(frame));

    ASSERT_TRUE(decoder.Rewind());
    ASSERT_TRUE(decoder.Read(frame));
    EXPECT_EQ(FrameNumber(frame), 0);
}

TEST(OpenCvVideoDecoder, RewindsByReopeningWhenSeekingIsNotPossible) {
    QTemporaryDir directory;
    const std::string path = WriteClip(directory, 2, 10.0);

    wbc::OpenCvVideoDecoder decoder;
    ASSERT_TRUE(decoder.Open(path));
    // A closed capture cannot seek, which stands in for a container that
    // does not support it: the rewind has to fall back to opening again.
    decoder.Close();
    ASSERT_TRUE(decoder.Rewind());

    cv::Mat frame;
    ASSERT_TRUE(decoder.Read(frame));
    EXPECT_EQ(FrameNumber(frame), 0);
}

TEST(OpenCvVideoDecoder, NamesAFileItCouldNotOpen) {
    wbc::OpenCvVideoDecoder decoder;
    EXPECT_FALSE(decoder.Open("/nonexistent/clip.mp4"));
    EXPECT_NE(decoder.LastError().find("/nonexistent/clip.mp4"), std::string::npos);
}

TEST(FirstFrameOf, ReturnsTheOpeningFrame) {
    QTemporaryDir directory;
    const cv::Mat frame = wbc::FirstFrameOf(WriteClip(directory, 3, 10.0));
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(FrameNumber(frame), 0);
}

TEST(FirstFrameOf, ReturnsNothingForAFileThatIsNotAVideo) {
    EXPECT_TRUE(wbc::FirstFrameOf("/nonexistent/clip.mp4").empty());
}
