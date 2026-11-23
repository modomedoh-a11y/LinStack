#define _POSIX_C_SOURCE 200809L
#include "wlr_c_bridge.h"
#include "wlroots_compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#ifndef WLR_VERSION_MINOR
#define WLR_VERSION_MINOR 20
#endif

/* -------------------------------------------------------------------------
 * Display & Event Loop Lifecycle
 * ------------------------------------------------------------------------- */

struct wl_display *wlr_bridge_display_create(void) {
    return wl_display_create();
}

struct wl_event_loop *wlr_bridge_display_get_event_loop(struct wl_display *display) {
    if (!display) return NULL;
    return wl_display_get_event_loop(display);
}

int wlr_bridge_display_add_socket(struct wl_display *display, const char *socket_name) {
    if (!display || !socket_name) return -1;
    return wl_display_add_socket(display, socket_name);
}

void wlr_bridge_display_flush_clients(struct wl_display *display) {
    if (display) {
        wl_display_flush_clients(display);
    }
}

void wlr_bridge_display_destroy_clients(struct wl_display *display) {
    if (display) {
        wl_display_destroy_clients(display);
    }
}

void wlr_bridge_display_destroy(struct wl_display *display) {
    if (display) {
        wl_display_destroy(display);
    }
}

int wlr_bridge_event_loop_dispatch(struct wl_event_loop *loop, int timeout_ms) {
    if (!loop) return -1;
    return wl_event_loop_dispatch(loop, timeout_ms);
}

int wlr_bridge_event_loop_get_fd(struct wl_event_loop *loop) {
    if (!loop) return -1;
    return wl_event_loop_get_fd(loop);
}

void wlr_bridge_client_get_credentials(struct wl_client *client, pid_t *pid, uid_t *uid, gid_t *gid) {
    if (client) {
        wl_client_get_credentials(client, pid, uid, gid);
    }
}

/* -------------------------------------------------------------------------
 * Headless Backend & Output Management
 * ------------------------------------------------------------------------- */

struct wlr_backend *wlr_bridge_headless_backend_create(struct wl_event_loop *loop) {
    if (!loop) return NULL;
    return wlr_headless_backend_create(loop);
}

bool wlr_bridge_backend_start(struct wlr_backend *backend) {
    if (!backend) return false;
    return wlr_backend_start(backend);
}

void wlr_bridge_backend_destroy(struct wlr_backend *backend) {
    if (backend) {
        wlr_backend_destroy(backend);
    }
}

struct wlr_output_layout *wlr_bridge_output_layout_create(struct wl_display *display) {
#if WLR_VERSION_MINOR >= 19
    return wlr_output_layout_create(display);
#else
    (void)display;
    return wlr_output_layout_create();
#endif
}

void wlr_bridge_output_layout_destroy(struct wlr_output_layout *layout) {
    if (layout) {
        wlr_output_layout_destroy(layout);
    }
}

void wlr_bridge_output_layout_add_auto(struct wlr_output_layout *layout, struct wlr_output *output) {
    if (layout && output) {
        wlr_output_layout_add_auto(layout, output);
    }
}

struct wlr_output *wlr_bridge_headless_add_output(struct wlr_backend *backend, unsigned int width, unsigned int height) {
    if (!backend) return NULL;
    return wlr_headless_add_output(backend, width, height);
}

bool wlr_bridge_output_init_render(struct wlr_output *output, struct wlr_allocator *allocator, struct wlr_renderer *renderer) {
    if (!output || !allocator || !renderer) return false;
    return wlr_output_init_render(output, allocator, renderer);
}

bool wlr_bridge_output_configure(struct wlr_output *output, int width, int height, int refresh_hz) {
    if (!output) return false;
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    int mhz = (refresh_hz > 0 ? refresh_hz : 60) * 1000;
    wlr_output_state_set_custom_mode(&state, width, height, mhz);
    bool ok = wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
    return ok;
}

void wlr_bridge_output_create_global(struct wlr_output *output, struct wl_display *display) {
#if WLR_VERSION_MINOR >= 19
    if (output && display) {
        wlr_output_create_global(output, display);
    }
#else
    (void)display;
    if (output) {
        wlr_output_create_global(output);
    }
#endif
}

