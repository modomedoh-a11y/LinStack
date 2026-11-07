#include "SettingsDialog.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPainter>
#include <QSize>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("LinStack 1.0 - Settings");
    setFixedSize(700, 520);
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

    // Tab Widget
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createPerformanceTab(), "Performance");
    m_tabs->addTab(createDisplayTab(), "Display");
    m_tabs->addTab(createGraphicsTab(), "Graphics && Audio");
    m_tabs->addTab(createPhoneProfileTab(), "Phone Profile");
    m_tabs->addTab(createShortcutsTab(), "Shortcuts");
    rootLayout->addWidget(m_tabs);

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
        emit settingsApplied();
        accept();
    });
    btnRow->addWidget(btnSave);

    rootLayout->addLayout(btnRow);
}

QWidget *SettingsDialog::createPerformanceTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    // Performance Mode
    auto *perfGroup = new QGroupBox("Engine Performance Mode", tab);
    perfGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *perfLayout = new QVBoxLayout(perfGroup);

    m_comboPerfMode = new QComboBox(perfGroup);
    m_comboPerfMode->addItem("High Performance Mode (Direct Mesa DRI3 Passthrough)");
    m_comboPerfMode->addItem("Compatibility Mode (Safe Render Fallback)");
    perfLayout->addWidget(m_comboPerfMode);
    layout->addWidget(perfGroup);

    // High Frame Rate Slider (60 to 240 FPS)
    auto *fpsGroup = new QGroupBox("Frame Rate (High FPS && Wayland V-Sync)", tab);
    fpsGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *fpsLayout = new QVBoxLayout(fpsGroup);
    auto *fpsH = new QHBoxLayout();
    m_sliderFps = new QSlider(Qt::Horizontal, fpsGroup);
    m_sliderFps->setRange(60, 240);
    m_sliderFps->setSingleStep(15);
    m_sliderFps->setValue(120);
    m_lblFpsValue = new QLabel("120 FPS", fpsGroup);
    m_lblFpsValue->setStyleSheet("color: #00FF66; font-weight: 700; font-family: monospace; min-width: 70px;");
    connect(m_sliderFps, &QSlider::valueChanged, this, [this](int val) {
        m_lblFpsValue->setText(QString("%1 FPS").arg(val));
    });
    fpsH->addWidget(m_sliderFps);
    fpsH->addWidget(m_lblFpsValue);
    fpsLayout->addLayout(fpsH);

    m_checkVsync = new QCheckBox("Synchronize with Wayland Compositor V-Sync", fpsGroup);
    m_checkVsync->setChecked(true);
    fpsLayout->addWidget(m_checkVsync);

    layout->addWidget(fpsGroup);

    // Memory Trimmer Action
    auto *memGroup = new QGroupBox("Memory Management", tab);
    memGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *memLayout = new QHBoxLayout(memGroup);

    auto *memDesc = new QLabel("Dynamic LXC memory sharing is active with Linux kernel. Click to trim unused cached memory.", memGroup);
    memDesc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    memLayout->addWidget(memDesc, 1);

    m_btnTrimMemory = new QPushButton("🧹 Trim Memory (madvise)", memGroup);
    m_btnTrimMemory->setStyleSheet("QPushButton { background: #161F30; border: 1px solid #10B981; color: #00FF66; font-weight: 600; padding: 6px 12px; }");
    connect(m_btnTrimMemory, &QPushButton::clicked, this, &SettingsDialog::memoryTrimRequested);
    memLayout->addWidget(m_btnTrimMemory);

    layout->addWidget(memGroup);
    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createDisplayTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    // Display Orientation
    auto *oriGroup = new QGroupBox("Display Orientation", tab);
    oriGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *oriLayout = new QHBoxLayout(oriGroup);
    m_radioLandscape = new QRadioButton("Landscape (Tablet Mode - 16:9)", oriGroup);
    m_radioPortrait = new QRadioButton("Portrait (Phone Mode - 9:16)", oriGroup);
    m_radioLandscape->setChecked(true);
    oriLayout->addWidget(m_radioLandscape);
    oriLayout->addWidget(m_radioPortrait);
    layout->addWidget(oriGroup);

    // Resolution Presets (960x540 to 3840x2160)
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
    m_comboDpi->setCurrentIndex(1);
    dpiLayout->addWidget(m_comboDpi);
    layout->addWidget(dpiGroup);

    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createGraphicsTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    auto *gpuGroup = new QGroupBox("Graphics Subsystem && Renderer", tab);
    gpuGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *gpuLayout = new QFormLayout(gpuGroup);

    m_comboRenderer = new QComboBox(gpuGroup);
    m_comboRenderer->addItem("OpenGL ES 3.2 (Mesa DRI3 Direct Rendering)");
    m_comboRenderer->addItem("Vulkan Mesa Open-Source Drivers (RADV / ANV / NVK)");
    gpuLayout->addRow("Renderer:", m_comboRenderer);

    m_comboAstc = new QComboBox(gpuGroup);
    m_comboAstc->addItem("Hardware Decoding (Direct GPU)");
    m_comboAstc->addItem("Software Decoding (CPU Emulated)");
    gpuLayout->addRow("ASTC Textures:", m_comboAstc);

    m_comboAudioBackend = new QComboBox(gpuGroup);
    m_comboAudioBackend->addItem("PipeWire Low-Latency Native Client");
    m_comboAudioBackend->addItem("PulseAudio Compatibility Layer");
    gpuLayout->addRow("Audio Subsystem:", m_comboAudioBackend);

    layout->addWidget(gpuGroup);
    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createPhoneProfileTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    auto *phoneGroup = new QGroupBox("Device Spoofing (linstack-scripts)", tab);
    phoneGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *phoneLayout = new QVBoxLayout(phoneGroup);

    auto *phoneDesc = new QLabel("Select a high-end mobile fingerprint to unlock 90/120/144 FPS in PUBG Mobile, Free Fire, etc.", phoneGroup);
    phoneDesc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    phoneLayout->addWidget(phoneDesc);

    m_comboPhoneProfile = new QComboBox(phoneGroup);
    m_comboPhoneProfile->addItem("ASUS ROG Phone 5 (144 FPS Profile - ASUS_I005D)");
    m_comboPhoneProfile->addItem("OnePlus 8T / 9 Pro (90 FPS Profile - LE2123)");
    m_comboPhoneProfile->addItem("Samsung Galaxy S10 / S21 Ultra (120 FPS Profile - SM-G998B)");
    phoneLayout->addWidget(m_comboPhoneProfile);
    layout->addWidget(phoneGroup);

    auto *adbGroup = new QGroupBox("ADB Network Bridge", tab);
    adbGroup->setStyleSheet("QGroupBox { color: #10B981; font-weight: 700; border: 1px solid #1F2937; border-radius: 6px; margin-top: 6px; padding-top: 14px; }");
    auto *adbLayout = new QVBoxLayout(adbGroup);

    m_checkAdbBridge = new QCheckBox("Enable Localhost ADB Bridge (127.0.0.1:5555)", adbGroup);
    m_checkAdbBridge->setChecked(true);
    adbLayout->addWidget(m_checkAdbBridge);
    layout->addWidget(adbGroup);

    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createShortcutsTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *desc = new QLabel("Hotkey Manager: Customize in-game shortcuts and system controls.", tab);
    desc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    layout->addWidget(desc);

    m_tableShortcuts = new QTableWidget(7, 3, tab);
    m_tableShortcuts->setHorizontalHeaderLabels(QStringList() << "Action" << "Default Hotkey" << "Customizable");
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableShortcuts->verticalHeader()->setVisible(false);
    m_tableShortcuts->setStyleSheet(
        "QTableWidget { background-color: #0D121D; border: 1px solid #1F2937; border-radius: 6px; color: #F9FAFB; gridline-color: #1F2937; }"
        "QHeaderView::section { background-color: #161F30; color: #10B981; border: none; padding: 6px; font-weight: 700; }"
    );

    struct ShortcutEntry { QString action; QString hotkey; };
    ShortcutEntry entries[] = {
        {"Toggle Fullscreen", "F11"},
        {"Keymapper Overlay / Editor", "Ctrl + Shift + K"},
        {"Lock / Free Mouse Cursor", "Ctrl + Alt"},
        {"Toggle Eco Mode (1 FPS)", "Ctrl + Shift + E"},
        {"Take Screenshot", "Ctrl + Shift + S"},
        {"Volume Up / Down", "Ctrl + Up / Down"},
        {"Boss Key (Minimize/Hide)", "Ctrl + Shift + H"}
    };

    for (int i = 0; i < 7; ++i) {
        auto *itAction = new QTableWidgetItem(entries[i].action);
        auto *itHotkey = new QTableWidgetItem(entries[i].hotkey);
        itHotkey->setTextAlignment(Qt::AlignCenter);
        itHotkey->setForeground(Theme::ColorNeon);
        auto *itMod = new QTableWidgetItem("Yes");
        itMod->setTextAlignment(Qt::AlignCenter);
        itMod->setForeground(Theme::ColorMint);

        m_tableShortcuts->setItem(i, 0, itAction);
        m_tableShortcuts->setItem(i, 1, itHotkey);
        m_tableShortcuts->setItem(i, 2, itMod);
    }

    layout->addWidget(m_tableShortcuts);
    return tab;
}

