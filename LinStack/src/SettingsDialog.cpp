#include "SettingsDialog.h"
#include "Theme.h"
#include "Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPainter>
#include <QSize>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSettings>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

// ============================================================================
// Interactive Hotkey KeyCaptureButton
// ============================================================================
class KeyCaptureButton : public QPushButton {
public:
    KeyCaptureButton(const QString &id, const QString &initialSeq, QWidget *parent = nullptr)
        : QPushButton(initialSeq, parent), m_id(id), m_savedSeq(initialSeq) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        applyNormalStyle();
        connect(this, &QPushButton::clicked, this, &KeyCaptureButton::startListening);
    }

    QString keyId() const { return m_id; }
    QString sequence() const { return m_savedSeq; }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (!m_listening) {
            QPushButton::keyPressEvent(e);
            return;
        }
        e->accept();
        int key = e->key();

        if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt ||
            key == Qt::Key_AltGr || key == Qt::Key_Meta) {
            return;
        }

        if (key == Qt::Key_Escape) {
            stopListening(m_savedSeq);
            return;
        }

        QKeyCombination combo(e->modifiers(), static_cast<Qt::Key>(key));
        QString seqStr = QKeySequence(combo).toString(QKeySequence::PortableText);
        if (!seqStr.isEmpty()) {
            m_savedSeq = seqStr;
            QSettings("LinStack", "LinStack").setValue("shortcuts/" + m_id, m_savedSeq);
        }
        stopListening(m_savedSeq);
    }

    void focusOutEvent(QFocusEvent *e) override {
        if (m_listening) {
            stopListening(m_savedSeq);
        }
        QPushButton::focusOutEvent(e);
    }

private:
    void startListening() {
        m_listening = true;
        setText("⌨ Press keys...");
        setStyleSheet(
            "QPushButton { background-color: rgba(0, 255, 102, 0.15); color: #00FF66; "
            "border: 1px solid #00FF66; border-radius: 5px; font-weight: 700; padding: 4px 10px; }"
        );
    }

    void stopListening(const QString &display) {
        m_listening = false;
        setText(display);
        applyNormalStyle();
        clearFocus();
    }

    void applyNormalStyle() {
        setStyleSheet(
            "QPushButton { background-color: #161F30; color: #00FF66; border: 1px solid #1F2937; "
            "border-radius: 5px; font-weight: 700; font-family: monospace; padding: 4px 10px; }"
            "QPushButton:hover { border-color: #10B981; background-color: #1E293B; }"
        );
    }

    QString m_id;
    QString m_savedSeq;
    bool m_listening = false;
};

// ============================================================================
// SettingsDialog Implementation
// ============================================================================

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("LinStack 1.0 - Settings");
    setFixedSize(760, 560);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    setupUi();
}

void SettingsDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    // Titlebar row
    auto *titleRow = new QHBoxLayout();
    auto *titleIcon = new QLabel(this);
    titleIcon->setPixmap(Theme::renderIconPixmap(Theme::IconType::Settings, Theme::ColorNeon, 22));
    titleRow->addWidget(titleIcon);

    auto *titleText = new QLabel("⚙ LinStack Configuration & Engine Settings", this);
    titleText->setStyleSheet("color: #F9FAFB; font-size: 15px; font-weight: 700;");
    titleRow->addWidget(titleText);

    titleRow->addStretch(1);

    auto *btnClose = new QPushButton(this);
    btnClose->setFixedSize(28, 28);
    btnClose->setIcon(Theme::createIcon(Theme::IconType::WindowClose, Theme::ColorTextMuted, 14));
    btnClose->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:hover { background: #EF4444; border-radius: 4px; }");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(btnClose);

    rootLayout->addLayout(titleRow);

    // Horizontal Split Layout: Left Sidebar + Right Stacked Widget
    auto *mainSplit = new QHBoxLayout();
    mainSplit->setSpacing(12);

    // Left Sidebar Navigation
    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(185);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setStyleSheet(
        "QListWidget {"
        "  background-color: #0D121D;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 8px;"
        "  outline: none;"
        "  padding: 6px 4px;"
        "}"
        "QListWidget::item {"
        "  height: 38px;"
        "  color: #9CA3AF;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  padding-left: 10px;"
        "  margin: 2px 2px;"
        "  border-radius: 6px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #161F30;"
        "  color: #10B981;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(16, 185, 129, 0.16);"
        "  color: #00FF66;"
        "  border-left: 3px solid #00FF66;"
        "  font-weight: 700;"
        "}"
    );

    const QStringList navLabels = {
        "⚡  Performance",
        "🖥  Display",
        "🎮  Graphics & Audio",
        "🖱  Devices",
        "⚙  Preferences",
        "📱  Phone Profile",
        "⌨  Shortcuts",
        "📁  Shared Storage",
        "🛠  Advanced"
    };

    for (const auto &text : navLabels) {
        auto *item = new QListWidgetItem(text, m_sidebar);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    mainSplit->addWidget(m_sidebar);

    // Right Stacked Widget
    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet(
        "QStackedWidget {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 8px;"
        "}"
    );

    // 9 Sections in Exact Order
    m_stack->addWidget(createPerformanceTab());    // 0
    m_stack->addWidget(createDisplayTab());        // 1
    m_stack->addWidget(createGraphicsAudioTab());  // 2
    m_stack->addWidget(createDevicesTab());        // 3
    m_stack->addWidget(createPreferencesTab());    // 4
    m_stack->addWidget(createPhoneProfileTab());   // 5
    m_stack->addWidget(createShortcutsTab());      // 6
    m_stack->addWidget(createSharedStorageTab());  // 7
    m_stack->addWidget(createAdvancedTab());       // 8

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_sidebar->setCurrentRow(0);

    mainSplit->addWidget(m_stack, 1);
    rootLayout->addLayout(mainSplit, 1);

    // Bottom Action Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    auto *btnCancel = new QPushButton("Cancel", this);
    btnCancel->setFixedWidth(100);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(btnCancel);

    auto *btnSave = new QPushButton("Save Changes", this);
    btnSave->setProperty("primary", true);
    btnSave->setFixedWidth(130);
    connect(btnSave, &QPushButton::clicked, this, [this]() {
        QSettings s("LinStack", "LinStack");

        // 1. Performance
        s.setValue("perf/mode", m_comboPerfMode->currentIndex());
        s.setValue("perf/fps", m_sliderFps->value());
        s.setValue("perf/vsync", isVsyncEnabled());

        // 2. Display
        s.setValue("display/orientation", m_radioPortrait->isChecked() ? 1 : 0);
        s.setValue("display/resolution", m_comboResolution->currentText());
        s.setValue("display/dpi", dpi());

        // 3. Graphics & Audio
        s.setValue("graphics/renderer", m_comboRenderer->currentIndex());
        s.setValue("graphics/astc", m_comboAstc->currentIndex());
        s.setValue("graphics/audio", m_comboAudioBackend->currentIndex());

        // 4. Devices
        s.setValue("devices/passthrough_mode", m_comboPassthrough->currentIndex());
        s.setValue("devices/gamepad_autodetect", m_checkGamepadAuto->isChecked());
        s.setValue("devices/mic_device", m_comboMicDevice->currentIndex());
        s.setValue("devices/cam_device", m_comboCameraDevice->currentIndex());

        // 5. Preferences
        s.setValue("preferences/confirm_exit", confirmExitEnabled());
        s.setValue("preferences/show_toasts", m_checkShowToasts->isChecked());
        s.setValue("preferences/remember_geometry", m_checkRememberGeometry->isChecked());
        s.setValue("preferences/autostart_session", m_checkAutostartSession->isChecked());
        s.setValue("preferences/pause_minimized", m_checkPauseMinimized->isChecked());

        // 6. Phone Profile
        s.setValue("phone/profile", deviceProfile());

        // 8. Shared Storage
        s.setValue("storage/shared_enabled", isSharedStorageEnabled());
        s.setValue("storage/shared_linux_path", sharedStoragePath());

        // 9. Advanced
        s.setValue("advanced/adb_bridge_enabled", isAdbBridgeEnabled());
        s.setValue("advanced/swiftshader", isSwiftShaderEnabled());
        s.setValue("advanced/direct_dmabuf", m_checkDirectDmaBuf->isChecked());

        emit settingsApplied();
        accept();
    });
    btnRow->addWidget(btnSave);

    rootLayout->addLayout(btnRow);
}

// ----------------------------------------------------------------------------
// 1. Performance
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createPerformanceTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    // Engine Performance Mode
    auto *perfGroup = new QGroupBox("Engine Performance Mode", tab);
    perfGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *perfLayout = new QVBoxLayout(perfGroup);

    m_comboPerfMode = new QComboBox(perfGroup);
    m_comboPerfMode->addItem("High Performance Mode (Direct Mesa DRI3 Passthrough)");
    m_comboPerfMode->addItem("Compatibility Mode (Safe Render Fallback)");
    m_comboPerfMode->setCurrentIndex(s.value("perf/mode", 0).toInt());
    perfLayout->addWidget(m_comboPerfMode);
    layout->addWidget(perfGroup);

    // High Frame Rate Slider (60 to 240 FPS)
    auto *fpsGroup = new QGroupBox("Frame Rate (High FPS & Wayland V-Sync)", tab);
    fpsGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *fpsLayout = new QVBoxLayout(fpsGroup);
    auto *fpsH = new QHBoxLayout();

    int savedFps = s.value("perf/fps", 120).toInt();
    m_sliderFps = new QSlider(Qt::Horizontal, fpsGroup);
    m_sliderFps->setRange(60, 240);
    m_sliderFps->setSingleStep(15);
    m_sliderFps->setValue(savedFps);

    m_lblFpsValue = new QLabel(QString("%1 FPS").arg(savedFps), fpsGroup);
    m_lblFpsValue->setStyleSheet("color: #00FF66; font-weight: 700; font-family: monospace; min-width: 70px;");
    connect(m_sliderFps, &QSlider::valueChanged, this, [this](int val) {
        m_lblFpsValue->setText(QString("%1 FPS").arg(val));
    });
    fpsH->addWidget(m_sliderFps);
    fpsH->addWidget(m_lblFpsValue);
    fpsLayout->addLayout(fpsH);

    // Wayland V-Sync toggle
    m_checkVsync = new QCheckBox("Synchronize with Wayland Compositor V-Sync", fpsGroup);
    m_checkVsync->setChecked(s.value("perf/vsync", true).toBool());
    fpsLayout->addWidget(m_checkVsync);

    layout->addWidget(fpsGroup);
    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 2. Display
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createDisplayTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    // Display Orientation
    auto *oriGroup = new QGroupBox("Display Orientation", tab);
    oriGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *oriLayout = new QHBoxLayout(oriGroup);

    m_radioLandscape = new QRadioButton("Landscape (Tablet Mode - 16:9)", oriGroup);
    m_radioPortrait = new QRadioButton("Portrait (Phone Mode - 9:16)", oriGroup);
    int savedOri = s.value("display/orientation", 0).toInt();
    if (savedOri == 1) {
        m_radioPortrait->setChecked(true);
    } else {
        m_radioLandscape->setChecked(true);
    }
    oriLayout->addWidget(m_radioLandscape);
    oriLayout->addWidget(m_radioPortrait);
    layout->addWidget(oriGroup);

    // Resolution Presets
    auto *resGroup = new QGroupBox("Display Resolution", tab);
    resGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *resLayout = new QVBoxLayout(resGroup);
    m_comboResolution = new QComboBox(resGroup);
    m_comboResolution->addItem("1920 x 1080 (FHD - Recommended)");
    m_comboResolution->addItem("1600 x 900 (HD+ - Balanced)");
    m_comboResolution->addItem("1280 x 720 (HD - High Performance)");
    m_comboResolution->addItem("960 x 540 (qHD - Maximum Framerates)");
    m_comboResolution->addItem("2560 x 1440 (2K QHD)");
    m_comboResolution->addItem("3840 x 2160 (4K UHD Extreme)");
    m_comboResolution->addItem("Custom (Set via linstack prop wm density)");

    QString savedRes = s.value("display/resolution", "1920 x 1080 (FHD - Recommended)").toString();
    int resIdx = m_comboResolution->findText(savedRes);
    if (resIdx >= 0) m_comboResolution->setCurrentIndex(resIdx);
    resLayout->addWidget(m_comboResolution);
    layout->addWidget(resGroup);

    // Pixel Density (DPI)
    auto *dpiGroup = new QGroupBox("Pixel Density (DPI)", tab);
    dpiGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *dpiLayout = new QVBoxLayout(dpiGroup);
    m_comboDpi = new QComboBox(dpiGroup);
    m_comboDpi->addItem("Ultra High DPI (480 DPI)", 480);
    m_comboDpi->addItem("High DPI (320 DPI) - Crisp fonts && gaming HUD", 320);
    m_comboDpi->addItem("Medium DPI (240 DPI) - Standard Android layout", 240);
    m_comboDpi->addItem("Low DPI (160 DPI) - Maximum screen space", 160);

    int savedDpi = s.value("display/dpi", 320).toInt();
    int dpiIdx = m_comboDpi->findData(savedDpi);
    if (dpiIdx >= 0) m_comboDpi->setCurrentIndex(dpiIdx); else m_comboDpi->setCurrentIndex(1);
    dpiLayout->addWidget(m_comboDpi);
    layout->addWidget(dpiGroup);

    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 3. Graphics & Audio
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createGraphicsAudioTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    auto *gpuGroup = new QGroupBox("Graphics Subsystem & Renderer", tab);
    gpuGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *gpuLayout = new QFormLayout(gpuGroup);

    m_comboRenderer = new QComboBox(gpuGroup);
    m_comboRenderer->addItem("OpenGL ES 3.2 (Mesa DRI3 Direct Rendering)");
    m_comboRenderer->addItem("Vulkan Mesa Open-Source Drivers (RADV / ANV / NVK)");
    m_comboRenderer->setCurrentIndex(s.value("graphics/renderer", 0).toInt());
    gpuLayout->addRow("Graphics Renderer:", m_comboRenderer);

    m_comboAstc = new QComboBox(gpuGroup);
    m_comboAstc->addItem("Hardware Decoding (Direct GPU)");
    m_comboAstc->addItem("Software Decoding (CPU Emulated)");
    m_comboAstc->setCurrentIndex(s.value("graphics/astc", 0).toInt());
    gpuLayout->addRow("ASTC Textures:", m_comboAstc);

    m_comboAudioBackend = new QComboBox(gpuGroup);
    m_comboAudioBackend->addItem("PipeWire Low-Latency Native Client");
    m_comboAudioBackend->addItem("PulseAudio Compatibility Layer");
    m_comboAudioBackend->setCurrentIndex(s.value("graphics/audio", 0).toInt());
    gpuLayout->addRow("Audio Subsystem:", m_comboAudioBackend);

    layout->addWidget(gpuGroup);
    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 4. Devices
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createDevicesTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    // Input Peripherals
    auto *inputGroup = new QGroupBox("Input & Peripherals Configuration", tab);
    inputGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *inputLayout = new QFormLayout(inputGroup);

    m_comboPassthrough = new QComboBox(inputGroup);
    m_comboPassthrough->addItem("Raw Input (Direct libevdev & Wayland Seat Capture)");
    m_comboPassthrough->addItem("Standard Desktop Pointer (Qt Synthesized Events)");
    m_comboPassthrough->setCurrentIndex(s.value("devices/passthrough_mode", 0).toInt());
    inputLayout->addRow("Passthrough Mode:", m_comboPassthrough);

    m_checkGamepadAuto = new QCheckBox("Enable Gamepad / Controller Auto-Detection (Xbox / DualShock / Generic)", inputGroup);
    m_checkGamepadAuto->setChecked(s.value("devices/gamepad_autodetect", true).toBool());
    inputLayout->addRow("", m_checkGamepadAuto);

    layout->addWidget(inputGroup);

    // Audio & Camera Devices
    auto *mediaGroup = new QGroupBox("Audio Input & Camera Devices", tab);
    mediaGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *mediaLayout = new QFormLayout(mediaGroup);

    m_comboMicDevice = new QComboBox(mediaGroup);
    m_comboMicDevice->addItem("Default System Microphone (PipeWire / ALSA)");
    m_comboMicDevice->addItem("Virtual PulseAudio Stream");
    m_comboMicDevice->addItem("Disabled");
    m_comboMicDevice->setCurrentIndex(s.value("devices/mic_device", 0).toInt());
    mediaLayout->addRow("Microphone Device:", m_comboMicDevice);

    m_comboCameraDevice = new QComboBox(mediaGroup);
    m_comboCameraDevice->addItem("Virtual Video Loopback (/dev/video0)");
    m_comboCameraDevice->addItem("Integrated Webcam (V4L2 Direct)");
    m_comboCameraDevice->addItem("Disabled");
    m_comboCameraDevice->setCurrentIndex(s.value("devices/cam_device", 0).toInt());
    mediaLayout->addRow("Camera Device:", m_comboCameraDevice);

    layout->addWidget(mediaGroup);
    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 5. Preferences
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createPreferencesTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    auto *prefGroup = new QGroupBox("General Application Preferences", tab);
    prefGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *prefLayout = new QVBoxLayout(prefGroup);
    prefLayout->setSpacing(10);

    m_checkConfirmExit = new QCheckBox("Confirm before closing LinStack", prefGroup);
    m_checkConfirmExit->setChecked(s.value("preferences/confirm_exit", true).toBool());
    prefLayout->addWidget(m_checkConfirmExit);

    m_checkShowToasts = new QCheckBox("Show toast notifications for container events", prefGroup);
    m_checkShowToasts->setChecked(s.value("preferences/show_toasts", true).toBool());
    prefLayout->addWidget(m_checkShowToasts);

    m_checkRememberGeometry = new QCheckBox("Remember window size and position on exit", prefGroup);
    m_checkRememberGeometry->setChecked(s.value("preferences/remember_geometry", true).toBool());
    prefLayout->addWidget(m_checkRememberGeometry);

    m_checkAutostartSession = new QCheckBox("Auto-start Android session when launching LinStack", prefGroup);
    m_checkAutostartSession->setChecked(s.value("preferences/autostart_session", true).toBool());
    prefLayout->addWidget(m_checkAutostartSession);

    m_checkPauseMinimized = new QCheckBox("Pause rendering when minimized (Battery Saver)", prefGroup);
    m_checkPauseMinimized->setChecked(s.value("preferences/pause_minimized", false).toBool());
    prefLayout->addWidget(m_checkPauseMinimized);

    layout->addWidget(prefGroup);
    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 6. Phone Profile
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createPhoneProfileTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    auto *phoneGroup = new QGroupBox("Device Spoofing (linstack-scripts)", tab);
    phoneGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *phoneLayout = new QVBoxLayout(phoneGroup);

    auto *phoneDesc = new QLabel("Select a high-end mobile fingerprint to unlock 90/120/144 FPS in PUBG Mobile, Free Fire, etc.", phoneGroup);
    phoneDesc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    phoneLayout->addWidget(phoneDesc);

    m_comboPhoneProfile = new QComboBox(phoneGroup);
    m_comboPhoneProfile->addItem("ASUS ROG Phone 5 (144 FPS Profile - ASUS_I005D)");
    m_comboPhoneProfile->addItem("OnePlus 9 Pro (90 FPS Profile - LE2123)");
    m_comboPhoneProfile->addItem("Samsung Galaxy S21 Ultra (120 FPS Profile - SM-G998B)");

    QString savedProfile = s.value("phone/profile", "ASUS ROG Phone 5 (144 FPS Profile - ASUS_I005D)").toString();
    int profIdx = m_comboPhoneProfile->findText(savedProfile);
    if (profIdx >= 0) m_comboPhoneProfile->setCurrentIndex(profIdx);

    phoneLayout->addWidget(m_comboPhoneProfile);
    layout->addWidget(phoneGroup);

    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 7. Shortcuts
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createShortcutsTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *desc = new QLabel("Click any shortcut button below and press your desired key combination (ESC to cancel).", tab);
    desc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    layout->addWidget(desc);

    m_tableShortcuts = new QTableWidget(8, 2, tab);
    m_tableShortcuts->setHorizontalHeaderLabels(QStringList() << "Action" << "Hotkey (Click to Record)");
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tableShortcuts->setColumnWidth(1, 210);
    m_tableShortcuts->verticalHeader()->setVisible(false);
    m_tableShortcuts->verticalHeader()->setDefaultSectionSize(36);
    m_tableShortcuts->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableShortcuts->setFocusPolicy(Qt::NoFocus);
    m_tableShortcuts->setStyleSheet(
        "QTableWidget { background-color: #0D121D; border: 1px solid #1F2937; border-radius: 6px; color: #F9FAFB; gridline-color: #1F2937; }"
        "QHeaderView::section { background-color: #161F30; color: #10B981; border: none; padding: 6px; font-weight: 700; }"
    );

    struct ShortcutEntry { QString keyId; QString action; QString defaultHotkey; };
    ShortcutEntry entries[] = {
        {"fullscreen",       "Toggle Fullscreen",          "F11"},
        {"keymapper_toggle", "Enable / Disable Keymapper", "Ctrl+Shift+M"},
        {"keymapper",        "Keymapper Editor Mode",      "Ctrl+Shift+K"},
        {"aim_lock",         "Lock / Free Mouse Cursor",   "Ctrl+Alt"},
        {"eco_mode",         "Toggle Eco Mode (1 FPS)",    "Ctrl+Shift+E"},
        {"screenshot",       "Take Screenshot",            "Ctrl+Shift+S"},
        {"volume",           "Volume Up / Down",           "Ctrl+Up"},
        {"boss_key",         "Boss Key (Minimize/Hide)",   "Ctrl+Shift+H"}
    };

    QSettings settings("LinStack", "LinStack");

    for (int i = 0; i < 8; ++i) {
        auto *itAction = new QTableWidgetItem(entries[i].action);
        itAction->setFlags(itAction->flags() & ~Qt::ItemIsEditable);
        m_tableShortcuts->setItem(i, 0, itAction);

        QString currentSeq = settings.value("shortcuts/" + entries[i].keyId, entries[i].defaultHotkey).toString();
        auto *captureBtn = new KeyCaptureButton(entries[i].keyId, currentSeq, m_tableShortcuts);

        auto *cellWidget = new QWidget(m_tableShortcuts);
        auto *cellLayout = new QHBoxLayout(cellWidget);
        cellLayout->setContentsMargins(6, 3, 6, 3);
        cellLayout->addWidget(captureBtn);
        m_tableShortcuts->setCellWidget(i, 1, cellWidget);
    }

    layout->addWidget(m_tableShortcuts);
    return tab;
}

