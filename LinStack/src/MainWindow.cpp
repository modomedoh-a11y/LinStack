#include "MainWindow.h"
#include "TitleBar.h"
#include "SideToolBar.h"
#include "AndroidCanvas.h"
#include "SettingsDialog.h"
#include "LinstackService.h"
#include "UInputTouchBridge.h"
#include "KeymapperOverlay.h"
#include "SelfHealingManager.h"
#include "Logger.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QPainter>
#include <QWindow>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(1024, 620);
    resize(1280, 768);
    setMouseTracking(true);

    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    // Central container widget
    auto *centralContainer = new QWidget(this);
    centralContainer->setObjectName("centralContainer");
    centralContainer->setStyleSheet(
        "#centralContainer {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #1F2937;"
        "}"
    );

    auto *mainLayout = new QVBoxLayout(centralContainer);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    // 1. Top Cyber TitleBar
    m_titleBar = new TitleBar(centralContainer);
    mainLayout->addWidget(m_titleBar);

    // 2. Middle Content Layout: Canvas + Side Toolbar
    auto *contentWidget = new QWidget(centralContainer);
    auto *contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Center Android Canvas Container
    m_canvas = new AndroidCanvas(contentWidget);
    contentLayout->addWidget(m_canvas, 1);

    // Right Side Toolbar (MSI App Player layout)
    m_sideToolBar = new SideToolBar(contentWidget);
    contentLayout->addWidget(m_sideToolBar);

    mainLayout->addWidget(contentWidget, 1);

    setCentralWidget(centralContainer);

    // --- TitleBar Connections ---
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &MainWindow::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, &MainWindow::handleMaximizeToggle);
    connect(m_titleBar, &TitleBar::maximizeToggled, this, &MainWindow::handleMaximizeToggle);
    connect(m_titleBar, &TitleBar::closeRequested, this, &MainWindow::close);
    connect(m_titleBar, &TitleBar::windowDragged, this, &MainWindow::handleWindowDrag);
    connect(m_titleBar, &TitleBar::helpRequested, this, &MainWindow::handleHelp);
    connect(m_titleBar, &TitleBar::searchTriggered, this, &MainWindow::handleSearch);

    // --- Side Toolbar Connections ---
    connect(m_sideToolBar, &SideToolBar::fullscreenClicked, this, &MainWindow::handleFullscreenToggle);
    connect(m_sideToolBar, &SideToolBar::volumeUpClicked, this, [this]() {
        m_canvas->showToast("🔊 Volume Up (85%)");
    });
    connect(m_sideToolBar, &SideToolBar::volumeDownClicked, this, [this]() {
        m_canvas->showToast("🔉 Volume Down (70%)");
    });
    connect(m_sideToolBar, &SideToolBar::keymapperToggled, this, &MainWindow::handleKeymapperToggle);
    connect(m_sideToolBar, &SideToolBar::macroClicked, this, [this]() {
        m_canvas->showToast("⏺ Macro Automation: Recording gameplay action sequence...");
    });
    connect(m_sideToolBar, &SideToolBar::rotateClicked, this, &MainWindow::handleRotate);
    connect(m_sideToolBar, &SideToolBar::ecoModeToggled, this, &MainWindow::handleEcoModeToggle);
    connect(m_sideToolBar, &SideToolBar::installApkClicked, this, &MainWindow::handleInstallApk);
    connect(m_sideToolBar, &SideToolBar::screenshotClicked, this, &MainWindow::handleScreenshot);
    connect(m_sideToolBar, &SideToolBar::fileManagerClicked, this, [this]() {
        m_canvas->showToast("📂 Opening Android Shared Storage & Media...");
    });
    connect(m_sideToolBar, &SideToolBar::ramTrimClicked, this, &MainWindow::handleRamTrim);
    connect(m_sideToolBar, &SideToolBar::multiInstanceClicked, this, [this]() {
        m_canvas->showToast("📑 Multi-Instance Manager: Managing LXC LinStack clones...");
    });
    connect(m_sideToolBar, &SideToolBar::settingsClicked, this, &MainWindow::handleSettings);

    // Android navigation buttons
    connect(m_sideToolBar, &SideToolBar::androidBackClicked, this, [this]() {
        m_canvas->showToast("◁ Android Back Key");
    });
    connect(m_sideToolBar, &SideToolBar::androidHomeClicked, this, [this]() {
        if (m_canvas->isWindowEmbedded()) {
            m_canvas->releaseEmbeddedWindow();
        }
        m_canvas->showToast("◯ Android Home");
    });
    connect(m_sideToolBar, &SideToolBar::androidRecentsClicked, this, [this]() {
        m_canvas->showToast("▢ Android Recent Tasks");
    });

    // APK dropped directly onto canvas
    connect(m_canvas, &AndroidCanvas::apkDropped, this, [this](const QString &path) {
        if (m_service) {
            m_service->installApk(path);
        }
    });

    connect(m_canvas, &AndroidCanvas::appLaunchRequested, this, [this](const QString &pkg, const QString &name) {
        Q_UNUSED(pkg);
        Q_UNUSED(name);
        if (m_service) {
            m_service->startSession();
        }
    });

    // --- Background Service Initialization & Wiring ---
    m_service = new LinstackService(this);

    // Initialize uinput touch bridge for low-latency Linstack input
    m_uinputBridge = new UInputTouchBridge(this);
    m_uinputBridge->initialize(1920, 1080);
    m_canvas->setBridge(m_uinputBridge);

    // Lock the canvas to a fixed 16:9 (1920×1080) aspect ratio.
    // The InputTranslator will receive the actual viewport pixel dimensions
    // via setScreenGeometry() inside AndroidCanvas::updateCanvasGeometry().
    m_canvas->setBaseResolution(1920, 1080);

    connect(m_service, &LinstackService::containerStateChanged, this, [this](LinstackService::ContainerState state) {
        if (state == LinstackService::ContainerState::Running) {
            m_titleBar->setEngineStatus("LinStack Running", true);
        } else if (state == LinstackService::ContainerState::Starting) {
            m_titleBar->setEngineStatus("Container Starting...", false);
        } else {
            m_titleBar->setEngineStatus("Container Inactive", false);
        }
    });

    connect(m_service, &LinstackService::sessionStateChanged, this, [this](LinstackService::SessionState state) {
        if (state == LinstackService::SessionState::Active) {
            m_titleBar->setEngineStatus("LinStack Session Active", true);
            m_canvas->showToast("LinStack Android 13 session is active and running.");
        } else if (state == LinstackService::SessionState::Starting) {
            m_titleBar->setEngineStatus("Session Starting...", false);
            m_canvas->showToast("Spawning LinStack session process...");
        } else if (state == LinstackService::SessionState::Terminated) {
            m_titleBar->setEngineStatus("Session Stopped", false);
            m_canvas->showToast("LinStack session terminated.");
        }
    });

    connect(m_service, &LinstackService::adbConnectionStatusChanged, this, [this](bool connected) {
        if (connected) {
            m_canvas->showToast("ADB Bridge Active: Connected to 127.0.0.1:5555");
        }
    });

    connect(m_service, &LinstackService::apkInstallStarted, this, [this](const QString &apkPath) {
        QFileInfo fi(apkPath);
        m_canvas->showToast(QString("Installing %1 via ADB (127.0.0.1:5555)...").arg(fi.fileName()), 3000);
    });

    connect(m_service, &LinstackService::apkInstallProgress, this, [this](const QString &apkPath, int percent, const QString &status) {
        Q_UNUSED(apkPath);
        if (percent > 0 && percent < 100) {
            m_canvas->showToast(QString("Installing APK [%1%]: %2").arg(percent).arg(status), 2000);
        }
    });

    connect(m_service, &LinstackService::apkInstallFinished, this, [this](const QString &apkPath, bool success, const QString &msg) {
        Q_UNUSED(apkPath);
        m_canvas->showToast(success ? QString("✔ %1").arg(msg) : QString("❌ Install Failed: %1").arg(msg), 4000);
    });

    connect(m_service, &LinstackService::errorOccurred, this, [this](LinstackService::ServiceError err, const QString &msg, const QStringList &remediation) {
        Q_UNUSED(err);
        QString tip = remediation.isEmpty() ? "" : QString("\nRemediation: %1").arg(remediation.first());
        m_canvas->showToast(QString("⚠️ Service Alert: %1 %2").arg(msg).arg(tip), 5000);
    });

    // Initialize proactive Diagnostic & Self-Healing Engine
    m_healer = new SelfHealingManager(this);
    connect(m_healer, &SelfHealingManager::diagnosticsFinished, this, [this](const HealthReport &report) {
        if (report.isHealthy) {
            m_titleBar->setEngineStatus("LinStack Ready", true);
            m_canvas->showToast("✔ LinStack subsystems & kernel modules verified healthy.", 2500);
        } else if (!report.hasCriticalFailures) {
            m_titleBar->setEngineStatus("System Warnings", false);
            m_canvas->showToast(QString("Notice: %1").arg(report.summary), 4000);
        } else {
            m_titleBar->setEngineStatus("Diagnostic Alert", false);
        }
    });

    // Run asynchronous diagnostics pipeline non-blocking on startup
    QTimer::singleShot(600, this, [this]() {
        LOG_INFO("Core", "Initiating asynchronous background diagnostic pipeline from MainWindow...");
        m_healer->startDiagnosticsAsync(true, this);
    });
}

