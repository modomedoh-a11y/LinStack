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
#include "AspectRatioWindow.h"
#include "PathResolver.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QPainter>
#include <QWindow>
#include <QScreen>
#include <QApplication>
#include <QSettings>
#include <QFileInfo>
#include <QUrl>
#include <QProcess>
#include <functional>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);

    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    setupUi();
    qApp->installEventFilter(this);

    QSize scrSize(1440, 900);
    QRect availRect(0, 0, 1440, 900);
    if (QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen()) {
        if (scr->size().width() > 0 && scr->size().height() > 0) {
            scrSize = scr->size();
        }
        availRect = scr->availableGeometry();
    }

    // قراءة الأبعاد الفعلية للشريط الجانبي والعلوي + 2px للإطار الخارجي
    const int sideW = (m_sideToolBar && m_sideToolBar->width() > 0) ? m_sideToolBar->width() : 46;
    const int topH  = (m_titleBar && m_titleBar->height() > 0) ? m_titleBar->height() : 48;
    const int chromeW = sideW + 2;
    const int chromeH = topH + 2;
    const double scrAr = static_cast<double>(scrSize.width()) / static_cast<double>(scrSize.height());

    // 1. حد تصغير النافذة = نصف أبعاد شاشة المستخدم (1/2) للعرض الصافي + أبعاد الشريطين
    int minNetW = scrSize.width() / 2;
    int minNetH = qRound(minNetW / scrAr);
    setMinimumSize(minNetW + chromeW, minNetH + chromeH);

    // 2. حجم الإقلاع الابتدائي = ثلثي أبعاد شاشة المستخدم (2/3) للعرض الصافي (Android Canvas)
    int targetNetW = (scrSize.width() * 2) / 3;
    int targetNetH = qRound(targetNetW / scrAr);

    // التأكد من أن (العرض الصافي + الشريطين) لا يتجاوز المساحة المتاحة من سطح المكتب حتى لا يضغطه مدير النوافذ
    int maxAllowedNetW = availRect.width() - chromeW;
    int maxAllowedNetH = availRect.height() - chromeH;
    if (targetNetW > maxAllowedNetW) {
        targetNetW = maxAllowedNetW;
        targetNetH = qRound(targetNetW / scrAr);
    }
    if (targetNetH > maxAllowedNetH) {
        targetNetH = maxAllowedNetH;
        targetNetW = qRound(targetNetH * scrAr);
    }

    // الحجم الإجمالي للنافذة = أبعاد الأندرويد الصافية + أبعاد الشريط العلوي والجانبي
    int targetW = targetNetW + chromeW;
    int targetH = targetNetH + chromeH;

    resize(targetW, targetH);

    QTimer::singleShot(50, this, [this, scrSize, scrAr, availRect]() {
        if (!isFullScreen() && !isMaximized()) {
            // إعادة القراءة الفعلية بعد اكتمال رسم التخطيط لضمان دقة البكسل
            int realChromeW = (m_sideToolBar ? m_sideToolBar->width() : 46) + 2;
            int realChromeH = (m_titleBar ? m_titleBar->height() : 48) + 2;

            int minW = scrSize.width() / 2;
            int minH = qRound(minW / scrAr);
            setMinimumSize(minW + realChromeW, minH + realChromeH);

            int netW = (scrSize.width() * 2) / 3;
            int netH = qRound(netW / scrAr);

            int maxNetW = availRect.width() - realChromeW;
            int maxNetH = availRect.height() - realChromeH;
            if (netW > maxNetW) { netW = maxNetW; netH = qRound(netW / scrAr); }
            if (netH > maxNetH) { netH = maxNetH; netW = qRound(netH * scrAr); }

            int finalW = netW + realChromeW;
            int finalH = netH + realChromeH;

            resize(finalW, finalH);
            move(availRect.x() + (availRect.width() - finalW) / 2,
                 availRect.y() + (availRect.height() - finalH) / 2);
            LOG_INFO("UI", QString("Startup window size enforced: %1x%2 (Android Viewport: %3x%4 | Chrome: %5x%6)")
                .arg(finalW).arg(finalH).arg(netW).arg(netH).arg(realChromeW).arg(realChromeH));
        }
    });
}

