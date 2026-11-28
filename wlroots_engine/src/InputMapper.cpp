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
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_pendingReleaseCameraSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_pendingReleaseCameraSlot);
            m_pendingReleaseCameraSlot = -1;
        }
        if (m_activeAimCameraSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now + 1, m_activeAimCameraSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimCameraSlot);
            m_activeAimCameraSlot = -1;
        }
        if (m_firing && m_activeAimFireSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now + 2, m_activeAimFireSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimFireSlot);
            m_activeAimFireSlot = -1;
            m_firing = false;
        }
    }

    m_aimLockActive = active;
    m_activeAimIndex = active ? nodeIndex : -1;
    m_accumAimX = 0.0f;
    m_accumAimY = 0.0f;
    m_lastAimMoveMs = 0;
    m_smoothAimDx = 0.0f;
    m_smoothAimDy = 0.0f;
    m_aimNeedsTouchDown = true;
    if (!active && !m_aimSuspendedByFreeLook) {
        m_suspendedAimIndex = -1;
    }
}

void InputMapper::tick(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 0. Aim Lock Idle Reset: silently reset touch point to anchor after 450ms of mouse idle
    if (m_aimLockActive && !m_freeLookActive && m_activeAimCameraSlot >= 0 && m_lastAimMoveMs > 0) {
        if (now - m_lastAimMoveMs >= 450) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_activeAimCameraSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimCameraSlot);
            m_activeAimCameraSlot = -1;
            m_accumAimX = 0.0f;
            m_accumAimY = 0.0f;
            m_lastAimMoveMs = 0;
        }
    }

    // 1. Burst Tap (supports triggering multiple Burst buttons simultaneously)
    for (auto &kv : m_activeBursts) {
        for (ActiveBurst &b : kv.second) {
            uint32_t intervalMs = static_cast<uint32_t>(std::max(10, 1000 / std::clamp(b.cfg.burstHz, 1, 100)));
            if (b.isDown && (now - b.lastClickMs >= intervalMs / 2)) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, b.slot);
                wlr_bridge_seat_touch_notify_frame(m_seat);
                b.isDown = false;
            } else if (!b.isDown && (now - b.lastClickMs >= intervalMs)) {
                double tx = b.cfg.relX * m_androidWidth;
                double ty = b.cfg.relY * m_androidHeight;
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, b.slot, tx, ty);
                wlr_bridge_seat_touch_notify_frame(m_seat);
                b.isDown = true;
                b.lastClickMs = now;
            }
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

        wlr_bridge_seat_touch_notify_motion(m_seat, now, it->slot, ax, ay);
        wlr_bridge_seat_touch_notify_frame(m_seat);

        if (progress >= 1.0f) {
            wlr_bridge_seat_touch_notify_up(m_seat, now + 1, it->slot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(it->slot);
            it = m_activeSwipes.erase(it);
        } else {
            ++it;
        }
    }
}

void InputMapper::updateDpad(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
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
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_activeDpadSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
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
        wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, m_activeDpadSlot, centerX, centerY);
        wlr_bridge_seat_touch_notify_frame(m_seat);
        wlr_bridge_seat_touch_notify_motion(m_seat, now + 1, m_activeDpadSlot, targetX, targetY);
        wlr_bridge_seat_touch_notify_frame(m_seat);
    } else if (m_activeDpadSlot >= 0) {
        wlr_bridge_seat_touch_notify_motion(m_seat, now, m_activeDpadSlot, targetX, targetY);
        wlr_bridge_seat_touch_notify_frame(m_seat);
    }
}


void InputMapper::sendRecentsCombo(struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    wlr_bridge_seat_keyboard_enter(m_seat, rootSurf);

    uint32_t now = getTimestampMs();
    wlr_bridge_seat_keyboard_notify_key(m_seat, now, 56, 1);
    wlr_bridge_seat_keyboard_notify_modifiers(m_seat);
    wlr_bridge_seat_keyboard_notify_key(m_seat, now + 10, 15, 1);
    wlr_bridge_seat_keyboard_notify_key(m_seat, now + 25, 15, 0);
    wlr_bridge_seat_keyboard_notify_key(m_seat, now + 35, 56, 0);
    wlr_bridge_seat_keyboard_notify_modifiers(m_seat);
}

