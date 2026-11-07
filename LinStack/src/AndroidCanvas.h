#ifndef ANDROIDCANVAS_H
#define ANDROIDCANVAS_H

#include <QWidget>
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

#include "Theme.h"

class KeymapperOverlay;
class UInputTouchBridge;

struct AndroidAppEntry {
    QString packageName;
    QString displayName;
    Theme::IconType iconType = Theme::IconType::AndroidRecent;
    QColor accentColor = QColor(16, 185, 129);
    bool isGame = false;
};

class AndroidCanvas : public QWidget
{
    Q_OBJECT

public:
    enum class Orientation {
        Landscape,
        Portrait
    };

    explicit AndroidCanvas(QWidget *parent = nullptr);
    ~AndroidCanvas() override;

    // Window Embedding APIs
    bool embedWindow(WId winId);
    bool embedWindow(QWindow *foreignWindow);
    void releaseEmbeddedWindow();
    bool isWindowEmbedded() const { return m_embeddedWindow != nullptr || m_simulatedActive; }

    // Dynamic Installed Apps Management
    void setInstalledApps(const QList<AndroidAppEntry> &apps);
    void addInstalledApp(const AndroidAppEntry &app);
    void refreshInstalledApps();

    void setOrientation(Orientation orientation);
    Orientation orientation() const { return m_orientation; }

    // ── Aspect Ratio & Letterboxing ──────────────────────────────────────────
    // Sets the base resolution used to derive the locked aspect ratio.
    // Letterbox bars are painted with the cyber-dark background colour when the
    // window aspect does not match (default: 1920 × 1080 → 16:9).
    void setBaseResolution(int baseWidth, int baseHeight);
    int  baseWidth()  const { return m_baseWidth; }
    int  baseHeight() const { return m_baseHeight; }

    // Returns the sub-rect of this widget that holds the scaled Android content
    // (after letterbox/pillarbox bars are removed).
    QRect viewportRect() const { return m_viewportRect; }

    void showToast(const QString &message, int durationMs = 3000);
    void setKeymapperVisible(bool visible);
    bool isKeymapperVisible() const;
    KeymapperOverlay *keymapperOverlay() const { return m_keymapperOverlay; }
    void setBridge(UInputTouchBridge *bridge);

    QString activeAppPackage() const { return m_activePackage; }

signals:
    void windowEmbedded(WId winId);
    void windowReleased();
    void appLaunchRequested(const QString &packageName, const QString &displayName);
    void apkDropped(const QString &filePath);
    void searchStoreRequested(const QString &query);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void handleAppCardClicked(const QString &packageName, const QString &displayName);

private:
    void setupUi();
    QWidget *createHomeScreen();
    void rebuildAppsGrid();
    QWidget *createAppCard(const AndroidAppEntry &app);
    void updateCanvasGeometry();
    void launchAndEmbedApp(const QString &packageName, const QString &displayName);

    QStackedWidget *m_stack = nullptr;
    QWidget *m_homeWidget = nullptr;
    QWidget *m_appsGridContainer = nullptr;

    // Embedded Linstack window container
    QWidget *m_embedContainerWidget = nullptr;
    QWidget *m_windowContainer = nullptr;
    QWindow *m_embeddedWindow = nullptr;

    // In-canvas game viewport & top bar
    QWidget *m_gameViewportWidget = nullptr;
    QLabel *m_gameTitleBadge = nullptr;
    QLabel *m_gameFpsBadge = nullptr;
    bool m_simulatedActive = false;
    QString m_activePackage;

    KeymapperOverlay *m_keymapperOverlay = nullptr;
    QLabel *m_toastLabel = nullptr;
    QTimer *m_toastTimer = nullptr;

    bool m_isDragOver = false;
    Orientation m_orientation = Orientation::Landscape;

    // ── Aspect Ratio & Letterboxing state ────────────────────────────────────
    int   m_baseWidth    = 1920;  // Default base resolution width
    int   m_baseHeight   = 1080;  // Default base resolution height
    QRect m_viewportRect;         // Computed letterboxed/pillarboxed content rect

    QList<AndroidAppEntry> m_installedApps;
};

#endif // ANDROIDCANVAS_H
