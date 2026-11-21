#include "InputTranslator.h"
#include "UInputTouchBridge.h"
#include <cmath>
#include <algorithm>

InputTranslator::InputTranslator(UInputTouchBridge *bridge, QObject *parent)
: QObject(parent)
, m_bridge(bridge)
{
    m_dpadNode.type = KeyNodeType::Dpad;
    m_dpadNode.relX = 0.18;
    m_dpadNode.relY = 0.70;
    m_dpadNode.radius = 60.0;
    m_dpadNode.scanUp = 0;
    m_dpadNode.scanLeft = 0;
    m_dpadNode.scanDown = 0;
    m_dpadNode.scanRight = 0;

    m_swipeTimer = new QTimer(this);
    m_swipeTimer->setInterval(16);
    connect(m_swipeTimer, &QTimer::timeout, this, &InputTranslator::processSwipeStep);

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
    // تصفير D-Pad و FreeLook أولاً حتى لا يعملا إذا قام المستخدم بحذفهما
    m_dpadNode = KeyBindingNode();
    m_dpadNode.scanUp = 0; m_dpadNode.scanDown = 0; m_dpadNode.scanLeft = 0; m_dpadNode.scanRight = 0;
    m_freeLookNode = KeyBindingNode();
    m_freeLookNode.primaryScanCode = 0;

    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::Dpad) {
            m_dpadNode = node;
        } else if (node.type == KeyNodeType::FreeLook) {
            m_freeLookNode = node;
        }
    }
}

QPointF InputTranslator::activeAimAnchorNormalized() const
{
    if (m_aimLockActive && m_activeAimIndex >= 0 && m_activeAimIndex < m_nodes.size()) {
        return QPointF(m_nodes[m_activeAimIndex].relX, m_nodes[m_activeAimIndex].relY);
    }
    if (m_freeLookActive) {
        return QPointF(m_freeLookNode.relX, m_freeLookNode.relY);
    }
    return QPointF(0.5, 0.5);
}

void InputTranslator::setAimLockActive(bool active, int nodeIndex)
{
    if (active && (nodeIndex < 0 || nodeIndex >= m_nodes.size() || m_nodes[nodeIndex].type != KeyNodeType::AimShoot)) {
        return;
    }

    if (m_aimLockActive == active && (!active || m_activeAimIndex == nodeIndex)) {
        return;
    }

    if (m_aimLockActive && m_bridge) {
        m_bridge->touchUp(1);
        if (m_firing) {
            m_firing = false;
            m_bridge->touchUp(2);
            emit touchInjected(2, 0, 0, false);
        }
        emit touchInjected(1, 0, 0, false);
    }

    m_aimLockActive = active;
    m_activeAimIndex = active ? nodeIndex : -1;
    m_aimSwipeAccumX = 0;
    m_aimSwipeAccumY = 0;

    QPointF anchorNorm(0.5, 0.5);
    if (m_aimLockActive) {
        const auto &aim = m_nodes[m_activeAimIndex];
        anchorNorm = QPointF(aim.relX, aim.relY);
        int cx = static_cast<int>(aim.relX * m_screenWidth);
        int cy = static_cast<int>(aim.relY * m_screenHeight);
        if (m_bridge) {
            m_bridge->touchDown(1, cx, cy);
        }
        emit touchInjected(1, cx, cy, true);
    }

    emit aimLockToggled(m_aimLockActive, anchorNorm);
}

