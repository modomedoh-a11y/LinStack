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
#include <QGraphicsDropShadowEffect>
#include <QWindow>
#include <QMouseEvent>
#include <QFrame>
#include <QStandardPaths>
#include <QTimer>
#include "PathResolver.h"

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

    const auto warns = report.warningChecks();
    const auto fails = report.failedChecks();

    bool hasBlockingIssue = !fails.isEmpty();
    if (!hasBlockingIssue) {
        for (const auto &c : warns) {
            if (c.requiresElevation || c.id == "KERNEL_BINDER") {
                hasBlockingIssue = true;
                break;
            }
        }
    }

    if (!hasBlockingIssue) {
        LOG_INFO("SelfHealing", "Diagnostics verified all critical subsystems healthy.");
        if (m_recoveryDialog && m_recoveryDialog->isVisible()) {
            m_recoveryDialog->setStatus("All critical subsystems healthy! Ready.", false);
            m_recoveryDialog->accept();
        }
        return;
    }


    LOG_WARN("SelfHealing", QString("Diagnostics reported issues/warnings: %1").arg(report.summary));

    // Step 1: Attempt non-elevated programmatic fixes first
    bool progFixed = executeProgrammaticFixes(report);

    // Step 2: Check if any failed or warning checks require root elevation (e.g., binderfs)
    bool hasElevatedIssues = false;
    for (const auto &c : fails) {
        if (c.requiresElevation) {
            hasElevatedIssues = true;
            break;
        }
    }
    if (!hasElevatedIssues) {
        for (const auto &c : warns) {
            if (c.requiresElevation || c.id == "KERNEL_BINDER") {
                hasElevatedIssues = true;
                break;
            }
        }
    }

    if (hasElevatedIssues || (!progFixed && report.hasCriticalFailures)) {
        LOG_FAULT("SelfHealing", "FAULT_REQUIRES_ATTENTION",
                  "Unresolved faults or critical warnings remain. Presenting recovery options.",
                  "Launch elevated auto-heal or check logs.");
        showRecoveryDialog(report, m_parentUi);
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
            QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            if (dir.isEmpty()) {
                dir = QDir::homePath() + "/.local/share/linStack";
            }
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
    QString resDir = PathResolver::resolveResourcesDir();
    QString coreDir = PathResolver::resolveCoreDir();

    // 1. Ensure core backend is installed if missing
    if (!QFileInfo::exists("/usr/bin/linstack") && !coreDir.isEmpty()) {
        commands << QString("make -C \"%1\" install 2>/dev/null || true").arg(coreDir);
    }

    // 2. Ensure container config exists and images_path points to resolved resources
    if (!QFileInfo::exists("/var/lib/linstack/linstack.cfg")) {
        commands << QString("linstack init -f -i \"%1\" 2>/dev/null || true").arg(resDir);
    } else if (!resDir.isEmpty()) {
        commands << QString("sed -i 's|images_path = .*|images_path = %1|' /var/lib/linstack/linstack.cfg 2>/dev/null || true").arg(resDir);
    }

    // 3. Kernel binder IPC module & filesystem check
    bool needBinder = false;
    for (const auto &c : report.checks) {
        if (c.id == "KERNEL_BINDER" && c.status != CheckStatus::Passed) {
            needBinder = true;
            break;
        }
    }

    if (needBinder || !isBinderAccessible()) {
        // Check if binderfs is already mounted (kernel built-in binder, e.g. Fedora 6.x)
        bool binderfsAlreadyMounted = QFileInfo::exists("/dev/binderfs/binder");
        bool binderDevExists = QFileInfo::exists("/dev/binder");

        if (!binderfsAlreadyMounted && !binderDevExists) {
            // Module not loaded at all — attempt modprobe then mount binderfs
            commands << "modprobe binder_linux devices=binder,hwbinder,vndbinder 2>/dev/null || true";
            commands << "mkdir -p /dev/binderfs";
            commands << "mount -t binder binder /dev/binderfs 2>/dev/null || true";
        }
        // Always try to fix permissions on whichever nodes exist
        commands << "chmod 0666 /dev/binder /dev/binderfs/binder /dev/vndbinder /dev/hwbinder 2>/dev/null || true";
    }

    // 4. Ensure container daemon service is enabled and restarted
    commands << "systemctl enable --now linstack-container.service 2>/dev/null || true";
    commands << "systemctl restart linstack-container.service 2>/dev/null || true";

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
    PathResolver::configureProcess(&proc, QStringList() << "status");
    proc.start();
    if (!proc.waitForFinished(1500)) {
        proc.kill();
        proc.waitForFinished(300);
        return false;
    }
    QString output = proc.readAllStandardOutput() + proc.readAllStandardError();
    return output.contains("RUNNING", Qt::CaseInsensitive) && !output.contains("STOPPED", Qt::CaseInsensitive);
}

