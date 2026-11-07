#include "SideToolBar.h"
#include "Theme.h"

#include <QPainter>

SideToolBar::SideToolBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(46);
    setupUi();
}

QPushButton *SideToolBar::createToolButton(const QString &tooltip, Theme::IconType iconType, const char *slot)
{
    auto *btn = new QPushButton(this);
    btn->setFixedSize(36, 34);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setIcon(Theme::createIcon(iconType, Theme::ColorTextMuted, 18));
    btn->setIconSize(QSize(18, 18));
    btn->setToolTip(tooltip);
    btn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 6px;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: #161F30;"
        "  border: 1px solid rgba(16, 185, 129, 0.5);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(16, 185, 129, 0.25);"
        "}"
    );

    if (slot) {
        connect(btn, SIGNAL(clicked()), this, slot);
    }
    return btn;
}

void SideToolBar::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 6);
    layout->setSpacing(3);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // 1. Fullscreen
    m_btnFullscreen = createToolButton("Fullscreen (F11)", Theme::IconType::Fullscreen, SIGNAL(fullscreenClicked()));
    layout->addWidget(m_btnFullscreen);

    // 2. Volume Up
    m_btnVolumeUp = createToolButton("Volume Up (Ctrl+Up)", Theme::IconType::VolumeUp, SIGNAL(volumeUpClicked()));
    layout->addWidget(m_btnVolumeUp);

    // 3. Volume Down
    m_btnVolumeDown = createToolButton("Volume Down (Ctrl+Down)", Theme::IconType::VolumeDown, SIGNAL(volumeDownClicked()));
    layout->addWidget(m_btnVolumeDown);

    // 4. Keymapper Toggle (Ctrl+Shift+K)
    m_btnKeymapper = createToolButton("Keymapper Overlay (Ctrl+Shift+K)", Theme::IconType::Keymapper);
    connect(m_btnKeymapper, &QPushButton::clicked, this, [this]() {
        setKeymapperActive(!m_keymapperActive);
        emit keymapperToggled(m_keymapperActive);
    });
    layout->addWidget(m_btnKeymapper);

    // 5. Macro
    m_btnMacro = createToolButton("Macro Recorder / Script Automation", Theme::IconType::Macro, SIGNAL(macroClicked()));
    layout->addWidget(m_btnMacro);

    // 6. Screen Rotation
    m_btnRotate = createToolButton("Screen Rotation: Landscape / Portrait", Theme::IconType::Rotate, SIGNAL(rotateClicked()));
    layout->addWidget(m_btnRotate);

    // 7. Eco Mode (Ctrl+Shift+E)
    m_btnEcoMode = createToolButton("Eco Mode: Lock 1 FPS in Background (Ctrl+Shift+E)", Theme::IconType::EcoMode);
    connect(m_btnEcoMode, &QPushButton::clicked, this, [this]() {
        setEcoModeActive(!m_ecoModeActive);
        emit ecoModeToggled(m_ecoModeActive);
    });
    layout->addWidget(m_btnEcoMode);

    // 8. APK Drop & Install (Ctrl+Shift+B)
    m_btnInstallApk = createToolButton("Install APK (Ctrl+Shift+B)", Theme::IconType::InstallApk, SIGNAL(installApkClicked()));
    layout->addWidget(m_btnInstallApk);

    // 9. Screenshot (Ctrl+Shift+S)
    m_btnScreenshot = createToolButton("Take Screenshot (Ctrl+Shift+S)", Theme::IconType::Screenshot, SIGNAL(screenshotClicked()));
    layout->addWidget(m_btnScreenshot);

    // 10. File Manager
    m_btnFileManager = createToolButton("Android File Manager & Shared Storage", Theme::IconType::FileManager, SIGNAL(fileManagerClicked()));
    layout->addWidget(m_btnFileManager);

    // 11. Trim Memory (RAM Trimmer)
    m_btnRamTrim = createToolButton("Trim Memory: Free Cached RAM (madvise)", Theme::IconType::RamTrim, SIGNAL(ramTrimClicked()));
    layout->addWidget(m_btnRamTrim);

    // 12. Multi-Instance Manager
    m_btnMultiInstance = createToolButton("Multi-Instance Manager (LXC Containers)", Theme::IconType::MultiInstance, SIGNAL(multiInstanceClicked()));
    layout->addWidget(m_btnMultiInstance);

    // 13. Settings (Gear Icon)
    m_btnSettings = createToolButton("Settings (Ctrl+Shift+I)", Theme::IconType::Settings, SIGNAL(settingsClicked()));
    layout->addWidget(m_btnSettings);

    layout->addStretch(1);

    // Separator line before Android Navigation
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #1F2937; margin: 2px 4px;");
    layout->addWidget(sep);

    // 14. Back (◁)
    m_btnBack = createToolButton("Android Back (◁)", Theme::IconType::AndroidBackTriangle, SIGNAL(androidBackClicked()));
    layout->addWidget(m_btnBack);

    // 15. Home (◯)
    m_btnHome = createToolButton("Android Home (◯)", Theme::IconType::AndroidHomeCircle, SIGNAL(androidHomeClicked()));
    layout->addWidget(m_btnHome);

    // Recents (▢)
    m_btnRecents = createToolButton("Android Recent Apps (▢)", Theme::IconType::AndroidRecentSquare, SIGNAL(androidRecentsClicked()));
    layout->addWidget(m_btnRecents);
}

void SideToolBar::setEcoModeActive(bool active)
{
    m_ecoModeActive = active;
    if (m_ecoModeActive) {
        m_btnEcoMode->setIcon(Theme::createIcon(Theme::IconType::EcoMode, Theme::ColorNeon, 18));
        m_btnEcoMode->setStyleSheet(
            "QPushButton {"
            "  background: rgba(0, 255, 102, 0.15);"
            "  border: 1px solid #00FF66;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(0, 255, 102, 0.25);"
            "}"
        );
    } else {
        m_btnEcoMode->setIcon(Theme::createIcon(Theme::IconType::EcoMode, Theme::ColorTextMuted, 18));
        m_btnEcoMode->setStyleSheet(
            "QPushButton {"
            "  background: transparent;"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background: #161F30;"
            "  border: 1px solid rgba(16, 185, 129, 0.5);"
            "}"
        );
    }
}

void SideToolBar::setKeymapperActive(bool active)
{
    m_keymapperActive = active;
    if (m_keymapperActive) {
        m_btnKeymapper->setIcon(Theme::createIcon(Theme::IconType::Keymapper, Theme::ColorEmerald, 18));
        m_btnKeymapper->setStyleSheet(
            "QPushButton {"
            "  background: rgba(16, 185, 129, 0.2);"
            "  border: 1px solid #10B981;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(16, 185, 129, 0.3);"
            "}"
        );
    } else {
        m_btnKeymapper->setIcon(Theme::createIcon(Theme::IconType::Keymapper, Theme::ColorTextMuted, 18));
        m_btnKeymapper->setStyleSheet(
            "QPushButton {"
            "  background: transparent;"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background: #161F30;"
            "  border: 1px solid rgba(16, 185, 129, 0.5);"
            "}"
        );
    }
}

void SideToolBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Theme::ColorBgPanel);

    // Left border
    p.setPen(Theme::ColorBorder);
    p.drawLine(0, 0, 0, height());
}
