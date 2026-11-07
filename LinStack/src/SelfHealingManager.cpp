#include "SelfHealingManager.h"
#include "Logger.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QElapsedTimer>
#include <QApplication>
#include <QScrollBar>

// ─────────────────────────────────────────────────────────────────────────────
// SelfHealingManager Implementation
// ─────────────────────────────────────────────────────────────────────────────

SelfHealingManager::SelfHealingManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_engine, &DiagnosticsEngine::progressUpdated, this, &SelfHealingManager::diagnosticsProgress);
    connect(&m_engine, &DiagnosticsEngine::diagnosticsFinished, this, &SelfHealingManager::handleDiagnosticsFinished);
}

SelfHealingManager::~SelfHealingManager() = default;

void SelfHealingManager::startDiagnosticsAsync(bool autoHeal, QWidget *parentUi)
{
    m_autoHeal = autoHeal;
    m_parentUi = parentUi;
    LOG_INFO("SelfHealing", "Initiating asynchronous diagnostic pipeline...");
    m_engine.runDiagnosticsAsync();
}

void SelfHealingManager::handleDiagnosticsFinished(const HealthReport &report)
{
    emit diagnosticsFinished(report);

    if (report.isHealthy) {
        LOG_INFO("SelfHealing", "Diagnostics verified all systems healthy. Ensuring session running...");
        ensureSessionRunning();
        if (m_recoveryDialog && m_recoveryDialog->isVisible()) {
            m_recoveryDialog->setStatus("All subsystems healthy! Ready.", false);
            QThread::msleep(800);
            m_recoveryDialog->accept();
        }
        return;
    }

    LOG_WARN("SelfHealing", QString("Diagnostics reported issues: %1").arg(report.summary));

    // Step 1: Attempt non-elevated programmatic fixes first
    bool progFixed = executeProgrammaticFixes(report);

    // Step 2: If there are elevated issues (like missing kernel binders), prompt or show recovery dialog
    bool hasElevatedIssues = false;
    for (const auto &c : report.failedChecks()) {
        if (c.requiresElevation) {
            hasElevatedIssues = true;
            break;
        }
    }

    if (hasElevatedIssues || (!progFixed && report.hasCriticalFailures)) {
        LOG_FAULT("SelfHealing", "FAULT_REQUIRES_ATTENTION",
                  "Unresolved critical faults remain. Presenting recovery options.",
                  "Launch elevated auto-heal or check logs.");
        showRecoveryDialog(report, m_parentUi);
    } else {
        // Only warnings remain or user-space fixes were applied
        ensureSessionRunning();
    }
}

bool SelfHealingManager::executeProgrammaticFixes(const HealthReport &report)
{
    bool anyAttempted = false;
    bool allSuccess = true;

    for (const auto &c : report.checks) {
        if (c.status == CheckStatus::Passed || c.requiresElevation) {
            continue;
        }

        anyAttempted = true;
        emit healingStarted(QString("Resolving: %1").arg(c.name));

        if (c.id == "ADB_SOCKET") {
            LOG_INFO("SelfHealing", "Auto-remediating ADB socket: starting adb server and connecting to 127.0.0.1:5555");
            QProcess p;
            p.start("adb", QStringList() << "start-server");
            p.waitForFinished(3000);

            QProcess pConnect;
            pConnect.start("adb", QStringList() << "connect" << "127.0.0.1:5555");
            pConnect.waitForFinished(3000);
            Logger::instance().logSubprocess("adb connect 127.0.0.1:5555", pConnect.exitCode(),
                                             pConnect.readAllStandardOutput(), pConnect.readAllStandardError());
        } else if (c.id == "ROOTFS_OVERLAY") {
            LOG_INFO("SelfHealing", "Ensuring user storage directory exists with correct permissions...");
            QString dir = QDir::homePath() + "/.local/share/LinStack";
            QDir().mkpath(dir);
        }
    }

    if (anyAttempted) {
        emit healingFinished(allSuccess, "Programmatic fixes applied.");
    }
    return allSuccess;
}

