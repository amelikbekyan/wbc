// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>

#include "core/Translations.h"
#include "fakes/FakeLoopbackAdmin.h"
#include "fakes/FakePipelineController.h"
#include "ui/MainWindow.h"

// The window has to stay usable on screens smaller than Full HD. What is left
// of a screen once the desktop's panel and the window's title bar have taken
// theirs is estimated at 90 pixels of height, which is on the generous side
// for GNOME and KDE at 100% scaling.
namespace {

constexpr int kDesktopChrome = 90;

struct Screen {
    const char* name;
    int width;
    int height;
    QSize Usable() const { return {width, height - kDesktopChrome}; }
};

// The laptop screen most sold below Full HD, a common monitor, and a netbook.
constexpr Screen kScreens[] = {{"1366x768", 1366, 768}, {"1280x720", 1280, 720},
                               {"1024x600", 1024, 600}};

struct Window {
    QTemporaryDir directory;
    QSettings store{directory.filePath("settings.ini"), QSettings::IniFormat};
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          [] {
                              return std::vector<wbc::DeviceInfo>{
                                  {"/dev/video0", "USB2.0 HD UVC WebCam", true, false}};
                          },
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [](const QString&) { return true; }};

    Window() {
        admin.SetState(wbc::LoopbackState::Ready);
        window.Refresh();
    }

    // Shown offscreen at `size`, with the layout settled, as a user on that
    // screen would see it. A window whose layout will not shrink that far
    // stays larger than asked -- and then everything fits inside a window
    // that no longer fits the screen -- so the size it actually took is
    // checked before anything is concluded from it.
    void ShowAt(QSize size) {
        window.resize(size);
        window.show();
        QApplication::processEvents();
        EXPECT_LE(window.width(), size.width()) << "the window refused to be that narrow";
        EXPECT_LE(window.height(), size.height()) << "the window refused to be that short";
    }

    // Where `widget` sits, in the window's own coordinates.
    QRect PlaceOf(const QWidget* widget) const {
        return {widget->mapTo(&window, QPoint(0, 0)), widget->size()};
    }
};

