// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QSettings>
#include <QTemporaryDir>

#include "core/Settings.h"

namespace {

QString TempIniPath(QTemporaryDir& directory) {
    return directory.filePath("settings.ini");
}

}  // namespace

TEST(Settings, AnEmptyStoreYieldsTheDocumentedDefaults) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);

    const wbc::AppSettings settings = wbc::LoadSettings(store);
    EXPECT_EQ(settings.mode, wbc::BackgroundMode::Blur);
    EXPECT_EQ(settings.blurRadius, 15);
    EXPECT_TRUE(settings.cameraPath.isEmpty());
    EXPECT_EQ(settings.color, QColor(0, 177, 64));
    EXPECT_TRUE(settings.backgroundImagePath.isEmpty());
}

TEST(Settings, SurvivesARoundTrip) {
    QTemporaryDir directory;
    wbc::AppSettings written;
    written.cameraPath = "/dev/video0";
    written.mode = wbc::BackgroundMode::Image;
    written.blurRadius = 7;
    written.color = QColor(1, 2, 3);
    written.backgroundImagePath = "/tmp/beach.png";

    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        wbc::SaveSettings(store, written);
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    const wbc::AppSettings read = wbc::LoadSettings(store);
    EXPECT_EQ(read.cameraPath, "/dev/video0");
    EXPECT_EQ(read.mode, wbc::BackgroundMode::Image);
    EXPECT_EQ(read.blurRadius, 7);
    EXPECT_EQ(read.color, QColor(1, 2, 3));
    EXPECT_EQ(read.backgroundImagePath, "/tmp/beach.png");
}

TEST(Settings, ColorModeRoundTripsCorrectly) {
    QTemporaryDir directory;
    wbc::AppSettings written;
    written.mode = wbc::BackgroundMode::Color;

    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        wbc::SaveSettings(store, written);
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    const wbc::AppSettings read = wbc::LoadSettings(store);
    EXPECT_EQ(read.mode, wbc::BackgroundMode::Color);
}

TEST(Settings, AnUnknownModeFallsBackToBlurRatherThanUndefinedBehaviour) {
    QTemporaryDir directory;
    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        store.setValue("background/mode", 99);
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    EXPECT_EQ(wbc::LoadSettings(store).mode, wbc::BackgroundMode::Blur);
}

TEST(Settings, ABlurRadiusOutsideTheAllowedRangeIsClamped) {
    QTemporaryDir directory;
    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        store.setValue("background/blurRadius", 9999);
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    EXPECT_EQ(wbc::LoadSettings(store).blurRadius, wbc::kMaxBlurRadius);
}

TEST(Settings, ANegativeBlurRadiusBecomesZero) {
    QTemporaryDir directory;
    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        store.setValue("background/blurRadius", -5);
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    EXPECT_EQ(wbc::LoadSettings(store).blurRadius, 0);
}

TEST(Settings, ConvertsToCompositorSettingsWithTheColourInBlueGreenRedOrder) {
    wbc::AppSettings settings;
    settings.mode = wbc::BackgroundMode::Color;
    settings.blurRadius = 3;
    settings.color = QColor(10, 20, 30);  // red, green, blue

    const wbc::CompositorSettings converted = settings.ToCompositorSettings();
    EXPECT_EQ(converted.mode, wbc::BackgroundMode::Color);
    EXPECT_EQ(converted.blurRadius, 3);
    EXPECT_DOUBLE_EQ(converted.color[0], 30.0);  // blue
    EXPECT_DOUBLE_EQ(converted.color[1], 20.0);  // green
    EXPECT_DOUBLE_EQ(converted.color[2], 10.0);  // red
}

TEST(Settings, AColourStringThatCannotBeParsedFallsBackToTheDefault) {
    QTemporaryDir directory;
    {
        QSettings store(TempIniPath(directory), QSettings::IniFormat);
        // A settings file is editable by hand and outlives upgrades, so a
        // value that is not a colour at all is a case the loader has to
        // survive rather than pass on to QColor's invalid state.
        store.setValue("background/color", "not a colour");
    }

    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    const wbc::AppSettings settings = wbc::LoadSettings(store);
    EXPECT_TRUE(settings.color.isValid());
    EXPECT_EQ(settings.color, QColor(0, 177, 64));
}

TEST(Settings, AnEmptyStoreYieldsTheMaskDefaults) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);

    const wbc::MaskSettings mask = wbc::LoadSettings(store).mask;
    EXPECT_FLOAT_EQ(mask.threshold, 0.0F);
    EXPECT_FLOAT_EQ(mask.smoothing, 0.7F);
    EXPECT_EQ(mask.edgeRadius, 8);
}

TEST(Settings, TheMaskSettingsSurviveASaveAndLoad) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);

    wbc::AppSettings saved;
    saved.mask.threshold = 0.4F;
    saved.mask.smoothing = 0.2F;
    saved.mask.edgeRadius = 12;
    wbc::SaveSettings(store, saved);

    const wbc::MaskSettings loaded = wbc::LoadSettings(store).mask;
    EXPECT_FLOAT_EQ(loaded.threshold, 0.4F);
    EXPECT_FLOAT_EQ(loaded.smoothing, 0.2F);
    EXPECT_EQ(loaded.edgeRadius, 12);
}

TEST(Settings, MaskValuesOutsideTheirRangeAreClampedRatherThanPassedOn) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    store.setValue("mask/threshold", 5.0);
    store.setValue("mask/smoothing", -3.0);
    store.setValue("mask/edgeRadius", 9000);
    store.sync();

    const wbc::MaskSettings mask = wbc::LoadSettings(store).mask;
    EXPECT_FLOAT_EQ(mask.threshold, wbc::kMaxMaskThreshold);
    EXPECT_FLOAT_EQ(mask.smoothing, 0.0F);
    EXPECT_EQ(mask.edgeRadius, wbc::kMaxMaskEdgeRadius);
}

TEST(Settings, TheVideoModeAndItsFileSurviveASaveAndLoad) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);

    wbc::AppSettings saved;
    saved.mode = wbc::BackgroundMode::Video;
    saved.backgroundVideoPath = "/videos/beach.mp4";
    wbc::SaveSettings(store, saved);

    const wbc::AppSettings loaded = wbc::LoadSettings(store);
    EXPECT_EQ(loaded.mode, wbc::BackgroundMode::Video);
    EXPECT_EQ(loaded.backgroundVideoPath, "/videos/beach.mp4");
}

TEST(Settings, AnEmptyStoreHasNoVideo) {
    QTemporaryDir directory;
    QSettings store(TempIniPath(directory), QSettings::IniFormat);
    EXPECT_TRUE(wbc::LoadSettings(store).backgroundVideoPath.isEmpty());
}
