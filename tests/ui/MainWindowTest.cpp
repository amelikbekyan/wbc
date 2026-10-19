// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <opencv2/videoio.hpp>

#include "fakes/FakeLoopbackAdmin.h"
#include "fakes/FakePipelineController.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/ProgramIcon.h"

namespace {

std::vector<wbc::DeviceInfo> TwoCamerasAndALoopback() {
    return {{"/dev/video0", "USB2.0 HD UVC WebCam", true, false},
            {"/dev/video2", "Second Camera", true, false},
            {"/dev/video10", wbc::kVirtualCameraLabel, false, true}};
}

// Holds the window and everything it was given, so a test can look at both.
struct Fixture {
    QTemporaryDir directory;
    QSettings store{directory.filePath("settings.ini"), QSettings::IniFormat};
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    QString fileToChoose;
    wbc::MainWindow::FileKind askedFor = wbc::MainWindow::FileKind::Picture;
    QColor colorToChoose{QColor(10, 20, 30)};
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [this](QWidget*, wbc::MainWindow::FileKind kind) {
                              askedFor = kind;
                              return fileToChoose;
                          },
                          [this](QWidget*, const QColor&) { return colorToChoose; }};

    template <typename T>
    T* Find(const char* name) {
        T* widget = window.findChild<T*>(name);
        EXPECT_NE(widget, nullptr) << "no widget named " << name;
        return widget;
    }
};

}  // namespace

TEST(MainWindow, ListsTheCamerasAndLeavesOutOurOwnVirtualDevice) {
    Fixture fixture;
    auto* combo = fixture.Find<QComboBox>("cameraCombo");
    ASSERT_EQ(combo->count(), 2);
    EXPECT_EQ(combo->itemText(0), "USB2.0 HD UVC WebCam");
    EXPECT_EQ(combo->itemData(0).toString(), "/dev/video0");
}

TEST(MainWindow, ShowsTheVirtualCameraWhenItIsReadyAndOffersNoSetup) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::Ready);
    fixture.window.Refresh();

    EXPECT_NE(fixture.Find<QLabel>("virtualCameraLabel")->text().indexOf("/dev/video10"), -1);
    // Corrected from EXPECT_FALSE(isVisible()): a never-shown window makes
    // isVisible() false for every child regardless of setVisible(), so that
    // assertion cannot fail. isHidden() reflects the explicit setVisible(false).
    EXPECT_TRUE(fixture.Find<QPushButton>("setupButton")->isHidden());
}

TEST(MainWindow, OffersToLoadTheModuleWhenItIsInstalledButNotLoaded) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.window.Refresh();

    auto* button = fixture.Find<QPushButton>("setupButton");
    EXPECT_TRUE(button->isEnabled());
    EXPECT_NE(fixture.Find<QLabel>("setupHint")->text().indexOf("modprobe"), -1);

    button->click();
    EXPECT_EQ(fixture.admin.LoadCount(), 1);
}

TEST(MainWindow, ShowsTheInstallCommandAndDoesNotOfferToLoadWhenTheModuleIsMissing) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotInstalled);
    fixture.window.Refresh();

    EXPECT_FALSE(fixture.Find<QPushButton>("setupButton")->isEnabled());
    EXPECT_NE(fixture.Find<QLabel>("setupHint")->text().indexOf("v4l2loopback-dkms"), -1);
}

TEST(MainWindow, ReportsAFailedModuleLoadWithoutPretendingItWorked) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.admin.SetLoadSucceeds(false);
    fixture.window.Refresh();

    fixture.Find<QPushButton>("setupButton")->click();
    EXPECT_FALSE(fixture.Find<QLabel>("messageLabel")->text().isEmpty());
}

TEST(MainWindow, OffersNoCommandToCopyWhileTheButtonCanAskForThePassword) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.window.Refresh();

    EXPECT_TRUE(fixture.Find<QLabel>("manualCommand")->isHidden());
    EXPECT_TRUE(fixture.Find<QPushButton>("copyCommandButton")->isHidden());
}

TEST(MainWindow, WhenPkexecCannotAskTheCommandReplacesTheButton) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.admin.SetLoadSucceeds(false);
    fixture.admin.SetFailureNeedsManualSetup(true);
    fixture.window.Refresh();

    fixture.Find<QPushButton>("setupButton")->click();

    auto* command = fixture.Find<QLabel>("manualCommand");
    EXPECT_FALSE(command->isHidden());
    EXPECT_EQ(command->text(), QString::fromStdString(fixture.admin.ManualCommand()));
    EXPECT_TRUE(command->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
    EXPECT_TRUE(command->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard));
    EXPECT_FALSE(fixture.Find<QPushButton>("copyCommandButton")->isHidden());
    EXPECT_TRUE(fixture.Find<QPushButton>("setupButton")->isHidden())
        << "a button that would fail the same way again";
    EXPECT_NE(fixture.Find<QLabel>("setupHint")->text().indexOf("terminal"), -1);
    // The rebuilt panel must not have wiped the reason it was rebuilt.
    EXPECT_EQ(fixture.Find<QLabel>("messageLabel")->text(),
              QString::fromStdString(fixture.admin.LastError()));
}

