#include "KeymapperOverlay.h"
#include "UInputTouchBridge.h"
#include "Theme.h"
#include "InputMapper.h"
#include "AndroidCanvas.h"

#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QSettings>
#include <QCursor>
#include <QCoreApplication>
#include <cmath>

static QString scanCodeToLabel(quint32 scanCode, int fallbackQtKey = 0)
{
    switch (scanCode) {
        case 65: return "SPACE";
        case 25: return "W";
        case 38: return "A";
        case 39: return "S";
        case 40: return "D";
        case 24: return "Q";
        case 26: return "E";
        case 27: return "R";
        case 28: return "T";
        case 52: return "Z";
        case 53: return "X";
        case 54: return "C";
        case 55: return "V";
        case 56: return "B";
        case 33: return "F";
        case 34: return "G";
        case 50: return "SHIFT";
        case 37: return "CTRL";
        case 64: return "ALT";
        case 23: return "TAB";
        case 67: return "F1";
        case 68: return "F2";
        case 69: return "F3";
        case 70: return "F4";
        case 111: return "UP";
        case 116: return "DOWN";
        case 113: return "LEFT";
        case 114: return "RIGHT";
        default: break;
    }
    if (fallbackQtKey != 0) {
        QString s = QKeySequence(fallbackQtKey).toString().toUpper();
        if (!s.isEmpty()) return s;
    }
    return QString("KEY_%1").arg(scanCode);
}

KeymapperOverlay::KeymapperOverlay(QWidget *parent)
: QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_translator = new InputTranslator(nullptr, this);
    connect(m_translator, &InputTranslator::aimLockToggled, this, [this](bool active, QPointF anchorNorm) {
        m_activeAimAnchorNorm = anchorNorm;
        if (active) {
            setCursor(Qt::BlankCursor);
            QPoint localAnchor(qRound(anchorNorm.x() * width()), qRound(anchorNorm.y() * height()));
            QPoint globalAnchor = mapToGlobal(localAnchor);
            QCursor::setPos(globalAnchor);
            m_lastMousePos = globalAnchor;
            m_ignoreNextMouseMove = true;
            emit toastRequested("🎯 Aim Lock Active (Press Aim Key or ESC to unlock)");
        } else {
            setCursor(Qt::ArrowCursor);
            emit toastRequested("Aim Lock Released");
        }
        emit aimLockAnchorChanged(active, anchorNorm);
        update();
    });

    connect(m_translator, &InputTranslator::freeLookToggled, this, [this](bool active, QPointF anchorNorm) {
        m_activeAimAnchorNorm = anchorNorm;
        if (active) {
            setCursor(Qt::BlankCursor);
            QPoint localAnchor(qRound(anchorNorm.x() * width()), qRound(anchorNorm.y() * height()));
            QPoint globalAnchor = mapToGlobal(localAnchor);
            QCursor::setPos(globalAnchor);
            m_lastMousePos = globalAnchor;
            m_ignoreNextMouseMove = true;
        } else {
            setCursor(Qt::ArrowCursor);
        }
        emit aimLockAnchorChanged(active, anchorNorm);
        update();
    });

    connect(m_translator, &InputTranslator::smartCastAimUpdated, this,
            [this](const QString &id, const QPointF &center, const QPointF &target) {
                Q_UNUSED(id);
                m_smartCastCenter = center;
                m_smartCastTarget = target;
                m_showSmartCastLine = true;
                update();
            });

    setupEditorUi();
    setupNodeSettingsPanel();

    QSettings settings("LinStack", "Keymapper");
    QString lastProfile = settings.value("lastProfile", "Default").toString();
    refreshProfileList(lastProfile);
    if (!loadProfileByName(lastProfile)) {
        resetToDefaultBinds();
        saveProfileByName("Default");
        refreshProfileList("Default");
    }

    setMode(Mode::Play);
}

void KeymapperOverlay::setBridge(UInputTouchBridge *bridge)
{
    m_bridge = bridge;
    if (m_translator) {
        m_translator->deleteLater();
    }
    m_translator = new InputTranslator(m_bridge, this);
    m_translator->setScreenGeometry(width(), height());
    connect(m_translator, &InputTranslator::aimLockToggled, this, [this](bool active, QPointF anchorNorm) {
        m_activeAimAnchorNorm = anchorNorm;
        if (active) {
            setCursor(Qt::BlankCursor);
            QPoint localAnchor(qRound(anchorNorm.x() * width()), qRound(anchorNorm.y() * height()));
            QPoint globalAnchor = mapToGlobal(localAnchor);
            QCursor::setPos(globalAnchor);
            m_lastMousePos = globalAnchor;
            m_ignoreNextMouseMove = true;
            emit toastRequested("🎯 Aim Lock Active (Press Aim Key or ESC to unlock)");
        } else {
            setCursor(Qt::ArrowCursor);
            emit toastRequested("Aim Lock Released");
        }
        emit aimLockAnchorChanged(active, anchorNorm);
        update();
    });
    connect(m_translator, &InputTranslator::freeLookToggled, this, [this](bool active, QPointF anchorNorm) {
        m_activeAimAnchorNorm = anchorNorm;
        if (active) {
            setCursor(Qt::BlankCursor);
            QPoint localAnchor(qRound(anchorNorm.x() * width()), qRound(anchorNorm.y() * height()));
            QPoint globalAnchor = mapToGlobal(localAnchor);
            QCursor::setPos(globalAnchor);
            m_lastMousePos = globalAnchor;
            m_ignoreNextMouseMove = true;
        } else {
            setCursor(Qt::ArrowCursor);
        }
        emit aimLockAnchorChanged(active, anchorNorm);
        update();
    });
    connect(m_translator, &InputTranslator::smartCastAimUpdated, this,
            [this](const QString &id, const QPointF &center, const QPointF &target) {
                Q_UNUSED(id);
                m_smartCastCenter = center;
                m_smartCastTarget = target;
                m_showSmartCastLine = true;
                update();
            });
    syncTranslatorNodes();
}

void KeymapperOverlay::setCurrentAppId(const QString &appId)
{
    m_appId = appId.isEmpty() ? "default" : appId;
}

QString KeymapperOverlay::profilesDir() const
{
    QString dir = QString("%1/.config/LinStack/keymaps/profiles").arg(QDir::homePath());
    QDir().mkpath(dir);
    return dir;
}

void KeymapperOverlay::refreshProfileList(const QString &selectProfile)
{
    if (!m_profileCombo) return;
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    QDir dir(profilesDir());
    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
    for (const QString &f : files) {
        m_profileCombo->addItem(QFileInfo(f).completeBaseName());
    }

    if (m_profileCombo->count() == 0) {
        m_profileCombo->addItem("Default");
    }

    if (!selectProfile.isEmpty()) {
        int idx = m_profileCombo->findText(selectProfile);
        if (idx >= 0) {
            m_profileCombo->setCurrentIndex(idx);
            m_currentProfileName = selectProfile;
        }
    }
    m_profileCombo->blockSignals(false);
}

void KeymapperOverlay::createNewProfile()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Keymap Profile",
                                         "Enter profile name (e.g. FreeFire, PUBG, BrawlStars):",
                                         QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    name.replace('/', '_').replace('\\', '_');
    resetToDefaultBinds();
    m_currentProfileName = name;
    saveProfileByName(name);
    refreshProfileList(name);
    emit toastRequested(QString("✔ Created new profile: %1").arg(name));
}

void KeymapperOverlay::duplicateCurrentProfile()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "Duplicate Profile",
                                         "Enter name for the copied profile:",
                                         QLineEdit::Normal, m_currentProfileName + " Copy", &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    name.replace('/', '_').replace('\\', '_');
    m_currentProfileName = name;
    saveProfileByName(name);
    refreshProfileList(name);
    emit toastRequested(QString("✔ Duplicated profile as: %1").arg(name));
}

void KeymapperOverlay::deleteCurrentProfile()
{
    if (m_profileCombo->count() <= 1) {
        emit toastRequested("Cannot delete the last remaining profile.");
        return;
    }
    QString path = QString("%1/%2.json").arg(profilesDir(), m_currentProfileName);
    QFile::remove(path);

    refreshProfileList();
    QString nextName = m_profileCombo->currentText();
    loadProfileByName(nextName);
    emit toastRequested(QString("🗑 Deleted profile. Switched to: %1").arg(nextName));
}

void KeymapperOverlay::exportProfileToFile()
{
    QString defaultFile = QString("%1/%2.json").arg(QDir::homePath(), m_currentProfileName);
    QString outPath = QFileDialog::getSaveFileName(this, "Export Keymap Profile", defaultFile, "JSON Keymap (*.json)");
    if (outPath.isEmpty()) return;
    if (!outPath.endsWith(".json", Qt::CaseInsensitive)) outPath += ".json";

    saveProfileByName(m_currentProfileName);
    QString srcPath = QString("%1/%2.json").arg(profilesDir(), m_currentProfileName);
    QFile::remove(outPath);
    if (QFile::copy(srcPath, outPath)) {
        emit toastRequested(QString("📤 Exported profile to: %1").arg(QFileInfo(outPath).fileName()));
    } else {
        emit toastRequested("❌ Failed to export profile file.");
    }
}

