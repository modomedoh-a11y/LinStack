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

void InputMapper::setAimLockActive(bool active, int nodeIndex) {
    if (active && (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_aimNodes.size()))) {
        return;
    }

    if (m_aimLockActive == active && (!active || m_activeAimIndex == nodeIndex)) {
        return;
    }

    uint32_t now = getTimestampMs();

    if (m_aimLockActive && m_seat) {
        if (m_activeAimCameraSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_activeAimCameraSlot);
            releaseSlot(m_activeAimCameraSlot);
            m_activeAimCameraSlot = -1;
        }
        if (m_firing && m_activeAimFireSlot >= 0) {
            wlr_seat_touch_notify_up(m_seat, now, m_activeAimFireSlot);
            releaseSlot(m_activeAimFireSlot);
            m_activeAimFireSlot = -1;
            m_firing = false;
        }
        wlr_seat_touch_notify_frame(m_seat);
    }

    m_aimLockActive = active;
    m_activeAimIndex = active ? nodeIndex : -1;
    m_accumAimX = 0.0f;
    m_accumAimY = 0.0f;
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

    if (scanCode == 9 && m_aimLockActive) {
        setAimLockActive(false, -1);
        return true;
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

    const auto *aim = activeAimNode();
    if (aim) {
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

        if (m_activeAimCameraSlot < 0) {
            m_activeAimCameraSlot = allocateSlot();
            double cx = aim->centerX * m_androidWidth;
            double cy = aim->centerY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now, m_activeAimCameraSlot, cx, cy);
            wlr_seat_touch_notify_frame(m_seat);
        }

        m_accumAimX += normDx;
        m_accumAimY += normDy;

        float distSq = m_accumAimX * m_accumAimX + m_accumAimY * m_accumAimY;
        float maxRadiusSq = aim->maxSweepRadius * aim->maxSweepRadius;

        if (distSq >= maxRadiusSq) {
            wlr_seat_touch_notify_up(m_seat, now, m_activeAimCameraSlot);
            wlr_seat_touch_notify_frame(m_seat);

            m_accumAimX = 0.0f;
            m_accumAimY = 0.0f;

            double cx = aim->centerX * m_androidWidth;
            double cy = aim->centerY * m_androidHeight;
            wlr_seat_touch_notify_down(m_seat, rootSurf, now + 1, m_activeAimCameraSlot, cx, cy);
            wlr_seat_touch_notify_frame(m_seat);
        } else {
            double targetX = std::clamp(static_cast<double>((aim->centerX + m_accumAimX) * m_androidWidth), 0.0, static_cast<double>(m_androidWidth - 1));
            double targetY = std::clamp(static_cast<double>((aim->centerY + m_accumAimY) * m_androidHeight), 0.0, static_cast<double>(m_androidHeight - 1));
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
