#pragma once

/**
 * @file Graphics.hpp
 * @brief Graphics API abstraction layer
 *
 * Provides a unified interface for different graphics APIs across platforms:
 * - OpenGL (Desktop)
 * - OpenGL ES (Mobile, WebGL)
 * - Vulkan (Cross-platform)
 * - Metal (Apple platforms)
 * - WebGPU (Future)
 */

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace Nova {

// =============================================================================
// Graphics API Types
// =============================================================================

/**
 * @brief Supported graphics APIs
 */
enum class GraphicsAPI {
    None,
    OpenGL,     // OpenGL 4.x (Desktop)
    OpenGLES,   // OpenGL ES 3.x (Mobile, Web)
    Vulkan,     // Vulkan 1.x
    Metal,      // Metal (Apple)
    DirectX12,  // DirectX 12 (Windows)
    WebGL,      // WebGL 2.0
    WebGPU      // Future: WebGPU
};

/**
 * @brief Convert GraphicsAPI to string
 */
constexpr const char* GraphicsAPIToString(GraphicsAPI api) noexcept {
    switch (api) {
        case GraphicsAPI::None:      return "None";
        case GraphicsAPI::OpenGL:    return "OpenGL";
        case GraphicsAPI::OpenGLES:  return "OpenGL ES";
        case GraphicsAPI::Vulkan:    return "Vulkan";
        case GraphicsAPI::Metal:     return "Metal";
        case GraphicsAPI::DirectX12: return "DirectX 12";
        case GraphicsAPI::WebGL:     return "WebGL";
        case GraphicsAPI::WebGPU:    return "WebGPU";
        default:                     return "Unknown";
    }
}

// =============================================================================
// Graphics Capabilities
// =============================================================================

/**
 * @brief Graphics feature flags
 */
enum class GraphicsFeature {
    ComputeShaders,
    GeometryShaders,
    TessellationShaders,
    Instancing,
    MultiDrawIndirect,
    TextureArrays,
    CubemapArrays,
    SSBO,               // Shader Storage Buffer Objects
    UBO,                // Uniform Buffer Objects
    ImageLoadStore,
    AtomicCounters,
    DepthClamp,
    SeamlessCubemap,
    AnisotropicFiltering,
    TextureCompressionS3TC,
    TextureCompressionETC2,
    TextureCompressionASTC,
    TextureCompressionBC,
    HDRRenderTargets,
    MSAA,
    VariableRateShading,
    RayTracing,
    MeshShaders,
    Bindless,
    SparseTextures,
    ConservativeRaster
};

/**
 * @brief GPU vendor
 */
enum class GPUVendor {
    Unknown,
    NVIDIA,
    AMD,
    Intel,
    Apple,
    ARM,        // Mali
    Qualcomm,   // Adreno
    ImgTec,     // PowerVR
    Broadcom,
    Software    // Software renderer
};

/**
 * @brief Graphics capabilities structure
 */
struct GraphicsCapabilities {
    // API Info
    GraphicsAPI api = GraphicsAPI::None;
    std::string apiVersion;
    std::string shadingLanguageVersion;

    // Device Info
    GPUVendor vendor = GPUVendor::Unknown;
    std::string vendorString;
    std::string rendererString;
    std::string driverVersion;

    // Texture Limits
    int maxTextureSize = 4096;
    int maxCubemapSize = 4096;
    int max3DTextureSize = 256;
    int maxArrayTextureLayers = 256;
    int maxTextureUnits = 16;
    int maxTextureImageUnits = 16;
    float maxAnisotropy = 1.0f;

    // Framebuffer Limits
    int maxColorAttachments = 8;
    int maxDrawBuffers = 8;
    int maxFramebufferWidth = 4096;
    int maxFramebufferHeight = 4096;
    int maxFramebufferSamples = 4;
    int maxRenderbufferSize = 4096;

    // Shader Limits
    int maxVertexAttributes = 16;
    int maxVertexUniforms = 1024;
    int maxFragmentUniforms = 1024;
    int maxUniformBlockSize = 16384;
    int maxUniformBufferBindings = 12;
    int maxSSBOSize = 0;
    int maxSSBOBindings = 0;
    int maxComputeWorkGroupInvocations = 0;
    int maxComputeWorkGroupSize[3] = {0, 0, 0};
    int maxComputeWorkGroupCount[3] = {0, 0, 0};
    int maxComputeSharedMemorySize = 0;

    // Viewport Limits
    int maxViewportWidth = 4096;
    int maxViewportHeight = 4096;
    int maxViewports = 1;
    float viewportBounds[2] = {-32768.0f, 32768.0f};

    // Line/Point Limits
    float lineWidthRange[2] = {1.0f, 1.0f};
    float pointSizeRange[2] = {1.0f, 1.0f};

    // Memory Info (if available)
    uint64_t totalVideoMemory = 0;      // Bytes
    uint64_t availableVideoMemory = 0;  // Bytes

    // Feature Support
    bool supportsComputeShaders = false;
    bool supportsGeometryShaders = false;
    bool supportsTessellation = false;
    bool supportsInstancing = true;
    bool supportsMultiDrawIndirect = false;
    bool supportsSSBO = false;
    bool supportsImageLoadStore = false;
    bool supportsBindless = false;
    bool supportsRayTracing = false;
    bool supportsMeshShaders = false;

    // Texture Compression
    bool supportsS3TC = false;          // DXT
    bool supportsETC2 = false;
    bool supportsASTC = false;
    bool supportsBC = false;            // BC6H, BC7
    bool supportsPVRTC = false;

    /**
     * @brief Check if a feature is supported
     */
    bool HasFeature(GraphicsFeature feature) const noexcept;
};

// =============================================================================
// Graphics Context Configuration
// =============================================================================

/**
 * @brief Graphics context creation configuration
 */
struct GraphicsConfig {
    GraphicsAPI preferredAPI = GraphicsAPI::OpenGL;
    int majorVersion = 4;
    int minorVersion = 6;
    bool debug = false;
    bool vsync = true;
    int swapInterval = 1;
    int samples = 4;              // MSAA samples
    int colorBits = 32;
    int depthBits = 24;
    int stencilBits = 8;
    bool sRGB = true;
    bool doubleBuffer = true;
    bool forwardCompatible = true;
    bool coreProfile = true;
};

// =============================================================================
// Graphics Context Interface
// =============================================================================

/**
 * @brief Abstract graphics context interface
 *
 * Provides basic graphics context management. Platform-specific
 * implementations handle actual context creation and management.
 */
class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;

    /**
     * @brief Initialize graphics context
     * @return true if initialization succeeded
     */
    virtual bool Initialize(const GraphicsConfig& config) = 0;

    /**
     * @brief Shutdown and cleanup
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Make this context current
     */
    virtual void MakeCurrent() = 0;

    /**
     * @brief Check if this context is current
     */
    [[nodiscard]] virtual bool IsCurrent() const = 0;

    /**
     * @brief Swap front and back buffers
     */
    virtual void SwapBuffers() = 0;

    /**
     * @brief Set vsync mode
     */
    virtual void SetVSync(bool enabled) = 0;

    /**
     * @brief Get the graphics API used by this context
     */
    [[nodiscard]] virtual GraphicsAPI GetAPI() const noexcept = 0;

    /**
     * @brief Get graphics capabilities
     */
    [[nodiscard]] virtual const GraphicsCapabilities& GetCapabilities() const = 0;

protected:
    GraphicsCapabilities m_capabilities;
};