bool InputTranslator::handleScanCodePress(quint32 scanCode, int fallbackKey)
{
    if (scanCode == 0) return false;

    // 1. AimLock (Toggle)
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].type == KeyNodeType::AimShoot && m_nodes[i].primaryScanCode == scanCode) {
            if (m_aimLockActive && m_activeAimIndex == i) {
                setAimLockActive(false, -1);
            } else {
                setAimLockActive(true, i);
            }
            return true;
        }
    }

    // 2. ESC unlocks AimLock or FreeLook
    if (scanCode == 9 || fallbackKey == Qt::Key_Escape) {
        if (m_aimLockActive) {
            setAimLockActive(false, -1);
            return true;
        }
        if (m_freeLookActive) {
            m_freeLookActive = false;
            if (m_bridge) m_bridge->touchUp(6);
            emit touchInjected(6, 0, 0, false);
            emit freeLookToggled(false, QPointF(0.5, 0.5));
            return true;
        }
    }

    m_heldScanCodes.insert(scanCode);

    // 3. D-Pad
    if (scanCode == m_dpadNode.scanUp || scanCode == m_dpadNode.scanDown ||
        scanCode == m_dpadNode.scanLeft || scanCode == m_dpadNode.scanRight) {
        updateDpad();
        return true;
    }

    // 4. Other custom nodes
    for (auto &node : m_nodes) {
        if (node.primaryScanCode == scanCode) {
            if (node.type == KeyNodeType::BurstTap) {
                startBurstTap(node);
                return true;
            }
            // FreeLook: works ONLY while held down (Hold-to-Look)
            if (node.type == KeyNodeType::FreeLook) {
                if (!m_freeLookActive) {
                    m_freeLookActive = true;
                    m_freeLookNode = node;
                    m_freeLookAccumX = 0;
                    m_freeLookAccumY = 0;
                    int cx = static_cast<int>(node.relX * m_screenWidth);
                    int cy = static_cast<int>(node.relY * m_screenHeight);
                    if (m_bridge) m_bridge->touchDown(6, cx, cy);
                    emit touchInjected(6, cx, cy, true);
                    emit freeLookToggled(true, QPointF(node.relX, node.relY));
                }
                return true;
            }
                if (node.type == KeyNodeType::EdgeScroll) {
                    int dx = 0, dy = 0;
                    if (node.direction == "up")    dy = -1;
                    if (node.direction == "down")  dy =  1;
                    if (node.direction == "left")  dx = -1;
                    if (node.direction == "right") dx =  1;
                    startEdgeScroll(dx, dy, node);
                    return true;
                }
                if (node.type == KeyNodeType::Swipe) {
                    triggerSwipe(node);
                    return true;
                }
                if (node.type == KeyNodeType::SmartCast) {
                    m_smartCasting = true;
                    m_activeSmartCastNode = node;
                    m_smartCastAccumX = 0.0;
                    m_smartCastAccumY = 0.0;
                    int cx = static_cast<int>(node.relX * m_screenWidth);
                    int cy = static_cast<int>(node.relY * m_screenHeight);
                    if (m_bridge) m_bridge->touchDown(4, cx, cy);
                    emit touchInjected(4, cx, cy, true);
                    emit smartCastAimUpdated(node.id, QPointF(cx, cy), QPointF(cx, cy));
                    return true;
                }
                if (node.type == KeyNodeType::Tap) {
                    int slot = -1;
                    for (int s = 5; s < 10; ++s) {
                        if (!m_scanToSlot.values().contains(s)) {
                            slot = s;
                            break;
                        }
                    }
                    if (slot != -1) {
                        m_scanToSlot.insert(scanCode, slot);
                        int px = static_cast<int>(node.relX * m_screenWidth);
                        int py = static_cast<int>(node.relY * m_screenHeight);
                        if (m_bridge) m_bridge->touchDown(slot, px, py);
                        emit touchInjected(slot, px, py, true);
                    }
                    return true;
                }
            }
        }

        return false;
}

bool InputTranslator::handleScanCodeRelease(quint32 scanCode, int fallbackKey)
{
    Q_UNUSED(fallbackKey);
    if (scanCode == 0) return false;
    m_heldScanCodes.remove(scanCode);

    // تحرير Free Look فور رفع اليد عن الزر
    if (m_freeLookActive && m_freeLookNode.primaryScanCode == scanCode) {
        m_freeLookActive = false;
        if (m_bridge) m_bridge->touchUp(6);
        emit touchInjected(6, 0, 0, false);
        emit freeLookToggled(false, QPointF(0.5, 0.5));
        return true;
    }

    if (scanCode == m_dpadNode.scanUp || scanCode == m_dpadNode.scanDown ||
        scanCode == m_dpadNode.scanLeft || scanCode == m_dpadNode.scanRight) {
        updateDpad();
        return true;
    }

    if (m_smartCasting && m_activeSmartCastNode.primaryScanCode == scanCode) {
        m_smartCasting = false;
        if (m_bridge) m_bridge->touchUp(4);
        emit touchInjected(4, 0, 0, false);
        return true;
    }

    if (m_burstTimers.contains(scanCode)) {
        stopBurstTap(scanCode);
        return true;
    }

    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::EdgeScroll && node.primaryScanCode == scanCode) {
            stopEdgeScroll();
            return true;
        }
    }

    if (m_scanToSlot.contains(scanCode)) {
        int slot = m_scanToSlot.take(scanCode);
        if (m_bridge) m_bridge->touchUp(slot);
        emit touchInjected(slot, 0, 0, false);
        return true;
    }

    return false;
}

