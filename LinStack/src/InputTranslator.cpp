#include "InputTranslator.h"
#include "UInputTouchBridge.h"

#include <cmath>

InputTranslator::InputTranslator(UInputTouchBridge *bridge, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
{
    // Default D-Pad (WASD)
    m_dpadNode.type = KeyNodeType::Dpad;
    m_dpadNode.relX = 0.18;
    m_dpadNode.relY = 0.70;
    m_dpadNode.radius = 60.0;

    // Default Aim & Shoot
    m_aimNode.type = KeyNodeType::AimShoot;
    m_aimNode.relX = 0.52;
    m_aimNode.relY = 0.50;
    m_aimNode.sensitivity = 1.2;
    m_aimNode.sensitivityX = 1.2;
    m_aimNode.sensitivityY = 1.2;
    m_aimNode.aimActivationKey = Qt::Key_F1;
    m_aimNode.aimSuspendKey    = Qt::Key_Alt;
    m_aimNode.fireRelX = 0.82;
    m_aimNode.fireRelY = 0.64;

    // Swipe Timer
    m_swipeTimer = new QTimer(this);
    m_swipeTimer->setInterval(25);
    connect(m_swipeTimer, &QTimer::timeout, this, &InputTranslator::processSwipeStep);

    // Edge Scroll Timer (continuous panning, 30ms interval)
    m_edgeScrollTimer = new QTimer(this);
    m_edgeScrollTimer->setInterval(30);
    connect(m_edgeScrollTimer, &QTimer::timeout, this, &InputTranslator::processEdgeScroll);
}

void InputTranslator::setScreenGeometry(int width, int height)
{
    m_screenWidth = qMax(320, width);
    m_screenHeight = qMax(240, height);
}

void InputTranslator::setNodes(const QList<KeyBindingNode> &nodes)
{
    m_nodes = nodes;
    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::Dpad) {
            m_dpadNode = node;
        } else if (node.type == KeyNodeType::AimShoot) {
            m_aimNode = node;
        } else if (node.type == KeyNodeType::FreeLook) {
            m_freeLookNode = node;
        }
    }
}

void InputTranslator::setAimLockActive(bool active)
{
    if (m_aimLockActive != active) {
        m_aimLockActive = active;
        m_aimSuspended  = false;
        if (!m_aimLockActive) {
            if (m_bridge) {
                m_bridge->touchUp(1);
            }
            m_aimSwipeAccumX = 0;
            m_aimSwipeAccumY = 0;
        }
        emit aimLockToggled(m_aimLockActive);
    }
}

bool InputTranslator::handleKeyPress(int key)
{
    m_heldKeys.insert(key);

    // ESC releases aim lock and free look
    if (key == Qt::Key_Escape) {
        if (m_aimLockActive) { setAimLockActive(false); return true; }
        if (m_freeLookActive) {
            m_freeLookActive = false;
            if (m_bridge) m_bridge->touchUp(6);
            emit freeLookToggled(false);
            return true;
        }
    }

    // Alt key: temporarily suspend aim lock
    if (key == m_aimNode.aimSuspendKey && m_aimLockActive) {
        m_aimSuspended = true;
        if (m_bridge) m_bridge->touchUp(1);
        m_aimSwipeAccumX = 0;
        m_aimSwipeAccumY = 0;
        return true;
    }

    // WASD movement handling (Slot 0)
    if (key == Qt::Key_W || key == Qt::Key_A || key == Qt::Key_S || key == Qt::Key_D) {
        updateDpad();
        return true;
    }

    // Check custom nodes
    for (auto &node : m_nodes) {
        if (node.primaryKey == key) {
            // f) Burst Tap Node
            if (node.type == KeyNodeType::BurstTap) {
                startBurstTap(node);
                return true;
            }

            // g) Free Look Node
            if (node.type == KeyNodeType::FreeLook) {
                m_freeLookActive = !m_freeLookActive;
                m_freeLookNode = node;
                m_freeLookAccumX = 0;
                m_freeLookAccumY = 0;
                if (!m_freeLookActive && m_bridge) {
                    m_bridge->touchUp(6);
                }
                emit freeLookToggled(m_freeLookActive);
                return true;
            }

            // h) Edge Scroll Node — direction determined by node's direction field
            if (node.type == KeyNodeType::EdgeScroll) {
                int dx = 0, dy = 0;
                if (node.direction == "up")    dy = -1;
                if (node.direction == "down")  dy =  1;
                if (node.direction == "left")  dx = -1;
                if (node.direction == "right") dx =  1;
                startEdgeScroll(dx, dy, node);
                return true;
            }

            // d) Swipe Node
            if (node.type == KeyNodeType::Swipe) {
                triggerSwipe(node);
                return true;
            }

            // e) Smart Cast / Skill Radius Node
            if (node.type == KeyNodeType::SmartCast) {
                m_smartCasting = true;
                m_activeSmartCastNode = node;
                int cx = static_cast<int>(node.relX * m_screenWidth);
                int cy = static_cast<int>(node.relY * m_screenHeight);
                if (m_bridge) {
                    m_bridge->touchDown(4, cx, cy);
                }
                emit touchInjected(4, cx, cy, true);
                return true;
            }

            // a) Tap Node (Slots 5 to 9)
            if (node.type == KeyNodeType::Tap) {
                int slot = -1;
                for (int s = 5; s < 10; ++s) {
                    if (!m_keyToSlot.values().contains(s)) {
                        slot = s;
                        break;
                    }
                }
                if (slot != -1) {
                    m_keyToSlot.insert(key, slot);
                    int px = static_cast<int>(node.relX * m_screenWidth);
                    int py = static_cast<int>(node.relY * m_screenHeight);
                    if (m_bridge) {
                        m_bridge->touchDown(slot, px, py);
                    }
                    emit touchInjected(slot, px, py, true);
                }
                return true;
            }
        }
    }

    return false;
}