void InputMapper::sendSystemKey(uint32_t evdevKeyCode, struct wlr_surface *surface) {
    if (!m_seat || !surface) return;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    wlr_bridge_seat_keyboard_enter(m_seat, rootSurf);

    uint32_t now = getTimestampMs();
    wlr_bridge_seat_keyboard_notify_key(m_seat, now, evdevKeyCode, 1);
    wlr_bridge_seat_keyboard_notify_key(m_seat, now + 15, evdevKeyCode, 0);
}

void InputMapper::forwardNativeKey(uint32_t scanCode, bool pressed, struct wlr_surface *surface) {
    if (!m_seat || !surface || scanCode == 0) return;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;

    wlr_bridge_seat_keyboard_enter(m_seat, rootSurf);

    uint32_t now = getTimestampMs();
    uint32_t evdevKey = (scanCode >= 8) ? (scanCode - 8) : scanCode;
    wlr_bridge_seat_keyboard_notify_key(m_seat, now, evdevKey, pressed ? 1 : 0);
    wlr_bridge_seat_keyboard_notify_modifiers(m_seat);
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
                wlr_bridge_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
                releaseSlot(m_pendingReleaseFreeLookSlot);
                m_pendingReleaseFreeLookSlot = -1;
            }
            if (m_activeFreeLookSlot >= 0) {
                wlr_bridge_seat_touch_notify_up(m_seat, now + 1, m_activeFreeLookSlot);
                releaseSlot(m_activeFreeLookSlot);
                m_activeFreeLookSlot = -1;
            }
            wlr_bridge_seat_touch_notify_frame(m_seat);
            handledEsc = true;
        }
        if (handledEsc) return true;
    }

    m_heldScanCodes.insert(scanCode);
    bool handled = false;

    if (scanCode == m_dpad.scanUp || scanCode == m_dpad.scanDown ||
        scanCode == m_dpad.scanLeft || scanCode == m_dpad.scanRight) {
        updateDpad(surface);
        handled = true;
    }

    if (!m_seat || !surface) return handled;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 1. Free Look
    for (int i = 0; i < static_cast<int>(m_freeLookNodes.size()); ++i) {
        if (m_freeLookNodes[i].holdScanCode == scanCode) {
            if (!m_freeLookActive) {
                if (m_aimLockActive) {
                    m_aimSuspendedByFreeLook = true;
                    m_suspendedAimIndex = m_activeAimIndex;
                    setAimLockActive(false, -1);
                    m_aimSuspendedByFreeLook = true;
                }
                m_freeLookActive = true;
                m_activeFreeLookIndex = i;
                m_accumFreeLookX = 0.0f;
                m_accumFreeLookY = 0.0f;
            }
            handled = true;
            break;
        }
    }

    // 2. Burst Tap (trigger all Burst buttons bound to this key simultaneously)
    if (m_activeBursts.find(scanCode) == m_activeBursts.end()) {
        std::vector<ActiveBurst> bursts;
        for (const auto &bn : m_burstNodes) {
            if (bn.scanCode == scanCode) {
                ActiveBurst ab;
                ab.cfg = bn;
                ab.slot = allocateSlot();
                ab.lastClickMs = now;
                ab.isDown = true;
                bursts.push_back(ab);

                double tx = bn.relX * m_androidWidth;
                double ty = bn.relY * m_androidHeight;
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, ab.slot, tx, ty);
            }
        }
        if (!bursts.empty()) {
            wlr_bridge_seat_touch_notify_frame(m_seat);
            m_activeBursts[scanCode] = std::move(bursts);
            handled = true;
        }
    } else {
        handled = true;
    }

    // 3. Swipe (trigger all Swipe paths bound to this key simultaneously)
    bool anySwipe = false;
    for (const auto &sw : m_swipeNodes) {
        if (sw.scanCode == scanCode) {
            ActiveSwipe as;
            as.cfg = sw;
            as.slot = allocateSlot();
            as.startMs = now;
            m_activeSwipes.push_back(as);

            double sx = sw.startX * m_androidWidth;
            double sy = sw.startY * m_androidHeight;
            wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, as.slot, sx, sy);
            anySwipe = true;
        }
    }
    if (anySwipe) {
        wlr_bridge_seat_touch_notify_frame(m_seat);
        handled = true;
    }

    // 4. Smart Cast (trigger all Smart Cast buttons bound to this key simultaneously)
    bool anySmartCast = false;
    for (const auto &sc : m_smartCastNodes) {
        if (sc.scanCode == scanCode) {
            bool alreadyActive = false;
            for (const auto &asc : m_activeSmartCasts) {
                if (asc.cfg.id == sc.id && asc.cfg.scanCode == scanCode) {
                    alreadyActive = true;
                    break;
                }
            }
            if (!alreadyActive) {
                ActiveSmartCast asc;
                asc.cfg = sc;
                asc.slot = allocateSlot();
                asc.accumX = 0.0f;
                asc.accumY = 0.0f;
                m_activeSmartCasts.push_back(asc);

                double cx = sc.centerX * m_androidWidth;
                double cy = sc.centerY * m_androidHeight;
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, asc.slot, cx, cy);
                anySmartCast = true;
            }
            handled = true;
        }
    }
    if (anySmartCast) {
        m_smartCasting = true;
        wlr_bridge_seat_touch_notify_frame(m_seat);
    }

    // 5. Tap (trigger all Tap buttons bound to this key simultaneously)
    if (m_scanCodeToSlots.find(scanCode) == m_scanCodeToSlots.end()) {
        std::vector<int32_t> slots;
        for (const auto &node : m_tapNodes) {
            if (node.scanCode == scanCode) {
                int32_t slot = allocateSlot();
                slots.push_back(slot);

                double tx = node.relX * m_androidWidth;
                double ty = node.relY * m_androidHeight;
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, slot, tx, ty);
            }
        }
        if (!slots.empty()) {
            wlr_bridge_seat_touch_notify_frame(m_seat);
            m_scanCodeToSlots[scanCode] = std::move(slots);
            handled = true;
        }
    } else {
        handled = true;
    }

    return handled;
}