/* -------------------------------------------------------------------------
 * Renderer, Allocator & EGL Integration
 * ------------------------------------------------------------------------- */

struct wlr_renderer *wlr_bridge_gles2_renderer_create_with_drm_fd(int drm_fd) {
    if (drm_fd < 0) return NULL;
    return wlr_gles2_renderer_create_with_drm_fd(drm_fd);
}

struct wlr_renderer *wlr_bridge_renderer_autocreate(struct wlr_backend *backend) {
    if (!backend) return NULL;
    return wlr_renderer_autocreate(backend);
}

bool wlr_bridge_renderer_init_wl_display(struct wlr_renderer *renderer, struct wl_display *display) {
    if (!renderer || !display) return false;
    return wlr_renderer_init_wl_display(renderer, display);
}

struct wlr_allocator *wlr_bridge_allocator_autocreate(struct wlr_backend *backend, struct wlr_renderer *renderer) {
    if (!backend || !renderer) return NULL;
    return wlr_allocator_autocreate(backend, renderer);
}

EGLDisplay wlr_bridge_get_egl_display(struct wlr_renderer *renderer) {
    if (!renderer) return EGL_NO_DISPLAY;
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl) return EGL_NO_DISPLAY;
    return wlr_egl_get_display(egl);
}

EGLContext wlr_bridge_get_egl_context(struct wlr_renderer *renderer) {
    if (!renderer) return EGL_NO_CONTEXT;
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl) return EGL_NO_CONTEXT;
    return wlr_egl_get_context(egl);
}

/* -------------------------------------------------------------------------
 * Protocols & Managers
 * ------------------------------------------------------------------------- */

struct wlr_compositor *wlr_bridge_compositor_create(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer) {
    if (!display || !renderer) return NULL;
    return wlr_compositor_create(display, version, renderer);
}

struct wlr_subcompositor *wlr_bridge_subcompositor_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_subcompositor_create(display);
}

struct wlr_data_device_manager *wlr_bridge_data_device_manager_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_data_device_manager_create(display);
}

struct wlr_shm *wlr_bridge_shm_create_with_renderer(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer) {
    if (!display || !renderer) return NULL;
    return wlr_shm_create_with_renderer(display, version, renderer);
}

void wlr_bridge_display_init_shm(struct wl_display *display) {
    if (display) {
        wl_display_init_shm(display);
    }
}

struct wlr_linux_dmabuf_v1 *wlr_bridge_linux_dmabuf_create(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer) {
    if (!display || !renderer) return NULL;
    return wlr_linux_dmabuf_v1_create_with_renderer(display, version, renderer);
}

struct wlr_presentation *wlr_bridge_presentation_create(struct wl_display *display, struct wlr_backend *backend, uint32_t version) {
    if (!display || !backend) return NULL;
#if WLR_VERSION_MINOR >= 19
    return wlr_presentation_create(display, backend, version);
#else
    (void)version;
    return wlr_presentation_create(display, backend);
#endif
}

struct wlr_viewporter *wlr_bridge_viewporter_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_viewporter_create(display);
}

struct wlr_single_pixel_buffer_manager_v1 *wlr_bridge_single_pixel_buffer_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_single_pixel_buffer_manager_v1_create(display);
}

struct wlr_xdg_output_manager_v1 *wlr_bridge_xdg_output_manager_create(struct wl_display *display, struct wlr_output_layout *layout) {
    if (!display || !layout) return NULL;
    return wlr_xdg_output_manager_v1_create(display, layout);
}

struct wlr_fractional_scale_manager_v1 *wlr_bridge_fractional_scale_create(struct wl_display *display, uint32_t version) {
    if (!display) return NULL;
    return wlr_fractional_scale_manager_v1_create(display, version);
}

struct wlr_pointer_constraints_v1 *wlr_bridge_pointer_constraints_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_pointer_constraints_v1_create(display);
}

struct wlr_xdg_shell *wlr_bridge_xdg_shell_create(struct wl_display *display, uint32_t version) {
    if (!display) return NULL;
    return wlr_xdg_shell_create(display, version);
}