bool InputTranslator::handleKeyRelease(int key)
{
    m_heldKeys.remove(key);

    // Alt key: resume aim lock if it was suspended
    if (key == m_aimNode.aimSuspendKey && m_aimSuspended && m_aimLockActive) {
        m_aimSuspended = false;
        // Re-anchor aim touch at center
        int cx = static_cast<int>(m_aimNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_aimNode.relY * m_screenHeight);
        if (m_bridge) m_bridge->touchDown(1, cx, cy);
        return true;
    }

    // WASD release
    if (key == Qt::Key_W || key == Qt::Key_A || key == Qt::Key_S || key == Qt::Key_D) {
        updateDpad();
        return true;
    }

    // Smart Cast release -> trigger cast at aimed location
    if (m_smartCasting && m_activeSmartCastNode.primaryKey == key) {
        m_smartCasting = false;
        if (m_bridge) {
            m_bridge->touchUp(4);
        }
        emit touchInjected(4, 0, 0, false);
        return true;
    }

    // BurstTap release
    if (m_burstTimers.contains(key)) {
        stopBurstTap(key);
        return true;
    }

    // EdgeScroll release
    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::EdgeScroll && node.primaryKey == key) {
            stopEdgeScroll();
            return true;
        }
    }

    // Tap node release
    if (m_keyToSlot.contains(key)) {
        int slot = m_keyToSlot.take(key);
        if (m_bridge) {
            m_bridge->touchUp(slot);
        }
        emit touchInjected(slot, 0, 0, false);
        return true;
    }

    return false;
}

void InputTranslator::updateDpad()
{
    bool w = m_heldKeys.contains(Qt::Key_W);
    bool s = m_heldKeys.contains(Qt::Key_S);
    bool a = m_heldKeys.contains(Qt::Key_A);
    bool d = m_heldKeys.contains(Qt::Key_D);

    qreal dx = 0;
    qreal dy = 0;

    if (w) dy -= 1.0;
    if (s) dy += 1.0;
    if (a) dx -= 1.0;
    if (d) dx += 1.0;

    if (dx == 0 && dy == 0) {
        if (m_dpadActive) {
            m_dpadActive = false;
            if (m_bridge) {
                m_bridge->touchUp(0);
            }
            emit touchInjected(0, 0, 0, false);
        }
        return;
    }

    // Smooth 8-way directional normalization
    qreal len = std::sqrt(dx * dx + dy * dy);
    dx /= len;
    dy /= len;

    int cx = static_cast<int>(m_dpadNode.relX * m_screenWidth);
    int cy = static_cast<int>(m_dpadNode.relY * m_screenHeight);

    int targetX = cx + static_cast<int>(dx * m_dpadNode.radius);
    int targetY = cy + static_cast<int>(dy * m_dpadNode.radius);

    if (!m_dpadActive) {
        m_dpadActive = true;
        if (m_bridge) {
            m_bridge->touchDown(0, targetX, targetY);
        }
    } else {
        if (m_bridge) {
            m_bridge->touchMove(0, targetX, targetY);
        }
    }

    emit touchInjected(0, targetX, targetY, true);
}

void InputTranslator::triggerSwipe(const KeyBindingNode &node)
{
    m_swipeStart = QPointF(node.relX * m_screenWidth, node.relY * m_screenHeight);
    m_swipeEnd = QPointF(node.endRelX * m_screenWidth, node.endRelY * m_screenHeight);
    m_swipeCurrentStep = 0;
    m_swipeTotalSteps = 4;

    if (m_bridge) {
        m_bridge->touchDown(3, static_cast<int>(m_swipeStart.x()), static_cast<int>(m_swipeStart.y()));
    }
    m_swipeTimer->start();
}