bool SelfHealingManager::ensureSessionRunning()
{
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";
    QString socketPath = runtimeDir + "/wayland-linstack-0";

    // Enforce environment variables
    qputenv("WAYLAND_DISPLAY", "wayland-linstack-0");
    qputenv("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
    qputenv("XDG_RUNTIME_DIR", runtimeDir.toUtf8());

    if (isSessionActive()) {
        LOG_INFO("SelfHealing", "LinStack session is already active. Launching show-full-ui on wayland-linstack-0...");

        QProcess *uiProc = new QProcess(this);
        connect(uiProc, &QProcess::readyReadStandardOutput, this, [uiProc]() {
            const QString out = QString::fromUtf8(uiProc->readAllStandardOutput()).trimmed();
            if (!out.isEmpty()) LOG_INFO("UI[STDOUT]", out);
        });
        connect(uiProc, &QProcess::readyReadStandardError, this, [uiProc]() {
            const QString err = QString::fromUtf8(uiProc->readAllStandardError()).trimmed();
            if (!err.isEmpty()) LOG_WARN("UI[STDERR]", err);
        });
        connect(uiProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [uiProc](int code, QProcess::ExitStatus) {
            LOG_INFO("UI", QString("linstack show-full-ui process exited with code %1").arg(code));
            uiProc->deleteLater();
        });

        QProcessEnvironment uiEnv = QProcessEnvironment::systemEnvironment();
        uiEnv.insert("XDG_RUNTIME_DIR", runtimeDir);
        uiEnv.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
        uiEnv.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
        uiProc->setProcessEnvironment(uiEnv);
        PathResolver::configureProcess(uiProc, QStringList() << "show-full-ui");
        uiProc->start();
        return true;
    }

    // Verify embedded compositor socket exists before starting session
    int socketWaitRetries = 0;
    while (!QFile::exists(socketPath) && socketWaitRetries < 20) {
        QThread::msleep(100);
        socketWaitRetries++;
    }

    if (!QFile::exists(socketPath)) {
        LOG_WARN("SelfHealing", QString("Embedded Wayland socket %1 not found yet. Deferring session start.").arg(socketPath));
        return false;
    }
    LOG_INFO("SelfHealing", QString("Verified active embedded Wayland socket: %1. Starting session.").arg(socketPath));

    LOG_INFO("SelfHealing", "Starting LinStack session with WAYLAND_DISPLAY=wayland-linstack-0...");
    QProcess *proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc]() {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) LOG_INFO("Session[STDOUT]", out);
    });
    connect(proc, &QProcess::readyReadStandardError, this, [proc]() {
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!err.isEmpty()) LOG_WARN("Session[STDERR]", err);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [proc](int code, QProcess::ExitStatus) {
        LOG_INFO("Session", QString("linstack session start process exited with code %1").arg(code));
        proc->deleteLater();
    });

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_RUNTIME_DIR", runtimeDir);
    env.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
    env.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
    proc->setProcessEnvironment(env);
    PathResolver::configureProcess(proc, QStringList() << "session" << "start");
    proc->start();

    // Trigger show-full-ui after session starts
    QTimer::singleShot(2500, this, [this, runtimeDir]() {
        QProcess *uiProc = new QProcess(this);
        connect(uiProc, &QProcess::readyReadStandardOutput, this, [uiProc]() {
            const QString out = QString::fromUtf8(uiProc->readAllStandardOutput()).trimmed();
            if (!out.isEmpty()) LOG_INFO("UI[STDOUT]", out);
        });
        connect(uiProc, &QProcess::readyReadStandardError, this, [uiProc]() {
            const QString err = QString::fromUtf8(uiProc->readAllStandardError()).trimmed();
            if (!err.isEmpty()) LOG_WARN("UI[STDERR]", err);
        });
        connect(uiProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [uiProc](int code, QProcess::ExitStatus) {
            LOG_INFO("UI", QString("linstack show-full-ui process exited with code %1").arg(code));
            uiProc->deleteLater();
        });

        QProcessEnvironment uiEnv = QProcessEnvironment::systemEnvironment();
        uiEnv.insert("XDG_RUNTIME_DIR", runtimeDir);
        uiEnv.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
        uiEnv.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
        uiProc->setProcessEnvironment(uiEnv);
        PathResolver::configureProcess(uiProc, QStringList() << "show-full-ui");
        uiProc->start();
    });

    return true;
}

