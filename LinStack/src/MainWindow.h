#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QRect>

class TitleBar;
class SideToolBar;
class AndroidCanvas;
class SettingsDialog;
class LinstackService;
class UInputTouchBridge;
class SelfHealingManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void handleWindowDrag(const QPoint &delta);
    void handleMaximizeToggle();
    void handleFullscreenToggle();
    void handleEcoModeToggle(bool active);
    void handleKeymapperToggle(bool active);
    void handleInstallApk();
    void handleScreenshot();
    void handleRotate();
    void handleRamTrim();
    void handleSettings();
    void handleHelp();
    void handleSearch(const QString &query);
    void handleDiagnostics();

private:
    void setupUi();
    void checkEdgeResize(const QPoint &pos);
    Qt::Edges calculateEdges(const QPoint &pos);

    TitleBar *m_titleBar = nullptr;
    AndroidCanvas *m_canvas = nullptr;
    SideToolBar *m_sideToolBar = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
    LinstackService *m_service = nullptr;
    UInputTouchBridge *m_uinputBridge = nullptr;
    SelfHealingManager *m_healer = nullptr;

    // Resize state
    bool m_resizing = false;
    Qt::Edges m_resizeEdges = Qt::Edges();
    QRect m_dragStartGeometry;
    QPoint m_dragStartMousePos;
    const int m_resizeBorderWidth = 8;
};

#endif // MAINWINDOW_H
