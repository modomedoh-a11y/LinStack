#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QSurfaceFormat>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "InitializationDialog.h"
#include "AndroidCanvas.h"
#include "KeymapperOverlay.h"
#include "Theme.h"

#include "SelfHealingManager.h"
#include "Logger.h"
#include "PathResolver.h"
#include <QProcess>

int main(int argc, char *argv[])
{
    // Install centralized thread-safe logger and Qt message handler
    Logger::installQtMessageHandler();
    LOG_INFO("Core", "=== LinStack 1.0 (Cyber Emerald Edition) initializing ===");

    // Configure hardware integration for embedded Wayland compositor & Mesa DRI3 DMA-BUF buffer sharing
    qputenv("QSG_RHI_BACKEND", "opengl");
    qputenv("QT_WAYLAND_HARDWARE_INTEGRATION", "wayland-egl");
    qputenv("QT_WAYLAND_SERVER_BUFFER_INTEGRATION", "dmabuf-server");
    qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "wayland-egl");

    // Qt 6 High-DPI and GLES2 configuration
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QSurfaceFormat glFormat;
    glFormat.setRenderableType(QSurfaceFormat::OpenGLES);
    glFormat.setVersion(2, 0);
    glFormat.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(glFormat);

    QApplication app(argc, argv);
    app.setApplicationName("LinStack");
    app.setApplicationDisplayName("LinStack 1.0");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LinStack Project");

    bool isScreenshotMode = false;
    QString launchPackage;
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg.startsWith("--screenshot")) {
            isScreenshotMode = true;
        } else if (arg == "--launch" && i + 1 < argc) {
            launchPackage = argv[i + 1];
        }
    }

    if (!isScreenshotMode) {
        QString serverName = "LinStack_IPC_" + QString::fromLocal8Bit(qgetenv("USER"));
        if (serverName == "LinStack_IPC_") {
            serverName = "LinStack_IPC_default";
        }

        QLocalSocket clientSocket;
        clientSocket.connectToServer(serverName);
        if (clientSocket.waitForConnected(400)) {
            LOG_INFO("Core", "LinStack is already running. Forwarding launch command via IPC...");
            QString msg = launchPackage.isEmpty() ? "ACTIVATE" : ("LAUNCH:" + launchPackage);
            clientSocket.write(msg.toUtf8() + "\n");
            clientSocket.waitForBytesWritten(1000);
            clientSocket.disconnectFromServer();
            return 0;
        }

        QLocalServer::removeServer(serverName);
    }

    // Fast non-session pre-flight diagnostic check (kernel binder / modules)
    // Note: session is NOT started here; MainWindow initializes the embedded compositor first
    // الفحص التشخيصي يعمل في الخلفية من MainWindow لفتح الواجهة فوراً دون تأخير

    // Apply dark cyber palette
    QPalette cyberPalette;
    cyberPalette.setColor(QPalette::Window, Theme::ColorBgDark);
    cyberPalette.setColor(QPalette::WindowText, Theme::ColorTextPrimary);
    cyberPalette.setColor(QPalette::Base, Theme::ColorBgDarkAlt);
    cyberPalette.setColor(QPalette::AlternateBase, Theme::ColorBgSurface);
    cyberPalette.setColor(QPalette::ToolTipBase, Theme::ColorBgPanel);
    cyberPalette.setColor(QPalette::ToolTipText, Theme::ColorTextPrimary);
    cyberPalette.setColor(QPalette::Text, Theme::ColorTextPrimary);
    cyberPalette.setColor(QPalette::Button, Theme::ColorBgSurface);
    cyberPalette.setColor(QPalette::ButtonText, Theme::ColorTextPrimary);
    cyberPalette.setColor(QPalette::Highlight, Theme::ColorEmerald);
    cyberPalette.setColor(QPalette::HighlightedText, Theme::ColorBgDark);
    cyberPalette.setColor(QPalette::PlaceholderText, Theme::ColorTextDim);
    app.setPalette(cyberPalette);

    // Apply global stylesheet
    app.setStyleSheet(Theme::cyberEmeraldStyleSheet());

    // Support CLI screenshot flags for headless validation and standalone dialog preview
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--screenshot-init" && i + 1 < argc) {
            QString path = argv[i + 1];
            InitializationDialog dlg;
            dlg.show();
            app.processEvents();
            QPixmap pix = dlg.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-settings" && i + 1 < argc) {
            QString path = argv[i + 1];
            SettingsDialog dlg;
            dlg.show();
            app.processEvents();
            QPixmap pix = dlg.grab();
            pix.save(path);
            return 0;
        }
    }

    MainWindow window;

    if (!isScreenshotMode) {
        QString serverName = "LinStack_IPC_" + QString::fromLocal8Bit(qgetenv("USER"));
        if (serverName == "LinStack_IPC_") {
            serverName = "LinStack_IPC_default";
        }
        auto *ipcServer = new QLocalServer(&app);
        if (ipcServer->listen(serverName)) {
            QObject::connect(ipcServer, &QLocalServer::newConnection, [ipcServer, &window]() {
                while (QLocalSocket *client = ipcServer->nextPendingConnection()) {
                    QObject::connect(client, &QLocalSocket::readyRead, [client, &window]() {
                        QByteArray data = client->readAll();
                        QString msg = QString::fromUtf8(data).trimmed();
                        if (msg.startsWith("LAUNCH:")) {
                            QString pkg = msg.mid(7).trimmed();
                            LOG_INFO("Core", "IPC launch requested for package: " + pkg);
                            window.bringToFront();
                            window.launchApp(pkg);
                        } else if (msg == "ACTIVATE") {
                            window.bringToFront();
                        }
                    });
                }
            });
        }
    }

    window.show();

    if (!launchPackage.isEmpty()) {
        QTimer::singleShot(400, [&window, launchPackage]() {
            window.launchApp(launchPackage);
        });
    }

    // Support CLI screenshot flags for headless validation and UI preview
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--screenshot" && i + 1 < argc) {
            QString path = argv[i + 1];
            app.processEvents();
            window.resize(1280, 768);
            window.show();
            app.processEvents();
            QPixmap pix = window.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-keymapper" && i + 1 < argc) {
            QString path = argv[i + 1];
            window.resize(1280, 768);
            window.show();
            // trigger keymapper overlay
            QMetaObject::invokeMethod(&window, "handleKeymapperToggle", Q_ARG(bool, true));
            app.processEvents();
            QPixmap pix = window.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-keymapper-edit" && i + 1 < argc) {
            QString path = argv[i + 1];
            window.resize(1280, 768);
            window.show();
            // trigger keymapper overlay in Edit Mode
            QMetaObject::invokeMethod(&window, "handleKeymapperToggle", Q_ARG(bool, true));
            auto *canvas = window.findChild<AndroidCanvas*>();
            if (canvas && canvas->keymapperOverlay()) {
                canvas->keymapperOverlay()->setMode(KeymapperOverlay::Mode::Edit);
            }
            app.processEvents();
            QPixmap pix = window.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-init" && i + 1 < argc) {
            QString path = argv[i + 1];
            InitializationDialog dlg;
            dlg.show();
            app.processEvents();
            QPixmap pix = dlg.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-settings" && i + 1 < argc) {
            QString path = argv[i + 1];
            SettingsDialog dlg;
            dlg.show();
            app.processEvents();
            QPixmap pix = dlg.grab();
            pix.save(path);
            return 0;
        }
        if (QString(argv[i]) == "--screenshot-diagnostics" && i + 1 < argc) {
            QString path = argv[i + 1];
            RecoveryDialog dlg;
            HealthReport report;
            report.isHealthy = false;
            report.hasCriticalFailures = true;
            report.summary = "Subsystem fault detected: Binder IPC interface missing / unreachable.";

            DiagnosticCheckResult binderCheck;
            binderCheck.id = "kernel_binder";
            binderCheck.category = DiagnosticCategory::KernelModule;
            binderCheck.name = "Kernel Binder Interface";
            binderCheck.status = CheckStatus::Failed;
            binderCheck.errorCode = "E_BINDER_MISSING";
            binderCheck.details = "Neither /dev/binder nor /dev/binderfs/binder were found with valid rw permissions.";
            binderCheck.remediationHint = "Run 'modprobe binder_linux' or allow linStack elevated repair to mount binderfs.";
            report.checks.append(binderCheck);

            DiagnosticCheckResult serviceCheck;
            serviceCheck.id = "systemd_service";
            serviceCheck.category = DiagnosticCategory::ServiceSocket;
            serviceCheck.name = "LinStack Systemd Container Service";
            serviceCheck.status = CheckStatus::Warning;
            serviceCheck.errorCode = "W_SERVICE_INACTIVE";
            serviceCheck.details = "linstack-container.service unit is currently inactive.";
            serviceCheck.remediationHint = "Attempt automated service restart via systemctl.";
            report.checks.append(serviceCheck);

            dlg.setReport(report);
            dlg.show();
            app.processEvents();
            QPixmap pix = dlg.grab();
            pix.save(path);
            return 0;
        }
    }

    return app.exec();
}
