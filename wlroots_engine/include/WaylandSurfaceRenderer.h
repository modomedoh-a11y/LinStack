#ifndef WAYLAND_SURFACE_RENDERER_H
#define WAYLAND_SURFACE_RENDERER_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE 1
#endif

#include <array>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// EGL headers for EGLImageKHR cross-context texture import
#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#ifndef EGL_NO_PLATFORM_SPECIFIC_TYPES
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wlr_c_bridge.h"

struct ViewportRect {
    int x = 0;
    int y = 0;
    int width = 1920;
    int height = 1080;
};

class WaylandSurfaceRenderer {
public:
    WaylandSurfaceRenderer();
    ~WaylandSurfaceRenderer();

    /**
     * @brief Initializes OpenGL shaders, quad VBOs, and GL state.
     *        Must be called with Qt's OpenGL context current (inside initializeGL).
     */
    bool initialize();

    /**
     * @brief [Legacy] Renders a wlr_surface directly — only valid when wlroots'
     *        EGL context is current. Kept for reference; use renderEglImage() or
     *        renderCpuPixels() from paintGL() instead.
     */
    bool renderSurface(struct wlr_surface *surface, const ViewportRect &destRect,
                       int windowWidth, int windowHeight);

    /**
     * @brief Zero-copy path: renders a cross-context EGLImage into Qt's
     *        framebuffer using GL_TEXTURE_EXTERNAL_OES.
     *        Call with Qt's OpenGL context current.
     *
     * @param eglImage  An EGLImage created from wlroots' GL_TEXTURE_2D texture
     *                  via eglCreateImage(..., EGL_GL_TEXTURE_2D, ...).
     * @param hasAlpha  Whether the source texture has an alpha channel.
     * @param srcW/srcH Source texture dimensions (for aspect ratio computation).
     */
    bool renderEglImage(EGLImage eglImage, bool hasAlpha,
                        int srcW, int srcH,
                        const ViewportRect &destRect,
                        int windowWidth, int windowHeight);

    /**
     * @brief CPU-readback path: uploads raw RGBA8888 pixels into a GL_TEXTURE_2D
     *        and renders it. Used when wlroots gives us a GL_TEXTURE_EXTERNAL_OES
     *        texture that cannot be directly wrapped in an EGLImageKHR.
     *        Call with Qt's OpenGL context current.
     *
     * @param pixels  Pointer to RGBA/ABGR pixel data.
     * @param srcW    Width of the pixel buffer.
     * @param srcH    Height of the pixel buffer.
     */
    bool renderCpuPixels(const uint8_t *pixels, int srcW, int srcH,
                         const ViewportRect &destRect,
                         int windowWidth, int windowHeight);

    /**
     * @brief Sends wl_surface.frame done callback to signal Android SurfaceFlinger
     *        that presentation has finished and the next frame can be committed.
     *        Must be called with wlroots' EGL context current.
     */
    void sendFrameDone(struct wlr_surface *surface);

    /**
     * @brief Clears letterbox / pillarbox margins with cyber dark background color.
     */
    void clearLetterboxBars(const ViewportRect &destRect, int windowWidth, int windowHeight);

private:
    GLuint createProgram(const char *vertSrc, const char *fragSrc);
    void   setupOrthoMatrix(float left, float right, float bottom, float top,
                            std::array<float, 16> &out);
    bool   drawQuad(GLuint program, GLint uProj, GLint uTex,
                    GLenum texTarget, GLuint texId,
                    const ViewportRect &destRect,
                    int windowWidth, int windowHeight);

    GLuint m_program2D = 0;
    GLuint m_programExternalOES = 0;

    GLint m_uProj2D = -1;
    GLint m_uTex2D  = -1;

    GLint m_uProjExternal = -1;
    GLint m_uTexExternal  = -1;

    GLuint m_vbo = 0;

    // Persistent CPU-upload texture (reused across frames to avoid re-allocation)
    GLuint m_cpuTex = 0;
    int    m_cpuTexW = 0;
    int    m_cpuTexH = 0;

    // Persistent OES texture for EGLImageKHR import
    GLuint m_oesTex = 0;

    bool m_initialized = false;
};

#endif // WAYLAND_SURFACE_RENDERER_H
