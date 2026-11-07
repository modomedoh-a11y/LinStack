#include "KeymapperOverlay.h"
#include "UInputTouchBridge.h"
#include "Theme.h"

#include <QComboBox>
#include <QSlider>
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
#include <QCursor>
#include <cmath>

KeymapperOverlay::KeymapperOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_translator = new InputTranslator(nullptr, this);
    connect(m_translator, &InputTranslator::aimLockToggled, this, [this](bool active) {
        if (active) {
            setCursor(Qt::BlankCursor);
            emit toastRequested("🎯 Aim Lock Active: Mouse moves camera look. Press ESC or RMB to exit.");
        } else {
            setCursor(Qt::ArrowCursor);
            emit toastRequested("Aim Lock Released.");
        }
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
    resetToDefaultBinds();
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
    syncTranslatorNodes();
}

void KeymapperOverlay::setCurrentAppId(const QString &appId)
{
    m_appId = appId.isEmpty() ? "default" : appId;
    // Attempt auto-load for this game
    QString path = QString("%1/.config/LinStack/keymaps/%2.json").arg(QDir::homePath()).arg(m_appId);
    if (QFileInfo::exists(path)) {
        loadLayoutFromJson(path);
    }
}

void KeymapperOverlay::setupEditorUi()
{
    // ── Shared stylesheet for all buttons ─────────────────────────────────────
    const QString btnSS =
        "QPushButton {"
        "  background-color: #161F30;"
        "  color: #E5E7EB;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 12px;"
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

    // ── Floating top toolbar ───────────────────────────────────────────────────
    m_editorBar = new QWidget(this);
    m_editorBar->setFixedHeight(46);
    m_editorBar->setStyleSheet(
        "QWidget {"
        "  background-color: rgba(17,24,39,230);"
        "  border: 1px solid #10B981;"
        "  border-radius: 23px;"
        "}"
        + btnSS +
        "QComboBox {"
        "  background: #161F30; color: #E5E7EB; border: 1px solid #1F2937;"
        "  border-radius: 8px; padding: 2px 6px; font-size: 10px;"
        "}"
        "QComboBox:hover { border-color: #10B981; }"
        "QComboBox QAbstractItemView { background: #111827; color: #E5E7EB; }"
        "QSlider::groove:horizontal { height: 3px; background: #1F2937; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #10B981; width: 12px; height: 12px; border-radius: 6px; margin: -5px 0; }"
        "QSlider::sub-page:horizontal { background: #10B981; border-radius: 2px; }"
    );

    auto *barLayout = new QHBoxLayout(m_editorBar);
    barLayout->setContentsMargins(12, 4, 12, 4);
    barLayout->setSpacing(5);

    // Profile selector
    auto *profileLabel = new QLabel("Profile:", m_editorBar);
    profileLabel->setStyleSheet("color: #9CA3AF; font-size: 9px; font-weight: 600;");
    barLayout->addWidget(profileLabel);

    m_profileCombo = new QComboBox(m_editorBar);
    m_profileCombo->setFixedWidth(90);
    m_profileCombo->addItems({"Default", "FPS Shooter", "MOBA / AoS", "Battle Royale", "Side-Scroller"});
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, [this](const QString &profile) {
        emit toastRequested(QString("Profile switched to: %1").arg(profile));
    });
    barLayout->addWidget(m_profileCombo);

    // Opacity slider
    auto *opLabel = new QLabel("Opacity:", m_editorBar);
    opLabel->setStyleSheet("color: #9CA3AF; font-size: 9px; font-weight: 600;");
    barLayout->addWidget(opLabel);

    m_opacitySlider = new QSlider(Qt::Horizontal, m_editorBar);
    m_opacitySlider->setRange(20, 100);
    m_opacitySlider->setValue(static_cast<int>(m_opacity * 100));
    m_opacitySlider->setFixedWidth(70);
    m_opacityLabel = new QLabel(QString("%1%").arg(m_opacitySlider->value()), m_editorBar);
    m_opacityLabel->setStyleSheet("color: #10B981; font-size: 9px; font-weight: 700; min-width: 28px;");
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_opacityLabel->setText(QString("%1%").arg(v));
        setOverlayOpacity(v / 100.0);
    });
    barLayout->addWidget(m_opacitySlider);
    barLayout->addWidget(m_opacityLabel);

    // Separator
    auto *sep1 = new QFrame(m_editorBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #1F2937; margin: 4px 2px;");
    barLayout->addWidget(sep1);

    // Clear All
    m_btnClearAll = new QPushButton("🗑 Clear All", m_editorBar);
    connect(m_btnClearAll, &QPushButton::clicked, this, [this]() {
        m_nodes.clear();
        m_selectedNodeIndex = -1;
        syncTranslatorNodes();
        update();
        emit toastRequested("All keymap nodes cleared.");
    });
    barLayout->addWidget(m_btnClearAll);

    m_btnSave = new QPushButton("💾 Save", m_editorBar);
    connect(m_btnSave, &QPushButton::clicked, this, [this]() { saveLayoutToJson(); });
    barLayout->addWidget(m_btnSave);

    m_btnLoad = new QPushButton("📂 Load", m_editorBar);
    connect(m_btnLoad, &QPushButton::clicked, this, [this]() { loadLayoutFromJson(); });
    barLayout->addWidget(m_btnLoad);

    m_btnReset = new QPushButton("↺ Reset", m_editorBar);
    connect(m_btnReset, &QPushButton::clicked, this, &KeymapperOverlay::resetToDefaultBinds);
    barLayout->addWidget(m_btnReset);

    m_btnToggleMode = new QPushButton("✔ Done", m_editorBar);
    m_btnToggleMode->setProperty("primary", true);
    connect(m_btnToggleMode, &QPushButton::clicked, this, [this]() { setMode(Mode::Play); });
    barLayout->addWidget(m_btnToggleMode);

    // ── Right-side collapsible Node Catalog panel ─────────────────────────────
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

    // Container panel (right side, vertical)
    m_catalogPanel = new QWidget(this);
    m_catalogPanel->setFixedWidth(138);
    m_catalogPanel->setStyleSheet(
        "QWidget {"
        "  background-color: rgba(11,15,25,215);"
        "  border: 1px solid #1F2937;"
        "  border-radius: 10px;"
        "}"
        + btnSS
    );

    auto *panelLayout = new QVBoxLayout(m_catalogPanel);
    panelLayout->setContentsMargins(6, 8, 6, 8);
    panelLayout->setSpacing(5);

    // Panel header with collapse toggle
    auto *headerRow = new QHBoxLayout();
    auto *headerLabel = new QLabel("NODE CATALOG", m_catalogPanel);
    headerLabel->setStyleSheet("color: #10B981; font-size: 9px; font-weight: 700; letter-spacing: 1px; background: transparent; border: none;");
    headerRow->addWidget(headerLabel);

    m_btnToggleCatalog = new QPushButton("◁", m_catalogPanel);
    m_btnToggleCatalog->setFixedSize(20, 20);
    m_btnToggleCatalog->setStyleSheet(
        "QPushButton { background: transparent; color: #10B981; border: none; font-size: 10px; font-weight: 700; padding: 0; }"
        "QPushButton:hover { color: #00FF66; }"
    );
    connect(m_btnToggleCatalog, &QPushButton::clicked, this, [this]() {
        m_catalogExpanded = !m_catalogExpanded;
        m_btnToggleCatalog->setText(m_catalogExpanded ? "◁" : "▷");
        // Resize panel to collapsed/expanded width
        m_catalogPanel->setFixedWidth(m_catalogExpanded ? 138 : 22);
        positionEditorBar();
        update();
    });
    headerRow->addWidget(m_btnToggleCatalog);
    panelLayout->addLayout(headerRow);

    auto *sep = new QFrame(m_catalogPanel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #1F2937; background: #1F2937; border: none; max-height: 1px; margin: 2px 0;");
    panelLayout->addWidget(sep);

    // ── a) Tap Node
    m_btnAddTap = new QPushButton("👆  Tap Spot", m_catalogPanel);
    connect(m_btnAddTap, &QPushButton::clicked, this, [this]() {
        KeyBindingNode tap;
        tap.id         = QString("tap_%1").arg(m_nodes.size() + 1);
        tap.type       = KeyNodeType::Tap;
        tap.label      = "SPACE";
        tap.action     = "Action";
        tap.primaryKey = Qt::Key_Space;
        tap.relX       = 0.85; tap.relY = 0.75;
        tap.radius     = 24.0;
        addNode(tap);
    });
    panelLayout->addWidget(m_btnAddTap);

    // ── f) Burst Tap Node
    m_btnAddBurstTap = new QPushButton("🔥  Burst Tap", m_catalogPanel);
    connect(m_btnAddBurstTap, &QPushButton::clicked, this, [this]() {
        KeyBindingNode bt;
        bt.id         = QString("burst_%1").arg(m_nodes.size() + 1);
        bt.type       = KeyNodeType::BurstTap;
        bt.label      = "T";
        bt.action     = "Burst (10/s)";
        bt.primaryKey = Qt::Key_T;
        bt.relX       = 0.78; bt.relY = 0.62;
        bt.radius     = 24.0;
        bt.burstHz    = 10;
        addNode(bt);
    });
    panelLayout->addWidget(m_btnAddBurstTap);

    // ── b) WASD D-Pad
    m_btnAddDpad = new QPushButton("🕹  D-Pad (WASD)", m_catalogPanel);
    connect(m_btnAddDpad, &QPushButton::clicked, this, [this]() {
        KeyBindingNode dpad;
        dpad.id     = "dpad";
        dpad.type   = KeyNodeType::Dpad;
        dpad.label  = "WASD";
        dpad.action = "Movement";
        dpad.relX   = 0.18; dpad.relY = 0.70;
        dpad.radius = 60.0;
        addNode(dpad);
    });
    panelLayout->addWidget(m_btnAddDpad);

    // ── c) Aim, Pan & Shoot
    m_btnAddAim = new QPushButton("🎯  Aim / Shoot", m_catalogPanel);
    connect(m_btnAddAim, &QPushButton::clicked, this, [this]() {
        KeyBindingNode aim;
        aim.id              = "aim_shoot";
        aim.type            = KeyNodeType::AimShoot;
        aim.label           = "F1";
        aim.action          = "Look & Fire";
        aim.primaryKey      = Qt::Key_F1;
        aim.aimActivationKey = Qt::Key_F1;
        aim.aimSuspendKey   = Qt::Key_Alt;
        aim.relX            = 0.50; aim.relY = 0.50;
        aim.radius          = 28.0;
        aim.sensitivityX    = 1.2; aim.sensitivityY = 1.2;
        aim.fireRelX        = 0.82; aim.fireRelY = 0.64;
        addNode(aim);
    });
    panelLayout->addWidget(m_btnAddAim);

    // ── g) Free Look / Eye
    m_btnAddFreeLook = new QPushButton("👁  Free Look", m_catalogPanel);
    connect(m_btnAddFreeLook, &QPushButton::clicked, this, [this]() {
        KeyBindingNode fl;
        fl.id            = QString("freelook_%1").arg(m_nodes.size() + 1);
        fl.type          = KeyNodeType::FreeLook;
        fl.label         = "Z";
        fl.action        = "360° Camera";
        fl.primaryKey    = Qt::Key_Z;
        fl.relX          = 0.60; fl.relY = 0.45;
        fl.radius        = 26.0;
        fl.sensitivityX  = 1.0; fl.sensitivityY = 1.0;
        addNode(fl);
    });
    panelLayout->addWidget(m_btnAddFreeLook);

    // ── d) Swipe Node
    m_btnAddSwipe = new QPushButton("💨  Swipe", m_catalogPanel);
    connect(m_btnAddSwipe, &QPushButton::clicked, this, [this]() {
        KeyBindingNode sw;
        sw.id         = QString("swipe_%1").arg(m_nodes.size() + 1);
        sw.type       = KeyNodeType::Swipe;
        sw.label      = "UP";
        sw.action     = "Swipe Up";
        sw.primaryKey = Qt::Key_Up;
        sw.direction  = "up";
        sw.relX       = 0.50; sw.relY = 0.75;
        sw.endRelX    = 0.50; sw.endRelY = 0.40;
        sw.durationMs = 150;
        sw.radius     = 22.0;
        addNode(sw);
    });
    panelLayout->addWidget(m_btnAddSwipe);

    // ── e) Smart Cast / Skill Radius
    m_btnAddSmartCast = new QPushButton("⚡  Smart Cast", m_catalogPanel);
    connect(m_btnAddSmartCast, &QPushButton::clicked, this, [this]() {
        KeyBindingNode sc;
        sc.id         = QString("smart_cast_%1").arg(m_nodes.size() + 1);
        sc.type       = KeyNodeType::SmartCast;
        sc.label      = "Q";
        sc.action     = "Skill Cast";
        sc.primaryKey = Qt::Key_Q;
        sc.relX       = 0.75; sc.relY = 0.75;
        sc.radius     = 28.0;
        sc.castRadius = 160.0;
        addNode(sc);
    });
    panelLayout->addWidget(m_btnAddSmartCast);

    // ── h) Edge Scroll
    m_btnAddEdgeScroll = new QPushButton("⬡  Edge Scroll", m_catalogPanel);
    connect(m_btnAddEdgeScroll, &QPushButton::clicked, this, [this]() {
        KeyBindingNode es;
        es.id              = QString("edge_scroll_%1").arg(m_nodes.size() + 1);
        es.type            = KeyNodeType::EdgeScroll;
        es.label           = "LEFT";
        es.action          = "Pan Map";
        es.primaryKey      = Qt::Key_Left;
        es.direction       = "left";
        es.relX            = 0.5; es.relY = 0.5;
        es.radius          = 20.0;
        es.edgeThreshold   = 40;
        es.edgeScrollSpeed = 12.0;
        addNode(es);
    });
    panelLayout->addWidget(m_btnAddEdgeScroll);

    panelLayout->addStretch(1);
}

void KeymapperOverlay::setMode(Mode mode)
{
    m_mode = mode;
    m_selectedNodeIndex = -1;
    m_isDraggingNode = false;
    m_isDraggingSwipeEnd = false;

    if (m_mode == Mode::Edit) {
        m_editorBar->show();
        m_editorBar->raise();
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setCursor(Qt::ArrowCursor);
        emit toastRequested("⌨ Keymapper Edit Mode: Drag nodes, click to rebind, or click Save.");
    } else {
        m_editorBar->hide();
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setCursor(Qt::ArrowCursor);
        m_showSmartCastLine = false;
    }

    emit modeChanged(m_mode);
    update();
}

void KeymapperOverlay::setOverlayOpacity(qreal opacity)
{
    m_opacity = qBound(0.1, opacity, 1.0);
    update();
}

void KeymapperOverlay::syncTranslatorNodes()
{
    if (m_translator) {
        m_translator->setNodes(m_nodes);
        m_translator->setScreenGeometry(width(), height());
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
    if (node.type == KeyNodeType::AimShoot) {
        for (int i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i].type == KeyNodeType::AimShoot) {
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
        syncTranslatorNodes();
        update();
    }
}

void KeymapperOverlay::resetToDefaultBinds()
{
    m_nodes.clear();

    // 1. WASD D-Pad (8-way)
    m_nodes.append(KeyBindingNode{
        "dpad", KeyNodeType::Dpad, "WASD", "Movement", 0,
        0.18, 0.70, 60.0, 1.0
    });

    // 2. Aim, Pan & Shoot
    KeyBindingNode aim;
    aim.id = "aim_shoot";
    aim.type = KeyNodeType::AimShoot;
    aim.label = "F1 / RMB";
    aim.action = "Aim & Shoot";
    aim.primaryKey = Qt::Key_F1;
    aim.relX = 0.50;
    aim.relY = 0.50;
    aim.radius = 28.0;
    aim.sensitivity = 1.2;
    aim.fireRelX = 0.82;
    aim.fireRelY = 0.64;
    m_nodes.append(aim);

    // 3. Tap Nodes
    m_nodes.append(KeyBindingNode{"tap_jump", KeyNodeType::Tap, "SPACE", "Jump", Qt::Key_Space, 0.88, 0.78, 24.0, 1.0});
    m_nodes.append(KeyBindingNode{"tap_crouch", KeyNodeType::Tap, "C", "Crouch", Qt::Key_C, 0.82, 0.86, 22.0, 1.0});
    m_nodes.append(KeyBindingNode{"tap_reload", KeyNodeType::Tap, "R", "Reload", Qt::Key_R, 0.88, 0.52, 22.0, 1.0});
    m_nodes.append(KeyBindingNode{"tap_sprint", KeyNodeType::Tap, "SHIFT", "Sprint", Qt::Key_Shift, 0.18, 0.44, 22.0, 1.0});

    // 4. Swipe Node (Subway Surfers gesture)
    KeyBindingNode sw;
    sw.id = "swipe_up";
    sw.type = KeyNodeType::Swipe;
    sw.label = "UP";
    sw.action = "Swipe Jump";
    sw.primaryKey = Qt::Key_Up;
    sw.direction = "up";
    sw.relX = 0.50;
    sw.relY = 0.75;
    sw.endRelX = 0.50;
    sw.endRelY = 0.40;
    sw.durationMs = 150;
    sw.radius = 22.0;
    m_nodes.append(sw);

    // 5. Smart Cast Node (MOBA skill)
    KeyBindingNode sc;
    sc.id = "smart_cast_q";
    sc.type = KeyNodeType::SmartCast;
    sc.label = "Q";
    sc.action = "Smart Cast";
    sc.primaryKey = Qt::Key_Q;
    sc.relX = 0.76;
    sc.relY = 0.76;
    sc.radius = 26.0;
    sc.castRadius = 150.0;
    m_nodes.append(sc);

    syncTranslatorNodes();
    update();
}

bool KeymapperOverlay::saveLayoutToJson(const QString &filePath)
{
    QString targetPath = filePath;
    if (targetPath.isEmpty()) {
        QString dir = QString("%1/.config/LinStack/keymaps").arg(QDir::homePath());
        QDir().mkpath(dir);
        targetPath = QString("%1/%2.json").arg(dir).arg(m_appId.isEmpty() ? "default" : m_appId);
    }

    QJsonObject root;
    root["game_id"]      = m_appId.isEmpty() ? "LinStack.default" : m_appId;
    root["format"]       = "relative_v2"; // Relative coordinate format (0.0–1.0)

    QJsonArray controlsArray;
    for (const auto &node : m_nodes) {
        QJsonObject c;

        // ── All positions stored as normalised relative coords (0.0–1.0) ──────
        c["rel_x"]  = node.relX;
        c["rel_y"]  = node.relY;
        c["radius"] = node.radius;

        switch (node.type) {
        case KeyNodeType::Dpad:
            c["type"]      = "dpad";
            c["key_up"]    = "W";
            c["key_down"]  = "S";
            c["key_left"]  = "A";
            c["key_right"] = "D";
            break;
        case KeyNodeType::AimShoot:
            c["type"]           = "aim_shoot";
            c["activation_key"] = "F1";
            c["sensitivity"]    = node.sensitivity;
            c["fire_rel_x"]     = node.fireRelX;  // relative, not absolute
            c["fire_rel_y"]     = node.fireRelY;
            break;
        case KeyNodeType::Tap:
            c["type"]   = "tap";
            c["key"]    = node.label;
            c["action"] = node.action;
            break;
        case KeyNodeType::Swipe:
            c["type"]        = "swipe";
            c["direction"]   = node.direction;
            c["key"]         = node.label;
            c["end_rel_x"]   = node.endRelX;  // relative, not absolute
            c["end_rel_y"]   = node.endRelY;
            c["duration_ms"] = node.durationMs;
            break;
        case KeyNodeType::SmartCast:
            c["type"]        = "smart_cast";
            c["key"]         = node.label;
            c["cast_radius"] = node.castRadius;
            break;
        case KeyNodeType::BurstTap:
            c["type"]     = "burst_tap";
            c["key"]      = node.label;
            c["action"]   = node.action;
            c["burst_hz"] = node.burstHz;
            break;
        case KeyNodeType::FreeLook:
            c["type"]          = "free_look";
            c["key"]           = node.label;
            c["sensitivity_x"] = node.sensitivityX;
            c["sensitivity_y"] = node.sensitivityY;
            break;
        case KeyNodeType::EdgeScroll:
            c["type"]             = "edge_scroll";
            c["key"]              = node.label;
            c["direction"]        = node.direction;
            c["edge_threshold"]   = node.edgeThreshold;
            c["edge_scroll_speed"]= node.edgeScrollSpeed;
            break;
        }
        controlsArray.append(c);
    }
    root["controls"] = controlsArray;

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit toastRequested("Failed to write keymap JSON file.");
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    emit layoutSaved(targetPath);
    emit toastRequested(QString("✔ Keymap saved: %1").arg(QFileInfo(targetPath).fileName()));
    return true;
}

bool KeymapperOverlay::loadLayoutFromJson(const QString &filePath)
{
    QString targetPath = filePath;
    if (targetPath.isEmpty()) {
        targetPath = QString("%1/.config/LinStack/keymaps/%2.json")
                         .arg(QDir::homePath())
                         .arg(m_appId.isEmpty() ? "default" : m_appId);
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    m_appId = root["game_id"].toString(m_appId);
    QJsonArray controls = root["controls"].toArray();

    // Detect format: new relative_v2 uses rel_x/rel_y; legacy uses absolute x/y
    const bool isRelativeFormat = root["format"].toString() == "relative_v2";

    // Legacy format may need pixel→relative conversion based on overlay size.
    // Use a safe non-zero fallback so division is harmless.
    const qreal legacyW = qMax(1, width());
    const qreal legacyH = qMax(1, height());

    m_nodes.clear();

    for (const auto &val : controls) {
        QJsonObject c = val.toObject();
        QString typeStr = c["type"].toString();

        KeyBindingNode node;

        // ── Position: prefer relative coords; fall back to legacy absolute ───
        if (isRelativeFormat) {
            node.relX = c["rel_x"].toDouble(0.5);
            node.relY = c["rel_y"].toDouble(0.5);
        } else {
            node.relX = c["x"].toDouble(legacyW * 0.5) / legacyW;
            node.relY = c["y"].toDouble(legacyH * 0.5) / legacyH;
        }
        node.radius = c["radius"].toDouble(24.0);

        if (typeStr == "dpad") {
            node.type   = KeyNodeType::Dpad;
            node.id     = "dpad";
            node.label  = "WASD";
            node.action = "Movement";
        } else if (typeStr == "aim_shoot") {
            node.type        = KeyNodeType::AimShoot;
            node.id          = "aim_shoot";
            node.label       = c["activation_key"].toString("F1");
            node.action      = "Aim & Shoot";
            node.sensitivity = c["sensitivity"].toDouble(1.2);
            if (isRelativeFormat) {
                node.fireRelX = c["fire_rel_x"].toDouble(0.82);
                node.fireRelY = c["fire_rel_y"].toDouble(0.64);
            } else {
                node.fireRelX = c["fire_x"].toDouble(legacyW * 0.82) / legacyW;
                node.fireRelY = c["fire_y"].toDouble(legacyH * 0.64) / legacyH;
            }
        } else if (typeStr == "swipe") {
            node.type       = KeyNodeType::Swipe;
            node.id         = QString("swipe_%1").arg(m_nodes.size());
            node.label      = c["key"].toString("UP");
            node.direction  = c["direction"].toString("up");
            node.durationMs = c["duration_ms"].toInt(150);
            node.primaryKey = Qt::Key_Up;
            if (isRelativeFormat) {
                node.endRelX = c["end_rel_x"].toDouble(0.5);
                node.endRelY = c["end_rel_y"].toDouble(0.3);
            } else {
                node.endRelX = c["end_x"].toDouble(legacyW * 0.5) / legacyW;
                node.endRelY = c["end_y"].toDouble(legacyH * 0.3) / legacyH;
            }
        } else if (typeStr == "smart_cast") {
            node.type       = KeyNodeType::SmartCast;
            node.id         = QString("smart_cast_%1").arg(m_nodes.size());
            node.label      = c["key"].toString("Q");
            node.castRadius = c["cast_radius"].isUndefined()
                                  ? c["radius"].toDouble(160.0)   // legacy key
                                  : c["cast_radius"].toDouble(160.0);
            node.primaryKey = Qt::Key_Q;
        } else if (typeStr == "burst_tap") {
            node.type       = KeyNodeType::BurstTap;
            node.id         = QString("burst_%1").arg(m_nodes.size());
            node.label      = c["key"].toString("T");
            node.action     = c["action"].toString("Burst");
            node.burstHz    = c["burst_hz"].toInt(10);
            node.primaryKey = QKeySequence(node.label).isEmpty() ? Qt::Key_T
                                  : QKeySequence(node.label)[0].toCombined();
        } else if (typeStr == "free_look") {
            node.type        = KeyNodeType::FreeLook;
            node.id          = QString("freelook_%1").arg(m_nodes.size());
            node.label       = c["key"].toString("Z");
            node.sensitivityX = c["sensitivity_x"].toDouble(1.0);
            node.sensitivityY = c["sensitivity_y"].toDouble(1.0);
            node.primaryKey  = QKeySequence(node.label).isEmpty() ? Qt::Key_Z
                                   : QKeySequence(node.label)[0].toCombined();
        } else if (typeStr == "edge_scroll") {
            node.type           = KeyNodeType::EdgeScroll;
            node.id             = QString("edge_scroll_%1").arg(m_nodes.size());
            node.label          = c["key"].toString("LEFT");
            node.direction      = c["direction"].toString("left");
            node.edgeThreshold  = c["edge_threshold"].toInt(40);
            node.edgeScrollSpeed = c["edge_scroll_speed"].toDouble(12.0);
            node.primaryKey     = Qt::Key_Left;
        } else {
            // Default: Tap
            node.type       = KeyNodeType::Tap;
            node.id         = QString("tap_%1").arg(m_nodes.size());
            node.label      = c["key"].toString("SPACE");
            node.action     = c["action"].toString("Action");
            node.primaryKey = QKeySequence(node.label).isEmpty() ? Qt::Key_Space
                                  : QKeySequence(node.label)[0].toCombined();
        }
        m_nodes.append(node);
    }

    syncTranslatorNodes();
    emit layoutLoaded(targetPath);
    emit toastRequested(QString("✔ Keymap loaded: %1").arg(QFileInfo(targetPath).fileName()));
    update();
    return true;
}

int KeymapperOverlay::findNodeAt(const QPointF &pos) const
{
    for (int i = m_nodes.size() - 1; i >= 0; --i) {
        qreal cx = m_nodes[i].relX * width();
        qreal cy = m_nodes[i].relY * height();
        qreal r = m_nodes[i].radius;

        qreal distSq = (pos.x() - cx) * (pos.x() - cx) + (pos.y() - cy) * (pos.y() - cy);
        if (distSq <= r * r) {
            return i;
        }

        // For swipe, check if clicking the end point
        if (m_nodes[i].type == KeyNodeType::Swipe) {
            qreal ex = m_nodes[i].endRelX * width();
            qreal ey = m_nodes[i].endRelY * height();
            qreal distEndSq = (pos.x() - ex) * (pos.x() - ex) + (pos.y() - ey) * (pos.y() - ey);
            if (distEndSq <= 16.0 * 16.0) {
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
        m_editorBar->move((width() - m_editorBar->width()) / 2, 40);
    }
}

void KeymapperOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_editorBar) {
        m_editorBar->adjustSize();
        m_editorBar->move((width() - m_editorBar->width()) / 2, 40);
    }
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
            node.radius = qBound(16.0, node.radius + delta, 120.0);
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
            int idx = findNodeAt(event->position());
            if (idx >= 0) {
                m_selectedNodeIndex = idx;
                m_isDraggingNode = true;

                // Check if user clicked the swipe endpoint
                if (m_nodes[idx].type == KeyNodeType::Swipe) {
                    qreal ex = m_nodes[idx].endRelX * width();
                    qreal ey = m_nodes[idx].endRelY * height();
                    qreal distEndSq = (event->position().x() - ex) * (event->position().x() - ex) +
                                      (event->position().y() - ey) * (event->position().y() - ey);
                    if (distEndSq <= 16.0 * 16.0) {
                        m_isDraggingSwipeEnd = true;
                        m_dragOffset = event->position() - QPointF(ex, ey);
                        update();
                        event->accept();
                        return;
                    }
                }

                m_isDraggingSwipeEnd = false;
                qreal cx = m_nodes[idx].relX * width();
                qreal cy = m_nodes[idx].relY * height();
                m_dragOffset = event->position() - QPointF(cx, cy);
                setCursor(Qt::ClosedHandCursor);
                update();
                event->accept();
                return;
            } else {
                m_selectedNodeIndex = -1;
                update();
            }
        }
    } else {
        if (m_translator && m_translator->handleMousePress(event->button())) {
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void KeymapperOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit) {
        if (m_isDraggingNode && m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
            QPointF newPos = event->position() - m_dragOffset;
            qreal relX = qBound(0.02, newPos.x() / width(), 0.98);
            qreal relY = qBound(0.02, newPos.y() / height(), 0.98);

            if (m_isDraggingSwipeEnd) {
                m_nodes[m_selectedNodeIndex].endRelX = relX;
                m_nodes[m_selectedNodeIndex].endRelY = relY;
            } else {
                m_nodes[m_selectedNodeIndex].relX = relX;
                m_nodes[m_selectedNodeIndex].relY = relY;
            }

            syncTranslatorNodes();
            update();
            event->accept();
            return;
        } else {
            int hoverIdx = findNodeAt(event->position());
            setCursor(hoverIdx >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
    } else {
        // Play mode mouse movement for Aim Look and Smart Cast
        if (m_translator) {
            if (m_translator->isAimLockActive()) {
                QPoint currentPos = event->position().toPoint();
                if (!m_ignoreNextMouseMove) {
                    int dx = currentPos.x() - m_lastMousePos.x();
                    int dy = currentPos.y() - m_lastMousePos.y();
                    m_translator->handleMouseMove(dx, dy, currentPos);
                }
                m_ignoreNextMouseMove = false;

                // Infinite raw mouse deltas
                QPoint center(width() / 2, height() / 2);
                m_lastMousePos = center;
                m_ignoreNextMouseMove = true;
                QCursor::setPos(mapToGlobal(center));
                event->accept();
                return;
            } else {
                m_lastMousePos = event->position().toPoint();
                m_translator->handleMouseMove(0, 0, m_lastMousePos);
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void KeymapperOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit) {
        if (event->button() == Qt::LeftButton && m_isDraggingNode) {
            m_isDraggingNode = false;
            m_isDraggingSwipeEnd = false;
            setCursor(Qt::OpenHandCursor);
            update();
            event->accept();
            return;
        }
    } else {
        if (m_translator && m_translator->handleMouseRelease(event->button())) {
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void KeymapperOverlay::keyPressEvent(QKeyEvent *event)
{
    if (m_mode == Mode::Edit) {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            removeSelectedNode();
            event->accept();
            return;
        }
        // Rebind key for selected node
        if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < m_nodes.size()) {
            auto &node = m_nodes[m_selectedNodeIndex];
            if (node.type != KeyNodeType::Dpad) {
                node.primaryKey = event->key();
                node.label = QKeySequence(event->key()).toString().toUpper();
                syncTranslatorNodes();
                update();
                emit toastRequested(QString("Rebound node to key: %1").arg(node.label));
                event->accept();
                return;
            }
        }
    } else {
        if (m_translator && m_translator->handleKeyPress(event->key())) {
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void KeymapperOverlay::keyReleaseEvent(QKeyEvent *event)
{
    if (m_mode == Mode::Play) {
        if (m_translator && m_translator->handleKeyRelease(event->key())) {
            event->accept();
            return;
        }
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

    // In edit mode, draw subtle cyber grid
    if (m_mode == Mode::Edit) {
        p.setPen(QColor(16, 185, 129, 25));
        const int gridSize = 40;
        for (int x = 0; x < w; x += gridSize) {
            p.drawLine(x, 0, x, h);
        }
        for (int y = 0; y < h; y += gridSize) {
            p.drawLine(0, y, w, y);
        }
    }

    // Top status indicator in Play mode
    if (m_mode == Mode::Play) {
        QRectF banner(w * 0.5 - 150, 10, 300, 26);
        p.setBrush(QColor(17, 24, 39, 210));
        p.setPen(QPen(Theme::ColorEmerald, 1.2));
        p.drawRoundedRect(banner, 13, 13);

        p.setPen(m_translator && m_translator->isAimLockActive() ? Theme::ColorNeon : Theme::ColorMint);
        QFont f = p.font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);

        QString statusText = m_translator && m_translator->isAimLockActive()
            ? "🎯 AIM LOCK ACTIVE (ESC / RMB to Exit)"
            : "⌨ KEYMAPPER ACTIVE (Ctrl+Shift+K to Edit)";
        p.drawText(banner, Qt::AlignCenter, statusText);
    }

    // Smart Cast dynamic direction line
    if (m_showSmartCastLine && m_mode == Mode::Play) {
        p.setPen(QPen(Theme::ColorNeon, 2.5, Qt::DashLine));
        p.drawLine(m_smartCastCenter, m_smartCastTarget);
        p.setBrush(Theme::ColorNeon);
        p.drawEllipse(m_smartCastTarget, 6, 6);
    }

    // Draw all 5 node types
    for (int i = 0; i < m_nodes.size(); ++i) {
        const auto &node = m_nodes[i];
        const qreal cx = node.relX * w;
        const qreal cy = node.relY * h;
        const bool isSelected = (m_mode == Mode::Edit && m_selectedNodeIndex == i);

        switch (node.type) {
        // b) WASD D-Pad Node
        case KeyNodeType::Dpad: {
            p.setBrush(QColor(17, 24, 39, isSelected ? 220 : 180));
            p.setPen(QPen(isSelected ? Theme::ColorNeon : Theme::ColorEmerald, isSelected ? 2.5 : 1.5));
            p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

            const qreal step = node.radius * 0.55;
            struct SubKey { QString text; qreal dx; qreal dy; };
            SubKey keys[] = {
                {"W", 0, -step},
                {"S", 0, step},
                {"A", -step, 0},
                {"D", step, 0}
            };

            for (const auto &sk : keys) {
                QPointF kp(cx + sk.dx, cy + sk.dy);
                p.setBrush(QColor(22, 31, 48, 220));
                p.setPen(QPen(Theme::ColorNeon, 1.2));
                p.drawRoundedRect(QRectF(kp.x() - 12, kp.y() - 12, 24, 24), 4, 4);

                p.setPen(Theme::ColorTextPrimary);
                QFont kf = p.font();
                kf.setPointSize(9);
                kf.setBold(true);
                p.setFont(kf);
                p.drawText(QRectF(kp.x() - 12, kp.y() - 12, 24, 24), Qt::AlignCenter, sk.text);
            }

            p.setBrush(Theme::ColorEmerald);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(cx, cy), 4, 4);
            break;
        }

        // c) Aim, Pan & Shoot Node
        case KeyNodeType::AimShoot: {
            // Aim Look Center
            p.setBrush(QColor(17, 24, 39, isSelected ? 230 : 190));
            p.setPen(QPen(QColor(59, 130, 246), isSelected ? 2.5 : 1.5));
            p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

            p.setPen(Theme::ColorTextPrimary);
            QFont f = p.font();
            f.setPointSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                       Qt::AlignCenter, node.label);

            QRectF subRect(cx - 45, cy + node.radius + 2, 90, 14);
            p.setPen(QColor(147, 197, 253));
            f.setPointSize(7);
            f.setBold(false);
            p.setFont(f);
            p.drawText(subRect, Qt::AlignCenter, "Aim / Look");

            // Fire Button Location
            qreal fx = node.fireRelX * w;
            qreal fy = node.fireRelY * h;
            p.setBrush(QColor(239, 68, 68, 180));
            p.setPen(QPen(Theme::ColorTextPrimary, 1.5));
            p.drawEllipse(QPointF(fx, fy), 24, 24);
            p.setPen(Theme::ColorTextPrimary);
            f.setPointSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(fx - 24, fy - 24, 48, 48), Qt::AlignCenter, "LMB\nFire");
            break;
        }

        // d) Swipe Node / Gesture
        case KeyNodeType::Swipe: {
            qreal ex = node.endRelX * w;
            qreal ey = node.endRelY * h;

            // Swipe path line with arrow
            p.setPen(QPen(QColor(245, 158, 11), 3.0, Qt::DashLine));
            p.drawLine(QPointF(cx, cy), QPointF(ex, ey));

            // Start point
            p.setBrush(QColor(17, 24, 39, 210));
            p.setPen(QPen(QColor(245, 158, 11), isSelected ? 2.5 : 1.5));
            p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

            p.setPen(Theme::ColorTextPrimary);
            QFont f = p.font();
            f.setPointSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                       Qt::AlignCenter, node.label);

            // End point handle
            p.setBrush(QColor(245, 158, 11));
            p.setPen(QPen(Theme::ColorTextPrimary, 1.5));
            p.drawEllipse(QPointF(ex, ey), 10, 10);
            break;
        }

        // e) Smart Cast / Skill Radius Node
        case KeyNodeType::SmartCast: {
            // Outer radius preview in edit mode or when selected
            p.setBrush(QColor(168, 85, 247, 25));
            p.setPen(QPen(QColor(168, 85, 247, 180), 1.2, Qt::DashLine));
            p.drawEllipse(QPointF(cx, cy), node.castRadius, node.castRadius);

            // Center skill button
            p.setBrush(QColor(17, 24, 39, 210));
            p.setPen(QPen(QColor(168, 85, 247), isSelected ? 2.5 : 1.5));
            p.drawEllipse(QPointF(cx, cy), node.radius, node.radius);

            p.setPen(Theme::ColorTextPrimary);
            QFont f = p.font();
            f.setPointSize(9);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2),
                       Qt::AlignCenter, node.label);

            QRectF sub(cx - 45, cy + node.radius + 2, 90, 14);
            p.setPen(QColor(216, 180, 254));
            f.setPointSize(7);
            f.setBold(false);
            p.setFont(f);
            p.drawText(sub, Qt::AlignCenter, "Smart Cast");
            break;
        }

        // a) Tap Node
        case KeyNodeType::Tap:
        default: {
            QRectF nodeRect(cx - node.radius, cy - node.radius, node.radius * 2, node.radius * 2);
            p.setBrush(QColor(17, 24, 39, isSelected ? 230 : 190));
            p.setPen(QPen(Theme::ColorEmerald, isSelected ? 2.5 : 1.5));
            p.drawEllipse(nodeRect);

            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Theme::ColorNeon, 0.8));
            p.drawEllipse(QPointF(cx, cy), node.radius - 3, node.radius - 3);

            p.setPen(Theme::ColorTextPrimary);
            QFont lf = p.font();
            lf.setPointSize(node.radius > 24 ? 9 : 8);
            lf.setBold(true);
            p.setFont(lf);
            p.drawText(nodeRect, Qt::AlignCenter, node.label);

            if (!node.action.isEmpty()) {
                QRectF subRect(cx - 45, cy + node.radius + 2, 90, 14);
                p.setPen(Theme::ColorMint);
                QFont sf = p.font();
                sf.setPointSize(7);
                sf.setBold(false);
                p.setFont(sf);
                p.drawText(subRect, Qt::AlignCenter, node.action);
            }
            break;
        }
        }

        // Selection rectangle and coordinates readout in edit mode
        if (isSelected) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Theme::ColorNeon, 1.5, Qt::DashLine));
            p.drawRect(QRectF(cx - node.radius - 6, cy - node.radius - 6,
                              (node.radius + 6) * 2, (node.radius + 6) * 2));

            QRectF infoBox(cx - 75, cy - node.radius - 22, 150, 16);
            p.setBrush(QColor(11, 15, 25, 230));
            p.setPen(QPen(Theme::ColorNeon, 1.0));
            p.drawRoundedRect(infoBox, 3, 3);

            p.setPen(Theme::ColorNeon);
            QFont ifont = p.font();
            ifont.setPointSize(7);
            p.setFont(ifont);
            p.drawText(infoBox, Qt::AlignCenter,
                       QString("X: %1% Y: %2% | R: %3px")
                           .arg(static_cast<int>(node.relX * 100))
                           .arg(static_cast<int>(node.relY * 100))
                           .arg(static_cast<int>(node.radius)));
        }
    }
}
