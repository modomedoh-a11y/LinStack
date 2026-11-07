#ifndef LINSTACKSERVICE_H
#define LINSTACKSERVICE_H

#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QMap>

/**
 * @brief Diagnostic report for Linux kernel and subsystem readiness for Linstack.
 */
struct KernelDiagnostic {
    bool binderFound = false;
    bool binderfsMounted = false;
    bool vndbinderFound = false;
    bool hwbinderFound = false;
    bool ashmemOrMemfdFound = false;
    bool systemdActive = false;
    bool linstackInstalled = false;
    bool adbInstalled = false;

    QString summary;
    QStringList issues;
    QStringList remediationCommands;

    bool isReady() const {
        return (binderFound || binderfsMounted) && linstackInstalled;
    }
};

/**
 * @brief Predefined device spoofing profile for unlocking 90/120/144 FPS in games.
 */
struct DeviceProfileInfo {
    QString displayName;
    QString manufacturer;
    QString brand;
    QString model;
    QString device;
    QString fingerprint;
    int maxSupportedFps = 120;
};

/**
 * @brief High-performance background service managing Linstack, linstack-scripts, and ADB socket integration.
 */
class LinstackService : public QObject
{
    Q_OBJECT

public:
    enum class ContainerState {
        Stopped,
        Starting,
        Running,
        Stopping,
        Failed
    };
    Q_ENUM(ContainerState)

    enum class SessionState {
        Idle,
        Starting,
        Active,
        Stopping,
        Terminated,
        Failed
    };
    Q_ENUM(SessionState)

    enum class ServiceError {
        NoError,
        KernelBinderMissing,
        BinderfsNotMounted,
        SystemdServiceFailed,
        LinstackSessionFailed,
        LinstackScriptsNotFound,
        PythonScriptExecutionFailed,
        AdbConnectionRefused,
        AdbInstallFailed,
        PermissionDenied
    };
    Q_ENUM(ServiceError)

    explicit LinstackService(QObject *parent = nullptr);
    ~LinstackService() override;

    // --- Pre-flight Kernel & Environment Diagnostics ---
    KernelDiagnostic runKernelDiagnostics();

    // --- Container Lifecycle (systemctl / linstack container) ---
    void startContainerService();
    void stopContainerService();
    void restartContainerService();
    ContainerState containerState() const { return m_containerState; }

    // --- Session Lifecycle (linstack session start/stop) ---
    void startSession();
    void stopSession();
    SessionState sessionState() const { return m_sessionState; }

    // --- linstack-scripts Integration (Casualsnek/linstack_script) ---
    void setScriptsPath(const QString &path);
    QString scriptsPath() const { return m_scriptsPath; }
    bool isScriptsInstalled() const;
    void installScriptsEnvironment();

    void installGApps();
    void installLibndk();
    void installLibhoudini();

    QList<DeviceProfileInfo> predefinedProfiles() const;
    void applyDeviceProfile(const DeviceProfileInfo &profile);
    void applyDeviceProfileByName(const QString &profileName);

    // --- ADB Socket Connection & APK Installation (127.0.0.1:5555) ---
    bool isAdbConnected() const { return m_adbConnected; }
    void checkAdbConnection();
    void installApk(const QString &apkFilePath);

    // --- Status Query ---
    void refreshStatus();

signals:
    // Status signals
    void containerStateChanged(LinstackService::ContainerState state);
    void sessionStateChanged(LinstackService::SessionState state);
    void adbConnectionStatusChanged(bool connected);

    // Error & Diagnostics
    void errorOccurred(LinstackService::ServiceError error, const QString &message, const QStringList &remediation);
    void diagnosticFinished(const KernelDiagnostic &diagnostic);

    // Logging & Console Output
    void logMessage(const QString &category, const QString &message);

    // Script Operations
    void scriptOperationStarted(const QString &operation);
    void scriptOperationProgress(const QString &operation, int percent, const QString &statusText);
    void scriptOperationFinished(const QString &operation, bool success, const QString &details);

    // APK Installation
    void apkInstallStarted(const QString &apkPath);
    void apkInstallProgress(const QString &apkPath, int percent, const QString &status);
    void apkInstallFinished(const QString &apkPath, bool success, const QString &message);

private slots:
    void handleSessionProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleSessionProcessError(QProcess::ProcessError error);
    void handleSessionStdout();
    void handleSessionStderr();

    void handleAdbSocketConnected();
    void handleAdbSocketError();

private:
    void setContainerState(ContainerState state);
    void setSessionState(SessionState state);
    void executeScriptCommand(const QString &operationName, const QStringList &scriptArgs);
    void executeAdbInstallProcess(const QString &apkPath);
    QString findPythonInterpreter() const;

    ContainerState m_containerState = ContainerState::Stopped;
    SessionState m_sessionState = SessionState::Idle;

    // Active long-running session process
    QProcess *m_sessionProcess = nullptr;

    // Active script operation process
    QProcess *m_scriptProcess = nullptr;
    QString m_activeScriptOp;

    // ADB Socket client & poller
    QTcpSocket *m_adbSocket = nullptr;
    QTimer *m_adbPollTimer = nullptr;
    QTimer *m_statusPollTimer = nullptr;
    bool m_adbConnected = false;

    // Active APK install processes queue
    struct ApkInstallJob {
        QString apkPath;
        QProcess *process;
    };
    QList<ApkInstallJob> m_activeInstalls;

    QString m_scriptsPath;
    QMap<QString, DeviceProfileInfo> m_profiles;
};

#endif // LINSTACKSERVICE_H
