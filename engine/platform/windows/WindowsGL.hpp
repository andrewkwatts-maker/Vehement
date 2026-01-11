#pragma once

/**
 * @file WindowsGL.hpp
 * @brief WGL OpenGL context management for Windows
 *
 * Features:
 * - Modern WGL context creation with extensions
 * - Extension loading
 * - VSync control
 * - Context sharing for multi-threading
 * - Debug output support
 */

#ifdef NOVA_PLATFORM_WINDOWS

#include "../Graphics.hpp"
#include <string>
#include <vector>

// Forward declarations
struct HWND__;
typedef HWND__* HWND;
struct HDC__;
typedef HDC__* HDC;
struct HGLRC__;
typedef HGLRC__* HGLRC;

namespace Nova {

/**
 * @brief WGL extension function pointers
 */
struct WGLExtensions {
    // WGL_ARB_create_context
    void* wglCreateContextAttribsARB = nullptr;

    // WGL_ARB_pixel_format
    void* wglChoosePixelFormatARB = nullptr;
    void* wglGetPixelFormatAttribivARB = nullptr;

    // WGL_EXT_swap_control
    void* wglSwapIntervalEXT = nullptr;
    void* wglGetSwapIntervalEXT = nullptr;

    // WGL_ARB_extensions_string
    void* wglGetExtensionsStringARB = nullptr;

    // Feature support
    bool ARB_create_context = false;
    bool ARB_create_context_profile = false;
    bool ARB_create_context_robustness = false;
    bool ARB_pixel_format = false;
    bool ARB_multisample = false;
    bool EXT_swap_control = false;
    bool EXT_swap_control_tear = false;
    bool ARB_framebuffer_sRGB = false;

    bool loaded = false;
};

/**
 * @brief WGL context configuration
 */
struct WGLContextConfig {
    int majorVersion = 4;
    int minorVersion = 6;
    bool coreProfile = true;
    bool forwardCompatible = true;
    bool debug = false;
    bool noError = false;  // GL_KHR_no_error
    int samples = 0;       // MSAA samples
    bool sRGB = true;
    int colorBits = 32;
    int depthBits = 24;
    int stencilBits = 8;
    HGLRC sharedContext = nullptr;  // For context sharing
};

/**
 * @brief Windows OpenGL context using WGL
 */
class WindowsGLContext : public GraphicsContext {
public:
    WindowsGLContext();
    ~WindowsGLContext() override;

    // Non-copyable
    WindowsGLContext(const WindowsGLContext&) = delete;
    WindowsGLContext& operator=(const WindowsGLContext&) = delete;

    /**
     * @brief Create context for window
     * @param hwnd Window handle
     * @param config Context configuration
     * @return true if successful
     */
    bool Create(HWND hwnd, const WGLContextConfig& config = {});

    /**
     * @brief Destroy the context
     */
    void Destroy();

    // GraphicsContext interface
    bool Initialize(const GraphicsConfig& config) override;
    void Shutdown() override;
    void MakeCurrent() override;
    [[nodiscard]] bool IsCurrent() const override;
    void SwapBuffers() override;
    void SetVSync(bool enabled) override;
    [[nodiscard]] GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::OpenGL; }
    [[nodiscard]] const GraphicsCapabilities& GetCapabilities() const override { return m_capabilities; }

    // WGL-specific
    [[nodiscard]] HGLRC GetHGLRC() const { return m_hglrc; }
    [[nodiscard]] HDC GetHDC() const { return m_hdc; }
    [[nodiscard]] bool IsValid() const { return m_hglrc != nullptr; }

    /**
     * @brief Get WGL extension support
     */
    [[nodiscard]] const WGLExtensions& GetExtensions() const { return m_extensions; }

    /**
     * @brief Check if extension is supported
     */
    [[nodiscard]] bool HasExtension(const std::string& name) const;

    /**
     * @brief Get list of supported extensions
     */
    [[nodiscard]] std::vector<std::string> GetExtensionList() const;

    /**
     * @brief Set swap interval
     * @param interval 0=no vsync, 1=vsync, -1=adaptive vsync
     */
    void SetSwapInterval(int interval);

    /**
     * @brief Get current swap interval
     */
    [[nodiscard]] int GetSwapInterval() const;

    /**
     * @brief Load OpenGL function pointer
     */
    static void* GetProcAddress(const char* name);

    /**
     * @brief Enable OpenGL debug output
     */
    void EnableDebugOutput(bool enabled);

private:
    bool CreateLegacyContext();
    bool CreateModernContext(const WGLContextConfig& config);
    void LoadExtensions();
    void QueryCapabilities();

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    WGLExtensions m_extensions;
    WGLContextConfig m_config;
    std::vector<std::string> m_extensionList;
    bool m_vsync = true;
};

/**
 * @brief OpenGL function loader for Windows
 */
class WindowsGLLoader {
public:
    /**
     * @brief Load core OpenGL functions
     * @return true if successful
     */
    static bool LoadGL();

    /**
     * @brief Check if GL is loaded
     */
    static bool IsLoaded();

    /**
     * @brief Get function pointer
     */
    static void* GetFunction(const char* name);

private:
    static bool s_loaded;
};

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
