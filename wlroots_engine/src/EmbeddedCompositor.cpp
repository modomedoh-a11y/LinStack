#include "EmbeddedCompositor.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <xkbcommon/xkbcommon.h>

void handleClientCreatedThunk(struct wl_listener *listener, void *data) {
    auto *compositor = wl_container_of(listener, (EmbeddedCompositor *)nullptr, m_clientCreatedListener);
    auto *client = static_cast<struct wl_client *>(data);
    compositor->handleClientCreated(client);
}

void handleNewSurfaceThunk(struct wl_listener *listener, void *data) {
    auto *compositor = wl_container_of(listener, (EmbeddedCompositor *)nullptr, m_newSurfaceListener);
    auto *surface = static_cast<struct wlr_surface *>(data);
    compositor->handleNewSurface(surface);
}

void handleNewXdgSurfaceThunk(struct wl_listener *listener, void *data) {
    auto *compositor = wl_container_of(listener, (EmbeddedCompositor *)nullptr, m_newXdgSurfaceListener);
    auto *xdgSurface = static_cast<struct wlr_xdg_surface *>(data);
    compositor->handleNewXdgSurface(xdgSurface);
}

void handleNewXdgToplevelThunk(struct wl_listener *listener, void *data) {
    auto *compositor = wl_container_of(listener, (EmbeddedCompositor *)nullptr, m_newXdgToplevelListener);
    auto *toplevel = static_cast<struct wlr_xdg_toplevel *>(data);
    compositor->handleNewXdgToplevel(toplevel);
}

EmbeddedCompositor::EmbeddedCompositor(const EmbeddedCompositorConfig &config)
: m_config(config)
{
    wl_list_init(&m_clientCreatedListener.link);
    wl_list_init(&m_newSurfaceListener.link);
    wl_list_init(&m_newXdgSurfaceListener.link);
    wl_list_init(&m_newXdgToplevelListener.link);
}

EmbeddedCompositor::~EmbeddedCompositor() {
    shutdown();
}

bool EmbeddedCompositor::makeCurrent() {
    if (!m_renderer) return false;
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(m_renderer);
    if (!egl) return false;
    EGLDisplay dpy = wlr_egl_get_display(egl);
    EGLContext ctx = wlr_egl_get_context(egl);
    return eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx) == EGL_TRUE;
}

EGLDisplay EmbeddedCompositor::getEglDisplay() const {
    if (!m_renderer) return EGL_NO_DISPLAY;
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(m_renderer);
    if (!egl) return EGL_NO_DISPLAY;
    return wlr_egl_get_display(egl);
}

EGLContext EmbeddedCompositor::getEglContext() const {
    if (!m_renderer) return EGL_NO_CONTEXT;
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(m_renderer);
    if (!egl) return EGL_NO_CONTEXT;
    return wlr_egl_get_context(egl);
}

