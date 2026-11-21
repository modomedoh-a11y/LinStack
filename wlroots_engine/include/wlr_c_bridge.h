#ifndef WLR_C_BRIDGE_H
#define WLR_C_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#ifndef EGL_NO_PLATFORM_SPECIFIC_TYPES
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Opaque forward declarations of Wayland & wlroots types for C++20
 * ------------------------------------------------------------------------- */
struct wl_display;
struct wl_event_loop;
struct wl_client;
struct wlr_backend;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_subcompositor;
struct wlr_data_device_manager;
struct wlr_shm;
struct wlr_linux_dmabuf_v1;
struct wlr_presentation;
struct wlr_viewporter;
struct wlr_xdg_output_manager_v1;
struct wlr_fractional_scale_manager_v1;
struct wlr_pointer_constraints_v1;
struct wlr_single_pixel_buffer_manager_v1;
struct wlr_linux_drm_syncobj_manager_v1;
struct wlr_xdg_shell;
struct wlr_seat;
struct wlr_relative_pointer_manager_v1;
struct wlr_output_layout;
struct wlr_output;
struct wlr_surface;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_texture;
struct wlr_keyboard;

/* -------------------------------------------------------------------------
 * Data structures for C/C++ cross-boundary transfer
 * ------------------------------------------------------------------------- */
enum wlr_bridge_seat_caps {
    WLR_BRIDGE_SEAT_CAP_POINTER  = (1 << 0),
    WLR_BRIDGE_SEAT_CAP_KEYBOARD = (1 << 1),
    WLR_BRIDGE_SEAT_CAP_TOUCH    = (1 << 2),
};

struct wlr_bridge_dmabuf_attribs {
    int32_t width;
    int32_t height;
    uint32_t format;
    uint64_t modifier;
    int n_planes;
    int offset[4];
    int stride[4];
    int fd[4];
};

/* -------------------------------------------------------------------------
 * Opaque Listener handle types
 * ------------------------------------------------------------------------- */
struct wlr_bridge_listener;
struct wlr_bridge_raw_surface_record;
struct wlr_bridge_xdg_surface_record;
struct wlr_bridge_xdg_toplevel_record;

/* Callback typedefs */
typedef void (*wlr_bridge_client_created_cb)(void *user, struct wl_client *client);
typedef void (*wlr_bridge_surface_cb)(void *user, struct wlr_surface *surface);
typedef void (*wlr_bridge_xdg_surface_cb)(void *user, struct wlr_xdg_surface *xdg_surface);
typedef void (*wlr_bridge_xdg_toplevel_cb)(void *user, struct wlr_xdg_toplevel *toplevel);

/* -------------------------------------------------------------------------
 * Display & Event Loop Lifecycle
 * ------------------------------------------------------------------------- */
struct wl_display    *wlr_bridge_display_create(void);
struct wl_event_loop *wlr_bridge_display_get_event_loop(struct wl_display *display);
int                   wlr_bridge_display_add_socket(struct wl_display *display, const char *socket_name);
void                  wlr_bridge_display_flush_clients(struct wl_display *display);
void                  wlr_bridge_display_destroy_clients(struct wl_display *display);
void                  wlr_bridge_display_destroy(struct wl_display *display);
int                   wlr_bridge_event_loop_dispatch(struct wl_event_loop *loop, int timeout_ms);
int                   wlr_bridge_event_loop_get_fd(struct wl_event_loop *loop);
void                  wlr_bridge_client_get_credentials(struct wl_client *client, pid_t *pid, uid_t *uid, gid_t *gid);

/* -------------------------------------------------------------------------
 * Headless Backend & Output Management
 * ------------------------------------------------------------------------- */
struct wlr_backend       *wlr_bridge_headless_backend_create(struct wl_event_loop *loop);
bool                      wlr_bridge_backend_start(struct wlr_backend *backend);
void                      wlr_bridge_backend_destroy(struct wlr_backend *backend);

struct wlr_output_layout *wlr_bridge_output_layout_create(struct wl_display *display);
void                      wlr_bridge_output_layout_destroy(struct wlr_output_layout *layout);
void                      wlr_bridge_output_layout_add_auto(struct wlr_output_layout *layout, struct wlr_output *output);

struct wlr_output        *wlr_bridge_headless_add_output(struct wlr_backend *backend, unsigned int width, unsigned int height);
bool                      wlr_bridge_output_init_render(struct wlr_output *output, struct wlr_allocator *allocator, struct wlr_renderer *renderer);
bool                      wlr_bridge_output_configure(struct wlr_output *output, int width, int height, int refresh_hz);
void                      wlr_bridge_output_create_global(struct wlr_output *output, struct wl_display *display);