struct wlr_relative_pointer_manager_v1 *wlr_bridge_relative_pointer_manager_create(struct wl_display *display) {
    if (!display) return NULL;
    return wlr_relative_pointer_manager_v1_create(display);
}

/* -------------------------------------------------------------------------
 * Seat & Input Management
 * ------------------------------------------------------------------------- */

struct wlr_seat *wlr_bridge_seat_create(struct wl_display *display, const char *name) {
    if (!display || !name) return NULL;
    return wlr_seat_create(display, name);
}

void wlr_bridge_seat_set_capabilities(struct wlr_seat *seat, uint32_t caps) {
    if (!seat) return;
    uint32_t wl_caps = 0;
    if (caps & WLR_BRIDGE_SEAT_CAP_POINTER)  wl_caps |= WL_SEAT_CAPABILITY_POINTER;
    if (caps & WLR_BRIDGE_SEAT_CAP_KEYBOARD) wl_caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (caps & WLR_BRIDGE_SEAT_CAP_TOUCH)    wl_caps |= WL_SEAT_CAPABILITY_TOUCH;
    wlr_seat_set_capabilities(seat, wl_caps);
}

struct wlr_keyboard *wlr_bridge_virtual_keyboard_create(struct wlr_seat *seat) {
    if (!seat) return NULL;
    struct wlr_keyboard *kbd = calloc(1, sizeof(*kbd));
    if (!kbd) return NULL;

    static const struct wlr_keyboard_impl virtual_kbd_impl = {
        .name = "linstack-virtual-kbd",
        .led_update = NULL
    };
#if WLR_VERSION_MINOR >= 18
    wlr_keyboard_init(kbd, &virtual_kbd_impl, "linstack-virtual-kbd");
#else
    wlr_keyboard_init(kbd, &virtual_kbd_impl);
#endif

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx) {
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (keymap) {
            wlr_keyboard_set_keymap(kbd, keymap);
            wlr_seat_set_keyboard(seat, kbd);
            xkb_keymap_unref(keymap);
        }
        xkb_context_unref(ctx);
    }
    return kbd;
}

void wlr_bridge_virtual_keyboard_destroy(struct wlr_keyboard *keyboard) {
    if (!keyboard) return;
    wlr_keyboard_finish(keyboard);
    free(keyboard);
}

void wlr_bridge_seat_touch_notify_down(struct wlr_seat *seat, struct wlr_surface *surface,
                                       uint32_t time_ms, int32_t touch_id, double sx, double sy) {
    if (seat && surface) {
        wlr_seat_touch_notify_down(seat, surface, time_ms, touch_id, sx, sy);
    }
}

void wlr_bridge_seat_touch_notify_up(struct wlr_seat *seat, uint32_t time_ms, int32_t touch_id) {
    if (seat) {
        wlr_seat_touch_notify_up(seat, time_ms, touch_id);
    }
}

void wlr_bridge_seat_touch_notify_motion(struct wlr_seat *seat, uint32_t time_ms, int32_t touch_id, double sx, double sy) {
    if (seat) {
        wlr_seat_touch_notify_motion(seat, time_ms, touch_id, sx, sy);
    }
}

void wlr_bridge_seat_touch_notify_frame(struct wlr_seat *seat) {
    if (seat) {
        wlr_seat_touch_notify_frame(seat);
    }
}

