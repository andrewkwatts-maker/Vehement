/**
 * @file WindowsGL.cpp
 * @brief WGL OpenGL context implementation
 */

#include "WindowsGL.hpp"

#ifdef NOVA_PLATFORM_WINDOWS

#include <Windows.h>
#include <GL/gl.h>
#include <cstring>

// WGL extension constants
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB         0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_OPENGL_NO_ERROR_ARB   0x31B3

// Pixel format constants
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9

// WGL function typedefs
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef BOOL (WINAPI *PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC, int, int, UINT, const int*, int*);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
typedef int (WINAPI *PFNWGLGETSWAPINTERVALEXTPROC)(void);
typedef const char* (WINAPI *PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);

#pragma comment(lib, "opengl32.lib")

namespace Nova {

// Static member
bool WindowsGLLoader::s_loaded = false;

// =============================================================================
// WindowsGLContext
// =============================================================================

WindowsGLContext::WindowsGLContext() = default;

WindowsGLContext::~WindowsGLContext() {
    Destroy();
}

bool WindowsGLContext::Create(HWND hwnd, const WGLContextConfig& config) {
    if (!hwnd) {
        return false;
    }

    m_hwnd = hwnd;
    m_hdc = GetDC(hwnd);
    m_config = config;

    if (!m_hdc) {
        return false;
    }

    // First, create a legacy context to load WGL extensions
    if (!CreateLegacyContext()) {
        return false;
    }

    // Load WGL extensions
    LoadExtensions();

    // Now create the modern context if extensions are available
    if (m_extensions.ARB_create_context) {
        // Destroy the legacy context
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;

        // Create modern context
        if (!CreateModernContext(config)) {
            return false;
        }
    }

    // Make context current
    if (!wglMakeCurrent(m_hdc, m_hglrc)) {
        Destroy();
        return false;
    }

    // Set initial VSync
    SetVSync(true);

    // Query capabilities
    QueryCapabilities();

    return true;
}

void WindowsGLContext::Destroy() {
    if (m_hglrc) {
        if (wglGetCurrentContext() == m_hglrc) {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }

    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }

    m_hwnd = nullptr;
}

bool WindowsGLContext::CreateLegacyContext() {
    // Set up pixel format
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (pixelFormat == 0) {
        return false;
    }

    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        return false;
    }

    // Create legacy context
    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc) {
        return false;
    }

    // Make current to load extensions
    if (!wglMakeCurrent(m_hdc, m_hglrc)) {
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
        return false;
    }

    return true;
}

bool WindowsGLContext::CreateModernContext(const WGLContextConfig& config) {
    auto wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(
        m_extensions.wglChoosePixelFormatARB);
    auto wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
        m_extensions.wglCreateContextAttribsARB);

    if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB) {
        return false;
    }

    // Choose pixel format using ARB extension
    int pixelFormatAttribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, config.colorBits,
        WGL_DEPTH_BITS_ARB, config.depthBits,
        WGL_STENCIL_BITS_ARB, config.stencilBits,
        WGL_SAMPLE_BUFFERS_ARB, config.samples > 0 ? GL_TRUE : GL_FALSE,
        WGL_SAMPLES_ARB, config.samples,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, config.sRGB ? GL_TRUE : GL_FALSE,
        0
    };

    int pixelFormat;
    UINT numFormats;
    if (!wglChoosePixelFormatARB(m_hdc, pixelFormatAttribs, nullptr, 1, &pixelFormat, &numFormats) || numFormats == 0) {
        // Fallback without MSAA
        pixelFormatAttribs[16] = GL_FALSE;
        pixelFormatAttribs[18] = 0;
        if (!wglChoosePixelFormatARB(m_hdc, pixelFormatAttribs, nullptr, 1, &pixelFormat, &numFormats) || numFormats == 0) {
            return false;
        }
    }

    // Set pixel format
    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(m_hdc, pixelFormat, sizeof(pfd), &pfd);
    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        return false;
    }

    // Create context attributes
    int contextFlags = 0;
    if (config.forwardCompatible) {
        contextFlags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    }
    if (config.debug) {
        contextFlags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    }

    int profileMask = config.coreProfile
        ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB
        : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

    std::vector<int> attribs = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, config.majorVersion,
        WGL_CONTEXT_MINOR_VERSION_ARB, config.minorVersion,
        WGL_CONTEXT_FLAGS_ARB, contextFlags,
        WGL_CONTEXT_PROFILE_MASK_ARB, profileMask,
    };

    if (config.noError) {
        attribs.push_back(WGL_CONTEXT_OPENGL_NO_ERROR_ARB);
        attribs.push_back(GL_TRUE);
    }

    attribs.push_back(0);

    // Create context
    m_hglrc = wglCreateContextAttribsARB(m_hdc, config.sharedContext, attribs.data());
    return m_hglrc != nullptr;
}

