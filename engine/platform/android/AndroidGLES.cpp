#include "AndroidGLES.hpp"
#include "AndroidPlatform.hpp"

#include <sstream>
#include <algorithm>
#include <cstring>

namespace Nova {

AndroidGLES::~AndroidGLES() {
    Shutdown();
}

bool AndroidGLES::Initialize(ANativeWindow* window) {
    return Initialize(window, EGLConfigOptions{});
}

bool AndroidGLES::Initialize(ANativeWindow* window, const EGLConfigOptions& options) {
    if (m_initialized) {
        NOVA_LOGW("AndroidGLES already initialized");
        return true;
    }

    if (!window) {
        NOVA_LOGE("Invalid native window");
        return false;
    }

    m_window = window;
    m_configOptions = options;

    // Initialize EGL display
    if (!InitializeDisplay()) {
        return false;
    }

    // Choose EGL configuration
    if (!ChooseConfig(options)) {
        Shutdown();
        return false;
    }

    // Create EGL context
    if (!CreateContext()) {
        Shutdown();
        return false;
    }

    // Create initial surface
    if (!CreateSurface(window)) {
        Shutdown();
        return false;
    }

    // Make context current
    if (!MakeCurrent()) {
        Shutdown();
        return false;
    }

    // Query version and extensions
    QueryVersion();
    QueryExtensions();

    m_initialized = true;

    NOVA_LOGI("OpenGL ES initialized:");
    NOVA_LOGI("  Version: %s", m_version.versionString.c_str());
    NOVA_LOGI("  Renderer: %s", m_version.rendererString.c_str());
    NOVA_LOGI("  GLSL: %s", m_version.shadingLanguageVersion.c_str());
    NOVA_LOGI("  Surface: %dx%d", m_surfaceWidth, m_surfaceHeight);

    return true;
}

void AndroidGLES::Shutdown() {
    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }

        if (m_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_display, m_surface);
            m_surface = EGL_NO_SURFACE;
        }

        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
    }

    m_window = nullptr;
    m_surfaceWidth = 0;
    m_surfaceHeight = 0;
    m_initialized = false;

    NOVA_LOGI("AndroidGLES shutdown complete");
}

bool AndroidGLES::InitializeDisplay() {
    // Get EGL display
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) {
        NOVA_LOGE("Failed to get EGL display");
        return false;
    }

    // Initialize EGL
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(m_display, &majorVersion, &minorVersion)) {
        NOVA_LOGE("Failed to initialize EGL: %s", GetEGLErrorString(eglGetError()));
        return false;
    }

    NOVA_LOGI("EGL initialized: version %d.%d", majorVersion, minorVersion);

    // Query EGL extensions
    const char* eglExtStr = eglQueryString(m_display, EGL_EXTENSIONS);
    if (eglExtStr) {
        std::istringstream iss(eglExtStr);
        std::string ext;
        while (iss >> ext) {
            m_eglExtensions.push_back(ext);
        }
    }

    return true;
}

bool AndroidGLES::ChooseConfig(const EGLConfigOptions& options) {
    // Build attribute list
    std::vector<EGLint> attribs = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, options.redBits,
        EGL_GREEN_SIZE, options.greenBits,
        EGL_BLUE_SIZE, options.blueBits,
        EGL_ALPHA_SIZE, options.alphaBits,
        EGL_DEPTH_SIZE, options.depthBits,
        EGL_STENCIL_SIZE, options.stencilBits,
    };

    // Add multisampling if requested
    if (options.samples > 1) {
        attribs.push_back(EGL_SAMPLE_BUFFERS);
        attribs.push_back(1);
        attribs.push_back(EGL_SAMPLES);
        attribs.push_back(options.samples);
    }

    attribs.push_back(EGL_NONE);

    // Choose config
    EGLint numConfigs;
    if (!eglChooseConfig(m_display, attribs.data(), &m_config, 1, &numConfigs) || numConfigs == 0) {
        NOVA_LOGE("Failed to choose EGL config: %s", GetEGLErrorString(eglGetError()));

        // Try fallback with minimal requirements
        std::vector<EGLint> fallbackAttribs = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };

        if (!eglChooseConfig(m_display, fallbackAttribs.data(), &m_config, 1, &numConfigs) || numConfigs == 0) {
            NOVA_LOGE("Failed to choose fallback EGL config");
            return false;
        }
        NOVA_LOGW("Using fallback EGL configuration");
    }

    return true;
}

bool AndroidGLES::CreateContext() {
    // Try ES 3.2 first, then 3.1, then 3.0
    const int versions[][2] = {{3, 2}, {3, 1}, {3, 0}};

    for (const auto& ver : versions) {
        EGLint contextAttribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, ver[0],
            EGL_CONTEXT_MINOR_VERSION, ver[1],
            EGL_NONE
        };

        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttribs);
        if (m_context != EGL_NO_CONTEXT) {
            m_version.major = ver[0];
            m_version.minor = ver[1];
            NOVA_LOGI("Created OpenGL ES %d.%d context", ver[0], ver[1]);
            return true;
        }
    }

    NOVA_LOGE("Failed to create EGL context: %s", GetEGLErrorString(eglGetError()));
    return false;
}