void wlr_bridge_seat_keyboard_enter(struct wlr_seat *seat, struct wlr_surface *surface) {
    if (!seat || !surface) return;
    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(seat);
    if (kbd) {
        wlr_seat_keyboard_notify_enter(seat, surface, kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
    } else {
        wlr_seat_keyboard_notify_enter(seat, surface, NULL, 0, NULL);
    }
}

void wlr_bridge_seat_keyboard_notify_key(struct wlr_seat *seat, uint32_t time_ms, uint32_t key, uint32_t state) {
    if (seat) {
        wlr_seat_keyboard_notify_key(seat, time_ms, key, state);
    }
}

void wlr_bridge_seat_keyboard_notify_modifiers(struct wlr_seat *seat) {
    if (seat) {
        struct wlr_keyboard *kbd = wlr_seat_get_keyboard(seat);
        if (kbd) {
            wlr_seat_keyboard_notify_modifiers(seat, &kbd->modifiers);
        } else {
            wlr_seat_keyboard_notify_modifiers(seat, NULL);
        }
    }
}

/* -------------------------------------------------------------------------
 * Surface, Texture & DMA-BUF Queries
 * ------------------------------------------------------------------------- */

struct wlr_surface *wlr_bridge_surface_get_root_surface(struct wlr_surface *surface) {
    if (!surface) return NULL;
    return wlr_surface_get_root_surface(surface);
}

bool wlr_bridge_surface_has_buffer(struct wlr_surface *surface) {
    if (!surface) return false;
    return wlr_surface_has_buffer(surface);
}

void wlr_bridge_surface_get_size(struct wlr_surface *surface, int *w, int *h) {
    if (!surface) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = surface->current.width;
    if (h) *h = surface->current.height;
}

struct wlr_texture *wlr_bridge_surface_get_texture(struct wlr_surface *surface) {
    if (!surface) return NULL;
    return wlr_surface_get_texture(surface);
}

void wlr_bridge_surface_send_frame_done(struct wlr_surface *surface) {
    if (!surface) return;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(surface, &now);
}

bool wlr_bridge_surface_get_dmabuf(struct wlr_surface *surface, struct wlr_bridge_dmabuf_attribs *out) {
    if (!surface || !surface->buffer || !out) return false;
    struct wlr_dmabuf_attributes dmabuf = {0};
    if (!wlr_buffer_get_dmabuf(&surface->buffer->base, &dmabuf)) {
        return false;
    }
    out->width = dmabuf.width;
    out->height = dmabuf.height;
    out->format = dmabuf.format;
    out->modifier = dmabuf.modifier;
    out->n_planes = dmabuf.n_planes;
    for (int i = 0; i < dmabuf.n_planes && i < 4; ++i) {
        out->offset[i] = (int)dmabuf.offset[i];
        out->stride[i] = (int)dmabuf.stride[i];
        out->fd[i] = dmabuf.fd[i];
    }
    return true;
}

bool wlr_bridge_texture_is_gles2(struct wlr_texture *texture) {
    if (!texture) return false;
    return wlr_texture_is_gles2(texture);
}

void wlr_bridge_texture_get_size(struct wlr_texture *texture, int *w, int *h) {
    if (!texture) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = (int)texture->width;
    if (h) *h = (int)texture->height;
}

bool wlr_bridge_gles2_texture_get_attribs(struct wlr_texture *texture, uint32_t *target, uint32_t *tex) {
    if (!texture || !wlr_texture_is_gles2(texture)) return false;
    struct wlr_gles2_texture_attribs attribs;
    wlr_gles2_texture_get_attribs(texture, &attribs);
    if (target) *target = attribs.target;
    if (tex) *tex = attribs.tex;
    return true;
}

bool wlr_bridge_texture_read_pixels(struct wlr_texture *texture, uint32_t drm_format, uint32_t stride, void *data) {
    if (!texture || !data) return false;
    struct wlr_texture_read_pixels_options opts = {
        .format = drm_format,
        .stride = stride,
        .data = data
    };
    return wlr_texture_read_pixels(texture, &opts);
}

/* -------------------------------------------------------------------------
 * XDG Surface & Toplevel Actions
 * ------------------------------------------------------------------------- */

struct wlr_surface *wlr_bridge_xdg_surface_get_surface(struct wlr_xdg_surface *xdg_surface) {
    if (!xdg_surface) return NULL;
    return xdg_surface->surface;
}

struct wlr_xdg_toplevel *wlr_bridge_xdg_surface_get_toplevel(struct wlr_xdg_surface *xdg_surface) {
    if (!xdg_surface) return NULL;
    return xdg_surface->toplevel;
}

struct wlr_xdg_toplevel *wlr_bridge_xdg_toplevel_try_from_surface(struct wlr_surface *surface) {
    if (!surface) return NULL;
    return wlr_xdg_toplevel_try_from_wlr_surface(surface);
}

struct wlr_surface *wlr_bridge_xdg_toplevel_get_surface(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel || !toplevel->base) return NULL;
    return toplevel->base->surface;
}

