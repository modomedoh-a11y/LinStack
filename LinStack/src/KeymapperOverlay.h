#ifndef KEYMAPPEROVERLAY_H
#define KEYMAPPEROVERLAY_H

#include <QWidget>
#include <QList>
#include <QVector>
#include <QString>
#include <QPointF>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "InputTranslator.h"

class UInputTouchBridge;
class InputMapper;

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

    void setInputMapper(InputMapper *mapper);
    InputMapper *inputMapper() const { return m_inputMapper; }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setCurrentAppId(const QString &appId);
    QString currentAppId() const { return m_appId; }

    void setOverlayOpacity(qreal opacity);
    qreal overlayOpacity() const { return m_opacity; }

    // Profile Management & Auto-Save/Load
    QString profilesDir() const;
    void refreshProfileList(const QString &selectProfile = QString());
    bool saveProfileByName(const QString &profileName);
    bool loadProfileByName(const QString &profileName);
    void createNewProfile();
    void duplicateCurrentProfile();
    void deleteCurrentProfile();
    void exportProfileToFile();
    void importProfileFromFile();
    void resetToDefaultBinds();

    // Node management
    void addNode(const KeyBindingNode &node);
    void removeSelectedNode();

signals:
    void modeChanged(KeymapperOverlay::Mode newMode);
    void layoutSaved(const QString &filePath);
    void layoutLoaded(const QString &filePath);
    void toastRequested(const QString &message);
    void aimLockAnchorChanged(bool active, QPointF normPos);

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
    void setupNodeSettingsPanel();
    void updateNodeSettingsPanel();
    int  findNodeAt(const QPointF &pos) const;
    bool isGearClicked(int nodeIndex, const QPointF &pos) const;
    bool isDeleteClicked(int nodeIndex, const QPointF &pos) const;
    bool hasNodeSettings(KeyNodeType type) const;
    void syncTranslatorNodes();
    void positionEditorBar();

    Mode m_mode = Mode::Play;
    qreal m_opacity = 0.85;
    QString m_appId = "default";
    QString m_currentProfileName = "Default";

    UInputTouchBridge *m_bridge = nullptr;
    InputTranslator   *m_translator = nullptr;
    InputMapper       *m_inputMapper = nullptr;

    QList<KeyBindingNode> m_nodes;

    // Drag-and-drop state in Edit mode
    int     m_selectedNodeIndex  = -1;
    bool    m_isDraggingNode     = false;
    bool    m_isDraggingSwipeEnd = false;
    bool    m_isDraggingFireBtn  = false;
    bool    m_isResizingDpad     = false;
    QPointF m_dragOffset;

    // Quick creation via click/drag on empty canvas in Edit mode
    bool             m_isDrawingFreehand = false;
    QPointF          m_emptyPressPos;
    QVector<QPointF> m_freehandPoints;

    // Draggable node settings panel state
    bool    m_isDraggingSettingsPanel = false;
    QPointF m_settingsDragOffset;

    // Mouse lock state in Play mode (anchored at AimLock or FreeLook node position)
    QPoint  m_lastMousePos;
    QPointF m_activeAimAnchorNorm = QPointF(0.5, 0.5);
    bool    m_ignoreNextMouseMove = false;

    // Smart cast preview
    QPointF m_smartCastCenter;
    QPointF m_smartCastTarget;
    bool    m_showSmartCastLine = false;

    // Floating editor toolbar
    QWidget     *m_editorBar        = nullptr;
    QComboBox   *m_profileCombo     = nullptr;
    QPushButton *m_btnNewProfile    = nullptr;
    QPushButton *m_btnDupProfile    = nullptr;
    QPushButton *m_btnDelProfile    = nullptr;
    QPushButton *m_btnExport        = nullptr;
    QPushButton *m_btnImport        = nullptr;
    QSlider     *m_opacitySlider    = nullptr;
    QLabel      *m_opacityLabel     = nullptr;
    QPushButton *m_btnClearAll      = nullptr;
    QPushButton *m_btnReset         = nullptr;
    QPushButton *m_btnToggleMode    = nullptr;

    // Right-side Node Catalog panel (no minimize button)
    QWidget     *m_catalogPanel     = nullptr;
    QPushButton *m_btnAddTap        = nullptr;
    QPushButton *m_btnAddDpad       = nullptr;
    QPushButton *m_btnAddAim        = nullptr;
    QPushButton *m_btnAddSwipe      = nullptr;
    QPushButton *m_btnAddSmartCast  = nullptr;
    QPushButton *m_btnAddBurstTap   = nullptr;
    QPushButton *m_btnAddFreeLook   = nullptr;

    // Draggable Per-node Context Settings Popup
    QWidget        *m_nodeSettingsPanel = nullptr;
    QLabel         *m_settingsTitle     = nullptr;
    QVBoxLayout    *m_settingsBody      = nullptr;
    bool            m_settingsOpen      = false;
};

#endif // KEYMAPPEROVERLAY_H
