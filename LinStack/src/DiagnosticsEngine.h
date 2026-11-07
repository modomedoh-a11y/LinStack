#ifndef DIAGNOSTICSENGINE_H
#define DIAGNOSTICSENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QThread>

enum class CheckStatus {
    Passed,
    Warning,
    Failed
};

enum class DiagnosticCategory {
    KernelModule,
    ServiceSocket,
    ContainerRootfs,
    DisplaySurface
};

struct DiagnosticCheckResult {
    QString id;
    DiagnosticCategory category;
    QString name;
    CheckStatus status = CheckStatus::Passed;
    QString details;
    QString errorCode;
    QString remediationHint;
    bool canAutoHeal = false;
    bool requiresElevation = false;
};

struct HealthReport {
    QDateTime timestamp;
    bool isHealthy = true;
    bool hasWarnings = false;
    bool hasCriticalFailures = false;
    QList<DiagnosticCheckResult> checks;
    QString summary;

    QList<DiagnosticCheckResult> failedChecks() const;
    QList<DiagnosticCheckResult> warningChecks() const;
    QList<DiagnosticCheckResult> autoHealableChecks() const;
    QString formatReport() const;
};

class DiagnosticsWorker : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticsWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void performChecks();

signals:
    void progressUpdated(int percent, const QString &checkName);
    void checkCompleted(const DiagnosticCheckResult &result);
    void diagnosticsFinished(const HealthReport &report);
};

class DiagnosticsEngine : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticsEngine(QObject *parent = nullptr);
    ~DiagnosticsEngine() override;

    // Asynchronous diagnostic execution (runs in background QThread)
    void runDiagnosticsAsync();
    bool isRunning() const { return m_isRunning; }

    // Synchronous diagnostic execution (blocking, for CLI or pre-flight)
    HealthReport runDiagnosticsSync();

    // Cache of the latest report
    HealthReport latestReport() const { return m_latestReport; }

signals:
    void progressUpdated(int percent, const QString &checkName);
    void checkCompleted(const DiagnosticCheckResult &result);
    void diagnosticsFinished(const HealthReport &report);

private slots:
    void handleDiagnosticsFinished(const HealthReport &report);

private:
    bool m_isRunning = false;
    HealthReport m_latestReport;
    QThread *m_workerThread = nullptr;
    DiagnosticsWorker *m_worker = nullptr;
};

#endif // DIAGNOSTICSENGINE_H