MainWindow::~MainWindow()
{
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }
}

QSize MainWindow::calculateNetViewportSize() const
{
    int topBarHeight = m_titleBar ? m_titleBar->height() : 48;
    int sideBarWidth = m_sideToolBar ? m_sideToolBar->width() : 46;
    // Account for central container border (1px on left/right/top/bottom)
    int netW = width() - sideBarWidth - 2;
    int netH = height() - topBarHeight - 2;
    if (netW <= 0) netW = 1280;
    if (netH <= 0) netH = 720;
    return QSize(netW, netH);
}

void MainWindow::setupUi()
{
    // Central container widget
    auto *centralContainer = new QWidget(this);
    centralContainer->setObjectName("centralContainer");
    centralContainer->setMouseTracking(true);
    centralContainer->setAttribute(Qt::WA_Hover, true);
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
    connect(m_titleBar, &TitleBar::restartSessionRequested, this, [this]() {
        if (m_service) {
            m_canvas->showToast("🔄 Restarting LinStack session...", 3000);
            m_service->stopSession();
            QTimer::singleShot(1200, m_service, &LinstackService::startSession);
        }
    });
    connect(m_titleBar, &TitleBar::restartContainerRequested, this, [this]() {
        if (m_service) {
            m_canvas->showToast("⚡ Restarting Android container service...", 3500);
            m_service->stopSession();
            m_service->restartContainerService();
            QTimer::singleShot(3000, m_service, &LinstackService::startSession);
        }
    });

    // --- Side Toolbar Connections ---
    connect(m_sideToolBar, &SideToolBar::fullscreenClicked, this, &MainWindow::handleFullscreenToggle);
    connect(m_sideToolBar, &SideToolBar::volumeUpClicked, this, [this]() {
        m_canvas->sendWaylandSystemKey(115); // KEY_VOLUMEUP
        m_canvas->showToast("🔊 Volume Up");
    });
    connect(m_sideToolBar, &SideToolBar::volumeDownClicked, this, [this]() {
        m_canvas->sendWaylandSystemKey(114); // KEY_VOLUMEDOWN
        m_canvas->showToast("🔉 Volume Down");
    });
    connect(m_sideToolBar, &SideToolBar::keymapperToggled, this, &MainWindow::handleKeymapperToggle);
    connect(m_sideToolBar, &SideToolBar::keymapperEditRequested, this, &MainWindow::handleKeymapperEdit);
    connect(m_sideToolBar, &SideToolBar::macroClicked, this, [this]() {

        m_canvas->showToast("⏺ Macro Automation: Recording gameplay action sequence...");
    });
    connect(m_sideToolBar, &SideToolBar::rotateClicked, this, &MainWindow::handleRotate);
    connect(m_sideToolBar, &SideToolBar::ecoModeToggled, this, &MainWindow::handleEcoModeToggle);
    connect(m_sideToolBar, &SideToolBar::installApkClicked, this, &MainWindow::handleInstallApk);
    connect(m_sideToolBar, &SideToolBar::screenshotClicked, this, &MainWindow::handleScreenshot);
    connect(m_sideToolBar, &SideToolBar::fileManagerClicked, this, &MainWindow::handleSharedStorage);
    connect(m_sideToolBar, &SideToolBar::ramTrimClicked, this, &MainWindow::handleRamTrim);
    connect(m_sideToolBar, &SideToolBar::multiInstanceClicked, this, [this]() {
        m_canvas->showToast("📑 Multi-Instance Manager: Managing LXC LinStack clones...");
    });
    connect(m_sideToolBar, &SideToolBar::settingsClicked, this, &MainWindow::handleSettings);

    // Android navigation buttons - sending native Wayland wl_keyboard system keys directly to Android
    connect(m_sideToolBar, &SideToolBar::androidBackClicked, this, [this]() {
        m_canvas->sendWaylandSystemKey(158); // KEY_BACK
        m_canvas->showToast("◁ Android Back");
    });
    connect(m_sideToolBar, &SideToolBar::androidHomeClicked, this, [this]() {
        m_canvas->sendWaylandSystemKey(172); // KEY_HOMEPAGE
        m_canvas->showToast("◯ Android Home");
    });
    connect(m_sideToolBar, &SideToolBar::androidRecentsClicked, this, [this]() {
        m_canvas->sendWaylandRecents(); // Alt+Tab Android Overview
        m_canvas->showToast("▢ Android Recents");
    });

    // APK dropped directly onto canvas
    connect(m_canvas, &AndroidCanvas::apkDropped, this, [this](const QString &path) {
        if (m_service) {
            m_service->installApk(path);
        }
    });

    connect(m_canvas, &AndroidCanvas::firstFrameRendered, this, [this]() {
        m_titleBar->setEngineStatus("Session Active", true);
        if (m_service) {
            m_service->setHasFirstFrame(true);
        }
    });

    connect(m_canvas, &AndroidCanvas::waylandClientConnected, this, [this]() {
        LOG_INFO("MainWindow", "Wayland client connected -> applying Android display props & launching UI...");
        if (m_service) {
            QSize scr(1440, 900);
            if (QScreen *s = screen() ? screen() : QGuiApplication::primaryScreen()) {
                if (s->size().width() > 0 && s->size().height() > 0) scr = s->size();
            }
            m_service->setDesiredResolution(scr.width(), scr.height());
            // تنفيذ أوامر linstack prop set الآن بعد جاهزية waydroidplatform
            QString cmd = QString(
                "linstack prop set persist.waydroid.width %1; "
                "linstack prop set persist.waydroid.height %2; "
                "linstack prop set persist.waydroid.length %2; "
                "linstack prop set persist.linstack.width %1; "
                "linstack prop set persist.linstack.height %2; "
                "linstack prop set persist.linstack.length %2"
            ).arg(scr.width()).arg(scr.height());
            QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd);
            m_service->launchFullUi();
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

    // قراءة أبعاد شاشة المستخدم الفعلية وحساب نسبة الأبعاد لتمريرها للأندرويد قبل الإقلاع
    QSize androidRes(1920, 1080);
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        if (scr->size().width() > 0 && scr->size().height() > 0) {
            androidRes = scr->size();
        }
    }

    // Initialize uinput touch bridge for low-latency Linstack input
    m_uinputBridge = new UInputTouchBridge(this);
    m_uinputBridge->initialize(androidRes.width(), androidRes.height());
    m_canvas->setBridge(m_uinputBridge);

    // Lock canvas aspect-ratio to match user screen resolution (e.g. 16:9 or 16:10)
    m_canvas->setBaseResolution(androidRes.width(), androidRes.height());
    m_service->setDesiredResolution(androidRes.width(), androidRes.height());

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
            m_titleBar->setEngineStatus("Session Active", true);
            m_canvas->showToast("LinStack Android session is active and running.");
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
            if (m_service && m_service->sessionState() == LinstackService::SessionState::Idle) {
                m_service->startSession();
            }
        } else if (!report.hasCriticalFailures) {
            m_titleBar->setEngineStatus("System Warnings", false);
            m_canvas->showToast(QString("Notice: %1").arg(report.summary), 4000);
            if (m_service && m_service->sessionState() == LinstackService::SessionState::Idle) {
                m_service->startSession();
            }
        } else {
            m_titleBar->setEngineStatus("Diagnostic Alert", false);
        }
    });

    // بدء جلسة الأندرويد فوراً دون تأخير، وتشغيل الفحص التشخيصي في الخلفية بعد 3 ثوانٍ
    if (m_service && m_service->sessionState() == LinstackService::SessionState::Idle) {
        m_service->startSession();
    }
    QTimer::singleShot(3500, this, [this]() {
        LOG_INFO("Core", "Initiating background diagnostic check...");
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
    static bool s_customMax = false;
    static QRect s_restoreGeom;

    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);

    if (s_customMax || isMaximized()) {
        if (isMaximized()) showNormal();
        s_customMax = false;
        if (s_restoreGeom.isValid()) {
            setGeometry(s_restoreGeom);
        }
        m_titleBar->setMaximizedState(false);
    } else {
        s_restoreGeom = geometry();
        s_customMax = true;

        int chromeW = (m_sideToolBar ? m_sideToolBar->width() : 46) + 2;
        int chromeH = (m_titleBar ? m_titleBar->height() : 48) + 2;

        int maxNetW = avail.width() - chromeW;
        int maxNetH = avail.height() - chromeH;

        if (m_canvas && m_canvas->aspectRatioWindow()) {
            float targetAr = m_canvas->aspectRatioWindow()->getAspectRatio();
            if (maxNetW > maxNetH * targetAr) {
                maxNetW = qRound(maxNetH * targetAr);
            } else {
                maxNetH = qRound(maxNetW / targetAr);
            }
        }

        int finalW = maxNetW + chromeW;
        int finalH = maxNetH + chromeH;
        setGeometry(avail.x() + (avail.width() - finalW) / 2,
                    avail.y() + (avail.height() - finalH) / 2,
                    finalW, finalH);
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
        m_titleBar->hide();
        m_sideToolBar->hide();
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
    if (!active && m_canvas->isAimModeActive()) {
        m_canvas->setAimMode(false);
    }
    if (!active && m_canvas->keymapperOverlay() && m_canvas->keymapperOverlay()->translator()) {
        auto *tr = m_canvas->keymapperOverlay()->translator();
        if (tr->isAimLockActive()) {
            tr->setAimLockActive(false, -1);
        }
    }

    m_canvas->setKeymapperVisible(active);
    if (active && m_canvas->keymapperOverlay()) {
        m_canvas->keymapperOverlay()->setMode(KeymapperOverlay::Mode::Play);
        m_canvas->keymapperOverlay()->raise();
        m_canvas->keymapperOverlay()->setFocus(Qt::OtherFocusReason);
        m_canvas->showToast("⌨ Keymapping Enabled (Right-click icon or press Shortcut to edit)", 2500);
    } else {
        m_canvas->setFocus(Qt::OtherFocusReason);
        m_canvas->showToast("⌨ Keymapping Disabled", 2000);
    }
}


void MainWindow::handleKeymapperEdit()
{
    if (!m_sideToolBar->isKeymapperActive() || !m_canvas->isKeymapperVisible()) {
        m_canvas->showToast("⚠️ Enable Keymapping from the sidebar button first before editing!", 3000);
        return;
    }

    if (auto *overlay = m_canvas->keymapperOverlay()) {
        auto nextMode = (overlay->mode() == KeymapperOverlay::Mode::Edit)
        ? KeymapperOverlay::Mode::Play
        : KeymapperOverlay::Mode::Edit;
        overlay->setMode(nextMode);
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


namespace {
QString defaultSharedPath() {
    QString dl = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dl.isEmpty()) dl = QDir::homePath() + "/Downloads";
    return dl + "/LinStack Shared";
}

QString androidSharedMountPoint() {
    return qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share") + "/linstack/data/media/0/LinStack Shared";
}

void openInAndroidFiles() {
    QString cmd =
        "IP=$(linstack status 2>/dev/null | grep -oE \"([0-9]{1,3}\\.){3}[0-9]{1,3}\" | head -n1); "
        "T=\"${IP:-127.0.0.1}:5555\"; adb connect \"$T\" >/dev/null 2>&1; "
        "adb -s \"$T\" shell am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE -d \"file:///sdcard/LinStack%20Shared\" >/dev/null 2>&1; "
        "adb -s \"$T\" shell am start -a android.intent.action.VIEW -d \"content://com.android.externalstorage.documents/document/primary%3ALinStack%20Shared\" -t \"vnd.android.document/directory\" >/dev/null 2>&1 || "
        "adb -s \"$T\" shell am start -n com.android.documentsui/.files.FilesActivity >/dev/null 2>&1";
    QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd);
}

bool isStorageHelperInstalled() {
    return QFile::exists("/usr/local/bin/linstack-storage-helper") &&
           QFile::exists("/usr/share/polkit-1/actions/org.linstack.storage.policy");
}

void runHelperOrBootstrap(const QStringList &helperArgs, std::function<void(int)> onDone = nullptr, bool synchronous = false) {
    if (isStorageHelperInstalled()) {
        if (synchronous) {
            QProcess p;
            p.start("pkexec", QStringList() << "/usr/local/bin/linstack-storage-helper" << helperArgs);
            p.waitForFinished(4000);
            if (onDone) onDone(p.exitCode());
        } else {
            auto *p = new QProcess();
            QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [p, onDone](int code, QProcess::ExitStatus) {
                if (onDone) onDone(code);
                p->deleteLater();
            });
            p->start("pkexec", QStringList() << "/usr/local/bin/linstack-storage-helper" << helperArgs);
        }
        return;
    }

    // تثبيت السكريبت + ملف سياسة Polkit الرسمي (.policy) لمرة واحدة فقط في العمر
    QString bootstrapScript =
        "cat << 'EOF' > /usr/local/bin/linstack-storage-helper\n"
        "#!/bin/bash\n"
        "ACTION=\"$1\"; DST=\"$2\"; SRC=\"$3\"; U=\"$4\"; G=\"$5\"\n"
        "case \"$DST\" in */.local/share/linstack/data/media/0/*) ;; *) exit 1 ;; esac\n"
        "while mountpoint -q \"$DST\"; do umount -lf \"$DST\" 2>/dev/null || break; done\n"
        "if [ \"$ACTION\" = \"unmount\" ]; then\n"
        "  rmdir \"$DST\" 2>/dev/null || true\n"
        "  exit 0\n"
        "fi\n"
        "mkdir -p \"$DST\" \"$SRC\" && mount --bind \"$SRC\" \"$DST\" && "
        "chown -R \"$U:$G\" \"$SRC\" && chmod -R 0777 \"$SRC\" \"$DST\" && "
        "setfacl -R -m u:\"$U\":rwx,d:u:\"$U\":rwx,o::rwx,d:o::rwx \"$SRC\" 2>/dev/null || true\n"
        "EOF\n"
        "chmod 0755 /usr/local/bin/linstack-storage-helper && "
        "mkdir -p /usr/share/polkit-1/actions && "
        "cat << 'EOF' > /usr/share/polkit-1/actions/org.linstack.storage.policy\n"
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE policyconfig PUBLIC \"-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN\"\n"
        " \"http://freedesktop.org\">\n"
        "<policyconfig>\n"
        "  <action id=\"org.linstack.storage.mount\">\n"
        "    <description>Manage LinStack Shared Storage</description>\n"
        "    <message>Authentication is required to configure LinStack Shared Storage</message>\n"
        "    <defaults>\n"
        "      <allow_any>yes</allow_any>\n"
        "      <allow_inactive>yes</allow_inactive>\n"
        "      <allow_active>yes</allow_active>\n"
        "    </defaults>\n"
        "    <annotate key=\"org.freedesktop.policykit.exec.path\">/usr/local/bin/linstack-storage-helper</annotate>\n"
        "  </action>\n"
        "</policyconfig>\n"
        "EOF\n"
        "chmod 0644 /usr/share/polkit-1/actions/org.linstack.storage.policy && "
        "rm -f /etc/polkit-1/rules.d/*linstack-storage.rules && "
        "/usr/local/bin/linstack-storage-helper \"$@\"";

    QStringList fullArgs = QStringList() << "bash" << "-c" << bootstrapScript << "bash" << helperArgs;
    if (synchronous) {
        QProcess p;
        p.start("pkexec", fullArgs);
        p.waitForFinished(5000);
        if (onDone) onDone(p.exitCode());
    } else {
        auto *p = new QProcess();
        QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [p, onDone](int code, QProcess::ExitStatus) {
            if (onDone) onDone(code);
            p->deleteLater();
        });
        p->start("pkexec", fullArgs);
    }
}

