#include "InputMapper.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>

InputMapper::InputMapper(struct wlr_seat *seat, struct wlr_relative_pointer_manager_v1 *relPointerMgr)
: m_seat(seat)
, m_relPointerMgr(relPointerMgr)
{
    setupDefaultProfile();
}

InputMapper::~InputMapper() = default;

uint32_t InputMapper::getTimestampMs() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

int32_t InputMapper::allocateSlot() {
    for (int32_t s = 0; s < 10; ++s) {
        if (m_activeSlots.find(s) == m_activeSlots.end()) {
            m_activeSlots.insert(s);
            return s;
        }
    }
    return 0;
}

void InputMapper::releaseSlot(int32_t slot) {
    if (slot >= 0) {
        m_activeSlots.erase(slot);
    }
}

void InputMapper::setGeometry(const ViewportRect &viewport, int androidWidth, int androidHeight) {
    m_viewport = viewport;
    m_androidWidth = std::max(1, androidWidth);
    m_androidHeight = std::max(1, androidHeight);
}

void InputMapper::windowToAndroid(int winX, int winY, double &outAndroidX, double &outAndroidY) const {
    if (m_viewport.width <= 0 || m_viewport.height <= 0) {
        outAndroidX = 0.0;
        outAndroidY = 0.0;
        return;
    }
    double relX = std::clamp(static_cast<double>(winX - m_viewport.x) / m_viewport.width, 0.0, 1.0);
    double relY = std::clamp(static_cast<double>(winY - m_viewport.y) / m_viewport.height, 0.0, 1.0);
    outAndroidX = relX * m_androidWidth;
    outAndroidY = relY * m_androidHeight;
}

void InputMapper::windowToNormalized(int winX, int winY, float &outNormX, float &outNormY) const {
    if (m_viewport.width <= 0 || m_viewport.height <= 0) {
        outNormX = 0.0f;
        outNormY = 0.0f;
        return;
    }
    outNormX = std::clamp(static_cast<float>(winX - m_viewport.x) / m_viewport.width, 0.0f, 1.0f);
    outNormY = std::clamp(static_cast<float>(winY - m_viewport.y) / m_viewport.height, 0.0f, 1.0f);
}

void InputMapper::setupDefaultProfile() {
    m_tapNodes.clear();
    m_tapNodes.push_back(KeyNode{"jump", WlrKeyNodeType::Tap, 65, 0.88f, 0.78f, 0});
    m_tapNodes.push_back(KeyNode{"reload", WlrKeyNodeType::Tap, 27, 0.88f, 0.52f, 0});
    m_tapNodes.push_back(KeyNode{"crouch", WlrKeyNodeType::Tap, 54, 0.82f, 0.86f, 0});
    m_tapNodes.push_back(KeyNode{"interact", WlrKeyNodeType::Tap, 26, 0.75f, 0.40f, 0});

    m_dpad.centerX = 0.18f;
    m_dpad.centerY = 0.70f;
    m_dpad.radius = 0.08f;
    m_dpad.scanUp = 25;
    m_dpad.scanLeft = 38;
    m_dpad.scanDown = 39;
    m_dpad.scanRight = 40;
    m_dpad.touchSlot = 0;

    m_aimNodes.clear();
    AimLockNodeConfig aim;
    aim.id = "aim_lock_1";
    aim.toggleScanCode = 67;
    aim.centerX = 0.50f;
    aim.centerY = 0.50f;
    aim.fireX = 0.82f;
    aim.fireY = 0.64f;
    aim.sensitivityX = 1.2f;
    aim.sensitivityY = 1.2f;
    aim.acceleration = false;
    aim.maxSweepRadius = 0.18f;
    aim.cameraTouchSlot = 0;
    aim.fireTouchSlot = 1;
    m_aimNodes.push_back(aim);
}

const AimLockNodeConfig* InputMapper::activeAimNode() const {
    if (m_aimLockActive && m_activeAimIndex >= 0 && m_activeAimIndex < static_cast<int>(m_aimNodes.size())) {
        return &m_aimNodes[m_activeAimIndex];
    }
    return nullptr;
}