void InputTranslator::processSwipeStep()
{
    m_swipeCurrentStep++;
    qreal progress = static_cast<qreal>(m_swipeCurrentStep) / m_swipeTotalSteps;

    int currentX = static_cast<int>(m_swipeStart.x() + (m_swipeEnd.x() - m_swipeStart.x()) * progress);
    int currentY = static_cast<int>(m_swipeStart.y() + (m_swipeEnd.y() - m_swipeStart.y()) * progress);

    if (m_swipeCurrentStep >= m_swipeTotalSteps) {
        m_swipeTimer->stop();
        if (m_bridge) {
            m_bridge->touchMove(3, currentX, currentY);
            m_bridge->touchUp(3);
        }
        emit touchInjected(3, currentX, currentY, false);
    } else {
        if (m_bridge) {
            m_bridge->touchMove(3, currentX, currentY);
        }
        emit touchInjected(3, currentX, currentY, true);
    }
}

bool InputTranslator::handleMouseMove(int deltaX, int deltaY, const QPoint &globalPos)
{
    m_lastMousePos = globalPos;

    // Smart Cast skill aiming
    if (m_smartCasting) {
        int cx = static_cast<int>(m_activeSmartCastNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_activeSmartCastNode.relY * m_screenHeight);

        qreal mx = globalPos.x() - cx;
        qreal my = globalPos.y() - cy;
        qreal dist = std::sqrt(mx * mx + my * my);
        qreal maxR = m_activeSmartCastNode.castRadius;

        if (dist > maxR && dist > 0.001) {
            mx = (mx / dist) * maxR;
            my = (my / dist) * maxR;
        }

        int targetX = cx + static_cast<int>(mx);
        int targetY = cy + static_cast<int>(my);

        if (m_bridge) {
            m_bridge->touchMove(4, targetX, targetY);
        }
        emit smartCastAimUpdated(m_activeSmartCastNode.id, QPointF(cx, cy), QPointF(targetX, targetY));
        emit touchInjected(4, targetX, targetY, true);
        return true;
    }

    // FreeLook camera drag (Slot 6) — independent of D-Pad (Slot 0)
    if (m_freeLookActive) {
        int cx = static_cast<int>(m_freeLookNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_freeLookNode.relY * m_screenHeight);

        m_freeLookAccumX += static_cast<int>(deltaX * m_freeLookNode.sensitivityX);
        m_freeLookAccumY += static_cast<int>(deltaY * m_freeLookNode.sensitivityY);

        const int maxSweep = 100;
        if (std::abs(m_freeLookAccumX) > maxSweep || std::abs(m_freeLookAccumY) > maxSweep) {
            if (m_bridge) m_bridge->touchUp(6);
            m_freeLookAccumX = 0;
            m_freeLookAccumY = 0;
            if (m_bridge) m_bridge->touchDown(6, cx, cy);
        } else {
            int tx = cx + m_freeLookAccumX;
            int ty = cy + m_freeLookAccumY;
            if (m_bridge) m_bridge->touchMove(6, tx, ty);
            emit touchInjected(6, tx, ty, true);
        }
        return true;
    }

    // Aim, Pan & Shoot (Slot 1) — respects per-axis sensitivity and suspend state
    if (m_aimLockActive && !m_aimSuspended) {
        int cx = static_cast<int>(m_aimNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_aimNode.relY * m_screenHeight);

        m_aimSwipeAccumX += static_cast<int>(deltaX * m_aimNode.sensitivityX);
        m_aimSwipeAccumY += static_cast<int>(deltaY * m_aimNode.sensitivityY);

        const int maxSweepRadius = 90;
        if (std::abs(m_aimSwipeAccumX) > maxSweepRadius || std::abs(m_aimSwipeAccumY) > maxSweepRadius) {
            if (m_bridge) {
                m_bridge->touchUp(1);
            }
            m_aimSwipeAccumX = 0;
            m_aimSwipeAccumY = 0;
            if (m_bridge) {
                m_bridge->touchDown(1, cx, cy);
            }
        } else {
            int tx = cx + m_aimSwipeAccumX;
            int ty = cy + m_aimSwipeAccumY;
            if (m_bridge) {
                m_bridge->touchMove(1, tx, ty);
            }
            emit touchInjected(1, tx, ty, true);
        }
        return true;
    }

    return false;
}