struct wlr_xdg_surface *wlr_bridge_xdg_toplevel_get_base(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel) return NULL;
    return toplevel->base;
}

bool wlr_bridge_xdg_toplevel_is_initial_commit(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel || !toplevel->base) return false;
    return toplevel->base->initial_commit;
}

bool wlr_bridge_xdg_toplevel_is_initialized(struct wlr_xdg_toplevel *toplevel) {
    if (!toplevel || !toplevel->base) return false;
    return toplevel->base->initialized;
}

void wlr_bridge_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel, int width, int height) {
    if (toplevel) {
        wlr_xdg_toplevel_set_size(toplevel, (uint32_t)width, (uint32_t)height);
    }
}

void wlr_bridge_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *toplevel, bool activated) {
    if (toplevel) {
        wlr_xdg_toplevel_set_activated(toplevel, activated);
    }
}

void wlr_bridge_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *toplevel, bool fullscreen) {
    if (toplevel) {
        wlr_xdg_toplevel_set_fullscreen(toplevel, fullscreen);
    }
}

void wlr_bridge_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *toplevel, bool maximized) {
    if (toplevel) {
        wlr_xdg_toplevel_set_maximized(toplevel, maximized);
    }
}

void wlr_bridge_xdg_surface_schedule_configure(struct wlr_xdg_surface *xdg_surface) {
    if (xdg_surface) {
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
}

/* -------------------------------------------------------------------------
 * Event Listeners Registration & Tracking
 * ------------------------------------------------------------------------- */

struct wlr_bridge_listener {
    struct wl_listener listener;
    void (*callback)(void *user, void *data);
    void *user;
};

static void generic_listener_notify(struct wl_listener *listener, void *data) {
    struct wlr_bridge_listener *bl = wl_container_of(listener, bl, listener);
    if (bl && bl->callback) {
        bl->callback(bl->user, data);
    }
}

struct wlr_bridge_listener *wlr_bridge_display_add_client_created_listener(struct wl_display *display,
                                                                          wlr_bridge_client_created_cb cb, void *user) {
    if (!display || !cb) return NULL;
    struct wlr_bridge_listener *bl = calloc(1, sizeof(*bl));
    if (!bl) return NULL;
    bl->callback = (void (*)(void *, void *))cb;
    bl->user = user;
    bl->listener.notify = generic_listener_notify;
    wl_list_init(&bl->listener.link);
    wl_display_add_client_created_listener(display, &bl->listener);
    return bl;
}

struct wlr_bridge_listener *wlr_bridge_compositor_add_new_surface_listener(struct wlr_compositor *compositor,
                                                                          wlr_bridge_surface_cb cb, void *user) {
    if (!compositor || !cb) return NULL;
    struct wlr_bridge_listener *bl = calloc(1, sizeof(*bl));
    if (!bl) return NULL;
    bl->callback = (void (*)(void *, void *))cb;
    bl->user = user;
    bl->listener.notify = generic_listener_notify;
    wl_list_init(&bl->listener.link);
    wl_signal_add(&compositor->events.new_surface, &bl->listener);
    return bl;
}

struct wlr_bridge_listener *wlr_bridge_xdg_shell_add_new_surface_listener(struct wlr_xdg_shell *shell,
                                                                          wlr_bridge_xdg_surface_cb cb, void *user) {
    if (!shell || !cb) return NULL;
    struct wlr_bridge_listener *bl = calloc(1, sizeof(*bl));
    if (!bl) return NULL;
    bl->callback = (void (*)(void *, void *))cb;
    bl->user = user;
    bl->listener.notify = generic_listener_notify;
    wl_list_init(&bl->listener.link);
    wl_signal_add(&shell->events.new_surface, &bl->listener);
    return bl;
}

struct wlr_bridge_listener *wlr_bridge_xdg_shell_add_new_toplevel_listener(struct wlr_xdg_shell *shell,
                                                                          wlr_bridge_xdg_toplevel_cb cb, void *user) {
    if (!shell || !cb) return NULL;
    struct wlr_bridge_listener *bl = calloc(1, sizeof(*bl));
    if (!bl) return NULL;
    bl->callback = (void (*)(void *, void *))cb;
    bl->user = user;
    bl->listener.notify = generic_listener_notify;
    wl_list_init(&bl->listener.link);
    wl_signal_add(&shell->events.new_toplevel, &bl->listener);
    return bl;
}

void wlr_bridge_listener_destroy(struct wlr_bridge_listener *listener) {
    if (!listener) return;
    if (listener->listener.link.prev && listener->listener.link.next) {
        wl_list_remove(&listener->listener.link);
    }
    free(listener);
}

/* --- Raw Surface Record --- */
struct wlr_bridge_raw_surface_record {
    struct wlr_surface *surface;
    void *user;
    void (*on_commit)(void *user, struct wlr_surface *surface);
    void (*on_destroy)(void *user, struct wlr_surface *surface);
    struct wl_listener commit_listener;
    struct wl_listener destroy_listener;
};

static void raw_surface_commit_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_raw_surface_record *rec =
        wl_container_of(listener, rec, commit_listener);
    if (rec && rec->on_commit) {
        rec->on_commit(rec->user, rec->surface);
    }
}