void InputTranslator::updateDpad()
{
    bool w = m_heldScanCodes.contains(m_dpadNode.scanUp);
    bool s = m_heldScanCodes.contains(m_dpadNode.scanDown);
    bool a = m_heldScanCodes.contains(m_dpadNode.scanLeft);
    bool d = m_heldScanCodes.contains(m_dpadNode.scanRight);

    qreal dx = 0, dy = 0;
    if (w) dy -= 1.0;
    if (s) dy += 1.0;
    if (a) dx -= 1.0;
    if (d) dx += 1.0;

    if (dx == 0 && dy == 0) {
        if (m_dpadActive) {
            m_dpadActive = false;
            if (m_bridge) m_bridge->touchUp(0);
            emit touchInjected(0, 0, 0, false);
        }
        return;
    }

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
            m_bridge->touchDown(0, cx, cy);
            m_bridge->touchMove(0, targetX, targetY);
        }
    } else {
        if (m_bridge) m_bridge->touchMove(0, targetX, targetY);
    }

    emit touchInjected(0, targetX, targetY, true);
}

void InputTranslator::triggerSwipe(const KeyBindingNode &node)
{
    m_activeSwipeNode = node;
    m_swipeStart = QPointF(node.relX * m_screenWidth, node.relY * m_screenHeight);
    m_swipeEnd = QPointF(node.endRelX * m_screenWidth, node.endRelY * m_screenHeight);
    m_swipeCurrentStep = 0;
    m_swipeTotalSteps = qMax(2, node.durationMs / 16);

    if (m_bridge) {
        m_bridge->touchDown(3, static_cast<int>(m_swipeStart.x()), static_cast<int>(m_swipeStart.y()));
    }
    emit touchInjected(3, static_cast<int>(m_swipeStart.x()), static_cast<int>(m_swipeStart.y()), true);
    m_swipeTimer->start();
}
void InputTranslator::processSwipeStep()
{
    m_swipeCurrentStep++;
    qreal progress = qBound(0.0, static_cast<qreal>(m_swipeCurrentStep) / m_swipeTotalSteps, 1.0);

    int currentX = 0;
    int currentY = 0;

    // إذا كان للزر مسار حر مرسوم يدوياً (Freehand Swipe)، نتتبع نقاط المسار بدقة
    if (m_activeSwipeNode.pathPoints.size() >= 2) {
        const auto &pts = m_activeSwipeNode.pathPoints;
        qreal scaledPos = progress * (pts.size() - 1);
        int idx0 = qBound(0, static_cast<int>(std::floor(scaledPos)), static_cast<int>(pts.size() - 1));
        int idx1 = qBound(0, idx0 + 1, static_cast<int>(pts.size() - 1));
        qreal localT = scaledPos - idx0;

        qreal rx = pts[idx0].x() + (pts[idx1].x() - pts[idx0].x()) * localT;
        qreal ry = pts[idx0].y() + (pts[idx1].y() - pts[idx0].y()) * localT;
        currentX = static_cast<int>(rx * m_screenWidth);
        currentY = static_cast<int>(ry * m_screenHeight);
    } else {
        currentX = static_cast<int>(m_swipeStart.x() + (m_swipeEnd.x() - m_swipeStart.x()) * progress);
        currentY = static_cast<int>(m_swipeStart.y() + (m_swipeEnd.y() - m_swipeStart.y()) * progress);
    }

    if (m_swipeCurrentStep >= m_swipeTotalSteps) {
        m_swipeTimer->stop();
        if (m_bridge) {
            m_bridge->touchMove(3, currentX, currentY);
            m_bridge->touchUp(3);
        }
        emit touchInjected(3, currentX, currentY, false);
    } else {
        if (m_bridge) m_bridge->touchMove(3, currentX, currentY);
        emit touchInjected(3, currentX, currentY, true);
    }
}

