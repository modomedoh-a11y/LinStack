#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE 1
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <utility>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "wlr_c_bridge.h"
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

struct BurstTapConfig {
    std::string id;
    uint32_t scanCode = 0;
    float relX = 0.5f;
    float relY = 0.5f;
    int burstHz = 10;
};

struct FreeLookConfig {
    std::string id;
    uint32_t holdScanCode = 0;
    float centerX = 0.5f;
    float centerY = 0.5f;
    float sensitivityX = 1.2f;
    float sensitivityY = 1.2f;
    bool acceleration = false;
};

struct SwipeConfig {
    std::string id;
    uint32_t scanCode = 0;
    float startX = 0.5f;
    float startY = 0.7f;
    float endX = 0.5f;
    float endY = 0.3f;
    int durationMs = 150;
    std::vector<std::pair<float, float>> pathPoints;
};

struct SmartCastConfig {
    std::string id;
    uint32_t scanCode = 0;
    float centerX = 0.75f;
    float centerY = 0.75f;
    float castRadius = 160.0f;
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
    void sendSystemKey(uint32_t evdevKeyCode, struct wlr_surface *surface);
    void sendRecentsCombo(struct wlr_surface *surface);

    bool handleMouseDown(int button, int winX, int winY, struct wlr_surface *surface);
    bool handleMouseUp(int button, int winX, int winY, struct wlr_surface *surface);
    bool handleMouseMove(int winX, int winY, int deltaX, int deltaY, struct wlr_surface *surface);

    // Periodic tick for BurstTap and Swipe animations
    void tick(struct wlr_surface *surface);

    // Multi-Node Aim Lock & Free Look Management
    void setAimLockActive(bool active, int nodeIndex = 0);
    bool isAimLockActive() const { return m_aimLockActive; }
    int activeAimNodeIndex() const { return m_activeAimIndex; }
    const AimLockNodeConfig* activeAimNode() const;

    bool isFreeLookActive() const { return m_freeLookActive; }
    const FreeLookConfig* activeFreeLookNode() const;
    bool isSmartCasting() const { return m_smartCasting; }

    void setupDefaultProfile();

    void clearTapNodes() { m_tapNodes.clear(); }
    void addTapNode(const KeyNode &node) { m_tapNodes.push_back(node); }
    void setDpad(const DpadConfig &dpad) { m_dpad = dpad; }
    void clearAimNodes() { m_aimNodes.clear(); }
    void addAimNode(const AimLockNodeConfig &aim) { m_aimNodes.push_back(aim); }

    void clearExtraNodes() {
        m_burstNodes.clear();
        m_freeLookNodes.clear();
        m_swipeNodes.clear();
        m_smartCastNodes.clear();
    }
    void addBurstNode(const BurstTapConfig &b) { m_burstNodes.push_back(b); }
    void addFreeLookNode(const FreeLookConfig &fl) { m_freeLookNodes.push_back(fl); }
    void addSwipeNode(const SwipeConfig &sw) { m_swipeNodes.push_back(sw); }
    void addSmartCastNode(const SmartCastConfig &sc) { m_smartCastNodes.push_back(sc); }

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
    std::vector<BurstTapConfig> m_burstNodes;
    std::vector<FreeLookConfig> m_freeLookNodes;
    std::vector<SwipeConfig> m_swipeNodes;
    std::vector<SmartCastConfig> m_smartCastNodes;

    std::set<uint32_t> m_heldScanCodes;
    std::set<int32_t> m_activeSlots;
    std::unordered_map<uint32_t, int32_t> m_scanCodeToSlot;

    bool m_dpadActive = false;
    int32_t m_activeDpadSlot = -1;

    bool m_aimLockActive = false;
    int m_activeAimIndex = -1;
    int32_t m_activeAimCameraSlot = -1;
    int32_t m_pendingReleaseCameraSlot = -1;
    int32_t m_activeAimFireSlot = -1;
    bool m_firing = false;

    bool m_isTouching = false;
    int32_t m_activeMouseSlot = -1;

    float m_accumAimX = 0.0f;
    float m_accumAimY = 0.0f;

    // Burst Tap state
    struct ActiveBurst {
        BurstTapConfig cfg;
        int32_t slot = -1;
        uint32_t lastClickMs = 0;
        bool isDown = false;
    };
    std::unordered_map<uint32_t, ActiveBurst> m_activeBursts;

    // Free Look state
    bool m_freeLookActive = false;
    int m_activeFreeLookIndex = -1;
    int32_t m_activeFreeLookSlot = -1;
    int32_t m_pendingReleaseFreeLookSlot = -1;
    float m_accumFreeLookX = 0.0f;
    float m_accumFreeLookY = 0.0f;

    // Swipe state
    struct ActiveSwipe {
        SwipeConfig cfg;
        int32_t slot = -1;
        uint32_t startMs = 0;
    };
    std::vector<ActiveSwipe> m_activeSwipes;

    // Smart Cast state
    bool m_smartCasting = false;
    SmartCastConfig m_activeSmartCast;
    int32_t m_activeSmartCastSlot = -1;
    float m_smartCastAccumX = 0.0f;
    float m_smartCastAccumY = 0.0f;
};

#endif // INPUT_MAPPER_H
