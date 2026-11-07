#ifndef SELFHEALINGMANAGER_H
#define SELFHEALINGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QPoint>
#include <QMouseEvent>
#include <functional>

#include "DiagnosticsEngine.h"

class RecoveryDialog;

// Type aliases for architecture consistency
using DiagnosticsDialog = RecoveryDialog;
using SelfHealingDialog = RecoveryDialog;

/**
 * @brief Coordinates proactive health diagnostics, fault classification, automated
 * remediation (user-space & elevated pkexec), and guided non-blocking UI recovery.
 */
class SelfHealingManager : public QObject
{
    Q_OBJECT

public:
    explicit SelfHealingManager(QObject *parent = nullptr);
    ~SelfHealingManager() override;

    // Direct access to the internal diagnostic engine
    DiagnosticsEngine *engine() { return &m_engine; }

    // Proactive startup & runtime pipeline
    void startDiagnosticsAsync(bool autoHeal = true, QWidget *parentUi = nullptr);

    // Automated fault resolution
    bool executeProgrammaticFixes(const HealthReport &report);
    bool executeElevatedFixes(const HealthReport &report);

    // Shows the non-blocking cyber-themed Recovery Dialog
    void showRecoveryDialog(const HealthReport &report, QWidget *parent = nullptr);

    // Backward-compatible synchronous methods
    bool checkHealth();
    bool ensureSessionRunning();
    bool promptRepair();
    bool executeRepair();

signals:
    void healingStarted(const QString &actionDescription);
    void healingProgress(int percent, const QString &statusText);
    void healingFinished(bool success, const QString &summary);

    void diagnosticsProgress(int percent, const QString &checkName);
    void diagnosticsFinished(const HealthReport &report);

    void repairStarted();
    void repairFinished(bool success);
    void logMessage(const QString &msg);

private slots:
    void handleDiagnosticsFinished(const HealthReport &report);

private:
    bool isBinderAccessible();
    bool isSessionActive();

    DiagnosticsEngine m_engine;
    RecoveryDialog *m_recoveryDialog = nullptr;
    bool m_autoHeal = true;
    QWidget *m_parentUi = nullptr;
};

/**
 * @brief Non-blocking, Cyber-Emerald themed recovery dialog presenting fault codes,
 * log excerpts, and one-click auto-repair actions.
 */
class RecoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecoveryDialog(QWidget *parent = nullptr);
    void setReport(const HealthReport &report);
    void setStatus(const QString &status, bool inProgress);

signals:
    void autoHealRequested();
    void recheckRequested();
    void ignoreAndContinue();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();

    QWidget *m_mainContainer = nullptr;
    QWidget *m_titleBarWidget = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_errorCodeBadge = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    QTextEdit *m_logConsole = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QPushButton *m_minBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_btnAutoHeal = nullptr;
    QPushButton *m_btnRecheck = nullptr;
    QPushButton *m_btnContinue = nullptr;
    QPushButton *m_btnExit = nullptr;

    HealthReport m_report;

    bool m_isDragging = false;
    QPoint m_dragStartPos;
};

#endif // SELFHEALINGMANAGER_H
