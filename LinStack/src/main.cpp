#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QSurfaceFormat>
#include "MainWindow.h"
#include "SettingsDialog.h"
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

    bool isHeadless = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]).startsWith("--screenshot")) {
            isHeadless = true;
            break;
        }
    }

    // Fast non-session pre-flight diagnostic check (kernel binder / modules)
    // Note: session is NOT started here; MainWindow initializes the embedded compositor first
    SelfHealingManager healer;
    if (!isHeadless && !healer.checkHealth()) {
        if (!healer.promptRepair()) {
            return 1;
        }
    }

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

    MainWindow window;
    window.show();

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
