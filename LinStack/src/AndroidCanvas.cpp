#include <QDir>
#include <QTextStream>
#include "AndroidCanvas.h"
#include "KeymapperOverlay.h"
#include "UInputTouchBridge.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMimeData>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <QRect>

AndroidCanvas::AndroidCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_Hover, true);
    setupUi();
    refreshInstalledApps();
}

AndroidCanvas::~AndroidCanvas()
{
    releaseEmbeddedWindow();
}

void AndroidCanvas::setupUi()
{
    // No layout is applied to this widget — we manually position the stacked
    // content within the computed letterbox viewport in updateCanvasGeometry().

    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: transparent;");

    // Page 0: Clean Android Launcher (Installed Apps Only)
    m_homeWidget = createHomeScreen();
    m_stack->addWidget(m_homeWidget);

    // Page 1: In-Frame Embedded App Viewport
    m_embedContainerWidget = new QWidget(this);
    m_embedContainerWidget->setStyleSheet("background-color: #000000;");
    auto *embedLayout = new QVBoxLayout(m_embedContainerWidget);
    embedLayout->setContentsMargins(0, 0, 0, 0);
    embedLayout->setSpacing(0);

    // Embedded Game Top Navigation Bar
    auto *gameNavBar = new QWidget(m_embedContainerWidget);
    gameNavBar->setFixedHeight(36);
    gameNavBar->setStyleSheet("background-color: #0D121D; border-bottom: 1px solid #1F2937;");
    auto *nbLayout = new QHBoxLayout(gameNavBar);
    nbLayout->setContentsMargins(12, 0, 12, 0);
    nbLayout->setSpacing(8);

    auto *btnBackHome = new QPushButton("◁ Launcher", gameNavBar);
    btnBackHome->setCursor(Qt::PointingHandCursor);
    btnBackHome->setStyleSheet(
        "QPushButton { background: #161F30; color: #00FF66; border: 1px solid #1F2937; border-radius: 4px; padding: 4px 10px; font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { background: #1E293B; border-color: #10B981; }"
    );
    connect(btnBackHome, &QPushButton::clicked, this, &AndroidCanvas::releaseEmbeddedWindow);
    nbLayout->addWidget(btnBackHome);

    m_gameTitleBadge = new QLabel("Active App", gameNavBar);
    m_gameTitleBadge->setStyleSheet("color: #F9FAFB; font-size: 12px; font-weight: 700;");
    nbLayout->addWidget(m_gameTitleBadge);

    nbLayout->addStretch(1);

    m_gameFpsBadge = new QLabel("120 FPS | Mesa DRI3 (Wayland)", gameNavBar);
    m_gameFpsBadge->setStyleSheet("color: #10B981; font-family: monospace; font-size: 11px; font-weight: 600;");
    nbLayout->addWidget(m_gameFpsBadge);

    embedLayout->addWidget(gameNavBar);

    // Viewport area for window container
    m_gameViewportWidget = new QWidget(m_embedContainerWidget);
    m_gameViewportWidget->setStyleSheet("background-color: #05070D;");
    auto *vpLayout = new QVBoxLayout(m_gameViewportWidget);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(0);
    embedLayout->addWidget(m_gameViewportWidget, 1);

    m_stack->addWidget(m_embedContainerWidget);
    // NOTE: m_stack geometry is managed by updateCanvasGeometry() (letterboxing),
    // so we do NOT add it to any layout here.

    // Overlay: Keymapper HUD Layer (Active over app viewport)
    m_keymapperOverlay = new KeymapperOverlay(this);
    m_keymapperOverlay->hide();
    connect(m_keymapperOverlay, &KeymapperOverlay::toastRequested, this, [this](const QString &msg) {
        showToast(msg, 3500);
    });

    // Floating Toast Notification
    m_toastLabel = new QLabel(this);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #111827;"
        "  color: #00FF66;"
        "  border: 1px solid #10B981;"
        "  border-radius: 8px;"
        "  padding: 8px 18px;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
    );
    m_toastLabel->hide();

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toastLabel, &QLabel::hide);
}

