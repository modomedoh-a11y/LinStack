#include "WaylandSurfaceRenderer.h"

#include <iostream>
#include <ctime>
#include <cmath>

// Load glEGLImageTargetTexture2DOES at runtime via eglGetProcAddress.
// The function may not be exported directly by libGLESv2.so on all Mesa builds,
// but is always available via the EGL proc-address mechanism.
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_glEGLImageTargetTexture2DOES = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Shader sources
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Standard vertex shader shared by all programs
const char *VERTEX_SHADER_SRC = R"(
    attribute vec2 a_pos;
    attribute vec2 a_texcoord;
    varying vec2 v_texcoord;
    uniform mat4 u_proj;

    void main() {
        gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
        v_texcoord = a_texcoord;
    }
)";

// Standard sampler2D fragment shader (CPU-upload textures, GL_TEXTURE_2D)
const char *FRAGMENT_SHADER_2D_SRC = R"(
    precision mediump float;
    varying vec2 v_texcoord;
    uniform sampler2D u_texture;

    void main() {
        gl_FragColor = texture2D(u_texture, v_texcoord);
    }
)";

// GL_TEXTURE_EXTERNAL_OES shader — required for EGLImageKHR import and
// for DMA-BUF textures created directly in wlroots' context.
const char *FRAGMENT_SHADER_EXTERNAL_SRC = R"(
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    varying vec2 v_texcoord;
    uniform samplerExternalOES u_texture;

    void main() {
        gl_FragColor = texture2D(u_texture, v_texcoord);
    }
)";

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

WaylandSurfaceRenderer::WaylandSurfaceRenderer() = default;