/* -------------------------------------------------------------------------
 * Renderer, Allocator & EGL Integration
 * ------------------------------------------------------------------------- */
struct wlr_renderer  *wlr_bridge_gles2_renderer_create_with_drm_fd(int drm_fd);
struct wlr_renderer  *wlr_bridge_renderer_autocreate(struct wlr_backend *backend);
bool                  wlr_bridge_renderer_init_wl_display(struct wlr_renderer *renderer, struct wl_display *display);
struct wlr_allocator *wlr_bridge_allocator_autocreate(struct wlr_backend *backend, struct wlr_renderer *renderer);
EGLDisplay            wlr_bridge_get_egl_display(struct wlr_renderer *renderer);
EGLContext            wlr_bridge_get_egl_context(struct wlr_renderer *renderer);

/* -------------------------------------------------------------------------
 * Protocols & Managers
 * ------------------------------------------------------------------------- */
struct wlr_compositor                    *wlr_bridge_compositor_create(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer);
struct wlr_subcompositor                 *wlr_bridge_subcompositor_create(struct wl_display *display);
struct wlr_data_device_manager           *wlr_bridge_data_device_manager_create(struct wl_display *display);
struct wlr_shm                           *wlr_bridge_shm_create_with_renderer(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer);
void                                      wlr_bridge_display_init_shm(struct wl_display *display);
struct wlr_linux_dmabuf_v1               *wlr_bridge_linux_dmabuf_create(struct wl_display *display, uint32_t version, struct wlr_renderer *renderer);
struct wlr_presentation                  *wlr_bridge_presentation_create(struct wl_display *display, struct wlr_backend *backend, uint32_t version);
struct wlr_viewporter                    *wlr_bridge_viewporter_create(struct wl_display *display);
struct wlr_single_pixel_buffer_manager_v1 *wlr_bridge_single_pixel_buffer_create(struct wl_display *display);
struct wlr_xdg_output_manager_v1         *wlr_bridge_xdg_output_manager_create(struct wl_display *display, struct wlr_output_layout *layout);
struct wlr_fractional_scale_manager_v1   *wlr_bridge_fractional_scale_create(struct wl_display *display, uint32_t version);
struct wlr_pointer_constraints_v1        *wlr_bridge_pointer_constraints_create(struct wl_display *display);
struct wlr_xdg_shell                     *wlr_bridge_xdg_shell_create(struct wl_display *display, uint32_t version);
struct wlr_relative_pointer_manager_v1   *wlr_bridge_relative_pointer_manager_create(struct wl_display *display);

/* -------------------------------------------------------------------------
 * Seat & Input Management
 * ------------------------------------------------------------------------- */
struct wlr_seat      *wlr_bridge_seat_create(struct wl_display *display, const char *name);
void                  wlr_bridge_seat_set_capabilities(struct wlr_seat *seat, uint32_t caps);
struct wlr_keyboard  *wlr_bridge_virtual_keyboard_create(struct wlr_seat *seat);
void                  wlr_bridge_virtual_keyboard_destroy(struct wlr_keyboard *keyboard);

void wlr_bridge_seat_touch_notify_down(struct wlr_seat *seat, struct wlr_surface *surface,
                                       uint32_t time_ms, int32_t touch_id, double sx, double sy);
void wlr_bridge_seat_touch_notify_up(struct wlr_seat *seat, uint32_t time_ms, int32_t touch_id);
void wlr_bridge_seat_touch_notify_motion(struct wlr_seat *seat, uint32_t time_ms, int32_t touch_id, double sx, double sy);
void wlr_bridge_seat_touch_notify_frame(struct wlr_seat *seat);
void wlr_bridge_seat_keyboard_enter(struct wlr_seat *seat, struct wlr_surface *surface);
void wlr_bridge_seat_keyboard_notify_key(struct wlr_seat *seat, uint32_t time_ms, uint32_t key, uint32_t state);
void wlr_bridge_seat_keyboard_notify_modifiers(struct wlr_seat *seat);

/* -------------------------------------------------------------------------
 * Surface, Texture & DMA-BUF Queries
 * ------------------------------------------------------------------------- */
struct wlr_surface *wlr_bridge_surface_get_root_surface(struct wlr_surface *surface);
bool                wlr_bridge_surface_has_buffer(struct wlr_surface *surface);
void                wlr_bridge_surface_get_size(struct wlr_surface *surface, int *w, int *h);
struct wlr_texture *wlr_bridge_surface_get_texture(struct wlr_surface *surface);
void                wlr_bridge_surface_send_frame_done(struct wlr_surface *surface);
bool                wlr_bridge_surface_get_dmabuf(struct wlr_surface *surface, struct wlr_bridge_dmabuf_attribs *out);

