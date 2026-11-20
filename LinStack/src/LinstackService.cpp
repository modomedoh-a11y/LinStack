#include "LinstackService.h"
#include "PathResolver.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDateTime>
#include <QThread>

LinstackService::LinstackService(QObject *parent)
    : QObject(parent)
{
    // User data location follows XDG via PathResolver
    m_scriptsPath = PathResolver::userDataDir() + "/linstack_script";

    // Initialize predefined gaming profiles for unlocking high refresh rates
    m_profiles.insert("ROG Phone 5", DeviceProfileInfo{
        "ASUS ROG Phone 5 (144 FPS)",
        "asus",
        "asus",
        "ASUS_I005D",
        "ASUS_I005D",
        "asus/WW_I005D/ASUS_I005D:11/RKQ1.201022.002/18.0840.2201.226:user/release-keys",
        144
    });

    m_profiles.insert("Galaxy S21 Ultra", DeviceProfileInfo{
        "Samsung Galaxy S21 Ultra (120 FPS)",
        "samsung",
        "samsung",
        "SM-G998B",
        "p3s",
        "samsung/p3sxxx/p3s:12/SP1A.210812.016/G998BXXU4BVE9:user/release-keys",
        120
    });

    m_profiles.insert("OnePlus 9 Pro", DeviceProfileInfo{
        "OnePlus 9 Pro (90 FPS)",
        "OnePlus",
        "OnePlus",
        "LE2123",
        "OnePlus9Pro",
        "OnePlus/OnePlus9Pro_EEA/OnePlus9Pro:11/RKQ1.201112.002/2107082109:user/release-keys",
        90
    });

    // ADB Socket client setup
    m_adbSocket = new QTcpSocket(this);
    connect(m_adbSocket, &QTcpSocket::connected, this, &LinstackService::handleAdbSocketConnected);
    connect(m_adbSocket, &QTcpSocket::errorOccurred, this, &LinstackService::handleAdbSocketError);

    // Periodic ADB probe timer (every 4 seconds)
    m_adbPollTimer = new QTimer(this);
    m_adbPollTimer->setInterval(4000);
    connect(m_adbPollTimer, &QTimer::timeout, this, &LinstackService::checkAdbConnection);
    m_adbPollTimer->start();

    // Periodic container/session status poller
    m_statusPollTimer = new QTimer(this);
    m_statusPollTimer->setInterval(3000);
    connect(m_statusPollTimer, &QTimer::timeout, this, &LinstackService::refreshStatus);
    m_statusPollTimer->start();
}

LinstackService::~LinstackService()
{
    m_isStopping = true;
    // إيقاف جميع المؤقتات لمنع إعادة تشغيل الواجهة أثناء إغلاق البرنامج
    if (m_adbPollTimer) m_adbPollTimer->stop();
    if (m_statusPollTimer) m_statusPollTimer->stop();
    if (m_sessionPollTimer) m_sessionPollTimer->stop();
    if (m_uiRetryTimer) m_uiRetryTimer->stop();

    // إغلاق عملية الواجهة (show-full-ui) بنظافة
    if (m_uiProcess && m_uiProcess->state() != QProcess::NotRunning) {
        m_uiProcess->terminate();
        if (!m_uiProcess->waitForFinished(1000)) {
            m_uiProcess->kill();
            m_uiProcess->waitForFinished(500);
        }
    }

    // إغلاق عملية الجلسة (session start) بنظافة
    if (m_sessionProcess && m_sessionProcess->state() != QProcess::NotRunning) {
        m_sessionProcess->terminate();
        if (!m_sessionProcess->waitForFinished(1500)) {
            m_sessionProcess->kill();
            m_sessionProcess->waitForFinished(500);
        }
    }

    // إغلاق أي سكريبت قيد التشغيل
    if (m_scriptProcess && m_scriptProcess->state() != QProcess::NotRunning) {
        m_scriptProcess->terminate();
        if (!m_scriptProcess->waitForFinished(1000)) {
            m_scriptProcess->kill();
            m_scriptProcess->waitForFinished(500);
        }
    }
}