void KeymapperOverlay::importProfileFromFile()
{
    QString inPath = QFileDialog::getOpenFileName(this, "Import Keymap Profile", QDir::homePath(), "JSON Keymap (*.json)");
    if (inPath.isEmpty()) return;

    QString importedName = QFileInfo(inPath).completeBaseName();
    QString destPath = QString("%1/%2.json").arg(profilesDir(), importedName);
    QFile::remove(destPath);
    if (QFile::copy(inPath, destPath)) {
        refreshProfileList(importedName);
        loadProfileByName(importedName);
        emit toastRequested(QString("📥 Imported and loaded profile: %1").arg(importedName));
    } else {
        emit toastRequested("❌ Failed to import profile file.");
    }
}

void KeymapperOverlay::setupEditorUi()
{
    const QString btnSS =
    "QPushButton {"
    "  background-color: #161F30;"
    "  color: #E5E7EB;"
    "  border: 1px solid #1F2937;"
    "  border-radius: 10px;"
    "  padding: 3px 8px;"
    "  font-size: 10px;"
    "  font-weight: 600;"
    "}"
    "QPushButton:hover {"
    "  background-color: #1E293B;"
    "  border-color: #00FF66;"
    "  color: #00FF66;"
    "}"
    "QPushButton[primary='true'] {"
    "  background-color: #10B981;"
    "  color: #0B0F19;"
    "  font-weight: 700;"
    "}"
    "QPushButton[primary='true']:hover {"
    "  background-color: #00FF66;"
    "}";

    m_editorBar = new QWidget(this);
    m_editorBar->setFixedHeight(44);
    m_editorBar->setStyleSheet(
        "QWidget {"
        "  background-color: rgba(17,24,39,235);"
        "  border: 1px solid #10B981;"
        "  border-radius: 22px;"
        "}"
        + btnSS +
        "QComboBox {"
        "  background: #161F30; color: #00FF66; border: 1px solid #1F2937;"
        "  border-radius: 8px; padding: 2px 8px; font-size: 10px; font-weight: 700;"
        "}"
        "QComboBox:hover { border-color: #10B981; }"
        "QComboBox QAbstractItemView { background: #111827; color: #E5E7EB; selection-background-color: #10B981; selection-color: #0B0F19; }"
        "QSlider::groove:horizontal { height: 3px; background: #1F2937; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #10B981; width: 12px; height: 12px; border-radius: 6px; margin: -5px 0; }"
        "QSlider::sub-page:horizontal { background: #10B981; border-radius: 2px; }"
    );

    auto *barLayout = new QHBoxLayout(m_editorBar);
    barLayout->setContentsMargins(12, 4, 12, 4);
    barLayout->setSpacing(4);

    auto *profileLabel = new QLabel("Profile:", m_editorBar);
    profileLabel->setStyleSheet("color: #9CA3AF; font-size: 9px; font-weight: 600; border: none; background: transparent;");
    barLayout->addWidget(profileLabel);

    m_profileCombo = new QComboBox(m_editorBar);
    m_profileCombo->setFixedWidth(115);
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, [this](const QString &profile) {
        if (!profile.isEmpty() && profile != m_currentProfileName) {
            saveProfileByName(m_currentProfileName);
            loadProfileByName(profile);
            emit toastRequested(QString("Loaded profile: %1").arg(profile));
        }
    });
    barLayout->addWidget(m_profileCombo);

    m_btnNewProfile = new QPushButton("+ New", m_editorBar);
    m_btnNewProfile->setToolTip("Create new named profile");
    connect(m_btnNewProfile, &QPushButton::clicked, this, &KeymapperOverlay::createNewProfile);
    barLayout->addWidget(m_btnNewProfile);

    m_btnDupProfile = new QPushButton("Copy", m_editorBar);
    m_btnDupProfile->setToolTip("Duplicate current profile");
    connect(m_btnDupProfile, &QPushButton::clicked, this, &KeymapperOverlay::duplicateCurrentProfile);
    barLayout->addWidget(m_btnDupProfile);

    m_btnDelProfile = new QPushButton("Del", m_editorBar);
    m_btnDelProfile->setToolTip("Delete current profile");
    connect(m_btnDelProfile, &QPushButton::clicked, this, &KeymapperOverlay::deleteCurrentProfile);
    barLayout->addWidget(m_btnDelProfile);

    auto *sep0 = new QFrame(m_editorBar);
    sep0->setFrameShape(QFrame::VLine);
    sep0->setStyleSheet("color: #1F2937; margin: 4px 2px;");
    barLayout->addWidget(sep0);

    m_btnImport = new QPushButton("📥 Import", m_editorBar);
    connect(m_btnImport, &QPushButton::clicked, this, &KeymapperOverlay::importProfileFromFile);
    barLayout->addWidget(m_btnImport);

    m_btnExport = new QPushButton("📤 Export", m_editorBar);
    connect(m_btnExport, &QPushButton::clicked, this, &KeymapperOverlay::exportProfileToFile);
    barLayout->addWidget(m_btnExport);

    auto *sep1 = new QFrame(m_editorBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #1F2937; margin: 4px 2px;");
    barLayout->addWidget(sep1);

    m_opacitySlider = new QSlider(Qt::Horizontal, m_editorBar);
    m_opacitySlider->setRange(20, 100);
    m_opacitySlider->setValue(static_cast<int>(m_opacity * 100));
    m_opacitySlider->setFixedWidth(95);
    m_opacitySlider->setToolTip("HUD Opacity");
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
        setOverlayOpacity(v / 100.0);
    });
    barLayout->addWidget(m_opacitySlider);

    m_btnClearAll = new QPushButton("🗑 Clear", m_editorBar);
    connect(m_btnClearAll, &QPushButton::clicked, this, [this]() {
        m_nodes.clear();
        m_selectedNodeIndex = -1;
        updateNodeSettingsPanel();
        syncTranslatorNodes();
        update();
    });
    barLayout->addWidget(m_btnClearAll);

    m_btnReset = new QPushButton("↺ Reset", m_editorBar);
    connect(m_btnReset, &QPushButton::clicked, this, &KeymapperOverlay::resetToDefaultBinds);
    barLayout->addWidget(m_btnReset);

    m_btnToggleMode = new QPushButton("✔ Done", m_editorBar);
    m_btnToggleMode->setProperty("primary", true);
    connect(m_btnToggleMode, &QPushButton::clicked, this, [this]() { setMode(Mode::Play); });
    barLayout->addWidget(m_btnToggleMode);

    setupNodeCatalogPanel();
}