bool InputTranslator::handleMousePress(Qt::MouseButton btn)
{
    if (btn == Qt::RightButton) {
        setAimLockActive(!m_aimLockActive);
        return true;
    }

    if (btn == Qt::LeftButton && m_aimLockActive) {
        // Fire button in Slot 2
        m_firing = true;
        int fx = static_cast<int>(m_aimNode.fireRelX * m_screenWidth);
        int fy = static_cast<int>(m_aimNode.fireRelY * m_screenHeight);
        if (m_bridge) {
            m_bridge->touchDown(2, fx, fy);
        }
        emit touchInjected(2, fx, fy, true);
        return true;
    }

    return false;
}

bool InputTranslator::handleMouseRelease(Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton && m_firing) {
        m_firing = false;
        if (m_bridge) {
            m_bridge->touchUp(2);
        }
        emit touchInjected(2, 0, 0, false);
        return true;
    }
    return false;
}

// ── BurstTap ─────────────────────────────────────────────────────────────────

void InputTranslator::startBurstTap(const KeyBindingNode &node)
{
    if (m_burstTimers.contains(node.primaryKey)) return;

    // Find an available touch slot (5-9)
    int slot = -1;
    for (int s = 5; s < 10; ++s) {
        if (!m_keyToSlot.values().contains(s)) { slot = s; break; }
    }
    if (slot == -1) return;

    m_keyToSlot.insert(node.primaryKey, slot);
    const int px = static_cast<int>(node.relX * m_screenWidth);
    const int py = static_cast<int>(node.relY * m_screenHeight);

    // Hz clamped to [1, 40]
    const int hz = qBound(1, node.burstHz, 40);
    const int intervalMs = 1000 / hz;

    QTimer *timer = new QTimer(this);
    timer->setInterval(intervalMs);

    // Capture by value to avoid dangling reference
    const int capturedSlot = slot;
    connect(timer, &QTimer::timeout, this, [this, capturedSlot, px, py]() {
        if (m_bridge) {
            m_bridge->touchDown(capturedSlot, px, py);
            m_bridge->touchUp(capturedSlot);
        }
        emit touchInjected(capturedSlot, px, py, true);
        emit touchInjected(capturedSlot, 0, 0, false);
    });
    timer->start();
    m_burstTimers.insert(node.primaryKey, timer);
}

void InputTranslator::stopBurstTap(int key)
{
    if (QTimer *t = m_burstTimers.take(key)) {
        t->stop();
        t->deleteLater();
    }
    if (m_keyToSlot.contains(key)) {
        int slot = m_keyToSlot.take(key);
        if (m_bridge) m_bridge->touchUp(slot);
        emit touchInjected(slot, 0, 0, false);
    }
}

// ── EdgeScroll ────────────────────────────────────────────────────────────────

void InputTranslator::startEdgeScroll(int dx, int dy, const KeyBindingNode &node)
{
    m_edgeScrollDirX = dx;
    m_edgeScrollDirY = dy;
    m_edgeScrollActive = true;

    // Anchor the scroll touch at node center
    const int cx = static_cast<int>(node.relX * m_screenWidth);
    const int cy = static_cast<int>(node.relY * m_screenHeight);
    if (m_bridge) m_bridge->touchDown(9, cx, cy);

    m_edgeScrollTimer->start();
}

void InputTranslator::stopEdgeScroll()
{
    m_edgeScrollActive = false;
    m_edgeScrollTimer->stop();
    if (m_bridge) m_bridge->touchUp(9);
    emit touchInjected(9, 0, 0, false);
}

void InputTranslator::processEdgeScroll()
{
    if (!m_edgeScrollActive) { m_edgeScrollTimer->stop(); return; }

    // Find an EdgeScroll node to get speed
    qreal speed = 12.0;
    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::EdgeScroll) { speed = node.edgeScrollSpeed; break; }
    }

    // Accumulate position for slot 9
    static int accumX = 0;
    static int accumY = 0;
    accumX += static_cast<int>(m_edgeScrollDirX * speed);
    accumY += static_cast<int>(m_edgeScrollDirY * speed);

    // Wrap around to avoid unbounded scroll
    if (std::abs(accumX) > 200 || std::abs(accumY) > 200) {
        if (m_bridge) m_bridge->touchUp(9);
        accumX = 0; accumY = 0;
        // Re-anchor at center of screen
        if (m_bridge) m_bridge->touchDown(9, m_screenWidth / 2, m_screenHeight / 2);
    } else {
        int tx = m_screenWidth / 2 + accumX;
        int ty = m_screenHeight / 2 + accumY;
        if (m_bridge) m_bridge->touchMove(9, tx, ty);
        emit touchInjected(9, tx, ty, true);
    }
}