QWidget *AndroidCanvas::createHomeScreen()
{
    auto *home = new QWidget(this);
    home->setStyleSheet("background-color: #0B0F19;");

    auto *mainLayout = new QVBoxLayout(home);
    mainLayout->setContentsMargins(28, 16, 28, 20);
    mainLayout->setSpacing(16);

    // 1. Android Status Bar Simulation
    auto *statusBar = new QWidget(home);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("background: transparent;");
    auto *sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(4, 0, 4, 0);

    auto *osInfo = new QLabel("⚡ LinStack OS 13 | Mesa DRI3 (Wayland) | 60.0 FPS", statusBar);
    osInfo->setStyleSheet("color: #6B7280; font-size: 11px; font-weight: 600; font-family: monospace;");
    sbLayout->addWidget(osInfo);

    sbLayout->addStretch(1);

    auto *clockLabel = new QLabel(QTime::currentTime().toString("hh:mm AP"), statusBar);
    clockLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; font-weight: 600; font-family: monospace;");
    sbLayout->addWidget(clockLabel);

    auto *batteryWifi = new QLabel("100% 🔋  5G 📶", statusBar);
    batteryWifi->setStyleSheet("color: #10B981; font-size: 11px; font-weight: 600;");
    sbLayout->addWidget(batteryWifi);

    mainLayout->addWidget(statusBar);

    // 2. Installed Apps Header & Refresh Action
    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(4, 8, 4, 0);

    auto *titleLbl = new QLabel("INSTALLED ANDROID APPS", home);
    titleLbl->setStyleSheet("color: #00FF66; font-size: 12px; font-weight: 800; letter-spacing: 1.5px;");
    headerRow->addWidget(titleLbl);

    auto *countBadge = new QLabel(QString("%1 Apps").arg(m_installedApps.size()), home);
    countBadge->setStyleSheet("background-color: rgba(16, 185, 129, 0.15); color: #10B981; border: 1px solid rgba(16, 185, 129, 0.4); border-radius: 4px; padding: 2px 7px; font-size: 10px; font-weight: 700;");
    headerRow->addWidget(countBadge);

    headerRow->addStretch(1);

    auto *btnRefresh = new QPushButton("🔄 Refresh Apps", home);
    btnRefresh->setCursor(Qt::PointingHandCursor);
    btnRefresh->setStyleSheet(
        "QPushButton { background: #161F30; color: #9CA3AF; border: 1px solid #1F2937; border-radius: 4px; padding: 4px 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #1E293B; border-color: #10B981; color: #00FF66; }"
    );
    connect(btnRefresh, &QPushButton::clicked, this, &AndroidCanvas::refreshInstalledApps);
    headerRow->addWidget(btnRefresh);

    mainLayout->addLayout(headerRow);

    // 3. Grid Area for Dynamically Fetched Apps
    auto *scrollArea = new QScrollArea(home);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    m_appsGridContainer = new QWidget(scrollArea);
    m_appsGridContainer->setStyleSheet("background: transparent;");
    scrollArea->setWidget(m_appsGridContainer);

    mainLayout->addWidget(scrollArea, 1);

    return home;
}

void AndroidCanvas::refreshInstalledApps()
{
    m_installedApps.clear();

    // Scan ~/.local/share/applications/linstack.*.desktop
    QString appDir = QDir::homePath() + "/.local/share/applications";
    QDir dir(appDir);
    QStringList filters;
    filters << "linstack.*.desktop" << "linstack.*.desktop";
    dir.setNameFilters(filters);
    
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &fileInfo : list) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString name;
            QString exec;
            QString pkg;
            
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("Name=")) {
                    name = line.mid(5).trimmed();
                } else if (line.startsWith("Exec=")) {
                    exec = line.mid(5).trimmed();
                    if (exec.contains("app launch ")) {
                        QString p = exec.section("app launch ", 1, 1).section(" ", 0, 0);
                        if (!p.isEmpty()) pkg = p;
                    }
                }
            }
            
            if (name.isEmpty()) {
                name = fileInfo.baseName().remove("linstack.").remove("linstack.");
            }
            if (pkg.isEmpty()) {
                pkg = "com.linstack." + fileInfo.baseName().remove("linstack.").remove("linstack.").toLower();
            }

            // STRICTLY REMOVE: "Terminal", "Game Center", "POPULAR GAMES TO PLAY", etc.
            QString lowerName = name.toLower();
            if (lowerName.contains("terminal") ||
                lowerName.contains("game center") ||
                lowerName.contains("popular games") ||
                lowerName.contains("container ready")) {
                continue;
            }

            Theme::IconType iconType = Theme::IconType::AndroidRecent;
            QColor color = Theme::ColorEmerald;
            bool isGame = false;

            if (lowerName.contains("play store")) {
                iconType = Theme::IconType::PlayStore;
                color = Theme::ColorEmerald;
            } else if (lowerName.contains("settings")) {
                iconType = Theme::IconType::Settings;
                color = Theme::ColorMint;
            } else if (lowerName.contains("file") || lowerName.contains("document")) {
                iconType = Theme::IconType::FileManager;
                color = QColor(59, 130, 246);
            } else if (lowerName.contains("chrome") || lowerName.contains("browser")) {
                iconType = Theme::IconType::Display;
                color = QColor(249, 115, 22);
            } else {
                iconType = Theme::IconType::Gamepad;
                color = Theme::ColorNeon;
                isGame = true;
            }

            m_installedApps.append(AndroidAppEntry{pkg, name, iconType, color, isGame});
        }
    }

    rebuildAppsGrid();
}