// =============================================================================
// Graphics Utility Class
// =============================================================================

/**
 * @brief Graphics utility and factory class
 *
 * Provides static methods for graphics API detection, capability
 * queries, and context creation.
 */
class Graphics {
public:
    // -------------------------------------------------------------------------
    // API Detection
    // -------------------------------------------------------------------------

    /**
     * @brief Get the preferred graphics API for current platform
     */
    static GraphicsAPI GetPreferredAPI();

    /**
     * @brief Get list of available graphics APIs
     */
    static std::vector<GraphicsAPI> GetAvailableAPIs();

    /**
     * @brief Check if a graphics API is available
     */
    static bool IsAPIAvailable(GraphicsAPI api);

    /**
     * @brief Get the best available API
     */
    static GraphicsAPI GetBestAvailableAPI();

    // -------------------------------------------------------------------------
    // Context Creation
    // -------------------------------------------------------------------------

    /**
     * @brief Create a graphics context
     * @param api Graphics API to use
     * @return Unique pointer to context, or nullptr on failure
     */
    static std::unique_ptr<GraphicsContext> CreateContext(GraphicsAPI api);

    /**
     * @brief Create context with configuration
     */
    static std::unique_ptr<GraphicsContext> CreateContext(const GraphicsConfig& config);

    // -------------------------------------------------------------------------
    // Capability Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Check if compute shaders are supported
     */
    static bool SupportsComputeShaders();

