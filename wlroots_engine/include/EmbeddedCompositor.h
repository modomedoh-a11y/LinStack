#ifndef EMBEDDED_COMPOSITOR_H
#define EMBEDDED_COMPOSITOR_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE 1
#endif

// Include standard C++ headers first before re-mapping C99 static array bounds
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "wlroots_compat.h"

struct EmbeddedCompositorConfig {
    std::string socketName = "wayland-linstack-0";
    int defaultWidth = 1920;
    int defaultHeight = 1080;
    int refreshRate = 60; // سيتحدث تلقائياً من شاشة المستخدم
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

    // Getters
    struct wl_display *getDisplay() const { return m_display; }
    struct wl_event_loop *getEventLoop() const { return m_eventLoop; }
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
     *        Required for cross-context EGLImageKHR texture sharing into Qt's context.
     */
    EGLDisplay getEglDisplay() const;

    /**
     * @brief Returns the EGL context used by the wlroots GLES2 renderer.
     *        Required for cross-context EGLImageKHR creation from wlroots GL textures.
     */
    EGLContext getEglContext() const;

    // Surface lifecycle callbacks
    std::function<void(struct wlr_surface *surface)> onSurfaceMapped;
    std::function<void(struct wlr_surface *surface)> onSurfaceUnmapped;
    std::function<void(struct wlr_surface *surface)> onSurfaceCommit;
    std::function<void(struct wl_client *client)> onClientConnected;

    // Internal listener callbacks (invoked from C listener thunks)
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
    struct wlr_linux_drm_syncobj_manager_v1 *m_drmSyncobj = nullptr;
    struct wlr_xdg_shell *m_xdgShell = nullptr;
    struct wlr_seat *m_seat = nullptr;
    struct wlr_relative_pointer_manager_v1 *m_relativePointerManager = nullptr;
    struct wlr_output_layout *m_outputLayout = nullptr;
    struct wlr_output *m_headlessOutput = nullptr;
    int m_drmFd = -1;

    struct wlr_surface *m_activeSurface = nullptr;
    struct wlr_xdg_toplevel *m_activeToplevel = nullptr;

    friend void handleNewXdgSurfaceThunk(struct wl_listener *listener, void *data);
    friend void handleNewSurfaceThunk(struct wl_listener *listener, void *data);
    friend void handleClientCreatedThunk(struct wl_listener *listener, void *data);
    friend void handleNewXdgToplevelThunk(struct wl_listener *listener, void *data);

    // Signal listeners
    struct wl_listener m_clientCreatedListener;
    struct wl_listener m_newSurfaceListener;
    struct wl_listener m_newXdgSurfaceListener;
    struct wl_listener m_newXdgToplevelListener;

    // Track active XDG surface listeners
    struct XdgSurfaceRecord {
        EmbeddedCompositor *compositor = nullptr;
        struct wlr_xdg_surface *xdgSurface = nullptr;
        struct wl_listener map{};
        struct wl_listener unmap{};
        struct wl_listener destroy{};
        struct wl_listener commit{};

        XdgSurfaceRecord() {
            wl_list_init(&map.link);
            wl_list_init(&unmap.link);
            wl_list_init(&destroy.link);
            wl_list_init(&commit.link);
        }
        ~XdgSurfaceRecord() {
            wl_list_remove(&map.link);
            wl_list_remove(&unmap.link);
            wl_list_remove(&destroy.link);
            wl_list_remove(&commit.link);
        }
    };
    std::vector<std::unique_ptr<XdgSurfaceRecord>> m_surfaceRecords;

    // Track XDG toplevels (fires AFTER role assignment — correct place to configure)
    struct XdgToplevelRecord {
        EmbeddedCompositor *compositor = nullptr;
        struct wlr_xdg_toplevel *toplevel = nullptr;
        struct wl_listener request_maximize{};
        struct wl_listener request_fullscreen{};
        struct wl_listener request_minimize{};
        struct wl_listener destroy{};

        XdgToplevelRecord() {
            wl_list_init(&request_maximize.link);
            wl_list_init(&request_fullscreen.link);
            wl_list_init(&request_minimize.link);
            wl_list_init(&destroy.link);
        }
        ~XdgToplevelRecord() {
            wl_list_remove(&request_maximize.link);
            wl_list_remove(&request_fullscreen.link);
            wl_list_remove(&request_minimize.link);
            wl_list_remove(&destroy.link);
        }
    };
    std::vector<std::unique_ptr<XdgToplevelRecord>> m_toplevelRecords;

    // Track all created wl_surface instances (including hwcomposer surfaces & subsurfaces)
    struct RawSurfaceRecord {
        EmbeddedCompositor *compositor = nullptr;
        struct wlr_surface *surface = nullptr;
        struct wl_listener commit{};
        struct wl_listener destroy{};

        RawSurfaceRecord() {
            wl_list_init(&commit.link);
            wl_list_init(&destroy.link);
        }
        ~RawSurfaceRecord() {
            wl_list_remove(&commit.link);
            wl_list_remove(&destroy.link);
        }
    };
    std::vector<std::unique_ptr<RawSurfaceRecord>> m_rawSurfaceRecords;
};

#endif // EMBEDDED_COMPOSITOR_H
