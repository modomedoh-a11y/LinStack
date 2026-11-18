#ifndef WLROOTS_COMPAT_H
#define WLROOTS_COMPAT_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE 1
#endif

// EGL headers must precede wlroots headers to avoid redefinition issues
#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#ifndef EGL_NO_PLATFORM_SPECIFIC_TYPES
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

// DRM fourcc pixel format codes (needed for wlr_texture_read_pixels format arg)
#include <drm_fourcc.h>

#ifdef __cplusplus
#define static
extern "C" {
#endif

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/allocator.h>
#include <wlr/render/gles2.h>
#include <wlr/render/egl.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

#ifdef __cplusplus
}
#undef static
#endif

#endif // WLROOTS_COMPAT_H