bool SelfHealingManager::checkHealth()
{
    // Fast synchronous check
    HealthReport rep = m_engine.runDiagnosticsSync();
    if (!rep.isHealthy) {
        LOG_WARN("SelfHealing", QString("checkHealth() detected issues: %1").arg(rep.summary));
        return false;
    }
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
    setWindowTitle("linStack Runtime Diagnostics & Self-Healing");
    setFixedSize(740, 560);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    setupUi();
}

void RecoveryDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(0);

    // Main styled container widget hosting cyber-emerald styling
    m_mainContainer = new QWidget(this);
    m_mainContainer->setObjectName("recoveryContainer");
    m_mainContainer->setAttribute(Qt::WA_StyledBackground, true);
    m_mainContainer->setStyleSheet(
        "#recoveryContainer {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #10B981;"
        "  border-radius: 10px;"
        "}"
    );

    // Smooth drop shadow around the frameless dialog container
    auto *shadow = new QGraphicsDropShadowEffect(m_mainContainer);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 200));
    shadow->setOffset(0, 6);
    m_mainContainer->setGraphicsEffect(shadow);

    rootLayout->addWidget(m_mainContainer);

    auto *containerLayout = new QVBoxLayout(m_mainContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // ── 1. Custom Draggable Title Bar ─────────────────────────────────────────
    m_titleBarWidget = new QWidget(m_mainContainer);
    m_titleBarWidget->setObjectName("recoveryTitleBar");
    m_titleBarWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBarWidget->setFixedHeight(44);
    m_titleBarWidget->setStyleSheet(
        "#recoveryTitleBar {"
        "  background-color: #111827;"
        "  border-top-left-radius: 9px;"
        "  border-top-right-radius: 9px;"
        "  border-bottom: 1px solid #1F2937;"
        "}"
    );
    m_titleBarWidget->installEventFilter(this);

    auto *titleLayout = new QHBoxLayout(m_titleBarWidget);
    titleLayout->setContentsMargins(14, 0, 8, 0);
    titleLayout->setSpacing(10);

    // Cyber Shield / Diagnostics Icon
    auto *iconLbl = new QLabel(m_titleBarWidget);
    iconLbl->setPixmap(Theme::renderIconPixmap(Theme::IconType::Shield, Theme::ColorNeon, 20));
    iconLbl->installEventFilter(this);
    titleLayout->addWidget(iconLbl);

    // Dialog Title Text
    m_titleLabel = new QLabel("linStack Runtime Diagnostics & Self-Healing", m_titleBarWidget);
    m_titleLabel->setStyleSheet("color: #F9FAFB; font-size: 13px; font-weight: 700;");
    m_titleLabel->installEventFilter(this);
    titleLayout->addWidget(m_titleLabel);

    // Fault Status / Code Badge
    m_errorCodeBadge = new QLabel("[DIAGNOSTICS]", m_titleBarWidget);
    m_errorCodeBadge->setStyleSheet(
        "background-color: rgba(239, 68, 68, 0.2);"
        "color: #EF4444;"
        "border: 1px solid #EF4444;"
        "border-radius: 4px;"
        "padding: 2px 7px;"
        "font-size: 10px;"
        "font-weight: 700;"
        "font-family: monospace;"
    );
    m_errorCodeBadge->installEventFilter(this);
    titleLayout->addWidget(m_errorCodeBadge);

    titleLayout->addStretch(1);

    // Minimize Button
    m_minBtn = new QPushButton(m_titleBarWidget);
    m_minBtn->setFixedSize(36, 30);
    m_minBtn->setIcon(Theme::createIcon(Theme::IconType::WindowMinimize, Theme::ColorTextMuted, 12));
    m_minBtn->setToolTip("Minimize");
    m_minBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: #161F30; color: #10B981; }"
    );
    connect(m_minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    titleLayout->addWidget(m_minBtn);

    // Close Button
    m_closeBtn = new QPushButton(m_titleBarWidget);
    m_closeBtn->setFixedSize(36, 30);
    m_closeBtn->setIcon(Theme::createIcon(Theme::IconType::WindowClose, Theme::ColorTextMuted, 12));
    m_closeBtn->setToolTip("Close Diagnostics");
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: #EF4444; border-radius: 4px; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(m_closeBtn);

    containerLayout->addWidget(m_titleBarWidget);

    // ── 2. Content Area ───────────────────────────────────────────────────────
    auto *contentArea = new QWidget(m_mainContainer);
    auto *contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(18, 14, 18, 16);
    contentLayout->setSpacing(12);

    // Status Summary Banner Card
    auto *statusCard = new QFrame(contentArea);
    statusCard->setObjectName("statusCard");
    statusCard->setStyleSheet(
        "#statusCard {"
        "  background-color: #161F30;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 6px;"
        "}"
    );
    auto *statusCardLayout = new QVBoxLayout(statusCard);
    statusCardLayout->setContentsMargins(12, 10, 12, 10);
    statusCardLayout->setSpacing(4);

    m_descriptionLabel = new QLabel(statusCard);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet("color: #E5E7EB; font-size: 11px; line-height: 1.4;");
    statusCardLayout->addWidget(m_descriptionLabel);
    contentLayout->addWidget(statusCard);

    // Progress Bar for Active Operations
    m_progressBar = new QProgressBar(contentArea);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: #1F2937; border: none; border-radius: 2px; }"
        "QProgressBar::chunk { background: #00FF66; border-radius: 2px; }"
    );
    m_progressBar->hide();
    contentLayout->addWidget(m_progressBar);

    // Monospace Log Console Header
    auto *consoleHeader = new QLabel("RECENT RUNTIME & DIAGNOSTIC LOGS", contentArea);
    consoleHeader->setStyleSheet("color: #10B981; font-size: 10px; font-weight: 700; letter-spacing: 1px;");
    contentLayout->addWidget(consoleHeader);

    // Monospace Log Console
    m_logConsole = new QTextEdit(contentArea);
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet(
        "QTextEdit {"
        "  background-color: #05070D;"
        "  color: #34D399;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
        "  font-size: 10px;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    );
    contentLayout->addWidget(m_logConsole, 1);

    // ── 3. Bottom Action Buttons ──────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_btnExit = new QPushButton("Exit", contentArea);
    m_btnExit->setFixedWidth(80);
    m_btnExit->setStyleSheet(
        "QPushButton { background-color: #1F2937; color: #9CA3AF; border: 1px solid #374151; border-radius: 4px; padding: 6px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: #374151; color: #F9FAFB; }"
    );
    connect(m_btnExit, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_btnExit);

    m_btnContinue = new QPushButton("Continue Anyway", contentArea);
    m_btnContinue->setToolTip("Proceed into linStack despite detected faults");
    m_btnContinue->setStyleSheet(
        "QPushButton { background-color: #1F2937; color: #9CA3AF; border: 1px solid #374151; border-radius: 4px; padding: 6px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: #374151; color: #F9FAFB; }"
    );
    connect(m_btnContinue, &QPushButton::clicked, this, &RecoveryDialog::ignoreAndContinue);
    btnRow->addWidget(m_btnContinue);

    btnRow->addStretch(1);

    m_btnRecheck = new QPushButton("🔄 Re-Scan", contentArea);
    m_btnRecheck->setFixedHeight(32);
    m_btnRecheck->setFixedWidth(110);
    m_btnRecheck->setStyleSheet(
        "QPushButton { background-color: #161F30; color: #34D399; border: 1px solid #10B981; border-radius: 4px; padding: 4px 10px; font-weight: 600; font-size: 11px; }"
        "QPushButton:hover { background-color: rgba(16, 185, 129, 0.2); }"
    );
    connect(m_btnRecheck, &QPushButton::clicked, this, &RecoveryDialog::recheckRequested);
    btnRow->addWidget(m_btnRecheck);

    m_btnAutoHeal = new QPushButton("⚡ Auto-Heal Now", contentArea);
    m_btnAutoHeal->setProperty("primary", true);
    m_btnAutoHeal->setFixedHeight(32);
    m_btnAutoHeal->setFixedWidth(155);
    m_btnAutoHeal->setStyleSheet(
        "QPushButton { background-color: #10B981; color: #0B0F19; border: 1px solid #00FF66; border-radius: 4px; padding: 4px 12px; font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { background-color: #00FF66; }"
        "QPushButton:disabled { background-color: #1F2937; color: #6B7280; border-color: #374151; }"
    );
    connect(m_btnAutoHeal, &QPushButton::clicked, this, &RecoveryDialog::autoHealRequested);
    btnRow->addWidget(m_btnAutoHeal);

    contentLayout->addLayout(btnRow);

    containerLayout->addWidget(contentArea, 1);
}

