// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/MainWindow.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "comp/OpenCvVideoDecoder.h"
#include "ui/ProgramIcon.h"
#include "core/ImageConvert.h"

namespace wbc {
namespace {

// The control panel's width. Wide enough for the longest translated label to
// wrap onto two lines rather than five, narrow enough to leave a 1024-pixel
// screen a preview of useful size.
constexpr int kPanelWidth = 340;

// Where the window's size and position are kept between runs.
constexpr auto kGeometryKey = "ui/geometry";

// The size to open at when nothing is remembered: large enough for a good
// preview, and never more of the screen than this share of it.
constexpr QSize kPreferredSize{1280, 760};
constexpr double kLargestShareOfScreen = 0.9;

// The smallest preview still worth having: a 16:9 frame at a size where a
// face is recognisable.
constexpr QSize kPreviewMinimum{320, 180};

// Every preview on the Background tab -- the picture, the colour and the
// video -- is this size: 16:9, the shape of the camera's frame and so of the
// background the preview stands for.
constexpr QSize kPreviewBoxSize{96, 54};

// A framed box of exactly kPreviewBoxSize inside its frame, whatever is put
// in it, so that the row it sits in never changes shape.
QLabel* MakePreviewBox(QWidget* parent, const char* name) {
    auto* box = new QLabel(parent);
    box->setObjectName(name);
    box->setFrameShape(QFrame::StyledPanel);
    box->setAlignment(Qt::AlignCenter);
    const int frame = 2 * box->frameWidth();
    box->setFixedSize(kPreviewBoxSize + QSize(frame, frame));
    return box;
}

// A preview and the button that changes it, indented under their mode.
QHBoxLayout* MakeChoiceRow(int indent, QLabel* preview, QPushButton* button) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(indent, 0, 0, 0);
    row->addWidget(preview);
    row->addWidget(button, 1);
    return row;
}

// The image cropped from its centre to fill `box`, as CoverCrop crops the
// background to fill the frame: the preview shows the part of the picture
// that will actually be behind the person, not the whole file shrunk.
QPixmap CoverThumbnail(const QImage& image, QSize box) {
    const QImage scaled =
        image.scaled(box, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return QPixmap::fromImage(scaled.copy((scaled.width() - box.width()) / 2,
                                          (scaled.height() - box.height()) / 2, box.width(),
                                          box.height()));
}

// A settings tab's contents, in a scroll area that only scrolls vertically:
// the panel has a fixed width, so a horizontal scroll bar would mean a
// control too wide for it, which should be fixed rather than scrolled to.
QScrollArea* MakeScrollable(QWidget* page) {
    auto* area = new QScrollArea;
    area->setWidget(page);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return area;
}

// A mask control is a label carrying the name and the current value, then the
// slider itself. The label's text is written by the slider's handler, so it is
// handed back through `label` rather than being set here.
QSlider* MakeMaskSlider(QWidget* parent,
                        QVBoxLayout* layout,
                        const char* sliderName,
                        const char* labelName,
                        QLabel** label,
                        int maximum) {
    *label = new QLabel(parent);
    (*label)->setObjectName(labelName);
    // Wraps, because the panel's width is fixed and a label that does not
    // decides the panel's minimum width by itself: in Russian, in a wider
    // font than the author's, "Stabilise the mask over time: 70%" was wider
    // than the panel.
    (*label)->setWordWrap(true);
    layout->addWidget(*label);

    auto* slider = new QSlider(Qt::Horizontal, parent);
    slider->setObjectName(sliderName);
    slider->setRange(0, maximum);
    layout->addWidget(slider);
    return slider;
}

}  // namespace

MainWindow::MainWindow(IPipelineController& controller,
                       ILoopbackAdmin& admin,
                       QSettings& store,
                       DeviceListFn devices,
                       FileChooserFn chooseFile,
                       ColorChooserFn chooseColor,
                       LanguageChangeFn changeLanguage,
                       QWidget* parent)
    : QMainWindow(parent),
      controller_(controller),
      admin_(admin),
      store_(store),
      devices_(std::move(devices)),
      chooseFile_(std::move(chooseFile)),
      chooseColor_(std::move(chooseColor)),
      changeLanguage_(std::move(changeLanguage)),
      settings_(LoadSettings(store)) {
    // A settings file may hold no language at all, or one this build no
    // longer ships. Either way the window has to open in something.
    settings_.language = ResolveLanguage(settings_.language, QLocale::system());
    setWindowIcon(ProgramIcon());
    BuildUi();
    ConnectSignals();
    ApplySettingsToUi();
    Refresh();
    ApplyInitialGeometry();
}

void MainWindow::ApplyInitialGeometry() {
    // A remembered size and position win. Qt puts a window that was left on
    // a monitor that has since gone back onto one that is still there.
    if (restoreGeometry(store_.value(kGeometryKey).toByteArray())) {
        return;
    }
    const QSize available = screen()->availableGeometry().size();
    resize(kPreferredSize.boundedTo(available * kLargestShareOfScreen)
               .expandedTo(minimumSizeHint()));
}

void MainWindow::BuildUi() {
    // Two columns, because screens are wider than they are tall: stacked in
    // one column this window needed 917 pixels of height and 338 of width,
    // which no screen below Full HD has to give, and left most of the width
    // unused. The preview takes whatever width the panel does not.
    auto* central = new QWidget(this);
    auto* outer = new QVBoxLayout(central);
    auto* columns = new QHBoxLayout();
    outer->addLayout(columns, 1);

    previewLabel_ = new QLabel(central);
    previewLabel_->setObjectName("previewLabel");
    previewLabel_->setMinimumSize(kPreviewMinimum);
    // Ignored, so that the pixmap shown in it cannot push the window larger:
    // a label otherwise asks for the size of its pixmap, which is scaled to
    // the label, which then asks for more.
    previewLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    previewLabel_->setAlignment(Qt::AlignCenter);
    columns->addWidget(previewLabel_, 1);

    auto* panel = new QWidget(central);
    panel->setObjectName("controlPanel");
    panel->setFixedWidth(kPanelWidth);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    columns->addWidget(panel);

    // What the program cannot be used without sits at the top of the panel,
    // where no amount of settings can push it out of view.
    auto* deviceRow = new QHBoxLayout();
    cameraLabel_ = new QLabel(panel);
    cameraLabel_->setObjectName("cameraLabel");
    deviceRow->addWidget(cameraLabel_);
    cameraCombo_ = new QComboBox(panel);
    cameraCombo_->setObjectName("cameraCombo");
    // A long device name would otherwise widen the whole panel.
    cameraCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    cameraCombo_->setMinimumContentsLength(12);
    deviceRow->addWidget(cameraCombo_, 1);
    panelLayout->addLayout(deviceRow);

    virtualCameraLabel_ = new QLabel(panel);
    virtualCameraLabel_->setObjectName("virtualCameraLabel");
    virtualCameraLabel_->setWordWrap(true);
    panelLayout->addWidget(virtualCameraLabel_);

    useHint_ = new QLabel(panel);
    useHint_->setObjectName("useHint");
    useHint_->setWordWrap(true);
    panelLayout->addWidget(useHint_);

    setupHint_ = new QLabel(panel);
    setupHint_->setObjectName("setupHint");
    setupHint_->setWordWrap(true);
    setupHint_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    panelLayout->addWidget(setupHint_);

    // Selectable with the keyboard as well as the mouse, and with a button
    // beside it, because the command is meant to be pasted into a terminal
    // and a mistyped option fails there without saying which one.
    auto* manualRow = new QHBoxLayout();
    manualCommand_ = new QLabel(panel);
    manualCommand_->setObjectName("manualCommand");
    manualCommand_->setWordWrap(true);
    manualCommand_->setTextFormat(Qt::PlainText);
    manualCommand_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    manualCommand_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                            Qt::TextSelectableByKeyboard);
    manualRow->addWidget(manualCommand_, 1);
    copyCommandButton_ = new QPushButton(panel);
    copyCommandButton_->setObjectName("copyCommandButton");
    manualRow->addWidget(copyCommandButton_, 0, Qt::AlignTop);
    panelLayout->addLayout(manualRow);