KernelDiagnostic LinstackService::runKernelDiagnostics()
{
    KernelDiagnostic diag;

    // 1. Check for binder nodes
    diag.binderFound = QFileInfo::exists("/dev/binder") || QFileInfo::exists("/dev/binderfs/binder");
    diag.binderfsMounted = QFileInfo::exists("/dev/binderfs");
    diag.vndbinderFound = QFileInfo::exists("/dev/vndbinder") || QFileInfo::exists("/dev/binderfs/vndbinder");
    diag.hwbinderFound = QFileInfo::exists("/dev/hwbinder") || QFileInfo::exists("/dev/binderfs/hwbinder");

    // 2. Check for ashmem or memfd
    diag.ashmemOrMemfdFound = QFileInfo::exists("/dev/ashmem") || QFileInfo::exists("/proc/sys/vm");

    // 3. Check for systemd
    diag.systemdActive = QFileInfo::exists("/run/systemd/system");

    // 4. Check for installed binaries via portable path resolver
    diag.linstackInstalled = !PathResolver::resolveLinstackExecutable().isEmpty();
    diag.adbInstalled = !QStandardPaths::findExecutable("adb").isEmpty();

    // Evaluate issues and construct remediation instructions
    if (!diag.binderFound) {
        diag.issues.append("Missing Linux kernel binder driver (/dev/binder or /dev/binderfs/binder).");
        diag.remediationCommands.append("sudo modprobe binder_linux");
        diag.remediationCommands.append("sudo mkdir -p /dev/binderfs && sudo mount -t binder binder /dev/binderfs");
    }

    if (!diag.vndbinderFound || !diag.hwbinderFound) {
        diag.issues.append("VND/HW binder nodes not found in /dev/binderfs.");
        diag.remediationCommands.append("sudo mount -o remount,devices=\"binder,hwbinder,vndbinder\" /dev/binderfs");
    }

    if (!diag.linstackInstalled) {
        diag.issues.append("LinStack binary not found in system PATH.");
        diag.remediationCommands.append("linstack --help");
    }

    if (!diag.adbInstalled) {
        diag.issues.append("Android Debug Bridge (adb) binary not found.");
        diag.remediationCommands.append("sudo apt install adb  # (or sudo dnf install android-tools)");
    }

    if (diag.isReady()) {
        diag.summary = "All Linux kernel binders, binderfs nodes, and LinStack subsystems are verified.";
    } else {
        diag.summary = QString("Kernel diagnostics detected %1 potential blocker(s).").arg(diag.issues.size());
    }

    emit diagnosticFinished(diag);
    emit logMessage("Diagnostics", diag.summary);

    return diag;
}

void LinstackService::setContainerState(ContainerState state)
{
    if (m_containerState != state) {
        m_containerState = state;
        emit containerStateChanged(m_containerState);
    }
}

void LinstackService::setSessionState(SessionState state)
{
    if (m_sessionState != state) {
        m_sessionState = state;
        emit sessionStateChanged(m_sessionState);
    }
}

void LinstackService::startContainerService()
{
    emit logMessage("Container", "Starting linstack-container service...");
    setContainerState(ContainerState::Starting);

    auto *proc = new QProcess(this);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        Q_UNUSED(status);
        if (exitCode == 0) {
            emit logMessage("Container", "linstack-container service started successfully.");
            setContainerState(ContainerState::Running);
        } else {
            QString err = proc->readAllStandardError();
            if (err.isEmpty()) err = proc->readAllStandardOutput();
            emit logMessage("Container", QString("Failed to start container (exit %1): %2").arg(exitCode).arg(err));
            setContainerState(ContainerState::Failed);

            QStringList remediation;
            remediation << "sudo systemctl start linstack-container.service"
                        << "sudo systemctl status linstack-container.service"
                        << "journalctl -u linstack-container.service -e";

            emit errorOccurred(ServiceError::SystemdServiceFailed,
                               QString("Container service failed to start: %1").arg(err.trimmed()),
                               remediation);
        }
        proc->deleteLater();
    });

    if (QFileInfo::exists("/run/systemd/system")) {
        proc->start("systemctl", QStringList() << "start" << "linstack-container.service");
    } else {
        PathResolver::configureProcess(proc, QStringList() << "container" << "start");
        proc->start();
    }
}

void LinstackService::stopContainerService()
{
    emit logMessage("Container", "Stopping linstack-container service...");
    setContainerState(ContainerState::Stopping);

    auto *proc = new QProcess(this);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        Q_UNUSED(exitCode);
        Q_UNUSED(status);
        setContainerState(ContainerState::Stopped);
        emit logMessage("Container", "linstack-container service stopped.");
        proc->deleteLater();
    });

    if (QFileInfo::exists("/run/systemd/system")) {
        proc->start("systemctl", QStringList() << "stop" << "linstack-container.service");
    } else {
        PathResolver::configureProcess(proc, QStringList() << "container" << "stop");
        proc->start();
    }
}

void LinstackService::restartContainerService()
{
    emit logMessage("Container", "Restarting linstack-container service...");
    stopContainerService();
    QTimer::singleShot(1500, this, &LinstackService::startContainerService);
}

bool LinstackService::isSessionRunning() const
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

void LinstackService::setHasFirstFrame(bool hasFrame)
{
    m_hasFirstFrame = hasFrame;
    if (m_hasFirstFrame) {
        if (m_uiRetryTimer) {
            m_uiRetryTimer->stop();
        }
        LOG_INFO("Session", "First Android surface frame committed! Stopping show-full-ui retries.");
    }
}

