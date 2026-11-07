#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QFile>
#include <QDateTime>

enum class LogLevel {
    Info,
    Warning,
    Fault,
    CriticalFatal
};

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger &instance();

    // Configuration
    void setLogFilePath(const QString &path);
    QString logFilePath() const { return m_logFilePath; }
    void setConsoleEcho(bool echo) { m_consoleEcho = echo; }

    // Core logging API
    void log(LogLevel level, const QString &category, const QString &message, const QString &details = QString());
    void logSubprocess(const QString &command, int exitCode, const QString &stdoutStr, const QString &stderrStr);
    void logFault(const QString &category, const QString &errorCode, const QString &message, const QString &remediationHint = QString());
    void logCritical(const QString &category, const QString &message, bool captureTrace = true);

    // Stack trace acquisition (execinfo backtrace)
    static QString captureStackTrace(int maxFrames = 25);

    // Retrieve recent logs for post-mortem / Recovery Dialog
    QStringList recentLogs(int maxLines = 100) const;
    QString recentLogsText(int maxLines = 50) const;
    void clearRecentLogs();

    // Global Qt message handler integration
    static void installQtMessageHandler();

signals:
    void logAppended(const QString &formattedMessage, LogLevel level);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void writeToFile(const QString &formattedLine);
    void checkRotation();
    static QString levelToString(LogLevel level);

    mutable QMutex m_mutex;
    QString m_logFilePath;
    QFile m_logFile;
    QStringList m_recentBuffer;
    const int m_maxBufferSize = 250;
    const qint64 m_maxFileSize = 5 * 1024 * 1024; // 5 MB rotation
    bool m_consoleEcho = true;
};

// Convenience logging macros
#define LOG_INFO(category, msg) \
    Logger::instance().log(LogLevel::Info, category, msg)

#define LOG_WARN(category, msg) \
    Logger::instance().log(LogLevel::Warning, category, msg)

#define LOG_FAULT(category, code, msg, hint) \
    Logger::instance().logFault(category, code, msg, hint)

#define LOG_CRITICAL(category, msg) \
    Logger::instance().logCritical(category, msg, true)

#endif // LOGGER_H