    setupButton_ = new QPushButton(panel);
    setupButton_->setObjectName("setupButton");
    panelLayout->addWidget(setupButton_);

    startButton_ = new QPushButton(panel);
    startButton_->setObjectName("startButton");
    panelLayout->addWidget(startButton_);

    statsLabel_ = new QLabel(panel);
    statsLabel_->setObjectName("statsLabel");
    panelLayout->addWidget(statsLabel_);

    // The settings, which are changed now and then rather than used, share
    // the rest of the panel in tabs, each inside a scroll area: on a screen
    // too small for a tab it scrolls rather than being cut off.
    tabs_ = new QTabWidget(panel);
    tabs_->setObjectName("settingsTabs");
    panelLayout->addWidget(tabs_, 1);

    // Each mode, then indented beneath it what belongs to it, so that no
    // setting has to be matched to its mode by guesswork. The picture, the
    // colour and the video each have a preview of one fixed size, and their
    // buttons one width, whatever the shape of the file chosen.
    auto* backgroundPage = new QWidget;
    auto* modeLayout = new QVBoxLayout(backgroundPage);
    blurRadio_ = new QRadioButton(backgroundPage);
    blurRadio_->setObjectName("blurRadio");
    imageRadio_ = new QRadioButton(backgroundPage);
    imageRadio_->setObjectName("imageRadio");
    colorRadio_ = new QRadioButton(backgroundPage);
    colorRadio_->setObjectName("colorRadio");
    videoRadio_ = new QRadioButton(backgroundPage);
    videoRadio_->setObjectName("videoRadio");