bool InputMapper::handleScanCodeUp(uint32_t scanCode, struct wlr_surface *surface) {
    if (scanCode == 0) return false;
    m_heldScanCodes.erase(scanCode);

    bool handled = false;
    if (scanCode == m_dpad.scanUp || scanCode == m_dpad.scanDown ||
        scanCode == m_dpad.scanLeft || scanCode == m_dpad.scanRight) {
        updateDpad(surface);
        handled = true;
    }

    if (!m_seat) return handled;
    uint32_t now = getTimestampMs();

    // Free Look release
    if (m_freeLookActive && m_activeFreeLookIndex >= 0 &&
        m_freeLookNodes[m_activeFreeLookIndex].holdScanCode == scanCode) {
        m_freeLookActive = false;
        m_activeFreeLookIndex = -1;
        if (m_pendingReleaseFreeLookSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
            releaseSlot(m_pendingReleaseFreeLookSlot);
            m_pendingReleaseFreeLookSlot = -1;
        }
        if (m_activeFreeLookSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now + 1, m_activeFreeLookSlot);
            releaseSlot(m_activeFreeLookSlot);
            m_activeFreeLookSlot = -1;
        }
        wlr_bridge_seat_touch_notify_frame(m_seat);
        if (m_aimSuspendedByFreeLook && m_suspendedAimIndex >= 0) {
            int restoreIdx = m_suspendedAimIndex;
            m_aimSuspendedByFreeLook = false;
            m_suspendedAimIndex = -1;
            setAimLockActive(true, restoreIdx);
        }
        handled = true;
    }

    // Burst Tap release (release all Burst buttons bound to this key)
    auto burstIt = m_activeBursts.find(scanCode);
    if (burstIt != m_activeBursts.end()) {
        bool anyUp = false;
        for (const auto &b : burstIt->second) {
            if (b.isDown) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, b.slot);
                anyUp = true;
            }
            releaseSlot(b.slot);
        }
        if (anyUp) wlr_bridge_seat_touch_notify_frame(m_seat);
        m_activeBursts.erase(burstIt);
        handled = true;
    }

    // Smart Cast release (release all Smart Cast buttons bound to this key)
    bool anyScUp = false;
    for (auto it = m_activeSmartCasts.begin(); it != m_activeSmartCasts.end(); ) {
        if (it->cfg.scanCode == scanCode) {
            if (it->slot >= 0) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, it->slot);
                releaseSlot(it->slot);
                anyScUp = true;
            }
            it = m_activeSmartCasts.erase(it);
            handled = true;
        } else {
            ++it;
        }
    }
    if (anyScUp) {
        wlr_bridge_seat_touch_notify_frame(m_seat);
    }
    m_smartCasting = !m_activeSmartCasts.empty();

    // Tap release (release all Tap buttons bound to this key)
    auto it = m_scanCodeToSlots.find(scanCode);
    if (it != m_scanCodeToSlots.end()) {
        for (int32_t slot : it->second) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, slot);
            releaseSlot(slot);
        }
        wlr_bridge_seat_touch_notify_frame(m_seat);
        m_scanCodeToSlots.erase(it);
        handled = true;
    }

    return handled;
}

