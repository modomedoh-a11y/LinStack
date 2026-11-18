#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE 1
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "wlroots_compat.h"
#include "WaylandSurfaceRenderer.h"

enum class WlrKeyNodeType {
    Tap,
    Dpad,
    AimLock
};

struct KeyNode {
    std::string id;
    WlrKeyNodeType type = WlrKeyNodeType::Tap;
    uint32_t scanCode = 0; // Hardware scancode (layout-independent)
    float relX = 0.5f;
    float relY = 0.5f;
    int32_t touchSlot = 3;
};

struct DpadConfig {
    float centerX = 0.18f;
    float centerY = 0.70f;
    float radius = 0.08f;
    uint32_t scanUp = 25;    // W (evdev 17 + 8)
    uint32_t scanLeft = 38;  // A (evdev 30 + 8)
    uint32_t scanDown = 39;  // S (evdev 31 + 8)
    uint32_t scanRight = 40; // D (evdev 32 + 8)
    int32_t touchSlot = 0;
};

struct AimLockNodeConfig {
    std::string id;
    uint32_t toggleScanCode = 67; // F1 (evdev 59 + 8)
    float centerX = 0.50f;
    float centerY = 0.50f;
    float fireX = 0.82f;
    float fireY = 0.64f;
    float sensitivityX = 1.2f;
    float sensitivityY = 1.2f;
    bool acceleration = false;
    float maxSweepRadius = 0.18f;
    int32_t cameraTouchSlot = 1;
    int32_t fireTouchSlot = 2;
};

class InputMapper {
public:
    explicit InputMapper(struct wlr_seat *seat = nullptr,
                         struct wlr_relative_pointer_manager_v1 *relPointerMgr = nullptr);
    ~InputMapper();

    void setSeat(struct wlr_seat *seat) { m_seat = seat; }
    void setRelativePointerManager(struct wlr_relative_pointer_manager_v1 *mgr) { m_relPointerMgr = mgr; }

    void setGeometry(const ViewportRect &viewport, int androidWidth, int androidHeight);
    void windowToAndroid(int winX, int winY, double &outAndroidX, double &outAndroidY) const;
    void windowToNormalized(int winX, int winY, float &outNormX, float &outNormY) const;

    // Hardware Scancode Event Handlers (works across all keyboard languages)
    bool handleScanCodeDown(uint32_t scanCode, struct wlr_surface *surface);
    bool handleScanCodeUp(uint32_t scanCode, struct wlr_surface *surface);

    bool handleMouseDown(int button, int winX, int winY, struct wlr_surface *surface);
    bool handleMouseUp(int button, int winX, int winY, struct wlr_surface *surface);
    bool handleMouseMove(int winX, int winY, int deltaX, int deltaY, struct wlr_surface *surface);

    // Multi-Node Aim Lock Management
    void setAimLockActive(bool active, int nodeIndex = 0);
    bool isAimLockActive() const { return m_aimLockActive; }
    int activeAimNodeIndex() const { return m_activeAimIndex; }
    const AimLockNodeConfig* activeAimNode() const;

    void setupDefaultProfile();

    void clearTapNodes() { m_tapNodes.clear(); }
    void addTapNode(const KeyNode &node) { m_tapNodes.push_back(node); }
    void setDpad(const DpadConfig &dpad) { m_dpad = dpad; }
    void clearAimNodes() { m_aimNodes.clear(); }
    void addAimNode(const AimLockNodeConfig &aim) { m_aimNodes.push_back(aim); }

    const std::vector<KeyNode>& getTapNodes() const { return m_tapNodes; }
    const DpadConfig& getDpad() const { return m_dpad; }
    const std::vector<AimLockNodeConfig>& getAimNodes() const { return m_aimNodes; }

private:
    uint32_t getTimestampMs() const;
    void updateDpad(struct wlr_surface *surface);
    int32_t allocateSlot();
    void releaseSlot(int32_t slot);

    struct wlr_seat *m_seat = nullptr;
    struct wlr_relative_pointer_manager_v1 *m_relPointerMgr = nullptr;

    ViewportRect m_viewport;
    int m_androidWidth = 1920;
    int m_androidHeight = 1080;

    std::vector<KeyNode> m_tapNodes;
    DpadConfig m_dpad;
    std::vector<AimLockNodeConfig> m_aimNodes;

    std::set<uint32_t> m_heldScanCodes;
    std::set<int32_t> m_activeSlots;
    std::unordered_map<uint32_t, int32_t> m_scanCodeToSlot;

    bool m_dpadActive = false;
    int32_t m_activeDpadSlot = -1;

    bool m_aimLockActive = false;
    int m_activeAimIndex = -1;
    int32_t m_activeAimCameraSlot = -1;
    int32_t m_activeAimFireSlot = -1;
    bool m_firing = false;

    bool m_isTouching = false;
    int32_t m_activeMouseSlot = -1;

    float m_accumAimX = 0.0f;
    float m_accumAimY = 0.0f;
};

#endif // INPUT_MAPPER_H