    blurLabel_ = new QLabel(backgroundPage);
    blurLabel_->setObjectName("blurLabel");
    blurLabel_->setWordWrap(true);
    blurSlider_ = new QSlider(Qt::Horizontal, backgroundPage);
    blurSlider_->setObjectName("blurSlider");
    blurSlider_->setRange(0, kMaxBlurRadius);

    imagePreviewLabel_ = MakePreviewBox(backgroundPage, "imagePreviewLabel");
    chooseImageButton_ = new QPushButton(backgroundPage);
    chooseImageButton_->setObjectName("chooseImageButton");
    colorSwatch_ = MakePreviewBox(backgroundPage, "colorSwatch");
    chooseColorButton_ = new QPushButton(backgroundPage);
    chooseColorButton_->setObjectName("chooseColorButton");
    videoPreviewLabel_ = MakePreviewBox(backgroundPage, "videoPreviewLabel");
    chooseVideoButton_ = new QPushButton(backgroundPage);
    chooseVideoButton_->setObjectName("chooseVideoButton");

    // Indented to where a mode button's text begins, past its indicator.
    const int indent = style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
                       style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

    auto* blurBlock = new QVBoxLayout();
    blurBlock->setContentsMargins(indent, 0, 0, 0);
    blurBlock->addWidget(blurLabel_);
    blurBlock->addWidget(blurSlider_);

    modeLayout->addWidget(blurRadio_);
    modeLayout->addLayout(blurBlock);
    modeLayout->addWidget(imageRadio_);
    modeLayout->addLayout(MakeChoiceRow(indent, imagePreviewLabel_, chooseImageButton_));
    modeLayout->addWidget(colorRadio_);
    modeLayout->addLayout(MakeChoiceRow(indent, colorSwatch_, chooseColorButton_));
    modeLayout->addWidget(videoRadio_);
    modeLayout->addLayout(MakeChoiceRow(indent, videoPreviewLabel_, chooseVideoButton_));
    modeLayout->addStretch(1);

