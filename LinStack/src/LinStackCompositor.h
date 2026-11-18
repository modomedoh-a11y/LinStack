#ifndef LINSTACKCOMPOSITOR_H
#define LINSTACKCOMPOSITOR_H

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QQuickWindow>
#include <QSize>

/**
 * @brief High-performance embedded Wayland Compositor (Qt6::WaylandCompositor).
 * Listens on private socket 'linstack-wayland-0' in XDG_RUNTIME_DIR and binds incoming
 * Waydroid/Android XDG toplevel surfaces directly into the LinStack viewport.
 */
class LinStackCompositor : public QWaylandCompositor
{
    Q_OBJECT

public:
    explicit LinStackCompositor(QQuickWindow *renderWindow, QObject *parent = nullptr);
    ~LinStackCompositor() override;

    void create() override;

    void setOutputSize(const QSize &size);
    QSize outputSize() const { return m_outputSize; }

    QWaylandQuickOutput *quickOutput() const { return m_quickOutput; }
    QWaylandXdgShell *xdgShell() const { return m_xdgShell; }
    QString socketNameString() const { return m_socketName; }

signals:
    void surfaceMapped(QWaylandSurface *surface);
    void toplevelMapped(QWaylandXdgToplevel *toplevel, QWaylandSurface *surface);
    void surfaceUnmapped(QWaylandSurface *surface);

private slots:
    void onSurfaceCreated(QWaylandSurface *surface);
    void onToplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface);

private:
    QQuickWindow *m_renderWindow = nullptr;
    QWaylandQuickOutput *m_quickOutput = nullptr;
    QWaylandXdgShell *m_xdgShell = nullptr;
    QWaylandXdgDecorationManagerV1 *m_decorationManager = nullptr;
    QSize m_outputSize{1920, 1080};
    QString m_socketName{"wayland-linstack-0"};
};

#endif // LINSTACKCOMPOSITOR_H