void WindowsGLContext::LoadExtensions() {
    if (m_extensions.loaded) {
        return;
    }

    // Get extension string function
    auto wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
        wglGetProcAddress("wglGetExtensionsStringARB"));

    if (wglGetExtensionsStringARB) {
        const char* extensions = wglGetExtensionsStringARB(m_hdc);
        if (extensions) {
            // Parse extension string
            std::string extStr(extensions);
            size_t start = 0;
            size_t end;
            while ((end = extStr.find(' ', start)) != std::string::npos) {
                m_extensionList.push_back(extStr.substr(start, end - start));
                start = end + 1;
            }
            if (start < extStr.length()) {
                m_extensionList.push_back(extStr.substr(start));
            }

            // Check for specific extensions
            m_extensions.ARB_create_context = HasExtension("WGL_ARB_create_context");
            m_extensions.ARB_create_context_profile = HasExtension("WGL_ARB_create_context_profile");
            m_extensions.ARB_create_context_robustness = HasExtension("WGL_ARB_create_context_robustness");
            m_extensions.ARB_pixel_format = HasExtension("WGL_ARB_pixel_format");
            m_extensions.ARB_multisample = HasExtension("WGL_ARB_multisample");
            m_extensions.EXT_swap_control = HasExtension("WGL_EXT_swap_control");
            m_extensions.EXT_swap_control_tear = HasExtension("WGL_EXT_swap_control_tear");
            m_extensions.ARB_framebuffer_sRGB = HasExtension("WGL_ARB_framebuffer_sRGB");
        }

        m_extensions.wglGetExtensionsStringARB = reinterpret_cast<void*>(wglGetExtensionsStringARB);
    }

    // Load function pointers
    m_extensions.wglCreateContextAttribsARB = reinterpret_cast<void*>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    m_extensions.wglChoosePixelFormatARB = reinterpret_cast<void*>(
        wglGetProcAddress("wglChoosePixelFormatARB"));
    m_extensions.wglGetPixelFormatAttribivARB = reinterpret_cast<void*>(
        wglGetProcAddress("wglGetPixelFormatAttribivARB"));
    m_extensions.wglSwapIntervalEXT = reinterpret_cast<void*>(
        wglGetProcAddress("wglSwapIntervalEXT"));
    m_extensions.wglGetSwapIntervalEXT = reinterpret_cast<void*>(
        wglGetProcAddress("wglGetSwapIntervalEXT"));

    m_extensions.loaded = true;
}

void WindowsGLContext::QueryCapabilities() {
    m_capabilities.api = GraphicsAPI::OpenGL;

    // Query GL strings
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (vendor) m_capabilities.vendorString = vendor;
    if (renderer) m_capabilities.rendererString = renderer;
    if (version) m_capabilities.apiVersion = version;
    if (glsl) m_capabilities.shadingLanguageVersion = glsl;

    // Detect vendor
    if (m_capabilities.vendorString.find("NVIDIA") != std::string::npos) {
        m_capabilities.vendor = GPUVendor::NVIDIA;
    } else if (m_capabilities.vendorString.find("AMD") != std::string::npos ||
               m_capabilities.vendorString.find("ATI") != std::string::npos) {
        m_capabilities.vendor = GPUVendor::AMD;
    } else if (m_capabilities.vendorString.find("Intel") != std::string::npos) {
        m_capabilities.vendor = GPUVendor::Intel;
    }

    // Query limits
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_capabilities.maxTextureSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &m_capabilities.maxCubemapSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_capabilities.max3DTextureSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &m_capabilities.maxArrayTextureLayers);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_capabilities.maxTextureUnits);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_capabilities.maxTextureImageUnits);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &m_capabilities.maxAnisotropy);

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &m_capabilities.maxColorAttachments);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &m_capabilities.maxDrawBuffers);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &m_capabilities.maxRenderbufferSize);
    glGetIntegerv(GL_MAX_SAMPLES, &m_capabilities.maxFramebufferSamples);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &m_capabilities.maxVertexAttributes);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &m_capabilities.maxVertexUniforms);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &m_capabilities.maxFragmentUniforms);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &m_capabilities.maxUniformBlockSize);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &m_capabilities.maxUniformBufferBindings);

    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, reinterpret_cast<GLint*>(&m_capabilities.maxViewportWidth));
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, m_capabilities.lineWidthRange);
    glGetFloatv(GL_POINT_SIZE_RANGE, m_capabilities.pointSizeRange);

    // Check for features
    int majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    m_capabilities.supportsComputeShaders = (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3));
    m_capabilities.supportsGeometryShaders = (majorVersion >= 3 || (majorVersion == 3 && minorVersion >= 2));
    m_capabilities.supportsTessellation = (majorVersion >= 4);
    m_capabilities.supportsInstancing = true;
    m_capabilities.supportsSSBO = (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3));
    m_capabilities.supportsImageLoadStore = (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 2));
    m_capabilities.supportsMultiDrawIndirect = (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3));

    // Compression support
    m_capabilities.supportsS3TC = true;  // Almost universal on Windows
    m_capabilities.supportsBC = (majorVersion >= 4);
}