    // A preview is part of its mode, and a click on it selects that mode.
    for (QLabel* preview : {imagePreviewLabel_, colorSwatch_, videoPreviewLabel_}) {
        preview->setCursor(Qt::PointingHandCursor);
        preview->installEventFilter(this);
    }

    tabs_->addTab(MakeScrollable(backgroundPage), QString{});

    // The mask the model produces is not always clean: it leaves a faint
    // alpha on whatever the room contains, which shows through the background
    // as a ghost of the real interior. Nothing here can be given a sensible
    // fixed value -- it depends on the room, the light and the camera -- so
    // the three that matter are the user's to turn.
    auto* maskPage = new QWidget;
    auto* maskLayout = new QVBoxLayout(maskPage);

    maskThresholdSlider_ = MakeMaskSlider(maskPage, maskLayout, "maskThresholdSlider",
                                          "maskThresholdLabel", &maskThresholdLabel_,
                                          static_cast<int>(kMaxMaskThreshold * 100.0F));
    maskSmoothingSlider_ = MakeMaskSlider(maskPage, maskLayout, "maskSmoothingSlider",
                                          "maskSmoothingLabel", &maskSmoothingLabel_,
                                          static_cast<int>(kMaxMaskSmoothing * 100.0F));
    maskEdgeSlider_ = MakeMaskSlider(maskPage, maskLayout, "maskEdgeSlider", "maskEdgeLabel",
                                     &maskEdgeLabel_, kMaxMaskEdgeRadius);

    maskLayout->addStretch(1);

    tabs_->addTab(MakeScrollable(maskPage), QString{});

    // Along the bottom, across both columns: what went wrong, which can be a
    // long sentence and so gets the width, and the language.
    auto* bottomRow = new QHBoxLayout();
    messageLabel_ = new QLabel(central);
    messageLabel_->setObjectName("messageLabel");
    messageLabel_->setWordWrap(true);
    bottomRow->addWidget(messageLabel_, 1);
    languageLabel_ = new QLabel(central);
    languageLabel_->setObjectName("languageLabel");
    bottomRow->addWidget(languageLabel_);
    languageCombo_ = new QComboBox(central);
    languageCombo_->setObjectName("languageCombo");
    for (const QString& code : AvailableLanguages()) {
        languageCombo_->addItem(LanguageDisplayName(code), code);
    }
    bottomRow->addWidget(languageCombo_);
    outer->addLayout(bottomRow);

    RetranslateUi();

    setCentralWidget(central);
}

