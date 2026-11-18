#include "PathResolver.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>

#ifndef LIN_STACK_INSTALL_PREFIX
#define LIN_STACK_INSTALL_PREFIX "/usr"
#endif

#ifndef LIN_STACK_DATADIR
#define LIN_STACK_DATADIR LIN_STACK_INSTALL_PREFIX "/share/linstack"
#endif

QString PathResolver::resolveResourcesDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        // 1. Relative to executable / project root (local development & portable builds)
        appDir + "/resources",
        appDir + "/../resources",
        appDir + "/../../resources",
        appDir + "/../LinStack/resources",
        appDir + "/../../LinStack/resources",
        QDir::currentPath() + "/resources",
        QDir::currentPath() + "/../resources",
        // 2. Standard system installation directories
        QString(LIN_STACK_DATADIR) + "/resources",
        "/usr/share/linstack/resources",
        "/usr/local/share/linstack/resources",
        "/var/lib/linstack/images",
        "/usr/share/linstack-extra/images",
        "/etc/linstack-extra/images"
    };

    for (const QString &dir : candidates) {
        QFileInfo fi(dir);
        if (fi.exists() && fi.isDir()) {
            // Prefer directory that actually contains system.img if present
            if (QFileInfo::exists(dir + "/system.img")) {
                return QDir::cleanPath(dir);
            }
        }
    }

    // Fallback: return the first existing candidate directory
    for (const QString &dir : candidates) {
        if (QFileInfo::exists(dir) && QFileInfo(dir).isDir()) {
            return QDir::cleanPath(dir);
        }
    }
    return QDir::cleanPath(appDir + "/resources");
}

QString PathResolver::resolveResource(const QString &fileName)
{
    if (QFileInfo::exists(fileName) && QFileInfo(fileName).isAbsolute()) {
        return QDir::cleanPath(fileName);
    }

    QString cleanName = fileName;
    if (cleanName.startsWith("resources/")) {
        cleanName = cleanName.mid(10);
    }

    QString dir = resolveResourcesDir();
    QString fullPath = dir + "/" + cleanName;
    if (QFileInfo::exists(fullPath)) {
        return QDir::cleanPath(fullPath);
    }

    // Also check direct candidates relative to app and cwd
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList directCandidates = {
        appDir + "/" + fileName,
        appDir + "/../" + fileName,
        appDir + "/resources/" + cleanName,
        appDir + "/../resources/" + cleanName,
        QDir::currentPath() + "/" + fileName,
        QDir::currentPath() + "/resources/" + cleanName,
        QString(LIN_STACK_DATADIR) + "/resources/" + cleanName,
        "/usr/share/linstack/resources/" + cleanName,
        "/usr/local/share/linstack/resources/" + cleanName
    };

    for (const QString &path : directCandidates) {
        if (QFileInfo::exists(path)) {
            return QDir::cleanPath(path);
        }
    }

    return fullPath;
}

QString PathResolver::resolvePython3()
{
    QString py = QStandardPaths::findExecutable("python3");
    if (!py.isEmpty()) {
        return py;
    }
    return "python3";
}

QString PathResolver::resolveLinstackExecutable()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidatePaths = {
        // 1. Local project / portable builds (LinStack_core/linstack.py or wrapper)
        appDir + "/linstack",
        appDir + "/../linstack",
        appDir + "/LinStack_core/linstack.py",
        appDir + "/../LinStack_core/linstack.py",
        appDir + "/../../LinStack_core/linstack.py",
        appDir + "/../linstack.py",
        QDir::currentPath() + "/linstack",
        QDir::currentPath() + "/LinStack_core/linstack.py",
        QDir::currentPath() + "/../LinStack_core/linstack.py",
        // 2. Standard system installation directories
        QString(LIN_STACK_INSTALL_PREFIX) + "/bin/linstack",
        "/usr/local/bin/linstack",
        "/usr/bin/linstack"
    };

    for (const QString &p : candidatePaths) {
        if (QFileInfo::exists(p)) {
            return QDir::cleanPath(p);
        }
    }

    QString systemExe = QStandardPaths::findExecutable("linstack");
    if (!systemExe.isEmpty()) {
        return systemExe;
    }

    return "linstack";
}