int SettingsDialog::targetFps() const
{
    return m_sliderFps->value();
}

QString SettingsDialog::resolution() const
{
    return m_comboResolution->currentText();
}

QSize SettingsDialog::baseResolutionSize() const
{
    // The combo items are formatted as "W x H (description)".
    // Extract the first two numbers before any space-x-space or " (".
    QString text = m_comboResolution->currentText();
    // Split on " x " — format is "1920 x 1080 (...)"
    QStringList parts = text.split(" x ", Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        bool okW = false, okH = false;
        int w = parts[0].trimmed().toInt(&okW);
        // The height part may be followed by a space+parenthetical description
        int h = parts[1].trimmed().section(' ', 0, 0).toInt(&okH);
        if (okW && okH && w > 0 && h > 0) {
            return QSize(w, h);
        }
    }
    return QSize(1920, 1080); // Safe fallback
}

int SettingsDialog::dpi() const
{
    return m_comboDpi->currentData().toInt();
}

QString SettingsDialog::deviceProfile() const
{
    return m_comboPhoneProfile->currentText();
}

bool SettingsDialog::isAdbBridgeEnabled() const
{
    return m_checkAdbBridge->isChecked();
}

bool SettingsDialog::isVsyncEnabled() const
{
    return m_checkVsync->isChecked();
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