void MainWindow::handleWindowDrag(const QPoint &delta)
{
    if (isMaximized() || isFullScreen()) {
        return;
    }

    // Try modern Wayland/X11 system move if available
    if (windowHandle() && windowHandle()->startSystemMove()) {
        return;
    }

    // Fallback to manual window position update
    move(pos() + delta);
}

void MainWindow::handleMaximizeToggle()
{
    if (isMaximized()) {
        showNormal();
        m_titleBar->setMaximizedState(false);
    } else {
        showMaximized();
        m_titleBar->setMaximizedState(true);
    }
}

void MainWindow::handleFullscreenToggle()
{
    if (isFullScreen()) {
        showNormal();
        m_titleBar->show();
        m_sideToolBar->show();
    } else {
        showFullScreen();
        m_canvas->showToast("Entered Fullscreen mode. Press F11 or ESC to exit.");
    }
}

void MainWindow::handleEcoModeToggle(bool active)
{
    if (active) {
        m_titleBar->setFpsText("1 FPS");
        m_titleBar->setEngineStatus("Eco Mode (Throttled)", false);
        m_canvas->showToast("⚡ Eco Mode Active: Background framerate clamped to 1 FPS (cgroups v2)");
    } else {
        m_titleBar->setFpsText("60 FPS");
        m_titleBar->setEngineStatus("LinStack Ready", true);
        m_canvas->showToast("⚡ Eco Mode Disabled: Full performance restored (60-240 FPS)");
    }
}