void LinstackService::startSession()
{
    m_isStopping = false;
    if (m_sessionState == SessionState::Starting || m_sessionState == SessionState::Active) {
        emit logMessage("Session", "LinStack session is already starting or active.");
        return;
    }

    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";

    // Set WAYLAND_DISPLAY and XDG_RUNTIME_DIR in process environment
    qputenv("WAYLAND_DISPLAY", "wayland-linstack-0");
    qputenv("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
    qputenv("XDG_RUNTIME_DIR", runtimeDir.toUtf8());

    // 1. التأكد من أن خدمة الحاوية (linstack-container) تعمل قبل ضبط الخصائص وبدء الجلسة
    if (QFileInfo::exists("/run/systemd/system")) {
        QProcess sysChk;
        sysChk.start("systemctl", QStringList() << "is-active" << "--quiet" << "linstack-container.service");
        sysChk.waitForFinished(1000);
        if (sysChk.exitCode() != 0) {
            LOG_INFO("Container", "Starting linstack-container.service before configuring display resolution...");
            QProcess sysStart;
            sysStart.start("systemctl", QStringList() << "start" << "linstack-container.service");
            sysStart.waitForFinished(4000);
        }
    }

    // 2. كتابة أوامر تحديد أبعاد شاشة الأندرويد (width & height) بشكل متزامن قبل إقلاع الجلسة
    if (m_desiredWidth > 0 && m_desiredHeight > 0) {
        // حذف أو تحديث display-manager-state.xml حتى لا يفرض DisplayManager ارتفاعاً قديماً (مثل 1080)
        QString dmStateFile = PathResolver::userDataDir() + "/data/system/display-manager-state.xml";
        if (QFile::exists(dmStateFile)) {
            QFile f(dmStateFile);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString xml = QString::fromUtf8(f.readAll());
                f.close();
                xml.replace(QRegularExpression("<stable-display-width>\d+</stable-display-width>"),
                            QString("<stable-display-width>%1</stable-display-width>").arg(m_desiredWidth));
                xml.replace(QRegularExpression("<stable-display-height>\d+</stable-display-height>"),
                            QString("<stable-display-height>%1</stable-display-height>").arg(m_desiredHeight));
                if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    f.write(xml.toUtf8());
                    f.close();
                }
            }
        }
        LOG_INFO("Session", QString("Setting container resolution properties: %1x%2").arg(m_desiredWidth).arg(m_desiredHeight));
        QProcess pW;
        PathResolver::configureProcess(&pW, QStringList() << "prop" << "set" << "persist.waydroid.width" << QString::number(m_desiredWidth));
        pW.start();
        pW.waitForFinished(2000);

        QProcess pH;
        PathResolver::configureProcess(&pH, QStringList() << "prop" << "set" << "persist.waydroid.height" << QString::number(m_desiredHeight));
        pH.start();
        pH.waitForFinished(2000);

        QProcess pLW;
        PathResolver::configureProcess(&pLW, QStringList() << "prop" << "set" << "persist.linstack.width" << QString::number(m_desiredWidth));
        pLW.start();
        pLW.waitForFinished(1500);

        QProcess pLH;
        PathResolver::configureProcess(&pLH, QStringList() << "prop" << "set" << "persist.linstack.height" << QString::number(m_desiredHeight));
        pLH.start();
        pLH.waitForFinished(1500);
    }

    // Pre-flight check
    KernelDiagnostic diag = runKernelDiagnostics();
    if (!diag.binderFound) {
        emit errorOccurred(ServiceError::KernelBinderMissing,
                           "Cannot start LinStack session: Linux kernel binder driver is not loaded.",
                           diag.remediationCommands);
        setSessionState(SessionState::Failed);
        return;
    }

    // If session is already active or running, immediately trigger show-full-ui without hanging on session start
    m_wasAlreadyRunning = isSessionRunning();
    if (m_wasAlreadyRunning) {
        LOG_INFO("Session", "Active LinStack container session detected (RUNNING). Immediately triggering show-full-ui on wayland-linstack-0...");
        emit logMessage("Session", "Active session running. Triggering show-full-ui immediately...");
        setSessionState(SessionState::Active);
        launchFullUi();
        return;
    }

    emit logMessage("Session", "Spawning LinStack session (linstack session start)...");
    LOG_INFO("Session", "Spawning LinStack session (linstack session start) on wayland-linstack-0...");
    setSessionState(SessionState::Starting);

    if (m_sessionProcess) {
        if (m_sessionProcess->state() != QProcess::NotRunning) {
            m_sessionProcess->kill();
            m_sessionProcess->waitForFinished(500);
        }
        m_sessionProcess->deleteLater();
    }

    m_sessionProcess = new QProcess(this);
    connect(m_sessionProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LinstackService::handleSessionProcessFinished);
    connect(m_sessionProcess, &QProcess::errorOccurred,
            this, &LinstackService::handleSessionProcessError);
    connect(m_sessionProcess, &QProcess::readyReadStandardOutput,
            this, &LinstackService::handleSessionStdout);
    connect(m_sessionProcess, &QProcess::readyReadStandardError,
            this, &LinstackService::handleSessionStderr);

    PathResolver::configureProcess(m_sessionProcess, QStringList() << "session" << "start");

    // Configure Wayland display for embedded compositor session
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_RUNTIME_DIR", runtimeDir);
    env.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
    env.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
    m_sessionProcess->setProcessEnvironment(env);

    m_sessionProcess->start();

    // Start poll timer to wait for session RUNNING state before launching show-full-ui
    if (!m_sessionPollTimer) {
        m_sessionPollTimer = new QTimer(this);
        connect(m_sessionPollTimer, &QTimer::timeout, this, &LinstackService::checkSessionStartupStatus);
    }
    m_sessionPollCount = 0;
    m_sessionPollTimer->start(1000);
}

