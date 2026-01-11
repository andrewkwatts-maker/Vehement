#pragma once

/**
 * @file Renderer.hpp
 * @brief Cross-platform render backend selection and abstraction
 *
 * Provides automatic backend selection and a unified rendering interface:
 * - OpenGL 4.6 on Windows/Linux/macOS
 * - OpenGL ES 3.0 on Android/iOS
 * - Metal on iOS/macOS
 * - Vulkan on Windows/Linux/Android
 */

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace Nova {

// ============================================================================
// Render Backend Types
// ============================================================================

/**
 * @brief Available rendering backends
 */
enum class RenderBackend {
    None,
    OpenGL,      // OpenGL 4.x (Desktop)
    OpenGLES,    // OpenGL ES 3.x (Mobile)
    Vulkan,      // Vulkan 1.x (Cross-platform)
    Metal,       // Metal (Apple platforms)
    Direct3D12,  // DirectX 12 (Windows)
    WebGL        // WebGL 2.0 (Web)
};

/**
 * @brief Convert backend to string
 */
constexpr const char* RenderBackendToString(RenderBackend backend) noexcept {
    switch (backend) {
        case RenderBackend::None:      return "None";
        case RenderBackend::OpenGL:    return "OpenGL";
        case RenderBackend::OpenGLES:  return "OpenGL ES";
        case RenderBackend::Vulkan:    return "Vulkan";
        case RenderBackend::Metal:     return "Metal";
        case RenderBackend::Direct3D12: return "Direct3D 12";
        case RenderBackend::WebGL:     return "WebGL";
        default:                       return "Unknown";
    }
}

/**
 * @brief Render backend capabilities
 */
struct RenderCapabilities {
    RenderBackend backend = RenderBackend::None;

    // Version info
    std::string vendorName;
    std::string rendererName;
    std::string apiVersion;
    std::string shadingLanguageVersion;

    // Texture capabilities
    int maxTextureSize = 0;
    int maxTextureUnits = 0;
    int max3DTextureSize = 0;
    int maxArrayTextureLayers = 0;
    int maxCubeMapSize = 0;
    bool hasTextureCompression = false;
    bool hasASTCCompression = false;
    bool hasBC7Compression = false;
    bool hasETC2Compression = false;

    // Shader capabilities
    int maxVertexAttributes = 0;
    int maxUniformBufferBindings = 0;
    int maxUniformBlockSize = 0;
    int maxComputeWorkGroupSize[3] = {0, 0, 0};
    bool hasGeometryShaders = false;
    bool hasTessellationShaders = false;
    bool hasComputeShaders = false;
    bool hasMeshShaders = false;
    bool hasRayTracing = false;

    // Framebuffer capabilities
    int maxColorAttachments = 0;
    int maxDrawBuffers = 0;
    int maxSamples = 0;
    bool hasMultisampling = false;
    bool hasIndependentBlend = false;

    // Memory info
    uint64_t dedicatedVideoMemory = 0;
    uint64_t sharedSystemMemory = 0;

    // Features
    bool hasInstancing = false;
    bool hasIndirectDraw = false;
    bool hasBindlessTextures = false;
    bool hasMultiDrawIndirect = false;
    bool hasConditionalRendering = false;
    bool hasAnisotropicFiltering = false;
    float maxAnisotropy = 1.0f;
};

/**
 * @brief Backend initialization configuration
 */
struct RenderConfig {
    RenderBackend preferredBackend = RenderBackend::None;  // Auto-select if None
    bool enableValidation = false;   // Enable debug/validation layers
    bool enableVSync = true;
    int multisamplingSamples = 4;    // MSAA samples (0 to disable)
    bool sRGBFramebuffer = true;
    bool doubleBuffering = true;

    // Vulkan-specific
    bool vulkanPreferDiscreteGPU = true;

    // OpenGL-specific
    int openGLMajorVersion = 4;
    int openGLMinorVersion = 6;
    bool openGLCoreProfile = true;
};

// ============================================================================
// Backend Detection
// ============================================================================

/**
 * @brief Check if a backend is available on current platform
 */
bool IsBackendAvailable(RenderBackend backend);

/**
 * @brief Get list of available backends on current platform
 */
std::vector<RenderBackend> GetAvailableBackends();

/**
 * @brief Get recommended backend for current platform
 */
RenderBackend GetRecommendedBackend();

/**
 * @brief Get default backends by platform
 *
 * Platform defaults:
 * - Windows: Vulkan > Direct3D12 > OpenGL
 * - Linux: Vulkan > OpenGL
 * - macOS: Metal > OpenGL
 * - iOS: Metal > OpenGLES
 * - Android: Vulkan > OpenGLES
 * - Web: WebGL
 */
RenderBackend GetPlatformDefaultBackend();

// ============================================================================
// Render Context Interface
// ============================================================================

/**
 * @brief Abstract render context
 *
 * Platform-specific implementations handle context creation,
 * swapchain management, and resource binding.
 */
class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize render context
     * @param config Configuration options
     * @param windowHandle Native window handle
     * @return true on success
     */
    virtual bool Initialize(const RenderConfig& config, void* windowHandle) = 0;

    /**
     * @brief Shutdown and release resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if context is valid
     */
    [[nodiscard]] virtual bool IsValid() const = 0;

    // -------------------------------------------------------------------------
    // Frame Management
    // -------------------------------------------------------------------------

    /**
     * @brief Begin new frame
     */
    virtual void BeginFrame() = 0;

    /**
     * @brief End frame and present
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Present to screen (swap buffers)
     */
    virtual void Present() = 0;

    /**
     * @brief Wait for GPU to finish
     */
    virtual void WaitIdle() = 0;

    // -------------------------------------------------------------------------
    // State Management
    // -------------------------------------------------------------------------

    /**
     * @brief Set viewport
     */
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    /**
     * @brief Set scissor rect
     */
    virtual void SetScissor(int x, int y, int width, int height) = 0;

    /**
     * @brief Clear framebuffer
     */
    virtual void Clear(float r, float g, float b, float a, float depth, uint8_t stencil) = 0;

    // -------------------------------------------------------------------------
    // Capabilities
    // -------------------------------------------------------------------------

    /**
     * @brief Get backend type
     */
    [[nodiscard]] virtual RenderBackend GetBackend() const = 0;

    /**
     * @brief Get render capabilities
     */
    [[nodiscard]] virtual const RenderCapabilities& GetCapabilities() const = 0;

    // -------------------------------------------------------------------------
    // Resize Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle window resize
     */
    virtual void OnResize(int width, int height) = 0;

    /**
     * @brief Get current framebuffer size
     */
    [[nodiscard]] virtual void GetFramebufferSize(int& width, int& height) const = 0;

    // -------------------------------------------------------------------------
    // VSync
    // -------------------------------------------------------------------------

    /**
     * @brief Set VSync mode
     */
    virtual void SetVSync(bool enabled) = 0;

    /**
     * @brief Get VSync state
     */
    [[nodiscard]] virtual bool IsVSyncEnabled() const = 0;

    // -------------------------------------------------------------------------
    // Native Handles (for interop)
    // -------------------------------------------------------------------------

    /**
     * @brief Get native device handle
     *
     * Returns:
     * - OpenGL: None (uses global state)
     * - Vulkan: VkDevice
     * - Metal: id<MTLDevice>
     * - D3D12: ID3D12Device*
     */
    [[nodiscard]] virtual void* GetNativeDevice() const = 0;

    /**
     * @brief Get native command queue/context
     *
     * Returns:
     * - OpenGL: None
     * - Vulkan: VkQueue
     * - Metal: id<MTLCommandQueue>
     * - D3D12: ID3D12CommandQueue*
     */
    [[nodiscard]] virtual void* GetNativeCommandQueue() const = 0;
};

// ============================================================================
// Factory
// ============================================================================

/**
 * @brief Create render context for specified backend
 * @param backend Render backend to use
 * @return Render context instance, or nullptr on failure
 */
std::unique_ptr<IRenderContext> CreateRenderContext(RenderBackend backend);

/**
 * @brief Create render context with automatic backend selection
 * @param config Configuration (preferredBackend used if specified)
 * @return Render context instance, or nullptr on failure
 */
std::unique_ptr<IRenderContext> CreateRenderContext(const RenderConfig& config = {});

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Check if backend supports compute shaders
 */
bool SupportsComputeShaders(RenderBackend backend);

/**
 * @brief Check if backend supports ray tracing
 */
bool SupportsRayTracing(RenderBackend backend);

/**
 * @brief Check if backend supports mesh shaders
 */
bool SupportsMeshShaders(RenderBackend backend);

/**
 * @brief Get recommended texture format for platform
 *
 * Returns the best compressed texture format:
 * - iOS/macOS: ASTC
 * - Android: ASTC or ETC2
 * - Windows/Linux: BC7 or BC3
 */
std::string GetRecommendedTextureFormat();

} // namespace Nova