bool EmbeddedCompositor::initialize() {
    std::cout << "[EmbeddedCompositor] Initializing Wayland display server..." << std::endl;

    m_display = wl_display_create();
    if (!m_display) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create Wayland display." << std::endl;
        return false;
    }

    m_eventLoop = wl_display_get_event_loop(m_display);
    if (!m_eventLoop) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to get Wayland event loop." << std::endl;
        return false;
    }

    // Listen to incoming Wayland client connections
    m_clientCreatedListener.notify = handleClientCreatedThunk;
    wl_display_add_client_created_listener(m_display, &m_clientCreatedListener);

    // 1. Create headless backend for the embedded container
    m_backend = wlr_headless_backend_create(m_eventLoop);
    if (!m_backend) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create headless wlroots backend." << std::endl;
        return false;
    }

    // 2. Hardware-accelerated GLES2/Vulkan renderer bound to Intel DRM render node
    setenv("WLR_RENDER_DRM_DEVICE", "/dev/dri/renderD128", 1);
    m_drmFd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (m_drmFd >= 0) {
        std::cout << "[EmbeddedCompositor] Opened DRM render node /dev/dri/renderD128 (fd: " << m_drmFd << ")" << std::endl;
        m_renderer = wlr_gles2_renderer_create_with_drm_fd(m_drmFd);
    }
    if (!m_renderer) {
        std::cout << "[EmbeddedCompositor] Falling back to wlr_renderer_autocreate..." << std::endl;
        m_renderer = wlr_renderer_autocreate(m_backend);
    }
    if (!m_renderer) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create wlroots renderer." << std::endl;
        return false;
    }

    m_allocator = wlr_allocator_autocreate(m_backend, m_renderer);
    if (!m_allocator) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create wlroots allocator." << std::endl;
        return false;
    }

    // Bind renderer to Wayland display (enables EGL/SHM Wayland protocols)
    wlr_renderer_init_wl_display(m_renderer, m_display);

    // 3. Register virtual headless output and output layout for Android hwcomposer
    m_outputLayout = wlr_output_layout_create(m_display);
    m_headlessOutput = wlr_headless_add_output(m_backend, m_config.defaultWidth, m_config.defaultHeight);
    if (m_headlessOutput) {
        wlr_output_init_render(m_headlessOutput, m_allocator, m_renderer);
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);
        wlr_output_state_set_custom_mode(&state, m_config.defaultWidth, m_config.defaultHeight, (m_config.refreshRate > 0 ? m_config.refreshRate : 60) * 1000);
        wlr_output_commit_state(m_headlessOutput, &state);
        wlr_output_state_finish(&state);
        wlr_output_layout_add_auto(m_outputLayout, m_headlessOutput);
        wlr_output_create_global(m_headlessOutput, m_display);
        std::cout << "[EmbeddedCompositor] Virtual headless output configured: "
        << m_config.defaultWidth << "x" << m_config.defaultHeight << "  @ " << (m_config.refreshRate > 0 ? m_config.refreshRate : 60) << "Hz" << std::endl;
    } else {
        std::cerr << "[EmbeddedCompositor] Warning: Failed to create headless output for container." << std::endl;
    }

    // XDG Output Manager (exposes output geometry and names to Wayland clients)
    m_xdgOutputManager = wlr_xdg_output_manager_v1_create(m_display, m_outputLayout);
    if (m_xdgOutputManager) {
        std::cout << "[EmbeddedCompositor] zxdg_output_manager_v1 interface registered." << std::endl;
    }

    // 4. Core Wayland Compositor (version 5) with renderer backing
    m_compositor = wlr_compositor_create(m_display, 5, m_renderer);
    m_newSurfaceListener.notify = handleNewSurfaceThunk;
    wl_signal_add(&m_compositor->events.new_surface, &m_newSurfaceListener);

    // Subcompositor & Data Device Manager (mandatory for Android hwcomposer & clipboard)
    m_subcompositor = wlr_subcompositor_create(m_display);
    m_dataDeviceManager = wlr_data_device_manager_create(m_display);

    // 5. Shared Memory (wl_shm) + Linux DMA-BUF v1 hardware zero-copy buffer sharing
    m_shm = wlr_shm_create_with_renderer(m_display, 1, m_renderer);
    if (!m_shm) {
        wl_display_init_shm(m_display);
    }
    std::cout << "[EmbeddedCompositor] wl_shm interface registered." << std::endl;

    m_linuxDmabuf = wlr_linux_dmabuf_v1_create_with_renderer(m_display, 4, m_renderer);
    if (!m_linuxDmabuf) {
        std::cerr << "[EmbeddedCompositor] Warning: Failed to create linux-dmabuf-v1 global." << std::endl;
    } else {
        std::cout << "[EmbeddedCompositor] linux-dmabuf-v1 interface initialized for zero-copy DMA-BUF import." << std::endl;
    }

    // 6. Presentation Time Protocol (wp_presentation) for VSync frame synchronization feedback
    m_presentation = wlr_presentation_create(m_display, m_backend, 1);
    if (m_presentation) {
        std::cout << "[EmbeddedCompositor] wp_presentation interface registered for VSync presentation feedback." << std::endl;
    }

    // 7. Viewporter & Single Pixel Buffer Manager
    m_viewporter = wlr_viewporter_create(m_display);
    if (m_viewporter) {
        std::cout << "[EmbeddedCompositor] wp_viewporter interface registered." << std::endl;
    }
    m_singlePixelBuffer = wlr_single_pixel_buffer_manager_v1_create(m_display);

    // Fractional scale & Pointer constraints (for high-DPI scaling and mouse lock)
    m_fractionalScale = wlr_fractional_scale_manager_v1_create(m_display, 1);
    if (m_fractionalScale) {
        std::cout << "[EmbeddedCompositor] wp_fractional_scale_manager_v1 interface registered." << std::endl;
    }
    m_pointerConstraints = wlr_pointer_constraints_v1_create(m_display);
    if (m_pointerConstraints) {
        std::cout << "[EmbeddedCompositor] zwp_pointer_constraints_v1 interface registered." << std::endl;
    }

    // [تعديل 1]: تم تعطيل Explicit Sync لتجنب تعارضه مع تعريف Mesa لبطاقة Intel HD 530
    /*
     *    if (m_drmFd >= 0) {
     *        m_drmSyncobj = wlr_linux_drm_syncobj_manager_v1_create(m_display, 1, m_drmFd);
     *        if (m_drmSyncobj) {
     *            std::cout << "[EmbeddedCompositor] wp_linux_drm_syncobj_v1 interface registered for explicit sync." << std::endl;
}
}
*/

    // 6. XDG Shell v3 interface
    m_xdgShell = wlr_xdg_shell_create(m_display, 3);
    m_newXdgSurfaceListener.notify = handleNewXdgSurfaceThunk;
    wl_signal_add(&m_xdgShell->events.new_surface, &m_newXdgSurfaceListener);
    m_newXdgToplevelListener.notify = handleNewXdgToplevelThunk;
    wl_signal_add(&m_xdgShell->events.new_toplevel, &m_newXdgToplevelListener);

    // 7. Wayland input seat (multi-touch, pointer, keyboard)
    m_seat = wlr_seat_create(m_display, "linstack-seat-0");
    wlr_seat_set_capabilities(m_seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_TOUCH);

    // Initialize virtual wlr_keyboard directly (wlroots 0.20 API) for Android system keys
    static const struct wlr_keyboard_impl virtual_kbd_impl = { .name = "linstack-virtual-kbd", .led_update = nullptr };
    wlr_keyboard_init(&m_virtualKeyboard, &virtual_kbd_impl, "linstack-virtual-kbd");
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap) {
        wlr_keyboard_set_keymap(&m_virtualKeyboard, keymap);
        wlr_seat_set_keyboard(m_seat, &m_virtualKeyboard);
        m_keyboardInitialized = true;
        xkb_keymap_unref(keymap);
    }
    if (ctx) xkb_context_unref(ctx);

    // 8. Relative pointer manager for FPS mouse look
    m_relativePointerManager = wlr_relative_pointer_manager_v1_create(m_display);

    // 9. Bind dedicated Wayland display socket (safely unlink any stale socket files first)
    const char *xdgRuntime = getenv("XDG_RUNTIME_DIR");
    if (xdgRuntime) {
        std::string sock = std::string(xdgRuntime) + "/" + m_config.socketName;
        std::string lock = sock + ".lock";
        unlink(sock.c_str());
        unlink(lock.c_str());
    }

    int res = wl_display_add_socket(m_display, m_config.socketName.c_str());
    if (res != 0) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to bind socket: " << m_config.socketName << std::endl;
        return false;
    }

    std::cout << "[EmbeddedCompositor] Successfully listening on Wayland socket: " << m_config.socketName << std::endl;
    return true;
}