static void raw_surface_destroy_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_raw_surface_record *rec =
        wl_container_of(listener, rec, destroy_listener);
    if (rec) {
        if (rec->destroy_listener.link.prev && rec->destroy_listener.link.next) {
            wl_list_remove(&rec->destroy_listener.link);
            wl_list_init(&rec->destroy_listener.link);
        }
        if (rec->on_destroy) {
            rec->on_destroy(rec->user, rec->surface);
        }
    }
}

struct wlr_bridge_raw_surface_record *wlr_bridge_raw_surface_record_create(
    struct wlr_surface *surface,
    void (*on_commit)(void *user, struct wlr_surface *surface),
    void (*on_destroy)(void *user, struct wlr_surface *surface),
    void *user
) {
    if (!surface) return NULL;
    struct wlr_bridge_raw_surface_record *rec = calloc(1, sizeof(*rec));
    if (!rec) return NULL;
    rec->surface = surface;
    rec->user = user;
    rec->on_commit = on_commit;
    rec->on_destroy = on_destroy;

    rec->commit_listener.notify = raw_surface_commit_notify;
    wl_list_init(&rec->commit_listener.link);
    wl_signal_add(&surface->events.commit, &rec->commit_listener);

    rec->destroy_listener.notify = raw_surface_destroy_notify;
    wl_list_init(&rec->destroy_listener.link);
    wl_signal_add(&surface->events.destroy, &rec->destroy_listener);

    return rec;
}

struct wlr_surface *wlr_bridge_raw_surface_record_get_surface(struct wlr_bridge_raw_surface_record *record) {
    return record ? record->surface : NULL;
}

void wlr_bridge_raw_surface_record_destroy(struct wlr_bridge_raw_surface_record *record) {
    if (!record) return;
    if (record->commit_listener.link.prev && record->commit_listener.link.next) {
        wl_list_remove(&record->commit_listener.link);
    }
    if (record->destroy_listener.link.prev && record->destroy_listener.link.next) {
        wl_list_remove(&record->destroy_listener.link);
    }
    free(record);
}

/* --- XDG Surface Record --- */
struct wlr_bridge_xdg_surface_record {
    struct wlr_xdg_surface *xdg_surface;
    void *user;
    void (*on_map)(void *user, struct wlr_xdg_surface *xdg_surface);
    void (*on_unmap)(void *user, struct wlr_xdg_surface *xdg_surface);
    void (*on_destroy)(void *user, struct wlr_xdg_surface *xdg_surface);
    void (*on_commit)(void *user, struct wlr_xdg_surface *xdg_surface);
    struct wl_listener map_listener;
    struct wl_listener unmap_listener;
    struct wl_listener destroy_listener;
    struct wl_listener commit_listener;
};