void KeymapperOverlay::setupNodeCatalogPanel()
{
    const QString btnSS =
    "QPushButton {"
    "  background-color: #111827;"
    "  color: #E5E7EB;"
    "  border: 1px solid #1F2937;"
    "  border-radius: 8px;"
    "  padding: 6px 8px;"
    "  font-size: 10px;"
    "  font-weight: 600;"
    "  text-align: left;"
    "}"
    "QPushButton:hover {"
    "  background-color: #161F30;"
    "  border-color: #10B981;"
    "  color: #00FF66;"
    "}";

    m_catalogPanel = new QWidget(this);
    m_catalogPanel->setFixedWidth(154);
    m_catalogPanel->setStyleSheet(
        "QWidget {"
        "  background-color: rgba(11,15,25,225);"
        "  border: 1px solid #1F2937;"
        "  border-radius: 10px;"
        "}"
        + btnSS
    );

    auto *panelLayout = new QVBoxLayout(m_catalogPanel);
    panelLayout->setContentsMargins(6, 8, 6, 8);
    panelLayout->setSpacing(5);

    auto *headerLabel = new QLabel("CONTROLS", m_catalogPanel);
    headerLabel->setStyleSheet("color: #10B981; font-size: 9px; font-weight: 700; letter-spacing: 1px; background: transparent; border: none;");
    panelLayout->addWidget(headerLabel);

    auto *sep = new QFrame(m_catalogPanel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #1F2937; background: #1F2937; border: none; max-height: 1px; margin: 2px 0;");
    panelLayout->addWidget(sep);
    // 1. Tap Spot + علامة تنبيه عند الـ Hover
    m_btnAddTap = new QPushButton("👆  Tap Spot          ⓘ", m_catalogPanel);
    m_btnAddTap->setToolTip("Tip: Click anywhere on an empty spot on the screen to instantly create a Tap button!");
    connect(m_btnAddTap, &QPushButton::clicked, this, [this]() {
        KeyBindingNode tap;
        tap.id              = QString("tap_%1").arg(m_nodes.size() + 1);
        tap.type            = KeyNodeType::Tap;
        tap.label           = "SPACE";
        tap.action          = "Tap";
        tap.primaryKey      = Qt::Key_Space;
        tap.primaryScanCode = 65;
        tap.relX            = 0.85; tap.relY = 0.75;
        tap.radius          = 24.0;
        addNode(tap);
    });
    panelLayout->addWidget(m_btnAddTap);

    // 2. Burst Tap
    m_btnAddBurstTap = new QPushButton("🔥  Burst Tap", m_catalogPanel);
    m_btnAddBurstTap->setToolTip("ضغط متتالي سريع (من 1 إلى 100 ضغطة/ثانية)");
    connect(m_btnAddBurstTap, &QPushButton::clicked, this, [this]() {
        KeyBindingNode bt;
        bt.id              = QString("burst_%1").arg(m_nodes.size() + 1);
        bt.type            = KeyNodeType::BurstTap;
        bt.label           = "T";
        bt.action          = "Burst";
        bt.primaryKey      = Qt::Key_T;
        bt.primaryScanCode = 28;
        bt.relX            = 0.78; bt.relY = 0.62;
        bt.radius          = 24.0;
        bt.burstHz         = 10;
        addNode(bt);
    });
    panelLayout->addWidget(m_btnAddBurstTap);

    // 3. D-Pad
    m_btnAddDpad = new QPushButton("🕹  D-Pad (WASD)", m_catalogPanel);
    connect(m_btnAddDpad, &QPushButton::clicked, this, [this]() {
        KeyBindingNode dpad;
        dpad.id        = "dpad";
        dpad.type      = KeyNodeType::Dpad;
        dpad.label     = "WASD";
        dpad.action    = "Movement";
        dpad.relX      = 0.18; dpad.relY = 0.70;
        dpad.radius    = 60.0;
        dpad.scanUp    = 25;
        dpad.scanLeft  = 38;
        dpad.scanDown  = 39;
        dpad.scanRight = 40;
        addNode(dpad);
    });
    panelLayout->addWidget(m_btnAddDpad);

    // 4. Aim Lock
    m_btnAddAim = new QPushButton("🎯  Aim Lock", m_catalogPanel);
    connect(m_btnAddAim, &QPushButton::clicked, this, [this]() {
        int count = 0;
        for (const auto &n : m_nodes) if (n.type == KeyNodeType::AimShoot) count++;
        KeyBindingNode aim;
        aim.id              = QString("aim_lock_%1").arg(count + 1);
        aim.type            = KeyNodeType::AimShoot;
        aim.primaryScanCode = (count == 0) ? 67 : 37;
        aim.primaryKey      = (count == 0) ? Qt::Key_F1 : Qt::Key_Control;
        aim.label           = scanCodeToLabel(aim.primaryScanCode, aim.primaryKey);
        aim.action          = "Aim Lock";
        aim.relX            = 0.55 + (count * 0.08);
        aim.relY            = 0.50;
        aim.radius          = 28.0;
        aim.sensitivityX    = 1.2;
        aim.sensitivityY    = 1.2;
        aim.acceleration    = false;
        aim.enableFireButton = (count == 0);
        aim.fireRelX        = 0.82;
        aim.fireRelY        = 0.64;
        addNode(aim);
    });
    panelLayout->addWidget(m_btnAddAim);

    // 5. Free Look (يعمل أثناء الضغط المستمر فقط)
    m_btnAddFreeLook = new QPushButton("👁  Free Look", m_catalogPanel);
    m_btnAddFreeLook->setToolTip("قفل المؤشر وتدوير الكاميرا 360° أثناء الضغط المستمر على الزر فقط");
    connect(m_btnAddFreeLook, &QPushButton::clicked, this, [this]() {
        KeyBindingNode fl;
        fl.id              = QString("freelook_%1").arg(m_nodes.size() + 1);
        fl.type            = KeyNodeType::FreeLook;
        fl.label           = "ALT";
        fl.action          = "Free Look";
        fl.primaryKey      = Qt::Key_Alt;
        fl.primaryScanCode = 64;
        fl.relX            = 0.60; fl.relY = 0.45;
        fl.radius          = 26.0;
        fl.sensitivityX    = 1.2; fl.sensitivityY = 1.2;
        addNode(fl);
    });
    panelLayout->addWidget(m_btnAddFreeLook);

    // 6. Swipe + علامة تنبيه عند الـ Hover
    m_btnAddSwipe = new QPushButton("💨  Swipe              ⓘ", m_catalogPanel);
    m_btnAddSwipe->setToolTip("Tip: Click and drag anywhere on an empty spot to draw a custom green Swipe path!");
    connect(m_btnAddSwipe, &QPushButton::clicked, this, [this]() {
        KeyBindingNode sw;
        sw.id              = QString("swipe_%1").arg(m_nodes.size() + 1);
        sw.type            = KeyNodeType::Swipe;
        sw.label           = "UP";
        sw.action          = "Swipe";
        sw.primaryKey      = Qt::Key_Up;
        sw.primaryScanCode = 111;
        sw.direction       = "up";
        sw.relX            = 0.50; sw.relY = 0.75;
        sw.endRelX         = 0.50; sw.endRelY = 0.40;
        sw.durationMs      = 150;
        sw.radius          = 22.0;
        addNode(sw);
    });
    panelLayout->addWidget(m_btnAddSwipe);

    // 7. Smart Cast
    m_btnAddSmartCast = new QPushButton("⚡  Smart Cast", m_catalogPanel);
    m_btnAddSmartCast->setToolTip("توجيه المهارات نحو مؤشر الماوس أثناء الضغط وإطلاقها عند الإفلات");
    connect(m_btnAddSmartCast, &QPushButton::clicked, this, [this]() {
        KeyBindingNode sc;
        sc.id              = QString("smart_cast_%1").arg(m_nodes.size() + 1);
        sc.type            = KeyNodeType::SmartCast;
        sc.label           = "Q";
        sc.action          = "Smart Cast";
        sc.primaryKey      = Qt::Key_Q;
        sc.primaryScanCode = 24;
        sc.relX            = 0.75; sc.relY = 0.75;
        sc.radius          = 28.0;
        sc.castRadius      = 160.0;
        addNode(sc);
    });
    panelLayout->addWidget(m_btnAddSmartCast);

    panelLayout->addStretch(1);
}

void KeymapperOverlay::setupNodeSettingsPanel()
{
    m_nodeSettingsPanel = new QWidget(this);
    m_nodeSettingsPanel->setFixedWidth(290);
    m_nodeSettingsPanel->setCursor(Qt::OpenHandCursor);
    m_nodeSettingsPanel->setStyleSheet(
        "QWidget {"
        "  background-color: rgba(11, 15, 25, 245);"
        "  border: 1px solid #00FF66;"
        "  border-radius: 10px;"
        "}"
        "QLabel { color: #E5E7EB; font-size: 10px; font-weight: 600; border: none; background: transparent; }"
        "QDoubleSpinBox, QSpinBox {"
        "  background: #161F30; color: #00FF66; border: 1px solid #1F2937; border-radius: 4px; padding: 2px 6px; font-weight: 700;"
        "}"
        "QSlider::groove:horizontal { height: 4px; background: #1F2937; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00FF66; width: 12px; height: 12px; border-radius: 6px; margin: -4px 0; }"
        "QSlider::sub-page:horizontal { background: #10B981; border-radius: 2px; }"
        "QCheckBox { color: #E5E7EB; font-size: 10px; font-weight: 600; spacing: 6px; border: none; background: transparent; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; border: 1px solid #10B981; background: #161F30; }"
        "QCheckBox::indicator:checked { background: #00FF66; }"
    );

    auto *mainLay = new QVBoxLayout(m_nodeSettingsPanel);
    mainLay->setContentsMargins(10, 8, 10, 10);
    mainLay->setSpacing(8);

    auto *header = new QHBoxLayout();
    m_settingsTitle = new QLabel("⚙ Control Settings", m_nodeSettingsPanel);
    m_settingsTitle->setStyleSheet("color: #00FF66; font-size: 11px; font-weight: 700; border: none; background: transparent;");
    header->addWidget(m_settingsTitle);

    auto *btnClose = new QPushButton("✕", m_nodeSettingsPanel);
    btnClose->setFixedSize(18, 18);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet("QPushButton { background: transparent; color: #9CA3AF; border: none; font-weight: 700; } QPushButton:hover { color: #EF4444; }");
    connect(btnClose, &QPushButton::clicked, this, [this]() {
        m_settingsOpen = false;
        m_nodeSettingsPanel->hide();
    });
    header->addWidget(btnClose);
    mainLay->addLayout(header);

    m_settingsBody = new QVBoxLayout();
    m_settingsBody->setSpacing(6);
    mainLay->addLayout(m_settingsBody);

    m_nodeSettingsPanel->hide();
}
bool KeymapperOverlay::hasNodeSettings(KeyNodeType type) const
{
    return (type == KeyNodeType::AimShoot ||
            type == KeyNodeType::Dpad ||
            type == KeyNodeType::BurstTap ||
            type == KeyNodeType::Swipe ||
            type == KeyNodeType::FreeLook ||
            type == KeyNodeType::SmartCast);
}

void KeymapperOverlay::updateNodeSettingsPanel()
{
    if (!m_nodeSettingsPanel || !m_settingsBody) return;

    QLayoutItem *item;
    while ((item = m_settingsBody->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    if (m_mode != Mode::Edit || !m_settingsOpen || m_selectedNodeIndex < 0 || m_selectedNodeIndex >= m_nodes.size()) {
        m_nodeSettingsPanel->hide();
        return;
    }

    KeyBindingNode &node = m_nodes[m_selectedNodeIndex];
    if (!hasNodeSettings(node.type)) {
        m_nodeSettingsPanel->hide();
        return;
    }

    if (node.type == KeyNodeType::AimShoot || node.type == KeyNodeType::FreeLook) {
        m_settingsTitle->setText(node.type == KeyNodeType::AimShoot ? "🎯 Aim Lock Settings" : "👁 Free Look Settings");

        auto *rowX = new QHBoxLayout();
        rowX->addWidget(new QLabel("Sensitivity X:", m_nodeSettingsPanel));
        auto *spinX = new QDoubleSpinBox(m_nodeSettingsPanel);
        spinX->setRange(0.1, 10.0);
        spinX->setSingleStep(0.1);
        spinX->setValue(node.sensitivityX);
        connect(spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].sensitivityX = val;
                syncTranslatorNodes();
            }
        });
        rowX->addWidget(spinX);
        m_settingsBody->addLayout(rowX);

        auto *rowY = new QHBoxLayout();
        rowY->addWidget(new QLabel("Sensitivity Y:", m_nodeSettingsPanel));
        auto *spinY = new QDoubleSpinBox(m_nodeSettingsPanel);
        spinY->setRange(0.1, 10.0);
        spinY->setSingleStep(0.1);
        spinY->setValue(node.sensitivityY);
        connect(spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].sensitivityY = val;
                syncTranslatorNodes();
            }
        });
        rowY->addWidget(spinY);
        m_settingsBody->addLayout(rowY);

        auto *chkAccel = new QCheckBox("Mouse Acceleration (Off = 1:1)", m_nodeSettingsPanel);
        chkAccel->setChecked(node.acceleration);
        connect(chkAccel, &QCheckBox::toggled, this, [this](bool checked) {
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].acceleration = checked;
                syncTranslatorNodes();
            }
        });
        m_settingsBody->addWidget(chkAccel);

        if (node.type == KeyNodeType::AimShoot) {
            auto *chkFire = new QCheckBox("Enable LMB Fire Button", m_nodeSettingsPanel);
            chkFire->setChecked(node.enableFireButton);
            connect(chkFire, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                    m_nodes[m_selectedNodeIndex].enableFireButton = checked;
                    syncTranslatorNodes();
                    update();
                }
            });
            m_settingsBody->addWidget(chkFire);
        }

    } else if (node.type == KeyNodeType::Dpad) {
        m_settingsTitle->setText("🕹 D-Pad Settings");

        auto *rowRad = new QHBoxLayout();
        rowRad->addWidget(new QLabel("Size:", m_nodeSettingsPanel));

        auto *sliderRad = new QSlider(Qt::Horizontal, m_nodeSettingsPanel);
        sliderRad->setMinimumWidth(110);
        sliderRad->setRange(30, 180);
        sliderRad->setValue(qRound(node.radius));

        auto *spinRad = new QSpinBox(m_nodeSettingsPanel);
        spinRad->setRange(30, 180);
        spinRad->setSuffix(" px");
        spinRad->setValue(qRound(node.radius));

        connect(sliderRad, &QSlider::valueChanged, this, [this, spinRad](int val) {
            spinRad->blockSignals(true);
            spinRad->setValue(val);
            spinRad->blockSignals(false);
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].radius = val;
                syncTranslatorNodes();
                update();
            }
        });
        connect(spinRad, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, sliderRad](int val) {
            sliderRad->blockSignals(true);
            sliderRad->setValue(val);
            sliderRad->blockSignals(false);
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].radius = val;
                syncTranslatorNodes();
                update();
            }
        });

        rowRad->addWidget(sliderRad);
        rowRad->addWidget(spinRad);
        m_settingsBody->addLayout(rowRad);

    } else if (node.type == KeyNodeType::BurstTap) {
        m_settingsTitle->setText("🔥 Burst Tap Settings");

        auto *rowHz = new QHBoxLayout();
        rowHz->addWidget(new QLabel("Clicks/Sec:", m_nodeSettingsPanel));

        auto *sliderHz = new QSlider(Qt::Horizontal, m_nodeSettingsPanel);
        sliderHz->setRange(1, 100);
        sliderHz->setValue(qBound(1, node.burstHz, 100));

        auto *spinHz = new QSpinBox(m_nodeSettingsPanel);
        spinHz->setRange(1, 100);
        spinHz->setSuffix(" Hz");
        spinHz->setValue(qBound(1, node.burstHz, 100));

        connect(sliderHz, &QSlider::valueChanged, this, [this, spinHz](int val) {
            spinHz->blockSignals(true);
            spinHz->setValue(val);
            spinHz->blockSignals(false);
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].burstHz = val;
                syncTranslatorNodes();
            }
        });
        connect(spinHz, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, sliderHz](int val) {
            sliderHz->blockSignals(true);
            sliderHz->setValue(val);
            sliderHz->blockSignals(false);
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].burstHz = val;
                syncTranslatorNodes();
            }
        });

        rowHz->addWidget(sliderHz);
        rowHz->addWidget(spinHz);
        m_settingsBody->addLayout(rowHz);

    } else if (node.type == KeyNodeType::Swipe) {
        m_settingsTitle->setText("💨 Swipe Settings");

        auto *rowDur = new QHBoxLayout();
        rowDur->addWidget(new QLabel("Duration:", m_nodeSettingsPanel));
        auto *spinDur = new QSpinBox(m_nodeSettingsPanel);
        spinDur->setRange(40, 1500);
        spinDur->setSingleStep(20);
        spinDur->setSuffix(" ms");
        spinDur->setValue(node.durationMs);
        connect(spinDur, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].durationMs = val;
                syncTranslatorNodes();
            }
        });
        rowDur->addWidget(spinDur);
        m_settingsBody->addLayout(rowDur);

    } else if (node.type == KeyNodeType::SmartCast) {
        m_settingsTitle->setText("⚡ Smart Cast Settings");

        auto *rowCast = new QHBoxLayout();
        rowCast->addWidget(new QLabel("Cast Range:", m_nodeSettingsPanel));
        auto *sliderCast = new QSlider(Qt::Horizontal, m_nodeSettingsPanel);
        sliderCast->setRange(60, 400);
        sliderCast->setValue(qRound(node.castRadius));

        auto *lblCast = new QLabel(QString("%1 px").arg(qRound(node.castRadius)), m_nodeSettingsPanel);
        lblCast->setStyleSheet("color: #00FF66; font-family: monospace; min-width: 42px;");

        connect(sliderCast, &QSlider::valueChanged, this, [this, lblCast](int val) {
            if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
                m_nodes[m_selectedNodeIndex].castRadius = val;
                lblCast->setText(QString("%1 px").arg(val));
                syncTranslatorNodes();
                update();
            }
        });
        rowCast->addWidget(sliderCast);
        rowCast->addWidget(lblCast);
        m_settingsBody->addLayout(rowCast);
    }

    m_nodeSettingsPanel->adjustSize();
    int px = qBound(10, qRound(node.relX * width() + node.radius + 14), width() - m_nodeSettingsPanel->width() - 165);
    int py = qBound(70, qRound(node.relY * height() - 30), height() - m_nodeSettingsPanel->height() - 10);
    m_nodeSettingsPanel->move(px, py);
    m_nodeSettingsPanel->show();
    m_nodeSettingsPanel->raise();
}
void KeymapperOverlay::setMode(Mode mode)
{
    Mode prevMode = m_mode;
    m_mode = mode;
    m_selectedNodeIndex = -1;
    m_isDraggingNode = false;
    m_isDraggingSwipeEnd = false;
    m_isDraggingFireBtn = false;
    m_isDraggingSettingsPanel = false;
    m_isDrawingFreehand = false;
    m_freehandPoints.clear();
    m_settingsOpen = false;
    if (m_nodeSettingsPanel) m_nodeSettingsPanel->hide();

    if (m_mode == Mode::Edit) {
        if (m_translator && m_translator->isAimLockActive()) {
            m_translator->setAimLockActive(false, -1);
        }
        m_editorBar->show();
        m_editorBar->raise();
        m_catalogPanel->show();
        m_catalogPanel->raise();
        positionEditorBar();
        setCursor(Qt::ArrowCursor);
        emit toastRequested("⌨ Edit Mode: Click empty space to add Tap, drag to draw Swipe, or click ⚙ for settings.");
    } else {
        m_editorBar->hide();
        m_catalogPanel->hide();
        setCursor(Qt::ArrowCursor);
        m_showSmartCastLine = false;

        if (prevMode == Mode::Edit) {
            saveProfileByName(m_currentProfileName);
            emit toastRequested(QString("✔ Auto-saved profile: %1").arg(m_currentProfileName));
        }
    }

    emit modeChanged(m_mode);
    update();
}