bool SelfHealingManager::executeElevatedFixes(const HealthReport &report)
{
    emit repairStarted();
    emit healingStarted("Executing elevated kernel & service repair via pkexec...");

    QStringList commands;

    // Check if binder modprobe is needed
    bool needBinder = false;
    for (const auto &c : report.checks) {
        if (c.id == "KERNEL_BINDER" && c.status != CheckStatus::Passed) {
            needBinder = true;
            break;
        }
    }

    if (needBinder || !isBinderAccessible()) {
        commands << "modprobe binder_linux devices=binder,hwbinder,vndbinder";
        commands << "mkdir -p /dev/binderfs";
        commands << "mount -t binder binder /dev/binderfs 2>/dev/null || true";
        commands << "chmod 0666 /dev/binder /dev/binderfs/binder /dev/vndbinder /dev/hwbinder 2>/dev/null || true";
    }

    commands << "systemctl restart linstack-container.service 2>/dev/null || systemctl restart linstack-container.service 2>/dev/null || true";

    QString bashCmd = commands.join(" && ");
    LOG_INFO("SelfHealing", QString("Spawning pkexec command: %1").arg(bashCmd));

    QProcess proc;
    proc.start("pkexec", QStringList() << "bash" << "-c" << bashCmd);

    if (!proc.waitForFinished(20000)) {
        LOG_CRITICAL("SelfHealing", "pkexec repair timed out after 20 seconds.");
        emit repairFinished(false);
        emit healingFinished(false, "Elevated repair timed out.");
        return false;
    }

    int exitCode = proc.exitCode();
    QString stdOut = proc.readAllStandardOutput();
    QString stdErr = proc.readAllStandardError();
    Logger::instance().logSubprocess(QString("pkexec bash -c '%1'").arg(bashCmd), exitCode, stdOut, stdErr);

    bool success = (exitCode == 0);
    emit repairFinished(success);
    emit healingFinished(success, success ? "Elevated repair completed successfully." : "pkexec repair failed.");

    if (success) {
        ensureSessionRunning();
    }
    return success;
}

void SelfHealingManager::showRecoveryDialog(const HealthReport &report, QWidget *parent)
{
    if (!m_recoveryDialog) {
        m_recoveryDialog = new RecoveryDialog(parent);

        connect(m_recoveryDialog, &RecoveryDialog::autoHealRequested, this, [this]() {
            m_recoveryDialog->setStatus("Running automated elevated healing...", true);
            QThread::msleep(200);

            // Execute elevated repair in background or process
            bool ok = executeElevatedFixes(m_engine.latestReport());
            if (ok) {
                m_recoveryDialog->setStatus("Repair finished! Re-running diagnostics...", true);
                m_engine.runDiagnosticsAsync();
            } else {
                m_recoveryDialog->setStatus("Repair encountered an error. Check logs below.", false);
            }
        });

        connect(m_recoveryDialog, &RecoveryDialog::recheckRequested, this, [this]() {
            m_recoveryDialog->setStatus("Re-scanning system health...", true);
            m_engine.runDiagnosticsAsync();
        });

        connect(m_recoveryDialog, &RecoveryDialog::ignoreAndContinue, this, [this]() {
            LOG_WARN("SelfHealing", "User elected to ignore diagnostic warnings and continue execution.");
            m_recoveryDialog->accept();
            ensureSessionRunning();
        });
    }

    m_recoveryDialog->setReport(report);
    m_recoveryDialog->show();
    m_recoveryDialog->raise();
    m_recoveryDialog->activateWindow();
}

// ── Backward-compatible helper methods ────────────────────────────────────────

bool SelfHealingManager::isBinderAccessible()
{
    bool devBinder = QFileInfo::exists("/dev/binder") && QFileInfo("/dev/binder").isReadable();
    bool binderfs = QFileInfo::exists("/dev/binderfs/binder") && QFileInfo("/dev/binderfs/binder").isReadable();
    return devBinder || binderfs;
}

bool SelfHealingManager::isSessionActive()
{
    QProcess proc;
    proc.start("linstack", QStringList() << "session" << "status");
    proc.waitForFinished(3000);
    QString output = proc.readAllStandardOutput() + proc.readAllStandardError();
    return output.contains("RUNNING", Qt::CaseInsensitive);
}