static void xdg_surface_map_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_surface_record *rec = wl_container_of(listener, rec, map_listener);
    if (rec && rec->on_map) rec->on_map(rec->user, rec->xdg_surface);
}

static void xdg_surface_unmap_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_surface_record *rec = wl_container_of(listener, rec, unmap_listener);
    if (rec && rec->on_unmap) rec->on_unmap(rec->user, rec->xdg_surface);
}

static void xdg_surface_destroy_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_surface_record *rec = wl_container_of(listener, rec, destroy_listener);
    if (rec) {
        if (rec->destroy_listener.link.prev && rec->destroy_listener.link.next) {
            wl_list_remove(&rec->destroy_listener.link);
            wl_list_init(&rec->destroy_listener.link);
        }
        if (rec->on_destroy) rec->on_destroy(rec->user, rec->xdg_surface);
    }
}

static void xdg_surface_commit_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_surface_record *rec = wl_container_of(listener, rec, commit_listener);
    if (rec && rec->on_commit) rec->on_commit(rec->user, rec->xdg_surface);
}

struct wlr_bridge_xdg_surface_record *wlr_bridge_xdg_surface_record_create(
    struct wlr_xdg_surface *xdg_surface,
    void (*on_map)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_unmap)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_destroy)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_commit)(void *user, struct wlr_xdg_surface *xdg_surface),
    void *user
) {
    if (!xdg_surface || !xdg_surface->surface) return NULL;
    struct wlr_bridge_xdg_surface_record *rec = calloc(1, sizeof(*rec));
    if (!rec) return NULL;
    rec->xdg_surface = xdg_surface;
    rec->user = user;
    rec->on_map = on_map;
    rec->on_unmap = on_unmap;
    rec->on_destroy = on_destroy;
    rec->on_commit = on_commit;

    rec->map_listener.notify = xdg_surface_map_notify;
    wl_list_init(&rec->map_listener.link);
    wl_signal_add(&xdg_surface->surface->events.map, &rec->map_listener);

    rec->unmap_listener.notify = xdg_surface_unmap_notify;
    wl_list_init(&rec->unmap_listener.link);
    wl_signal_add(&xdg_surface->surface->events.unmap, &rec->unmap_listener);

    rec->destroy_listener.notify = xdg_surface_destroy_notify;
    wl_list_init(&rec->destroy_listener.link);
    wl_signal_add(&xdg_surface->events.destroy, &rec->destroy_listener);

    rec->commit_listener.notify = xdg_surface_commit_notify;
    wl_list_init(&rec->commit_listener.link);
    wl_signal_add(&xdg_surface->surface->events.commit, &rec->commit_listener);

    return rec;
}

struct wlr_xdg_surface *wlr_bridge_xdg_surface_record_get_xdg_surface(struct wlr_bridge_xdg_surface_record *record) {
    return record ? record->xdg_surface : NULL;
}

void wlr_bridge_xdg_surface_record_destroy(struct wlr_bridge_xdg_surface_record *record) {
    if (!record) return;
    if (record->map_listener.link.prev && record->map_listener.link.next) {
        wl_list_remove(&record->map_listener.link);
    }
    if (record->unmap_listener.link.prev && record->unmap_listener.link.next) {
        wl_list_remove(&record->unmap_listener.link);
    }
    if (record->destroy_listener.link.prev && record->destroy_listener.link.next) {
        wl_list_remove(&record->destroy_listener.link);
    }
    if (record->commit_listener.link.prev && record->commit_listener.link.next) {
        wl_list_remove(&record->commit_listener.link);
    }
    free(record);
}

/* --- XDG Toplevel Record --- */
struct wlr_bridge_xdg_toplevel_record {
    struct wlr_xdg_toplevel *toplevel;
    void *user;
    void (*on_request_maximize)(void *user, struct wlr_xdg_toplevel *toplevel);
    void (*on_request_fullscreen)(void *user, struct wlr_xdg_toplevel *toplevel);
    void (*on_request_minimize)(void *user, struct wlr_xdg_toplevel *toplevel);
    void (*on_destroy)(void *user, struct wlr_xdg_toplevel *toplevel);
    struct wl_listener request_maximize_listener;
    struct wl_listener request_fullscreen_listener;
    struct wl_listener request_minimize_listener;
    struct wl_listener destroy_listener;
};