bool InputTranslator::handleMouseMove(int deltaX, int deltaY, const QPoint &globalPos)
{
    m_lastMousePos = globalPos;

    // 1. Smart Cast: يحرك نقطة الإلقاء نحو مؤشر الماوس (أو بالإزاحة النسبية إذا كان المؤشر مقفلاً)
    if (m_smartCasting) {
        int cx = static_cast<int>(m_activeSmartCastNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_activeSmartCastNode.relY * m_screenHeight);

        qreal mx = 0.0;
        qreal my = 0.0;

        if (m_aimLockActive && (deltaX != 0 || deltaY != 0)) {
            m_smartCastAccumX += deltaX * 1.5;
            m_smartCastAccumY += deltaY * 1.5;
            mx = m_smartCastAccumX;
            my = m_smartCastAccumY;
        } else {
            mx = globalPos.x() - cx;
            my = globalPos.y() - cy;
        }

        qreal dist = std::sqrt(mx * mx + my * my);
        qreal maxR = m_activeSmartCastNode.castRadius;

        if (dist > maxR && dist > 0.001) {
            mx = (mx / dist) * maxR;
            my = (my / dist) * maxR;
            m_smartCastAccumX = mx;
            m_smartCastAccumY = my;
        }

        int targetX = std::clamp(cx + static_cast<int>(mx), 0, m_screenWidth - 1);
        int targetY = std::clamp(cy + static_cast<int>(my), 0, m_screenHeight - 1);

        if (m_bridge) m_bridge->touchMove(4, targetX, targetY);
        emit smartCastAimUpdated(m_activeSmartCastNode.id, QPointF(cx, cy), QPointF(targetX, targetY));
        emit touchInjected(4, targetX, targetY, true);
        return true;
    }

    // 2. Free Look (يعمل أثناء الضغط المستمر على الزر فقط)
    if (m_freeLookActive) {
        int cx = static_cast<int>(m_freeLookNode.relX * m_screenWidth);
        int cy = static_cast<int>(m_freeLookNode.relY * m_screenHeight);

        qreal effDx = static_cast<qreal>(deltaX);
        qreal effDy = static_cast<qreal>(deltaY);
        if (m_freeLookNode.acceleration) {
            qreal speed = std::sqrt(effDx * effDx + effDy * effDy);
            qreal factor = 1.0 + qMin(speed * 0.02, 1.5);
            effDx *= factor;
            effDy *= factor;
        }

        m_freeLookAccumX += static_cast<int>(effDx * m_freeLookNode.sensitivityX);
        m_freeLookAccumY += static_cast<int>(effDy * m_freeLookNode.sensitivityY);

        const int maxSweep = static_cast<int>(m_freeLookNode.radius > 10 ? m_freeLookNode.radius * 4.0 : 160);
        int distSq = m_freeLookAccumX * m_freeLookAccumX + m_freeLookAccumY * m_freeLookAccumY;

        if (distSq >= maxSweep * maxSweep) {
            if (m_bridge) m_bridge->touchUp(6);
            m_freeLookAccumX = 0;
            m_freeLookAccumY = 0;
            if (m_bridge) m_bridge->touchDown(6, cx, cy);
            emit touchInjected(6, cx, cy, true);
        } else {
            int tx = std::clamp(cx + m_freeLookAccumX, 0, m_screenWidth - 1);
            int ty = std::clamp(cy + m_freeLookAccumY, 0, m_screenHeight - 1);
            if (m_bridge) m_bridge->touchMove(6, tx, ty);
            emit touchInjected(6, tx, ty, true);
        }
        return true;
    }

    // 3. Aim Lock (تبديل بالضغط)
    if (m_aimLockActive && m_activeAimIndex >= 0 && m_activeAimIndex < m_nodes.size()) {
        const auto &aim = m_nodes[m_activeAimIndex];
        int cx = static_cast<int>(aim.relX * m_screenWidth);
        int cy = static_cast<int>(aim.relY * m_screenHeight);

        qreal effDx = static_cast<qreal>(deltaX);
        qreal effDy = static_cast<qreal>(deltaY);

        if (aim.acceleration) {
            qreal speed = std::sqrt(effDx * effDx + effDy * effDy);
            qreal factor = 1.0 + qMin(speed * 0.02, 1.5);
            effDx *= factor;
            effDy *= factor;
        }

        m_aimSwipeAccumX += static_cast<int>(effDx * aim.sensitivityX);
        m_aimSwipeAccumY += static_cast<int>(effDy * aim.sensitivityY);

        const int maxSweepRadius = static_cast<int>(aim.radius > 10 ? aim.radius * 4.0 : 160);
        int distSq = m_aimSwipeAccumX * m_aimSwipeAccumX + m_aimSwipeAccumY * m_aimSwipeAccumY;

        if (distSq >= maxSweepRadius * maxSweepRadius) {
            if (m_bridge) m_bridge->touchUp(1);
            m_aimSwipeAccumX = 0;
            m_aimSwipeAccumY = 0;
            if (m_bridge) m_bridge->touchDown(1, cx, cy);
            emit touchInjected(1, cx, cy, true);
        } else {
            int tx = std::clamp(cx + m_aimSwipeAccumX, 0, m_screenWidth - 1);
            int ty = std::clamp(cy + m_aimSwipeAccumY, 0, m_screenHeight - 1);
            if (m_bridge) m_bridge->touchMove(1, tx, ty);
            emit touchInjected(1, tx, ty, true);
        }
        return true;
    }

    return false;
}