bool SelfHealingManager::ensureSessionRunning()
{
    if (isSessionActive()) {
        return true;
    }

    LOG_INFO("SelfHealing", "LinStack session is stopped. Starting session background process...");
    QProcess::startDetached("linstack", QStringList() << "session" << "start");

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        if (isSessionActive()) {
            LOG_INFO("SelfHealing", "LinStack session is now RUNNING.");
            return true;
        }
        QThread::msleep(500);
    }

    return isSessionActive();
}

bool SelfHealingManager::checkHealth()
{
    // Fast synchronous check
    HealthReport rep = m_engine.runDiagnosticsSync();
    if (!rep.isHealthy) {
        LOG_WARN("SelfHealing", QString("checkHealth() detected issues: %1").arg(rep.summary));
        return false;
    }
    ensureSessionRunning();
    return true;
}

bool SelfHealingManager::promptRepair()
{
    HealthReport rep = m_engine.latestReport();
    if (rep.checks.isEmpty()) {
        rep = m_engine.runDiagnosticsSync();
    }

    RecoveryDialog dlg(nullptr);
    dlg.setReport(rep);

    connect(&dlg, &RecoveryDialog::autoHealRequested, this, [this, &dlg, &rep]() {
        dlg.setStatus("Executing auto-repair...", true);
        bool ok = executeElevatedFixes(rep);
        if (ok) {
            dlg.accept();
        } else {
            dlg.setStatus("Auto-repair failed. Review logs or continue anyway.", false);
        }
    });

    connect(&dlg, &RecoveryDialog::ignoreAndContinue, &dlg, &QDialog::accept);

    return (dlg.exec() == QDialog::Accepted);
}

bool SelfHealingManager::executeRepair()
{
    HealthReport rep = m_engine.latestReport();
    return executeElevatedFixes(rep);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecoveryDialog Implementation (Cyber-Emerald Themed)
// ─────────────────────────────────────────────────────────────────────────────

RecoveryDialog::RecoveryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("LinStack - Engine Diagnostics & Recovery");
    setFixedSize(680, 520);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    setupUi();
}

void RecoveryDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(14);

    // ── 1. Title bar row with shield icon ─────────────────────────────────────
    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);

    auto *iconLbl = new QLabel(this);
    iconLbl->setPixmap(Theme::renderIconPixmap(Theme::IconType::Shield, Theme::ColorNeon, 26));
    headerRow->addWidget(iconLbl);

    m_titleLabel = new QLabel("LinStack Runtime Diagnostics & Self-Healing", this);
    m_titleLabel->setStyleSheet("color: #F9FAFB; font-size: 15px; font-weight: 700;");
    headerRow->addWidget(m_titleLabel);

    headerRow->addStretch(1);

    m_errorCodeBadge = new QLabel("[FAULT_DETECTED]", this);
    m_errorCodeBadge->setStyleSheet(
        "background-color: rgba(239, 68, 68, 0.2);"
        "color: #EF4444;"
        "border: 1px solid #EF4444;"
        "border-radius: 4px;"
        "padding: 3px 8px;"
        "font-size: 10px;"
        "font-weight: 700;"
        "font-family: monospace;"
    );
    headerRow->addWidget(m_errorCodeBadge);

    rootLayout->addLayout(headerRow);

    // ── 2. Description & Diagnosis Info ───────────────────────────────────────
    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet("color: #E5E7EB; font-size: 11px; line-height: 1.4;");
    rootLayout->addWidget(m_descriptionLabel);

    // ── 3. Progress Bar for Active Operations ────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: #1F2937; border: none; border-radius: 2px; }"
        "QProgressBar::chunk { background: #00FF66; border-radius: 2px; }"
    );
    m_progressBar->hide();
    rootLayout->addWidget(m_progressBar);

    // ── 4. Expandable Log Console ─────────────────────────────────────────────
    auto *consoleHeader = new QLabel("RECENT RUNTIME & DIAGNOSTIC LOGS", this);
    consoleHeader->setStyleSheet("color: #10B981; font-size: 10px; font-weight: 700; letter-spacing: 1px;");
    rootLayout->addWidget(consoleHeader);

    m_logConsole = new QTextEdit(this);
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet(
        "QTextEdit {"
        "  background-color: #05070D;"
        "  color: #34D399;"
        "  font-family: monospace;"
        "  font-size: 10px;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    );
    rootLayout->addWidget(m_logConsole, 1);

    // ── 5. Action Buttons ─────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_btnExit = new QPushButton("Exit", this);
    m_btnExit->setFixedWidth(80);
    connect(m_btnExit, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_btnExit);

    m_btnContinue = new QPushButton("Continue Anyway", this);
    m_btnContinue->setToolTip("Proceed into LinStack despite detected faults");
    connect(m_btnContinue, &QPushButton::clicked, this, &RecoveryDialog::ignoreAndContinue);
    btnRow->addWidget(m_btnContinue);

    btnRow->addStretch(1);

    m_btnRecheck = new QPushButton("🔄 Re-Scan", this);
    m_btnRecheck->setFixedWidth(100);
    connect(m_btnRecheck, &QPushButton::clicked, this, &RecoveryDialog::recheckRequested);
    btnRow->addWidget(m_btnRecheck);

    m_btnAutoHeal = new QPushButton("⚡ Auto-Heal Now", this);
    m_btnAutoHeal->setProperty("primary", true);
    m_btnAutoHeal->setFixedWidth(140);
    connect(m_btnAutoHeal, &QPushButton::clicked, this, &RecoveryDialog::autoHealRequested);
    btnRow->addWidget(m_btnAutoHeal);

    rootLayout->addLayout(btnRow);
}