void AndroidCanvas::setInstalledApps(const QList<AndroidAppEntry> &apps)
{
    m_installedApps = apps;
    rebuildAppsGrid();
}

void AndroidCanvas::addInstalledApp(const AndroidAppEntry &app)
{
    m_installedApps.append(app);
    rebuildAppsGrid();
}

void AndroidCanvas::rebuildAppsGrid()
{
    if (!m_appsGridContainer) return;

    qDeleteAll(m_appsGridContainer->children());

    auto *grid = new QGridLayout(m_appsGridContainer);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setSpacing(20);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    const int cols = 5;
    for (int i = 0; i < m_installedApps.size(); ++i) {
        int r = i / cols;
        int c = i % cols;
        grid->addWidget(createAppCard(m_installedApps[i]), r, c);
    }
}

QWidget *AndroidCanvas::createAppCard(const AndroidAppEntry &app)
{
    auto *card = new QPushButton(m_appsGridContainer);
    card->setFixedSize(110, 100);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(
        "QPushButton {"
        "  background-color: #111827;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 12px;"
        "  padding: 8px 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #161F30;"
        "  border: 1px solid #10B981;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1E293B;"
        "}"
    );

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(4, 6, 4, 6);
    cardLayout->setSpacing(6);
    cardLayout->setAlignment(Qt::AlignCenter);

    auto *iconLabel = new QLabel(card);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(Theme::renderIconPixmap(app.iconType, app.accentColor, 34));
    cardLayout->addWidget(iconLabel);

    auto *nameLabel = new QLabel(app.displayName, card);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("color: #E5E7EB; font-size: 11px; font-weight: 600;");
    cardLayout->addWidget(nameLabel);

    if (app.isGame) {
        auto *gameTag = new QLabel("120 FPS", card);
        gameTag->setAlignment(Qt::AlignCenter);
        gameTag->setStyleSheet("color: #00FF66; font-size: 9px; font-weight: 700; font-family: monospace;");
        cardLayout->addWidget(gameTag);
    }

    connect(card, &QPushButton::clicked, this, [this, app]() {
        handleAppCardClicked(app.packageName, app.displayName);
    });

    return card;
}

void AndroidCanvas::handleAppCardClicked(const QString &packageName, const QString &displayName)
{
    launchAndEmbedApp(packageName, displayName);
}

void AndroidCanvas::launchAndEmbedApp(const QString &packageName, const QString &displayName)
{
    m_activePackage = packageName;
    m_gameTitleBadge->setText(QString("Playing: %1 (%2)").arg(displayName).arg(packageName));

    // Force LinStack single-window mode before launching app
    QProcess::execute("linstack", QStringList() << "prop" << "set" << "persist.linstack.multi_windows" << "false");

    // Automatically load keymaps for this specific app
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setCurrentAppId(packageName);
        setKeymapperVisible(true);
    }

    // Emit intent launch request
    emit appLaunchRequested(packageName, displayName);

    // Switch central canvas to embedded app viewport
    m_simulatedActive = true;
    m_stack->setCurrentWidget(m_embedContainerWidget);

    showToast(QString("Launching %1 inside LinStack central canvas frame (Mesa DRI3)...").arg(displayName), 3500);

    QProcess::startDetached("linstack", QStringList() << "app" << "launch" << packageName);

    // Poll for created LinStack window surface to embed inside canvas
    QTimer::singleShot(1000, this, [this, displayName]() {
        QProcess proc;
        proc.start("wmctrl", QStringList() << "-l");
        if (proc.waitForFinished(2000)) {
            QString lines = QString::fromUtf8(proc.readAllStandardOutput());
            for (const QString &line : lines.split('\n')) {
                if (line.contains("LinStack", Qt::CaseInsensitive) || line.contains("Linstack", Qt::CaseInsensitive) || line.contains(displayName, Qt::CaseInsensitive)) {
                    QString idStr = line.section(' ', 0, 0).trimmed();
                    bool ok = false;
                    WId winId = static_cast<WId>(idStr.toULongLong(&ok, 16));
                    if (ok && winId != 0) {
                        embedWindow(winId);
                        break;
                    }
                }
            }
        }
    });
}