bool EmbeddedCompositor::start() {
    if (!m_backend) return false;
    if (!wlr_backend_start(m_backend)) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to start wlroots backend." << std::endl;
        return false;
    }
    std::cout << "[EmbeddedCompositor] Headless backend running successfully." << std::endl;
    return true;
}

int EmbeddedCompositor::dispatch(int timeoutMs) {
    if (!m_display || !m_eventLoop) return -1;
    wl_display_flush_clients(m_display);
    int res = wl_event_loop_dispatch(m_eventLoop, timeoutMs);
    wl_display_flush_clients(m_display);
    return res;
}

void EmbeddedCompositor::shutdown() {
    if (!m_display) return;

    // [تعديل 2]: فك ربط سياق EGL قبل تدمير الموارد لمنع خطأ EGL_BAD_SURFACE (0x300d)
    EGLDisplay dpy = getEglDisplay();
    if (dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    m_toplevelRecords.clear();
    m_surfaceRecords.clear();
    m_rawSurfaceRecords.clear();
    m_activeSurface = nullptr;
    m_activeToplevel = nullptr;

    if (m_clientCreatedListener.link.prev && m_clientCreatedListener.link.next) {
        wl_list_remove(&m_clientCreatedListener.link);
        wl_list_init(&m_clientCreatedListener.link);
    }

    if (m_newSurfaceListener.link.prev && m_newSurfaceListener.link.next) {
        wl_list_remove(&m_newSurfaceListener.link);
        wl_list_init(&m_newSurfaceListener.link);
    }

    if (m_newXdgSurfaceListener.link.prev && m_newXdgSurfaceListener.link.next) {
        wl_list_remove(&m_newXdgSurfaceListener.link);
        wl_list_init(&m_newXdgSurfaceListener.link);
    }

    if (m_newXdgToplevelListener.link.prev && m_newXdgToplevelListener.link.next) {
        wl_list_remove(&m_newXdgToplevelListener.link);
        wl_list_init(&m_newXdgToplevelListener.link);
    }

    if (m_keyboardInitialized) {
        wlr_keyboard_finish(&m_virtualKeyboard);
        m_keyboardInitialized = false;
    }

    if (m_keyboardInitialized) {
        wlr_keyboard_finish(&m_virtualKeyboard);
        m_keyboardInitialized = false;
    }

    if (m_outputLayout) {
        wlr_output_layout_destroy(m_outputLayout);
        m_outputLayout = nullptr;
    }
    m_headlessOutput = nullptr;

    if (m_backend) {
        wlr_backend_destroy(m_backend);
        m_backend = nullptr;
    }

    if (m_display) {
        wl_display_destroy_clients(m_display);
        wl_display_destroy(m_display);
        m_display = nullptr;
    }

    if (m_drmFd >= 0) {
        close(m_drmFd);
        m_drmFd = -1;
    }
    std::cout << "[EmbeddedCompositor] Compositor shutdown complete." << std::endl;
}

void EmbeddedCompositor::handleClientCreated(struct wl_client *client) {
    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(client, &pid, &uid, &gid);
    std::cout << "[EmbeddedCompositor] >>> Wayland Client Connected: PID=" << pid
    << ", UID=" << uid << ", GID=" << gid << " <<<" << std::endl;
    if (onClientConnected) {
        onClientConnected(client);
    }
}

void EmbeddedCompositor::handleNewSurface(struct wlr_surface *surface) {
    if (!surface) return;
    std::cout << "[EmbeddedCompositor] New wlr_surface created (" << surface << ")" << std::endl;

    auto record = std::make_unique<RawSurfaceRecord>();
    record->compositor = this;
    record->surface = surface;

    record->commit.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (RawSurfaceRecord *)nullptr, commit);
        rec->compositor->handleSurfaceCommit(rec->surface);
    };
    wl_signal_add(&surface->events.commit, &record->commit);

    record->destroy.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (RawSurfaceRecord *)nullptr, destroy);
        wl_list_remove(&rec->destroy.link);
        wl_list_init(&rec->destroy.link);
        rec->compositor->handleRawSurfaceDestroy(rec->surface);
    };
    wl_signal_add(&surface->events.destroy, &record->destroy);

    m_rawSurfaceRecords.push_back(std::move(record));
}