QString PathResolver::resolveToolsDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/tools",
        appDir + "/../tools",
        appDir + "/LinStack_core/tools",
        appDir + "/../LinStack_core/tools",
        appDir + "/../../LinStack_core/tools",
        QDir::currentPath() + "/tools",
        QDir::currentPath() + "/LinStack_core/tools",
        QString(LIN_STACK_DATADIR) + "/tools",
        "/usr/lib/linstack/tools",
        "/usr/share/linstack/tools"
    };

    for (const QString &d : candidates) {
        if (QFileInfo::exists(d) && QFileInfo(d).isDir()) {
            return QDir::cleanPath(d);
        }
    }
    return QString();
}

QString PathResolver::resolveCoreDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/LinStack_core",
        appDir + "/../LinStack_core",
        appDir + "/../../LinStack_core",
        QDir::currentPath() + "/LinStack_core",
        QDir::currentPath() + "/../LinStack_core",
    };
    for (const QString &d : candidates) {
        if (QFileInfo::exists(d + "/Makefile") || QFileInfo::exists(d + "/linstack.py")) {
            return QDir::cleanPath(d);
        }
    }
    return QString();
}

QString PathResolver::resolveLinstackCommand(QStringList &args)
{
    QString exe = resolveLinstackExecutable();
    if (exe.endsWith(".py")) {
        args.prepend(exe);
        return resolvePython3();
    }
    return exe;
}

void PathResolver::configureProcess(QProcess *proc, const QStringList &args)
{
    if (!proc) return;

    QStringList effectiveArgs = args;
    QString prog = resolveLinstackCommand(effectiveArgs);
    proc->setProgram(prog);
    proc->setArguments(effectiveArgs);

    // Setup process environment with PYTHONPATH and dynamic LINSTACK_IMAGES_PATH
    QProcessEnvironment env = proc->processEnvironment();
    if (env.isEmpty()) {
        env = QProcessEnvironment::systemEnvironment();
    }

    QString resDir = resolveResourcesDir();
    if (!resDir.isEmpty()) {
        env.insert("LINSTACK_IMAGES_PATH", resDir);
    }

    QString exe = resolveLinstackExecutable();
    QString toolsParent;
    if (exe.endsWith(".py")) {
        toolsParent = QFileInfo(exe).absolutePath();
    } else {
        QString toolsDir = resolveToolsDir();
        if (!toolsDir.isEmpty()) {
            toolsParent = QFileInfo(toolsDir).absolutePath();
        }
    }

    if (!toolsParent.isEmpty()) {
        QString cur = env.value("PYTHONPATH");
        if (!cur.isEmpty()) {
            env.insert("PYTHONPATH", toolsParent + ":" + cur);
        } else {
            env.insert("PYTHONPATH", toolsParent);
        }
    }
    proc->setProcessEnvironment(env);
}

int PathResolver::executeLinstack(const QStringList &args, int timeoutMs)
{
    QProcess proc;
    configureProcess(&proc, args);
    proc.start();
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(200);
        return -1;
    }
    return proc.exitCode();
}

bool PathResolver::startDetachedLinstack(const QStringList &args)
{
    QStringList effectiveArgs = args;
    QString prog = resolveLinstackCommand(effectiveArgs);
    return QProcess::startDetached(prog, effectiveArgs);
}

QString PathResolver::userDataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.local/share/linStack";
    }
    QDir().mkpath(dir);
    return dir;
}

QString PathResolver::userStateDir()
{
    QString stateDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                           .replace("/share", "/state") + "/linStack";
    if (!QDir().mkpath(stateDir)) {
        stateDir = QDir::homePath() + "/.local/state/linStack";
        QDir().mkpath(stateDir);
    }
    return stateDir;
}