TEST(MainWindow, AnOrdinaryFailureKeepsTheButtonForAnotherTry) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.admin.SetLoadSucceeds(false);
    fixture.window.Refresh();

    fixture.Find<QPushButton>("setupButton")->click();
    EXPECT_FALSE(fixture.Find<QPushButton>("setupButton")->isHidden());
    EXPECT_TRUE(fixture.Find<QLabel>("manualCommand")->isHidden());
}

TEST(MainWindow, TheCopyButtonPutsTheCommandOnTheClipboard) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.admin.SetInFlatpak(true);
    fixture.window.Refresh();

    QGuiApplication::clipboard()->clear();
    fixture.Find<QPushButton>("copyCommandButton")->click();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), fixture.Find<QLabel>("manualCommand")->text());
}

TEST(MainWindow, InFlatpakShowsTheHostCommandsAndNoButton) {
    for (const auto state : {wbc::LoopbackState::ModuleNotLoaded,
                             wbc::LoopbackState::ModuleNotInstalled}) {
        Fixture fixture;
        fixture.admin.SetState(state);
        fixture.admin.SetInFlatpak(true);
        fixture.window.Refresh();

        EXPECT_TRUE(fixture.Find<QPushButton>("setupButton")->isHidden());
        const QString command = fixture.Find<QLabel>("manualCommand")->text();
        EXPECT_NE(command.indexOf("v4l2loopback-dkms"), -1);
        EXPECT_NE(command.indexOf(QString::fromStdString(fixture.admin.ManualCommand())), -1);
        EXPECT_NE(fixture.Find<QLabel>("setupHint")->text().indexOf("Flatpak"), -1);
    }
}

TEST(MainWindow, OnceTheDeviceExistsTheCommandIsTakenAway) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.admin.SetInFlatpak(true);
    fixture.window.Refresh();
    ASSERT_FALSE(fixture.Find<QLabel>("manualCommand")->isHidden());

    // The user ran it on the host.
    fixture.admin.SetState(wbc::LoopbackState::Ready);
    fixture.window.Refresh();
    EXPECT_TRUE(fixture.Find<QLabel>("manualCommand")->isHidden());
    EXPECT_TRUE(fixture.Find<QPushButton>("copyCommandButton")->isHidden());
    EXPECT_TRUE(fixture.Find<QLabel>("setupHint")->isHidden());
}

TEST(MainWindow, StartingPassesTheSelectedCameraAndTheVirtualDeviceToThePipeline) {
    Fixture fixture;
    fixture.Find<QComboBox>("cameraCombo")->setCurrentIndex(1);
    fixture.Find<QPushButton>("startButton")->click();

    ASSERT_EQ(fixture.controller.StartCount(), 1);
    EXPECT_EQ(fixture.controller.LastConfig().cameraPath, "/dev/video2");
    EXPECT_EQ(fixture.controller.LastConfig().sinkPath, "/dev/video10");
}

TEST(MainWindow, TheStartButtonBecomesAStopButtonOnceRunning) {
    Fixture fixture;
    auto* button = fixture.Find<QPushButton>("startButton");
    button->click();
    EXPECT_NE(button->text().indexOf("Stop"), -1);

    button->click();
    EXPECT_EQ(fixture.controller.StopCount(), 1);
    EXPECT_NE(button->text().indexOf("Start"), -1);
}

TEST(MainWindow, AFailureToStartIsShownAndLeavesTheButtonOfferingToStart) {
    Fixture fixture;
    fixture.controller.SetStartSucceeds(false);
    auto* button = fixture.Find<QPushButton>("startButton");
    button->click();

    EXPECT_FALSE(fixture.Find<QLabel>("messageLabel")->text().isEmpty());
    EXPECT_NE(button->text().indexOf("Start"), -1);
}

TEST(MainWindow, RefusesToStartWithoutTheVirtualCamera) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.window.Refresh();

    fixture.Find<QPushButton>("startButton")->click();
    EXPECT_EQ(fixture.controller.StartCount(), 0);
    EXPECT_FALSE(fixture.Find<QLabel>("messageLabel")->text().isEmpty());
}