bool InputTranslator::handleMousePress(Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton && m_aimLockActive && m_activeAimIndex >= 0 && m_activeAimIndex < m_nodes.size()) {
        const auto &aim = m_nodes[m_activeAimIndex];
        if (!aim.enableFireButton) return false;
        m_firing = true;
        int fx = static_cast<int>(aim.fireRelX * m_screenWidth);
        int fy = static_cast<int>(aim.fireRelY * m_screenHeight);
        if (m_bridge) m_bridge->touchDown(2, fx, fy);
        emit touchInjected(2, fx, fy, true);
        return true;
    }
    return false;
}

bool InputTranslator::handleMouseRelease(Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton && m_firing) {
        m_firing = false;
        if (m_bridge) m_bridge->touchUp(2);
        emit touchInjected(2, 0, 0, false);
        return true;
    }
    return false;
}

void InputTranslator::startBurstTap(const KeyBindingNode &node)
{
    if (m_burstTimers.contains(node.primaryScanCode)) return;

    int slot = -1;
    for (int s = 5; s < 10; ++s) {
        if (!m_scanToSlot.values().contains(s)) { slot = s; break; }
    }
    if (slot == -1) return;

    m_scanToSlot.insert(node.primaryScanCode, slot);
    const int px = static_cast<int>(node.relX * m_screenWidth);
    const int py = static_cast<int>(node.relY * m_screenHeight);

    // سرعة الضغط من 1 إلى 100 ضغطة في الثانية
    const int hz = qBound(1, node.burstHz, 100);
    const int intervalMs = qMax(10, 1000 / hz);

    // تنفيذ أول ضغطة فوراً دون انتظار
    if (m_bridge) {
        m_bridge->touchDown(slot, px, py);
        m_bridge->touchUp(slot);
    }
    emit touchInjected(slot, px, py, true);
    emit touchInjected(slot, 0, 0, false);

    QTimer *timer = new QTimer(this);
    timer->setInterval(intervalMs);

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
    m_burstTimers.insert(node.primaryScanCode, timer);
}

void InputTranslator::stopBurstTap(quint32 scanCode)
{
    if (QTimer *t = m_burstTimers.take(scanCode)) {
        t->stop();
        t->deleteLater();
    }
    if (m_scanToSlot.contains(scanCode)) {
        int slot = m_scanToSlot.take(scanCode);
        if (m_bridge) m_bridge->touchUp(slot);
        emit touchInjected(slot, 0, 0, false);
    }
}

void InputTranslator::startEdgeScroll(int dx, int dy, const KeyBindingNode &node)
{
    m_edgeScrollDirX = dx;
    m_edgeScrollDirY = dy;
    m_edgeScrollActive = true;

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

    qreal speed = 12.0;
    for (const auto &node : m_nodes) {
        if (node.type == KeyNodeType::EdgeScroll) { speed = node.edgeScrollSpeed; break; }
    }

    static int accumX = 0;
    static int accumY = 0;
    accumX += static_cast<int>(m_edgeScrollDirX * speed);
    accumY += static_cast<int>(m_edgeScrollDirY * speed);

    if (std::abs(accumX) > 200 || std::abs(accumY) > 200) {
        if (m_bridge) m_bridge->touchUp(9);
        accumX = 0; accumY = 0;
        if (m_bridge) m_bridge->touchDown(9, m_screenWidth / 2, m_screenHeight / 2);
    } else {
        int tx = m_screenWidth / 2 + accumX;
        int ty = m_screenHeight / 2 + accumY;
        if (m_bridge) m_bridge->touchMove(9, tx, ty);
        emit touchInjected(9, tx, ty, true);
    }
}