void RecoveryDialog::setReport(const HealthReport &report)
{
    m_report = report;

    auto fails = report.failedChecks();
    auto warns = report.warningChecks();

    if (!fails.isEmpty()) {
        m_errorCodeBadge->setText(QString("[%1]").arg(fails.first().errorCode));
        m_errorCodeBadge->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.2); color: #EF4444; border: 1px solid #EF4444; "
            "border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    } else if (!warns.isEmpty()) {
        m_errorCodeBadge->setText(QString("[%1]").arg(warns.first().errorCode));
        m_errorCodeBadge->setStyleSheet(
            "background-color: rgba(245, 158, 11, 0.2); color: #F59E0B; border: 1px solid #F59E0B; "
            "border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    } else {
        m_errorCodeBadge->setText("[HEALTHY]");
        m_errorCodeBadge->setStyleSheet(
            "background-color: rgba(16, 185, 129, 0.2); color: #00FF66; border: 1px solid #10B981; "
            "border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    }

    QString desc = QString("<b>Status Summary:</b> %1<br><br>").arg(report.summary);
    if (!fails.isEmpty()) {
        desc += "<b>Critical Faults:</b><ul>";
        for (const auto &f : fails) {
            desc += QString("<li><b>%1:</b> %2<br><span style='color:#34D399;'>Remediation: %3</span></li>")
                        .arg(f.name, f.details, f.remediationHint);
        }
        desc += "</ul>";
    }

    m_descriptionLabel->setText(desc);

    // Update log console with recent lines
    m_logConsole->setPlainText(Logger::instance().recentLogsText(40));
    m_logConsole->verticalScrollBar()->setValue(m_logConsole->verticalScrollBar()->maximum());
}

void RecoveryDialog::setStatus(const QString &status, bool inProgress)
{
    m_titleLabel->setText(status);
    if (inProgress) {
        m_progressBar->show();
        m_btnAutoHeal->setEnabled(false);
        m_btnRecheck->setEnabled(false);
    } else {
        m_progressBar->hide();
        m_btnAutoHeal->setEnabled(true);
        m_btnRecheck->setEnabled(true);
    }
}

void RecoveryDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setBrush(Theme::ColorBgDark);
    p.setPen(QPen(Theme::ColorBorder, 1.5));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::ColorEmerald, 1.0));
    p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 7, 7);
}
