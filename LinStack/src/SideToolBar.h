#ifndef SIDETOOLBAR_H
#define SIDETOOLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include "Theme.h"

class SideToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit SideToolBar(QWidget *parent = nullptr);

    bool isEcoModeActive() const { return m_ecoModeActive; }
    bool isKeymapperActive() const { return m_keymapperActive; }

    void setEcoModeActive(bool active);
    void setKeymapperActive(bool active);

signals:
    // 15 Action buttons
    void fullscreenClicked();
    void volumeUpClicked();
    void volumeDownClicked();
    void keymapperToggled(bool active);
    void macroClicked();
    void rotateClicked();
    void ecoModeToggled(bool active);
    void installApkClicked();
    void screenshotClicked();
    void fileManagerClicked();
    void ramTrimClicked();
    void multiInstanceClicked();
    void settingsClicked();

    // Android Navigation
    void androidBackClicked();
    void androidHomeClicked();
    void androidRecentsClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    QPushButton *createToolButton(const QString &tooltip, Theme::IconType iconType, const char *slot = nullptr);

    // 15 Buttons
    QPushButton *m_btnFullscreen = nullptr;
    QPushButton *m_btnVolumeUp = nullptr;
    QPushButton *m_btnVolumeDown = nullptr;
    QPushButton *m_btnKeymapper = nullptr;
    QPushButton *m_btnMacro = nullptr;
    QPushButton *m_btnRotate = nullptr;
    QPushButton *m_btnEcoMode = nullptr;
    QPushButton *m_btnInstallApk = nullptr;
    QPushButton *m_btnScreenshot = nullptr;
    QPushButton *m_btnFileManager = nullptr;
    QPushButton *m_btnRamTrim = nullptr;
    QPushButton *m_btnMultiInstance = nullptr;
    QPushButton *m_btnSettings = nullptr;

    QPushButton *m_btnBack = nullptr;
    QPushButton *m_btnHome = nullptr;
    QPushButton *m_btnRecents = nullptr;

    bool m_ecoModeActive = false;
    bool m_keymapperActive = false;
};

#endif // SIDETOOLBAR_H
