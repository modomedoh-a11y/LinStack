#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
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

signals:
    void settingsApplied();
    void memoryTrimRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    QWidget *createPerformanceTab();
    QWidget *createDisplayTab();
    QWidget *createGraphicsTab();
    QWidget *createPhoneProfileTab();
    QWidget *createShortcutsTab();

    QTabWidget *m_tabs = nullptr;

    // Performance (Updated: CPU/Memory allocation removed)
    QComboBox *m_comboPerfMode = nullptr;
    QSlider *m_sliderFps = nullptr;
    QLabel *m_lblFpsValue = nullptr;
    QCheckBox *m_checkVsync = nullptr;
    QPushButton *m_btnTrimMemory = nullptr;

    // Display (960x540 to 3840x2160)
    QRadioButton *m_radioLandscape = nullptr;
    QRadioButton *m_radioPortrait = nullptr;
    QComboBox *m_comboResolution = nullptr;
    QComboBox *m_comboDpi = nullptr;

    // Graphics
    QComboBox *m_comboRenderer = nullptr;
    QComboBox *m_comboAstc = nullptr;
    QComboBox *m_comboAudioBackend = nullptr;

    // Phone Profile
    QComboBox *m_comboPhoneProfile = nullptr;
    QCheckBox *m_checkAdbBridge = nullptr;

    // Shortcuts (Hotkey Manager)
    QTableWidget *m_tableShortcuts = nullptr;
};

#endif // SETTINGSDIALOG_H