void LinstackService::checkSessionStartupStatus()
{
    m_sessionPollCount++;
    if (isSessionRunning()) {
        if (m_sessionPollTimer) m_sessionPollTimer->stop();
        setSessionState(SessionState::Active);
        if (m_desiredWidth > 0 && m_desiredHeight > 0) {
            LOG_INFO("Session", QString("Applying active session resolution: %1x%2").arg(m_desiredWidth).arg(m_desiredHeight));
            QProcess pW; PathResolver::configureProcess(&pW, QStringList() << "prop" << "set" << "persist.waydroid.width" << QString::number(m_desiredWidth)); pW.start(); pW.waitForFinished(1500);
            QProcess pH; PathResolver::configureProcess(&pH, QStringList() << "prop" << "set" << "persist.waydroid.height" << QString::number(m_desiredHeight)); pH.start(); pH.waitForFinished(1500);
        }
        emit logMessage("Session", "LinStack session is confirmed RUNNING. Launching UI...");
        LOG_INFO("Session", "LinStack session is confirmed RUNNING. Launching UI...");
        launchFullUi();
    } else if (m_sessionPollCount >= 30) {
        if (m_sessionPollTimer) m_sessionPollTimer->stop();
        emit logMessage("Session", "Session status poll timed out after 12s. Attempting show-full-ui fallback...");
        LOG_WARN("Session", "Session status poll timed out after 12s. Attempting show-full-ui fallback...");
        launchFullUi();
    }
}

void LinstackService::launchFullUi()
{
    if (m_isStopping) return;
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";

    if (!m_uiProcess) {
        m_uiProcess = new QProcess(this);
        connect(m_uiProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            const QString out = QString::fromUtf8(m_uiProcess->readAllStandardOutput()).trimmed();
            if (!out.isEmpty()) {
                emit logMessage("UI", out);
                LOG_INFO("UI[STDOUT]", out);
            }
        });
        connect(m_uiProcess, &QProcess::readyReadStandardError, this, [this, runtimeDir]() {
            const QString err = QString::fromUtf8(m_uiProcess->readAllStandardError()).trimmed();
            if (!err.isEmpty()) {
                emit logMessage("UI_ERR", err);
                LOG_WARN("UI[STDERR]", err);

                // إذا كانت الجلسة معلقة/تعمل مسبقاً، نُجبر الأندرويد على عرض شاشته عبر إطلاق تطبيق النظام
                if (err.contains("Already tracking a session", Qt::CaseInsensitive)) {
                    LOG_INFO("UI", "Session already tracked -> waking Android display surface via app launch...");
                    QProcess *wakeProc = new QProcess(this);
                    QProcessEnvironment wakeEnv = QProcessEnvironment::systemEnvironment();
                    wakeEnv.insert("XDG_RUNTIME_DIR", runtimeDir);
                    wakeEnv.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
                    wakeEnv.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
                    wakeProc->setProcessEnvironment(wakeEnv);
                    PathResolver::configureProcess(wakeProc, QStringList() << "app" << "launch" << "com.android.settings");
                    connect(wakeProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), wakeProc, &QProcess::deleteLater);
                    wakeProc->start();
                }
            }
        });
        connect(m_uiProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
            Q_UNUSED(status);
            emit logMessage("UI", QString("UI process exited (code %1)").arg(code));
            LOG_INFO("UI", QString("UI process exited (code %1)").arg(code));
        });
    }

    if (m_uiProcess->state() != QProcess::NotRunning) {
        LOG_INFO("UI", "show-full-ui process is already running.");
    } else {
        LOG_INFO("UI", "Spawning linstack show-full-ui with WAYLAND_DISPLAY=wayland-linstack-0...");
        QProcessEnvironment uiEnv = QProcessEnvironment::systemEnvironment();
        uiEnv.insert("XDG_RUNTIME_DIR", runtimeDir);
        uiEnv.insert("WAYLAND_DISPLAY", "wayland-linstack-0");
        uiEnv.insert("LINSTACK_WAYLAND_DISPLAY", "wayland-linstack-0");
        m_uiProcess->setProcessEnvironment(uiEnv);
        PathResolver::configureProcess(m_uiProcess, QStringList() << "show-full-ui");
        m_uiProcess->start();
    }

    // Start 2-second retry timer if not already active and first frame not yet committed
    if (!m_hasFirstFrame) {
        if (!m_uiRetryTimer) {
            m_uiRetryTimer = new QTimer(this);
            connect(m_uiRetryTimer, &QTimer::timeout, this, &LinstackService::retryFullUi);
        }
        if (!m_uiRetryTimer->isActive()) {
            m_uiRetryTimer->start(2000);
        }
    }
}

