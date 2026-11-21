#include "EmbeddedCompositor.h"
#include "WaylandSurfaceRenderer.h"
#include "InputMapper.h"
#include "AspectRatioWindow.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

static std::atomic<bool> g_running{true};

static void sigHandler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char *argv[]) {
    std::cout << "========================================================" << std::endl;
    std::cout << "  LinStack Embedded wlroots Compositor Engine v1.0      " << std::endl;
    std::cout << "  Native Waydroid / Android Container Integration       " << std::endl;
    std::cout << "========================================================" << std::endl;

    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);

    // 1. Configure Compositor with dedicated socket and resolution
    EmbeddedCompositorConfig config;
    config.socketName = "wayland-linstack-0";
    config.defaultWidth = 1920;
    config.defaultHeight = 1080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--socket" && i + 1 < argc) {
            config.socketName = argv[++i];
        } else if (arg == "--res" && i + 1 < argc) {
            std::string resStr = argv[++i];
            size_t xPos = resStr.find('x');
            if (xPos != std::string::npos) {
                config.defaultWidth = std::stoi(resStr.substr(0, xPos));
                config.defaultHeight = std::stoi(resStr.substr(xPos + 1));
            }
        }
    }

    EmbeddedCompositor compositor(config);

    if (!compositor.initialize()) {
        std::cerr << "[Main] Fatal: Failed to initialize embedded compositor." << std::endl;
        return 1;
    }

    if (!compositor.start()) {
        std::cerr << "[Main] Fatal: Failed to start wlroots backend." << std::endl;
        return 1;
    }

    // Bind EGL context to current thread
    compositor.makeCurrent();

    // 2. Initialize Hardware-Accelerated Zero-Copy Renderer
    WaylandSurfaceRenderer renderer;
    // Renderer GLES2 shader compilation
    renderer.initialize();

    // 3. Aspect Ratio and Window geometry manager
    AspectRatioWindow window(config.defaultWidth, config.defaultHeight);
    int currentWinWidth = 1280;
    int currentWinHeight = 720;
    ViewportRect viewport = window.computeViewport(currentWinWidth, currentWinHeight);

    // 4. Native Multi-Touch Input Engine
    InputMapper inputMapper(compositor.getSeat(), compositor.getRelativePointerManager());
    inputMapper.setGeometry(viewport, config.defaultWidth, config.defaultHeight);

    // Setup default game profile
    inputMapper.setupDefaultProfile();

    // 5. Connect Surface lifecycle callbacks
    compositor.onSurfaceMapped = [&](struct wlr_surface *surface) {
        std::cout << "[Main] >>> Android Surface is now ACTIVE and rendering inside viewport <<<" << std::endl;
    };

    compositor.onSurfaceUnmapped = [&](struct wlr_surface *surface) {
        (void)surface;
        std::cout << "[Main] <<< Android Surface unmapped >>>" << std::endl;
    };

    compositor.onSurfaceCommit = [&](struct wlr_surface *surface) {
        // Zero-copy render loop invocation on client commit
        renderer.clearLetterboxBars(viewport, currentWinWidth, currentWinHeight);
        if (renderer.renderSurface(surface, viewport, currentWinWidth, currentWinHeight)) {
            // Signal SurfaceFlinger that the frame has been presented
            renderer.sendFrameDone(surface);
        }
    };

    std::cout << "\n[Main] Compositor is READY." << std::endl;
    std::cout << "[Main] Socket: " << compositor.getSocketName() << std::endl;
    std::cout << "[Main] Launch container with:" << std::endl;
    std::cout << "  export WAYLAND_DISPLAY=" << compositor.getSocketName() << std::endl;
    std::cout << "  waydroid session start" << std::endl;
    std::cout << "  waydroid show-full-ui\n" << std::endl;

    // Main event loop
    while (g_running) {
        // Dispatch Wayland events (16ms = ~60 FPS polling timeout)
        compositor.dispatch(16);

        // Check if an active Android surface exists to render
        struct wlr_surface *activeSurface = compositor.getActiveSurface();
        if (activeSurface) {
            renderer.clearLetterboxBars(viewport, currentWinWidth, currentWinHeight);
            if (renderer.renderSurface(activeSurface, viewport, currentWinWidth, currentWinHeight)) {
                renderer.sendFrameDone(activeSurface);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[Main] Stopping embedded compositor..." << std::endl;
    compositor.shutdown();

    return 0;
}
