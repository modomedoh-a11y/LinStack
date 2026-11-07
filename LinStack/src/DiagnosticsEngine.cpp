#include "DiagnosticsEngine.h"
#include "Logger.h"

#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTcpSocket>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QTextStream>

QList<DiagnosticCheckResult> HealthReport::failedChecks() const
{
    QList<DiagnosticCheckResult> list;
    for (const auto &c : checks) {
        if (c.status == CheckStatus::Failed) list.append(c);
    }
    return list;
}

QList<DiagnosticCheckResult> HealthReport::warningChecks() const
{
    QList<DiagnosticCheckResult> list;
    for (const auto &c : checks) {
        if (c.status == CheckStatus::Warning) list.append(c);
    }
    return list;
}

QList<DiagnosticCheckResult> HealthReport::autoHealableChecks() const
{
    QList<DiagnosticCheckResult> list;
    for (const auto &c : checks) {
        if (c.status != CheckStatus::Passed && c.canAutoHeal) list.append(c);
    }
    return list;
}

QString HealthReport::formatReport() const
{
    QStringList out;
    out << QString("=== LinStack Health Diagnostic Report [%1] ===")
               .arg(timestamp.toString("yyyy-MM-dd HH:mm:ss"));
    out << QString("Overall Status: %1").arg(isHealthy ? "HEALTHY" : (hasCriticalFailures ? "CRITICAL" : "DEGRADED"));

    for (const auto &c : checks) {
        QString st = (c.status == CheckStatus::Passed) ? "PASS"
                     : (c.status == CheckStatus::Warning) ? "WARN" : "FAIL";
        out << QString("[%1] %2 (%3): %4").arg(st, c.id, c.name, c.details);
        if (!c.errorCode.isEmpty() && c.status != CheckStatus::Passed) {
            out << QString("       ErrorCode: %1 | Fix: %2").arg(c.errorCode, c.remediationHint);
        }
    }
    return out.join("\n");
}

// ── DiagnosticsWorker Implementation ─────────────────────────────────────────