// ----------------------------------------------------------------------------
// 8. Shared Storage
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createSharedStorageTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    auto *group = new QGroupBox("Linux <-> Android Shared Folder", tab);
    group->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *gLayout = new QVBoxLayout(group);
    gLayout->setSpacing(10);

    QSettings settings("LinStack", "LinStack");
    bool enabled = settings.value("storage/shared_enabled", false).toBool();
    m_checkSharedEnabled = new QCheckBox("Enable Shared Folder between Linux and Android", group);
    m_checkSharedEnabled->setChecked(enabled);
    m_checkSharedEnabled->setStyleSheet("QCheckBox { color: #F9FAFB; font-weight: 700; font-size: 12px; }");
    gLayout->addWidget(m_checkSharedEnabled);

    auto *desc = new QLabel("Shares the selected Linux folder with Android (/sdcard/<FolderName>). Default path is created automatically when enabled.", group);
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    gLayout->addWidget(desc);

    QString defDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defDir.isEmpty()) defDir = QDir::homePath() + "/Downloads";
    defDir += "/LinStack Shared";
    QString savedPath = settings.value("storage/shared_linux_path", defDir).toString().trimmed();
    if (savedPath.isEmpty()) savedPath = defDir;

    auto *row = new QHBoxLayout();
    m_editSharedPath = new QLineEdit(savedPath, group);
    m_editSharedPath->setReadOnly(true);
    m_editSharedPath->setEnabled(enabled);
    m_editSharedPath->setStyleSheet("QLineEdit { background: #0D121D; border: 1px solid #1F2937; border-radius: 5px; color: #F9FAFB; padding: 6px 10px; }");
    row->addWidget(m_editSharedPath, 1);

    auto *btnBrowse = new QPushButton("Choose Folder...", group);
    btnBrowse->setEnabled(enabled);
    btnBrowse->setStyleSheet("QPushButton { background: #161F30; border: 1px solid #10B981; color: #00FF66; font-weight: 600; padding: 6px 12px; border-radius: 5px; }");
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Linux Shared Folder", m_editSharedPath->text());
        if (!dir.isEmpty()) m_editSharedPath->setText(dir);
    });
    row->addWidget(btnBrowse);
    gLayout->addLayout(row);

    connect(m_checkSharedEnabled, &QCheckBox::toggled, this, [this, btnBrowse](bool checked) {
        m_editSharedPath->setEnabled(checked);
        btnBrowse->setEnabled(checked);
    });

    layout->addWidget(group);
    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// 9. Advanced
