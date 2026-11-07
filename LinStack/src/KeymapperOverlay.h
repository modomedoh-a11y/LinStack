#ifndef KEYMAPPEROVERLAY_H
#define KEYMAPPEROVERLAY_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QPointF>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "InputTranslator.h"

class UInputTouchBridge;

class KeymapperOverlay : public QWidget
{
    Q_OBJECT

public:
    enum class Mode {
        Play,
        Edit
    };
    Q_ENUM(Mode)

    explicit KeymapperOverlay(QWidget *parent = nullptr);

    void setBridge(UInputTouchBridge *bridge);
    InputTranslator *translator() const { return m_translator; }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setCurrentAppId(const QString &appId);
    QString currentAppId() const { return m_appId; }

    void setOverlayOpacity(qreal opacity);
    qreal overlayOpacity() const { return m_opacity; }

    // JSON Layout Management (~/.config/LinStack/keymaps/<app_id>.json)
    bool saveLayoutToJson(const QString &filePath = QString());
    bool loadLayoutFromJson(const QString &filePath = QString());
    void resetToDefaultBinds();

    // Node management
    void addNode(const KeyBindingNode &node);
    void removeSelectedNode();

signals:
    void modeChanged(KeymapperOverlay::Mode newMode);
    void layoutSaved(const QString &filePath);
    void layoutLoaded(const QString &filePath);
    void toastRequested(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupEditorUi();
    void setupNodeCatalogPanel();
    int  findNodeAt(const QPointF &pos) const;
    void syncTranslatorNodes();
    void positionEditorBar();

    // Draw individual node types
    void drawNodeBurstTap(QPainter &p, const KeyBindingNode &node, bool selected) const;
    void drawNodeFreeLook(QPainter &p, const KeyBindingNode &node, bool selected) const;
    void drawNodeEdgeScroll(QPainter &p, const KeyBindingNode &node, bool selected) const;

    Mode m_mode = Mode::Play;
    qreal m_opacity = 0.85;
    QString m_appId = "default";

    UInputTouchBridge *m_bridge = nullptr;
    InputTranslator   *m_translator = nullptr;

    QList<KeyBindingNode> m_nodes;

    // Drag-and-drop state in Edit mode
    int     m_selectedNodeIndex  = -1;
    bool    m_isDraggingNode     = false;
    bool    m_isDraggingSwipeEnd = false;
    QPointF m_dragOffset;

    // Mouse lock state in Play mode
    QPoint m_lastMousePos;
    bool   m_ignoreNextMouseMove = false;

    // Smart cast preview
    QPointF m_smartCastCenter;
    QPointF m_smartCastTarget;
    bool    m_showSmartCastLine = false;

    // ── Floating editor toolbar (horizontal, top-center) ──────────────────────
    QWidget     *m_editorBar       = nullptr;
    QComboBox   *m_profileCombo    = nullptr;  // Profile selector dropdown
    QSlider     *m_opacitySlider   = nullptr;  // Opacity 0–100%
    QLabel      *m_opacityLabel    = nullptr;

    QPushButton *m_btnAddTap       = nullptr;
    QPushButton *m_btnAddDpad      = nullptr;
    QPushButton *m_btnAddAim       = nullptr;
    QPushButton *m_btnAddSwipe     = nullptr;
    QPushButton *m_btnAddSmartCast = nullptr;
    QPushButton *m_btnAddBurstTap  = nullptr;
    QPushButton *m_btnAddFreeLook  = nullptr;
    QPushButton *m_btnAddEdgeScroll = nullptr;
    QPushButton *m_btnClearAll     = nullptr;
    QPushButton *m_btnSave         = nullptr;
    QPushButton *m_btnLoad         = nullptr;
    QPushButton *m_btnReset        = nullptr;
    QPushButton *m_btnToggleMode   = nullptr;

    // ── Right-side collapsible Node Catalog panel ─────────────────────────────
    QWidget     *m_catalogPanel    = nullptr;
    bool         m_catalogExpanded = true;
    QPushButton *m_btnToggleCatalog = nullptr;
};

#endif // KEYMAPPEROVERLAY_H
