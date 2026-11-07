#include "UInputTouchBridge.h"

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QDebug>

UInputTouchBridge::UInputTouchBridge(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < MAX_SLOTS; ++i) {
        m_slots[i] = SlotState{};
    }
}

UInputTouchBridge::~UInputTouchBridge()
{
    closeDevice();
}

void UInputTouchBridge::closeDevice()
{
    if (m_uinputDev) {
        libevdev_uinput_destroy(m_uinputDev);
        m_uinputDev = nullptr;
    }
    if (m_dev) {
        libevdev_free(m_dev);
        m_dev = nullptr;
    }
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    m_isReady = false;
}

bool UInputTouchBridge::initialize(int screenWidth, int screenHeight)
{
    closeDevice();

    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Check if /dev/uinput can be opened
    m_fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        int err = errno;
        m_mockMode = true;
        m_lastError = QString("Cannot open /dev/uinput (%1): %2. Running in dry-run simulation mode.\n"
                              "To grant permissions, execute:\n"
                              "sudo usermod -aG input $USER && sudo chmod 666 /dev/uinput")
                          .arg(err)
                          .arg(strerror(err));
        emit errorOccurred(m_lastError);
        return false;
    }

    m_dev = libevdev_new();
    if (!m_dev) {
        m_lastError = "Failed to allocate libevdev structure.";
        emit errorOccurred(m_lastError);
        closeDevice();
        m_mockMode = true;
        return false;
    }

    libevdev_set_name(m_dev, "LinStack Virtual Multi-Touch (Linstack)");
    libevdev_set_id_bustype(m_dev, BUS_VIRTUAL);
    libevdev_set_id_vendor(m_dev, 0x18D1); // Google
    libevdev_set_id_product(m_dev, 0x0001);
    libevdev_set_id_version(m_dev, 1);

    // Synchronizations & Touch keys
    libevdev_enable_event_type(m_dev, EV_SYN);
    libevdev_enable_event_type(m_dev, EV_KEY);
    libevdev_enable_event_code(m_dev, EV_KEY, BTN_TOUCH, nullptr);
    libevdev_enable_event_code(m_dev, EV_KEY, BTN_TOOL_FINGER, nullptr);

    // Absolute axis configuration for multi-touch
    libevdev_enable_event_type(m_dev, EV_ABS);

    struct input_absinfo abs_x = { 0, 0, m_screenWidth, 0, 0, 0 };
    struct input_absinfo abs_y = { 0, 0, m_screenHeight, 0, 0, 0 };
    struct input_absinfo abs_slot = { 0, 0, MAX_SLOTS - 1, 0, 0, 0 };
    struct input_absinfo abs_tracking = { 0, 0, 65535, 0, 0, 0 };
    struct input_absinfo abs_pressure = { 0, 0, 255, 0, 0, 0 };

    libevdev_enable_event_code(m_dev, EV_ABS, ABS_X, &abs_x);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_Y, &abs_y);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_MT_SLOT, &abs_slot);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_MT_TRACKING_ID, &abs_tracking);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_MT_POSITION_X, &abs_x);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_MT_POSITION_Y, &abs_y);
    libevdev_enable_event_code(m_dev, EV_ABS, ABS_MT_PRESSURE, &abs_pressure);

    int rc = libevdev_uinput_create_from_device(m_dev, m_fd, &m_uinputDev);
    if (rc < 0) {
        m_mockMode = true;
        m_lastError = QString("Failed to create uinput device via libevdev (code %1): %2")
                          .arg(rc).arg(strerror(-rc));
        emit errorOccurred(m_lastError);
        closeDevice();
        return false;
    }

    const char *devNode = libevdev_uinput_get_devnode(m_uinputDev);
    m_devicePath = devNode ? QString::fromUtf8(devNode) : "/dev/uinput";
    m_isReady = true;
    m_mockMode = false;

    emit deviceCreated(m_devicePath);
    return true;
}

void UInputTouchBridge::emitEvent(unsigned int type, unsigned int code, int value)
{
    if (m_uinputDev && m_isReady) {
        libevdev_uinput_write_event(m_uinputDev, type, code, value);
    }
}

void UInputTouchBridge::touchDown(int slot, int x, int y)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;

    m_slots[slot].active = true;
    m_slots[slot].x = x;
    m_slots[slot].y = y;
    m_slots[slot].trackingId = m_nextTrackingId++;
    if (m_nextTrackingId > 65530) m_nextTrackingId = 1;

    emitEvent(EV_ABS, ABS_MT_SLOT, slot);
    emitEvent(EV_ABS, ABS_MT_TRACKING_ID, m_slots[slot].trackingId);
    emitEvent(EV_ABS, ABS_MT_POSITION_X, x);
    emitEvent(EV_ABS, ABS_MT_POSITION_Y, y);
    emitEvent(EV_ABS, ABS_MT_PRESSURE, 180);
    emitEvent(EV_KEY, BTN_TOUCH, 1);
    emitEvent(EV_KEY, BTN_TOOL_FINGER, 1);
    emitEvent(EV_ABS, ABS_X, x);
    emitEvent(EV_ABS, ABS_Y, y);
    sync();

    emit touchEventInjected(slot, x, y, true);
}

void UInputTouchBridge::touchMove(int slot, int x, int y)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;
    if (!m_slots[slot].active) {
        touchDown(slot, x, y);
        return;
    }

    m_slots[slot].x = x;
    m_slots[slot].y = y;

    emitEvent(EV_ABS, ABS_MT_SLOT, slot);
    emitEvent(EV_ABS, ABS_MT_POSITION_X, x);
    emitEvent(EV_ABS, ABS_MT_POSITION_Y, y);
    emitEvent(EV_ABS, ABS_X, x);
    emitEvent(EV_ABS, ABS_Y, y);
    sync();

    emit touchEventInjected(slot, x, y, true);
}

void UInputTouchBridge::touchUp(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;
    if (!m_slots[slot].active) return;

    m_slots[slot].active = false;

    emitEvent(EV_ABS, ABS_MT_SLOT, slot);
    emitEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);

    // Check if any other slot remains active
    bool anyActive = false;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (m_slots[i].active) {
            anyActive = true;
            break;
        }
    }

    if (!anyActive) {
        emitEvent(EV_KEY, BTN_TOUCH, 0);
        emitEvent(EV_KEY, BTN_TOOL_FINGER, 0);
    }

    sync();
    emit touchEventInjected(slot, m_slots[slot].x, m_slots[slot].y, false);
}

void UInputTouchBridge::sync()
{
    emitEvent(EV_SYN, SYN_REPORT, 0);
}