bool AndroidGLES::CreateSurface(ANativeWindow* window) {
    if (!window) {
        NOVA_LOGE("Invalid native window for surface creation");
        return false;
    }

    // Destroy existing surface if any
    if (m_surface != EGL_NO_SURFACE) {
        DestroySurface();
    }

    m_window = window;

    // Set buffer geometry
    EGLint format;
    eglGetConfigAttrib(m_display, m_config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    // Create window surface
    m_surface = eglCreateWindowSurface(m_display, m_config, window, nullptr);
    if (m_surface == EGL_NO_SURFACE) {
        NOVA_LOGE("Failed to create EGL surface: %s", GetEGLErrorString(eglGetError()));
        return false;
    }

    // Query surface dimensions
    eglQuerySurface(m_display, m_surface, EGL_WIDTH, &m_surfaceWidth);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &m_surfaceHeight);

    NOVA_LOGI("Created EGL surface: %dx%d", m_surfaceWidth, m_surfaceHeight);
    return true;
}

void AndroidGLES::DestroySurface() {
    if (m_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
        m_surfaceWidth = 0;
        m_surfaceHeight = 0;
    }
}

void AndroidGLES::ResizeSurface(int width, int height) {
    if (m_surface == EGL_NO_SURFACE) {
        return;
    }

    // Query actual dimensions (may differ from requested)
    eglQuerySurface(m_display, m_surface, EGL_WIDTH, &m_surfaceWidth);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &m_surfaceHeight);

    // Update viewport
    glViewport(0, 0, m_surfaceWidth, m_surfaceHeight);

    NOVA_LOGI("Surface resized to: %dx%d", m_surfaceWidth, m_surfaceHeight);
}

bool AndroidGLES::MakeCurrent() {
    if (m_display == EGL_NO_DISPLAY || m_surface == EGL_NO_SURFACE || m_context == EGL_NO_CONTEXT) {
        return false;
    }

    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
        NOVA_LOGE("Failed to make context current: %s", GetEGLErrorString(eglGetError()));
        return false;
    }

    return true;
}

void AndroidGLES::ReleaseCurrent() {
    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}

void AndroidGLES::SwapBuffers() {
    if (m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE) {
        eglSwapBuffers(m_display, m_surface);
    }
}

void AndroidGLES::SetSwapInterval(int interval) {
    if (m_display != EGL_NO_DISPLAY) {
        eglSwapInterval(m_display, interval);
    }
}

void AndroidGLES::QueryVersion() {
    m_version.vendorString = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    m_version.rendererString = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_version.versionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    m_version.shadingLanguageVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Determine feature availability based on version
    m_version.hasComputeShaders = (m_version.major > 3) || (m_version.major == 3 && m_version.minor >= 1);
    m_version.hasGeometryShaders = (m_version.major > 3) || (m_version.major == 3 && m_version.minor >= 2);
    m_version.hasTessellation = (m_version.major > 3) || (m_version.major == 3 && m_version.minor >= 2);
}

void AndroidGLES::QueryExtensions() {
    const char* extStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extStr) {
        std::istringstream iss(extStr);
        std::string ext;
        while (iss >> ext) {
            m_glExtensions.push_back(ext);
        }
    }

    // Also query extensions via glGetStringi (ES 3.0+)
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i) {
        const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (ext) {
            auto it = std::find(m_glExtensions.begin(), m_glExtensions.end(), ext);
            if (it == m_glExtensions.end()) {
                m_glExtensions.push_back(ext);
            }
        }
    }
}

bool AndroidGLES::HasExtension(const std::string& extension) const {
    return std::find(m_glExtensions.begin(), m_glExtensions.end(), extension) != m_glExtensions.end();
}

bool AndroidGLES::HasEGLExtension(const std::string& extension) const {
    return std::find(m_eglExtensions.begin(), m_eglExtensions.end(), extension) != m_eglExtensions.end();
}

void AndroidGLES::EnableDebugOutput(bool enabled) {
    if (!m_version.hasComputeShaders) {
        // Debug output requires ES 3.2 or KHR_debug extension
        if (!HasExtension("GL_KHR_debug")) {
            NOVA_LOGW("Debug output not supported");
            return;
        }
    }

    if (enabled) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    } else {
        glDisable(GL_DEBUG_OUTPUT);
    }
}

bool AndroidGLES::CheckError(const char* location) {
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return true;
    }

    const char* errorStr;
    switch (error) {
        case GL_INVALID_ENUM:      errorStr = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE:     errorStr = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY:     errorStr = "GL_OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        default: errorStr = "Unknown"; break;
    }

    if (location) {
        NOVA_LOGE("OpenGL error at %s: %s (0x%x)", location, errorStr, error);
    } else {
        NOVA_LOGE("OpenGL error: %s (0x%x)", errorStr, error);
    }

    return false;
}

const char* AndroidGLES::GetEGLErrorString(EGLint error) {
    switch (error) {
        case EGL_SUCCESS:             return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
        default:                      return "Unknown EGL error";
    }
}

} // namespace Nova