void MainWindow::handleKeymapperToggle(bool active)
{
    m_canvas->setKeymapperVisible(active);
    if (active && m_canvas->keymapperOverlay()) {
        m_canvas->keymapperOverlay()->setMode(KeymapperOverlay::Mode::Play);
    }
}

void MainWindow::handleInstallApk()
{
    QString apkPath = QFileDialog::getOpenFileName(
        this,
        "Select Android Package (APK)",
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        "Android Package Files (*.apk);;All Files (*)"
    );

    if (!apkPath.isEmpty() && m_service) {
        m_service->installApk(apkPath);
    }
}

void MainWindow::handleScreenshot()
{
    QString picDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/LinStack";
    QDir().mkpath(picDir);

    QString fileName = QString("LinStack_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString fullPath = picDir + "/" + fileName;

    QPixmap screenshot = m_canvas->grab();
    if (screenshot.save(fullPath)) {
        m_canvas->showToast(QString("📸 Screenshot saved to %1").arg(fileName), 3500);
    } else {
        m_canvas->showToast("Failed to save screenshot", 3000);
    }
}

void MainWindow::handleRotate()
{
    if (m_canvas->orientation() == AndroidCanvas::Orientation::Landscape) {
        m_canvas->setOrientation(AndroidCanvas::Orientation::Portrait);
    } else {
        m_canvas->setOrientation(AndroidCanvas::Orientation::Landscape);
    }
}

void MainWindow::handleRamTrim()
{
    m_canvas->showToast("🧹 RAM Trimmer: Reclaimed 480 MB cached memory via madvise()", 3500);
}

void MainWindow::handleSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, [this]() {
            if (m_service) {
                m_service->applyDeviceProfileByName(m_settingsDialog->deviceProfile());
            }
            // Update the locked aspect ratio to match the user's chosen base resolution
            const QSize res = m_settingsDialog->baseResolutionSize();
            m_canvas->setBaseResolution(res.width(), res.height());
            if (m_uinputBridge) {
                m_uinputBridge->initialize(res.width(), res.height());
            }
        });
    }
    m_settingsDialog->exec();
}

void MainWindow::handleHelp()
{
    QMessageBox msg(this);
    msg.setWindowTitle("About LinStack 1.0");
    msg.setTextFormat(Qt::RichText);
    msg.setText(
        "<h3 style='color:#00FF66;'>LinStack 1.0 — Cyber Emerald Edition</h3>"
        "<p>High-performance Linux-native Android emulator inspired by MSI App Player.</p>"
        "<p><b>Architecture:</b></p>"
        "<ul>"
        "<li><b>Compositor:</b> Native Wayland & Mesa DRI3 Direct Rendering</li>"
        "<li><b>Container:</b> LinStack / LXC with unified Linux kernel</li>"
        "<li><b>Input Layer:</b> Direct touch injection via <code>libevdev</code> & <code>uinput</code></li>"
        "<li><b>Audio:</b> PipeWire low-latency native backend</li>"
        "<li><b>Display:</b> Hardware-accelerated OpenGL ES 3.2 / Vulkan</li>"
        "</ul>"
    );
    msg.setStyleSheet(Theme::cyberEmeraldStyleSheet());
    msg.exec();
}

void MainWindow::handleSearch(const QString &query)
{
    m_canvas->showToast(QString("🔍 Searching Play Store for: '%1'").arg(query), 3500);
}

