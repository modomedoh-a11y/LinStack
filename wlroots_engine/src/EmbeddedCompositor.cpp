#include "EmbeddedCompositor.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

EmbeddedCompositor::EmbeddedCompositor(const EmbeddedCompositorConfig &config)
    : m_config(config)
{
}

EmbeddedCompositor::~EmbeddedCompositor() {
    shutdown();
}

bool EmbeddedCompositor::makeCurrent() {
    EGLDisplay dpy = getEglDisplay();
    EGLContext ctx = getEglContext();
    if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx) == EGL_TRUE;
}

EGLDisplay EmbeddedCompositor::getEglDisplay() const {
    if (!m_renderer) return EGL_NO_DISPLAY;
    return wlr_bridge_get_egl_display(m_renderer);
}

EGLContext EmbeddedCompositor::getEglContext() const {
    if (!m_renderer) return EGL_NO_CONTEXT;
    return wlr_bridge_get_egl_context(m_renderer);
}

bool EmbeddedCompositor::initialize() {
    std::cout << "[EmbeddedCompositor] Initializing Wayland display server..." << std::endl;

    m_display = wlr_bridge_display_create();
    if (!m_display) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create Wayland display." << std::endl;
        return false;
    }

    m_eventLoop = wlr_bridge_display_get_event_loop(m_display);
    if (!m_eventLoop) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to get Wayland event loop." << std::endl;
        return false;
    }

    // Listen to incoming Wayland client connections
    m_clientCreatedListener = wlr_bridge_display_add_client_created_listener(
        m_display,
        [](void *user, struct wl_client *client) {
            static_cast<EmbeddedCompositor*>(user)->handleClientCreated(client);
        },
        this
    );

    // 1. Create headless backend for the embedded container
    m_backend = wlr_bridge_headless_backend_create(m_eventLoop);
    if (!m_backend) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create headless wlroots backend." << std::endl;
        return false;
    }

    // 2. Hardware-accelerated GLES2 renderer bound to Intel / AMD / DRM render node
    setenv("WLR_RENDER_DRM_DEVICE", "/dev/dri/renderD128", 1);
    m_drmFd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (m_drmFd >= 0) {
        std::cout << "[EmbeddedCompositor] Opened DRM render node /dev/dri/renderD128 (fd: " << m_drmFd << ")" << std::endl;
        m_renderer = wlr_bridge_gles2_renderer_create_with_drm_fd(m_drmFd);
    }
    if (!m_renderer) {
        std::cout << "[EmbeddedCompositor] Falling back to wlr_renderer_autocreate..." << std::endl;
        m_renderer = wlr_bridge_renderer_autocreate(m_backend);
    }
    if (!m_renderer) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create wlroots renderer." << std::endl;
        return false;
    }

    m_allocator = wlr_bridge_allocator_autocreate(m_backend, m_renderer);
    if (!m_allocator) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to create wlroots allocator." << std::endl;
        return false;
    }

    // Bind renderer to Wayland display
    wlr_bridge_renderer_init_wl_display(m_renderer, m_display);

    // 3. Register virtual headless output and output layout for Android hwcomposer
    m_outputLayout = wlr_bridge_output_layout_create(m_display);
    m_headlessOutput = wlr_bridge_headless_add_output(m_backend, m_config.defaultWidth, m_config.defaultHeight);
    if (m_headlessOutput) {
        wlr_bridge_output_init_render(m_headlessOutput, m_allocator, m_renderer);
        wlr_bridge_output_configure(m_headlessOutput, m_config.defaultWidth, m_config.defaultHeight,
                                    m_config.refreshRate > 0 ? m_config.refreshRate : 60);
        wlr_bridge_output_layout_add_auto(m_outputLayout, m_headlessOutput);
        wlr_bridge_output_create_global(m_headlessOutput, m_display);
        std::cout << "[EmbeddedCompositor] Virtual headless output configured: "
                  << m_config.defaultWidth << "x" << m_config.defaultHeight << " @ "
                  << (m_config.refreshRate > 0 ? m_config.refreshRate : 60) << "Hz" << std::endl;
    } else {
        std::cerr << "[EmbeddedCompositor] Warning: Failed to create headless output for container." << std::endl;
    }

    // XDG Output Manager
    m_xdgOutputManager = wlr_bridge_xdg_output_manager_create(m_display, m_outputLayout);
    if (m_xdgOutputManager) {
        std::cout << "[EmbeddedCompositor] zxdg_output_manager_v1 interface registered." << std::endl;
    }

    // 4. Core Wayland Compositor (version 5) with renderer backing
    m_compositor = wlr_bridge_compositor_create(m_display, 5, m_renderer);
    m_newSurfaceListener = wlr_bridge_compositor_add_new_surface_listener(
        m_compositor,
        [](void *user, struct wlr_surface *surface) {
            static_cast<EmbeddedCompositor*>(user)->handleNewSurface(surface);
        },
        this
    );

    // Subcompositor & Data Device Manager
    m_subcompositor = wlr_bridge_subcompositor_create(m_display);
    m_dataDeviceManager = wlr_bridge_data_device_manager_create(m_display);

    // 5. Shared Memory (wl_shm) + Linux DMA-BUF v1 zero-copy buffer sharing
    m_shm = wlr_bridge_shm_create_with_renderer(m_display, 1, m_renderer);
    if (!m_shm) {
        wlr_bridge_display_init_shm(m_display);
    }
    std::cout << "[EmbeddedCompositor] wl_shm interface registered." << std::endl;

    m_linuxDmabuf = wlr_bridge_linux_dmabuf_create(m_display, 4, m_renderer);
    if (!m_linuxDmabuf) {
        std::cerr << "[EmbeddedCompositor] Warning: Failed to create linux-dmabuf-v1 global." << std::endl;
    } else {
        std::cout << "[EmbeddedCompositor] linux-dmabuf-v1 interface initialized for zero-copy DMA-BUF import." << std::endl;
    }

    // 6. Presentation Time Protocol for VSync frame synchronization feedback
    m_presentation = wlr_bridge_presentation_create(m_display, m_backend, 1);
    if (m_presentation) {
        std::cout << "[EmbeddedCompositor] wp_presentation interface registered for VSync presentation feedback." << std::endl;
    }

    // 7. Viewporter & Single Pixel Buffer Manager
    m_viewporter = wlr_bridge_viewporter_create(m_display);
    if (m_viewporter) {
        std::cout << "[EmbeddedCompositor] wp_viewporter interface registered." << std::endl;
    }
    m_singlePixelBuffer = wlr_bridge_single_pixel_buffer_create(m_display);

    // Fractional scale & Pointer constraints
    m_fractionalScale = wlr_bridge_fractional_scale_create(m_display, 1);
    if (m_fractionalScale) {
        std::cout << "[EmbeddedCompositor] wp_fractional_scale_manager_v1 interface registered." << std::endl;
    }
    m_pointerConstraints = wlr_bridge_pointer_constraints_create(m_display);
    if (m_pointerConstraints) {
        std::cout << "[EmbeddedCompositor] zwp_pointer_constraints_v1 interface registered." << std::endl;
    }

    // 8. XDG Shell v3 interface
    m_xdgShell = wlr_bridge_xdg_shell_create(m_display, 3);
    m_newXdgSurfaceListener = wlr_bridge_xdg_shell_add_new_surface_listener(
        m_xdgShell,
        [](void *user, struct wlr_xdg_surface *xdgSurface) {
            static_cast<EmbeddedCompositor*>(user)->handleNewXdgSurface(xdgSurface);
        },
        this
    );
    m_newXdgToplevelListener = wlr_bridge_xdg_shell_add_new_toplevel_listener(
        m_xdgShell,
        [](void *user, struct wlr_xdg_toplevel *toplevel) {
            static_cast<EmbeddedCompositor*>(user)->handleNewXdgToplevel(toplevel);
        },
        this
    );

    // 9. Wayland input seat (multi-touch, pointer, keyboard)
    m_seat = wlr_bridge_seat_create(m_display, "linstack-seat-0");
    wlr_bridge_seat_set_capabilities(m_seat,
                                    WLR_BRIDGE_SEAT_CAP_POINTER |
                                    WLR_BRIDGE_SEAT_CAP_KEYBOARD |
                                    WLR_BRIDGE_SEAT_CAP_TOUCH);

    // Initialize virtual keyboard directly via C bridge
    m_virtualKeyboard = wlr_bridge_virtual_keyboard_create(m_seat);

    // 10. Relative pointer manager for FPS mouse look
    m_relativePointerManager = wlr_bridge_relative_pointer_manager_create(m_display);

    // 11. Bind dedicated Wayland display socket
    const char *xdgRuntime = getenv("XDG_RUNTIME_DIR");
    if (xdgRuntime) {
        std::string sock = std::string(xdgRuntime) + "/" + m_config.socketName;
        std::string lock = sock + ".lock";
        unlink(sock.c_str());
        unlink(lock.c_str());
    }

    int res = wlr_bridge_display_add_socket(m_display, m_config.socketName.c_str());
    if (res != 0) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to bind socket: " << m_config.socketName << std::endl;
        return false;
    }

    std::cout << "[EmbeddedCompositor] Successfully listening on Wayland socket: " << m_config.socketName << std::endl;
    return true;
}