void LinstackService::retryFullUi()
{
    if (m_hasFirstFrame) {
        m_uiRetryCount = 0;
        if (m_uiRetryTimer) m_uiRetryTimer->stop();
        return;
    }

    if (m_sessionState != SessionState::Active && !isSessionRunning()) {
        return;
    }

    m_uiRetryCount++;
    int maxRetries = m_wasAlreadyRunning ? 3 : 10;
    if (m_uiRetryCount >= maxRetries) {
        // الجلسة السابقة مرتبطة بمقبس Wayland قديم مغلق -> نعيد تشغيل الجلسة للارتباط بالمقبس الجديد
        LOG_WARN("UI", "Stale session detected (no frames after 4 retries). Rebinding session to new Wayland socket...");
        m_uiRetryCount = 0;
        if (m_uiRetryTimer) m_uiRetryTimer->stop();
        stopSession();
        QTimer::singleShot(1500, this, &LinstackService::startSession);
        return;
    }

    if (!m_uiProcess || m_uiProcess->state() == QProcess::NotRunning) {
        LOG_INFO("UI", "Retrying show-full-ui (waiting for first surface frame)...");
        launchFullUi();
    }
}

void LinstackService::stopSession()
{
    m_isStopping = true;
    emit logMessage("Session", "Stopping LinStack session...");
    LOG_INFO("Session", "Stopping LinStack session...");
    setSessionState(SessionState::Stopping);

    if (m_adbPollTimer) {
        m_adbPollTimer->stop();
    }
    if (m_statusPollTimer) {
        m_statusPollTimer->stop();
    }
    if (m_sessionPollTimer) {
        m_sessionPollTimer->stop();
    }
    if (m_uiRetryTimer) {
        m_uiRetryTimer->stop();
    }
    m_hasFirstFrame = false;

    if (m_uiProcess && m_uiProcess->state() != QProcess::NotRunning) {
        m_uiProcess->terminate();
        if (!m_uiProcess->waitForFinished(1000)) {
            m_uiProcess->kill();
            m_uiProcess->waitForFinished(500);
        }
    }

    if (m_sessionProcess && m_sessionProcess->state() != QProcess::NotRunning) {
        m_sessionProcess->terminate();
        if (!m_sessionProcess->waitForFinished(1500)) {
            m_sessionProcess->kill();
            m_sessionProcess->waitForFinished(500);
        }
    }

    // تنفيذ session stop بشكل متزامن لضمان إغلاق جلسة الأندرويد بالكامل قبل خروج البرنامج
    QProcess stopProc;
    PathResolver::configureProcess(&stopProc, QStringList() << "session" << "stop");
    stopProc.start();
    if (!stopProc.waitForFinished(5000)) {
        stopProc.kill();
        stopProc.waitForFinished(500);
    }

    setSessionState(SessionState::Idle);
    emit logMessage("Session", "LinStack session stopped.");
    LOG_INFO("Session", "LinStack session stopped.");
}


void LinstackService::handleSessionProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_isStopping || m_sessionState == SessionState::Stopping) return;
    emit logMessage("Session", QString("LinStack session process exited (code %1).").arg(exitCode));
    LOG_INFO("Session", QString("LinStack session process exited (code %1).").arg(exitCode));
    if (exitCode == 0 || isSessionRunning()) {
        LOG_INFO("Session", "Session process finished normally or session is active -> launching full UI.");
        setSessionState(SessionState::Active);
        launchFullUi();
    } else {
        setSessionState(SessionState::Terminated);
    }
}