    /**
     * @brief Check if geometry shaders are supported
     */
    static bool SupportsGeometryShaders();

    /**
     * @brief Check if tessellation is supported
     */
    static bool SupportsTessellation();

    /**
     * @brief Check if instancing is supported
     */
    static bool SupportsInstancing();

    /**
     * @brief Check if ray tracing is supported
     */
    static bool SupportsRayTracing();

    /**
     * @brief Get maximum texture size
     */
    static int GetMaxTextureSize();

    /**
     * @brief Get maximum uniform buffer size
     */
    static int GetMaxUniformBufferSize();

    /**
     * @brief Get maximum MSAA samples
     */
    static int GetMaxMSAASamples();

    /**
     * @brief Get total video memory in bytes
     */
    static uint64_t GetTotalVideoMemory();

    /**
     * @brief Get available video memory in bytes
     */
    static uint64_t GetAvailableVideoMemory();

    // -------------------------------------------------------------------------
    // Version Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Get current OpenGL version
     */
    static std::string GetOpenGLVersion();

    /**
     * @brief Get GLSL version
     */
    static std::string GetGLSLVersion();

    /**
     * @brief Get GPU renderer name
     */
    static std::string GetRendererName();

    /**
     * @brief Get GPU vendor name
     */
    static std::string GetVendorName();

    /**
     * @brief Detect GPU vendor from string
     */
    static GPUVendor DetectVendor(const std::string& vendorString);

    // -------------------------------------------------------------------------
    // Debug
    // -------------------------------------------------------------------------

    /**
     * @brief Enable debug output (if supported)
     */
    static void EnableDebugOutput(bool enabled);

    /**
     * @brief Set debug callback
     */
    using DebugCallback = std::function<void(
        int severity,
        const std::string& message,
        const std::string& source
    )>;
    static void SetDebugCallback(DebugCallback callback);

    /**
     * @brief Check for graphics errors
     * @return Error string or empty if no error
     */
    static std::string CheckError();

    /**
     * @brief Clear any pending errors
     */
    static void ClearErrors();

private:
    Graphics() = delete;  // Static class, no instantiation

    static DebugCallback s_debugCallback;
};

// =============================================================================
// Platform-Specific Helpers
// =============================================================================

/**
 * @brief Get recommended graphics API for platform
 */
inline GraphicsAPI GetRecommendedGraphicsAPI() {
#if defined(NOVA_PLATFORM_WINDOWS)
    return GraphicsAPI::OpenGL;  // Or Vulkan/DX12 if available
#elif defined(NOVA_PLATFORM_LINUX)
    return GraphicsAPI::OpenGL;  // Or Vulkan
#elif defined(NOVA_PLATFORM_MACOS)
    return GraphicsAPI::Metal;   // Deprecated OpenGL, use Metal
#elif defined(NOVA_PLATFORM_IOS)
    return GraphicsAPI::Metal;
#elif defined(NOVA_PLATFORM_ANDROID)
    return GraphicsAPI::OpenGLES; // Or Vulkan on newer devices
#elif defined(NOVA_PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    return GraphicsAPI::WebGL;
#else
    return GraphicsAPI::OpenGL;
#endif
}

/**
 * @brief Check if platform uses OpenGL ES
 */
constexpr bool UsesOpenGLES() noexcept {
#if defined(NOVA_PLATFORM_IOS) || defined(NOVA_PLATFORM_ANDROID) || defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get OpenGL version for platform
 */
inline std::pair<int, int> GetDefaultOpenGLVersion() noexcept {
#if defined(NOVA_PLATFORM_MACOS)
    return {4, 1};  // macOS supports up to OpenGL 4.1
#elif defined(NOVA_PLATFORM_IOS) || defined(NOVA_PLATFORM_ANDROID)
    return {3, 0};  // OpenGL ES 3.0
#elif defined(__EMSCRIPTEN__)
    return {2, 0};  // WebGL 2.0 (OpenGL ES 3.0 compatible)
#else
    return {4, 6};  // Full OpenGL 4.6
#endif
}

} // namespace Nova