TEST(MainWindow, RefusesToStartWithNoCameraAvailable) {
    QTemporaryDir directory;
    QSettings store(directory.filePath("settings.ini"), QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{
        controller, admin, store, [] { return std::vector<wbc::DeviceInfo>{}; },
        [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; }, [](QWidget*, const QColor&) { return QColor{}; }};

    window.findChild<QPushButton*>("startButton")->click();
    EXPECT_EQ(controller.StartCount(), 0);
    EXPECT_FALSE(window.findChild<QLabel*>("messageLabel")->text().isEmpty());
}

TEST(MainWindow, ChoosingAModeTellsThePipelineImmediately) {
    Fixture fixture;
    fixture.Find<QRadioButton>("colorRadio")->click();
    EXPECT_EQ(fixture.controller.LastSettings().mode, wbc::BackgroundMode::Color);

    fixture.Find<QRadioButton>("blurRadio")->click();
    EXPECT_EQ(fixture.controller.LastSettings().mode, wbc::BackgroundMode::Blur);
}

TEST(MainWindow, MovingTheBlurSliderChangesTheRadius) {
    Fixture fixture;
    fixture.Find<QSlider>("blurSlider")->setValue(33);
    EXPECT_EQ(fixture.controller.LastSettings().blurRadius, 33);
}

TEST(MainWindow, ChoosingAPictureHandsThePathToThePipelineAndSelectsImageMode) {
    Fixture fixture;
    fixture.fileToChoose = "/tmp/beach.png";
    fixture.Find<QPushButton>("chooseImageButton")->click();

    EXPECT_EQ(fixture.controller.LastImagePath(), "/tmp/beach.png");
    EXPECT_TRUE(fixture.Find<QRadioButton>("imageRadio")->isChecked());
}

TEST(MainWindow, APictureThatCannotBeReadIsReportedAndTheModeIsNotChanged) {
    Fixture fixture;
    fixture.fileToChoose = "/tmp/broken.png";
    fixture.controller.SetImageSucceeds(false);
    fixture.Find<QPushButton>("chooseImageButton")->click();

    EXPECT_FALSE(fixture.Find<QLabel>("messageLabel")->text().isEmpty());
    EXPECT_FALSE(fixture.Find<QRadioButton>("imageRadio")->isChecked());
}

TEST(MainWindow, CancellingThePicturePickerChangesNothing) {
    Fixture fixture;
    fixture.fileToChoose = QString{};
    fixture.Find<QPushButton>("chooseImageButton")->click();
    EXPECT_TRUE(fixture.controller.LastImagePath().isEmpty());
}

TEST(MainWindow, ChoosingAColourPassesItInBlueGreenRedOrder) {
    Fixture fixture;
    fixture.colorToChoose = QColor(10, 20, 30);  // red, green, blue
    fixture.Find<QPushButton>("chooseColorButton")->click();

    EXPECT_EQ(fixture.controller.LastSettings().mode, wbc::BackgroundMode::Color);
    EXPECT_DOUBLE_EQ(fixture.controller.LastSettings().color[0], 30.0);
    EXPECT_DOUBLE_EQ(fixture.controller.LastSettings().color[2], 10.0);
}

TEST(MainWindow, ShowsThePreviewItIsGiven) {
    Fixture fixture;
    QImage image(64, 48, QImage::Format_RGB888);
    image.fill(Qt::red);
    fixture.controller.EmitPreview(image);

    auto* preview = fixture.Find<QLabel>("previewLabel");
    ASSERT_FALSE(preview->pixmap().isNull());
    EXPECT_GT(preview->pixmap().width(), 0);
}

TEST(MainWindow, TellsTheUserWhatFormatTheCameraActuallyGave) {
    Fixture fixture;
    fixture.controller.EmitFormat(1280, 720, 30.0);

    // Spec section 8, row 7: a camera that quietly fell back to 640x480 when
    // 720p was asked for is only visible if the negotiated format is shown.
    const QString text = fixture.Find<QLabel>("virtualCameraLabel")->text();
    EXPECT_NE(text.indexOf("1280x720"), -1) << text.toStdString();
    EXPECT_NE(text.indexOf("30"), -1) << text.toStdString();
    EXPECT_NE(text.indexOf("/dev/video10"), -1) << text.toStdString();
}

TEST(MainWindow, TellsThePipelineWhenItHasTakenAPreview) {
    Fixture fixture;
    QImage image(64, 48, QImage::Format_RGB888);
    image.fill(Qt::green);
    fixture.controller.EmitPreview(image);

    // Until the window says so the pipeline drops previews instead of
    // queueing them, so failing to say so would freeze the preview forever.
    EXPECT_EQ(fixture.controller.PreviewConsumedCount(), 1);
}

TEST(MainWindow, ShowsAThumbnailOfTheChosenPicture) {
    Fixture fixture;
    const QString path = fixture.directory.filePath("beach.png");
    QImage picture(120, 80, QImage::Format_RGB888);
    picture.fill(Qt::blue);
    ASSERT_TRUE(picture.save(path)) << "could not write " << path.toStdString();

    fixture.fileToChoose = path;
    fixture.Find<QPushButton>("chooseImageButton")->click();

    auto* thumbnail = fixture.Find<QLabel>("imagePreviewLabel");
    ASSERT_FALSE(thumbnail->pixmap().isNull()) << "spec section 7 asks for a thumbnail";
    EXPECT_LE(thumbnail->pixmap().width(), 96);
    EXPECT_GT(thumbnail->pixmap().height(), 0);
}

TEST(MainWindow, ShowsTheThumbnailOfTheRestoredPictureOnStartup) {
    QTemporaryDir directory;
    const QString picturePath = directory.filePath("restored.png");
    QImage picture(120, 80, QImage::Format_RGB888);
    picture.fill(Qt::magenta);
    ASSERT_TRUE(picture.save(picturePath)) << "could not write " << picturePath.toStdString();

    const QString storePath = directory.filePath("settings.ini");
    {
        // Populated through SaveSettings, so the test restores by the same
        // route the application writes by.
        QSettings store(storePath, QSettings::IniFormat);
        wbc::AppSettings settings;
        settings.mode = wbc::BackgroundMode::Image;
        settings.backgroundImagePath = picturePath;
        wbc::SaveSettings(store, settings);
    }

    QSettings store(storePath, QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; }};

    // The half of the thumbnail a user actually notices: after reopening the
    // application it is the only thing that says which picture is in use.
    auto* thumbnail = window.findChild<QLabel*>("imagePreviewLabel");
    ASSERT_NE(thumbnail, nullptr);
    ASSERT_FALSE(thumbnail->pixmap().isNull()) << "no thumbnail for the restored picture";
    EXPECT_LE(thumbnail->pixmap().width(), 96);
}

