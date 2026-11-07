#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "AndroidCanvas.h"
#include "KeymapperOverlay.h"
#include "Theme.h"

#include "SelfHealingManager.h"
#include "Logger.h"
#include <QProcess>

int main(int argc, char *argv[])
{
    // Install centralized thread-safe logger and Qt message handler
    Logger::installQtMessageHandler();
    LOG_INFO("Core", "=== LinStack 1.0 (Cyber Emerald Edition) initializing ===");

    // Qt 6 High-DPI configuration
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("LinStack");
    app.setApplicationDisplayName("LinStack 1.0");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LinStack Project");

    // Force single-window mode
    QProcess::execute("linstack", QStringList() << "prop" << "set" << "persist.linstack.multi_windows" << "false");

    bool isHeadless = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]).startsWith("--screenshot")) {
            isHeadless = true;
            break;
        }
    }

    // Self-Healing Binder Check (interactive only when not running in headless/screenshot mode)
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
    }

    return app.exec();
}