void LinstackService::handleSessionProcessError(QProcess::ProcessError error)
{
    emit logMessage("Session", QString("LinStack process error: %1").arg(error));
    LOG_WARN("Session", QString("LinStack process error: %1").arg(error));
    setSessionState(SessionState::Failed);
    emit errorOccurred(ServiceError::LinstackSessionFailed,
                       "LinStack session process encountered an execution error.",
                       QStringList{"linstack log", "systemctl status linstack-container", "linstack session stop"});
}

void LinstackService::handleSessionStdout()
{
    if (!m_sessionProcess) return;
    const QString output = QString::fromUtf8(m_sessionProcess->readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) {
        emit logMessage("LinStack [STDOUT]", output);
        LOG_INFO("Session[STDOUT]", output);
        if (output.contains("Android with user 0 is ready", Qt::CaseInsensitive) ||
            output.contains("Session started", Qt::CaseInsensitive) ||
            output.contains("Session is already running", Qt::CaseInsensitive)) {
            setSessionState(SessionState::Active);
            launchFullUi();
        }
    }
}

void LinstackService::handleSessionStderr()
{
    if (!m_sessionProcess) return;
    const QString err = QString::fromUtf8(m_sessionProcess->readAllStandardError()).trimmed();
    if (!err.isEmpty()) {
        emit logMessage("LinStack [STDERR]", err);
        LOG_WARN("Session[STDERR]", err);
        if (err.contains("Session is already running", Qt::CaseInsensitive) ||
            err.contains("Already tracking a session", Qt::CaseInsensitive)) {
            LOG_INFO("Session", "Session already running reported on stderr -> activating and launching full UI.");
            setSessionState(SessionState::Active);
            launchFullUi();
        }
    }
}

void LinstackService::refreshStatus()
{
    if (m_sessionState == SessionState::Starting || m_sessionState == SessionState::Stopping) return;
    // Check linstack status non-blocking
    auto *statusProc = new QProcess(this);
    connect(statusProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, statusProc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QString out = statusProc->readAllStandardOutput();
            if (out.contains("Session: RUNNING", Qt::CaseInsensitive)) {
                setSessionState(SessionState::Active);
            }
            if (out.contains("Container: RUNNING", Qt::CaseInsensitive)) {
                setContainerState(ContainerState::Running);
            }
        }
        statusProc->deleteLater();
    });
    PathResolver::configureProcess(statusProc, QStringList() << "status");
    statusProc->start();
}

// =========================================================================
// linstack-scripts Integration (Casualsnek/linstack_script)
// =========================================================================

void LinstackService::setScriptsPath(const QString &path)
{
    m_scriptsPath = path;
}

bool LinstackService::isScriptsInstalled() const
{
    return QFileInfo::exists(m_scriptsPath + "/main.py");
}

QString LinstackService::findPythonInterpreter() const
{
    // Check for virtualenv python first
    QString venvPython = m_scriptsPath + "/venv/bin/python3";
    if (QFileInfo::exists(venvPython)) {
        return venvPython;
    }
    return QStandardPaths::findExecutable("python3");
}

void LinstackService::installScriptsEnvironment()
{
    emit scriptOperationStarted("Setup linstack-scripts Environment");
    emit scriptOperationProgress("Setup linstack-scripts Environment", 10, "Cloning Casualsnek/linstack_script...");

    QDir().mkpath(QFileInfo(m_scriptsPath).path());

    auto *cloneProc = new QProcess(this);
    cloneProc->setWorkingDirectory(QFileInfo(m_scriptsPath).path());

    QStringList cloneArgs;
    cloneArgs << "clone" << "https://github.com/Casualsnek/linstack_script" << m_scriptsPath;

    connect(cloneProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, cloneProc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0 || isScriptsInstalled()) {
            emit scriptOperationProgress("Setup linstack-scripts Environment", 50, "Creating Python virtualenv...");

            // Setup venv and requirements
            auto *venvProc = new QProcess(this);
            venvProc->setWorkingDirectory(m_scriptsPath);

            QString command = "python3 -m venv venv && ./venv/bin/pip install -r requirements.txt";
            connect(venvProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, venvProc](int venvExit, QProcess::ExitStatus) {
                if (venvExit == 0) {
                    emit scriptOperationProgress("Setup linstack-scripts Environment", 100, "Ready");
                    emit scriptOperationFinished("Setup linstack-scripts Environment", true, "linstack-scripts configured.");
                } else {
                    emit scriptOperationFinished("Setup linstack-scripts Environment", false, "Failed to install Python requirements.");
                }
                venvProc->deleteLater();
            });
            venvProc->start("/bin/bash", QStringList() << "-c" << command);
        } else {
            emit scriptOperationFinished("Setup linstack-scripts Environment", false, "Failed to git clone linstack_script repo.");
        }
        cloneProc->deleteLater();
    });

    cloneProc->start("git", cloneArgs);
}

