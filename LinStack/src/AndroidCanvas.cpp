#include <QDir>
#include <QSettings>
#include <QTextStream>
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
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTime>
#include <algorithm>
#include <vector>
#include <cstdint>

// EGL cross-context texture sharing (must come before wlroots headers)
#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#ifndef EGL_NO_PLATFORM_SPECIFIC_TYPES
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// DRM fourcc format codes needed for wlr_texture_read_pixels
#include <drm_fourcc.h>

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
extern "C" {
    #include <wayland-client.h>
    #include "pointer-constraints-unstable-v1-client-protocol.h"
    #include "relative-pointer-unstable-v1-client-protocol.h"
    }
#include <unistd.h>
#include <X11/Xlib.h>
#undef None
#undef Bool
#undef Status
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef CursorShape
#undef Unsorted

#include <fcntl.h>
#include "AndroidCanvas.h"
#include "KeymapperOverlay.h"
#include "InputTranslator.h"
#include "UInputTouchBridge.h"
#include "Theme.h"
#include "PathResolver.h"
#include "Logger.h"

#include "wlr_c_bridge.h"
#include "EmbeddedCompositor.h"
#include "WaylandSurfaceRenderer.h"
#include "AspectRatioWindow.h"
#include "InputMapper.h"

AndroidCanvas::AndroidCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(2, 0);
    fmt.setAlphaBufferSize(0); // Fully opaque, prevents transparent see-through buffer
    fmt.setSwapInterval(1);
    setFormat(fmt);

    setAcceptDrops(true);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        QSize s = scr->size();
        if (s.width() > 0 && s.height() > 0) {
            m_baseWidth = s.width();
            m_baseHeight = s.height();
        }
    }
    m_aspectRatio = std::make_unique<AspectRatioWindow>(m_baseWidth, m_baseHeight);
    m_simulatedActive = true;

    setupUi();
    initWaylandCompositor();
    refreshInstalledApps();
}

AndroidCanvas::~AndroidCanvas()
{
    releaseEmbeddedWindow();
    if (m_compositor) {
        m_compositor->shutdown();
    }
}

void AndroidCanvas::setupUi()
{
    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: transparent;");

    // Page 0: Clean Android Launcher (Installed Apps Grid - bypassed by default)
    m_homeWidget = createHomeScreen();
    m_stack->addWidget(m_homeWidget);
    m_stack->hide(); // Boot directly into live Android display viewport

    // Overlay: Keymapper HUD Layer (Active over app viewport)
    m_keymapperOverlay = new KeymapperOverlay(this);
    m_keymapperOverlay->hide();

    connect(m_keymapperOverlay, &KeymapperOverlay::toastRequested, this, [this](const QString &msg) {
        showToast(msg, 3500);
    });
    connect(m_keymapperOverlay, &KeymapperOverlay::aimLockAnchorChanged, this, [this](bool active, QPointF anchorNorm) {
        setAimMode(active, anchorNorm);
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

    auto *osInfo = new QLabel("⚡ LinStack Android | Mesa DRI3 Zero-Copy (wlroots) | 120 FPS", statusBar);
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

    QString appDir = QDir::homePath() + "/.local/share/applications";
    QDir dir(appDir);
    QStringList filters;
    filters << "linstack.*.desktop" << "waydroid.*.desktop";
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
                name = fileInfo.baseName();
            }
            if (pkg.isEmpty()) {
                pkg = fileInfo.baseName().remove("linstack.").remove("waydroid.");
            }

            AndroidAppEntry entry;
            entry.displayName = name;
            entry.packageName = pkg;
            entry.iconType = Theme::IconType::AndroidRecent;
            entry.accentColor = Theme::ColorNeon;
            entry.isGame = true;
            m_installedApps.append(entry);
        }
    }

    if (m_installedApps.isEmpty()) {
        AndroidAppEntry defaultBrowser;
        defaultBrowser.displayName = "Chromium Browser";
        defaultBrowser.packageName = "org.chromium.webview_shell";
        defaultBrowser.iconType = Theme::IconType::AndroidHome;
        defaultBrowser.accentColor = Theme::ColorMint;
        defaultBrowser.isGame = false;
        m_installedApps.append(defaultBrowser);

        AndroidAppEntry defaultSettings;
        defaultSettings.displayName = "Android Settings";
        defaultSettings.packageName = "com.android.settings";
        defaultSettings.iconType = Theme::IconType::Settings;
        defaultSettings.accentColor = Theme::ColorEmerald;
        defaultSettings.isGame = false;
        m_installedApps.append(defaultSettings);
    }

    rebuildAppsGrid();
}