TEST(MainWindow, ShowsNoThumbnailForAPictureThatIsNoLongerThere) {
    Fixture fixture;
    fixture.fileToChoose = "/nonexistent/gone.png";
    fixture.Find<QPushButton>("chooseImageButton")->click();

    // The message area explains; the thumbnail must not show a stale or
    // broken picture.
    EXPECT_TRUE(fixture.Find<QLabel>("imagePreviewLabel")->pixmap().isNull());
}

TEST(MainWindow, ShowsTheFrameRateAndDroppedFrames) {
    Fixture fixture;
    fixture.controller.EmitStats(29.7, 12.5, 4);

    const QString text = fixture.Find<QLabel>("statsLabel")->text();
    EXPECT_NE(text.indexOf("29.7"), -1);
    EXPECT_NE(text.indexOf("4"), -1);
}

TEST(MainWindow, APipelineFailureIsShownAndTheWindowReturnsToTheStoppedState) {
    Fixture fixture;
    fixture.Find<QPushButton>("startButton")->click();
    fixture.controller.EmitFailure("The camera was disconnected");

    EXPECT_NE(fixture.Find<QLabel>("messageLabel")->text().indexOf("disconnected"), -1);
    EXPECT_NE(fixture.Find<QPushButton>("startButton")->text().indexOf("Start"), -1);
}

TEST(MainWindow, ReleasesTheDevicesWhenThePipelineFails) {
    Fixture fixture;
    fixture.Find<QPushButton>("startButton")->click();
    ASSERT_EQ(fixture.controller.StartCount(), 1);

    fixture.controller.EmitFailure("The camera was disconnected");

    // Without this the camera stays open and streaming while the button
    // says Start — the application would be holding a device it claims to
    // have released.
    EXPECT_EQ(fixture.controller.StopCount(), 1);
    EXPECT_NE(fixture.Find<QPushButton>("startButton")->text().indexOf("Start"), -1);
}

TEST(MainWindow, RemembersTheChosenSettingsForNextTime) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");

    {
        QSettings store(path, QSettings::IniFormat);
        wbc::FakePipelineController controller;
        wbc::FakeLoopbackAdmin admin;
        wbc::MainWindow window{controller,
                              admin,
                              store,
                              TwoCamerasAndALoopback,
                              [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                              [](QWidget*, const QColor&) { return QColor{}; }};
        window.findChild<QSlider*>("blurSlider")->setValue(21);
        window.findChild<QRadioButton*>("colorRadio")->click();
        window.close();
    }

    QSettings store(path, QSettings::IniFormat);
    const wbc::AppSettings restored = wbc::LoadSettings(store);
    EXPECT_EQ(restored.blurRadius, 21);
    EXPECT_EQ(restored.mode, wbc::BackgroundMode::Color);
}

TEST(MainWindow, SaysSoWhenTheSelectedCameraDisappears) {
    QTemporaryDir directory;
    QSettings store(directory.filePath("settings.ini"), QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;

    bool bothPresent = true;
    auto devices = [&bothPresent] {
        std::vector<wbc::DeviceInfo> all{{"/dev/video0", "USB2.0 HD UVC WebCam", true, false},
                                        {"/dev/video10", wbc::kVirtualCameraLabel, false, true}};
        if (bothPresent) {
            all.insert(all.begin() + 1, {"/dev/video2", "Second Camera", true, false});
        }
        return all;
    };

    wbc::MainWindow window{controller, admin, store, devices,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; }};

    auto* combo = window.findChild<QComboBox*>("cameraCombo");
    ASSERT_NE(combo, nullptr);
    ASSERT_EQ(combo->count(), 2);
    combo->setCurrentIndex(1);
    ASSERT_EQ(combo->currentData().toString(), "/dev/video2");

    // The chosen camera is unplugged and the list is rebuilt.
    bothPresent = false;
    window.Refresh();

    auto* message = window.findChild<QLabel*>("messageLabel");
    ASSERT_NE(message, nullptr);
    EXPECT_FALSE(message->text().isEmpty()) << "the user was not told the camera changed";
    EXPECT_EQ(combo->currentData().toString(), "/dev/video0");
}

