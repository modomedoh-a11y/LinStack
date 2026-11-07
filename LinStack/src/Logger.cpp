#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QThread>
#include <iostream>
#include <execinfo.h>
#include <cstdlib>

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
    QString defaultDir = QDir::homePath() + "/.local/state/LinStack";
    QDir().mkpath(defaultDir);
    setLogFilePath(defaultDir + "/linstack.log");
}

Logger::~Logger()
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::setLogFilePath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    m_logFilePath = path;
    QFileInfo fi(m_logFilePath);
    QDir().mkpath(fi.absolutePath());

    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Fallback to /tmp if target directory is read-only or restricted
        QString fallbackPath = QString("/tmp/linstack_%1.log").arg(qgetenv("USER").isEmpty() ? "runtime" : QString::fromUtf8(qgetenv("USER")));
        m_logFilePath = fallbackPath;
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            std::cerr << "[Logger] Failed to open fallback log file: " << fallbackPath.toStdString() << std::endl;
        }
    }
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:          return "INFO";
    case LogLevel::Warning:       return "WARNING";
    case LogLevel::Fault:         return "FAULT";
    case LogLevel::CriticalFatal: return "CRITICAL_FATAL";
    }
    return "UNKNOWN";
}

void Logger::checkRotation()
{
    if (m_logFile.isOpen() && m_logFile.size() >= m_maxFileSize) {
        m_logFile.close();
        QString oldPath = m_logFilePath + ".old";
        QFile::remove(oldPath);
        QFile::rename(m_logFilePath, oldPath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            std::cerr << "[Logger] Failed to reopen log file after rotation: " << m_logFilePath.toStdString() << std::endl;
        }
    }
}

void Logger::writeToFile(const QString &formattedLine)
{
    checkRotation();
    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << formattedLine << "\n";
        stream.flush();
    }
}

void Logger::log(LogLevel level, const QString &category, const QString &message, const QString &details)
{
    QMutexLocker locker(&m_mutex);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString threadId = QString("0x%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    QString levelStr = levelToString(level);

    QString line = QString("[%1] [Thread %2] [%3] [%4] %5")
                       .arg(timestamp, threadId, levelStr, category, message);

    if (!details.isEmpty()) {
        line += QString(" | Details: %1").arg(details);
    }

    writeToFile(line);

    m_recentBuffer.append(line);
    while (m_recentBuffer.size() > m_maxBufferSize) {
        m_recentBuffer.removeFirst();
    }

    if (m_consoleEcho) {
        if (level == LogLevel::CriticalFatal || level == LogLevel::Fault) {
            std::cerr << line.toStdString() << std::endl;
        } else {
            std::cout << line.toStdString() << std::endl;
        }
    }

    emit logAppended(line, level);
}

void Logger::logFault(const QString &category, const QString &errorCode, const QString &message, const QString &remediationHint)
{
    QString detailedMsg = QString("[%1] %2").arg(errorCode, message);
    if (!remediationHint.isEmpty()) {
        detailedMsg += QString(" (Remediation: %1)").arg(remediationHint);
    }
    log(LogLevel::Fault, category, detailedMsg);
}

void Logger::logCritical(const QString &category, const QString &message, bool captureTrace)
{
    QString fullMsg = message;
    if (captureTrace) {
        QString trace = captureStackTrace(20);
        fullMsg += QString("\n--- Stack Trace ---\n%1").arg(trace);
    }
    log(LogLevel::CriticalFatal, category, fullMsg);
}

void Logger::logSubprocess(const QString &command, int exitCode, const QString &stdoutStr, const QString &stderrStr)
{
    LogLevel lvl = (exitCode == 0) ? LogLevel::Info : LogLevel::Fault;
    QString summary = QString("Process '%1' exited with code %2").arg(command).arg(exitCode);

    QString details;
    if (!stdoutStr.trimmed().isEmpty()) {
        details += QString("STDOUT: %1").arg(stdoutStr.trimmed().left(500));
    }
    if (!stderrStr.trimmed().isEmpty()) {
        if (!details.isEmpty()) details += " | ";
        details += QString("STDERR: %1").arg(stderrStr.trimmed().left(500));
    }

    log(lvl, "Subprocess", summary, details);
}

QString Logger::captureStackTrace(int maxFrames)
{
    void *buffer[128];
    int frames = backtrace(buffer, qMin(maxFrames, 128));
    char **symbols = backtrace_symbols(buffer, frames);

    if (!symbols) {
        return "Unable to resolve stack symbols";
    }

    QStringList traceLines;
    for (int i = 0; i < frames; ++i) {
        traceLines.append(QString("#%1 %2").arg(i, 2, 10, QChar('0')).arg(symbols[i]));
    }
    free(symbols);

    return traceLines.join("\n");
}

QStringList Logger::recentLogs(int maxLines) const
{
    QMutexLocker locker(&m_mutex);
    if (maxLines <= 0 || maxLines >= m_recentBuffer.size()) {
        return m_recentBuffer;
    }
    return m_recentBuffer.mid(m_recentBuffer.size() - maxLines);
}

QString Logger::recentLogsText(int maxLines) const
{
    return recentLogs(maxLines).join("\n");
}

void Logger::clearRecentLogs()
{
    QMutexLocker locker(&m_mutex);
    m_recentBuffer.clear();
}

static void qtMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString category = context.category ? QString::fromUtf8(context.category) : "Qt";
    LogLevel level = LogLevel::Info;

    switch (type) {
    case QtDebugMsg:
        level = LogLevel::Info;
        break;
    case QtInfoMsg:
        level = LogLevel::Info;
        break;
    case QtWarningMsg:
        level = LogLevel::Warning;
        break;
    case QtCriticalMsg:
        level = LogLevel::Fault;
        break;
    case QtFatalMsg:
        level = LogLevel::CriticalFatal;
        break;
    }

    Logger::instance().log(level, category, msg);
}

void Logger::installQtMessageHandler()
{
    qInstallMessageHandler(qtMessageOutput);
}