bool InputMapper::handleMappedMouseDown(int button, struct wlr_surface *surface) {
    if (button <= 1 || !m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    for (int i = 0; i < static_cast<int>(m_aimNodes.size()); ++i) {
        if (m_aimNodes[i].toggleMouseButton == button) {
            if (m_aimLockActive && m_activeAimIndex == i) setAimLockActive(false, -1);
            else setAimLockActive(true, i);
            return true;
        }
    }
    for (int i = 0; i < static_cast<int>(m_freeLookNodes.size()); ++i) {
        if (m_freeLookNodes[i].holdMouseButton == button) {
            if (!m_freeLookActive) {
                if (m_aimLockActive) {
                    m_aimSuspendedByFreeLook = true;
                    m_suspendedAimIndex = m_activeAimIndex;
                    setAimLockActive(false, -1);
                    m_aimSuspendedByFreeLook = true;
                }
                m_freeLookActive = true;
                m_activeFreeLookIndex = i;
                m_accumFreeLookX = 0.0f;
                m_accumFreeLookY = 0.0f;
            }
            return true;
        }
    }
    bool handled = false;
    uint32_t pseudoKey = 0x10000 + button;

    if (m_activeBursts.find(pseudoKey) == m_activeBursts.end()) {
        std::vector<ActiveBurst> bursts;
        for (const auto &bn : m_burstNodes) {
            if (bn.mouseButton == button) {
                ActiveBurst ab{bn, allocateSlot(), now, true};
                bursts.push_back(ab);
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, ab.slot, bn.relX * m_androidWidth, bn.relY * m_androidHeight);
            }
        }
        if (!bursts.empty()) {
            wlr_bridge_seat_touch_notify_frame(m_seat);
            m_activeBursts[pseudoKey] = std::move(bursts);
            handled = true;
        }
    } else {
        handled = true;
    }

    bool anySwipe = false;
    for (const auto &sw : m_swipeNodes) {
        if (sw.mouseButton == button) {
            ActiveSwipe as{sw, allocateSlot(), now};
            m_activeSwipes.push_back(as);
            wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, as.slot, sw.startX * m_androidWidth, sw.startY * m_androidHeight);
            anySwipe = true;
        }
    }
    if (anySwipe) {
        wlr_bridge_seat_touch_notify_frame(m_seat);
        handled = true;
    }

    bool anySc = false;
    for (const auto &sc : m_smartCastNodes) {
        if (sc.mouseButton == button) {
            bool alreadyActive = false;
            for (const auto &asc : m_activeSmartCasts) {
                if (asc.cfg.id == sc.id && asc.cfg.mouseButton == button) {
                    alreadyActive = true;
                    break;
                }
            }
            if (!alreadyActive) {
                ActiveSmartCast asc{sc, allocateSlot(), 0.0f, 0.0f};
                m_activeSmartCasts.push_back(asc);
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, asc.slot, sc.centerX * m_androidWidth, sc.centerY * m_androidHeight);
                anySc = true;
            }
            handled = true;
        }
    }
    if (anySc) {
        m_smartCasting = true;
        wlr_bridge_seat_touch_notify_frame(m_seat);
    }

    if (m_mouseBtnToSlots.find(button) == m_mouseBtnToSlots.end()) {
        std::vector<int32_t> slots;
        for (const auto &node : m_tapNodes) {
            if (node.mouseButton == button) {
                int32_t slot = allocateSlot();
                slots.push_back(slot);
                wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, slot, node.relX * m_androidWidth, node.relY * m_androidHeight);
            }
        }
        if (!slots.empty()) {
            wlr_bridge_seat_touch_notify_frame(m_seat);
            m_mouseBtnToSlots[button] = std::move(slots);
            handled = true;
        }
    } else {
        handled = true;
    }

    return handled;
}