void KeymapperOverlay::setOverlayOpacity(qreal opacity)
{
    m_opacity = qBound(0.1, opacity, 1.0);
    update();
}

void KeymapperOverlay::setInputMapper(InputMapper *mapper)
{
    m_inputMapper = mapper;
    syncTranslatorNodes();
}

void KeymapperOverlay::syncTranslatorNodes()
{
    if (m_translator) {
        m_translator->setNodes(m_nodes);
        m_translator->setScreenGeometry(width(), height());
    }

    if (m_inputMapper) {
        m_inputMapper->clearTapNodes();
        m_inputMapper->clearAimNodes();
        m_inputMapper->clearExtraNodes();

        DpadConfig emptyDp{};
        emptyDp.scanUp = 0;
        emptyDp.scanDown = 0;
        emptyDp.scanLeft = 0;
        emptyDp.scanRight = 0;
        m_inputMapper->setDpad(emptyDp);

        for (const auto &node : m_nodes) {
            if (node.type == KeyNodeType::Tap) {
                KeyNode kn;
                kn.id = node.id.toStdString();
                kn.type = WlrKeyNodeType::Tap;
                kn.scanCode = node.primaryScanCode;
                kn.relX = static_cast<float>(node.relX);
                kn.relY = static_cast<float>(node.relY);
                m_inputMapper->addTapNode(kn);
            } else if (node.type == KeyNodeType::BurstTap) {
                BurstTapConfig bt;
                bt.id = node.id.toStdString();
                bt.scanCode = node.primaryScanCode;
                bt.relX = static_cast<float>(node.relX);
                bt.relY = static_cast<float>(node.relY);
                bt.burstHz = node.burstHz;
                m_inputMapper->addBurstNode(bt);
            } else if (node.type == KeyNodeType::Dpad) {
                DpadConfig dp = m_inputMapper->getDpad();
                dp.centerX = static_cast<float>(node.relX);
                dp.centerY = static_cast<float>(node.relY);
                dp.radius = static_cast<float>(node.radius / 350.0f);
                dp.scanUp = node.scanUp;
                dp.scanDown = node.scanDown;
                dp.scanLeft = node.scanLeft;
                dp.scanRight = node.scanRight;
                m_inputMapper->setDpad(dp);
            } else if (node.type == KeyNodeType::AimShoot) {
                AimLockNodeConfig aim;
                aim.id = node.id.toStdString();
                aim.toggleScanCode = node.primaryScanCode;
                aim.centerX = static_cast<float>(node.relX);
                aim.centerY = static_cast<float>(node.relY);
                aim.fireX = static_cast<float>(node.fireRelX);
                aim.fireY = static_cast<float>(node.fireRelY);
                aim.sensitivityX = static_cast<float>(node.sensitivityX);
                aim.sensitivityY = static_cast<float>(node.sensitivityY);
                aim.acceleration = node.acceleration;
                m_inputMapper->addAimNode(aim);
            } else if (node.type == KeyNodeType::FreeLook) {
                FreeLookConfig fl;
                fl.id = node.id.toStdString();
                fl.holdScanCode = node.primaryScanCode;
                fl.centerX = static_cast<float>(node.relX);
                fl.centerY = static_cast<float>(node.relY);
                fl.sensitivityX = static_cast<float>(node.sensitivityX);
                fl.sensitivityY = static_cast<float>(node.sensitivityY);
                fl.acceleration = node.acceleration;
                m_inputMapper->addFreeLookNode(fl);
            } else if (node.type == KeyNodeType::Swipe) {
                SwipeConfig sw;
                sw.id = node.id.toStdString();
                sw.scanCode = node.primaryScanCode;
                sw.startX = static_cast<float>(node.relX);
                sw.startY = static_cast<float>(node.relY);
                sw.endX = static_cast<float>(node.endRelX);
                sw.endY = static_cast<float>(node.endRelY);
                sw.durationMs = node.durationMs;
                for (const auto &pt : node.pathPoints) {
                    sw.pathPoints.emplace_back(static_cast<float>(pt.x()), static_cast<float>(pt.y()));
                }
                m_inputMapper->addSwipeNode(sw);
            } else if (node.type == KeyNodeType::SmartCast) {
                SmartCastConfig sc;
                sc.id = node.id.toStdString();
                sc.scanCode = node.primaryScanCode;
                sc.centerX = static_cast<float>(node.relX);
                sc.centerY = static_cast<float>(node.relY);
                sc.castRadius = static_cast<float>(node.castRadius);
                m_inputMapper->addSmartCastNode(sc);
            }
        }
    }
}