void EmbeddedCompositor::handleRawSurfaceDestroy(struct wlr_surface *surface) {
    if (!surface) return;
    std::cout << "[EmbeddedCompositor] wlr_surface destroyed (" << surface << ")" << std::endl;
    if (m_activeSurface == surface) {
        m_activeSurface = nullptr;
        for (const auto &rec : m_rawSurfaceRecords) {
            if (rec->surface && rec->surface != surface && wlr_surface_has_buffer(rec->surface)) {
                m_activeSurface = rec->surface;
                break;
            }
        }
        if (onSurfaceUnmapped) {
            onSurfaceUnmapped(surface);
        }
    }

    auto it = std::remove_if(m_rawSurfaceRecords.begin(), m_rawSurfaceRecords.end(),
                             [surface](const std::unique_ptr<RawSurfaceRecord> &rec) {
                                 return rec->surface == surface;
                             });
    m_rawSurfaceRecords.erase(it, m_rawSurfaceRecords.end());
}

struct wlr_surface *EmbeddedCompositor::getRenderableSurface() const {
    struct wlr_surface *bestSurface = nullptr;
    int maxArea = 0;

    for (const auto &rec : m_rawSurfaceRecords) {
        if (rec->surface && (wlr_surface_has_buffer(rec->surface) || wlr_surface_get_texture(rec->surface))) {
            int area = rec->surface->current.width * rec->surface->current.height;
            if (area > maxArea) {
                maxArea = area;
                bestSurface = rec->surface;
            }
        }
    }
    if (bestSurface) return bestSurface;
    return m_activeSurface;
}


