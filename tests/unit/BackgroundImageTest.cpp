// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "comp/BackgroundImage.h"

namespace {

// Writes a real file, because the loader's job is precisely to deal with the
// filesystem; a mocked reader would test nothing that can actually break.
std::string WriteTempImage(const std::string& name, int width, int height, cv::Scalar colour) {
    const auto path = std::filesystem::temp_directory_path() / name;
    cv::imwrite(path.string(), cv::Mat(height, width, CV_8UC3, colour));
    return path.string();
}

}  // namespace

TEST(CoverCrop, FillsTheTargetExactly) {
    const cv::Mat source(100, 200, CV_8UC3, cv::Scalar{10, 20, 30});
    const cv::Mat out = wbc::CoverCrop(source, {640, 480});
    EXPECT_EQ(out.cols, 640);
    EXPECT_EQ(out.rows, 480);
    EXPECT_EQ(out.type(), CV_8UC3);
}

TEST(CoverCrop, AWiderSourceLosesItsSidesRatherThanBeingSquashed) {
    // Left half blue, right half red. Cropping a 2:1 source to a square must
    // keep the centre, so both halves stay visible and neither is stretched.
    cv::Mat source(100, 200, CV_8UC3, cv::Scalar{255, 0, 0});
    source.colRange(100, 200).setTo(cv::Scalar{0, 0, 255});

    const cv::Mat out = wbc::CoverCrop(source, {100, 100});
    EXPECT_EQ(out.cols, 100);
    EXPECT_EQ(out.rows, 100);
    EXPECT_EQ(out.at<cv::Vec3b>(50, 10)[0], 255);
    EXPECT_EQ(out.at<cv::Vec3b>(50, 90)[2], 255);
}

TEST(CoverCrop, ATallerSourceLosesItsTopAndBottom) {
    cv::Mat source(200, 100, CV_8UC3, cv::Scalar{255, 0, 0});
    source.rowRange(100, 200).setTo(cv::Scalar{0, 0, 255});

    const cv::Mat out = wbc::CoverCrop(source, {100, 100});
    EXPECT_EQ(out.cols, 100);
    EXPECT_EQ(out.rows, 100);
    EXPECT_EQ(out.at<cv::Vec3b>(10, 50)[0], 255);
    EXPECT_EQ(out.at<cv::Vec3b>(90, 50)[2], 255);
}

TEST(CoverCrop, AMatchingAspectRatioIsAPlainResize) {
    const cv::Mat source(360, 640, CV_8UC3, cv::Scalar{7, 8, 9});
    const cv::Mat out = wbc::CoverCrop(source, {1280, 720});
    EXPECT_EQ(out.size(), cv::Size(1280, 720));
    EXPECT_EQ(out.at<cv::Vec3b>(360, 640), cv::Vec3b(7, 8, 9));
}

TEST(CoverCrop, RejectsEmptySourceAndDegenerateTarget) {
    EXPECT_THROW(wbc::CoverCrop(cv::Mat{}, {10, 10}), std::invalid_argument);
    const cv::Mat source(10, 10, CV_8UC3, cv::Scalar{0, 0, 0});
    EXPECT_THROW(wbc::CoverCrop(source, {0, 10}), std::invalid_argument);
    EXPECT_THROW(wbc::CoverCrop(source, {10, -1}), std::invalid_argument);
}

TEST(BackgroundImage, StartsEmpty) {
    const wbc::BackgroundImage image;
    EXPECT_FALSE(image.IsLoaded());
    EXPECT_TRUE(image.For({640, 480}).empty());
}

TEST(BackgroundImage, LoadsARealFileAndServesItAtTheRequestedSize) {
    const auto path = WriteTempImage("wbc_background_ok.png", 200, 100, {1, 2, 3});
    wbc::BackgroundImage image;
    ASSERT_TRUE(image.Load(path)) << image.LastError();
    EXPECT_TRUE(image.IsLoaded());
    EXPECT_EQ(image.For({640, 480}).size(), cv::Size(640, 480));
}

TEST(BackgroundImage, ReportsAMissingFileWithoutThrowingAndStaysUnloaded) {
    wbc::BackgroundImage image;
    EXPECT_FALSE(image.Load("/nonexistent/path/to/background.png"));
    EXPECT_FALSE(image.IsLoaded());
    EXPECT_FALSE(image.LastError().empty());
}

TEST(BackgroundImage, ReportsAFileThatIsNotAnImage) {
    const auto path = (std::filesystem::temp_directory_path() / "wbc_background_broken.png").string();
    std::filesystem::remove(path);
    { std::ofstream(path) << "this is not a png"; }

    wbc::BackgroundImage image;
    EXPECT_FALSE(image.Load(path));
    EXPECT_FALSE(image.LastError().empty());
}

TEST(BackgroundImage, AFailedLoadKeepsThePreviousImageUsable) {
    const auto good = WriteTempImage("wbc_background_keep.png", 200, 100, {4, 5, 6});
    wbc::BackgroundImage image;
    ASSERT_TRUE(image.Load(good));
    EXPECT_FALSE(image.Load("/nonexistent/background.png"));
    EXPECT_TRUE(image.IsLoaded());
    EXPECT_EQ(image.For({320, 240}).size(), cv::Size(320, 240));
}

TEST(BackgroundImage, RepeatedRequestsAtTheSameSizeReuseTheCachedResult) {
    const auto path = WriteTempImage("wbc_background_cache.png", 200, 100, {9, 9, 9});
    wbc::BackgroundImage image;
    ASSERT_TRUE(image.Load(path));
    const cv::Mat first = image.For({640, 480});
    const cv::Mat second = image.For({640, 480});
    // A recomputed crop would own a different buffer; the same pointer is
    // the only evidence that the cache was hit.
    EXPECT_EQ(first.data, second.data);
}

TEST(BackgroundImage, ClearForgetsTheImage) {
    const auto path = WriteTempImage("wbc_background_clear.png", 200, 100, {1, 1, 1});
    wbc::BackgroundImage image;
    ASSERT_TRUE(image.Load(path));
    image.Clear();
    EXPECT_FALSE(image.IsLoaded());
    EXPECT_TRUE(image.For({640, 480}).empty());
}

TEST(BackgroundImage, LoadingANewPictureOfTheSameSizeDoesNotServeTheOldOne) {
    const auto first = WriteTempImage("wbc_background_first.png", 200, 100, {1, 1, 1});
    const auto second = WriteTempImage("wbc_background_second.png", 200, 100, {250, 250, 250});

    wbc::BackgroundImage image;
    ASSERT_TRUE(image.Load(first));
    ASSERT_EQ(image.For({640, 480}).at<cv::Vec3b>(240, 320), cv::Vec3b(1, 1, 1));

    ASSERT_TRUE(image.Load(second));
    EXPECT_EQ(image.For({640, 480}).at<cv::Vec3b>(240, 320), cv::Vec3b(250, 250, 250))
        << "the cache was not invalidated by Load";
}

TEST(CoverCrop, AnExtremeAspectRatioDoesNotAllocateAGiantIntermediate) {
    // Scaling before cropping would need roughly 24 GB here.
    const cv::Mat panorama(5000, 1, CV_8UC3, cv::Scalar{5, 10, 15});
    const cv::Mat out = wbc::CoverCrop(panorama, {1280, 720});
    EXPECT_EQ(out.size(), cv::Size(1280, 720));
    EXPECT_EQ(out.at<cv::Vec3b>(360, 640), cv::Vec3b(5, 10, 15));
}
