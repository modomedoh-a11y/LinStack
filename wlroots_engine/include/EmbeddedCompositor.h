#ifndef EMBEDDED_COMPOSITOR_H
#define EMBEDDED_COMPOSITOR_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "wlr_c_bridge.h"

struct EmbeddedCompositorConfig {
    std::string socketName = "wayland-linstack-0";
    int defaultWidth = 1920;
    int defaultHeight = 1080;
    int refreshRate = 60;
};

class EmbeddedCompositor {
public:
    explicit EmbeddedCompositor(const EmbeddedCompositorConfig &config = EmbeddedCompositorConfig{});
    ~EmbeddedCompositor();

    // Non-copyable
    EmbeddedCompositor(const EmbeddedCompositor &) = delete;
    EmbeddedCompositor &operator=(const EmbeddedCompositor &) = delete;

    /**
     * @brief Initializes Wayland display, headless wlroots backend, GLES2 renderer,
     * DMA-BUF importer, XDG Shell, seat, and binds the socket.
     */
    bool initialize();

    /**
     * @brief Starts the wlroots backend.
     */
    bool start();

    /**
     * @brief Dispatches any pending Wayland display and event loop events.
     * @param timeoutMs Timeout in milliseconds (0 for non-blocking poll).
     */
    int dispatch(int timeoutMs = 0);

    /**
     * @brief Shuts down the compositor, releases listeners, and removes the socket.
     */
    void shutdown();

    /**
     * @brief Binds the compositor's EGL context to the current thread.
     */
    bool makeCurrent();

    // Getters returning opaque pointers
    struct wl_display *getDisplay() const { return m_display; }
    struct wl_event_loop *getEventLoop() const { return m_eventLoop; }
    int getEventLoopFd() const { return wlr_bridge_event_loop_get_fd(m_eventLoop); }
    void flush() const { if (m_display) wlr_bridge_display_flush_clients(m_display); }
    struct wlr_backend *getBackend() const { return m_backend; }
    struct wlr_renderer *getRenderer() const { return m_renderer; }
    struct wlr_seat *getSeat() const { return m_seat; }
    struct wlr_relative_pointer_manager_v1 *getRelativePointerManager() const { return m_relativePointerManager; }
    struct wlr_output_layout *getOutputLayout() const { return m_outputLayout; }
    struct wlr_output *getHeadlessOutput() const { return m_headlessOutput; }
    struct wlr_presentation *getPresentation() const { return m_presentation; }
    struct wlr_viewporter *getViewporter() const { return m_viewporter; }
    struct wlr_xdg_output_manager_v1 *getXdgOutputManager() const { return m_xdgOutputManager; }
    struct wlr_fractional_scale_manager_v1 *getFractionalScale() const { return m_fractionalScale; }
    struct wlr_pointer_constraints_v1 *getPointerConstraints() const { return m_pointerConstraints; }
    struct wlr_data_device_manager *getDataDeviceManager() const { return m_dataDeviceManager; }
    struct wlr_surface *getActiveSurface() const { return m_activeSurface; }
    struct wlr_surface *getRenderableSurface() const;
    std::string getSocketName() const { return m_config.socketName; }
    int getWidth() const { return m_config.defaultWidth; }
    int getHeight() const { return m_config.defaultHeight; }

    /**
     * @brief Returns the EGL display used by the wlroots GLES2 renderer.
     */
    EGLDisplay getEglDisplay() const;

    /**
     * @brief Returns the EGL context used by the wlroots GLES2 renderer.
     */
    EGLContext getEglContext() const;

    // Surface lifecycle callbacks
    std::function<void(struct wlr_surface *surface)> onSurfaceMapped;
    std::function<void(struct wlr_surface *surface)> onSurfaceUnmapped;
    std::function<void(struct wlr_surface *surface)> onSurfaceCommit;
    std::function<void(struct wl_client *client)> onClientConnected;

    // Internal bridge callbacks
    void handleClientCreated(struct wl_client *client);
    void handleNewSurface(struct wlr_surface *surface);
    void handleNewXdgSurface(struct wlr_xdg_surface *xdgSurface);
    void handleNewXdgToplevel(struct wlr_xdg_toplevel *toplevel);
    void handleXdgSurfaceMap(struct wlr_xdg_surface *xdgSurface);
    void handleXdgSurfaceUnmap(struct wlr_xdg_surface *xdgSurface);
    void handleXdgSurfaceDestroy(struct wlr_xdg_surface *xdgSurface);
    void handleSurfaceCommit(struct wlr_surface *surface);
    void handleRawSurfaceDestroy(struct wlr_surface *surface);

private:
    EmbeddedCompositorConfig m_config;

    struct wl_display *m_display = nullptr;
    struct wl_event_loop *m_eventLoop = nullptr;
    struct wlr_backend *m_backend = nullptr;
    struct wlr_renderer *m_renderer = nullptr;
    struct wlr_allocator *m_allocator = nullptr;
    struct wlr_compositor *m_compositor = nullptr;
    struct wlr_subcompositor *m_subcompositor = nullptr;
    struct wlr_data_device_manager *m_dataDeviceManager = nullptr;
    struct wlr_shm *m_shm = nullptr;
    struct wlr_linux_dmabuf_v1 *m_linuxDmabuf = nullptr;
    struct wlr_presentation *m_presentation = nullptr;
    struct wlr_viewporter *m_viewporter = nullptr;
    struct wlr_xdg_output_manager_v1 *m_xdgOutputManager = nullptr;
    struct wlr_fractional_scale_manager_v1 *m_fractionalScale = nullptr;
    struct wlr_pointer_constraints_v1 *m_pointerConstraints = nullptr;
    struct wlr_single_pixel_buffer_manager_v1 *m_singlePixelBuffer = nullptr;
    struct wlr_xdg_shell *m_xdgShell = nullptr;
    struct wlr_seat *m_seat = nullptr;
    struct wlr_keyboard *m_virtualKeyboard = nullptr;
    struct wlr_relative_pointer_manager_v1 *m_relativePointerManager = nullptr;
    struct wlr_output_layout *m_outputLayout = nullptr;
    struct wlr_output *m_headlessOutput = nullptr;
    int m_drmFd = -1;

    struct wlr_surface *m_activeSurface = nullptr;
    struct wlr_xdg_toplevel *m_activeToplevel = nullptr;

    // Signal listeners from C bridge
    struct wlr_bridge_listener *m_clientCreatedListener = nullptr;
    struct wlr_bridge_listener *m_newSurfaceListener = nullptr;
    struct wlr_bridge_listener *m_newXdgSurfaceListener = nullptr;
    struct wlr_bridge_listener *m_newXdgToplevelListener = nullptr;

    // Track active XDG surface records
    std::vector<std::unique_ptr<struct wlr_bridge_xdg_surface_record, void(*)(struct wlr_bridge_xdg_surface_record*)>> m_surfaceRecords;

    // Track XDG toplevel records
    std::vector<std::unique_ptr<struct wlr_bridge_xdg_toplevel_record, void(*)(struct wlr_bridge_xdg_toplevel_record*)>> m_toplevelRecords;

    // Track raw surface records
    std::vector<std::unique_ptr<struct wlr_bridge_raw_surface_record, void(*)(struct wlr_bridge_raw_surface_record*)>> m_rawSurfaceRecords;
};

#endif // EMBEDDED_COMPOSITOR_H