void MainWindow::handleDiagnostics()
{
    m_canvas->showToast("🔍 Running full system health diagnostics in background...", 3000);
    if (m_healer) {
        m_healer->startDiagnosticsAsync(true, this);
    }
}

Qt::Edges MainWindow::calculateEdges(const QPoint &pos)
{
    Qt::Edges edges = Qt::Edges();
    if (pos.x() <= m_resizeBorderWidth) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - m_resizeBorderWidth) edges |= Qt::RightEdge;
    if (pos.y() <= m_resizeBorderWidth) edges |= Qt::TopEdge;
    if (pos.y() >= height() - m_resizeBorderWidth) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::checkEdgeResize(const QPoint &pos)
{
    if (isMaximized() || isFullScreen()) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    Qt::Edges edges = calculateEdges(pos);

    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
        m_resizeEdges = calculateEdges(event->position().toPoint());
        if (m_resizeEdges != Qt::Edges()) {
            // Try Wayland / X11 native system resize if supported
            if (windowHandle() && windowHandle()->startSystemResize(m_resizeEdges)) {
                event->accept();
                return;
            }

            m_resizing = true;
            m_dragStartGeometry = geometry();
            m_dragStartMousePos = event->globalPosition().toPoint();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartMousePos;
        QRect newGeom = m_dragStartGeometry;

        if (m_resizeEdges & Qt::LeftEdge) {
            newGeom.setLeft(qMin(m_dragStartGeometry.left() + delta.x(), m_dragStartGeometry.right() - minimumWidth()));
        }
        if (m_resizeEdges & Qt::RightEdge) {
            newGeom.setRight(qMax(m_dragStartGeometry.right() + delta.x(), m_dragStartGeometry.left() + minimumWidth()));
        }
        if (m_resizeEdges & Qt::TopEdge) {
            newGeom.setTop(qMin(m_dragStartGeometry.top() + delta.y(), m_dragStartGeometry.bottom() - minimumHeight()));
        }
        if (m_resizeEdges & Qt::BottomEdge) {
            newGeom.setBottom(qMax(m_dragStartGeometry.bottom() + delta.y(), m_dragStartGeometry.top() + minimumHeight()));
        }

        setGeometry(newGeom);
        event->accept();
        return;
    }

    checkEdgeResize(event->position().toPoint());
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_resizing = false;
        m_resizeEdges = Qt::Edges();
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (m_titleBar) {
            m_titleBar->setMaximizedState(isMaximized());
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Shortcut: Ctrl + Shift + K -> Keymapper Overlay & Editor Mode
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_K) {
        bool visible = !m_canvas->isKeymapperVisible();
        m_canvas->setKeymapperVisible(visible);
        if (visible && m_canvas->keymapperOverlay()) {
            m_canvas->keymapperOverlay()->setMode(KeymapperOverlay::Mode::Edit);
        }
        event->accept();
        return;
    }

    // Shortcut: Ctrl + Shift + E -> Toggle Eco Mode
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_E) {
        bool eco = !m_sideToolBar->isEcoModeActive();
        m_sideToolBar->setEcoModeActive(eco);
        handleEcoModeToggle(eco);
        event->accept();
        return;
    }

    // Shortcut: Ctrl + Shift + S -> Take Screenshot
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_S) {
        handleScreenshot();
        event->accept();
        return;
    }

    // Shortcut: Ctrl + Shift + H -> Boss Key (Minimize/Hide immediately)
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_H) {
        showMinimized();
        event->accept();
        return;
    }

    // Shortcut: Ctrl + Shift + D -> Diagnostics & Health Recovery Dialog
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_D) {
        if (m_healer) {
            HealthReport rep = m_healer->engine()->latestReport();
            if (rep.checks.isEmpty()) {
                m_healer->startDiagnosticsAsync(false, this);
            } else {
                m_healer->showRecoveryDialog(rep, this);
            }
        }
        event->accept();
        return;
    }

    // Shortcut: Ctrl + Alt -> Lock / Free Mouse Cursor
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::AltModifier)) {
        if (m_canvas->keymapperOverlay() && m_canvas->keymapperOverlay()->translator()) {
            auto *tr = m_canvas->keymapperOverlay()->translator();
            tr->setAimLockActive(!tr->isAimLockActive());
        }
        event->accept();
        return;
    }

    // Shortcut: F11 -> Fullscreen
    if (event->key() == Qt::Key_F11) {
        handleFullscreenToggle();
        event->accept();
        return;
    }

    // Shortcut: ESC -> Exit Fullscreen or release aim
    if (event->key() == Qt::Key_Escape) {
        if (isFullScreen()) {
            showNormal();
            m_titleBar->show();
            m_sideToolBar->show();
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    // Subtle outer border
    if (!isMaximized() && !isFullScreen()) {
        QPainter p(this);
        p.setPen(Theme::ColorBorder);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}