void MainWindow::ConnectSignals() {
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::OnStartStopClicked);
    connect(setupButton_, &QPushButton::clicked, this, &MainWindow::OnSetupClicked);
    connect(copyCommandButton_, &QPushButton::clicked, this, &MainWindow::OnCopyCommandClicked);
    connect(chooseImageButton_, &QPushButton::clicked, this, &MainWindow::OnChooseImageClicked);
    connect(chooseVideoButton_, &QPushButton::clicked, this, &MainWindow::OnChooseVideoClicked);
    connect(chooseColorButton_, &QPushButton::clicked, this, &MainWindow::OnChooseColorClicked);

    connect(blurRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            settings_.mode = BackgroundMode::Blur;
            PushSettingsToPipeline();
        }
    });
    connect(colorRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            settings_.mode = BackgroundMode::Color;
            PushSettingsToPipeline();
        }
    });
    connect(imageRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            settings_.mode = BackgroundMode::Image;
            PushSettingsToPipeline();
        }
    });
    connect(videoRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            settings_.mode = BackgroundMode::Video;
            PushSettingsToPipeline();
        }
    });
    connect(blurSlider_, &QSlider::valueChanged, this, [this](int value) {
        settings_.blurRadius = value;
        UpdateSliderLabels();
        PushSettingsToPipeline();
    });
    connect(maskThresholdSlider_, &QSlider::valueChanged, this, [this](int value) {
        settings_.mask.threshold = static_cast<float>(value) / 100.0F;
        UpdateSliderLabels();
        PushMaskSettingsToPipeline();
    });
    connect(maskSmoothingSlider_, &QSlider::valueChanged, this, [this](int value) {
        settings_.mask.smoothing = static_cast<float>(value) / 100.0F;
        UpdateSliderLabels();
        PushMaskSettingsToPipeline();
    });
    connect(languageCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString code = languageCombo_->itemData(index).toString();
        if (code.isEmpty() || code == settings_.language) {
            return;
        }
        settings_.language = code;
        if (changeLanguage_) {
            // The list offers only the languages this build embeds, and the
            // application installs every one of those, so there is no refusal
            // to report.
            static_cast<void>(changeLanguage_(code));
        }
    });
    connect(maskEdgeSlider_, &QSlider::valueChanged, this, [this](int value) {
        settings_.mask.edgeRadius = value;
        UpdateSliderLabels();
        PushMaskSettingsToPipeline();
    });

    connect(&controller_, &IPipelineController::previewReady, this, [this](const QImage& image) {
        previewLabel_->setPixmap(
            QPixmap::fromImage(image).scaled(previewLabel_->size(), Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
        // Only now is the pipeline free to send another one. Until this call
        // it drops previews rather than queueing them behind a GUI thread
        // that is not keeping up.
        controller_.PreviewConsumed();
    });
    connect(&controller_, &IPipelineController::statsUpdated, this,
            [this](double fps, double latencyMs, qulonglong dropped) {
                statsLabel_->setText(tr("%1 fps · %2 ms · %3 dropped")
                                         .arg(fps, 0, 'f', 1)
                                         .arg(latencyMs, 0, 'f', 1)
                                         .arg(dropped));
            });
    connect(&controller_, &IPipelineController::formatNegotiated, this,
            [this](int width, int height, double fps) {
                // Spec section 8, row 7. Rebuilt from loopbackPath_ rather than
                // appended to, so that a second Start does not leave two
                // geometries in the line.
                virtualCameraLabel_->setText(tr("Virtual camera: %1 · capturing %2x%3 at %4 fps")
                                                 .arg(loopbackPath_)
                                                 .arg(width)
                                                 .arg(height)
                                                 .arg(fps, 0, 'f', 0));
            });
    connect(&controller_, &IPipelineController::failed, this, [this](const QString& message) {
        // The worker loop leaves on its own when a frame fails, but only Stop()
        // closes the camera and the virtual camera. Without this call the
        // button would say Start while the capture device was still open and
        // streaming, and no other application could have it.
        controller_.Stop();
        ShowMessage(message);
        SetRunningState(false);
    });
}

void MainWindow::ApplySettingsToUi() {
    blurSlider_->setValue(settings_.blurRadius);
    // setValue only emits valueChanged when the value actually moves, and a
    // slider that starts at the stored value would leave its label empty, so
    // each label is written here as well as in the slider's own handler.
    maskThresholdSlider_->setValue(static_cast<int>(settings_.mask.threshold * 100.0F));
    maskSmoothingSlider_->setValue(static_cast<int>(settings_.mask.smoothing * 100.0F));
    maskEdgeSlider_->setValue(settings_.mask.edgeRadius);
    // setValue only emits valueChanged when the value actually moves, so a
    // slider that already stood at the stored value would leave its label
    // unwritten.
    UpdateSliderLabels();
    languageCombo_->setCurrentIndex(languageCombo_->findData(settings_.language));
    UpdateImagePreview(settings_.backgroundImagePath);
    UpdateVideoPreview(settings_.backgroundVideoPath);
    UpdateColorSwatch();
    switch (settings_.mode) {
        case BackgroundMode::Blur:
            blurRadio_->setChecked(true);
            break;
        case BackgroundMode::Image:
            imageRadio_->setChecked(true);
            break;
        case BackgroundMode::Color:
            colorRadio_->setChecked(true);
            break;
        case BackgroundMode::Video:
            videoRadio_->setChecked(true);
            break;
    }
}

void MainWindow::Refresh() {
    const auto all = devices_();
    const auto cameras =
        SelectCaptureDevices(all, kVirtualCameraLabel);

    const QString previous = cameraCombo_->currentData().toString();
    cameraCombo_->clear();
    for (const DeviceInfo& camera : cameras) {
        cameraCombo_->addItem(QString::fromStdString(camera.name), QString::fromStdString(camera.path));
    }

    const QString wanted = previous.isEmpty() ? settings_.cameraPath : previous;
    const int index = cameraCombo_->findData(wanted);
    if (index >= 0) {
        cameraCombo_->setCurrentIndex(index);
    } else if (!wanted.isEmpty() && cameraCombo_->count() > 0) {
        // QComboBox already selected the first item when the list was refilled,
        // so the selection has moved on its own. Saying nothing here would let
        // the user press Start and stream from a camera they never chose.
        ShowMessage(tr("The camera you had selected is no longer present. %1 is selected instead.")
                        .arg(cameraCombo_->currentText()));
    }

    const LoopbackState state = admin_.Probe();
    if (state == LoopbackState::Ready) {
        loopbackPath_ = QString::fromStdString(admin_.DevicePath());
        virtualCameraLabel_->setText(tr("Virtual camera: %1").arg(loopbackPath_));
        setupHint_->clear();
        setupHint_->setVisible(false);
        setupButton_->setVisible(false);
        ShowManualCommand(QString{});
        UpdateUseHint();
        return;
    }

    loopbackPath_.clear();
    virtualCameraLabel_->setText(state == LoopbackState::ModuleNotLoaded
                                     ? tr("Virtual camera: not set up yet")
                                     : tr("Virtual camera: the kernel module is not installed"));
    setupHint_->setVisible(true);

    if (admin_.InFlatpak()) {
        // Nothing inside the sandbox can install or load a module on the
        // host, so there is no button, only the commands. Both of them:
        // installing a package that is already there changes nothing, and
        // what the sandbox can see of the host's modules is not to be relied
        // on to tell which is needed.
        setupHint_->setText(tr("This copy of the program runs in a Flatpak sandbox, from which it "
                               "cannot set up the virtual camera. Run these commands on your "
                               "system, outside the sandbox:"));
        setupButton_->setVisible(false);
        ShowManualCommand(QString::fromStdString(admin_.InstallCommand() + "\n" +
                                                 admin_.ManualCommand()));
    } else if (state == LoopbackState::ModuleNotInstalled) {
        setupHint_->setText(tr("Install it first:\n%1")
                                .arg(QString::fromStdString(admin_.InstallCommand())));
        setupButton_->setVisible(true);
        // Asking for a password to load a module that is not there would
        // waste the user's time.
        setupButton_->setEnabled(false);
        ShowManualCommand(QString{});
    } else if (admin_.NeedsManualSetup()) {
        // pkexec has already failed to ask once, and would again: the button
        // gives way to the command.
        setupHint_->setText(tr("Run this in a terminal to set it up:"));
        setupButton_->setVisible(false);
        ShowManualCommand(QString::fromStdString(admin_.ManualCommand()));
    } else {
        setupHint_->setText(tr("Run this to create it:\n%1")
                                .arg(QString::fromStdString(admin_.LoadCommand())));
        setupButton_->setVisible(true);
        setupButton_->setEnabled(true);
        ShowManualCommand(QString{});
    }

    UpdateUseHint();
}

void MainWindow::PushSettingsToPipeline() {
    controller_.UpdateCompositorSettings(settings_.ToCompositorSettings());
}

void MainWindow::PushMaskSettingsToPipeline() {
    controller_.UpdateMaskSettings(settings_.mask);
}

void MainWindow::UpdateImagePreview(const QString& path) {
    // Spec section 7 asks for a thumbnail of the chosen picture: after a
    // restart in picture mode it is the only thing that says which picture is
    // in use. A file that has gone away since it was chosen clears the
    // thumbnail and leaves the message area to explain.
    // Asked only for a path there is: QImage warns on an empty one.
    const QImage picture = path.isEmpty() ? QImage{} : QImage(path);
    if (picture.isNull()) {
        imagePreviewLabel_->clear();
        imagePreviewLabel_->setToolTip(QString{});
        return;
    }
    imagePreviewLabel_->setPixmap(CoverThumbnail(picture, kPreviewBoxSize));
    imagePreviewLabel_->setToolTip(QFileInfo(path).fileName());
}

void MainWindow::UpdateVideoPreview(const QString& path) {
    // The video's opening frame, for the same reason the picture has a
    // thumbnail: after a restart it is what says which video is in use.
    const cv::Mat first = path.isEmpty() ? cv::Mat{} : FirstFrameOf(path.toStdString());
    if (first.empty()) {
        videoPreviewLabel_->clear();
        videoPreviewLabel_->setToolTip(QString{});
        return;
    }
    videoPreviewLabel_->setPixmap(CoverThumbnail(ToQImage(first), kPreviewBoxSize));
    videoPreviewLabel_->setToolTip(QFileInfo(path).fileName());
}

void MainWindow::UpdateColorSwatch() {
    QPixmap swatch(kPreviewBoxSize);
    swatch.fill(settings_.color);
    colorSwatch_->setPixmap(swatch);
    colorSwatch_->setToolTip(settings_.color.name(QColor::HexRgb));
}

void MainWindow::UpdateSliderLabels() {
    blurLabel_->setText(tr("Blur strength: %1 px").arg(blurSlider_->value()));
    maskThresholdLabel_->setText(tr("Cut off background below: %1%").arg(maskThresholdSlider_->value()));
    maskSmoothingLabel_->setText(tr("Steady the mask over time: %1%").arg(maskSmoothingSlider_->value()));
    maskEdgeLabel_->setText(tr("Follow the edges in the frame: %1 px").arg(maskEdgeSlider_->value()));
}

void MainWindow::RetranslateUi() {
    setWindowTitle(tr("Webcam Background Changer"));
    cameraLabel_->setText(tr("Camera:"));
    languageLabel_->setText(tr("Language:"));
    setupButton_->setText(tr("Set up the virtual camera"));
    copyCommandButton_->setText(tr("Copy"));
    tabs_->setTabText(0, tr("Background"));
    blurRadio_->setText(tr("Blur"));
    imageRadio_->setText(tr("Picture"));
    colorRadio_->setText(tr("Solid colour"));
    videoRadio_->setText(tr("Video"));
    chooseImageButton_->setText(tr("Choose a picture..."));
    chooseColorButton_->setText(tr("Choose a colour..."));
    chooseVideoButton_->setText(tr("Choose a video..."));
    tabs_->setTabText(1, tr("Mask"));
    if (previewLabel_->pixmap().isNull()) {
        previewLabel_->setText(tr("No preview yet"));
    }

    // These three write text of their own from the current state, so they are
    // asked to write it again rather than being duplicated here. The message
    // area and the statistics are left alone: both hold a sentence already
    // built from values, and there is nothing to rebuild it from.
    UpdateSliderLabels();
    SetRunningState(running_);
    Refresh();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // On release rather than press, as a button does: a press that is
    // dragged off the preview and let go elsewhere selects nothing.
    if (event->type() == QEvent::MouseButtonRelease &&
        static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        QRadioButton* radio = watched == imagePreviewLabel_ ? imageRadio_
                              : watched == colorSwatch_     ? colorRadio_
                              : watched == videoPreviewLabel_ ? videoRadio_
                                                              : nullptr;
        if (radio != nullptr) {
            radio->setChecked(true);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        RetranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::ShowMessage(const QString& message) {
    messageLabel_->setText(message);
}

void MainWindow::SetRunningState(bool running) {
    running_ = running;
    startButton_->setText(running ? tr("Stop") : tr("Start"));
    cameraCombo_->setEnabled(!running);
    UpdateUseHint();
}

// The loopback node is created with exclusive_caps=1, so it advertises itself
// as a camera only while this application is writing frames to it. A user who
// goes looking for it before pressing Start does not find it,
// and the application they were looking in has already cached the camera list
// it built without us. Both halves of that are worth saying out loud.
void MainWindow::UpdateUseHint() {
    if (loopbackPath_.isEmpty()) {
        useHint_->clear();
        useHint_->setVisible(false);
        return;
    }

    useHint_->setVisible(true);
    if (running_) {
        useHint_->setText(tr("Now choose %1 in the other application. If it was already open "
                             "before you pressed Start, restart it first.")
                              .arg(QString::fromLatin1(kVirtualCameraLabel)));
    } else {
        useHint_->setText(tr("Other applications list %1 only while this window is streaming. "
                             "Press Start here first, then choose it there.")
                              .arg(QString::fromLatin1(kVirtualCameraLabel)));
    }
}

void MainWindow::OnStartStopClicked() {
    if (running_) {
        controller_.Stop();
        SetRunningState(false);
        return;
    }

    if (cameraCombo_->count() == 0) {
        ShowMessage(tr("No camera was found. Connect one and press the button again."));
        return;
    }
    if (loopbackPath_.isEmpty()) {
        ShowMessage(tr("The virtual camera is not set up yet; see the note above."));
        return;
    }

    PipelineConfig config;
    config.cameraPath = cameraCombo_->currentData().toString().toStdString();
    config.sinkPath = loopbackPath_.toStdString();
    config.compositor = settings_.ToCompositorSettings();
    config.mask = settings_.mask;
    config.backgroundImagePath = settings_.backgroundImagePath;
    config.backgroundVideoPath = settings_.backgroundVideoPath;

    if (!controller_.Start(config)) {
        ShowMessage(controller_.LastError());
        SetRunningState(false);
        return;
    }

    settings_.cameraPath = cameraCombo_->currentData().toString();
    ShowMessage(QString{});
    SetRunningState(true);
}

void MainWindow::OnSetupClicked() {
    if (!admin_.LoadModule()) {
        // A failure can change what the panel offers -- once pkexec cannot
        // ask for a password, the command replaces the button -- so the panel
        // is rebuilt before the error is shown under it.
        Refresh();
        ShowMessage(QString::fromStdString(admin_.LastError()));
        return;
    }
    ShowMessage(QString{});
    Refresh();
}

void MainWindow::OnCopyCommandClicked() {
    QGuiApplication::clipboard()->setText(manualCommand_->text());
}

void MainWindow::ShowManualCommand(const QString& command) {
    manualCommand_->setText(command);
    manualCommand_->setVisible(!command.isEmpty());
    copyCommandButton_->setVisible(!command.isEmpty());
}

void MainWindow::OnChooseImageClicked() {
    const QString path = chooseFile_(this, FileKind::Picture);
    if (path.isEmpty()) {
        return;
    }
    if (!controller_.SetBackgroundImage(path)) {
        ShowMessage(controller_.LastError());
        return;
    }

    settings_.backgroundImagePath = path;
    UpdateImagePreview(path);
    imageRadio_->setChecked(true);
    ShowMessage(QString{});
}

void MainWindow::OnChooseVideoClicked() {
    const QString path = chooseFile_(this, FileKind::Video);
    if (path.isEmpty()) {
        return;
    }
    if (!controller_.SetBackgroundVideo(path)) {
        ShowMessage(controller_.LastError());
        return;
    }

    settings_.backgroundVideoPath = path;
    UpdateVideoPreview(path);
    videoRadio_->setChecked(true);
    ShowMessage(QString{});
}

void MainWindow::OnChooseColorClicked() {
    const QColor chosen = chooseColor_(this, settings_.color);
    if (!chosen.isValid()) {
        return;
    }
    settings_.color = chosen;
    UpdateColorSwatch();
    colorRadio_->setChecked(true);
    PushSettingsToPipeline();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    controller_.Stop();
    SaveSettings(store_, settings_);
    store_.setValue(kGeometryKey, saveGeometry());
    QMainWindow::closeEvent(event);
}

}  // namespace wbc