bool EmbeddedCompositor::start() {
    if (!m_backend) return false;
    if (!wlr_bridge_backend_start(m_backend)) {
        std::cerr << "[EmbeddedCompositor] Error: Failed to start wlroots backend." << std::endl;
        return false;
    }
    std::cout << "[EmbeddedCompositor] Headless backend running successfully." << std::endl;
    return true;
}

int EmbeddedCompositor::dispatch(int timeoutMs) {
    if (!m_display || !m_eventLoop) return -1;
    wlr_bridge_display_flush_clients(m_display);
    int res = wlr_bridge_event_loop_dispatch(m_eventLoop, timeoutMs);
    wlr_bridge_display_flush_clients(m_display);
    return res;
}

void EmbeddedCompositor::shutdown() {
    if (!m_display) return;

    EGLDisplay dpy = getEglDisplay();
    if (dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    m_toplevelRecords.clear();
    m_surfaceRecords.clear();
    m_rawSurfaceRecords.clear();
    m_activeSurface = nullptr;
    m_activeToplevel = nullptr;

    if (m_clientCreatedListener) {
        wlr_bridge_listener_destroy(m_clientCreatedListener);
        m_clientCreatedListener = nullptr;
    }
    if (m_newSurfaceListener) {
        wlr_bridge_listener_destroy(m_newSurfaceListener);
        m_newSurfaceListener = nullptr;
    }
    if (m_newXdgSurfaceListener) {
        wlr_bridge_listener_destroy(m_newXdgSurfaceListener);
        m_newXdgSurfaceListener = nullptr;
    }
    if (m_newXdgToplevelListener) {
        wlr_bridge_listener_destroy(m_newXdgToplevelListener);
        m_newXdgToplevelListener = nullptr;
    }

    if (m_virtualKeyboard) {
        wlr_bridge_virtual_keyboard_destroy(m_virtualKeyboard);
        m_virtualKeyboard = nullptr;
    }

    if (m_outputLayout) {
        wlr_bridge_output_layout_destroy(m_outputLayout);
        m_outputLayout = nullptr;
    }
    m_headlessOutput = nullptr;

    if (m_backend) {
        wlr_bridge_backend_destroy(m_backend);
        m_backend = nullptr;
    }

    if (m_display) {
        wlr_bridge_display_destroy_clients(m_display);
        wlr_bridge_display_destroy(m_display);
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
    wlr_bridge_client_get_credentials(client, &pid, &uid, &gid);
    std::cout << "[EmbeddedCompositor] >>> Wayland Client Connected: PID=" << pid
              << ", UID=" << uid << ", GID=" << gid << " <<<" << std::endl;
    if (onClientConnected) {
        onClientConnected(client);
    }
}

void EmbeddedCompositor::handleNewSurface(struct wlr_surface *surface) {
    if (!surface) return;
    std::cout << "[EmbeddedCompositor] New wlr_surface created (" << surface << ")" << std::endl;

    auto record = std::unique_ptr<struct wlr_bridge_raw_surface_record, void(*)(struct wlr_bridge_raw_surface_record*)>(
        wlr_bridge_raw_surface_record_create(
            surface,
            [](void *user, struct wlr_surface *s) {
                static_cast<EmbeddedCompositor*>(user)->handleSurfaceCommit(s);
            },
            [](void *user, struct wlr_surface *s) {
                static_cast<EmbeddedCompositor*>(user)->handleRawSurfaceDestroy(s);
            },
            this
        ),
        wlr_bridge_raw_surface_record_destroy
    );

    if (record) {
        m_rawSurfaceRecords.push_back(std::move(record));
    }
}

void EmbeddedCompositor::handleRawSurfaceDestroy(struct wlr_surface *surface) {
    if (!surface) return;
    std::cout << "[EmbeddedCompositor] wlr_surface destroyed (" << surface << ")" << std::endl;
    if (m_activeSurface == surface) {
        m_activeSurface = nullptr;
        for (const auto &rec : m_rawSurfaceRecords) {
            struct wlr_surface *s = wlr_bridge_raw_surface_record_get_surface(rec.get());
            if (s && s != surface && wlr_bridge_surface_has_buffer(s)) {
                m_activeSurface = s;
                break;
            }
        }
        if (onSurfaceUnmapped) {
            onSurfaceUnmapped(surface);
        }
    }

    auto it = std::remove_if(m_rawSurfaceRecords.begin(), m_rawSurfaceRecords.end(),
                             [surface](const auto &rec) {
                                 return wlr_bridge_raw_surface_record_get_surface(rec.get()) == surface;
                             });
    m_rawSurfaceRecords.erase(it, m_rawSurfaceRecords.end());
}

struct wlr_surface *EmbeddedCompositor::getRenderableSurface() const {
    struct wlr_surface *bestSurface = nullptr;
    int maxArea = 0;

    for (const auto &rec : m_rawSurfaceRecords) {
        struct wlr_surface *s = wlr_bridge_raw_surface_record_get_surface(rec.get());
        if (s && (wlr_bridge_surface_has_buffer(s) || wlr_bridge_surface_get_texture(s))) {
            int w = 0, h = 0;
            wlr_bridge_surface_get_size(s, &w, &h);
            int area = w * h;
            if (area > maxArea) {
                maxArea = area;
                bestSurface = s;
            }
        }
    }
    if (bestSurface) return bestSurface;
    return m_activeSurface;
}

void EmbeddedCompositor::handleNewXdgSurface(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;

    struct wlr_surface *surf = wlr_bridge_xdg_surface_get_surface(xdgSurface);
    std::cout << "[EmbeddedCompositor] Incoming XDG surface created (surface: "
              << surf << ") — awaiting role assignment" << std::endl;

    auto record = std::unique_ptr<struct wlr_bridge_xdg_surface_record, void(*)(struct wlr_bridge_xdg_surface_record*)>(
        wlr_bridge_xdg_surface_record_create(
            xdgSurface,
            [](void *user, struct wlr_xdg_surface *xs) {
                static_cast<EmbeddedCompositor*>(user)->handleXdgSurfaceMap(xs);
            },
            [](void *user, struct wlr_xdg_surface *xs) {
                static_cast<EmbeddedCompositor*>(user)->handleXdgSurfaceUnmap(xs);
            },
            [](void *user, struct wlr_xdg_surface *xs) {
                static_cast<EmbeddedCompositor*>(user)->handleXdgSurfaceDestroy(xs);
            },
            [](void *user, struct wlr_xdg_surface *xs) {
                EmbeddedCompositor *comp = static_cast<EmbeddedCompositor*>(user);
                struct wlr_surface *s = wlr_bridge_xdg_surface_get_surface(xs);
                struct wlr_xdg_toplevel *xdgToplevel = wlr_bridge_xdg_toplevel_try_from_surface(s);
                if (xdgToplevel && wlr_bridge_xdg_toplevel_is_initial_commit(xdgToplevel)) {
                    wlr_bridge_xdg_toplevel_set_size(xdgToplevel, comp->m_config.defaultWidth, comp->m_config.defaultHeight);
                    wlr_bridge_xdg_toplevel_set_activated(xdgToplevel, true);
                    wlr_bridge_xdg_toplevel_set_fullscreen(xdgToplevel, true);
                    std::cout << "[EmbeddedCompositor] initial_commit: configure "
                              << comp->m_config.defaultWidth << "x" << comp->m_config.defaultHeight
                              << " fullscreen+activated -> toplevel " << xdgToplevel << std::endl;
                }
                comp->handleSurfaceCommit(s);
            },
            this
        ),
        wlr_bridge_xdg_surface_record_destroy
    );

    if (record) {
        m_surfaceRecords.push_back(std::move(record));
    }
}

void EmbeddedCompositor::handleXdgSurfaceMap(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;
    struct wlr_surface *surf = wlr_bridge_xdg_surface_get_surface(xdgSurface);
    if (!surf) return;

    m_activeSurface = surf;
    struct wlr_xdg_toplevel *toplevel = wlr_bridge_xdg_toplevel_try_from_surface(surf);
    int surfW = 0, surfH = 0;
    wlr_bridge_surface_get_size(surf, &surfW, &surfH);

    if (toplevel && (surfW != m_config.defaultWidth || surfH != m_config.defaultHeight)) {
        wlr_bridge_xdg_toplevel_set_size(toplevel, m_config.defaultWidth, m_config.defaultHeight);
        wlr_bridge_xdg_surface_schedule_configure(xdgSurface);
    }
    std::cout << "[EmbeddedCompositor] Android Wayland surface mapped ("
              << surfW << "x" << surfH << ", surface: " << surf << ")" << std::endl;

    if (onSurfaceMapped) {
        onSurfaceMapped(m_activeSurface);
    }
}

void EmbeddedCompositor::handleXdgSurfaceUnmap(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;
    struct wlr_surface *surf = wlr_bridge_xdg_surface_get_surface(xdgSurface);
    std::cout << "[EmbeddedCompositor] Android Wayland surface unmapped (" << surf << ")" << std::endl;
    if (m_activeSurface == surf) {
        m_activeSurface = nullptr;
    }
    if (onSurfaceUnmapped) {
        onSurfaceUnmapped(surf);
    }
}

void EmbeddedCompositor::handleXdgSurfaceDestroy(struct wlr_xdg_surface *xdgSurface) {
    if (!xdgSurface) return;
    struct wlr_surface *surf = wlr_bridge_xdg_surface_get_surface(xdgSurface);
    struct wlr_xdg_toplevel *top = wlr_bridge_xdg_surface_get_toplevel(xdgSurface);

    if (m_activeSurface == surf) {
        m_activeSurface = nullptr;
    }
    if (m_activeToplevel == top) {
        m_activeToplevel = nullptr;
    }

    auto it = std::remove_if(m_surfaceRecords.begin(), m_surfaceRecords.end(),
                             [xdgSurface](const auto &rec) {
                                 return wlr_bridge_xdg_surface_record_get_xdg_surface(rec.get()) == xdgSurface;
                             });
    m_surfaceRecords.erase(it, m_surfaceRecords.end());
}

void EmbeddedCompositor::handleNewXdgToplevel(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel) return;

    struct wlr_surface *surf = wlr_bridge_xdg_toplevel_get_surface(toplevel);
    std::cout << "[EmbeddedCompositor] >>> New XDG toplevel (role assigned): "
              << toplevel << " surface=" << surf << std::endl;

    m_activeToplevel = toplevel;

    auto record = std::unique_ptr<struct wlr_bridge_xdg_toplevel_record, void(*)(struct wlr_bridge_xdg_toplevel_record*)>(
        wlr_bridge_xdg_toplevel_record_create(
            toplevel,
            [](void *user, struct wlr_xdg_toplevel *tl) {
                (void)user;
                if (tl && wlr_bridge_xdg_toplevel_is_initialized(tl)) {
                    wlr_bridge_xdg_toplevel_set_maximized(tl, true);
                    wlr_bridge_xdg_surface_schedule_configure(wlr_bridge_xdg_toplevel_get_base(tl));
                }
            },
            [](void *user, struct wlr_xdg_toplevel *tl) {
                (void)user;
                if (tl && wlr_bridge_xdg_toplevel_is_initialized(tl)) {
                    wlr_bridge_xdg_toplevel_set_fullscreen(tl, true);
                    wlr_bridge_xdg_surface_schedule_configure(wlr_bridge_xdg_toplevel_get_base(tl));
                    std::cout << "[EmbeddedCompositor] Granted fullscreen to XDG toplevel" << std::endl;
                }
            },
            [](void *user, struct wlr_xdg_toplevel *tl) {
                (void)user;
                if (tl && wlr_bridge_xdg_toplevel_is_initialized(tl)) {
                    wlr_bridge_xdg_surface_schedule_configure(wlr_bridge_xdg_toplevel_get_base(tl));
                }
            },
            [](void *user, struct wlr_xdg_toplevel *tl) {
                EmbeddedCompositor *comp = static_cast<EmbeddedCompositor*>(user);
                if (comp->m_activeToplevel == tl) {
                    comp->m_activeToplevel = nullptr;
                }
                auto it = std::remove_if(comp->m_toplevelRecords.begin(), comp->m_toplevelRecords.end(),
                                         [tl](const auto &r) {
                                             return wlr_bridge_xdg_toplevel_record_get_toplevel(r.get()) == tl;
                                         });
                comp->m_toplevelRecords.erase(it, comp->m_toplevelRecords.end());
                std::cout << "[EmbeddedCompositor] XDG toplevel destroyed" << std::endl;
            },
            this
        ),
        wlr_bridge_xdg_toplevel_record_destroy
    );

    if (record) {
        m_toplevelRecords.push_back(std::move(record));
    }
}

void EmbeddedCompositor::handleSurfaceCommit(struct wlr_surface *surface) {
    if (!surface) return;

    bool hasBuf = wlr_bridge_surface_has_buffer(surface);
    int surfW = 0, surfH = 0;
    wlr_bridge_surface_get_size(surface, &surfW, &surfH);

    if (!m_activeSurface && (hasBuf || surfW > 200)) {
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