TEST(MainWindow, TellsTheUserToPressStartBeforeLookingForTheCameraElsewhere) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::Ready);
    fixture.window.Refresh();

    auto* hint = fixture.Find<QLabel>("useHint");
    EXPECT_FALSE(hint->isHidden());
    EXPECT_NE(hint->text().indexOf("Start"), -1);
}

TEST(MainWindow, ChangesTheHintOnceStreamingHasBegun) {
    Fixture fixture;
    auto* hint = fixture.Find<QLabel>("useHint");
    const QString beforeStart = hint->text();

    fixture.Find<QPushButton>("startButton")->click();
    EXPECT_NE(hint->text(), beforeStart);
    EXPECT_NE(hint->text().indexOf("restart"), -1);
}

TEST(MainWindow, SaysNothingAboutChoosingTheCameraWhileThereIsNoneToChoose) {
    Fixture fixture;
    fixture.admin.SetState(wbc::LoopbackState::ModuleNotLoaded);
    fixture.window.Refresh();

    auto* hint = fixture.Find<QLabel>("useHint");
    EXPECT_TRUE(hint->isHidden());
    EXPECT_TRUE(hint->text().isEmpty());
}

TEST(MainWindow, MovingAMaskSliderRetunesTheRunningPipeline) {
    Fixture fixture;
    auto* threshold = fixture.Find<QSlider>("maskThresholdSlider");
    // Building the window already pushed the stored values once per slider
    // that moved off zero, so this counts the change rather than the total.
    const int before = fixture.controller.MaskSettingsCount();
    threshold->setValue(40);

    EXPECT_EQ(fixture.controller.MaskSettingsCount(), before + 1);
    EXPECT_FLOAT_EQ(fixture.controller.LastMaskSettings().threshold, 0.4F);

    fixture.Find<QSlider>("maskSmoothingSlider")->setValue(20);
    EXPECT_FLOAT_EQ(fixture.controller.LastMaskSettings().smoothing, 0.2F);

    fixture.Find<QSlider>("maskEdgeSlider")->setValue(3);
    EXPECT_EQ(fixture.controller.LastMaskSettings().edgeRadius, 3);
}

TEST(MainWindow, EachMaskSliderSaysWhatItIsAndWhereItStands) {
    Fixture fixture;
    // Written once at construction from the stored settings, before any
    // slider has moved.
    EXPECT_NE(fixture.Find<QLabel>("maskThresholdLabel")->text().indexOf("0%"), -1);
    EXPECT_NE(fixture.Find<QLabel>("maskSmoothingLabel")->text().indexOf("70%"), -1);
    EXPECT_NE(fixture.Find<QLabel>("maskEdgeLabel")->text().indexOf("8 px"), -1);

    fixture.Find<QSlider>("maskThresholdSlider")->setValue(35);
    EXPECT_NE(fixture.Find<QLabel>("maskThresholdLabel")->text().indexOf("35%"), -1);
}

TEST(MainWindow, StartsThePipelineWithTheMaskSettingsOnScreen) {
    Fixture fixture;
    fixture.Find<QSlider>("maskThresholdSlider")->setValue(25);
    fixture.Find<QSlider>("maskEdgeSlider")->setValue(4);
    fixture.Find<QPushButton>("startButton")->click();

    EXPECT_FLOAT_EQ(fixture.controller.LastConfig().mask.threshold, 0.25F);
    EXPECT_EQ(fixture.controller.LastConfig().mask.edgeRadius, 4);
}

TEST(MainWindow, RemembersTheMaskSettingsForTheNextSession) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::FakePipelineController controller;
        wbc::FakeLoopbackAdmin admin;
        wbc::MainWindow window{controller,
                              admin,
                              store,
                              TwoCamerasAndALoopback,
                              [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                              [](QWidget*, const QColor&) { return QColor{}; }};
        window.findChild<QSlider*>("maskThresholdSlider")->setValue(45);
        window.findChild<QSlider*>("maskSmoothingSlider")->setValue(15);
        window.findChild<QSlider*>("maskEdgeSlider")->setValue(11);
        window.close();
    }

    QSettings reopened(path, QSettings::IniFormat);
    const wbc::MaskSettings mask = wbc::LoadSettings(reopened).mask;
    EXPECT_FLOAT_EQ(mask.threshold, 0.45F);
    EXPECT_FLOAT_EQ(mask.smoothing, 0.15F);
    EXPECT_EQ(mask.edgeRadius, 11);
}

// A window built with a language recorder, so a test can watch it ask for a
// language without an application to install one.
namespace {

struct LanguageFixture {
    QTemporaryDir directory;
    QSettings store{directory.filePath("settings.ini"), QSettings::IniFormat};
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    QStringList asked;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [this](const QString& code) {
                              asked.append(code);
                              return true;
                          }};
};

}  // namespace