static void xdg_toplevel_maximize_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_toplevel_record *rec = wl_container_of(listener, rec, request_maximize_listener);
    if (rec && rec->on_request_maximize) rec->on_request_maximize(rec->user, rec->toplevel);
}

static void xdg_toplevel_fullscreen_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_toplevel_record *rec = wl_container_of(listener, rec, request_fullscreen_listener);
    if (rec && rec->on_request_fullscreen) rec->on_request_fullscreen(rec->user, rec->toplevel);
}

static void xdg_toplevel_minimize_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_toplevel_record *rec = wl_container_of(listener, rec, request_minimize_listener);
    if (rec && rec->on_request_minimize) rec->on_request_minimize(rec->user, rec->toplevel);
}

static void xdg_toplevel_destroy_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct wlr_bridge_xdg_toplevel_record *rec = wl_container_of(listener, rec, destroy_listener);
    if (rec) {
        if (rec->destroy_listener.link.prev && rec->destroy_listener.link.next) {
            wl_list_remove(&rec->destroy_listener.link);
            wl_list_init(&rec->destroy_listener.link);
        }
        if (rec->on_destroy) rec->on_destroy(rec->user, rec->toplevel);
    }
}

struct wlr_bridge_xdg_toplevel_record *wlr_bridge_xdg_toplevel_record_create(
    struct wlr_xdg_toplevel *toplevel,
    void (*on_request_maximize)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_request_fullscreen)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_request_minimize)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_destroy)(void *user, struct wlr_xdg_toplevel *toplevel),
    void *user
) {
    if (!toplevel) return NULL;
    struct wlr_bridge_xdg_toplevel_record *rec = calloc(1, sizeof(*rec));
    if (!rec) return NULL;
    rec->toplevel = toplevel;
    rec->user = user;
    rec->on_request_maximize = on_request_maximize;
    rec->on_request_fullscreen = on_request_fullscreen;
    rec->on_request_minimize = on_request_minimize;
    rec->on_destroy = on_destroy;

    rec->request_maximize_listener.notify = xdg_toplevel_maximize_notify;
    wl_list_init(&rec->request_maximize_listener.link);
    wl_signal_add(&toplevel->events.request_maximize, &rec->request_maximize_listener);

    rec->request_fullscreen_listener.notify = xdg_toplevel_fullscreen_notify;
    wl_list_init(&rec->request_fullscreen_listener.link);
    wl_signal_add(&toplevel->events.request_fullscreen, &rec->request_fullscreen_listener);

    rec->request_minimize_listener.notify = xdg_toplevel_minimize_notify;
    wl_list_init(&rec->request_minimize_listener.link);
    wl_signal_add(&toplevel->events.request_minimize, &rec->request_minimize_listener);

    rec->destroy_listener.notify = xdg_toplevel_destroy_notify;
    wl_list_init(&rec->destroy_listener.link);
    wl_signal_add(&toplevel->events.destroy, &rec->destroy_listener);

    return rec;
}

struct wlr_xdg_toplevel *wlr_bridge_xdg_toplevel_record_get_toplevel(struct wlr_bridge_xdg_toplevel_record *record) {
    return record ? record->toplevel : NULL;
}

void wlr_bridge_xdg_toplevel_record_destroy(struct wlr_bridge_xdg_toplevel_record *record) {
    if (!record) return;
    if (record->request_maximize_listener.link.prev && record->request_maximize_listener.link.next) {
        wl_list_remove(&record->request_maximize_listener.link);
    }
    if (record->request_fullscreen_listener.link.prev && record->request_fullscreen_listener.link.next) {
        wl_list_remove(&record->request_fullscreen_listener.link);
    }
    if (record->request_minimize_listener.link.prev && record->request_minimize_listener.link.next) {
        wl_list_remove(&record->request_minimize_listener.link);
    }
    if (record->destroy_listener.link.prev && record->destroy_listener.link.next) {
        wl_list_remove(&record->destroy_listener.link);
    }
    free(record);
}