bool wlr_bridge_texture_is_gles2(struct wlr_texture *texture);
void wlr_bridge_texture_get_size(struct wlr_texture *texture, int *w, int *h);
bool wlr_bridge_gles2_texture_get_attribs(struct wlr_texture *texture, uint32_t *target, uint32_t *tex);
bool wlr_bridge_texture_read_pixels(struct wlr_texture *texture, uint32_t drm_format, uint32_t stride, void *data);

/* -------------------------------------------------------------------------
 * XDG Surface & Toplevel Actions
 * ------------------------------------------------------------------------- */
struct wlr_surface      *wlr_bridge_xdg_surface_get_surface(struct wlr_xdg_surface *xdg_surface);
struct wlr_xdg_toplevel *wlr_bridge_xdg_surface_get_toplevel(struct wlr_xdg_surface *xdg_surface);
struct wlr_xdg_toplevel *wlr_bridge_xdg_toplevel_try_from_surface(struct wlr_surface *surface);
struct wlr_surface      *wlr_bridge_xdg_toplevel_get_surface(struct wlr_xdg_toplevel *toplevel);
struct wlr_xdg_surface  *wlr_bridge_xdg_toplevel_get_base(struct wlr_xdg_toplevel *toplevel);
bool                     wlr_bridge_xdg_toplevel_is_initial_commit(struct wlr_xdg_toplevel *toplevel);
bool                     wlr_bridge_xdg_toplevel_is_initialized(struct wlr_xdg_toplevel *toplevel);
void                     wlr_bridge_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel, int width, int height);
void                     wlr_bridge_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *toplevel, bool activated);
void                     wlr_bridge_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *toplevel, bool fullscreen);
void                     wlr_bridge_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *toplevel, bool maximized);
void                     wlr_bridge_xdg_surface_schedule_configure(struct wlr_xdg_surface *xdg_surface);

/* -------------------------------------------------------------------------
 * Event Listeners Registration & Tracking
 * ------------------------------------------------------------------------- */
struct wlr_bridge_listener *wlr_bridge_display_add_client_created_listener(struct wl_display *display,
                                                                          wlr_bridge_client_created_cb cb, void *user);
struct wlr_bridge_listener *wlr_bridge_compositor_add_new_surface_listener(struct wlr_compositor *compositor,
                                                                          wlr_bridge_surface_cb cb, void *user);
struct wlr_bridge_listener *wlr_bridge_xdg_shell_add_new_surface_listener(struct wlr_xdg_shell *shell,
                                                                          wlr_bridge_xdg_surface_cb cb, void *user);
struct wlr_bridge_listener *wlr_bridge_xdg_shell_add_new_toplevel_listener(struct wlr_xdg_shell *shell,
                                                                          wlr_bridge_xdg_toplevel_cb cb, void *user);
void                        wlr_bridge_listener_destroy(struct wlr_bridge_listener *listener);

struct wlr_bridge_raw_surface_record *wlr_bridge_raw_surface_record_create(
    struct wlr_surface *surface,
    void (*on_commit)(void *user, struct wlr_surface *surface),
    void (*on_destroy)(void *user, struct wlr_surface *surface),
    void *user
);
struct wlr_surface *wlr_bridge_raw_surface_record_get_surface(struct wlr_bridge_raw_surface_record *record);
void                wlr_bridge_raw_surface_record_destroy(struct wlr_bridge_raw_surface_record *record);

struct wlr_bridge_xdg_surface_record *wlr_bridge_xdg_surface_record_create(
    struct wlr_xdg_surface *xdg_surface,
    void (*on_map)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_unmap)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_destroy)(void *user, struct wlr_xdg_surface *xdg_surface),
    void (*on_commit)(void *user, struct wlr_xdg_surface *xdg_surface),
    void *user
);
struct wlr_xdg_surface *wlr_bridge_xdg_surface_record_get_xdg_surface(struct wlr_bridge_xdg_surface_record *record);
void                    wlr_bridge_xdg_surface_record_destroy(struct wlr_bridge_xdg_surface_record *record);

struct wlr_bridge_xdg_toplevel_record *wlr_bridge_xdg_toplevel_record_create(
    struct wlr_xdg_toplevel *toplevel,
    void (*on_request_maximize)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_request_fullscreen)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_request_minimize)(void *user, struct wlr_xdg_toplevel *toplevel),
    void (*on_destroy)(void *user, struct wlr_xdg_toplevel *toplevel),
    void *user
);
struct wlr_xdg_toplevel *wlr_bridge_xdg_toplevel_record_get_toplevel(struct wlr_bridge_xdg_toplevel_record *record);
void                     wlr_bridge_xdg_toplevel_record_destroy(struct wlr_bridge_xdg_toplevel_record *record);

#ifdef __cplusplus
}
#endif

#endif /* WLR_C_BRIDGE_H */
