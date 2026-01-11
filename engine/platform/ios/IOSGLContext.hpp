#pragma once

#include <cstdint>

namespace Nova {

/**
 * @brief OpenGL ES context manager for iOS
 *
 * Manages EAGLContext creation, framebuffer setup, and rendering
 * presentation for OpenGL ES 3.0 on iOS devices.
 */
class IOSGLContext {
public:
    IOSGLContext();
    ~IOSGLContext();

    // Non-copyable
    IOSGLContext(const IOSGLContext&) = delete;
    IOSGLContext& operator=(const IOSGLContext&) = delete;

    // =========================================================================
    // Context Management
    // =========================================================================

    /**
     * @brief Create OpenGL ES 3.0 context
     * @return true if context creation succeeded
     */
    bool CreateContext();

    /**
     * @brief Destroy the OpenGL ES context
     */
    void DestroyContext();

    /**
     * @brief Check if context is valid
     */
    [[nodiscard]] bool IsValid() const { return m_context != nullptr; }

    /**
     * @brief Get native EAGLContext pointer
     */
    [[nodiscard]] void* GetNativeContext() const { return m_context; }

    /**
     * @brief Make this context current on the calling thread
     */
    bool MakeCurrent();

    /**
     * @brief Clear the current context (set to nil)
     */
    static void ClearCurrent();

    // =========================================================================
    // Framebuffer Management
    // =========================================================================

    /**
     * @brief Create the default framebuffer with renderbuffers
     * @param width Framebuffer width in pixels
     * @param height Framebuffer height in pixels
     */
    void CreateFramebuffer(int width, int height);

    /**
     * @brief Resize the framebuffer (recreates renderbuffers)
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void ResizeFramebuffer(int width, int height);

    /**
     * @brief Delete the framebuffer and associated renderbuffers
     */
    void DeleteFramebuffer();

    /**
     * @brief Bind the default framebuffer
     */
    void BindFramebuffer();

    /**
     * @brief Get framebuffer dimensions
     */
    [[nodiscard]] int GetFramebufferWidth() const { return m_framebufferWidth; }
    [[nodiscard]] int GetFramebufferHeight() const { return m_framebufferHeight; }

    // =========================================================================
    // Presentation
    // =========================================================================

    /**
     * @brief Present the renderbuffer contents to the screen
     */
    void Present();

    /**
     * @brief Set the drawable layer (CAEAGLLayer)
     * @param layer The CAEAGLLayer to render to
     */
    void SetDrawableLayer(void* layer);

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Pause rendering (call when app enters background)
     */
    void Pause();

    /**
     * @brief Resume rendering (call when app enters foreground)
     */
    void Resume();

    /**
     * @brief Check if context is paused
     */
    [[nodiscard]] bool IsPaused() const { return m_paused; }

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Get OpenGL ES version string
     */
    [[nodiscard]] const char* GetVersionString() const;

    /**
     * @brief Get OpenGL ES renderer string
     */
    [[nodiscard]] const char* GetRendererString() const;

    /**
     * @brief Check for and log any OpenGL errors
     */
    static bool CheckError(const char* operation = nullptr);

private:
    void* m_context = nullptr;          // EAGLContext*
    void* m_drawableLayer = nullptr;    // CAEAGLLayer*

    // Framebuffer objects
    uint32_t m_framebuffer = 0;
    uint32_t m_colorRenderbuffer = 0;
    uint32_t m_depthRenderbuffer = 0;
    uint32_t m_stencilRenderbuffer = 0;
    uint32_t m_msaaFramebuffer = 0;
    uint32_t m_msaaColorRenderbuffer = 0;

    // Framebuffer dimensions
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;

    // State
    bool m_paused = false;
    bool m_useMSAA = false;
    int m_msaaSamples = 4;
};

} // namespace Nova