void LinstackService::executeScriptCommand(const QString &operationName, const QStringList &scriptArgs)
{
    if (m_scriptProcess && m_scriptProcess->state() != QProcess::NotRunning) {
        emit scriptOperationFinished(operationName, false, "Another script operation is already active.");
        return;
    }

    if (!isScriptsInstalled()) {
        emit errorOccurred(ServiceError::LinstackScriptsNotFound,
                           "linstack-scripts repository not found.",
                           QStringList{QString("git clone https://github.com/Casualsnek/linstack_script %1").arg(m_scriptsPath)});
        emit scriptOperationFinished(operationName, false, "linstack-scripts not installed.");
        return;
    }

    m_activeScriptOp = operationName;
    emit scriptOperationStarted(operationName);
    emit scriptOperationProgress(operationName, 10, "Starting script execution...");

    m_scriptProcess = new QProcess(this);
    m_scriptProcess->setWorkingDirectory(m_scriptsPath);

    QString pythonBin = findPythonInterpreter();
    QStringList fullArgs;
    fullArgs << "main.py" << scriptArgs;

    // Use pkexec or sudo if needed, or invoke directly
    QString executable = pythonBin;

    connect(m_scriptProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        if (!m_scriptProcess) return;
        QString text = QString::fromUtf8(m_scriptProcess->readAllStandardOutput()).trimmed();
        emit logMessage(m_activeScriptOp, text);

        int percent = 30;
        if (text.contains("Downloading", Qt::CaseInsensitive)) percent = 45;
        if (text.contains("Extracting", Qt::CaseInsensitive)) percent = 70;
        if (text.contains("Installing", Qt::CaseInsensitive) || text.contains("Patching", Qt::CaseInsensitive)) percent = 85;
        if (text.contains("Done", Qt::CaseInsensitive)) percent = 98;

        emit scriptOperationProgress(m_activeScriptOp, percent, text.left(60));
    });

    connect(m_scriptProcess, &QProcess::readyReadStandardError, this, [this]() {
        if (!m_scriptProcess) return;
        QString err = QString::fromUtf8(m_scriptProcess->readAllStandardError()).trimmed();
        emit logMessage(m_activeScriptOp + " [Error]", err);
    });

    connect(m_scriptProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        bool success = (exitCode == 0);
        emit scriptOperationProgress(m_activeScriptOp, 100, success ? "Completed" : "Failed");
        emit scriptOperationFinished(m_activeScriptOp, success,
                                     success ? QString("%1 finished successfully.").arg(m_activeScriptOp)
                                             : QString("%1 exited with error code %2.").arg(m_activeScriptOp).arg(exitCode));
        m_scriptProcess->deleteLater();
        m_scriptProcess = nullptr;
    });

    m_scriptProcess->start(executable, fullArgs);
}

void LinstackService::installGApps()
{
    executeScriptCommand("Install Google Play Services (GApps)", QStringList() << "install" << "gapps");
}

void LinstackService::installLibndk()
{
    executeScriptCommand("Install ARM Translation (libndk)", QStringList() << "install" << "libndk");
}

void LinstackService::installLibhoudini()
{
    executeScriptCommand("Install ARM Translation (libhoudini)", QStringList() << "install" << "libhoudini");
}

QList<DeviceProfileInfo> LinstackService::predefinedProfiles() const
{
    return m_profiles.values();
}

void LinstackService::applyDeviceProfileByName(const QString &profileName)
{
    for (const auto &prof : m_profiles) {
        if (prof.displayName.contains(profileName, Qt::CaseInsensitive) ||
            prof.model.compare(profileName, Qt::CaseInsensitive) == 0) {
            applyDeviceProfile(prof);
            return;
        }
    }
    emit logMessage("DeviceProfile", QString("Profile '%1' not found, applying default ROG Phone 5 profile.").arg(profileName));
    applyDeviceProfile(m_profiles.value("ROG Phone 5"));
}

void LinstackService::applyDeviceProfile(const DeviceProfileInfo &profile)
{
    emit scriptOperationStarted(QString("Apply Device Profile: %1").arg(profile.displayName));
    emit logMessage("DeviceProfile", QString("Configuring device fingerprint for %1 (%2 FPS unlocked)").arg(profile.displayName).arg(profile.maxSupportedFps));

    // Execute linstack prop set commands
    struct PropEntry { QString key; QString val; };
    QList<PropEntry> props = {
        {"ro.product.manufacturer", profile.manufacturer},
        {"ro.product.brand", profile.brand},
        {"ro.product.model", profile.model},
        {"ro.product.device", profile.device},
        {"ro.build.fingerprint", profile.fingerprint}
    };

    for (const auto &p : props) {
        PathResolver::startDetachedLinstack(QStringList() << "prop" << "set" << p.key << p.val);
    }

    emit scriptOperationFinished(QString("Apply Device Profile: %1").arg(profile.displayName),
                                 true,
                                 QString("Spoofed device properties to %1. Restart container to apply.").arg(profile.model));
}