void EmbeddedCompositor::handleNewXdgSurface(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;

    std::cout << "[EmbeddedCompositor] Incoming XDG surface created (surface: "
    << xdgSurface->surface << ") — awaiting role assignment" << std::endl;

    auto record = std::make_unique<XdgSurfaceRecord>();
    record->compositor = this;
    record->xdgSurface = xdgSurface;

    // Map listener
    record->map.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgSurfaceRecord *)nullptr, map);
        rec->compositor->handleXdgSurfaceMap(rec->xdgSurface);
    };
    wl_signal_add(&xdgSurface->surface->events.map, &record->map);

    // Unmap listener
    record->unmap.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgSurfaceRecord *)nullptr, unmap);
        rec->compositor->handleXdgSurfaceUnmap(rec->xdgSurface);
    };
    wl_signal_add(&xdgSurface->surface->events.unmap, &record->unmap);

    // Destroy listener
    record->destroy.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgSurfaceRecord *)nullptr, destroy);
        wl_list_remove(&rec->destroy.link);
        wl_list_init(&rec->destroy.link);
        rec->compositor->handleXdgSurfaceDestroy(rec->xdgSurface);
    };
    wl_signal_add(&xdgSurface->events.destroy, &record->destroy);

    // Commit listener
    record->commit.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgSurfaceRecord *)nullptr, commit);
        struct wlr_xdg_toplevel *xdgToplevel =
        wlr_xdg_toplevel_try_from_wlr_surface(rec->xdgSurface->surface);
        if (xdgToplevel && xdgToplevel->base->initial_commit) {
            EmbeddedCompositor *comp = rec->compositor;
            wlr_xdg_toplevel_set_size(xdgToplevel,
                                      comp->m_config.defaultWidth, comp->m_config.defaultHeight);
            wlr_xdg_toplevel_set_activated(xdgToplevel, true);
            wlr_xdg_toplevel_set_fullscreen(xdgToplevel, true);
            std::cout << "[EmbeddedCompositor] initial_commit: configure "
            << comp->m_config.defaultWidth << "x" << comp->m_config.defaultHeight
            << " fullscreen+activated → toplevel " << xdgToplevel << std::endl;
        }
        rec->compositor->handleSurfaceCommit(rec->xdgSurface->surface);
    };
    wl_signal_add(&xdgSurface->surface->events.commit, &record->commit);

    m_surfaceRecords.push_back(std::move(record));
}

void EmbeddedCompositor::handleXdgSurfaceMap(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface || !xdgSurface->surface) return;
    m_activeSurface = xdgSurface->surface;
    struct wlr_xdg_toplevel *toplevel = wlr_xdg_toplevel_try_from_wlr_surface(xdgSurface->surface);
    if (toplevel && (xdgSurface->surface->current.width != m_config.defaultWidth ||
                     xdgSurface->surface->current.height != m_config.defaultHeight)) {
        wlr_xdg_toplevel_set_size(toplevel, m_config.defaultWidth, m_config.defaultHeight);
        wlr_xdg_surface_schedule_configure(xdgSurface);
    }
    std::cout << "[EmbeddedCompositor] Android Wayland surface mapped ("
    << xdgSurface->surface->current.width << "x" << xdgSurface->surface->current.height
    << ", surface: " << xdgSurface->surface << ")" << std::endl;

    if (onSurfaceMapped) {
        onSurfaceMapped(m_activeSurface);
    }
}

