#include "LinStackCompositor.h"
#include "Logger.h"

#include <QStandardPaths>
#include <QFile>
#include <QDir>

LinStackCompositor::LinStackCompositor(QQuickWindow *renderWindow, QObject *parent)
    : QWaylandCompositor(parent)
    , m_renderWindow(renderWindow)
{
}

LinStackCompositor::~LinStackCompositor()
{
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";
    QFile::remove(runtimeDir + "/" + m_socketName);
    QFile::remove(runtimeDir + "/" + m_socketName + ".lock");
}

void LinStackCompositor::create()
{
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";

    setSocketName(m_socketName.toUtf8());

    // Clean up any stale socket or lock files before creating
    QFile::remove(runtimeDir + "/" + m_socketName);
    QFile::remove(runtimeDir + "/" + m_socketName + ".lock");

    // Initialize quick output attached to the render window
    if (m_renderWindow) {
        m_quickOutput = new QWaylandQuickOutput(this, m_renderWindow);
        m_quickOutput->setPhysicalSize(m_outputSize);
    }

    // Initialize XDG Shell extension
    m_xdgShell = new QWaylandXdgShell(this);
    m_xdgShell->initialize();
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &LinStackCompositor::onToplevelCreated);

    // Prefer server-side decoration so Android/Waydroid renders borderless
    m_decorationManager = new QWaylandXdgDecorationManagerV1();
    m_decorationManager->setExtensionContainer(this);
    m_decorationManager->initialize();
    m_decorationManager->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);

    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &LinStackCompositor::onSurfaceCreated);

    QWaylandCompositor::create();

    LOG_INFO("WaylandCompositor", QString("LinStack Wayland Compositor running on socket: %1/%2")
                                      .arg(runtimeDir, m_socketName));
}

void LinStackCompositor::setOutputSize(const QSize &size)
{
    if (size.isValid()) {
        m_outputSize = size;
        if (m_quickOutput) {
            m_quickOutput->setPhysicalSize(m_outputSize);
        }
    }
}

void LinStackCompositor::onToplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface)
{
    if (!toplevel || !xdgSurface) return;

    LOG_INFO("WaylandCompositor", QString("Bound Wayland XDG Toplevel: title='%1' appId='%2'")
                                      .arg(toplevel->title(), toplevel->appId()));

    // Configure client to match viewport geometry in activated state
    toplevel->sendConfigure(m_outputSize, QList<QWaylandXdgToplevel::State>{QWaylandXdgToplevel::ActivatedState});

    QWaylandSurface *surface = xdgSurface->surface();
    connect(toplevel, &QObject::destroyed, this, [this, surface]() {
        if (surface) {
            emit surfaceUnmapped(surface);
        }
    });

    emit toplevelMapped(toplevel, surface);
}

void LinStackCompositor::onSurfaceCreated(QWaylandSurface *surface)
{
    if (!surface) return;

    connect(surface, &QWaylandSurface::surfaceDestroyed, this, [this, surface]() {
        emit surfaceUnmapped(surface);
    });
}