// Whether `widget` sits inside a scroll area, and so can be brought into view
// even when it is not in view now.
bool CanBeScrolledTo(const QWidget* widget) {
    for (const QWidget* parent = widget->parentWidget(); parent != nullptr;
         parent = parent->parentWidget()) {
        if (qobject_cast<const QScrollArea*>(parent) != nullptr) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(ScreenFit, TheWindowCanBeMadeSmallEnoughForEachTargetScreen) {
    Window fixture;
    const QSize minimum = fixture.window.minimumSizeHint();
    for (const Screen& screen : kScreens) {
        EXPECT_LE(minimum.width(), screen.Usable().width()) << screen.name;
        EXPECT_LE(minimum.height(), screen.Usable().height()) << screen.name;
    }
}

TEST(ScreenFit, WhatStartsAndStopsTheCameraIsAlwaysInView) {
    for (const Screen& screen : kScreens) {
        Window fixture;
        fixture.ShowAt(screen.Usable());
        const QRect inside = fixture.window.rect();

        // Nothing a user needs in order to use the program at all may depend
        // on scrolling: choosing the camera, starting and stopping, and being
        // told what went wrong.
        for (const char* name : {"cameraCombo", "startButton", "statsLabel", "messageLabel",
                                 "languageCombo", "previewLabel"}) {
            const auto* widget = fixture.window.findChild<QWidget*>(name);
            ASSERT_NE(widget, nullptr) << name;
            EXPECT_TRUE(widget->isVisible()) << name << " on " << screen.name;
            EXPECT_TRUE(inside.contains(fixture.PlaceOf(widget)))
                << name << " is cut off on " << screen.name;
            EXPECT_FALSE(CanBeScrolledTo(widget))
                << name << " should not need scrolling on " << screen.name;
        }
    }
}

TEST(ScreenFit, EveryOtherControlIsInViewOrCanBeScrolledTo) {
    for (const Screen& screen : kScreens) {
        Window fixture;
        fixture.ShowAt(screen.Usable());
        const QRect inside = fixture.window.rect();

        for (const char* name :
             {"blurRadio", "imageRadio", "colorRadio", "videoRadio", "blurSlider",
              "blurLabel", "chooseImageButton", "chooseVideoButton", "chooseColorButton",
              "maskThresholdSlider",
              "maskSmoothingSlider", "maskEdgeSlider"}) {
            const auto* widget = fixture.window.findChild<QWidget*>(name);
            ASSERT_NE(widget, nullptr) << name;
            const bool inView = widget->isVisible() && inside.contains(fixture.PlaceOf(widget));
            EXPECT_TRUE(inView || CanBeScrolledTo(widget))
                << name << " cannot be reached on " << screen.name;
        }
    }
}

TEST(ScreenFit, OpensNoLargerThanTheScreenItIsOn) {
    Window fixture;
    const QRect available = QApplication::primaryScreen()->availableGeometry();
    EXPECT_LE(fixture.window.width(), available.width());
    EXPECT_LE(fixture.window.height(), available.height());
    EXPECT_GE(fixture.window.width(), fixture.window.minimumSizeHint().width());
    EXPECT_GE(fixture.window.height(), fixture.window.minimumSizeHint().height());
}

TEST(ScreenFit, OpensWhereItWasLeft) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");
    QSize left;
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::FakePipelineController controller;
        wbc::FakeLoopbackAdmin admin;
        wbc::MainWindow window{controller,
                              admin,
                              store,
                              [] { return std::vector<wbc::DeviceInfo>{}; },
                              [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                              [](QWidget*, const QColor&) { return QColor{}; },
                              [](const QString&) { return true; }};
        window.show();
        window.resize(window.minimumSizeHint() + QSize(40, 30));
        QApplication::processEvents();
        left = window.size();
        window.close();
    }

    QSettings store(path, QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          [] { return std::vector<wbc::DeviceInfo>{}; },
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [](const QString&) { return true; }};
    EXPECT_EQ(window.size(), left);
}

// Scrolling rescues height, not width: the panel has a fixed width and its
// tabs scroll only vertically, so a control wider than the panel is simply
// cut off. The longest translations and the longest states -- the virtual
// camera not yet set up, which shows a command, or in Flatpak the commands
// to copy -- are what have to fit.
TEST(ScreenFit, NothingIsCutOffAtTheSidesInAnyLanguage) {
    // The offscreen platform draws with a 9 point font, smaller than any
    // desktop's: at that size a Russian label fitted in a test that passed
    // while the same label was cut off on a real screen at
    // Noto Sans 10. The check is made a quarter larger than the platform's
    // default, which covers that desktop and leaves room for a user who has
    // turned the text size up.
    const QFont platformFont = QApplication::font();
    QFont larger = platformFont;
    larger.setPointSizeF(platformFont.pointSizeF() * 1.25);
    QApplication::setFont(larger);

    wbc::LanguageSwitcher languages{*QCoreApplication::instance()};
    for (const QString& language : wbc::AvailableLanguages()) {
        ASSERT_TRUE(languages.Use(language)) << qPrintable(language);

        QTemporaryDir directory;
        QSettings store(directory.filePath("settings.ini"), QSettings::IniFormat);
        wbc::FakePipelineController controller;
        wbc::FakeLoopbackAdmin admin;
        admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
        wbc::MainWindow window{controller,
                              admin,
                              store,
                              [] {
                                  return std::vector<wbc::DeviceInfo>{
                                      {"/dev/video0", "USB2.0 HD UVC WebCam", true, false}};
                              },
                              [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                              [](QWidget*, const QColor&) { return QColor{}; },
                              [](const QString&) { return true; }};
        window.Refresh();
        window.resize(kScreens[2].Usable());
        window.show();
        QApplication::processEvents();

        // Each tab is measured while it is the one shown: a hidden page keeps
        // whatever geometry it last had, which for a page never shown is
        // wider than the panel itself, and a check against that passes
        // whatever the page contains.
        auto* tabs = window.findChild<QTabWidget*>("settingsTabs");
        ASSERT_NE(tabs, nullptr);
        for (int index = 0; index < tabs->count(); ++index) {
            tabs->setCurrentIndex(index);
            QApplication::processEvents();
            auto* area = qobject_cast<QScrollArea*>(tabs->currentWidget());
            ASSERT_NE(area, nullptr) << "tab " << index << " does not scroll";
            EXPECT_LE(area->widget()->minimumSizeHint().width(), area->viewport()->width())
                << "tab " << index << " is wider than the panel in " << qPrintable(language);
        }
        const auto* panel = window.findChild<QWidget*>("controlPanel");
        ASSERT_NE(panel, nullptr);
        EXPECT_LE(panel->minimumSizeHint().width(), panel->width())
            << "the panel's own controls are wider than it in " << qPrintable(language);

        // In Flatpak the panel holds the host commands and a button beside
        // them instead, which is the widest the top of the panel gets.
        admin.SetInFlatpak(true);
        window.Refresh();
        QApplication::processEvents();
        EXPECT_LE(panel->minimumSizeHint().width(), panel->width())
            << "the host commands are wider than the panel in " << qPrintable(language);
    }
    languages.Use(wbc::kSourceLanguage);
    QApplication::setFont(platformFont);
}