void EmbeddedCompositor::handleXdgSurfaceUnmap(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;
    std::cout << "[EmbeddedCompositor] Android Wayland surface unmapped (" << xdgSurface->surface << ")" << std::endl;
    if (m_activeSurface == xdgSurface->surface) {
        m_activeSurface = nullptr;
    }
    if (onSurfaceUnmapped) {
        onSurfaceUnmapped(xdgSurface->surface);
    }
}

void EmbeddedCompositor::handleXdgSurfaceDestroy(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;
    if (m_activeSurface == xdgSurface->surface) {
        m_activeSurface = nullptr;
    }
    if (m_activeToplevel == xdgSurface->toplevel) {
        m_activeToplevel = nullptr;
    }

    auto it = std::remove_if(m_surfaceRecords.begin(), m_surfaceRecords.end(),
                             [xdgSurface](const std::unique_ptr<XdgSurfaceRecord> &rec) {
                                 return rec->xdgSurface == xdgSurface;
                             });
    m_surfaceRecords.erase(it, m_surfaceRecords.end());
}

void EmbeddedCompositor::handleNewXdgToplevel(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel) return;

    std::cout << "[EmbeddedCompositor] >>> New XDG toplevel (role assigned): "
    << toplevel << " surface=" << toplevel->base->surface << std::endl;

    m_activeToplevel = toplevel;

    auto record = std::make_unique<XdgToplevelRecord>();
    record->compositor = this;
    record->toplevel = toplevel;

    record->request_maximize.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgToplevelRecord *)nullptr, request_maximize);
        if (rec->toplevel && rec->toplevel->base && rec->toplevel->base->initialized) {
            wlr_xdg_toplevel_set_maximized(rec->toplevel, true);
            wlr_xdg_surface_schedule_configure(rec->toplevel->base);
        }
    };
    wl_signal_add(&toplevel->events.request_maximize, &record->request_maximize);

    record->request_fullscreen.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgToplevelRecord *)nullptr, request_fullscreen);
        if (rec->toplevel && rec->toplevel->base && rec->toplevel->base->initialized) {
            wlr_xdg_toplevel_set_fullscreen(rec->toplevel, true);
            wlr_xdg_surface_schedule_configure(rec->toplevel->base);
            std::cout << "[EmbeddedCompositor] Granted fullscreen to XDG toplevel" << std::endl;
        }
    };
    wl_signal_add(&toplevel->events.request_fullscreen, &record->request_fullscreen);

    record->request_minimize.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgToplevelRecord *)nullptr, request_minimize);
        if (rec->toplevel && rec->toplevel->base && rec->toplevel->base->initialized) {
            wlr_xdg_surface_schedule_configure(rec->toplevel->base);
        }
    };
    wl_signal_add(&toplevel->events.request_minimize, &record->request_minimize);

    record->destroy.notify = [](struct wl_listener *listener, void *data) {
        auto *rec = wl_container_of(listener, (XdgToplevelRecord *)nullptr, destroy);
        EmbeddedCompositor *comp = rec->compositor;
        if (comp->m_activeToplevel == rec->toplevel) {
            comp->m_activeToplevel = nullptr;
        }
        auto it = std::remove_if(comp->m_toplevelRecords.begin(), comp->m_toplevelRecords.end(),
                                 [rec](const std::unique_ptr<XdgToplevelRecord> &r) { return r.get() == rec; });
        comp->m_toplevelRecords.erase(it, comp->m_toplevelRecords.end());
        std::cout << "[EmbeddedCompositor] XDG toplevel destroyed" << std::endl;
    };
    wl_signal_add(&toplevel->events.destroy, &record->destroy);

    m_toplevelRecords.push_back(std::move(record));
}

void EmbeddedCompositor::handleSurfaceCommit(struct wlr_surface *surface) {
    if (!surface) return;

    bool hasBuf = wlr_surface_has_buffer(surface);

    if (!m_activeSurface && (hasBuf || surface->current.width > 200)) {
        m_activeSurface = surface;
        std::cout << "[EmbeddedCompositor] Promoting wlr_surface " << surface << " to active surface" << std::endl;
        if (onSurfaceMapped) {
            onSurfaceMapped(m_activeSurface);
        }
    }

    if (onSurfaceCommit) {
        onSurfaceCommit(surface);
    }
}