const FreeLookConfig* InputMapper::activeFreeLookNode() const {
    if (m_freeLookActive && m_activeFreeLookIndex >= 0 && m_activeFreeLookIndex < static_cast<int>(m_freeLookNodes.size())) {
        return &m_freeLookNodes[m_activeFreeLookIndex];
    }
    return nullptr;
}

void InputMapper::setAimLockActive(bool active, int nodeIndex) {
    if (active && (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_aimNodes.size()))) {
        return;
    }

    if (m_aimLockActive == active && (!active || m_activeAimIndex == nodeIndex)) {
        return;
    }

    uint32_t now = getTimestampMs();

    if (m_aimLockActive && m_seat) {
        if (m_pendingReleaseCameraSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_pendingReleaseCameraSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_pendingReleaseCameraSlot);
            m_pendingReleaseCameraSlot = -1;
        }
        if (m_activeAimCameraSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now + 1, m_activeAimCameraSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimCameraSlot);
            m_activeAimCameraSlot = -1;
        }
        if (m_firing && m_activeAimFireSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now + 2, m_activeAimFireSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimFireSlot);
            m_activeAimFireSlot = -1;
            m_firing = false;
        }
    }

    m_aimLockActive = active;
    m_activeAimIndex = active ? nodeIndex : -1;
    m_accumAimX = 0.0f;
    m_accumAimY = 0.0f;
}

void InputMapper::tick(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 1. Burst Tap
    for (auto &kv : m_activeBursts) {
        ActiveBurst &b = kv.second;
        uint32_t intervalMs = static_cast<uint32_t>(std::max(10, 1000 / std::clamp(b.cfg.burstHz, 1, 100)));
        if (b.isDown && (now - b.lastClickMs >= intervalMs / 2)) {
            wlr_seat_touch_notify_up(m_seat, now, b.slot);
            wlr_seat_touch_notify_frame(m_seat);
            b.isDown = false;
        } else if (!b.isDown && (now - b.lastClickMs >= intervalMs)) {
            double tx = b.cfg.relX * m_androidWidth;
            double ty = b.cfg.relY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, b.slot, tx, ty);
            wlr_seat_touch_notify_frame(m_seat);
            b.isDown = true;
            b.lastClickMs = now;
        }
    }

    // 2. Swipe
    for (auto it = m_activeSwipes.begin(); it != m_activeSwipes.end(); ) {
        uint32_t elapsed = now - it->startMs;
        float progress = std::clamp(static_cast<float>(elapsed) / std::max(1, it->cfg.durationMs), 0.0f, 1.0f);

        float rx = 0.0f, ry = 0.0f;
        if (it->cfg.pathPoints.size() >= 2) {
            float scaled = progress * (it->cfg.pathPoints.size() - 1);
            size_t idx0 = std::min(static_cast<size_t>(scaled), it->cfg.pathPoints.size() - 1);
            size_t idx1 = std::min(idx0 + 1, it->cfg.pathPoints.size() - 1);
            float localT = scaled - idx0;
            rx = it->cfg.pathPoints[idx0].first + (it->cfg.pathPoints[idx1].first - it->cfg.pathPoints[idx0].first) * localT;
            ry = it->cfg.pathPoints[idx0].second + (it->cfg.pathPoints[idx1].second - it->cfg.pathPoints[idx0].second) * localT;
        } else {
            rx = it->cfg.startX + (it->cfg.endX - it->cfg.startX) * progress;
            ry = it->cfg.startY + (it->cfg.endY - it->cfg.startY) * progress;
        }

        double ax = std::clamp(static_cast<double>(rx * m_androidWidth), 1.0, static_cast<double>(m_androidWidth - 2));
        double ay = std::clamp(static_cast<double>(ry * m_androidHeight), 1.0, static_cast<double>(m_androidHeight - 2));

        wlr_seat_touch_notify_motion(m_seat, now, it->slot, ax, ay);
        wlr_seat_touch_notify_frame(m_seat);

        if (progress >= 1.0f) {
            wlr_seat_touch_notify_up(m_seat, now + 1, it->slot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(it->slot);
            it = m_activeSwipes.erase(it);
        } else {
            ++it;
        }
    }
}