// =========================================================================
// ADB Socket Connection Handler & APK Installation (127.0.0.1:5555)
// =========================================================================

void LinstackService::checkAdbConnection()
{
    if (m_adbSocket->state() == QAbstractSocket::UnconnectedState) {
        m_adbSocket->connectToHost("127.0.0.1", 5555);
    }
}

void LinstackService::handleAdbSocketConnected()
{
    if (!m_adbConnected) {
        m_adbConnected = true;
        emit adbConnectionStatusChanged(true);
        emit logMessage("ADB", "Established direct socket connection to LinStack ADB daemon at 127.0.0.1:5555.");

        // Automatically connect adb client to local port
        auto *proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                proc, &QProcess::deleteLater);
        proc->start("adb", QStringList() << "connect" << "127.0.0.1:5555");
    }
    m_adbSocket->disconnectFromHost();
}

void LinstackService::handleAdbSocketError()
{
    if (m_adbConnected) {
        m_adbConnected = false;
        emit adbConnectionStatusChanged(false);
        emit logMessage("ADB", "ADB socket connection to 127.0.0.1:5555 disconnected.");
    }
}

void LinstackService::sendAndroidKeyEvent(int androidKeyCode)
{
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QProcess::deleteLater);

    QString cmd = QString("adb connect 127.0.0.1:5555 >/dev/null 2>&1; adb -s 127.0.0.1:5555 shell input keyevent %1").arg(androidKeyCode);
    proc->start("/bin/sh", QStringList() << "-c" << cmd);
}

void LinstackService::installApk(const QString &apkFilePath)
{
    QFileInfo fi(apkFilePath);
    if (!fi.exists() || !fi.isReadable()) {
        emit apkInstallFinished(apkFilePath, false, "APK file does not exist or cannot be read.");
        return;
    }

    if (!apkFilePath.endsWith(".apk", Qt::CaseInsensitive)) {
        emit apkInstallFinished(apkFilePath, false, "Selected file is not an Android Package (.apk).");
        return;
    }

    emit apkInstallStarted(apkFilePath);
    emit logMessage("ADB Install", QString("Preparing installation for: %1 (%2 MB)")
                    .arg(fi.fileName()).arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1));

    executeAdbInstallProcess(apkFilePath);
}

void LinstackService::executeAdbInstallProcess(const QString &apkPath)
{
    auto *proc = new QProcess(this);
    QString fileName = QFileInfo(apkPath).fileName();

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, apkPath, fileName]() {
        QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        emit logMessage("ADB Stream", text);

        int percent = 30;
        if (text.contains("Performing Streamed Install", Qt::CaseInsensitive)) percent = 50;
        if (text.contains("Success", Qt::CaseInsensitive)) percent = 100;

        // Parse regex for percentage if adb reports transfer progress
        static const QRegularExpression rx("(\\d+)%");
        auto match = rx.match(text);
        if (match.hasMatch()) {
            percent = match.captured(1).toInt();
        }

        emit apkInstallProgress(apkPath, percent, text.left(60));
    });

    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, fileName]() {
        QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        emit logMessage("ADB Error", QString("[%1]: %2").arg(fileName).arg(err));
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, apkPath, fileName](int exitCode, QProcess::ExitStatus) {
        QString fullOutput = proc->readAllStandardOutput() + " " + proc->readAllStandardError();
        bool success = (exitCode == 0) && fullOutput.contains("Success", Qt::CaseInsensitive);

        if (success) {
            emit apkInstallProgress(apkPath, 100, "Installed Successfully");
            emit apkInstallFinished(apkPath, true, QString("Package %1 installed successfully.").arg(fileName));
            emit logMessage("ADB Install", QString("Successfully installed %1 into LinStack container.").arg(fileName));
        } else {
            QString errorMsg = fullOutput.trimmed();
            if (errorMsg.isEmpty()) errorMsg = QString("Process exited with code %1").arg(exitCode);

            emit apkInstallFinished(apkPath, false, errorMsg);
            emit errorOccurred(ServiceError::AdbInstallFailed,
                               QString("Failed to install %1: %2").arg(fileName).arg(errorMsg),
                               QStringList{
                                   "adb connect 127.0.0.1:5555",
                                   "adb devices",
                                   QString("adb install -r -d \"%1\"").arg(apkPath)
                               });
        }
        proc->deleteLater();
    });

    // Invoke adb with destination target 127.0.0.1:5555
    QStringList args;
    args << "-s" << "127.0.0.1:5555" << "install" << "-r" << apkPath;
    proc->start("adb", args);
}