void DiagnosticsWorker::performChecks()
{
    HealthReport report;
    report.timestamp = QDateTime::currentDateTime();

    LOG_INFO("Diagnostics", "Beginning system health check pipeline...");
    emit progressUpdated(5, "Checking Linux kernel binder modules...");

    // ─────────────────────────────────────────────────────────────────────────
    // 1. Kernel Module Inspection
    // ─────────────────────────────────────────────────────────────────────────
    {
        DiagnosticCheckResult r;
        r.id = "KERNEL_BINDER";
        r.category = DiagnosticCategory::KernelModule;
        r.name = "Android Binder Driver (/dev/binder)";
        r.canAutoHeal = true;
        r.requiresElevation = true;

        bool devBinder = QFileInfo::exists("/dev/binder");
        bool devBinderfs = QFileInfo::exists("/dev/binderfs/binder");
        bool vndBinder = QFileInfo::exists("/dev/vndbinder") || QFileInfo::exists("/dev/binderfs/vndbinder");
        bool hwBinder = QFileInfo::exists("/dev/hwbinder") || QFileInfo::exists("/dev/binderfs/hwbinder");

        // Check /proc/modules for binder_linux
        bool moduleLoaded = false;
        QFile modFile("/proc/modules");
        if (modFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = modFile.readAll();
            moduleLoaded = content.contains("binder_linux") || content.contains("binder");
        }

        if ((devBinder || devBinderfs) && vndBinder && hwBinder) {
            r.status = CheckStatus::Passed;
            r.details = QString("Binder IPC nodes active (binder, vndbinder, hwbinder). FS: %1")
                            .arg(devBinderfs ? "/dev/binderfs" : "/dev");
            LOG_INFO("Diagnostics", r.details);
        } else if (devBinder || devBinderfs) {
            r.status = CheckStatus::Warning;
            r.errorCode = "WARN_BINDER_PARTIAL";
            r.details = "Main binder active, but vndbinder or hwbinder nodes are missing.";
            r.remediationHint = "modprobe binder_linux devices=binder,hwbinder,vndbinder";
            LOG_WARN("Diagnostics", r.details);
        } else {
            r.status = CheckStatus::Failed;
            r.errorCode = "ERR_BINDER_MISSING";
            r.details = moduleLoaded
                ? "Kernel module binder_linux is loaded but /dev/binderfs is not mounted."
                : "Kernel binder driver is not loaded (/dev/binder and /dev/binderfs missing).";
            r.remediationHint = "modprobe binder_linux devices=binder,hwbinder,vndbinder";
            LOG_FAULT("Diagnostics", r.errorCode, r.details, r.remediationHint);
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }
    emit progressUpdated(25, "Inspecting binder permissions...");

    {
        DiagnosticCheckResult r;
        r.id = "BINDER_PERMISSIONS";
        r.category = DiagnosticCategory::KernelModule;
        r.name = "Binder Node Permissions";
        r.canAutoHeal = true;
        r.requiresElevation = true;

        QString targetNode = QFileInfo::exists("/dev/binderfs/binder") ? "/dev/binderfs/binder" : "/dev/binder";
        if (QFileInfo::exists(targetNode)) {
            QFileInfo fi(targetNode);
            if (fi.isReadable() && fi.isWritable()) {
                r.status = CheckStatus::Passed;
                r.details = QString("%1 has full read/write permissions for current process.").arg(targetNode);
                LOG_INFO("Diagnostics", r.details);
            } else {
                r.status = CheckStatus::Failed;
                r.errorCode = "ERR_BINDER_PERMISSIONS";
                r.details = QString("Insufficient permissions on %1. Android container cannot map IPC.").arg(targetNode);
                r.remediationHint = QString("chmod 0666 %1").arg(targetNode);
                LOG_FAULT("Diagnostics", r.errorCode, r.details, r.remediationHint);
            }
        } else {
            r.status = CheckStatus::Warning;
            r.errorCode = "WARN_BINDER_NOT_FOUND";
            r.details = "Skipped permission check; binder node not present.";
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }
    emit progressUpdated(40, "Querying container systemd service & sockets...");

    // ─────────────────────────────────────────────────────────────────────────
    // 2. Service & Socket Health
    // ─────────────────────────────────────────────────────────────────────────
    {
        DiagnosticCheckResult r;
        r.id = "CONTAINER_SERVICE";
        r.category = DiagnosticCategory::ServiceSocket;
        r.name = "Container Daemon Service";
        r.canAutoHeal = true;
        r.requiresElevation = true;

        // Query systemctl is-active
        QProcess proc;
        proc.start("systemctl", QStringList() << "is-active" << "linstack-container.service");
        bool finished = proc.waitForFinished(2000);
        QString statusOut = finished ? QString::fromUtf8(proc.readAllStandardOutput()).trimmed() : "timeout";

        if (statusOut == "active") {
            r.status = CheckStatus::Passed;
            r.details = "linstack-container.service is active and running.";
            LOG_INFO("Diagnostics", r.details);
        } else {
            // Check fallback linstack-container
            QProcess procFallback;
            procFallback.start("systemctl", QStringList() << "is-active" << "linstack-container.service");
            if (procFallback.waitForFinished(1500) && procFallback.readAllStandardOutput().trimmed() == "active") {
                r.status = CheckStatus::Passed;
                r.details = "Container daemon is active under linstack-container service.";
                LOG_INFO("Diagnostics", r.details);
            } else {
                r.status = CheckStatus::Warning;
                r.errorCode = "WARN_CONTAINER_SERVICE_STOPPED";
                r.details = QString("Container service status: '%1'. Service is currently idle or stopped.").arg(statusOut);
                r.remediationHint = "systemctl restart linstack-container.service";
                LOG_WARN("Diagnostics", r.details);
            }
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }
    emit progressUpdated(55, "Testing ADB daemon socket connection...");

    {
        DiagnosticCheckResult r;
        r.id = "ADB_SOCKET";
        r.category = DiagnosticCategory::ServiceSocket;
        r.name = "ADB Daemon TCP Socket (127.0.0.1:5555)";
        r.canAutoHeal = true;
        r.requiresElevation = false;

        QTcpSocket socket;
        socket.connectToHost("127.0.0.1", 5555);
        if (socket.waitForConnected(1200)) {
            r.status = CheckStatus::Passed;
            r.details = "Successfully connected to Android ADB daemon at 127.0.0.1:5555.";
            socket.disconnectFromHost();
            LOG_INFO("Diagnostics", r.details);
        } else {
            r.status = CheckStatus::Warning;
            r.errorCode = "WARN_ADB_UNREACHABLE";
            r.details = "ADB daemon socket not responding on 127.0.0.1:5555. APK installation and adb shell may be delayed.";
            r.remediationHint = "adb connect 127.0.0.1:5555";
            LOG_WARN("Diagnostics", r.details);
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }
    emit progressUpdated(70, "Verifying container rootfs & overlay storage...");

    // ─────────────────────────────────────────────────────────────────────────
    // 3. Container Rootfs & Overlay Integrity
    // ─────────────────────────────────────────────────────────────────────────
    {
        DiagnosticCheckResult r;
        r.id = "ROOTFS_OVERLAY";
        r.category = DiagnosticCategory::ContainerRootfs;
        r.name = "Container Storage & Overlay Rootfs";
        r.canAutoHeal = true;
        r.requiresElevation = false;

        QString userStorage = QDir::homePath() + "/.local/share/LinStack";
        bool created = QDir().mkpath(userStorage);
        QFileInfo userFi(userStorage);

        if (!userFi.isWritable() || !created) {
            // Check fallback to /tmp or current directory if home is read-only
            QString altStorage = "/tmp/LinStack";
            QDir().mkpath(altStorage);
            QFileInfo altFi(altStorage);
            if (altFi.isWritable()) {
                r.status = CheckStatus::Warning;
                r.errorCode = "WARN_STORAGE_FALLBACK";
                r.details = QString("Default storage %1 is read-only; using fallback storage %2.").arg(userStorage, altStorage);
                r.remediationHint = QString("mkdir -p %1 && chmod u+rwx %1").arg(userStorage);
                LOG_WARN("Diagnostics", r.details);
            } else {
                r.status = CheckStatus::Failed;
                r.errorCode = "ERR_STORAGE_PERMISSIONS";
                r.details = QString("User storage directory %1 is not writable.").arg(userStorage);
                r.remediationHint = QString("chmod u+rwx %1").arg(userStorage);
                LOG_FAULT("Diagnostics", r.errorCode, r.details, r.remediationHint);
            }
        } else {
            QStorageInfo storage(userStorage);
            qint64 freeGb = storage.bytesAvailable() / (1024 * 1024 * 1024);
            if (freeGb < 1) {
                r.status = CheckStatus::Warning;
                r.errorCode = "WARN_LOW_DISK_SPACE";
                r.details = QString("Low disk space: %1 GB remaining on volume. Android images may fail to expand.").arg(freeGb);
                r.remediationHint = "Free up disk space on the root filesystem.";
                LOG_WARN("Diagnostics", r.details);
            } else {
                r.status = CheckStatus::Passed;
                r.details = QString("Rootfs storage writable. Available volume space: %1 GB.").arg(freeGb);
                LOG_INFO("Diagnostics", r.details);
            }
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }
    emit progressUpdated(85, "Checking display environment and window surfaces...");

    // ─────────────────────────────────────────────────────────────────────────
    // 4. Display & Window Surface Check
    // ─────────────────────────────────────────────────────────────────────────
    {
        DiagnosticCheckResult r;
        r.id = "DISPLAY_SURFACE";
        r.category = DiagnosticCategory::DisplaySurface;
        r.name = "Compositor & Wayland Display Protocol";
        r.canAutoHeal = false;
        r.requiresElevation = false;

        QString waylandDisp = qEnvironmentVariable("WAYLAND_DISPLAY");
        QString x11Disp = qEnvironmentVariable("DISPLAY");
        QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");

        if (waylandDisp.isEmpty() && x11Disp.isEmpty()) {
            r.status = CheckStatus::Failed;
            r.errorCode = "ERR_DISPLAY_NOT_FOUND";
            r.details = "Neither WAYLAND_DISPLAY nor DISPLAY is set. Cannot bind Android graphics output.";
            r.remediationHint = "Start LinStack inside an active desktop session.";
            LOG_CRITICAL("Diagnostics", r.details);
        } else {
            r.status = CheckStatus::Passed;
            r.details = QString("Active Display: Session=%1 | Wayland=%2 | X11=%3")
                            .arg(sessionType.isEmpty() ? "auto" : sessionType)
                            .arg(waylandDisp.isEmpty() ? "none" : waylandDisp)
                            .arg(x11Disp.isEmpty() ? "none" : x11Disp);
            LOG_INFO("Diagnostics", r.details);
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }

    {
        DiagnosticCheckResult r;
        r.id = "SURFACE_EMBED_WMCTRL";
        r.category = DiagnosticCategory::DisplaySurface;
        r.name = "Window Embedding Subsystem (wmctrl)";
        r.canAutoHeal = false;
        r.requiresElevation = false;

        QString wmctrlPath = QStandardPaths::findExecutable("wmctrl");
        if (!wmctrlPath.isEmpty()) {
            r.status = CheckStatus::Passed;
            r.details = QString("wmctrl helper is installed at %1 for surface detection.").arg(wmctrlPath);
            LOG_INFO("Diagnostics", r.details);
        } else {
            r.status = CheckStatus::Warning;
            r.errorCode = "WARN_WMCTRL_MISSING";
            r.details = "wmctrl binary not found in PATH. In-frame surface discovery may rely on native Wayland foreign toplevel.";
            r.remediationHint = "sudo apt install wmctrl || sudo dnf install wmctrl";
            LOG_WARN("Diagnostics", r.details);
        }
        report.checks.append(r);
        emit checkCompleted(r);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Finalize report
    // ─────────────────────────────────────────────────────────────────────────
    emit progressUpdated(100, "Diagnostics complete.");

    report.hasCriticalFailures = !report.failedChecks().isEmpty();
    report.hasWarnings = !report.warningChecks().isEmpty();
    report.isHealthy = !report.hasCriticalFailures;

    if (report.hasCriticalFailures) {
        report.summary = QString("Diagnostic found %1 critical fault(s) and %2 warning(s).")
                             .arg(report.failedChecks().size())
                             .arg(report.warningChecks().size());
    } else if (report.hasWarnings) {
        report.summary = QString("Diagnostics passed with %1 warning(s). System is functional.")
                             .arg(report.warningChecks().size());
    } else {
        report.summary = "All subsystems, kernel modules, sockets, and storage checks passed successfully.";
    }

    LOG_INFO("Diagnostics", report.summary);
    emit diagnosticsFinished(report);
}

// ── DiagnosticsEngine Implementation ─────────────────────────────────────────

DiagnosticsEngine::DiagnosticsEngine(QObject *parent)
    : QObject(parent)
{
}

DiagnosticsEngine::~DiagnosticsEngine()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(1000);
    }
}

void DiagnosticsEngine::runDiagnosticsAsync()
{
    if (m_isRunning) {
        LOG_WARN("Diagnostics", "Diagnostics already running in background.");
        return;
    }

    m_isRunning = true;

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }

    m_workerThread = new QThread(this);
    m_worker = new DiagnosticsWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &DiagnosticsWorker::performChecks);
    connect(m_worker, &DiagnosticsWorker::progressUpdated, this, &DiagnosticsEngine::progressUpdated);
    connect(m_worker, &DiagnosticsWorker::checkCompleted, this, &DiagnosticsEngine::checkCompleted);
    connect(m_worker, &DiagnosticsWorker::diagnosticsFinished, this, &DiagnosticsEngine::handleDiagnosticsFinished);

    // Clean up worker after thread finishes
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

void DiagnosticsEngine::handleDiagnosticsFinished(const HealthReport &report)
{
    m_latestReport = report;
    m_isRunning = false;

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
    }

    emit diagnosticsFinished(report);
}

HealthReport DiagnosticsEngine::runDiagnosticsSync()
{
    DiagnosticsWorker worker;
    HealthReport resultReport;

    connect(&worker, &DiagnosticsWorker::diagnosticsFinished, this,
            [&resultReport](const HealthReport &rep) {
        resultReport = rep;
    });

    worker.performChecks();
    m_latestReport = resultReport;
    return resultReport;
}