bool InputMapper::handleMappedMouseUp(int button, struct wlr_surface *surface) {
    (void)surface;
    if (button <= 1 || !m_seat) return false;
    uint32_t now = getTimestampMs();
    bool handled = false;

    if (m_freeLookActive && m_activeFreeLookIndex >= 0 &&
        m_freeLookNodes[m_activeFreeLookIndex].holdMouseButton == button) {
        m_freeLookActive = false;
        m_activeFreeLookIndex = -1;
        if (m_pendingReleaseFreeLookSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_pendingReleaseFreeLookSlot);
            releaseSlot(m_pendingReleaseFreeLookSlot);
            m_pendingReleaseFreeLookSlot = -1;
        }
        if (m_activeFreeLookSlot >= 0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now + 1, m_activeFreeLookSlot);
            releaseSlot(m_activeFreeLookSlot);
            m_activeFreeLookSlot = -1;
        }
        wlr_bridge_seat_touch_notify_frame(m_seat);
        if (m_aimSuspendedByFreeLook && m_suspendedAimIndex >= 0) {
            int restoreIdx = m_suspendedAimIndex;
            m_aimSuspendedByFreeLook = false;
            m_suspendedAimIndex = -1;
            setAimLockActive(true, restoreIdx);
        }
        handled = true;
    }

    uint32_t pseudoKey = 0x10000 + button;
    auto burstIt = m_activeBursts.find(pseudoKey);
    if (burstIt != m_activeBursts.end()) {
        bool anyUp = false;
        for (const auto &b : burstIt->second) {
            if (b.isDown) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, b.slot);
                anyUp = true;
            }
            releaseSlot(b.slot);
        }
        if (anyUp) wlr_bridge_seat_touch_notify_frame(m_seat);
        m_activeBursts.erase(burstIt);
        handled = true;
    }

    bool anyScUp = false;
    for (auto it = m_activeSmartCasts.begin(); it != m_activeSmartCasts.end(); ) {
        if (it->cfg.mouseButton == button) {
            if (it->slot >= 0) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, it->slot);
                releaseSlot(it->slot);
                anyScUp = true;
            }
            it = m_activeSmartCasts.erase(it);
            handled = true;
        } else {
            ++it;
        }
    }
    if (anyScUp) wlr_bridge_seat_touch_notify_frame(m_seat);
    m_smartCasting = !m_activeSmartCasts.empty();

    auto it = m_mouseBtnToSlots.find(button);
    if (it != m_mouseBtnToSlots.end()) {
        for (int32_t slot : it->second) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, slot);
            releaseSlot(slot);
        }
        wlr_bridge_seat_touch_notify_frame(m_seat);
        m_mouseBtnToSlots.erase(it);
        handled = true;
    }

    return handled;
}