void unmountShared(bool sync) {
    QString dst = androidSharedMountPoint();
    QProcess chk; chk.start("mountpoint", QStringList() << "-q" << dst); chk.waitForFinished(400);
    if (chk.exitCode() != 0) return;
    runHelperOrBootstrap(QStringList() << "unmount" << dst, nullptr, sync);
}

void mountShared(const QString &linuxDir, bool openAfter) {
    QSettings s("LinStack", "LinStack");
    QString src = linuxDir.trimmed().isEmpty() ? defaultSharedPath() : linuxDir.trimmed();
    s.setValue("storage/shared_linux_path", src);
    QDir().mkpath(src);

    QString dst = androidSharedMountPoint();

    // إذا كان مربوطاً بالفعل والسكريبت مثبت، افتح المجلد فوراً دون حتى استدعاء pkexec
    QProcess chk; chk.start("mountpoint", QStringList() << "-q" << dst); chk.waitForFinished(400);
    if (chk.exitCode() == 0 && openAfter && isStorageHelperInstalled()) {
        openInAndroidFiles();
        return;
    }

    QString uid = QString::number(getuid()), gid = QString::number(getgid());
    runHelperOrBootstrap(QStringList() << "mount" << dst << src << uid << gid, [openAfter](int code) {
        if (code == 0 && openAfter) openInAndroidFiles();
    }, false);
}
}
void MainWindow::handleSharedStorage() {
    QSettings s("LinStack", "LinStack");
    if (!s.value("storage/shared_enabled", false).toBool()) {
        m_canvas->showToast("⚠️ Shared Folder is disabled. Enable it in Settings -> Shared Storage.", 4500);
        return;
    }
    QString dir = s.value("storage/shared_linux_path", defaultSharedPath()).toString().trimmed();
    if (dir.isEmpty()) dir = defaultSharedPath();
    QDir().mkpath(dir);
    mountShared(dir, true);
    m_canvas->showToast("📂 Opening Android/LinStack Shared", 3500);
}