bool RecoveryDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBarWidget || watched == m_titleLabel || watched == m_errorCodeBadge) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (windowHandle() && windowHandle()->startSystemMove()) {
                    return true;
                }
                m_isDragging = true;
                m_dragStartPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_isDragging && (me->buttons() & Qt::LeftButton)) {
                move(me->globalPosition().toPoint() - m_dragStartPos);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_isDragging = false;
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void RecoveryDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_titleBarWidget) {
        QPoint ptInTitle = m_titleBarWidget->mapFrom(this, event->position().toPoint());
        if (m_titleBarWidget->rect().contains(ptInTitle)) {
            QWidget *child = m_titleBarWidget->childAt(ptInTitle);
            if (child != m_minBtn && child != m_closeBtn) {
                if (windowHandle() && windowHandle()->startSystemMove()) {
                    event->accept();
                    return;
                }
                m_isDragging = true;
                m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
                event->accept();
                return;
            }
        }
    }
    QDialog::mousePressEvent(event);
}

void RecoveryDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void RecoveryDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QDialog::mouseReleaseEvent(event);
}

void RecoveryDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // Background is translucent to permit QGraphicsDropShadowEffect rendering around m_mainContainer
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
            "border-radius: 4px; padding: 2px 7px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    } else if (!warns.isEmpty()) {
        m_errorCodeBadge->setText(QString("[%1]").arg(warns.first().errorCode));
        m_errorCodeBadge->setStyleSheet(
            "background-color: rgba(245, 158, 11, 0.2); color: #F59E0B; border: 1px solid #F59E0B; "
            "border-radius: 4px; padding: 2px 7px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    } else {
        m_errorCodeBadge->setText("[HEALTHY]");
        m_errorCodeBadge->setStyleSheet(
            "background-color: rgba(16, 185, 129, 0.2); color: #00FF66; border: 1px solid #10B981; "
            "border-radius: 4px; padding: 2px 7px; font-size: 10px; font-weight: 700; font-family: monospace;"
        );
    }

    QString desc = QString("<b>Status Summary:</b> %1<br><br>").arg(report.summary);
    if (!fails.isEmpty()) {
        desc += "<b>Critical Faults:</b><ul style='margin-top: 4px; margin-bottom: 4px;'>";
        for (const auto &f : fails) {
            desc += QString("<li><b>%1:</b> %2<br><span style='color:#34D399;'>Remediation: %3</span></li>")
                        .arg(f.name, f.details, f.remediationHint);
        }
        desc += "</ul>";
    }

    m_descriptionLabel->setText(desc);

    // Update log console with recent lines
    m_logConsole->setPlainText(Logger::instance().recentLogsText(50));
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
