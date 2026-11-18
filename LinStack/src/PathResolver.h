#ifndef PATHRESOLVER_H
#define PATHRESOLVER_H

#include <QString>
#include <QStringList>
#include <QProcess>

class PathResolver
{
public:
    // Resolves a resource file path (e.g., "system.img", "vendor.img", or "resources/system.img").
    // Priority order:
    //   1. Relative to executable / project root (appDirPath, appDirPath/.., etc.)
    //   2. Relative to current working directory
    //   3. Standard system installation directories (/usr/share/linstack/resources, etc.)
    static QString resolveResource(const QString &fileName);

    // Returns the directory where system.img / vendor.img reside
    static QString resolveResourcesDir();

    // Resolves the linstack executable or script path:
    // Priority order:
    //   1. Local project/portable script: LinStack_core/linstack.py, ../LinStack_core/linstack.py, ./linstack
    //   2. Standard system install: /usr/bin/linstack, /usr/local/bin/linstack, PATH
    static QString resolveLinstackExecutable();

    // Resolves python3 executable
    static QString resolvePython3();

    // Resolves command & arguments for executing linstack (handles python3 invocation for .py scripts)
    static QString resolveLinstackCommand(QStringList &args);

    // Configures a QProcess instance to run linstack with proper executable, args, and PYTHONPATH
    static void configureProcess(QProcess *proc, const QStringList &args);

    // Helper to synchronously execute linstack
    static int executeLinstack(const QStringList &args, int timeoutMs = 1500);

    // Helper to start detached linstack
    static bool startDetachedLinstack(const QStringList &args);

    // Resolves tools directory
    static QString resolveToolsDir();

    // Resolves core source directory (where LinStack_core/Makefile lives)
    static QString resolveCoreDir();

    // Standard XDG directories
    static QString userDataDir();
    static QString userStateDir();
};

#endif // PATHRESOLVER_H