bool WindowsGLContext::Initialize(const GraphicsConfig& config) {
    WGLContextConfig wglConfig;
    wglConfig.majorVersion = config.majorVersion;
    wglConfig.minorVersion = config.minorVersion;
    wglConfig.coreProfile = config.coreProfile;
    wglConfig.forwardCompatible = config.forwardCompatible;
    wglConfig.debug = config.debug;
    wglConfig.samples = config.samples;
    wglConfig.sRGB = config.sRGB;
    wglConfig.colorBits = config.colorBits;
    wglConfig.depthBits = config.depthBits;
    wglConfig.stencilBits = config.stencilBits;
    m_vsync = config.vsync;
    return true;  // Actual creation happens in Create()
}

void WindowsGLContext::Shutdown() {
    Destroy();
}

void WindowsGLContext::MakeCurrent() {
    if (m_hglrc && m_hdc) {
        wglMakeCurrent(m_hdc, m_hglrc);
    }
}

bool WindowsGLContext::IsCurrent() const {
    return wglGetCurrentContext() == m_hglrc;
}

void WindowsGLContext::SwapBuffers() {
    if (m_hdc) {
        ::SwapBuffers(m_hdc);
    }
}

void WindowsGLContext::SetVSync(bool enabled) {
    SetSwapInterval(enabled ? 1 : 0);
    m_vsync = enabled;
}

bool WindowsGLContext::HasExtension(const std::string& name) const {
    for (const auto& ext : m_extensionList) {
        if (ext == name) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> WindowsGLContext::GetExtensionList() const {
    return m_extensionList;
}

void WindowsGLContext::SetSwapInterval(int interval) {
    if (m_extensions.wglSwapIntervalEXT) {
        auto wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
            m_extensions.wglSwapIntervalEXT);

        // Check for adaptive vsync support
        if (interval < 0 && !m_extensions.EXT_swap_control_tear) {
            interval = 1;  // Fallback to regular vsync
        }

        wglSwapIntervalEXT(interval);
    }
}

int WindowsGLContext::GetSwapInterval() const {
    if (m_extensions.wglGetSwapIntervalEXT) {
        auto wglGetSwapIntervalEXT = reinterpret_cast<PFNWGLGETSWAPINTERVALEXTPROC>(
            m_extensions.wglGetSwapIntervalEXT);
        return wglGetSwapIntervalEXT();
    }
    return 0;
}

void* WindowsGLContext::GetProcAddress(const char* name) {
    void* p = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (p == nullptr || p == reinterpret_cast<void*>(0x1) ||
        p == reinterpret_cast<void*>(0x2) || p == reinterpret_cast<void*>(0x3) ||
        p == reinterpret_cast<void*>(-1)) {
        // Try loading from opengl32.dll
        HMODULE module = LoadLibraryA("opengl32.dll");
        if (module) {
            p = reinterpret_cast<void*>(::GetProcAddress(module, name));
        }
    }
    return p;
}

void WindowsGLContext::EnableDebugOutput(bool enabled) {
    if (enabled && m_config.debug) {
        // Would enable GL_DEBUG_OUTPUT here using glDebugMessageCallback
        // Requires loading the function pointer through GLAD or similar
    }
}

// =============================================================================
// WindowsGLLoader
// =============================================================================

bool WindowsGLLoader::LoadGL() {
    if (s_loaded) {
        return true;
    }

    // GLAD or similar loader would be used here
    // For now, assume GLFW or external loader handles this
    s_loaded = true;
    return true;
}

bool WindowsGLLoader::IsLoaded() {
    return s_loaded;
}

void* WindowsGLLoader::GetFunction(const char* name) {
    return WindowsGLContext::GetProcAddress(name);
}

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
