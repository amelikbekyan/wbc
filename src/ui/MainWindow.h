// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QMainWindow>

#include <functional>
#include <vector>

#include "camera/DeviceEnumerator.h"
#include "core/Translations.h"
#include "core/IPipelineController.h"
#include "core/Settings.h"
#include "sink/ILoopbackAdmin.h"

class QTabWidget;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSettings;
class QSlider;

namespace wbc {

// The single application window.
//
// Errors appear in an inline message area rather than in a modal dialog: a
// modal dialog stops the event loop, which would put every error path beyond
// the reach of a test, and a message shown in place is easier to act on than
// one that has to be dismissed first. The file and colour pickers are reached
// through callables so that tests can answer them without opening anything.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    using DeviceListFn = std::function<std::vector<DeviceInfo>()>;
    // What the file chooser is being asked for, so that it can offer the
    // right kinds of file.
    enum class FileKind { Picture, Video };
    using FileChooserFn = std::function<QString(QWidget*, FileKind)>;
    using ColorChooserFn = std::function<QColor(QWidget*, const QColor&)>;
    // Installs a language. Injected because installing one is a property of
    // the application, not of this window, and a test needs to watch the
    // window ask without the window having an application to ask.
    using LanguageChangeFn = std::function<bool(const QString&)>;

    MainWindow(IPipelineController& controller,
               ILoopbackAdmin& admin,
               QSettings& store,
               DeviceListFn devices,
               FileChooserFn chooseFile,
               ColorChooserFn chooseColor,
               LanguageChangeFn changeLanguage = {},
               QWidget* parent = nullptr);

    // Re-reads the device list and the loopback state and updates the window.
    void Refresh();

protected:
    void closeEvent(QCloseEvent* event) override;
    // Qt sends this to every widget when a translator is installed or
    // removed, which is how the window learns to rewrite itself.
    void changeEvent(QEvent* event) override;
    // Clicks on the Background tab's previews, which select their mode.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    // Opens where the window was last left, or else at a size that suits the
    // screen it is on, rather than at a fixed size that may not fit.
    void ApplyInitialGeometry();
    void ConnectSignals();
    void ApplySettingsToUi();
    void PushSettingsToPipeline();
    void PushMaskSettingsToPipeline();
    void UpdateImagePreview(const QString& path);
    void UpdateVideoPreview(const QString& path);
    void UpdateColorSwatch();
    void ShowMessage(const QString& message);
    void SetRunningState(bool running);
    void UpdateUseHint();
    // Every string the window shows that is not an error or a measurement,
    // written from the current translation. Called when the window is built
    // and again whenever the language changes.
    void RetranslateUi();
    void UpdateSliderLabels();
    void OnStartStopClicked();
    void OnSetupClicked();
    void OnCopyCommandClicked();
    // The field that holds a command for the user to run in a terminal, for
    // where the window cannot ask for the password itself. An empty command
    // hides it.
    void ShowManualCommand(const QString& command);
    void OnChooseImageClicked();
    void OnChooseVideoClicked();
    void OnChooseColorClicked();

    IPipelineController& controller_;
    ILoopbackAdmin& admin_;
    QSettings& store_;
    DeviceListFn devices_;
    FileChooserFn chooseFile_;
    ColorChooserFn chooseColor_;
    LanguageChangeFn changeLanguage_;

    AppSettings settings_;
    QString loopbackPath_;
    bool running_ = false;

    QComboBox* cameraCombo_ = nullptr;
    QLabel* cameraLabel_ = nullptr;
    QComboBox* languageCombo_ = nullptr;
    QLabel* languageLabel_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QLabel* virtualCameraLabel_ = nullptr;
    QPushButton* setupButton_ = nullptr;
    QLabel* setupHint_ = nullptr;
    QLabel* manualCommand_ = nullptr;
    QPushButton* copyCommandButton_ = nullptr;
    QLabel* useHint_ = nullptr;
    QRadioButton* blurRadio_ = nullptr;
    QRadioButton* imageRadio_ = nullptr;
    QRadioButton* colorRadio_ = nullptr;
    QRadioButton* videoRadio_ = nullptr;
    QSlider* blurSlider_ = nullptr;
    QLabel* blurLabel_ = nullptr;
    QSlider* maskThresholdSlider_ = nullptr;
    QSlider* maskSmoothingSlider_ = nullptr;
    QSlider* maskEdgeSlider_ = nullptr;
    QLabel* maskThresholdLabel_ = nullptr;
    QLabel* maskSmoothingLabel_ = nullptr;
    QLabel* maskEdgeLabel_ = nullptr;
    QPushButton* chooseImageButton_ = nullptr;
    QLabel* imagePreviewLabel_ = nullptr;
    QPushButton* chooseVideoButton_ = nullptr;
    QLabel* videoPreviewLabel_ = nullptr;
    QPushButton* chooseColorButton_ = nullptr;
    QLabel* colorSwatch_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};

}  // namespace wbc
