#ifndef UINPUTTOUCHBRIDGE_H
#define UINPUTTOUCHBRIDGE_H

#include <QString>
#include <QObject>

struct libevdev;
struct libevdev_uinput;

/**
 * @brief High-performance, low-latency multi-touch injection bridge using Linux uinput and libevdev.
 * Creates a virtual multi-touch device exposed to Linstack container (/dev/input/event*).
 */
class UInputTouchBridge : public QObject
{
    Q_OBJECT

public:
    explicit UInputTouchBridge(QObject *parent = nullptr);
    ~UInputTouchBridge() override;

    bool initialize(int screenWidth = 1920, int screenHeight = 1080);
    void closeDevice();

    bool isReady() const { return m_isReady; }
    bool isMockMode() const { return m_mockMode; }
    QString devicePath() const { return m_devicePath; }
    QString lastError() const { return m_lastError; }

    // Multi-touch slot injection (slots 0 to 9)
    void touchDown(int slot, int x, int y);
    void touchMove(int slot, int x, int y);
    void touchUp(int slot);
    void sync();

signals:
    void deviceCreated(const QString &devicePath);
    void errorOccurred(const QString &error);
    void touchEventInjected(int slot, int x, int y, bool isDown);

private:
    void emitEvent(unsigned int type, unsigned int code, int value);

    struct libevdev *m_dev = nullptr;
    struct libevdev_uinput *m_uinputDev = nullptr;
    int m_fd = -1;

    int m_screenWidth = 1920;
    int m_screenHeight = 1080;
    static constexpr int MAX_SLOTS = 10;

    struct SlotState {
        bool active = false;
        int x = 0;
        int y = 0;
        int trackingId = -1;
    };
    SlotState m_slots[MAX_SLOTS];
    int m_nextTrackingId = 1;

    bool m_isReady = false;
    bool m_mockMode = false;
    QString m_devicePath;
    QString m_lastError;
};

#endif // UINPUTTOUCHBRIDGE_H