TEST(MainWindow, ListsEveryShippedLanguageUnderItsOwnName) {
    LanguageFixture fixture;
    auto* combo = fixture.window.findChild<QComboBox*>("languageCombo");
    ASSERT_NE(combo, nullptr);
    ASSERT_EQ(combo->count(), wbc::AvailableLanguages().size());

    for (int i = 0; i < combo->count(); ++i) {
        const QString code = combo->itemData(i).toString();
        EXPECT_TRUE(wbc::AvailableLanguages().contains(code));
        EXPECT_EQ(combo->itemText(i), wbc::LanguageDisplayName(code));
    }
}

TEST(MainWindow, AsksForTheLanguageThatWasChosen) {
    LanguageFixture fixture;
    auto* combo = fixture.window.findChild<QComboBox*>("languageCombo");
    const int german = combo->findData("de");
    ASSERT_NE(german, -1);

    combo->setCurrentIndex(german);

    ASSERT_EQ(fixture.asked.size(), 1);
    EXPECT_EQ(fixture.asked.first(), "de");
}

TEST(MainWindow, RewritesItsTextWhenTheLanguageChanges) {
    LanguageFixture fixture;
    auto* blur = fixture.window.findChild<QRadioButton*>("blurRadio");
    ASSERT_NE(blur, nullptr);
    const QString before = blur->text();
    blur->setText("scribbled over");

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&fixture.window, &languageChange);

    EXPECT_EQ(blur->text(), before) << "the window did not rewrite itself";
}

TEST(MainWindow, RemembersTheChosenLanguageForTheNextSession) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::FakePipelineController controller;
        wbc::FakeLoopbackAdmin admin;
        wbc::MainWindow window{controller,
                              admin,
                              store,
                              TwoCamerasAndALoopback,
                              [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                              [](QWidget*, const QColor&) { return QColor{}; },
                              [](const QString&) { return true; }};
        auto* combo = window.findChild<QComboBox*>("languageCombo");
        combo->setCurrentIndex(combo->findData("es"));
        window.close();
    }

    QSettings reopened(path, QSettings::IniFormat);
    EXPECT_EQ(wbc::LoadSettings(reopened).language, "es");
}

TEST(MainWindow, OpensOnTheRememberedLanguage) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::AppSettings saved;
        saved.language = "fr";
        wbc::SaveSettings(store, saved);
    }

    QSettings store(path, QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [](const QString&) { return true; }};

    auto* combo = window.findChild<QComboBox*>("languageCombo");
    EXPECT_EQ(combo->currentData().toString(), "fr");
}

// The video background, from the window's side.
namespace {

// A real two-frame Motion JPEG file, which OpenCV writes without FFmpeg, for
// the thumbnail to be made from.
QString WriteClip(QTemporaryDir& directory) {
    const QString path = directory.filePath("clip.avi");
    cv::VideoWriter writer(path.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 10.0,
                           {32, 24});
    for (int i = 0; i < 2; ++i) {
        writer.write(cv::Mat(24, 32, CV_8UC3, cv::Scalar{0, 0, 255}));
    }
    return path;
}

}  // namespace

TEST(MainWindow, AsksForAVideoWhenAVideoIsWanted) {
    Fixture fixture;
    QTemporaryDir directory;
    fixture.fileToChoose = WriteClip(directory);

    fixture.Find<QPushButton>("chooseVideoButton")->click();

    EXPECT_EQ(fixture.askedFor, wbc::MainWindow::FileKind::Video);
    EXPECT_EQ(fixture.controller.LastVideoPath(), fixture.fileToChoose);
    EXPECT_TRUE(fixture.Find<QRadioButton>("videoRadio")->isChecked());
    EXPECT_EQ(fixture.controller.LastSettings().mode, wbc::BackgroundMode::Video);
    EXPECT_FALSE(fixture.Find<QLabel>("videoPreviewLabel")->pixmap().isNull())
        << "no thumbnail of the chosen video";
}

TEST(MainWindow, AsksForAPictureWhenAPictureIsWanted) {
    Fixture fixture;
    fixture.Find<QPushButton>("chooseImageButton")->click();
    EXPECT_EQ(fixture.askedFor, wbc::MainWindow::FileKind::Picture);
}

TEST(MainWindow, ShowsWhyAVideoWasRefusedAndLeavesTheModeAlone) {
    Fixture fixture;
    fixture.controller.SetVideoSucceeds(false);
    fixture.fileToChoose = "/videos/broken.mp4";

    fixture.Find<QPushButton>("chooseVideoButton")->click();

    EXPECT_FALSE(fixture.Find<QLabel>("messageLabel")->text().isEmpty());
    EXPECT_FALSE(fixture.Find<QRadioButton>("videoRadio")->isChecked());
}

TEST(MainWindow, DoesNothingWhenTheVideoChooserIsCancelled) {
    Fixture fixture;
    fixture.fileToChoose.clear();

    fixture.Find<QPushButton>("chooseVideoButton")->click();

    EXPECT_TRUE(fixture.controller.LastVideoPath().isEmpty());
    EXPECT_FALSE(fixture.Find<QRadioButton>("videoRadio")->isChecked());
}

