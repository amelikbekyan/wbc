// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSettings>

#include <memory>

#include "camera/DeviceEnumerator.h"
#include "camera/V4l2CameraSource.h"
#include "comp/BackgroundImage.h"
#include "comp/BackgroundVideo.h"
#include "comp/Compositor.h"
#include "comp/OpenCvVideoDecoder.h"
#include "core/CNumericLocale.h"
#include "core/FileModelLoader.h"
#include "core/HeapTuning.h"
#include "core/ModelPath.h"
#include "core/OpenCvThreads.h"
#include "core/PipelineController.h"
#include "core/SteadyClock.h"
#include "core/Translations.h"
#include "seg/MaskRefiner.h"
#include "seg/SelfieOnnx.h"
#include "sink/PipeWireAnnouncingSink.h"
#include "sink/PkexecLoopbackAdmin.h"
#include "sink/V4l2LoopbackSink.h"
#include "ui/MainWindow.h"
#include "ui/ProgramIcon.h"

int main(int argc, char* argv[]) {
    // Names the window to Wayland compositors, which match it to wbc.desktop
    // for its icon and its place in the dock. Qt also registers the name with
    // the desktop portal, which accepts it only as the first request on the
    // connection; set after the constructor, it comes too late, because the
    // constructor has already read the portal's appearance settings, and the
    // portal answers "Connection already associated with an application ID".
    QApplication::setDesktopFileName("wbc");
    QApplication application(argc, argv);
    QApplication::setOrganizationName("wbc");
    QApplication::setApplicationName("wbc");
    // For the dialogs that can appear before the window exists.
    QApplication::setWindowIcon(wbc::ProgramIcon());
    wbc::UseCNumericLocale();
    wbc::LimitOpenCvThreads();
    wbc::KeepFrameBuffersOnTheHeap();

    // Installed before the model is loaded, so that a failure to load it is
    // reported in the user's language rather than in English.
    QSettings store;
    wbc::LanguageSwitcher languages{application};
    languages.Use(wbc::ResolveLanguage(wbc::LoadSettings(store).language, QLocale::system()));

    wbc::SteadyClock clock;
    wbc::FileModelLoader modelLoader;

    const QString modelPath = wbc::FindModelFile();
    if (modelPath.isEmpty()) {
        QMessageBox::critical(
            nullptr, QObject::tr("Webcam Background Changer"),
            QObject::tr("The segmentation model was not found. It is looked for in:\n%1")
                .arg(wbc::ModelSearchPath(
                         QProcessEnvironment::systemEnvironment().value("WBC_MODEL"),
                         QCoreApplication::applicationDirPath())
                         .join('\n')));
        return 1;
    }

    std::unique_ptr<wbc::SelfieOnnx> segmenter;
    try {
        segmenter = std::make_unique<wbc::SelfieOnnx>(modelLoader, modelPath.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QObject::tr("Webcam Background Changer"),
                              QObject::tr("The segmentation model could not be loaded:\n%1")
                                  .arg(QString::fromUtf8(error.what())));
        return 1;
    }

    wbc::MaskRefiner refiner{clock};
    wbc::Compositor compositor;
    wbc::BackgroundImage background;
    wbc::BackgroundVideo video{clock, [] { return std::make_unique<wbc::OpenCvVideoDecoder>(); }};
    wbc::V4l2CameraSource camera;
    wbc::V4l2LoopbackSink loopback;
    auto sink = wbc::MakeDefaultPipeWireAnnouncingSink(loopback);
    wbc::PipelineController controller{camera,     *segmenter, refiner, compositor,
                                      background, video,      *sink,   clock};

    auto admin = wbc::MakeDefaultLoopbackAdmin();

    wbc::MainWindow window{
        controller,
        *admin,
        store,
        [] { return wbc::EnumerateDevices(); },
        [](QWidget* parent, wbc::MainWindow::FileKind kind) {
            if (kind == wbc::MainWindow::FileKind::Video) {
                return QFileDialog::getOpenFileName(
                    parent, QObject::tr("Choose a background video"), QString{},
                    QObject::tr("Videos (*.mp4 *.mkv *.webm *.mov *.avi)"));
            }
            return QFileDialog::getOpenFileName(parent, QObject::tr("Choose a background picture"),
                                                QString{},
                                                QObject::tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        },
        [](QWidget* parent, const QColor& current) {
            return QColorDialog::getColor(current, parent, QObject::tr("Choose a background colour"));
        },
        [&languages](const QString& language) { return languages.Use(language); }};

    window.show();
    return QApplication::exec();
}