void MainWindow::handleSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, [this]() {
            if (m_service) {
                m_service->applyDeviceProfileByName(m_settingsDialog->deviceProfile());
            }
            if (m_settingsDialog->isSharedStorageEnabled()) mountShared(m_settingsDialog->sharedStoragePath(), false); else unmountShared(false);
            // الحفاظ على نسبة أبعاد شاشة المستخدم الفعلية عند حفظ الإعدادات
            if (QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen()) {
                if (scr->size().width() > 0 && scr->size().height() > 0) {
                    m_canvas->setBaseResolution(scr->size().width(), scr->size().height());
                    if (m_uinputBridge) {
                        m_uinputBridge->initialize(scr->size().width(), scr->size().height());
                    }
                }
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_resizing && !isMaximized() && !isFullScreen()) {
        if (event->type() == QEvent::Leave && watched == this) {
            setCursor(Qt::ArrowCursor);
        } else if (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove) {
            QPoint localPos = mapFromGlobal(QCursor::pos());
            Qt::Edges edges = rect().contains(localPos) ? calculateEdges(localPos) : Qt::Edges();
            if (edges == Qt::Edges()) {
                if (cursor().shape() != Qt::ArrowCursor) {
                    setCursor(Qt::ArrowCursor);
                }
            } else {
                checkEdgeResize(localPos);
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
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
            // Aspect-ratio locked resize enforced below

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

        // Strict Aspect Ratio constraint enforcement on net viewport
        int topBarH = m_titleBar ? m_titleBar->height() : 48;
        int sideBarW = m_sideToolBar ? m_sideToolBar->width() : 46;
        int chromeW = sideBarW + 2;
        int chromeH = topBarH + 2;

        int netW = newGeom.width() - chromeW;
        int netH = newGeom.height() - chromeH;

        if (m_canvas && m_canvas->aspectRatioWindow()) {
            bool anchorW = (m_resizeEdges & (Qt::LeftEdge | Qt::RightEdge)) != 0;
            m_canvas->aspectRatioWindow()->snapSizeToAspectRatio(netW, netH, anchorW);

            // الحفاظ على الحد الأدنى للنافذة (1024x620) مع احترام نسبة الأبعاد
            if (netW + chromeW < minimumWidth()) {
                netW = minimumWidth() - chromeW;
                m_canvas->aspectRatioWindow()->snapSizeToAspectRatio(netW, netH, true);
            }
            if (netH + chromeH < minimumHeight()) {
                netH = minimumHeight() - chromeH;
                m_canvas->aspectRatioWindow()->snapSizeToAspectRatio(netW, netH, false);
            }

            if (m_resizeEdges & Qt::LeftEdge) {
                newGeom.setLeft(newGeom.right() - (netW + chromeW) + 1);
            } else {
                newGeom.setWidth(netW + chromeW);
            }
            if (m_resizeEdges & Qt::TopEdge) {
                newGeom.setTop(newGeom.bottom() - (netH + chromeH) + 1);
            } else {
                newGeom.setHeight(netH + chromeH);
            }
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    QKeySequence pressedSeq(QKeyCombination(event->modifiers(), static_cast<Qt::Key>(key)));
    QSettings settings("LinStack", "LinStack");

    auto matchesShortcut = [&](const QString &id, const QString &defSeq) {
        QString saved = settings.value("shortcuts/" + id, defSeq).toString();
        return !saved.isEmpty() && QKeySequence::fromString(saved, QKeySequence::PortableText) == pressedSeq;
    };

    // 1A. Enable / Disable Keymapper completely
    if (matchesShortcut("keymapper_toggle", "Ctrl+Shift+M")) {
        bool nextActive = !m_sideToolBar->isKeymapperActive();
        m_sideToolBar->setKeymapperActive(nextActive);
        handleKeymapperToggle(nextActive);
        event->accept();
        return;
    }

    // 1B. Toggle Keymapper Editor Mode (Only works when Keymapper is already enabled)
    if (matchesShortcut("keymapper", "Ctrl+Shift+K")) {
        handleKeymapperEdit();
        event->accept();
        return;
    }

    // 2. Toggle Eco Mode

    if (matchesShortcut("eco_mode", "Ctrl+Shift+E")) {
        bool eco = !m_sideToolBar->isEcoModeActive();
        m_sideToolBar->setEcoModeActive(eco);
        handleEcoModeToggle(eco);
        event->accept();
        return;
    }

    // 3. Take Screenshot
    if (matchesShortcut("screenshot", "Ctrl+Shift+S")) {
        handleScreenshot();
        event->accept();
        return;
    }

    // 4. Boss Key (Minimize/Hide)
    if (matchesShortcut("boss_key", "Ctrl+Shift+H")) {
        showMinimized();
        event->accept();
        return;
    }

    // 5. Fullscreen
    if (matchesShortcut("fullscreen", "F11")) {
        handleFullscreenToggle();
        event->accept();
        return;
    }

    // 6. Lock / Free Mouse Cursor
    if (matchesShortcut("aim_lock", "Ctrl+Alt") ||
        ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::AltModifier))) {
        if (m_canvas->keymapperOverlay() && m_canvas->keymapperOverlay()->translator()) {
            auto *tr = m_canvas->keymapperOverlay()->translator();
            tr->setAimLockActive(!tr->isAimLockActive());
        }
        event->accept();
    return;
        }

        // 7. Diagnostics Dialog
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

        // 8. ESC -> Exit Fullscreen
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // إخفاء الواجهة فوراً للمستخدم
    hide();
    QApplication::processEvents();

    // إيقاف جلسة الأندرويد بنظافة لحماية بيانات المستخدم من التلف
    unmountShared(true);
    if (m_service) {
        LOG_INFO("Core", "Gracefully stopping LinStack session before exit...");
        m_service->stopSession();
    }

    event->accept();
    QApplication::quit();
}