TEST(MainWindow, SelectingVideoModeTellsThePipeline) {
    Fixture fixture;
    fixture.Find<QRadioButton>("videoRadio")->setChecked(true);
    EXPECT_EQ(fixture.controller.LastSettings().mode, wbc::BackgroundMode::Video);
}

TEST(MainWindow, ReopensInVideoModeWithTheVideoItHad) {
    QTemporaryDir directory;
    const QString clip = WriteClip(directory);
    const QString path = directory.filePath("settings.ini");
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::AppSettings saved;
        saved.mode = wbc::BackgroundMode::Video;
        saved.backgroundVideoPath = clip;
        wbc::SaveSettings(store, saved);
    }

    QSettings store(path, QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [](const QString&) { return true; }};

    EXPECT_TRUE(window.findChild<QRadioButton*>("videoRadio")->isChecked());
    EXPECT_FALSE(window.findChild<QLabel*>("videoPreviewLabel")->pixmap().isNull());

    window.findChild<QPushButton*>("startButton")->click();
    EXPECT_EQ(controller.LastConfig().backgroundVideoPath, clip);
}

TEST(MainWindow, ShowsNoThumbnailForAVideoThatHasGone) {
    QTemporaryDir directory;
    const QString path = directory.filePath("settings.ini");
    {
        QSettings store(path, QSettings::IniFormat);
        wbc::AppSettings saved;
        saved.backgroundVideoPath = directory.filePath("deleted.mp4");
        wbc::SaveSettings(store, saved);
    }

    QSettings store(path, QSettings::IniFormat);
    wbc::FakePipelineController controller;
    wbc::FakeLoopbackAdmin admin;
    wbc::MainWindow window{controller,
                          admin,
                          store,
                          TwoCamerasAndALoopback,
                          [](QWidget*, wbc::MainWindow::FileKind) { return QString{}; },
                          [](QWidget*, const QColor&) { return QColor{}; },
                          [](const QString&) { return true; }};

    EXPECT_TRUE(window.findChild<QLabel*>("videoPreviewLabel")->pixmap().isNull());
}

TEST(MainWindow, TheBlurSliderSaysWhatItIsAndWhereItStands) {
    Fixture fixture;
    auto* label = fixture.Find<QLabel>("blurLabel");
    // Written at construction from the stored radius, before any movement.
    EXPECT_NE(label->text().indexOf("15 px"), -1) << qPrintable(label->text());

    fixture.Find<QSlider>("blurSlider")->setValue(32);
    EXPECT_NE(label->text().indexOf("32 px"), -1) << qPrintable(label->text());
}

TEST(MainWindow, TheBlurLabelIsRewrittenWhenTheLanguageChanges) {
    Fixture fixture;
    auto* label = fixture.Find<QLabel>("blurLabel");
    const QString before = label->text();
    label->setText("scribbled over");

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&fixture.window, &languageChange);

    EXPECT_EQ(label->text(), before);
}