bool InputMapper::handleMouseDown(int button, int winX, int winY, struct wlr_surface *surface) {
    if (!m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    if (button > 1 && handleMappedMouseDown(button, surface)) {
        return true;
    }

    const auto *aim = activeAimNode();
    if (aim) {
        if (button == 1 && !m_firing) {
            m_firing = true;
            m_activeAimFireSlot = allocateSlot();
            double fx = aim->fireX * m_androidWidth;
            double fy = aim->fireY * m_androidHeight;
            wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, m_activeAimFireSlot, fx, fy);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            return true;
        }
        return false;
    }

    if (button == 1 && !m_isTouching) {
        m_isTouching = true;
        m_activeMouseSlot = allocateSlot();
        double ax, ay;
        windowToAndroid(winX, winY, ax, ay);
        wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, m_activeMouseSlot, ax, ay);
        wlr_bridge_seat_touch_notify_frame(m_seat);
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

    if (button > 1 && handleMappedMouseUp(button, surface)) {
        return true;
    }

    const auto *aim = activeAimNode();
    if (aim) {
        if (button == 1 && m_firing) {
            m_firing = false;
            if (m_activeAimFireSlot >= 0) {
                wlr_bridge_seat_touch_notify_up(m_seat, now, m_activeAimFireSlot);
                wlr_bridge_seat_touch_notify_frame(m_seat);
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
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_activeMouseSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeMouseSlot);
            m_activeMouseSlot = -1;
        }
        return true;
    }

    return false;
}

