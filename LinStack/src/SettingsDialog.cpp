#include "SettingsDialog.h"
#include "Theme.h"

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
    m_tabs->addTab(createSharedStorageTab(), "Shared Storage");
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
        QSettings("LinStack", "LinStack").setValue("storage/shared_enabled", isSharedStorageEnabled());
        QSettings("LinStack", "LinStack").setValue("storage/shared_linux_path", sharedStoragePath());
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

    auto *desc = new QLabel("Click any shortcut button below and press your desired key combination (ESC to cancel).", tab);
    desc->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    layout->addWidget(desc);

    m_tableShortcuts = new QTableWidget(8, 2, tab);
    m_tableShortcuts->setHorizontalHeaderLabels(QStringList() << "Action" << "Hotkey (Click to Record)");
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tableShortcuts->setColumnWidth(1, 210);
    m_tableShortcuts->verticalHeader()->setVisible(false);
    m_tableShortcuts->verticalHeader()->setDefaultSectionSize(38);
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

    // كلاس زر مخصص للاستماع المباشر للكيبورد عند النقر عليه
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

            // تجاهل الضغط على مفاتيح التعديل بمفردها (Ctrl, Shift, Alt, Super)
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
            setText("⌨ Press shortcut keys...");
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
bool SettingsDialog::isSharedStorageEnabled() const { return m_checkSharedEnabled ? m_checkSharedEnabled->isChecked() : false; }
QString SettingsDialog::sharedStoragePath() const { return m_editSharedPath ? m_editSharedPath->text().trimmed() : QString(); }

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