// The Background tab. Each setting sits under its own mode button, and the
// picture, the colour and the video each have a preview of one size, 16:9
// like the camera's frame, whatever the shape of the file: previews that
// kept a file's own proportions made a landscape picture and a portrait
// video different widths, and the buttons beside them different widths too.
namespace {

// A picture of the given shape, as a file the window can load.
QString WritePicture(QTemporaryDir& directory, const char* name, int width, int height) {
    const QString path = directory.filePath(name);
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(QColor(200, 60, 20));
    image.save(path);
    return path;
}

// A portrait video: taller than it is wide, as a phone records one.
QString WritePortraitClip(QTemporaryDir& directory) {
    const QString path = directory.filePath("portrait.avi");
    cv::VideoWriter writer(path.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 10.0,
                           {24, 64});
    for (int i = 0; i < 2; ++i) {
        writer.write(cv::Mat(64, 24, CV_8UC3, cv::Scalar{255, 0, 0}));
    }
    return path;
}

QColor CentreOf(const QLabel* label) {
    const QImage image = label->pixmap().toImage();
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

}  // namespace

TEST(MainWindow, ShowsTheBackgroundColourThatIsChosen) {
    Fixture fixture;
    auto* swatch = fixture.Find<QLabel>("colorSwatch");
    // The stored colour, before anything is chosen.
    EXPECT_EQ(CentreOf(swatch), QColor(0, 177, 64));

    fixture.colorToChoose = QColor(10, 20, 30);
    fixture.Find<QPushButton>("chooseColorButton")->click();
    EXPECT_EQ(CentreOf(swatch), QColor(10, 20, 30));
}

TEST(MainWindow, EveryBackgroundPreviewIsOneSizeWhateverTheShapeOfTheFile) {
    Fixture fixture;
    QTemporaryDir directory;
    fixture.fileToChoose = WritePicture(directory, "wide.png", 400, 100);
    fixture.Find<QPushButton>("chooseImageButton")->click();
    fixture.fileToChoose = WritePortraitClip(directory);
    fixture.Find<QPushButton>("chooseVideoButton")->click();

    const auto* picture = fixture.Find<QLabel>("imagePreviewLabel");
    const auto* video = fixture.Find<QLabel>("videoPreviewLabel");
    const auto* colour = fixture.Find<QLabel>("colorSwatch");

    EXPECT_EQ(picture->size(), colour->size());
    EXPECT_EQ(video->size(), colour->size());
    // Filled, not letterboxed: cropped from the centre to 16:9, as the
    // background itself will be.
    EXPECT_EQ(picture->pixmap().size(), QSize(96, 54));
    EXPECT_EQ(video->pixmap().size(), QSize(96, 54));
    EXPECT_EQ(colour->pixmap().size(), QSize(96, 54));
}

TEST(MainWindow, TheChooseButtonsLineUp) {
    Fixture fixture;
    QTemporaryDir directory;
    fixture.fileToChoose = WritePicture(directory, "tall.png", 50, 300);
    fixture.Find<QPushButton>("chooseImageButton")->click();
    fixture.window.show();
    QApplication::processEvents();

    const auto* picture = fixture.Find<QPushButton>("chooseImageButton");
    const auto* colour = fixture.Find<QPushButton>("chooseColorButton");
    const auto* video = fixture.Find<QPushButton>("chooseVideoButton");
    EXPECT_EQ(picture->width(), colour->width());
    EXPECT_EQ(video->width(), colour->width());
    EXPECT_EQ(picture->x(), colour->x());
    EXPECT_EQ(video->x(), colour->x());
}

TEST(MainWindow, EachBackgroundSettingSitsUnderItsOwnModeButton) {
    Fixture fixture;
    fixture.window.show();
    QApplication::processEvents();

    // In the order the tab reads, top to bottom: each mode, then what
    // belongs to it, before the next mode begins.
    const char* order[] = {"blurRadio",  "blurSlider",       "imageRadio",
                           "chooseImageButton", "colorRadio", "chooseColorButton",
                           "videoRadio", "chooseVideoButton"};
    int previous = -1;
    for (const char* name : order) {
        const auto* widget = fixture.Find<QWidget>(name);
        const int y = widget->mapTo(&fixture.window, QPoint(0, 0)).y();
        EXPECT_GT(y, previous) << name << " is out of place";
        previous = y;
    }
}

// A preview is part of its mode: clicking the picture, the colour or the
// video selects that mode, as clicking the mode's own button would.
TEST(MainWindow, ClickingAPreviewSelectsItsMode) {
    const struct {
        const char* preview;
        const char* radio;
        wbc::BackgroundMode mode;
    } cases[] = {{"imagePreviewLabel", "imageRadio", wbc::BackgroundMode::Image},
                 {"colorSwatch", "colorRadio", wbc::BackgroundMode::Color},
                 {"videoPreviewLabel", "videoRadio", wbc::BackgroundMode::Video}};

    for (const auto& c : cases) {
        Fixture fixture;
        fixture.Find<QRadioButton>("blurRadio")->setChecked(true);

        QTest::mouseClick(fixture.Find<QLabel>(c.preview), Qt::LeftButton);

        EXPECT_TRUE(fixture.Find<QRadioButton>(c.radio)->isChecked()) << c.preview;
        EXPECT_EQ(fixture.controller.LastSettings().mode, c.mode) << c.preview;
    }
}

TEST(MainWindow, APreviewLooksClickable) {
    Fixture fixture;
    for (const char* preview : {"imagePreviewLabel", "colorSwatch", "videoPreviewLabel"}) {
        EXPECT_EQ(fixture.Find<QLabel>(preview)->cursor().shape(), Qt::PointingHandCursor)
            << preview;
    }
}

TEST(MainWindow, OnlyTheLeftButtonSelectsFromAPreview) {
    Fixture fixture;
    fixture.Find<QRadioButton>("blurRadio")->setChecked(true);

    QTest::mouseClick(fixture.Find<QLabel>("colorSwatch"), Qt::RightButton);

    EXPECT_TRUE(fixture.Find<QRadioButton>("blurRadio")->isChecked());
}

// The window carries the program's icon itself, so that it shows in the task
// bar however the program was started -- not only once cmake --install has
// put a desktop entry where the desktop looks for one.
TEST(MainWindow, CarriesTheProgramsIconAtEverySizeADesktopAsksFor) {
    Fixture fixture;
    const QIcon icon = fixture.window.windowIcon();
    ASSERT_FALSE(icon.isNull());
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        // Drawn at that size, not scaled down from a larger one: at 16 pixels
        // a scaled icon is a blur.
        EXPECT_TRUE(icon.availableSizes().contains(QSize(size, size))) << size;
    }
}

TEST(ProgramIcon, IsTheSameIconTheWindowCarries) {
    // main.cpp gives it to the application as well, for the dialogs shown
    // before the window exists.
    Fixture fixture;
    EXPECT_EQ(wbc::ProgramIcon().availableSizes(), fixture.window.windowIcon().availableSizes());
}
