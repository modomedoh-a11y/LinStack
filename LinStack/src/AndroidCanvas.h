#ifndef ANDROIDCANVAS_H
#define ANDROIDCANVAS_H

#include <QOpenGLWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QWindow>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMap>
#include <QRect>
#include <QPointF>
#include <QSocketNotifier>
#include <memory>
#include <vector>
#include <cstdint>

#include "Theme.h"

class KeymapperOverlay;
class UInputTouchBridge;
class EmbeddedCompositor;
class WaylandSurfaceRenderer;
class AspectRatioWindow;
class InputMapper;
struct ViewportRect;

struct AndroidAppEntry {
    QString packageName;
    QString displayName;
    Theme::IconType iconType = Theme::IconType::AndroidRecent;
    QColor accentColor = QColor(16, 185, 129);
    bool isGame = false;
};

class AndroidCanvas : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum class Orientation {
        Landscape,
        Portrait
    };

    explicit AndroidCanvas(QWidget *parent = nullptr);
    ~AndroidCanvas() override;

    bool embedWindow(WId winId);
    bool embedWindow(QWindow *foreignWindow);
    void releaseEmbeddedWindow();
    bool isWindowEmbedded() const { return m_simulatedActive; }

    void setInstalledApps(const QList<AndroidAppEntry> &apps);
    void addInstalledApp(const AndroidAppEntry &app);
    void refreshInstalledApps();

    void setOrientation(Orientation orientation);
    Orientation orientation() const { return m_orientation; }

    void setBaseResolution(int baseWidth, int baseHeight);
    int  baseWidth()  const { return m_baseWidth; }
    int  baseHeight() const { return m_baseHeight; }
    QRect viewportRect() const { return m_viewportRect; }

    void showToast(const QString &message, int durationMs = 3000);
    void setKeymapperVisible(bool visible);
    bool isKeymapperVisible() const;
    KeymapperOverlay *keymapperOverlay() const { return m_keymapperOverlay; }
    void setBridge(UInputTouchBridge *bridge);

    QString activeAppPackage() const { return m_activePackage; }
    void launchAndEmbedApp(const QString &packageName, const QString &displayName);

    void initWaylandCompositor();
    void sendWaylandSystemKey(uint32_t evdevKeyCode);
    void sendWaylandRecents();
    QString waylandSocketName() const { return m_waylandSocketName; }
    EmbeddedCompositor *compositor() const { return m_compositor.get(); }
    AspectRatioWindow *aspectRatioWindow() const { return m_aspectRatio.get(); }
    InputMapper *inputMapper() const { return m_inputMapper.get(); }

    // Aim Lock Mode (anchored at the AimLock node position, no right-click toggle)
    bool isAimModeActive() const { return m_aimModeActive; }
    bool isRawMouseActive() const { return m_rawMouseFd >= 0; }
    void setAimMode(bool active, QPointF anchorNorm = QPointF(0.5, 0.5));

signals:
    void windowEmbedded(WId winId);
    void windowReleased();
    void appLaunchRequested(const QString &packageName, const QString &displayName);
    void apkDropped(const QString &filePath);
    void searchStoreRequested(const QString &query);
    void aimModeChanged(bool active);
    void firstFrameRendered();
    void waylandClientConnected();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void handleAppCardClicked(const QString &packageName, const QString &displayName);

private:
    void setupUi();
    QWidget *createHomeScreen();
    void rebuildAppsGrid();
    QWidget *createAppCard(const AndroidAppEntry &app);
    void updateCanvasGeometry();

    std::unique_ptr<EmbeddedCompositor> m_compositor;
    std::unique_ptr<WaylandSurfaceRenderer> m_surfaceRenderer;
    std::unique_ptr<AspectRatioWindow> m_aspectRatio;
    std::unique_ptr<InputMapper> m_inputMapper;
    QSocketNotifier *m_waylandNotifier = nullptr;
    QTimer *m_waylandDispatchTimer = nullptr;
    QTimer *m_inputTickTimer = nullptr;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_homeWidget = nullptr;
    QWidget *m_appsGridContainer = nullptr;

    // Aim Lock State (anchored at AimLock node position)
    bool m_aimModeActive = false;
    bool m_ignoreNextMouseMove = false;
    QPoint m_lastMousePos;
    QPointF m_aimAnchorNorm = QPointF(0.5, 0.5);
    int m_rawMouseFd = -1;
    QSocketNotifier *m_rawMouseNotifier = nullptr;
    void *m_wlPointerConstraints = nullptr;
    void *m_wlLockedPointer = nullptr;
    void *m_wlRelPointerMgr = nullptr;
    void *m_wlRelPointer = nullptr;

    bool m_simulatedActive = true;
    bool m_hasFirstFrame = false;
    QString m_activePackage;

    KeymapperOverlay *m_keymapperOverlay = nullptr;
    UInputTouchBridge *m_touchBridge = nullptr;
    QString m_waylandSocketName = "wayland-linstack-0";
    QLabel *m_toastLabel = nullptr;
    QTimer *m_toastTimer = nullptr;

    bool m_isDragOver = false;
    Orientation m_orientation = Orientation::Landscape;

    int   m_baseWidth    = 1920;
    int   m_baseHeight   = 1080;
    QRect m_viewportRect;

    QList<AndroidAppEntry> m_installedApps;
    std::vector<uint8_t> m_cpuReadbackBuffer;
};

#endif // ANDROIDCANVAS_H