void InputMapper::updateDpad(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    bool up    = m_heldScanCodes.count(m_dpad.scanUp) > 0;
    bool down  = m_heldScanCodes.count(m_dpad.scanDown) > 0;
    bool left  = m_heldScanCodes.count(m_dpad.scanLeft) > 0;
    bool right = m_heldScanCodes.count(m_dpad.scanRight) > 0;

    float dx = 0.0f;
    float dy = 0.0f;

    if (up) dy -= 1.0f;
    if (down) dy += 1.0f;
    if (left) dx -= 1.0f;
    if (right) dx += 1.0f;

    uint32_t now = getTimestampMs();

    if (dx == 0.0f && dy == 0.0f) {
        if (m_dpadActive && m_activeDpadSlot >= 0) {
            m_dpadActive = false;
            wlr_seat_touch_notify_up(m_seat, now, m_activeDpadSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeDpadSlot);
            m_activeDpadSlot = -1;
        }
        return;
    }

    float len = std::sqrt(dx * dx + dy * dy);
    dx /= len;
    dy /= len;

    double centerX = m_dpad.centerX * m_androidWidth;
    double centerY = m_dpad.centerY * m_androidHeight;
    double targetX = (m_dpad.centerX + dx * m_dpad.radius) * m_androidWidth;
    double targetY = (m_dpad.centerY + dy * m_dpad.radius) * m_androidHeight;

    if (!m_dpadActive) {
        m_dpadActive = true;
        m_activeDpadSlot = allocateSlot();
        wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeDpadSlot, centerX, centerY);
        wlr_seat_touch_notify_frame(m_seat);
        wlr_seat_touch_notify_motion(m_seat, now + 1, m_activeDpadSlot, targetX, targetY);
        wlr_seat_touch_notify_frame(m_seat);
    } else if (m_activeDpadSlot >= 0) {
        wlr_seat_touch_notify_motion(m_seat, now, m_activeDpadSlot, targetX, targetY);
        wlr_seat_touch_notify_frame(m_seat);
    }
}