void AndroidCanvas::rebuildAppsGrid()
{
    if (!m_appsGridContainer) return;

    if (m_appsGridContainer->layout()) {
        QLayoutItem *child;
        while ((child = m_appsGridContainer->layout()->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        delete m_appsGridContainer->layout();
    }

    auto *grid = new QGridLayout(m_appsGridContainer);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(20);

    const int cols = 5;
    for (int i = 0; i < m_installedApps.size(); ++i) {
        QWidget *card = createAppCard(m_installedApps[i]);
        grid->addWidget(card, i / cols, i % cols);
    }

    for (int col = 0; col < cols; ++col) {
        grid->setColumnStretch(col, 1);
    }
}

QWidget *AndroidCanvas::createAppCard(const AndroidAppEntry &app)
{
    auto *card = new QWidget(m_appsGridContainer);
    card->setFixedSize(140, 150);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(
        "QWidget {"
        "  background-color: #111827;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 12px;"
        "}"
        "QWidget:hover {"
        "  background-color: #161F30;"
        "  border-color: #10B981;"
        "}"
    );

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 16, 12, 12);
    cardLayout->setSpacing(8);
    cardLayout->setAlignment(Qt::AlignCenter);

    auto *iconLabel = new QLabel(card);
    iconLabel->setFixedSize(54, 54);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(Theme::renderIconPixmap(app.iconType, app.accentColor, 36));
    iconLabel->setStyleSheet(
        QString("background-color: rgba(%1, %2, %3, 0.12); border-radius: 14px; border: 1px solid rgba(%1, %2, %3, 0.3);")
            .arg(app.accentColor.red())
            .arg(app.accentColor.green())
            .arg(app.accentColor.blue())
    );
    cardLayout->addWidget(iconLabel, 0, Qt::AlignCenter);

    auto *nameLabel = new QLabel(app.displayName, card);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("color: #F9FAFB; font-size: 11px; font-weight: 700; background: transparent; border: none;");
    cardLayout->addWidget(nameLabel);

    auto *pkgLabel = new QLabel(app.packageName.section('.', -1), card);
    pkgLabel->setAlignment(Qt::AlignCenter);
    pkgLabel->setStyleSheet("color: #6B7280; font-size: 9px; font-weight: 600; font-family: monospace; background: transparent; border: none;");
    cardLayout->addWidget(pkgLabel);

    auto *btnClick = new QPushButton(card);
    btnClick->setGeometry(0, 0, 140, 150);
    btnClick->setStyleSheet("background: transparent; border: none;");
    btnClick->setCursor(Qt::PointingHandCursor);
    connect(btnClick, &QPushButton::clicked, this, [this, app]() {
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

    if (m_keymapperOverlay) {
        m_keymapperOverlay->setCurrentAppId(packageName);
    }

    emit appLaunchRequested(packageName, displayName);


    m_simulatedActive = true;
    if (m_stack) {
        m_stack->hide();
    }

    showToast(QString("Launching %1 inside LinStack native viewport (zero-copy DMA-BUF)...").arg(displayName), 3500);
    PathResolver::startDetachedLinstack(QStringList() << "app" << "launch" << packageName);
    update();
}

bool AndroidCanvas::embedWindow(WId winId)
{
    Q_UNUSED(winId);
    m_simulatedActive = true;
    if (m_stack) m_stack->hide();
    update();
    return true;
}

bool AndroidCanvas::embedWindow(QWindow *foreignWindow)
{
    Q_UNUSED(foreignWindow);
    m_simulatedActive = true;
    if (m_stack) m_stack->hide();
    update();
    return true;
}

void AndroidCanvas::releaseEmbeddedWindow()
{
    if (m_aimModeActive) {
        setAimMode(false);
    }

    m_simulatedActive = true;
    if (m_stack) {
        m_stack->hide();
    }
    setKeymapperVisible(false);
    emit windowReleased();
    update();
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
    if (m_aspectRatio) {
        m_aspectRatio->setTargetResolution(m_baseWidth, m_baseHeight);
    }
    updateCanvasGeometry();
}

void AndroidCanvas::updateCanvasGeometry()
{
    int arW = (m_orientation == Orientation::Landscape) ? m_baseWidth  : m_baseHeight;
    int arH = (m_orientation == Orientation::Landscape) ? m_baseHeight : m_baseWidth;

    if (m_aspectRatio) {
        m_aspectRatio->setTargetResolution(arW, arH);
        ViewportRect vp = m_aspectRatio->computeViewport(width(), height());
        m_viewportRect = QRect(vp.x, vp.y, vp.width, vp.height);
        if (m_inputMapper) {
            m_inputMapper->setGeometry(vp, arW, arH);
        }
    }

    if (m_stack) {
        m_stack->setGeometry(m_viewportRect);
    }

    if (m_keymapperOverlay) {
        m_keymapperOverlay->setGeometry(m_viewportRect);
        if (m_keymapperOverlay->translator()) {
            m_keymapperOverlay->translator()->setScreenGeometry(m_baseWidth, m_baseHeight);
        }
    }

    if (m_toastLabel) {
        m_toastLabel->adjustSize();
        m_toastLabel->move(
            m_viewportRect.x() + (m_viewportRect.width()  - m_toastLabel->width())  / 2,
            m_viewportRect.y() +  m_viewportRect.height() - m_toastLabel->height() - 28
        );
    }
}

void AndroidCanvas::initializeGL()
{
    LOG_INFO("AndroidCanvas", "Initializing OpenGL ES 2.0 / DMA-BUF Surface Renderer...");
    m_surfaceRenderer = std::make_unique<WaylandSurfaceRenderer>();
    if (!m_surfaceRenderer->initialize()) {
        LOG_CRITICAL("AndroidCanvas", "Failed to compile zero-copy GLES2 shader program");
    } else {
        LOG_INFO("AndroidCanvas", "Zero-copy Wayland Surface GLES2 Renderer initialized successfully.");
    }
}

void AndroidCanvas::paintGL()
{
    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) return;

    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ViewportRect vp;
    if (m_aspectRatio) {
        vp = m_aspectRatio->computeViewport(w, h);
        m_viewportRect = QRect(vp.x, vp.y, vp.width, vp.height);
    } else {
        vp = ViewportRect{0, 0, w, h};
    }

    if (!m_surfaceRenderer) return;
    if (vp.x > 0 || vp.y > 0) {
        m_surfaceRenderer->clearLetterboxBars(vp, w, h);
    }

    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    if (!activeSurface) return;

    int surfW = 0, surfH = 0;
    wlr_bridge_surface_get_size(activeSurface, &surfW, &surfH);
    if (surfW > 0 && surfH > 0 && (surfW != m_baseWidth || surfH != m_baseHeight)) {
        m_baseWidth = surfW;
        m_baseHeight = surfH;
        if (m_aspectRatio) {
            m_aspectRatio->setTargetResolution(m_baseWidth, m_baseHeight);
            vp = m_aspectRatio->computeViewport(w, h);
            m_viewportRect = QRect(vp.x, vp.y, vp.width, vp.height);
        }
    }

    EGLDisplay qtDpy  = eglGetCurrentDisplay();
    EGLContext qtCtx  = eglGetCurrentContext();
    EGLSurface qtDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface qtRead = eglGetCurrentSurface(EGL_READ);

    bool rendered = false;

    // 1. True Zero-Copy DMA-BUF fast path (supports multi-plane buffers on Intel GPUs)
    struct wlr_bridge_dmabuf_attribs dmabuf = {};
    if (wlr_bridge_surface_get_dmabuf(activeSurface, &dmabuf)) {
        static auto s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
            eglGetProcAddress("eglCreateImageKHR"));
        static auto s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
            eglGetProcAddress("eglDestroyImageKHR"));

        if (s_eglCreateImageKHR && s_eglDestroyImageKHR && dmabuf.n_planes > 0) {
            std::vector<EGLint> attribs = {
                EGL_WIDTH, static_cast<EGLint>(dmabuf.width),
                EGL_HEIGHT, static_cast<EGLint>(dmabuf.height),
                EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(dmabuf.format)
            };

            const EGLint planeFdKeys[]    = { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
            const EGLint planeOffKeys[]   = { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
            const EGLint planePitchKeys[] = { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
            const EGLint planeModLoKeys[] = { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
            const EGLint planeModHiKeys[] = { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

            for (int i = 0; i < dmabuf.n_planes && i < 4; ++i) {
                attribs.push_back(planeFdKeys[i]);
                attribs.push_back(dmabuf.fd[i]);
                attribs.push_back(planeOffKeys[i]);
                attribs.push_back(static_cast<EGLint>(dmabuf.offset[i]));
                attribs.push_back(planePitchKeys[i]);
                attribs.push_back(static_cast<EGLint>(dmabuf.stride[i]));

                if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
                    attribs.push_back(planeModLoKeys[i]);
                    attribs.push_back(static_cast<EGLint>(dmabuf.modifier & 0xFFFFFFFF));
                    attribs.push_back(planeModHiKeys[i]);
                    attribs.push_back(static_cast<EGLint>(dmabuf.modifier >> 32));
                }
            }
            attribs.push_back(EGL_NONE);

            EGLImageKHR eglImg = s_eglCreateImageKHR(
                qtDpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());

            if (eglImg != EGL_NO_IMAGE_KHR) {
                rendered = m_surfaceRenderer->renderEglImage(
                    eglImg, true, dmabuf.width, dmabuf.height, vp, w, h);
                s_eglDestroyImageKHR(qtDpy, eglImg);
            } else {
                static bool loggedOnce = false;
                if (!loggedOnce) {
                    loggedOnce = true;
                    LOG_WARN("AndroidCanvas", QString("DMA-BUF EGLImage creation failed (err: 0x%1, planes: %2) - falling back to CPU!")
                    .arg(eglGetError(), 0, 16).arg(dmabuf.n_planes));
                }
            }
        }
    }

    // 2. CPU Readback fallback path if DMA-BUF import fails or client uses wl_shm
    if (!rendered) {
        if (qtDpy != EGL_NO_DISPLAY) {
            eglMakeCurrent(qtDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (m_compositor->makeCurrent()) {
            struct wlr_texture *wlrTex = wlr_bridge_surface_get_texture(activeSurface);
            int texW = 0, texH = 0;
            wlr_bridge_texture_get_size(wlrTex, &texW, &texH);
            bool readOk = false;
            if (wlrTex && texW > 0 && texH > 0) {
                uint32_t stride = static_cast<uint32_t>(texW) * 4u;
                m_cpuReadbackBuffer.resize(stride * texH);
                readOk = wlr_bridge_texture_read_pixels(wlrTex, DRM_FORMAT_ABGR8888, stride, m_cpuReadbackBuffer.data());
            }
            EGLDisplay wlrDpy = m_compositor->getEglDisplay();
            if (wlrDpy != EGL_NO_DISPLAY) {
                eglMakeCurrent(wlrDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
            if (qtDpy != EGL_NO_DISPLAY) {
                eglMakeCurrent(qtDpy, qtDraw, qtRead, qtCtx);
            }
            if (readOk) {
                rendered = m_surfaceRenderer->renderCpuPixels(
                    m_cpuReadbackBuffer.data(),
                    texW,
                    texH,
                    vp, w, h);
            }
        } else if (qtDpy != EGL_NO_DISPLAY) {
            eglMakeCurrent(qtDpy, qtDraw, qtRead, qtCtx);
        }
    }

    if (rendered) {
        m_surfaceRenderer->sendFrameDone(activeSurface);
    }
}



void AndroidCanvas::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
    updateCanvasGeometry();
}

void AndroidCanvas::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    updateCanvasGeometry();
}

void AndroidCanvas::paintEvent(QPaintEvent *event)
{
    QOpenGLWidget::paintEvent(event);

    if (m_hasFirstFrame && !m_isDragOver) return;

    if (!isValid()) return;


    QPainter p(this);
    if (!p.isActive()) return;
    p.setRenderHint(QPainter::Antialiasing, true);

    // If first Android frame has not arrived yet, display a clean "Booting Android..." splash screen
    if (!m_hasFirstFrame) {
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;

        p.setPen(Theme::ColorNeon);
        QFont fTitle = p.font();
        fTitle.setPointSize(18);
        fTitle.setBold(true);
        p.setFont(fTitle);
        p.drawText(QRectF(cx - 250, cy - 45, 500, 36), Qt::AlignCenter, "⚡ Booting Android...");

        p.setPen(Theme::ColorMint);
        QFont fSub = p.font();
        fSub.setPointSize(11);
        fSub.setBold(false);
        p.setFont(fSub);
        p.drawText(QRectF(cx - 250, cy, 500, 24), Qt::AlignCenter, "Initializing Wayland DMA-BUF hardware session...");

        p.setPen(Theme::ColorTextDim);
        QFont fDetail = p.font();
        fDetail.setPointSize(9);
        fDetail.setFamily("monospace");
        p.setFont(fDetail);
        p.drawText(QRectF(cx - 250, cy + 30, 500, 20), Qt::AlignCenter, "Direct OpenGL ES 2.0 / Mesa DRI3 Viewport Active");
    }

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

void AndroidCanvas::initWaylandCompositor()
{
    m_waylandSocketName = "wayland-linstack-0";
    qputenv("WAYLAND_DISPLAY", m_waylandSocketName.toUtf8());
    qputenv("LINSTACK_WAYLAND_DISPLAY", m_waylandSocketName.toUtf8());

    // Auto-detect host display refresh rate (60Hz, 75Hz, 120Hz, 144Hz, etc.)
    int screenHz = 60;
    if (QScreen *scr = this->screen() ? this->screen() : QGuiApplication::primaryScreen()) {
        if (scr->refreshRate() > 1.0) {
            screenHz = qRound(scr->refreshRate());
        }
    }

    EmbeddedCompositorConfig config;
    config.socketName = m_waylandSocketName.toStdString();
    config.defaultWidth = m_baseWidth;
    config.defaultHeight = m_baseHeight;
    config.refreshRate = screenHz;

    m_compositor = std::make_unique<EmbeddedCompositor>(config);

    if (!m_compositor->initialize()) {
        LOG_CRITICAL("AndroidCanvas", "Failed to initialize embedded wlroots compositor.");
        return;
    }

    if (!m_compositor->start()) {
        LOG_CRITICAL("AndroidCanvas", "Failed to start wlroots headless backend.");
        return;
    }

    m_inputMapper = std::make_unique<InputMapper>(m_compositor->getSeat(), m_compositor->getRelativePointerManager());
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setInputMapper(m_inputMapper.get());
    }

    // Integrate Wayland event loop into Qt event loop via QSocketNotifier
    int wlFd = m_compositor->getEventLoopFd();
    if (wlFd >= 0) {
        m_waylandNotifier = new QSocketNotifier(wlFd, QSocketNotifier::Read, this);
        connect(m_waylandNotifier, &QSocketNotifier::activated, this, [this]() {
            if (m_compositor) {
                m_compositor->dispatch(0);
            }
        });
    }

    // Lightweight fallback dispatch timer (50ms) since QSocketNotifier handles instant events
    m_waylandDispatchTimer = new QTimer(this);
    connect(m_waylandDispatchTimer, &QTimer::timeout, this, [this]() {
        if (m_compositor) {
            m_compositor->dispatch(0);
        }
    });
    m_waylandDispatchTimer->start(50);

    if (!m_inputTickTimer) {
        m_inputTickTimer = new QTimer(this);
        connect(m_inputTickTimer, &QTimer::timeout, this, [this]() {
            struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
            if (m_inputMapper && activeSurface) {
                m_inputMapper->tick(activeSurface);
                if (m_compositor) {
                    m_compositor->flush();
                }
            }
        });
    }
    m_inputTickTimer->start(10);


    // Connect Wayland surface lifecycle callbacks
    m_compositor->onClientConnected = [this](struct wl_client *client) {
        Q_UNUSED(client);
        QMetaObject::invokeMethod(this, [this]() {
            LOG_INFO("AndroidCanvas", "Wayland client connected to embedded compositor!");
            emit waylandClientConnected();
        }, Qt::QueuedConnection);
    };

    m_compositor->onSurfaceMapped = [this](struct wlr_surface *surface) {
        Q_UNUSED(surface);
        QMetaObject::invokeMethod(this, [this]() {
            LOG_INFO("AndroidCanvas", ">>> Android Container surface mapped (waiting for first buffer) <<<");
            m_simulatedActive = true;
            update();
        }, Qt::QueuedConnection);
    };

    m_compositor->onSurfaceUnmapped = [this](struct wlr_surface *surface) {
        Q_UNUSED(surface);
        QMetaObject::invokeMethod(this, [this]() {
            LOG_INFO("AndroidCanvas", "Android Surface unmapped");
            m_simulatedActive = true;
            m_hasFirstFrame = false;
            update();
        }, Qt::QueuedConnection);
    };

    m_compositor->onSurfaceCommit = [this](struct wlr_surface *surface) {
        if (m_compositor && surface != m_compositor->getRenderableSurface()) {
            return;
        }
        if (!m_hasFirstFrame && wlr_bridge_surface_has_buffer(surface)) {
            m_hasFirstFrame = true;
            QMetaObject::invokeMethod(this, [this]() {
                LOG_INFO("AndroidCanvas", ">>> First Android surface buffer committed — signalling firstFrameRendered <<<");
                emit firstFrameRendered();
                if (m_stack) m_stack->hide();
                showToast("LinStack Android — First Frame Received!", 3000);
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
    };


    LOG_INFO("AndroidCanvas", QString("Embedded wlroots Compositor running on socket: %1 @ %2Hz")
    .arg(m_waylandSocketName)
    .arg(screenHz));
}


void AndroidCanvas::sendWaylandRecents()
{
    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    if (m_inputMapper && activeSurface) {
        m_inputMapper->sendRecentsCombo(activeSurface);
        if (m_compositor) {
            m_compositor->flush();
        }
    }
}

void AndroidCanvas::sendWaylandSystemKey(uint32_t evdevKeyCode)
{
    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    if (m_inputMapper && activeSurface) {
        m_inputMapper->sendSystemKey(evdevKeyCode, activeSurface);
        if (m_compositor) {
            m_compositor->flush();
        }
    }
}

void AndroidCanvas::showToast(const QString &message, int durationMs)
{
    if (!QSettings("LinStack", "LinStack").value("preferences/show_toasts", true).toBool()) {
        if (m_toastLabel) m_toastLabel->hide();
        return;
    }
    m_toastLabel->setText(message);
    m_toastLabel->adjustSize();

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
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setKeymappingActive(visible);
    }
}

bool AndroidCanvas::isKeymapperVisible() const
{
    return m_keymapperOverlay ? m_keymapperOverlay->isVisible() : false;
}

void AndroidCanvas::setBridge(UInputTouchBridge *bridge)
{
    m_touchBridge = bridge;
    if (m_keymapperOverlay) {
        m_keymapperOverlay->setBridge(bridge);
    }
}

void AndroidCanvas::setAimMode(bool active, QPointF anchorNorm)
{
    m_aimModeActive = active;
    m_aimAnchorNorm = anchorNorm;

    auto *wlApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();

    if (m_aimModeActive) {
        setCursor(Qt::BlankCursor);
        if (window()) window()->setCursor(Qt::BlankCursor);
        if (m_keymapperOverlay) m_keymapperOverlay->setCursor(Qt::BlankCursor);
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);

        // 1. Lock host system pointer natively at Wayland protocol level
        auto *pni = QGuiApplication::platformNativeInterface();
        struct wl_surface *surf = (pni && window() && window()->windowHandle())
        ? static_cast<struct wl_surface *>(pni->nativeResourceForWindow("surface", window()->windowHandle()))
        : nullptr;

        if (wlApp) {
            if (surf && !m_wlLockedPointer) {
                struct wl_display *dpy = wlApp->display();
                struct wl_pointer *ptr = wlApp->pointer();

                if (dpy && ptr) {
                    if (!m_wlPointerConstraints || !m_wlRelPointerMgr) {
                        struct BindCtx { void **constraints; void **relMgr; };
                        BindCtx ctx{&m_wlPointerConstraints, &m_wlRelPointerMgr};
                        struct wl_registry *reg = wl_display_get_registry(dpy);
                        static struct wl_registry_listener regListener = {
                            [](void *data, struct wl_registry *r, uint32_t id, const char *iface, uint32_t ver) {
                                auto *c = static_cast<BindCtx *>(data);
                                if (std::string(iface) == zwp_pointer_constraints_v1_interface.name && !*c->constraints) {
                                    *c->constraints = wl_registry_bind(r, id, &zwp_pointer_constraints_v1_interface, std::min(ver, 1u));
                                } else if (std::string(iface) == zwp_relative_pointer_manager_v1_interface.name && !*c->relMgr) {
                                    *c->relMgr = wl_registry_bind(r, id, &zwp_relative_pointer_manager_v1_interface, std::min(ver, 1u));
                                }
                            },
                            [](void *, struct wl_registry *, uint32_t) {}
                        };
                        wl_registry_add_listener(reg, &regListener, &ctx);
                        wl_display_roundtrip(dpy);
                        wl_registry_destroy(reg);
                    }

                    if (m_wlPointerConstraints) {
                        m_wlLockedPointer = zwp_pointer_constraints_v1_lock_pointer(
                            static_cast<struct zwp_pointer_constraints_v1 *>(m_wlPointerConstraints),
                            surf, ptr, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
                    }
                    if (m_wlRelPointerMgr && !m_wlRelPointer) {
                        m_wlRelPointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
                            static_cast<struct zwp_relative_pointer_manager_v1 *>(m_wlRelPointerMgr), ptr);
                        static const struct zwp_relative_pointer_v1_listener relListener = {
                            [](void *data, struct zwp_relative_pointer_v1 *, uint32_t, uint32_t,
                               wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
                                auto *self = static_cast<AndroidCanvas *>(data);
                                if (!self->m_aimModeActive) return;
                                float fx = static_cast<float>(wl_fixed_to_double(dx_unaccel));
                                float fy = static_cast<float>(wl_fixed_to_double(dy_unaccel));
                                struct wlr_surface *activeSurface = self->m_compositor ? self->m_compositor->getRenderableSurface() : nullptr;
                                if (self->m_inputMapper && activeSurface && (fx != 0.0f || fy != 0.0f)) {
                                    self->m_inputMapper->handleMouseMove(0, 0, fx, fy, activeSurface);
                                    if (self->m_compositor) self->m_compositor->flush();
                                }
                            }
                        };
                        zwp_relative_pointer_v1_add_listener(
                            static_cast<struct zwp_relative_pointer_v1 *>(m_wlRelPointer), &relListener, this);
                    }
                    wl_display_flush(dpy);
                }
            }
        } else {
            // Confine pointer inside window via X11 XGrabPointer without grabMouse() so buttons remain clickable
            if (auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
                Display *xdpy = x11App->display();
                WId xwin = window() ? window()->winId() : winId();
                if (xdpy && xwin) {
                    XUngrabPointer(xdpy, CurrentTime);
                    XGrabPointer(
                        xdpy,
                        static_cast<Window>(xwin),
                        True,
                        PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                        GrabModeAsync,
                        GrabModeAsync,
                        static_cast<Window>(xwin),
                        0L,
                        CurrentTime
                    );
                    XFlush(xdpy);
                }
            }
            QPoint localAnchor(
                m_viewportRect.x() + qRound(m_aimAnchorNorm.x() * m_viewportRect.width()),
                m_viewportRect.y() + qRound(m_aimAnchorNorm.y() * m_viewportRect.height())
            );
            QPoint globalAnchor = mapToGlobal(localAnchor);
            m_lastMousePos = globalAnchor;
            m_ignoreNextMouseMove = true;
            QCursor::setPos(globalAnchor);
        }

        showToast("🎯 Aim Lock Active (Press Aim Key or ESC to unlock)", 2000);

        // 2. Read raw mouse deltas directly from /dev/input/mice
        if (m_rawMouseFd < 0) {
            m_rawMouseFd = ::open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
            if (m_rawMouseFd >= 0) {
                m_rawMouseNotifier = new QSocketNotifier(m_rawMouseFd, QSocketNotifier::Read, this);
                connect(m_rawMouseNotifier, &QSocketNotifier::activated, this, [this]() {
                    if (!m_aimModeActive || m_rawMouseFd < 0) return;
                    static int s_pendingDx = 0;
                    static int s_pendingDy = 0;
                    static bool s_flushScheduled = false;

                    signed char packet[3];
                    while (::read(m_rawMouseFd, packet, sizeof(packet)) == 3) {
                        s_pendingDx += static_cast<int>(packet[1]);
                        s_pendingDy -= static_cast<int>(packet[2]);
                    }

                    if (s_pendingDx != 0 || s_pendingDy != 0) {
                        int dx = s_pendingDx;
                        int dy = s_pendingDy;
                        s_pendingDx = 0;
                        s_pendingDy = 0;
                        struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
                        if (m_aimModeActive && m_inputMapper && activeSurface) {
                            m_inputMapper->handleMouseMove(0, 0, dx, dy, activeSurface);
                            if (m_compositor) m_compositor->flush();
                        }
                    }
                });
            }
        }

            } else {
        if (m_wlRelPointer) {
            zwp_relative_pointer_v1_destroy(static_cast<struct zwp_relative_pointer_v1 *>(m_wlRelPointer));
            m_wlRelPointer = nullptr;
        }
        if (m_wlLockedPointer) {
            zwp_locked_pointer_v1_destroy(static_cast<struct zwp_locked_pointer_v1 *>(m_wlLockedPointer));
            m_wlLockedPointer = nullptr;
            if (wlApp && wlApp->display()) wl_display_flush(wlApp->display());
        } else if (!wlApp) {
            if (auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
                if (Display *xdpy = x11App->display()) {
                    XUngrabPointer(xdpy, CurrentTime);
                    XFlush(xdpy);
                }
            }
        }

        while (QGuiApplication::overrideCursor()) {
            QGuiApplication::restoreOverrideCursor();
        }
        if (m_rawMouseNotifier) {
            delete m_rawMouseNotifier;
            m_rawMouseNotifier = nullptr;
        }
        if (m_rawMouseFd >= 0) {
            ::close(m_rawMouseFd);
            m_rawMouseFd = -1;
        }
        setCursor(Qt::ArrowCursor);
        if (window()) window()->setCursor(Qt::ArrowCursor);
        if (m_keymapperOverlay) m_keymapperOverlay->setCursor(Qt::ArrowCursor);
        showToast("Aim Lock Released", 1500);
    }

    emit aimModeChanged(m_aimModeActive);
}

void AndroidCanvas::mousePressEvent(QMouseEvent *event)
{
    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    int button = 0;
    if (event->button() == Qt::LeftButton) button = 1;
    else if (event->button() == Qt::RightButton) button = 3;
    else if (event->button() == Qt::MiddleButton) button = 2;
    else if (event->button() == Qt::BackButton || event->button() == Qt::XButton1) button = 4;
    else if (event->button() == Qt::ForwardButton || event->button() == Qt::XButton2) button = 5;

    if (m_inputMapper && activeSurface && button > 0) {
        int surfW = m_baseWidth;
        int surfH = m_baseHeight;
        wlr_bridge_surface_get_size(activeSurface, &surfW, &surfH);
        if (surfW <= 0) surfW = m_baseWidth;
        if (surfH <= 0) surfH = m_baseHeight;
        ViewportRect vp{m_viewportRect.x(), m_viewportRect.y(), m_viewportRect.width(), m_viewportRect.height()};
        m_inputMapper->setGeometry(vp, surfW, surfH);

        if (!isKeymapperVisible() && m_inputMapper->isAimLockActive()) {
            m_inputMapper->setAimLockActive(false, -1);
        }

        if (m_inputMapper->handleMouseDown(button, qRound(event->position().x()), qRound(event->position().y()), activeSurface)) {
            if (m_compositor) m_compositor->flush();
            bool nowLocked = m_inputMapper->isAimLockActive() || m_inputMapper->isFreeLookActive();
            if (isKeymapperVisible() && nowLocked != m_aimModeActive) {
                QPointF anchor(0.5, 0.5);
                if (const auto *aim = m_inputMapper->activeAimNode()) anchor = QPointF(aim->centerX, aim->centerY);
                else if (const auto *fl = m_inputMapper->activeFreeLookNode()) anchor = QPointF(fl->centerX, fl->centerY);
                setAimMode(nowLocked, anchor);
            }
            event->accept();
            return;
        }
    }
    QOpenGLWidget::mousePressEvent(event);
}

void AndroidCanvas::mouseMoveEvent(QMouseEvent *event)
{
    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;

    if (isKeymapperVisible() && m_aimModeActive) {
        // Ignore Qt mouse events when Wayland Relative Pointer or raw mouse reader is active to prevent double-input or drift
        if (m_wlRelPointer || m_rawMouseFd >= 0) {
            event->accept();
            return;
        }
        

        QPoint currentGlobal = event->globalPosition().toPoint();
        if (m_ignoreNextMouseMove) {
            m_ignoreNextMouseMove = false;
            m_lastMousePos = currentGlobal;
            event->accept();
            return;
        }

        int dx = currentGlobal.x() - m_lastMousePos.x();
        int dy = currentGlobal.y() - m_lastMousePos.y();
        m_lastMousePos = currentGlobal;

        // Ignore only extreme nonsensical jumps (> 800px) to prevent aim stutter on fast flicks
        if (std::abs(dx) > 800 || std::abs(dy) > 800) {
            event->accept();
            return;
        }

        if (m_inputMapper && activeSurface && (dx != 0 || dy != 0)) {
            m_inputMapper->handleMouseMove(qRound(event->position().x()), qRound(event->position().y()), dx, dy, activeSurface);
            if (m_compositor) m_compositor->flush();
        }

        // Re-center cursor only near window edges to prevent jitter
        QPoint localPos = event->position().toPoint();
        const int margin = 90;
        if (localPos.x() < margin || localPos.x() > width() - margin ||
            localPos.y() < margin || localPos.y() > height() - margin) {
            QPoint localAnchor(
                m_viewportRect.x() + qRound(m_aimAnchorNorm.x() * m_viewportRect.width()),
                               m_viewportRect.y() + qRound(m_aimAnchorNorm.y() * m_viewportRect.height())
            );
        QPoint globalAnchor = mapToGlobal(localAnchor);
        m_ignoreNextMouseMove = true;
        QCursor::setPos(globalAnchor);
            }

            event->accept();
            return;
    }

    if (isKeymapperVisible() && m_inputMapper && activeSurface && m_inputMapper->isSmartCasting()) {
        m_inputMapper->handleMouseMove(qRound(event->position().x()), qRound(event->position().y()), 0, 0, activeSurface);
        if (m_compositor) m_compositor->flush();
        event->accept();
        return;
    }

    if (m_inputMapper && activeSurface && (event->buttons() & Qt::LeftButton)) {
        m_inputMapper->handleMouseMove(qRound(event->position().x()), qRound(event->position().y()), 0, 0, activeSurface);
        if (m_compositor) m_compositor->flush();
        event->accept();
        return;
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void AndroidCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    int button = 0;
    if (event->button() == Qt::LeftButton) button = 1;
    else if (event->button() == Qt::RightButton) button = 3;
    else if (event->button() == Qt::MiddleButton) button = 2;
    else if (event->button() == Qt::BackButton || event->button() == Qt::XButton1) button = 4;
    else if (event->button() == Qt::ForwardButton || event->button() == Qt::XButton2) button = 5;

    if (m_inputMapper && activeSurface && button > 0) {
        if (m_inputMapper->handleMouseUp(button, qRound(event->position().x()), qRound(event->position().y()), activeSurface)) {
            if (m_compositor) m_compositor->flush();
            bool nowLocked = m_inputMapper->isAimLockActive() || m_inputMapper->isFreeLookActive();
            QPointF anchor(0.5, 0.5);
            if (const auto *aim = m_inputMapper->activeAimNode()) {
                anchor = QPointF(aim->centerX, aim->centerY);
            } else if (const auto *fl = m_inputMapper->activeFreeLookNode()) {
                anchor = QPointF(fl->centerX, fl->centerY);
            }
            if (isKeymapperVisible() && (nowLocked != m_aimModeActive || (nowLocked && anchor != m_aimAnchorNorm))) {
                setAimMode(nowLocked, anchor);
            }
            event->accept();
            return;
        }
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void AndroidCanvas::wheelEvent(QWheelEvent *event)

{
    QOpenGLWidget::wheelEvent(event);
}

void AndroidCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    if (isKeymapperVisible()) {
        if (m_inputMapper && activeSurface) {
            quint32 scanCode = event->nativeScanCode();
            if (m_inputMapper->handleScanCodeDown(scanCode, activeSurface)) {
                if (m_compositor) m_compositor->flush();
                bool nowLocked = m_inputMapper->isAimLockActive() || m_inputMapper->isFreeLookActive();
                QPointF anchor(0.5, 0.5);
                if (const auto *aim = m_inputMapper->activeAimNode()) {
                    anchor = QPointF(aim->centerX, aim->centerY);
                } else if (const auto *fl = m_inputMapper->activeFreeLookNode()) {
                    anchor = QPointF(fl->centerX, fl->centerY);
                }
                if (nowLocked != m_aimModeActive || (nowLocked && anchor != m_aimAnchorNorm)) {
                    setAimMode(nowLocked, anchor);
                }
                event->accept();
                return;
            }
        }
    } else {
        if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) ||
            event->key() == Qt::Key_F11) {
            event->ignore();
            return;
        }
        // When Keymapping is OFF, forward 100% of native keyPressEvent to Wayland wl_keyboard
        if (m_inputMapper && activeSurface) {
            quint32 scanCode = event->nativeScanCode();
            m_inputMapper->forwardNativeKey(scanCode, true, activeSurface);
            if (m_compositor) m_compositor->flush();
            event->accept();
            return;
        }
    }
    QOpenGLWidget::keyPressEvent(event);
}

void AndroidCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    struct wlr_surface *activeSurface = m_compositor ? m_compositor->getRenderableSurface() : nullptr;
    if (isKeymapperVisible()) {
        if (m_inputMapper && activeSurface) {
            quint32 scanCode = event->nativeScanCode();
            if (m_inputMapper->handleScanCodeUp(scanCode, activeSurface)) {
                if (m_compositor) m_compositor->flush();
                bool nowLocked = m_inputMapper->isAimLockActive() || m_inputMapper->isFreeLookActive();
                QPointF anchor(0.5, 0.5);
                if (const auto *aim = m_inputMapper->activeAimNode()) {
                    anchor = QPointF(aim->centerX, aim->centerY);
                } else if (const auto *fl = m_inputMapper->activeFreeLookNode()) {
                    anchor = QPointF(fl->centerX, fl->centerY);
                }
                if (nowLocked != m_aimModeActive || (nowLocked && anchor != m_aimAnchorNorm)) {
                    setAimMode(nowLocked, anchor);
                }
                event->accept();
                return;
            }
        }
    } else {
        if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) ||
            event->key() == Qt::Key_F11) {
            event->ignore();
            return;
        }
        // When Keymapping is OFF, forward 100% of native keyReleaseEvent to Wayland wl_keyboard
        if (m_inputMapper && activeSurface) {
            quint32 scanCode = event->nativeScanCode();
            m_inputMapper->forwardNativeKey(scanCode, false, activeSurface);
            if (m_compositor) m_compositor->flush();
            event->accept();
            return;
        }
    }
    QOpenGLWidget::keyReleaseEvent(event);
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

                AndroidAppEntry newApp;
                newApp.packageName = "com.custom." + fi.baseName().toLower();
                newApp.displayName = fi.baseName();
                newApp.iconType = Theme::IconType::InstallApk;
                newApp.accentColor = Theme::ColorNeon;
                newApp.isGame = true;
                addInstalledApp(newApp);

                showToast(QString("Installing APK: %1 via ADB / container...").arg(fi.fileName()), 4000);
                return;
            }
        }
    }
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
