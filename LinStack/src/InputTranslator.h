#ifndef INPUTTRANSLATOR_H
#define INPUTTRANSLATOR_H

#include <QObject>
#include <QPointF>
#include <QVector>
#include <QSet>
#include <QMap>
#include <QTimer>

class UInputTouchBridge;

enum class KeyNodeType {
    Tap,
    Dpad,
    AimShoot,
    Swipe,
    SmartCast,
    BurstTap,
    FreeLook,
    EdgeScroll
};

struct KeyBindingNode {
    QString id;
    KeyNodeType type = KeyNodeType::Tap;
    QString label;
    QString action;
    int primaryKey = 0;
    quint32 primaryScanCode = 0;

    // Relative coordinates (0.0 to 1.0)
    qreal relX = 0.5;
    qreal relY = 0.5;
    qreal radius = 24.0;
    qreal sensitivity = 1.2;

    // For AimLock / FreeLook
    qreal sensitivityX = 1.2;
    qreal sensitivityY = 1.2;
    bool  acceleration = false;
    bool  enableFireButton = true;
    qreal fireRelX = 0.82;
    qreal fireRelY = 0.64;

    // For D-Pad custom scancodes
    quint32 scanUp = 25;
    quint32 scanDown = 39;
    quint32 scanLeft = 38;
    quint32 scanRight = 40;

    // For BurstTap (1 to 100 clicks/sec)
    int burstHz = 10;

    // For EdgeScroll
    int edgeThreshold = 40;
    qreal edgeScrollSpeed = 12.0;

    // For Swipe (linear or recorded freehand path)
    qreal endRelX = 0.5;
    qreal endRelY = 0.3;
    int durationMs = 150;
    QString direction = "up";
    QVector<QPointF> pathPoints; // Normalized (0.0-1.0) waypoints for freehand swipe

    // For SmartCast
    qreal castRadius = 160.0;
    bool isCasting = false;

    int touchSlot = -1;
};

class InputTranslator : public QObject
{
    Q_OBJECT

public:
    explicit InputTranslator(UInputTouchBridge *bridge, QObject *parent = nullptr);

    void setScreenGeometry(int width, int height);
    void setNodes(const QList<KeyBindingNode> &nodes);
    QList<KeyBindingNode> nodes() const { return m_nodes; }

    bool handleScanCodePress(quint32 scanCode, int fallbackKey = 0);
    bool handleScanCodeRelease(quint32 scanCode, int fallbackKey = 0);
    bool handleMouseMove(int deltaX, int deltaY, const QPoint &localPos = QPoint());
    bool handleMousePress(Qt::MouseButton btn);
    bool handleMouseRelease(Qt::MouseButton btn);

    bool isAimLockActive() const { return m_aimLockActive; }
    bool isFreeLookActive() const { return m_freeLookActive; }
    bool isSmartCasting() const { return m_smartCasting; }
    int activeAimNodeIndex() const { return m_activeAimIndex; }
    QPointF activeAimAnchorNormalized() const;
    void setAimLockActive(bool active, int nodeIndex = 0);

signals:
    void aimLockToggled(bool active, QPointF anchorNorm);
    void freeLookToggled(bool active, QPointF anchorNorm);
    void touchInjected(int slot, int pixelX, int pixelY, bool down);
    void smartCastAimUpdated(const QString &nodeId, const QPointF &center, const QPointF &target);

private slots:
    void processSwipeStep();
    void processEdgeScroll();

private:
    void updateDpad();
    void triggerSwipe(const KeyBindingNode &node);
    void startBurstTap(const KeyBindingNode &node);
    void stopBurstTap(quint32 scanCode);
    void startEdgeScroll(int dx, int dy, const KeyBindingNode &node);
    void stopEdgeScroll();

    UInputTouchBridge *m_bridge = nullptr;
    int m_screenWidth = 1280;
    int m_screenHeight = 720;

    QList<KeyBindingNode> m_nodes;
    QSet<quint32> m_heldScanCodes;

    bool m_dpadActive = false;
    KeyBindingNode m_dpadNode;

    bool m_aimLockActive = false;
    int  m_activeAimIndex = -1;
    int  m_aimSwipeAccumX = 0;
    int  m_aimSwipeAccumY = 0;
    bool m_firing = false;

    bool m_freeLookActive = false;
    KeyBindingNode m_freeLookNode;
    int m_freeLookAccumX = 0;
    int m_freeLookAccumY = 0;

    QMap<quint32, QTimer*> m_burstTimers;

    bool   m_edgeScrollActive = false;
    QTimer *m_edgeScrollTimer = nullptr;
    int    m_edgeScrollDirX = 0;
    int    m_edgeScrollDirY = 0;

    QTimer *m_swipeTimer = nullptr;
    int m_swipeCurrentStep = 0;
    int m_swipeTotalSteps = 6;
    QPointF m_swipeStart;
    QPointF m_swipeEnd;
    QVector<QPointF> m_activeSwipePath;
    KeyBindingNode m_activeSwipeNode;
    qreal m_smartCastAccumX = 0.0;
    qreal m_smartCastAccumY = 0.0;

    bool m_smartCasting = false;
    KeyBindingNode m_activeSmartCastNode;
    QPoint m_lastMousePos;

    QMap<quint32, int> m_scanToSlot;
};

#endif // INPUTTRANSLATOR_H