void InputMapper::sendRecentsCombo(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(m_seat);
    if (kbd) {
        wlr_seat_keyboard_notify_enter(m_seat, rootSurf, kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
    } else {
        wlr_seat_keyboard_notify_enter(m_seat, rootSurf, nullptr, 0, nullptr);
    }

    uint32_t now = getTimestampMs();
    wlr_seat_keyboard_notify_key(m_seat, now, 56, WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_modifiers(m_seat, nullptr);
    wlr_seat_keyboard_notify_key(m_seat, now + 10, 15, WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(m_seat, now + 25, 15, WL_KEYBOARD_KEY_STATE_RELEASED);
    wlr_seat_keyboard_notify_key(m_seat, now + 35, 56, WL_KEYBOARD_KEY_STATE_RELEASED);
    wlr_seat_keyboard_notify_modifiers(m_seat, nullptr);
}

void InputMapper::sendSystemKey(uint32_t evdevKeyCode, struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(m_seat);
    if (kbd) {
        wlr_seat_keyboard_notify_enter(m_seat, rootSurf, kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
    } else {
        wlr_seat_keyboard_notify_enter(m_seat, rootSurf, nullptr, 0, nullptr);
    }

    uint32_t now = getTimestampMs();
    wlr_seat_keyboard_notify_key(m_seat, now, evdevKeyCode, WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(m_seat, now + 15, evdevKeyCode, WL_KEYBOARD_KEY_STATE_RELEASED);
}

bool InputMapper::handleScanCodeDown(uint32_t scanCode, struct wlr_surface *surface) {
    if (scanCode == 0) return false;

    for (int i = 0; i < static_cast<int>(m_aimNodes.size()); ++i) {
        if (m_aimNodes[i].toggleScanCode == scanCode) {
            if (m_aimLockActive && m_activeAimIndex == i) {
                setAimLockActive(false, -1);
            } else {
                setAimLockActive(true, i);
            }
            return true;
        }
    }

    if (scanCode == 9) {
        bool handledEsc = false;
        if (m_aimLockActive) {
            setAimLockActive(false, -1);
            handledEsc = true;
        }
        if (m_freeLookActive && m_seat) {
            uint32_t now = getTimestampMs();
            m_freeLookActive = false;
            m_activeFreeLookIndex = -1;
            if (m_pendingReleaseFreeLookSlot >= 0) {
                wlr_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
                releaseSlot(m_pendingReleaseFreeLookSlot);
                m_pendingReleaseFreeLookSlot = -1;
            }
            if (m_activeFreeLookSlot >= 0) {
                wlr_seat_touch_notify_up(m_seat, now + 1, m_activeFreeLookSlot);
                releaseSlot(m_activeFreeLookSlot);
                m_activeFreeLookSlot = -1;
            }
            wlr_seat_touch_notify_frame(m_seat);
            handledEsc = true;
        }
        if (handledEsc) return true;
    }

    m_heldScanCodes.insert(scanCode);

    if (scanCode == m_dpad.scanUp || scanCode == m_dpad.scanDown ||
        scanCode == m_dpad.scanLeft || scanCode == m_dpad.scanRight) {
        updateDpad(surface);
        return true;
    }

    if (!m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 1. Free Look
    for (int i = 0; i < static_cast<int>(m_freeLookNodes.size()); ++i) {
        if (m_freeLookNodes[i].holdScanCode == scanCode) {
            if (!m_freeLookActive) {
                m_freeLookActive = true;
                m_activeFreeLookIndex = i;
                m_accumFreeLookX = 0.0f;
                m_accumFreeLookY = 0.0f;
            }
            return true;
        }
    }

    // 2. Burst Tap
    for (const auto &bn : m_burstNodes) {
        if (bn.scanCode == scanCode) {
            if (m_activeBursts.find(scanCode) == m_activeBursts.end()) {
                ActiveBurst ab;
                ab.cfg = bn;
                ab.slot = allocateSlot();
                ab.lastClickMs = now;
                ab.isDown = true;
                m_activeBursts[scanCode] = ab;

                double tx = bn.relX * m_androidWidth;
                double ty = bn.relY * m_androidHeight;
                wlr_seat_touch_notify_down(m_seat, rootSurf, now, ab.slot, tx, ty);
                wlr_seat_touch_notify_frame(m_seat);
            }
            return true;
        }
    }

    // 3. Swipe
    for (const auto &sw : m_swipeNodes) {
        if (sw.scanCode == scanCode) {
            ActiveSwipe as;
            as.cfg = sw;
            as.slot = allocateSlot();
            as.startMs = now;
            m_activeSwipes.push_back(as);

            double sx = sw.startX * m_androidWidth;
            double sy = sw.startY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, as.slot, sx, sy);
            wlr_seat_touch_notify_frame(m_seat);
            return true;
        }
    }

    // 4. Smart Cast
    for (const auto &sc : m_smartCastNodes) {
        if (sc.scanCode == scanCode) {
            if (!m_smartCasting) {
                m_smartCasting = true;
                m_activeSmartCast = sc;
                m_activeSmartCastSlot = allocateSlot();
                m_smartCastAccumX = 0.0f;
                m_smartCastAccumY = 0.0f;

                double cx = sc.centerX * m_androidWidth;
                double cy = sc.centerY * m_androidHeight;
                wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeSmartCastSlot, cx, cy);
                wlr_seat_touch_notify_frame(m_seat);
            }
            return true;
        }
    }

    // 5. Tap
    for (const auto &node : m_tapNodes) {
        if (node.scanCode == scanCode) {
            if (m_scanCodeToSlot.find(scanCode) != m_scanCodeToSlot.end()) {
                return true;
            }
            int32_t slot = allocateSlot();
            m_scanCodeToSlot[scanCode] = slot;

            double tx = node.relX * m_androidWidth;
            double ty = node.relY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, slot, tx, ty);
            wlr_seat_touch_notify_frame(m_seat);
            return true;
        }
    }

    return false;
}

bool InputMapper::handleScanCodeUp(uint32_t scanCode, struct wlr_surface *surface) {
    if (scanCode == 0) return false;
    m_heldScanCodes.erase(scanCode);

    if (scanCode == m_dpad.scanUp || scanCode == m_dpad.scanDown ||
        scanCode == m_dpad.scanLeft || scanCode == m_dpad.scanRight) {
        updateDpad(surface);
        return true;
    }

    if (!m_seat) return false;
    uint32_t now = getTimestampMs();
    // Free Look release
    if (m_freeLookActive && m_activeFreeLookIndex >= 0 &&
        m_freeLookNodes[m_activeFreeLookIndex].holdScanCode == scanCode) {
        m_freeLookActive = false;
        m_activeFreeLookIndex = -1;
        if (m_pendingReleaseFreeLookSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
            releaseSlot(m_pendingReleaseFreeLookSlot);
            m_pendingReleaseFreeLookSlot = -1;
        }
        if (m_activeFreeLookSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now + 1, m_activeFreeLookSlot);
            releaseSlot(m_activeFreeLookSlot);
            m_activeFreeLookSlot = -1;
        }
        wlr_seat_touch_notify_frame(m_seat);
        return true;
    }

    // Burst Tap release
    auto burstIt = m_activeBursts.find(scanCode);
    if (burstIt != m_activeBursts.end()) {
        if (burstIt->second.isDown) {
            wlr_seat_touch_notify_up(m_seat, now, burstIt->second.slot);
            wlr_seat_touch_notify_frame(m_seat);
        }
        releaseSlot(burstIt->second.slot);
        m_activeBursts.erase(burstIt);
        return true;
    }

    // Smart Cast release
    if (m_smartCasting && m_activeSmartCast.scanCode == scanCode) {
        m_smartCasting = false;
        if (m_activeSmartCastSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_activeSmartCastSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeSmartCastSlot);
            m_activeSmartCastSlot = -1;
        }
        return true;
    }

    auto it = m_scanCodeToSlot.find(scanCode);
    if (it != m_scanCodeToSlot.end()) {
        int32_t slot = it->second;
        m_scanCodeToSlot.erase(it);
        wlr_seat_touch_notify_up(m_seat, now, slot);
        wlr_seat_touch_notify_frame(m_seat);
        releaseSlot(slot);
        return true;
    }

    return false;
}

bool InputMapper::handleMouseDown(int button, int winX, int winY, struct wlr_surface *surface) {
    if (!m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    const auto *aim = activeAimNode();
    if (aim) {
        if (button == 1 && !m_firing) {
            m_firing = true;
            m_activeAimFireSlot = allocateSlot();
            double fx = aim->fireX * m_androidWidth;
            double fy = aim->fireY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeAimFireSlot, fx, fy);
            wlr_seat_touch_notify_frame(m_seat);
            return true;
        }
        return false;
    }

    if (button == 1 && !m_isTouching) {
        m_isTouching = true;
        m_activeMouseSlot = allocateSlot();
        double ax, ay;
        windowToAndroid(winX, winY, ax, ay);
        wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeMouseSlot, ax, ay);
        wlr_seat_touch_notify_frame(m_seat);
        return true;
    }

    return false;
}