// ----------------------------------------------------------------------------
QWidget *SettingsDialog::createAdvancedTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QSettings s("LinStack", "LinStack");

    // ADB Network Bridge (Unchecked / false by default)
    auto *adbGroup = new QGroupBox("Android Debug Bridge (ADB)", tab);
    adbGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *adbLayout = new QVBoxLayout(adbGroup);
    adbLayout->setSpacing(8);

    m_checkAdbBridge = new QCheckBox("Enable ADB Debugging (127.0.0.1:5555)", adbGroup);
    bool adbDefault = s.value("advanced/adb_bridge_enabled", false).toBool();
    m_checkAdbBridge->setChecked(adbDefault);
    m_checkAdbBridge->setStyleSheet("QCheckBox { color: #F9FAFB; font-weight: 700; font-size: 12px; }");
    adbLayout->addWidget(m_checkAdbBridge);

    auto *adbDesc = new QLabel("Starts the localhost ADB socket daemon to allow adb shell, apk installation, and external developer debugging. Disabled by default for security.", adbGroup);
    adbDesc->setWordWrap(true);
    adbDesc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    adbLayout->addWidget(adbDesc);

    layout->addWidget(adbGroup);

    // Advanced Container & Rendering Options
    auto *containerGroup = new QGroupBox("Advanced Container & Rendering Pipeline", tab);
    containerGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *containerLayout = new QVBoxLayout(containerGroup);
    containerLayout->setSpacing(8);

    m_checkSwiftShader = new QCheckBox("Force Software Rendering fallback (SwiftShader / llvmpipe)", containerGroup);
    m_checkSwiftShader->setChecked(s.value("advanced/swiftshader", false).toBool());
    containerLayout->addWidget(m_checkSwiftShader);

    m_checkDirectDmaBuf = new QCheckBox("Enable Zero-Copy DMA-BUF Direct Texture Streaming", containerGroup);
    m_checkDirectDmaBuf->setChecked(s.value("advanced/direct_dmabuf", true).toBool());
    containerLayout->addWidget(m_checkDirectDmaBuf);

    layout->addWidget(containerGroup);

    // Diagnostics & Runtime Logs
    auto *logsGroup = new QGroupBox("Diagnostics & Runtime Logs", tab);
    logsGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *logsLayout = new QVBoxLayout(logsGroup);
    logsLayout->setSpacing(10);

    auto *logsDesc = new QLabel("Inspect diagnostic reports, container initialization events, and Wayland compositor traces.", logsGroup);
    logsDesc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    logsLayout->addWidget(logsDesc);

    auto *logBtnRow = new QHBoxLayout();
    m_btnOpenLogs = new QPushButton("📄 Show / Open Runtime Logs", logsGroup);
    m_btnOpenLogs->setStyleSheet("QPushButton { background: #161F30; border: 1px solid #10B981; color: #00FF66; font-weight: 600; padding: 6px 14px; border-radius: 5px; } QPushButton:hover { background: rgba(16, 185, 129, 0.2); }");
    connect(m_btnOpenLogs, &QPushButton::clicked, this, [this]() {
        QString logPath = Logger::instance().logFilePath();
        if (QFile::exists(logPath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
        } else {
            QMessageBox::information(this, "Runtime Logs", QString("Log file not found at:\n%1").arg(logPath));
        }
    });
    logBtnRow->addWidget(m_btnOpenLogs);

    m_btnOpenLogDir = new QPushButton("📁 Open Log Directory", logsGroup);
    m_btnOpenLogDir->setStyleSheet("QPushButton { background: #111827; border: 1px solid #1F2937; color: #9CA3AF; font-weight: 600; padding: 6px 14px; border-radius: 5px; } QPushButton:hover { color: #F9FAFB; border-color: #374151; }");
    connect(m_btnOpenLogDir, &QPushButton::clicked, this, []() {
        QString logPath = Logger::instance().logFilePath();
        QString logDir = QFileInfo(logPath).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
    });
    logBtnRow->addWidget(m_btnOpenLogDir);
    logBtnRow->addStretch(1);

    logsLayout->addLayout(logBtnRow);
    layout->addWidget(logsGroup);

    layout->addStretch(1);
    return tab;
}