void KeymapperOverlay::addNode(const KeyBindingNode &node)
{
    if (node.type == KeyNodeType::Dpad) {
        for (int i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i].type == KeyNodeType::Dpad) {
                m_nodes[i] = node;
                m_selectedNodeIndex = i;
                syncTranslatorNodes();
                update();
                return;
            }
        }
    }

    m_nodes.append(node);
    m_selectedNodeIndex = m_nodes.size() - 1;
    syncTranslatorNodes();
    update();
}

void KeymapperOverlay::removeSelectedNode()
{
    if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
        m_nodes.removeAt(m_selectedNodeIndex);
        m_selectedNodeIndex = -1;
        m_settingsOpen = false;
        updateNodeSettingsPanel();
        syncTranslatorNodes();
        update();
    }
}

void KeymapperOverlay::resetToDefaultBinds()
{
    m_nodes.clear();

    KeyBindingNode dpad;
    dpad.id = "dpad"; dpad.type = KeyNodeType::Dpad; dpad.label = "WASD"; dpad.action = "Move";
    dpad.relX = 0.18; dpad.relY = 0.70; dpad.radius = 60.0;
    dpad.scanUp = 25; dpad.scanLeft = 38; dpad.scanDown = 39; dpad.scanRight = 40;
    m_nodes.append(dpad);

    KeyBindingNode aim;
    aim.id = "aim_lock_1"; aim.type = KeyNodeType::AimShoot; aim.label = "F1"; aim.action = "Aim Lock";
    aim.primaryKey = Qt::Key_F1; aim.primaryScanCode = 67;
    aim.relX = 0.55; aim.relY = 0.50; aim.radius = 28.0;
    aim.sensitivityX = 1.2; aim.sensitivityY = 1.2; aim.acceleration = false;
    aim.enableFireButton = true; aim.fireRelX = 0.82; aim.fireRelY = 0.64;
    m_nodes.append(aim);

    KeyBindingNode jump{"tap_jump", KeyNodeType::Tap, "SPACE", "Jump", Qt::Key_Space, 65, 0.88, 0.78, 24.0};
    KeyBindingNode crouch{"tap_crouch", KeyNodeType::Tap, "C", "Crouch", Qt::Key_C, 54, 0.82, 0.86, 22.0};
    KeyBindingNode reload{"tap_reload", KeyNodeType::Tap, "R", "Reload", Qt::Key_R, 27, 0.88, 0.52, 22.0};
    m_nodes.append(jump);
    m_nodes.append(crouch);
    m_nodes.append(reload);

    syncTranslatorNodes();
    update();
}

bool KeymapperOverlay::saveProfileByName(const QString &profileName)
{
    QString name = profileName.isEmpty() ? "Default" : profileName;
    QString targetPath = QString("%1/%2.json").arg(profilesDir(), name);

    QJsonObject root;
    root["profile_name"] = name;
    root["format"]       = "relative_v3";

    QJsonArray controlsArray;
    for (const auto &node : m_nodes) {
        QJsonObject c;
        c["id"]        = node.id;
        c["rel_x"]     = node.relX;
        c["rel_y"]     = node.relY;
        c["radius"]    = node.radius;
        c["label"]     = node.label;
        c["action"]    = node.action;
        c["key"]       = node.primaryKey;
        c["scan_code"] = static_cast<qint64>(node.primaryScanCode);

        switch (node.type) {
            case KeyNodeType::Dpad:
                c["type"]       = "dpad";
                c["scan_up"]    = static_cast<qint64>(node.scanUp);
                c["scan_down"]  = static_cast<qint64>(node.scanDown);
                c["scan_left"]  = static_cast<qint64>(node.scanLeft);
                c["scan_right"] = static_cast<qint64>(node.scanRight);
                break;
            case KeyNodeType::AimShoot:
                c["type"]          = "aim_lock";
                c["sensitivity_x"] = node.sensitivityX;
                c["sensitivity_y"] = node.sensitivityY;
                c["acceleration"]  = node.acceleration;
                c["enable_fire"]   = node.enableFireButton;
                c["fire_rel_x"]    = node.fireRelX;
                c["fire_rel_y"]    = node.fireRelY;
                break;
            case KeyNodeType::Swipe: {
                c["type"]        = "swipe";
                c["end_rel_x"]   = node.endRelX;
                c["end_rel_y"]   = node.endRelY;
                c["duration_ms"] = node.durationMs;
                QJsonArray ptsArr;
                for (const auto &pt : node.pathPoints) {
                    QJsonObject po;
                    po["x"] = pt.x();
                    po["y"] = pt.y();
                    ptsArr.append(po);
                }
                c["path_points"] = ptsArr;
                break;
            }
            case KeyNodeType::BurstTap:
                c["type"]     = "burst_tap";
                c["burst_hz"] = node.burstHz;
                break;
            case KeyNodeType::FreeLook:
                c["type"]          = "free_look";
                c["sensitivity_x"] = node.sensitivityX;
                c["sensitivity_y"] = node.sensitivityY;
                c["acceleration"]  = node.acceleration;
                break;
            case KeyNodeType::SmartCast:
                c["type"]        = "smart_cast";
                c["cast_radius"] = node.castRadius;
                break;
            case KeyNodeType::Tap:
            default:
                c["type"] = "tap";
                break;
        }
        controlsArray.append(c);
    }
    root["controls"] = controlsArray;

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    QSettings("LinStack", "Keymapper").setValue("lastProfile", name);
    emit layoutSaved(targetPath);
    return true;
}
bool KeymapperOverlay::loadProfileByName(const QString &profileName)
{
    QString name = profileName.isEmpty() ? "Default" : profileName;
    QString targetPath = QString("%1/%2.json").arg(profilesDir(), name);

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    m_currentProfileName = name;
    QJsonArray controls = root["controls"].toArray();
    m_nodes.clear();

    for (const auto &val : controls) {
        QJsonObject c = val.toObject();
        QString typeStr = c["type"].toString();

        KeyBindingNode node;
        node.id              = c["id"].toString();
        node.relX            = c["rel_x"].toDouble(0.5);
        node.relY            = c["rel_y"].toDouble(0.5);
        node.radius          = c["radius"].toDouble(24.0);
        node.label           = c["label"].toString();
        node.action          = c["action"].toString();
        node.primaryKey      = c["key"].toInt(0);
        node.primaryScanCode = static_cast<quint32>(c["scan_code"].toInteger(0));

        if (typeStr == "dpad") {
            node.type      = KeyNodeType::Dpad;
            node.scanUp    = static_cast<quint32>(c["scan_up"].toInteger(25));
            node.scanDown  = static_cast<quint32>(c["scan_down"].toInteger(39));
            node.scanLeft  = static_cast<quint32>(c["scan_left"].toInteger(38));
            node.scanRight = static_cast<quint32>(c["scan_right"].toInteger(40));
        } else if (typeStr == "aim_lock" || typeStr == "aim_shoot") {
            node.type             = KeyNodeType::AimShoot;
            node.sensitivityX     = c["sensitivity_x"].toDouble(1.2);
            node.sensitivityY     = c["sensitivity_y"].toDouble(1.2);
            node.acceleration     = c["acceleration"].toBool(false);
            node.enableFireButton = c["enable_fire"].toBool(true);
            node.fireRelX         = c["fire_rel_x"].toDouble(0.82);
            node.fireRelY         = c["fire_rel_y"].toDouble(0.64);
        } else if (typeStr == "swipe") {
            node.type       = KeyNodeType::Swipe;
            node.endRelX    = c["end_rel_x"].toDouble(0.5);
            node.endRelY    = c["end_rel_y"].toDouble(0.3);
            node.durationMs = c["duration_ms"].toInt(150);
            if (c.contains("path_points") && c["path_points"].isArray()) {
                QJsonArray ptsArr = c["path_points"].toArray();
                for (const auto &pv : ptsArr) {
                    QJsonObject po = pv.toObject();
                    node.pathPoints.append(QPointF(po["x"].toDouble(), po["y"].toDouble()));
                }
            }
        } else if (typeStr == "burst_tap") {
            node.type    = KeyNodeType::BurstTap;
            node.burstHz = c["burst_hz"].toInt(10);
        } else if (typeStr == "free_look") {
            node.type         = KeyNodeType::FreeLook;
            node.sensitivityX = c["sensitivity_x"].toDouble(1.2);
            node.sensitivityY = c["sensitivity_y"].toDouble(1.2);
            node.acceleration = c["acceleration"].toBool(false);
        } else if (typeStr == "smart_cast") {
            node.type       = KeyNodeType::SmartCast;
            node.castRadius = c["cast_radius"].toDouble(160.0);
        } else {
            node.type = KeyNodeType::Tap;
        }
        m_nodes.append(node);
    }

    QSettings("LinStack", "Keymapper").setValue("lastProfile", name);
    syncTranslatorNodes();
    emit layoutLoaded(targetPath);
    update();
    return true;
}