bool InputMapper::handleMouseMove(int winX, int winY, float deltaX, float deltaY, struct wlr_surface *surface) {
    if (!m_seat || !surface) return false;
    struct wlr_surface *rootSurf = wlr_bridge_surface_get_root_surface(surface);
    if (!rootSurf) rootSurf = surface;
    uint32_t now = getTimestampMs();

    // 1. Smart Cast (move all active Smart Cast nodes simultaneously)
    if (m_smartCasting && !m_activeSmartCasts.empty()) {
        for (auto &asc : m_activeSmartCasts) {
            if (asc.slot < 0) continue;
            double cx = asc.cfg.centerX * m_androidWidth;
            double cy = asc.cfg.centerY * m_androidHeight;

            float mx = 0.0f, my = 0.0f;
            if (m_aimLockActive && (deltaX != 0 || deltaY != 0)) {
                asc.accumX += deltaX * 1.5f;
                asc.accumY += deltaY * 1.5f;
                mx = asc.accumX;
                my = asc.accumY;
            } else {
                double ax, ay;
                windowToAndroid(winX, winY, ax, ay);
                mx = static_cast<float>(ax - cx);
                my = static_cast<float>(ay - cy);
            }

            float dist = std::sqrt(mx * mx + my * my);
            float maxR = asc.cfg.castRadius;
            if (dist > maxR && dist > 0.001f) {
                mx = (mx / dist) * maxR;
                my = (my / dist) * maxR;
                asc.accumX = mx;
                asc.accumY = my;
            }

            double tx = std::clamp(cx + mx, 2.0, static_cast<double>(m_androidWidth - 3));
            double ty = std::clamp(cy + my, 2.0, static_cast<double>(m_androidHeight - 3));
            wlr_bridge_seat_touch_notify_motion(m_seat, now, asc.slot, tx, ty);
        }
        wlr_bridge_seat_touch_notify_frame(m_seat);
        return true;
    }

    // 2. Infinite Unbounded Free Look (1:1 Linear, Zero Acceleration, Zero Drift, No Limits)
    if (m_freeLookActive && m_activeFreeLookIndex >= 0 && m_activeFreeLookIndex < static_cast<int>(m_freeLookNodes.size())) {
        const auto &fl = m_freeLookNodes[m_activeFreeLookIndex];
        float rawDx = deltaX;
        float rawDy = deltaY;
        if (rawDx == 0.0f && rawDy == 0.0f) return true;

        const float refScale = static_cast<float>(std::max(1, m_viewport.height));
        float pixelDx = (rawDx * fl.sensitivityX) * (static_cast<float>(m_androidHeight) / refScale);
        float pixelDy = (rawDy * fl.sensitivityY) * (static_cast<float>(m_androidHeight) / refScale);

        const double startX = std::clamp(static_cast<double>(fl.centerX * m_androidWidth), m_androidWidth * 0.05, m_androidWidth * 0.95);
        const double startY = std::clamp(static_cast<double>(fl.centerY * m_androidHeight), m_androidHeight * 0.05, m_androidHeight * 0.95);

        if (m_activeFreeLookSlot < 0) {
            m_activeFreeLookSlot = allocateSlot();
            m_accumFreeLookX = static_cast<float>(startX);
            m_accumFreeLookY = static_cast<float>(startY);
            wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, m_activeFreeLookSlot, startX, startY);
            wlr_bridge_seat_touch_notify_frame(m_seat);
        }

        m_accumFreeLookX += pixelDx;
        m_accumFreeLookY += pixelDy;
        wlr_bridge_seat_touch_notify_motion(m_seat, now, m_activeFreeLookSlot, m_accumFreeLookX, m_accumFreeLookY);
        wlr_bridge_seat_touch_notify_frame(m_seat);
        return true;
    }

    // 3. Pure 1:1 Linear Aim Lock (Zero Acceleration, Zero Drift, ±10,000px + 450ms Idle Reset)
    const auto *aim = activeAimNode();
    if (aim && !m_freeLookActive) {
        float rawDx = deltaX;
        float rawDy = deltaY;
        if (rawDx == 0.0f && rawDy == 0.0f) return true;

        m_lastAimMoveMs = now;

        const float refScale = static_cast<float>(std::max(1, m_viewport.height));
        float pixelDx = (rawDx * aim->sensitivityX) * (static_cast<float>(m_androidHeight) / refScale);
        float pixelDy = (rawDy * aim->sensitivityY) * (static_cast<float>(m_androidHeight) / refScale);

        const double startX = std::clamp(static_cast<double>(aim->centerX * m_androidWidth), m_androidWidth * 0.55, m_androidWidth * 0.85);
        const double startY = std::clamp(static_cast<double>(aim->centerY * m_androidHeight), m_androidHeight * 0.25, m_androidHeight * 0.75);

        if (m_activeAimCameraSlot < 0) {
            m_activeAimCameraSlot = allocateSlot();
            m_accumAimX = static_cast<float>(startX);
            m_accumAimY = static_cast<float>(startY);
            wlr_bridge_seat_touch_notify_down(m_seat, rootSurf, now, m_activeAimCameraSlot, startX, startY);
            wlr_bridge_seat_touch_notify_frame(m_seat);
        }

        m_accumAimX += pixelDx;
        m_accumAimY += pixelDy;

        if (std::abs(m_accumAimX - startX) >= 10000.0 || std::abs(m_accumAimY - startY) >= 10000.0) {
            wlr_bridge_seat_touch_notify_up(m_seat, now, m_activeAimCameraSlot);
            wlr_bridge_seat_touch_notify_frame(m_seat);
            releaseSlot(m_activeAimCameraSlot);
            m_activeAimCameraSlot = -1;
            m_accumAimX = 0.0f;
            m_accumAimY = 0.0f;
            return true;
        }

        wlr_bridge_seat_touch_notify_motion(m_seat, now, m_activeAimCameraSlot, m_accumAimX, m_accumAimY);
        wlr_bridge_seat_touch_notify_frame(m_seat);
        return true;
    }

    if (!m_isTouching || m_activeMouseSlot < 0) return false;

    double ax, ay;
    windowToAndroid(winX, winY, ax, ay);
    wlr_bridge_seat_touch_notify_motion(m_seat, now, m_activeMouseSlot, ax, ay);
    wlr_bridge_seat_touch_notify_frame(m_seat);
    return true;
}
