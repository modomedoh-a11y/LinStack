#ifndef INPUTTRANSLATOR_H
#define INPUTTRANSLATOR_H

#include <QObject>
#include <QPointF>
#include <QSet>
#include <QMap>
#include <QTimer>

class UInputTouchBridge;

enum class KeyNodeType {
    Tap,        // a) Single key mapped to touch point
    Dpad,       // b) WASD 8-way directional touch joystick
    AimShoot,   // c) Aim, Pan & Shoot (mouse look + LMB shoot; F1 lock key)
    Swipe,      // d) Swipe gestures between two points
    SmartCast,  // e) Smart Cast / Skill Radius towards mouse
    BurstTap,   // f) Repeated/Burst Tap Node (up to 40 taps/sec via QTimer)
    FreeLook,   // g) Free Look / Eye (360° camera drag, no movement slot 0)
    EdgeScroll  // h) Edge Scroll (continuous panning when cursor nears edges)
};

struct KeyBindingNode {
    QString id;
    KeyNodeType type = KeyNodeType::Tap;
    QString label;
    QString action;
    int primaryKey = 0; // Qt::Key

    // Relative coordinates (0.0 to 1.0)
    qreal relX = 0.5;
    qreal relY = 0.5;
    qreal radius = 24.0;
    qreal sensitivity = 1.2;

    // For AimShoot / FreeLook: activation key override, suspend key, per-axis sensitivity
    int   aimActivationKey = Qt::Key_F1;   // Key that toggles aim lock (default F1)
    int   aimSuspendKey    = Qt::Key_Alt;  // Key that temporarily suspends aim lock
    qreal sensitivityX     = 1.2;          // Horizontal sensitivity multiplier
    qreal sensitivityY     = 1.2;          // Vertical sensitivity multiplier

    // For AimShoot: coordinates for Fire touch button
    qreal fireRelX = 0.82;
    qreal fireRelY = 0.64;

    // For BurstTap: tap frequency (taps per second, up to 40)
    int burstHz = 10;

    // For EdgeScroll: edge threshold in pixels, scroll speed
    int edgeThreshold = 40;
    qreal edgeScrollSpeed = 12.0;

    // For Swipe: endpoint coordinates and duration
    qreal endRelX = 0.5;
    qreal endRelY = 0.3;
    int durationMs = 150;
    QString direction = "up";

    // For SmartCast: skill cast radius
    qreal castRadius = 160.0;
    bool isCasting = false;

    // Associated touch slot
    int touchSlot = -1;
};

    /**
     * @brief Translates host keyboard & raw mouse deltas to multi-touch events injected into /dev/uinput.
     * Supports all 8 node types: Tap, WASD D-Pad, Aim/Shoot, Swipe/Gesture, Smart Cast,
     * Burst Tap, Free Look, and Edge Scroll.
     */class InputTranslator : public QObject
{
    Q_OBJECT

public:
    explicit InputTranslator(UInputTouchBridge *bridge, QObject *parent = nullptr);

    void setScreenGeometry(int width, int height);
    void setNodes(const QList<KeyBindingNode> &nodes);
    QList<KeyBindingNode> nodes() const { return m_nodes; }

    bool handleKeyPress(int key);
    bool handleKeyRelease(int key);
    bool handleMouseMove(int deltaX, int deltaY, const QPoint &globalPos = QPoint());
    bool handleMousePress(Qt::MouseButton btn);
    bool handleMouseRelease(Qt::MouseButton btn);

    bool isAimLockActive() const { return m_aimLockActive; }
    void setAimLockActive(bool active);

signals:
    void aimLockToggled(bool active);
    void freeLookToggled(bool active);
    void touchInjected(int slot, int pixelX, int pixelY, bool down);
    void smartCastAimUpdated(const QString &nodeId, const QPointF &center, const QPointF &target);

private slots:
    void processSwipeStep();
    void processEdgeScroll();

private:
    void updateDpad();
    void triggerSwipe(const KeyBindingNode &node);
    void startBurstTap(const KeyBindingNode &node);
    void stopBurstTap(int key);
    void startEdgeScroll(int dx, int dy, const KeyBindingNode &node);
    void stopEdgeScroll();

    UInputTouchBridge *m_bridge = nullptr;
    int m_screenWidth = 1280;
    int m_screenHeight = 720;

    QList<KeyBindingNode> m_nodes;
    QSet<int> m_heldKeys;

    // D-Pad state (Slot 0)
    bool m_dpadActive = false;
    KeyBindingNode m_dpadNode;

    // Aim/Shoot state (Slot 1 for Aim, Slot 2 for Shoot)
    bool m_aimLockActive = false;
    bool m_aimSuspended  = false;
    KeyBindingNode m_aimNode;
    int m_aimSwipeAccumX = 0;
    int m_aimSwipeAccumY = 0;
    bool m_firing = false;

    // FreeLook state (Slot 6 — camera drag, no D-pad interference)
    bool m_freeLookActive = false;
    KeyBindingNode m_freeLookNode;
    int m_freeLookAccumX = 0;
    int m_freeLookAccumY = 0;

    // BurstTap state: QTimer per active burst node (key→timer)
    QMap<int, QTimer*> m_burstTimers;

    // EdgeScroll state (Slot 9)
    bool   m_edgeScrollActive = false;
    QTimer *m_edgeScrollTimer = nullptr;
    int    m_edgeScrollDirX = 0;
    int    m_edgeScrollDirY = 0;

    // Swipe state (Slot 3)
    QTimer *m_swipeTimer = nullptr;
    int m_swipeCurrentStep = 0;
    int m_swipeTotalSteps = 4;
    QPointF m_swipeStart;
    QPointF m_swipeEnd;

    // Smart Cast state (Slot 4)
    bool m_smartCasting = false;
    KeyBindingNode m_activeSmartCastNode;
    QPoint m_lastMousePos;

    // Dynamic slot allocation for Tap nodes (Slots 5 to 9)
    QMap<int, int> m_keyToSlot;
};

#endif // INPUTTRANSLATOR_H