bool KeymapperOverlay::isGearClicked(int nodeIndex, const QPointF &pos) const
{
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) return false;
    const auto &node = m_nodes[nodeIndex];
    if (!hasNodeSettings(node.type)) return false;

    qreal gx = node.relX * width() + node.radius * 0.75;
    qreal gy = node.relY * height() - node.radius * 0.75;
    qreal distSq = (pos.x() - gx) * (pos.x() - gx) + (pos.y() - gy) * (pos.y() - gy);
    return distSq <= 14.0 * 14.0;
}

bool KeymapperOverlay::isDeleteClicked(int nodeIndex, const QPointF &pos) const
{
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) return false;
    const auto &node = m_nodes[nodeIndex];

    qreal dx = node.relX * width() - node.radius * 0.75;
    qreal dy = node.relY * height() - node.radius * 0.75;
    qreal distSq = (pos.x() - dx) * (pos.x() - dx) + (pos.y() - dy) * (pos.y() - dy);
    return distSq <= 14.0 * 14.0;
}

int KeymapperOverlay::findNodeAt(const QPointF &pos) const
{
    for (int i = m_nodes.size() - 1; i >= 0; --i) {
        if (isDeleteClicked(i, pos) || isGearClicked(i, pos)) return i;

        qreal cx = m_nodes[i].relX * width();
        qreal cy = m_nodes[i].relY * height();
        qreal r = m_nodes[i].radius;

        if ((pos.x() - cx) * (pos.x() - cx) + (pos.y() - cy) * (pos.y() - cy) <= r * r) {
            return i;
        }

        if (m_nodes[i].type == KeyNodeType::AimShoot && m_nodes[i].enableFireButton) {
            qreal fx = m_nodes[i].fireRelX * width();
            qreal fy = m_nodes[i].fireRelY * height();
            if ((pos.x() - fx) * (pos.x() - fx) + (pos.y() - fy) * (pos.y() - fy) <= 24.0 * 24.0) {
                return i;
            }
        }

        if (m_nodes[i].type == KeyNodeType::Swipe) {
            qreal ex = m_nodes[i].endRelX * width();
            qreal ey = m_nodes[i].endRelY * height();
            if ((pos.x() - ex) * (pos.x() - ex) + (pos.y() - ey) * (pos.y() - ey) <= 16.0 * 16.0) {
                return i;
            }
        }
    }
    return -1;
}

void KeymapperOverlay::positionEditorBar()
{
    if (m_editorBar) {
        m_editorBar->adjustSize();
        m_editorBar->move(qMax(8, (width() - m_editorBar->width()) / 2), 12);
    }
    if (m_catalogPanel) {
        m_catalogPanel->move(width() - m_catalogPanel->width() - 10, 68);
    }
}

void KeymapperOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionEditorBar();
    if (m_translator) {
        m_translator->setScreenGeometry(width(), height());
    }
}

void KeymapperOverlay::wheelEvent(QWheelEvent *event)
{
    if (m_mode == Mode::Edit && m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
        int delta = event->angleDelta().y() > 0 ? 4 : -4;
        auto &node = m_nodes[m_selectedNodeIndex];
        if (node.type == KeyNodeType::SmartCast) {
            node.castRadius = qBound(60.0, node.castRadius + delta * 2, 400.0);
        } else {
            node.radius = qBound(16.0, node.radius + delta, 140.0);
        }
        syncTranslatorNodes();
        update();
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void KeymapperOverlay::mousePressEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit) {
        if (event->button() == Qt::LeftButton) {
            // 1. التحقق من سحب لوحة خيارات الـ Keymapping نفسها لتحريكها بحرية
            if (m_nodeSettingsPanel && m_nodeSettingsPanel->isVisible() &&
                m_nodeSettingsPanel->geometry().contains(event->position().toPoint())) {
                m_isDraggingSettingsPanel = true;
                m_settingsDragOffset = event->position() - QPointF(m_nodeSettingsPanel->pos());
                m_nodeSettingsPanel->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return;
            }

            int idx = findNodeAt(event->position());
            if (idx >= 0) {
                m_selectedNodeIndex = idx;

                if (isDeleteClicked(idx, event->position())) {
                    removeSelectedNode();
                    event->accept();
                    return;
                }

                if (isGearClicked(idx, event->position())) {
                    m_settingsOpen = !m_settingsOpen;
                    updateNodeSettingsPanel();
                    update();
                    event->accept();
                    return;
                }

                m_isDraggingNode = true;
                m_isDraggingSwipeEnd = false;
                m_isDraggingFireBtn = false;
                m_isResizingDpad = false;

                if (m_nodes[idx].type == KeyNodeType::Dpad) {
                    qreal hx = m_nodes[idx].relX * width() + m_nodes[idx].radius * 0.72;
                    qreal hy = m_nodes[idx].relY * height() + m_nodes[idx].radius * 0.72;
                    if ((event->position().x() - hx) * (event->position().x() - hx) +
                        (event->position().y() - hy) * (event->position().y() - hy) <= 16.0 * 16.0) {
                        m_isResizingDpad = true;
                        setCursor(Qt::SizeFDiagCursor);
                        update();
                        event->accept();
                        return;
                    }
                }

                if (m_nodes[idx].type == KeyNodeType::AimShoot && m_nodes[idx].enableFireButton) {
                    qreal fx = m_nodes[idx].fireRelX * width();
                    qreal fy = m_nodes[idx].fireRelY * height();
                    if ((event->position().x() - fx) * (event->position().x() - fx) +
                        (event->position().y() - fy) * (event->position().y() - fy) <= 24.0 * 24.0) {
                        m_isDraggingFireBtn = true;
                        m_dragOffset = event->position() - QPointF(fx, fy);
                        update();
                        event->accept();
                        return;
                    }
                }

                if (m_nodes[idx].type == KeyNodeType::Swipe) {
                    qreal ex = m_nodes[idx].endRelX * width();
                    qreal ey = m_nodes[idx].endRelY * height();
                    if ((event->position().x() - ex) * (event->position().x() - ex) +
                        (event->position().y() - ey) * (event->position().y() - ey) <= 16.0 * 16.0) {
                        m_isDraggingSwipeEnd = true;
                        m_dragOffset = event->position() - QPointF(ex, ey);
                        update();
                        event->accept();
                        return;
                    }
                }

                qreal cx = m_nodes[idx].relX * width();
                qreal cy = m_nodes[idx].relY * height();
                m_dragOffset = event->position() - QPointF(cx, cy);
                setCursor(Qt::ClosedHandCursor);
                updateNodeSettingsPanel();
                update();
                event->accept();
                return;
            } else {
                // الضغط في مكان فارغ: تجهيز الإنشاء السريع (نقر = Tap Spot، سحب = Freehand Swipe بالخط الأخضر)
                m_selectedNodeIndex = -1;
                m_settingsOpen = false;
                updateNodeSettingsPanel();

                m_isDrawingFreehand = true;
                m_emptyPressPos = event->position();
                m_freehandPoints.clear();
                m_freehandPoints.append(event->position());
                update();
                event->accept();
                return;
            }
        }
    } else {
        if (m_translator && m_translator->handleMousePress(event->button())) {
            event->accept();
            return;
        }
        event->ignore();
        return;
    }
    QWidget::mousePressEvent(event);
}

void KeymapperOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit) {
        // تحريك نافذة الإعدادات بسلاسة
        if (m_isDraggingSettingsPanel && m_nodeSettingsPanel) {
            QPointF targetPos = event->position() - m_settingsDragOffset;
            int nx = qBound(8, qRound(targetPos.x()), width() - m_nodeSettingsPanel->width() - 8);
            int ny = qBound(60, qRound(targetPos.y()), height() - m_nodeSettingsPanel->height() - 8);
            m_nodeSettingsPanel->move(nx, ny);
            event->accept();
            return;
        }

        // تتبع حركة السحب بالماوس في مكان فارغ لرسم الخط الأخضر
        if (m_isDrawingFreehand) {
            if (m_freehandPoints.isEmpty() ||
                std::hypot(event->position().x() - m_freehandPoints.last().x(),
                           event->position().y() - m_freehandPoints.last().y()) >= 4.0) {
                m_freehandPoints.append(event->position());
                update();
            }
            event->accept();
            return;
        }

        if (m_isDraggingNode && m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
            QPointF newPos = event->position() - m_dragOffset;
            qreal relX = qBound(0.02, newPos.x() / width(), 0.98);
            qreal relY = qBound(0.02, newPos.y() / height(), 0.98);

            if (m_isResizingDpad) {
                qreal cx = m_nodes[m_selectedNodeIndex].relX * width();
                qreal cy = m_nodes[m_selectedNodeIndex].relY * height();
                qreal dist = std::hypot(event->position().x() - cx, event->position().y() - cy);
                m_nodes[m_selectedNodeIndex].radius = qBound(30.0, dist, 150.0);
                if (m_settingsOpen) updateNodeSettingsPanel();
            } else if (m_isDraggingFireBtn) {
                m_nodes[m_selectedNodeIndex].fireRelX = relX;
                m_nodes[m_selectedNodeIndex].fireRelY = relY;
            } else if (m_isDraggingSwipeEnd) {
                m_nodes[m_selectedNodeIndex].endRelX = relX;
                m_nodes[m_selectedNodeIndex].endRelY = relY;
                m_nodes[m_selectedNodeIndex].pathPoints.clear();
            } else {
                qreal dx = relX - m_nodes[m_selectedNodeIndex].relX;
                qreal dy = relY - m_nodes[m_selectedNodeIndex].relY;
                m_nodes[m_selectedNodeIndex].relX = relX;
                m_nodes[m_selectedNodeIndex].relY = relY;
                if (m_nodes[m_selectedNodeIndex].type == KeyNodeType::Swipe) {
                    m_nodes[m_selectedNodeIndex].endRelX = qBound(0.02, m_nodes[m_selectedNodeIndex].endRelX + dx, 0.98);
                    m_nodes[m_selectedNodeIndex].endRelY = qBound(0.02, m_nodes[m_selectedNodeIndex].endRelY + dy, 0.98);
                    for (auto &pt : m_nodes[m_selectedNodeIndex].pathPoints) {
                        pt.setX(qBound(0.02, pt.x() + dx, 0.98));
                        pt.setY(qBound(0.02, pt.y() + dy, 0.98));
                    }
                }
            }

            syncTranslatorNodes();
            update();
            event->accept();
            return;
        } else {
            int hoverIdx = findNodeAt(event->position());
            setCursor(hoverIdx >= 0 ? Qt::OpenHandCursor : Qt::CrossCursor);
        }
    } else {
        if (m_translator) {
            if (m_translator->isAimLockActive() || m_translator->isFreeLookActive()) {

                QPoint currentPos = event->position().toPoint();
                if (!m_ignoreNextMouseMove) {
                    int dx = currentPos.x() - m_lastMousePos.x();
                    int dy = currentPos.y() - m_lastMousePos.y();
                    m_translator->handleMouseMove(dx, dy, currentPos);
                }
                m_ignoreNextMouseMove = false;

                QPoint localAnchor(qRound(m_activeAimAnchorNorm.x() * width()), qRound(m_activeAimAnchorNorm.y() * height()));
                m_lastMousePos = localAnchor;
                m_ignoreNextMouseMove = true;
                QCursor::setPos(mapToGlobal(localAnchor));
                event->accept();
                return;
            } else {
                m_lastMousePos = event->position().toPoint();
                m_translator->handleMouseMove(0, 0, m_lastMousePos);
            }
        }
        event->ignore();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void KeymapperOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit) {
        if (event->button() == Qt::LeftButton) {
            if (m_isDraggingSettingsPanel) {
                m_isDraggingSettingsPanel = false;
                if (m_nodeSettingsPanel) m_nodeSettingsPanel->setCursor(Qt::OpenHandCursor);
                event->accept();
                return;
            }

            if (m_isDrawingFreehand) {
                m_isDrawingFreehand = false;
                qreal totalDist = std::hypot(event->position().x() - m_emptyPressPos.x(),
                                             event->position().y() - m_emptyPressPos.y());

                if (totalDist < 16.0) {
                    KeyBindingNode tap;
                    tap.id     = QString("tap_%1").arg(m_nodes.size() + 1);
                    tap.type   = KeyNodeType::Tap;
                    tap.label  = "?";
                    tap.action = "Tap";
                    tap.relX   = qBound(0.02, m_emptyPressPos.x() / width(), 0.98);
                    tap.relY   = qBound(0.02, m_emptyPressPos.y() / height(), 0.98);
                    tap.radius = 24.0;
                    addNode(tap);
                    emit toastRequested("👆 Tap Spot created! Press any key to bind it.");
                } else {
                    m_freehandPoints.append(event->position());
                    KeyBindingNode sw;
                    sw.id         = QString("swipe_%1").arg(m_nodes.size() + 1);
                    sw.type       = KeyNodeType::Swipe;
                    sw.label      = "?";
                    sw.action     = "Custom Swipe";
                    sw.relX       = qBound(0.02, m_freehandPoints.first().x() / width(), 0.98);
                    sw.relY       = qBound(0.02, m_freehandPoints.first().y() / height(), 0.98);
                    sw.endRelX    = qBound(0.02, m_freehandPoints.last().x() / width(), 0.98);
                    sw.endRelY    = qBound(0.02, m_freehandPoints.last().y() / height(), 0.98);
                    sw.durationMs = 180;
                    sw.radius     = 22.0;
                    for (const auto &pt : m_freehandPoints) {
                        sw.pathPoints.append(QPointF(qBound(0.02, pt.x() / width(), 0.98),
                                                     qBound(0.02, pt.y() / height(), 0.98)));
                    }
                    addNode(sw);
                    emit toastRequested("💨 Custom Path Swipe created! Press any key to bind it.");
                }
                m_freehandPoints.clear();
                update();
                event->accept();
                return;
            }

            if (m_isDraggingNode) {
                m_isDraggingNode = false;
                m_isDraggingSwipeEnd = false;
                m_isDraggingFireBtn = false;
                m_isResizingDpad = false;
                setCursor(Qt::OpenHandCursor);
                update();
                event->accept();
                return;
            }
        }
    } else {
        if (m_translator && m_translator->handleMouseRelease(event->button())) {
            event->accept();
            return;
        }
        event->ignore();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void KeymapperOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    if (m_mode == Mode::Edit) {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            removeSelectedNode();
            event->accept();
            return;
        }
        if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
            auto &node = m_nodes[m_selectedNodeIndex];
            if (node.type != KeyNodeType::Dpad) {
                quint32 sc = event->nativeScanCode();
                if (sc != 0) {
                    node.primaryScanCode = sc;
                    node.primaryKey = event->key();
                    node.label = scanCodeToLabel(sc, event->key());
                    m_selectedNodeIndex = -1;
                    m_settingsOpen = false;
                    updateNodeSettingsPanel();
                    syncTranslatorNodes();
                    update();
                    emit toastRequested(QString("Rebound control to key: %1").arg(node.label));
                    event->accept();
                    return;
                }
            }
        }

        quint32 sc = event->nativeScanCode();
        for (const auto &node : m_nodes) {
            if ((node.type == KeyNodeType::AimShoot || node.type == KeyNodeType::FreeLook) &&
                (node.primaryScanCode == sc || event->key() == Qt::Key_F1)) {
                event->accept();
                return;
            }
        }
    }

    if (parentWidget()) {
        QCoreApplication::sendEvent(parentWidget(), event);
        if (event->isAccepted()) return;
    }

    QWidget::keyPressEvent(event);
}

void KeymapperOverlay::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    if (m_showSmartCastLine) {
        m_showSmartCastLine = false;
        update();
    }

    if (parentWidget()) {
        QCoreApplication::sendEvent(parentWidget(), event);
        if (event->isAccepted()) return;
    }

    QWidget::keyReleaseEvent(event);
}

void KeymapperOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(m_opacity);

    const qreal w = width();
    const qreal h = height();

    if (m_mode == Mode::Edit) {
        p.setPen(QColor(16, 185, 129, 25));
        const int gridSize = 40;
        for (int x = 0; x < w; x += gridSize) p.drawLine(x, 0, x, h);
        for (int y = 0; y < h; y += gridSize) p.drawLine(0, y, w, y);

        // رسم الخط الأخضر المتوهج أثناء سحب الماوس لإنشاء Swipe مخصص
        if (m_isDrawingFreehand && m_freehandPoints.size() >= 2) {
            QPainterPath path;
            path.moveTo(m_freehandPoints.first());
            for (int k = 1; k < m_freehandPoints.size(); ++k) {
                path.lineTo(m_freehandPoints[k]);
            }
            p.setPen(QPen(QColor("#00FF66"), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
            p.setBrush(QColor("#00FF66"));
            p.drawEllipse(m_freehandPoints.last(), 6, 6);
        }
    }

    if (m_showSmartCastLine && m_mode == Mode::Play) {
        p.setPen(QPen(Theme::ColorNeon, 2.5, Qt::DashLine));
        p.drawLine(m_smartCastCenter, m_smartCastTarget);
        p.setBrush(Theme::ColorNeon);
        p.drawEllipse(m_smartCastTarget, 6, 6);
    }

    for (int i = 0; i < m_nodes.size(); ++i) {
        const auto &node = m_nodes[i];
        const qreal cx = node.relX * w;
        const qreal cy = node.relY * h;
        const bool isSelected = (m_mode == Mode::Edit && m_selectedNodeIndex == i);

        switch (node.type) {
            case KeyNodeType::Dpad: {
                p.setBrush(QColor(17, 24, 39, isSelected ? 220 : 180));
                p.setPen(QPen(isSelected ? Theme::ColorNeon : Theme::ColorEmerald, isSelected ? 2.5 : 1.5));
                p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

                const qreal step = node.radius * 0.55;
                struct SubKey { QString text; qreal dx; qreal dy; };
                SubKey keys[] = {{"W", 0, -step}, {"S", 0, step}, {"A", -step, 0}, {"D", step, 0}};

                for (const auto &sk : keys) {
                    QPointF kp(cx + sk.dx, cy + sk.dy);
                    p.setBrush(QColor(22, 31, 48, 220));
                    p.setPen(QPen(Theme::ColorNeon, 1.2));
                    p.drawRoundedRect(QRectF(kp.x() - 12, kp.y() - 12, 24, 24), 4, 4);
                    p.setPen(Theme::ColorTextPrimary);
                    QFont kf = p.font(); kf.setPointSize(9); kf.setBold(true); p.setFont(kf);
                    p.drawText(QRectF(kp.x() - 12, kp.y() - 12, 24, 24), Qt::AlignCenter, sk.text);
                }

                if (m_mode == Mode::Edit) {
                    qreal hx = cx + node.radius * 0.72;
                    qreal hy = cy + node.radius * 0.72;
                    p.setBrush(QColor(11, 15, 25, 245));
                    p.setPen(QPen(Theme::ColorNeon, 1.5));
                    p.drawEllipse(QPointF(hx, hy), 11, 11);

                    p.setPen(QPen(Theme::ColorNeon, 1.6));
                    p.drawLine(QPointF(hx - 4, hy - 4), QPointF(hx + 4, hy + 4));
                    p.drawLine(QPointF(hx - 4, hy - 4), QPointF(hx - 1, hy - 4));
                    p.drawLine(QPointF(hx - 4, hy - 4), QPointF(hx - 4, hy - 1));
                    p.drawLine(QPointF(hx + 4, hy + 4), QPointF(hx + 1, hy + 4));
                    p.drawLine(QPointF(hx + 4, hy + 4), QPointF(hx + 4, hy + 1));
                }
                break;
            }

            case KeyNodeType::AimShoot:
            case KeyNodeType::FreeLook: {
                bool isFL = (node.type == KeyNodeType::FreeLook);
                QColor accent = isFL ? QColor(16, 185, 129) : QColor(59, 130, 246);
                p.setBrush(QColor(17, 24, 39, isSelected ? 230 : 190));
                p.setPen(QPen(accent, isSelected ? 2.5 : 1.5));
                p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

                p.setPen(Theme::ColorTextPrimary);
                QFont f = p.font(); f.setPointSize(8); f.setBold(true); p.setFont(f);
                p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                           Qt::AlignCenter, node.label);

                QRectF subRect(cx - 45, cy + node.radius + 2, 90, 14);
                p.setPen(isFL ? QColor("#6EE7B7") : QColor(147, 197, 253));
                f.setPointSize(7); f.setBold(false); p.setFont(f);
                p.drawText(subRect, Qt::AlignCenter, isFL ? "Free Look" : "Aim Lock");

                if (!isFL && node.enableFireButton) {
                    qreal fx = node.fireRelX * w;
                    qreal fy = node.fireRelY * h;
                    p.setBrush(QColor(239, 68, 68, 180));
                    p.setPen(QPen(Theme::ColorTextPrimary, 1.5));
                    p.drawEllipse(QPointF(fx, fy), 22, 22);
                    p.setPen(Theme::ColorTextPrimary);
                    f.setPointSize(8); f.setBold(true); p.setFont(f);
                    p.drawText(QRectF(fx - 22, fy - 22, 44, 44), Qt::AlignCenter, "Fire");
                }
                break;
            }

            case KeyNodeType::Swipe: {
                qreal ex = node.endRelX * w;
                qreal ey = node.endRelY * h;

                if (node.pathPoints.size() >= 2) {
                    QPainterPath savedPath;
                    savedPath.moveTo(node.pathPoints.first().x() * w, node.pathPoints.first().y() * h);
                    for (int k = 1; k < node.pathPoints.size(); ++k) {
                        savedPath.lineTo(node.pathPoints[k].x() * w, node.pathPoints[k].y() * h);
                    }
                    p.setPen(QPen(QColor("#00FF66"), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    p.drawPath(savedPath);
                } else {
                    p.setPen(QPen(QColor(245, 158, 11), 3.0, Qt::DashLine));
                    p.drawLine(QPointF(cx, cy), QPointF(ex, ey));
                }

                p.setBrush(QColor(17, 24, 39, 210));
                p.setPen(QPen(node.pathPoints.size() >= 2 ? QColor("#00FF66") : QColor(245, 158, 11), isSelected ? 2.5 : 1.5));
                p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

                p.setPen(Theme::ColorTextPrimary);
                QFont f = p.font(); f.setPointSize(8); f.setBold(true); p.setFont(f);
                p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                           Qt::AlignCenter, node.label);

                p.setBrush(QColor(245, 158, 11));
                p.setPen(QPen(Theme::ColorTextPrimary, 1.5));
                p.drawEllipse(QPointF(ex, ey), 10, 10);
                break;
            }

            case KeyNodeType::SmartCast: {
                p.setBrush(QColor(168, 85, 247, 25));
                p.setPen(QPen(QColor(168, 85, 247, 180), 1.2, Qt::DashLine));
                p.drawEllipse(QPointF(cx, cy), node.castRadius, node.castRadius);

                p.setBrush(QColor(17, 24, 39, 210));
                p.setPen(QPen(QColor(168, 85, 247), isSelected ? 2.5 : 1.5));
                p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

                p.setPen(Theme::ColorTextPrimary);
                QFont f = p.font(); f.setPointSize(9); f.setBold(true); p.setFont(f);
                p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                           Qt::AlignCenter, node.label);
                break;
            }

            case KeyNodeType::Tap:
            case KeyNodeType::BurstTap:
            default: {
                bool isBurst = (node.type == KeyNodeType::BurstTap);
                QRectF nodeRect(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2);
                p.setBrush(QColor(17, 24, 39, isSelected ? 230 : 190));
                p.setPen(QPen(isBurst ? QColor("#F59E0B") : Theme::ColorEmerald, isSelected ? 2.5 : 1.5));
                p.drawEllipse(nodeRect);

                p.setPen(Theme::ColorTextPrimary);
                QFont lf = p.font(); lf.setPointSize(node.radius > 24 ? 9 : 8); lf.setBold(true); p.setFont(lf);
                p.drawText(nodeRect, Qt::AlignCenter, node.label);
                break;
            }
        }
        if (m_mode == Mode::Edit) {
            qreal delX = cx - node.radius * 0.75;
            qreal delY = cy - node.radius * 0.75;
            p.setBrush(QColor(127, 29, 29, 245));
            p.setPen(QPen(QColor("#EF4444"), 1.3));
            p.drawEllipse(QPointF(delX, delY), 10, 10);

            p.setPen(QColor("#FCA5A5"));
            QFont df = p.font(); df.setPointSize(8); df.setBold(true); p.setFont(df);
            p.drawText(QRectF(delX - 10, delY - 10, 20, 20), Qt::AlignCenter, "✕");

            if (hasNodeSettings(node.type)) {
                qreal gx = cx + node.radius * 0.75;
                qreal gy = cy - node.radius * 0.75;
                p.setBrush(QColor(11, 15, 25, 245));
                p.setPen(QPen(Theme::ColorNeon, 1.3));
                p.drawEllipse(QPointF(gx, gy), 11, 11);

                p.setPen(Theme::ColorNeon);
                QFont gf = p.font(); gf.setPointSize(9); gf.setBold(true); p.setFont(gf);
                p.drawText(QRectF(gx - 11, gy - 11, 22, 22), Qt::AlignCenter, "⚙");
            }
        }

        if (isSelected) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Theme::ColorNeon, 1.5, Qt::DashLine));
            p.drawRect(QRectF(cx - node.radius - 6, cy - node.radius - 6,
                              (node.radius + 6) * 2, (node.radius + 6) * 2));
        }
    }
}