bool InputMapper::handleMouseUp(int button, int winX, int winY, struct wlr_surface *surface) {
    (void)winX;
    (void)winY;
    (void)surface;

    if (!m_seat) return false;
    uint32_t now = getTimestampMs();

    const auto *aim = activeAimNode();
    if (aim) {
        if (button == 1 && m_firing) {
            m_firing = false;
            if (m_activeAimFireSlot >= 0) {
                wlr_seat_touch_notify_up(m_seat, now, m_activeAimFireSlot);
                wlr_seat_touch_notify_frame(m_seat);
                releaseSlot(m_activeAimFireSlot);
                m_activeAimFireSlot = -1;
            }
            return true;
        }
        return false;
    }

    if (button == 1 && m_isTouching) {
        m_isTouching = false;
        if (m_activeMouseSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_activeMouseSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeMouseSlot);
            m_activeMouseSlot = -1;
        }
        return true;
    }

    return false;
}

bool InputMapper::handleMouseMove(int winX, int winY, int deltaX, int deltaY, struct wlr_surface *surface) {
    if (!m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 1. Smart Cast
    if (m_smartCasting && m_activeSmartCastSlot >= 0) {
        double cx = m_activeSmartCast.centerX * m_androidWidth;
        double cy = m_activeSmartCast.centerY * m_androidHeight;

        float mx = 0.0f, my = 0.0f;
        if (m_aimLockActive && (deltaX != 0 || deltaY != 0)) {
            m_smartCastAccumX += deltaX * 1.5f;
            m_smartCastAccumY += deltaY * 1.5f;
            mx = m_smartCastAccumX;
            my = m_smartCastAccumY;
        } else {
            double ax, ay;
            windowToAndroid(winX, winY, ax, ay);
            mx = static_cast<float>(ax - cx);
            my = static_cast<float>(ay - cy);
        }

        float dist = std::sqrt(mx * mx + my * my);
        float maxR = m_activeSmartCast.castRadius;
        if (dist > maxR && dist > 0.001f) {
            mx = (mx / dist) * maxR;
            my = (my / dist) * maxR;
            m_smartCastAccumX = mx;
            m_smartCastAccumY = my;
        }

        double tx = std::clamp(cx + mx, 2.0, static_cast<double>(m_androidWidth - 3));
        double ty = std::clamp(cy + my, 2.0, static_cast<double>(m_androidHeight - 3));
        wlr_seat_touch_notify_motion(m_seat, now, m_activeSmartCastSlot, tx, ty);
        wlr_seat_touch_notify_frame(m_seat);
        return true;
    }

    // 2. Free Look
    if (m_freeLookActive && m_activeFreeLookIndex >= 0 && m_activeFreeLookIndex < static_cast<int>(m_freeLookNodes.size())) {
        const auto &fl = m_freeLookNodes[m_activeFreeLookIndex];
        if (m_pendingReleaseFreeLookSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_pendingReleaseFreeLookSlot);
            m_pendingReleaseFreeLookSlot = -1;
            now += 1;
        }

        float effDx = static_cast<float>(deltaX);
        float effDy = static_cast<float>(deltaY);
        if (fl.acceleration) {
            float speed = std::sqrt(effDx * effDx + effDy * effDy);
            float factor = 1.0f + std::min(speed * 0.02f, 1.5f);
            effDx *= factor;
            effDy *= factor;
        }

        float normDx = (effDx * fl.sensitivityX) / std::max(1, m_viewport.width);
        float normDy = (effDy * fl.sensitivityY) / std::max(1, m_viewport.height);
        const float baseX = std::clamp(fl.centerX, 0.05f, 0.95f);
        const float baseY = std::clamp(fl.centerY, 0.05f, 0.95f);

        if (m_activeFreeLookSlot < 0) {
            m_activeFreeLookSlot = allocateSlot();
            m_accumFreeLookX = 0.0f;
            m_accumFreeLookY = 0.0f;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeFreeLookSlot, baseX * m_androidWidth, baseY * m_androidHeight);
            wlr_seat_touch_notify_frame(m_seat);
            now += 1;
        }

        float nextX = baseX + m_accumFreeLookX + normDx;
        float nextY = baseY + m_accumFreeLookY + normDy;
        if (nextX <= 0.04f || nextX >= 0.96f || nextY <= 0.04f || nextY >= 0.96f) {
            int32_t oldSlot = m_activeFreeLookSlot;
            int32_t newSlot = allocateSlot();
            m_accumFreeLookX = normDx;
            m_accumFreeLookY = normDy;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, newSlot, baseX * m_androidWidth, baseY * m_androidHeight);
            wlr_seat_touch_notify_frame(m_seat);
            double tx = std::clamp(static_cast<double>((baseX + m_accumFreeLookX) * m_androidWidth), 2.0, static_cast<double>(m_androidWidth - 3));
            double ty = std::clamp(static_cast<double>((baseY + m_accumFreeLookY) * m_androidHeight), 2.0, static_cast<double>(m_androidHeight - 3));
            wlr_seat_touch_notify_motion(m_seat, now + 1, newSlot, tx, ty);
            wlr_seat_touch_notify_frame(m_seat);
            m_pendingReleaseFreeLookSlot = oldSlot;
            m_activeFreeLookSlot = newSlot;
        } else {
            m_accumFreeLookX += normDx;
            m_accumFreeLookY += normDy;
            double tx = std::clamp(static_cast<double>((baseX + m_accumFreeLookX) * m_androidWidth), 2.0, static_cast<double>(m_androidWidth - 3));
            double ty = std::clamp(static_cast<double>((baseY + m_accumFreeLookY) * m_androidHeight), 2.0, static_cast<double>(m_androidHeight - 3));
            wlr_seat_touch_notify_motion(m_seat, now, m_activeFreeLookSlot, tx, ty);
            wlr_seat_touch_notify_frame(m_seat);
        }
        return true;
    }

    // 3. Aim Lock
    const auto *aim = activeAimNode();
    if (aim) {
        if (m_pendingReleaseCameraSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_pendingReleaseCameraSlot);
            wlr_seat_touch_notify_frame(m_seat);
            releaseSlot(m_pendingReleaseCameraSlot);
            m_pendingReleaseCameraSlot = -1;
            now += 1;
        }

        float effDx = static_cast<float>(deltaX);
        float effDy = static_cast<float>(deltaY);

        if (aim->acceleration) {
            float speed = std::sqrt(effDx * effDx + effDy * effDy);
            float factor = 1.0f + std::min(speed * 0.02f, 1.5f);
            effDx *= factor;
            effDy *= factor;
        }

        float normDx = (effDx * aim->sensitivityX) / std::max(1, m_viewport.width);
        float normDy = (effDy * aim->sensitivityY) / std::max(1, m_viewport.height);

        const float baseCenterX = std::clamp(aim->centerX, 0.05f, 0.95f);
        const float baseCenterY = std::clamp(aim->centerY, 0.05f, 0.95f);

        if (m_activeAimCameraSlot < 0) {
            m_activeAimCameraSlot = allocateSlot();
            m_accumAimX = 0.0f;
            m_accumAimY = 0.0f;
            double cx = baseCenterX * m_androidWidth;
            double cy = baseCenterY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeAimCameraSlot, cx, cy);
            wlr_seat_touch_notify_frame(m_seat);
            now += 1;
        }

        float nextX = baseCenterX + m_accumAimX + normDx;
        float nextY = baseCenterY + m_accumAimY + normDy;

        if (nextX <= 0.04f || nextX >= 0.96f || nextY <= 0.04f || nextY >= 0.96f) {
            int32_t oldSlot = m_activeAimCameraSlot;
            int32_t newSlot = allocateSlot();

            m_accumAimX = normDx;
            m_accumAimY = normDy;

            double cx = baseCenterX * m_androidWidth;
            double cy = baseCenterY * m_androidHeight;

            wlr_seat_touch_notify_down(m_seat, rootSurf, now, newSlot, cx, cy);
            wlr_seat_touch_notify_frame(m_seat);

            double tx = std::clamp(static_cast<double>((baseCenterX + m_accumAimX) * m_androidWidth), 2.0, static_cast<double>(m_androidWidth - 3));
            double ty = std::clamp(static_cast<double>((baseCenterY + m_accumAimY) * m_androidHeight), 2.0, static_cast<double>(m_androidHeight - 3));
            wlr_seat_touch_notify_motion(m_seat, now + 1, newSlot, tx, ty);
            wlr_seat_touch_notify_frame(m_seat);

            m_pendingReleaseCameraSlot = oldSlot;
            m_activeAimCameraSlot = newSlot;
        } else {
            m_accumAimX += normDx;
            m_accumAimY += normDy;
            double targetX = std::clamp(static_cast<double>((baseCenterX + m_accumAimX) * m_androidWidth), 2.0, static_cast<double>(m_androidWidth - 3));
            double targetY = std::clamp(static_cast<double>((baseCenterY + m_accumAimY) * m_androidHeight), 2.0, static_cast<double>(m_androidHeight - 3));
            wlr_seat_touch_notify_motion(m_seat, now, m_activeAimCameraSlot, targetX, targetY);
            wlr_seat_touch_notify_frame(m_seat);
        }
        return true;
    }

    if (!m_isTouching || m_activeMouseSlot < 0) return false;

    double ax, ay;
    windowToAndroid(winX, winY, ax, ay);
    wlr_seat_touch_notify_motion(m_seat, now, m_activeMouseSlot, ax, ay);
    wlr_seat_touch_notify_frame(m_seat);
    return true;
}
