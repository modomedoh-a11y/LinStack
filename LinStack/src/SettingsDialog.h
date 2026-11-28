#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QSize>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    int     targetFps()          const;
    QString resolution()         const;
    QSize   baseResolutionSize() const; // Parsed QSize from the resolution combo
    int     dpi()                const;
    QString deviceProfile()      const;
    bool    isAdbBridgeEnabled() const;
    bool    isVsyncEnabled()     const;
    bool    isSharedStorageEnabled() const;
    QString sharedStoragePath()  const;
    bool    isSwiftShaderEnabled() const;
    bool    confirmExitEnabled() const;

signals:
    void settingsApplied();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();

    // 9 Exact Sidebar Sections
    QWidget *createPerformanceTab();
    QWidget *createDisplayTab();
    QWidget *createGraphicsAudioTab();
    QWidget *createDevicesTab();
    QWidget *createPreferencesTab();
    QWidget *createPhoneProfileTab();
    QWidget *createShortcutsTab();
    QWidget *createSharedStorageTab();
    QWidget *createAdvancedTab();

    // Navigation Widgets
    QListWidget    *m_sidebar = nullptr;
    QStackedWidget *m_stack   = nullptr;

    // 1. Performance
    QComboBox *m_comboPerfMode = nullptr;
    QSlider   *m_sliderFps     = nullptr;
    QLabel    *m_lblFpsValue   = nullptr;
    QCheckBox *m_checkVsync     = nullptr;

    // 2. Display
    QRadioButton *m_radioLandscape  = nullptr;
    QRadioButton *m_radioPortrait   = nullptr;
    QComboBox    *m_comboResolution = nullptr;
    QComboBox    *m_comboDpi        = nullptr;

    // 3. Graphics & Audio
    QComboBox *m_comboRenderer     = nullptr;
    QComboBox *m_comboAstc         = nullptr;
    QComboBox *m_comboAudioBackend = nullptr;

    // 4. Devices
    QComboBox *m_comboPassthrough  = nullptr;
    QCheckBox *m_checkGamepadAuto  = nullptr;
    QComboBox *m_comboMicDevice    = nullptr;
    QComboBox *m_comboCameraDevice = nullptr;

    // 5. Preferences
    QCheckBox *m_checkConfirmExit      = nullptr;
    QCheckBox *m_checkShowToasts       = nullptr;
    QCheckBox *m_checkRememberGeometry = nullptr;
    QCheckBox *m_checkAutostartSession = nullptr;
    QCheckBox *m_checkPauseMinimized   = nullptr;

    // 6. Phone Profile
    QComboBox *m_comboPhoneProfile = nullptr;

    // 7. Shortcuts
    QTableWidget *m_tableShortcuts = nullptr;

    // 8. Shared Storage
    QCheckBox *m_checkSharedEnabled = nullptr;
    QLineEdit *m_editSharedPath     = nullptr;

    // 9. Advanced
    QCheckBox   *m_checkAdbBridge    = nullptr;
    QCheckBox   *m_checkSwiftShader  = nullptr;
    QCheckBox   *m_checkDirectDmaBuf = nullptr;
    QPushButton *m_btnOpenLogs       = nullptr;
    QPushButton *m_btnOpenLogDir     = nullptr;
};

#endif // SETTINGSDIALOG_H