WaylandSurfaceRenderer::~WaylandSurfaceRenderer() {
    if (m_program2D)         glDeleteProgram(m_program2D);
    if (m_programExternalOES) glDeleteProgram(m_programExternalOES);
    if (m_vbo)   glDeleteBuffers(1, &m_vbo);
    if (m_cpuTex) glDeleteTextures(1, &m_cpuTex);
    if (m_oesTex) glDeleteTextures(1, &m_oesTex);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shader helpers
// ─────────────────────────────────────────────────────────────────────────────

GLuint WaylandSurfaceRenderer::createProgram(const char *vertSrc, const char *fragSrc) {
    auto compileShader = [](GLenum type, const char *src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 0) {
                std::string log(infoLen, '\0');
                glGetShaderInfoLog(shader, infoLen, nullptr, log.data());
                std::cerr << "[Renderer] Shader compilation error: " << log << std::endl;
            }
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (!vert) return 0;

    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!frag) { glDeleteShader(vert); return 0; }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glBindAttribLocation(program, 0, "a_pos");
    glBindAttribLocation(program, 1, "a_texcoord");
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::string log(infoLen, '\0');
            glGetProgramInfoLog(program, infoLen, nullptr, log.data());
            std::cerr << "[Renderer] Program link error: " << log << std::endl;
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialize (call inside Qt's initializeGL)
// ─────────────────────────────────────────────────────────────────────────────

bool WaylandSurfaceRenderer::initialize() {
    if (m_initialized) return true;

    // 1. GL_TEXTURE_2D sampler (CPU-upload & readback textures)
    m_program2D = createProgram(VERTEX_SHADER_SRC, FRAGMENT_SHADER_2D_SRC);
    if (!m_program2D) {
        std::cerr << "[Renderer] Error: Failed to compile 2D shader program." << std::endl;
        return false;
    }
    m_uProj2D = glGetUniformLocation(m_program2D, "u_proj");
    m_uTex2D  = glGetUniformLocation(m_program2D, "u_texture");

    // 2. samplerExternalOES (EGLImageKHR cross-context import and DMA-BUF)
    m_programExternalOES = createProgram(VERTEX_SHADER_SRC, FRAGMENT_SHADER_EXTERNAL_SRC);
    if (!m_programExternalOES) {
        std::cerr << "[Renderer] Warning: GL_OES_EGL_image_external unavailable; "
                     "falling back to 2D sampler." << std::endl;
    } else {
        m_uProjExternal = glGetUniformLocation(m_programExternalOES, "u_proj");
        m_uTexExternal  = glGetUniformLocation(m_programExternalOES, "u_texture");
    }

    // 3. Dynamic VBO for quad geometry
    glGenBuffers(1, &m_vbo);

    // 4. Persistent CPU-upload texture (avoid per-frame allocation)
    glGenTextures(1, &m_cpuTex);

    // 5. Persistent OES texture for EGLImageKHR import
    glGenTextures(1, &m_oesTex);

    // 6. Load OES EGL image extension function pointer at runtime
    s_glEGLImageTargetTexture2DOES =
        reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!s_glEGLImageTargetTexture2DOES) {
        std::cerr << "[Renderer] Warning: glEGLImageTargetTexture2DOES unavailable — "
                     "EGLImageKHR import path disabled; will use CPU readback." << std::endl;
    } else {
        std::cout << "[Renderer] glEGLImageTargetTexture2DOES loaded successfully." << std::endl;
    }

    m_initialized = true;
    std::cout << "[Renderer] Wayland Surface GLES2 Renderer initialized successfully." << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Ortho projection matrix
// ─────────────────────────────────────────────────────────────────────────────

void WaylandSurfaceRenderer::setupOrthoMatrix(float left, float right, float bottom, float top,
                                              std::array<float, 16> &out) {
    out.fill(0.0f);
    out[0]  =  2.0f / (right - left);
    out[5]  =  2.0f / (top - bottom);
    out[10] = -1.0f;
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[15] =  1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper: draw a textured quad into destRect
// ─────────────────────────────────────────────────────────────────────────────

bool WaylandSurfaceRenderer::drawQuad(GLuint program, GLint uProj, GLint uTex,
                                      GLenum texTarget, GLuint texId,
                                      const ViewportRect &destRect,
                                      int windowWidth, int windowHeight) {
    if (!program || texId == 0) return false;

    glUseProgram(program);

    std::array<float, 16> orthoMatrix;
    setupOrthoMatrix(0.0f, static_cast<float>(windowWidth),
                     static_cast<float>(windowHeight), 0.0f, orthoMatrix);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, orthoMatrix.data());

    float x1 = static_cast<float>(destRect.x);
    float y1 = static_cast<float>(destRect.y);
    float x2 = x1 + static_cast<float>(destRect.width);
    float y2 = y1 + static_cast<float>(destRect.height);

    // {pos.x, pos.y, tex.u, tex.v}
    float vertices[] = {
        x1, y1,  0.0f, 0.0f,
        x1, y2,  0.0f, 1.0f,
        x2, y1,  1.0f, 0.0f,
        x2, y1,  1.0f, 0.0f,
        x1, y2,  0.0f, 1.0f,
        x2, y2,  1.0f, 1.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texTarget, texId);
    glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(uTex, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(texTarget, 0);
    glUseProgram(0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// renderSurface — legacy path (wlroots context must be current)
// ─────────────────────────────────────────────────────────────────────────────

bool WaylandSurfaceRenderer::renderSurface(struct wlr_surface *surface,
                                           const ViewportRect &destRect,
                                           int windowWidth, int windowHeight) {
    if (!surface || !m_initialized) return false;

    struct wlr_texture *wlrTex = wlr_bridge_surface_get_texture(surface);
    if (!wlrTex) return false;
    if (!wlr_bridge_texture_is_gles2(wlrTex)) {
        std::cerr << "[Renderer] Warning: Surface texture is not GLES2." << std::endl;
        return false;
    }

    uint32_t target = 0, tex = 0;
    if (!wlr_bridge_gles2_texture_get_attribs(wlrTex, &target, &tex) || tex == 0) {
        return false;
    }

    GLuint prog  = (target == GL_TEXTURE_EXTERNAL_OES && m_programExternalOES)
                   ? m_programExternalOES : m_program2D;
    GLint  uProj = (prog == m_programExternalOES) ? m_uProjExternal : m_uProj2D;
    GLint  uTex  = (prog == m_programExternalOES) ? m_uTexExternal  : m_uTex2D;

    return drawQuad(prog, uProj, uTex, target, tex,
                    destRect, windowWidth, windowHeight);
}

// ─────────────────────────────────────────────────────────────────────────────
// renderEglImage — zero-copy path (Qt context current, wlroots GL_TEXTURE_2D source)
//
// Creates an OES texture in Qt's context backed by the EGLImageKHR that was
// created from wlroots' GL_TEXTURE_2D.  This is a true zero-copy path:
// no pixel data crosses the CPU.
// ─────────────────────────────────────────────────────────────────────────────

bool WaylandSurfaceRenderer::renderEglImage(EGLImage eglImage, bool /*hasAlpha*/,
                                            int /*srcW*/, int /*srcH*/,
                                            const ViewportRect &destRect,
                                            int windowWidth, int windowHeight) {
    if (!m_initialized || eglImage == EGL_NO_IMAGE) return false;
    if (!m_programExternalOES) {
        std::cerr << "[Renderer] renderEglImage: OES shader unavailable." << std::endl;
        return false;
    }
    if (!s_glEGLImageTargetTexture2DOES) {
        std::cerr << "[Renderer] renderEglImage: glEGLImageTargetTexture2DOES not loaded." << std::endl;
        return false;
    }

    // Bind the persistent OES texture and attach the EGLImage to it
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTex);
    s_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                   static_cast<GLeglImageOES>(eglImage));

    bool ok = drawQuad(m_programExternalOES, m_uProjExternal, m_uTexExternal,
                       GL_TEXTURE_EXTERNAL_OES, m_oesTex,
                       destRect, windowWidth, windowHeight);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// renderCpuPixels — CPU-readback fallback (Qt context current)
//
// Used when wlroots gives us a GL_TEXTURE_EXTERNAL_OES (DMA-BUF import) that
// cannot be wrapped in an EGL_GL_TEXTURE_2D_KHR EGLImage.  Uploads RGBA8888
// pixels into a persistent GL_TEXTURE_2D and renders with the standard 2D shader.
// ─────────────────────────────────────────────────────────────────────────────

bool WaylandSurfaceRenderer::renderCpuPixels(const uint8_t *pixels,
                                             int srcW, int srcH,
                                             const ViewportRect &destRect,
                                             int windowWidth, int windowHeight) {
    if (!m_initialized || !pixels || srcW <= 0 || srcH <= 0) return false;

    glBindTexture(GL_TEXTURE_2D, m_cpuTex);

    if (srcW != m_cpuTexW || srcH != m_cpuTexH) {
        // Re-allocate the texture storage for the new dimensions
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     srcW, srcH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        m_cpuTexW = srcW;
        m_cpuTexH = srcH;
    } else {
        // Same size — cheaper sub-image upload
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        srcW, srcH, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return drawQuad(m_program2D, m_uProj2D, m_uTex2D,
                    GL_TEXTURE_2D, m_cpuTex,
                    destRect, windowWidth, windowHeight);
}

// ─────────────────────────────────────────────────────────────────────────────
// sendFrameDone — must be called with wlroots' EGL context current
// ─────────────────────────────────────────────────────────────────────────────

void WaylandSurfaceRenderer::sendFrameDone(struct wlr_surface *surface) {
    if (!surface) return;
    wlr_bridge_surface_send_frame_done(surface);
}

// ─────────────────────────────────────────────────────────────────────────────
// clearLetterboxBars
// ─────────────────────────────────────────────────────────────────────────────

void WaylandSurfaceRenderer::clearLetterboxBars(const ViewportRect &destRect,
                                                int windowWidth, int windowHeight) {
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);

    // Top bar
    if (destRect.y > 0) {
        glScissor(0, windowHeight - destRect.y, windowWidth, destRect.y);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    // Bottom bar
    int bottomY = destRect.y + destRect.height;
    if (bottomY < windowHeight) {
        glScissor(0, 0, windowWidth, windowHeight - bottomY);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    // Left bar
    if (destRect.x > 0) {
        glScissor(0, 0, destRect.x, windowHeight);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    // Right bar
    int rightX = destRect.x + destRect.width;
    if (rightX < windowWidth) {
        glScissor(rightX, 0, windowWidth - rightX, windowHeight);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glDisable(GL_SCISSOR_TEST);
}
