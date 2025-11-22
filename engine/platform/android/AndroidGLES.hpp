#pragma once

/**
 * @file AndroidGLES.hpp
 * @brief OpenGL ES 3.0/3.1 context management for Android
 *
 * Provides EGL context and surface management for OpenGL ES rendering
 * on Android devices.
 */

#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>

#include <string>
#include <vector>

namespace Nova {

/**
 * @brief OpenGL ES version information
 */
struct GLESVersion {
    int major = 3;
    int minor = 0;
    bool hasComputeShaders = false;      // ES 3.1+
    bool hasGeometryShaders = false;     // ES 3.2+
    bool hasTessellation = false;        // ES 3.2+
    std::string vendorString;
    std::string rendererString;
    std::string versionString;
    std::string shadingLanguageVersion;
};

/**
 * @brief EGL configuration options
 */
struct EGLConfigOptions {
    int redBits = 8;
    int greenBits = 8;
    int blueBits = 8;
    int alphaBits = 8;
    int depthBits = 24;
    int stencilBits = 8;
    int samples = 0;
    bool sRGB = false;
};

/**
 * @brief OpenGL ES context manager for Android
 *
 * Handles EGL display, surface, and context creation and management.
 * Supports OpenGL ES 3.0, 3.1, and 3.2.
 */
class AndroidGLES {
public:
    AndroidGLES() = default;
    ~AndroidGLES();

    // Non-copyable, non-movable
    AndroidGLES(const AndroidGLES&) = delete;
    AndroidGLES& operator=(const AndroidGLES&) = delete;
    AndroidGLES(AndroidGLES&&) = delete;
    AndroidGLES& operator=(AndroidGLES&&) = delete;

    /**
     * @brief Initialize OpenGL ES context
     * @param window Native window handle
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeWindow* window);

    /**
     * @brief Initialize with custom configuration
     * @param window Native window handle
     * @param options EGL configuration options
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeWindow* window, const EGLConfigOptions& options);

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Create rendering surface for a native window
     * @param window Native window handle
     * @return true if surface was created
     */
    bool CreateSurface(ANativeWindow* window);

    /**
     * @brief Destroy the current rendering surface
     */
    void DestroySurface();

    /**
     * @brief Resize the surface (called on window size change)
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void ResizeSurface(int width, int height);

    /**
     * @brief Make this context current for the calling thread
     * @return true if successful
     */
    bool MakeCurrent();

    /**
     * @brief Release context from current thread
     */
    void ReleaseCurrent();

    /**
     * @brief Swap front and back buffers
     */
    void SwapBuffers();

    /**
     * @brief Set swap interval (VSync)
     * @param interval 0 = no vsync, 1 = vsync, n = sync every n frames
     */
    void SetSwapInterval(int interval);

    /**
     * @brief Check if context is valid and current
     */
    [[nodiscard]] bool IsValid() const { return m_context != EGL_NO_CONTEXT; }

    /**
     * @brief Check if surface is valid
     */
    [[nodiscard]] bool HasSurface() const { return m_surface != EGL_NO_SURFACE; }

    /**
     * @brief Get current surface dimensions
     */
    [[nodiscard]] int GetSurfaceWidth() const { return m_surfaceWidth; }
    [[nodiscard]] int GetSurfaceHeight() const { return m_surfaceHeight; }

    /**
     * @brief Get OpenGL ES version information
     */
    [[nodiscard]] const GLESVersion& GetVersion() const { return m_version; }

    /**
     * @brief Check if specific GL extension is supported
     * @param extension Extension name (e.g., "GL_OES_texture_float")
     */
    [[nodiscard]] bool HasExtension(const std::string& extension) const;

    /**
     * @brief Check if specific EGL extension is supported
     * @param extension Extension name
     */
    [[nodiscard]] bool HasEGLExtension(const std::string& extension) const;

    /**
     * @brief Get list of supported GL extensions
     */
    [[nodiscard]] const std::vector<std::string>& GetExtensions() const { return m_glExtensions; }

    /**
     * @brief Get EGL display handle
     */
    [[nodiscard]] EGLDisplay GetDisplay() const { return m_display; }

    /**
     * @brief Get EGL context handle
     */
    [[nodiscard]] EGLContext GetContext() const { return m_context; }

    /**
     * @brief Get EGL surface handle
     */
    [[nodiscard]] EGLSurface GetSurface() const { return m_surface; }

    /**
     * @brief Enable or disable debug output (if available)
     */
    void EnableDebugOutput(bool enabled);

    /**
     * @brief Check for GL errors and log them
     * @param location Description of where the check is performed
     * @return true if no errors
     */
    static bool CheckError(const char* location = nullptr);

    /**
     * @brief Get last EGL error as string
     */
    static const char* GetEGLErrorString(EGLint error);

private:
    bool InitializeDisplay();
    bool ChooseConfig(const EGLConfigOptions& options);
    bool CreateContext();
    void QueryVersion();
    void QueryExtensions();

    // EGL handles
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;

    // Native window reference
    ANativeWindow* m_window = nullptr;

    // Surface dimensions
    int m_surfaceWidth = 0;
    int m_surfaceHeight = 0;

    // Configuration
    EGLConfigOptions m_configOptions;

    // Version and extensions
    GLESVersion m_version;
    std::vector<std::string> m_glExtensions;
    std::vector<std::string> m_eglExtensions;

    bool m_initialized = false;
};

} // namespace Nova