// ----------------------------------------------------------------------------
// Getters
// ----------------------------------------------------------------------------
int SettingsDialog::targetFps() const
{
    return m_sliderFps ? m_sliderFps->value() : 120;
}

QString SettingsDialog::resolution() const
{
    return m_comboResolution ? m_comboResolution->currentText() : QString("1920 x 1080 (FHD - Recommended)");
}

QSize SettingsDialog::baseResolutionSize() const
{
    if (!m_comboResolution) return QSize(1920, 1080);
    QString text = m_comboResolution->currentText();
    QStringList parts = text.split(" x ", Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        bool okW = false, okH = false;
        int w = parts[0].trimmed().toInt(&okW);
        int h = parts[1].trimmed().section(' ', 0, 0).toInt(&okH);
        if (okW && okH && w > 0 && h > 0) {
            return QSize(w, h);
        }
    }
    return QSize(1920, 1080);
}

int SettingsDialog::dpi() const
{
    return m_comboDpi ? m_comboDpi->currentData().toInt() : 320;
}

QString SettingsDialog::deviceProfile() const
{
    return m_comboPhoneProfile ? m_comboPhoneProfile->currentText() : QString();
}

bool SettingsDialog::isAdbBridgeEnabled() const
{
    return m_checkAdbBridge ? m_checkAdbBridge->isChecked() : false;
}

bool SettingsDialog::isVsyncEnabled() const
{
    return m_checkVsync ? m_checkVsync->isChecked() : true;
}

bool SettingsDialog::isSharedStorageEnabled() const
{
    return m_checkSharedEnabled ? m_checkSharedEnabled->isChecked() : false;
}

QString SettingsDialog::sharedStoragePath() const
{
    return m_editSharedPath ? m_editSharedPath->text().trimmed() : QString();
}

bool SettingsDialog::isSwiftShaderEnabled() const
{
    return m_checkSwiftShader ? m_checkSwiftShader->isChecked() : false;
}

bool SettingsDialog::confirmExitEnabled() const
{
    return m_checkConfirmExit ? m_checkConfirmExit->isChecked() : true;
}

void SettingsDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setBrush(Theme::ColorBgPanel);
    p.setPen(QPen(Theme::ColorBorder, 1.5));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::ColorEmerald, 1.0));
    p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 7, 7);
}