bool AndroidCanvas::embedWindow(WId winId)
{
    if (winId == 0) return false;

    releaseEmbeddedWindow();

    m_embeddedWindow = QWindow::fromWinId(winId);
    if (!m_embeddedWindow) {
        return false;
    }

    return embedWindow(m_embeddedWindow);
}

bool AndroidCanvas::embedWindow(QWindow *foreignWindow)
{
    if (!foreignWindow) return false;

    m_embeddedWindow = foreignWindow;
    m_windowContainer = QWidget::createWindowContainer(m_embeddedWindow, m_gameViewportWidget);
    m_windowContainer->setFocusPolicy(Qt::StrongFocus);

    m_gameViewportWidget->layout()->addWidget(m_windowContainer);
    m_stack->setCurrentWidget(m_embedContainerWidget);

    emit windowEmbedded(foreignWindow->winId());
    showToast("LinStack Native App Surface Embedded inside LinStack Frame");
    return true;
}

void AndroidCanvas::releaseEmbeddedWindow()
{
    if (m_windowContainer) {
        m_gameViewportWidget->layout()->removeWidget(m_windowContainer);
        m_windowContainer->deleteLater();
        m_windowContainer = nullptr;
    }

    if (m_embeddedWindow) {
        m_embeddedWindow = nullptr;
    }

    m_simulatedActive = false;
    m_stack->setCurrentWidget(m_homeWidget);
    setKeymapperVisible(false);
    emit windowReleased();
}

void AndroidCanvas::setOrientation(Orientation orientation)
{
    m_orientation = orientation;
    updateCanvasGeometry();
    showToast(orientation == Orientation::Landscape ? "Screen rotated to Landscape (16:9)"
                                                    : "Screen rotated to Portrait (9:16)");
}

void AndroidCanvas::setBaseResolution(int baseWidth, int baseHeight)
{
    m_baseWidth  = qMax(1, baseWidth);
    m_baseHeight = qMax(1, baseHeight);
    updateCanvasGeometry();
}

void AndroidCanvas::updateCanvasGeometry()
{
    // ── 1. Compute the letterboxed viewport rect ──────────────────────────────
    // Determine the effective aspect ratio.  When in Portrait mode the base
    // resolution axes are swapped so the locked ratio rotates with orientation.
    int arW = (m_orientation == Orientation::Landscape) ? m_baseWidth  : m_baseHeight;
    int arH = (m_orientation == Orientation::Landscape) ? m_baseHeight : m_baseWidth;

    const int cw = width();
    const int ch = height();

    // Scale-to-fit: find the largest rect with the locked ratio inside the widget
    int vpW, vpH;
    if (cw * arH > ch * arW) {
        // Container is wider than the target ratio → pillarbox (vertical bars)
        vpH = ch;
        vpW = vpH * arW / arH;
    } else {
        // Container is taller than the target ratio → letterbox (horizontal bars)
        vpW = cw;
        vpH = vpW * arH / arW;
    }

    // Centre the viewport inside the widget
    int vpX = (cw - vpW) / 2;
    int vpY = (ch - vpH) / 2;
    m_viewportRect = QRect(vpX, vpY, vpW, vpH);

    // ── 2. Reposition the stacked-widget (Android content) to the viewport ───
    if (m_stack) {
        m_stack->setGeometry(m_viewportRect);
    }

    // ── 3. Keymapper overlay always covers the viewport rect only ────────────
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setGeometry(m_viewportRect);

        // Notify the InputTranslator about the new effective pixel dimensions so
        // that relative→absolute coordinate conversion stays accurate.
        if (m_keymapperOverlay->translator()) {
            m_keymapperOverlay->translator()->setScreenGeometry(vpW, vpH);
        }
    }

    // ── 4. Toast stays at the bottom-centre of the viewport ──────────────────
    if (m_toastLabel) {
        m_toastLabel->adjustSize();
        m_toastLabel->move(
            vpX + (vpW - m_toastLabel->width())  / 2,
            vpY +  vpH - m_toastLabel->height() - 28
        );
    }
}

void AndroidCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCanvasGeometry();
}

void AndroidCanvas::showToast(const QString &message, int durationMs)
{
    m_toastLabel->setText(message);
    m_toastLabel->adjustSize();

    // Position toast at the bottom-centre of the letterboxed viewport
    const QRect &vp = m_viewportRect.isValid() ? m_viewportRect : rect();
    m_toastLabel->move(
        vp.x() + (vp.width()  - m_toastLabel->width())  / 2,
        vp.y() +  vp.height() - m_toastLabel->height() - 28
    );
    m_toastLabel->show();
    m_toastLabel->raise();
    m_toastTimer->start(durationMs);
}

void AndroidCanvas::setKeymapperVisible(bool visible)
{
    m_keymapperOverlay->setVisible(visible);
    if (visible) {
        m_keymapperOverlay->raise();
    }
}

void AndroidCanvas::setBridge(UInputTouchBridge *bridge)
{
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setBridge(bridge);
    }
}

bool AndroidCanvas::isKeymapperVisible() const
{
    return m_keymapperOverlay->isVisible();
}

void AndroidCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            if (url.toLocalFile().endsWith(".apk", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                m_isDragOver = true;
                update();
                return;
            }
        }
    }
    event->ignore();
}

void AndroidCanvas::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    m_isDragOver = false;
    update();
}

void AndroidCanvas::dropEvent(QDropEvent *event)
{
    m_isDragOver = false;
    update();

    if (event->mimeData()->hasUrls()) {
        for (const auto &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (path.endsWith(".apk", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                QFileInfo fi(path);
                emit apkDropped(path);

                // Add newly installed app dynamically to launcher
                AndroidAppEntry newApp;
                newApp.packageName = "com.custom." + fi.baseName().toLower();
                newApp.displayName = fi.baseName();
                newApp.iconType = Theme::IconType::InstallApk;
                newApp.accentColor = Theme::ColorNeon;
                newApp.isGame = true;
                addInstalledApp(newApp);

                showToast(QString("Installing APK: %1 via ADB / LXC...").arg(fi.fileName()), 4000);
                return;
            }
        }
    }
}

void AndroidCanvas::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // ── Letterbox / Pillarbox bars ───────────────────────────────────────────
    // Fill any area outside the locked-aspect-ratio viewport with the cyber-dark
    // background colour so the bars look intentional rather than transparent.
    if (m_viewportRect != rect()) {
        const QColor barColor(11, 15, 25); // Theme::ColorBgDark  #0B0F19
        p.setBrush(barColor);
        p.setPen(Qt::NoPen);

        // Top bar
        if (m_viewportRect.top() > 0) {
            p.drawRect(0, 0, width(), m_viewportRect.top());
        }
        // Bottom bar
        if (m_viewportRect.bottom() < height() - 1) {
            p.drawRect(0, m_viewportRect.bottom() + 1, width(), height() - m_viewportRect.bottom() - 1);
        }
        // Left bar
        if (m_viewportRect.left() > 0) {
            p.drawRect(0, 0, m_viewportRect.left(), height());
        }
        // Right bar
        if (m_viewportRect.right() < width() - 1) {
            p.drawRect(m_viewportRect.right() + 1, 0, width() - m_viewportRect.right() - 1, height());
        }

        // Thin emerald accent line around the viewport edge
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(31, 41, 55), 1)); // Theme::ColorBorder
        p.drawRect(m_viewportRect.adjusted(-1, -1, 0, 0));
    }

    // ── APK drag-and-drop overlay ────────────────────────────────────────────
    if (m_isDragOver) {
        p.fillRect(rect(), QColor(11, 15, 25, 230));

        QPen pen(Theme::ColorNeon, 3.0, Qt::DashLine);
        p.setPen(pen);
        p.drawRoundedRect(rect().adjusted(16, 16, -16, -16), 16, 16);

        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;

        p.setPen(Theme::ColorTextPrimary);
        QFont f = p.font();
        f.setPointSize(16);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(cx - 200, cy - 30, 400, 30), Qt::AlignCenter, "📦 Drop APK to Install");

        p.setPen(Theme::ColorMint);
        f.setPointSize(11);
        f.setBold(false);
        p.setFont(f);
        p.drawText(QRectF(cx - 200, cy + 5, 400, 24), Qt::AlignCenter, "Direct injection into LinStack container environment");
    }
}
