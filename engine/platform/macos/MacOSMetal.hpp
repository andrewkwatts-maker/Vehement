#pragma once

/**
 * @file MacOSMetal.hpp
 * @brief Metal rendering support for macOS
 *
 * Features:
 * - MTKView integration
 * - Command buffer management
 * - Shader compilation (MSL)
 * - Resource management
 * - Triple buffering
 */

#ifdef NOVA_PLATFORM_MACOS

#include "../Graphics.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace Nova {

/**
 * @brief Metal shader type
 */
enum class MetalShaderType {
    Vertex,
    Fragment,
    Compute,
    Kernel = Compute
};

/**
 * @brief Metal buffer usage hints
 */
enum class MetalBufferUsage {
    Default,           // CPU write once, GPU read
    Dynamic,           // CPU write frequently
    Static,            // GPU only
    Managed,           // CPU/GPU shared (macOS only)
    Private            // GPU private
};

/**
 * @brief Metal texture format
 */
enum class MetalTextureFormat {
    RGBA8Unorm,
    RGBA8Snorm,
    RGBA16Float,
    RGBA32Float,
    R8Unorm,
    R16Float,
    R32Float,
    RG8Unorm,
    RG16Float,
    RG32Float,
    Depth32Float,
    Depth24Stencil8,
    BGRA8Unorm,
    BGRA8Srgb
};

/**
 * @brief Metal render pass descriptor
 */
struct MetalRenderPassDesc {
    bool clearColor = true;
    float clearColorValue[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    bool clearDepth = true;
    float clearDepthValue = 1.0f;
    bool clearStencil = true;
    uint8_t clearStencilValue = 0;
};

/**
 * @brief Metal context for macOS
 */
class MacOSMetalContext : public GraphicsContext {
public:
    MacOSMetalContext();
    ~MacOSMetalContext() override;

    // Non-copyable
    MacOSMetalContext(const MacOSMetalContext&) = delete;
    MacOSMetalContext& operator=(const MacOSMetalContext&) = delete;

    /**
     * @brief Create Metal context for window
     * @param nsWindow NSWindow pointer
     * @return true if successful
     */
    bool Create(void* nsWindow);

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
    [[nodiscard]] GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Metal; }
    [[nodiscard]] const GraphicsCapabilities& GetCapabilities() const override { return m_capabilities; }

    // -------------------------------------------------------------------------
    // Metal-specific API
    // -------------------------------------------------------------------------

    /**
     * @brief Get Metal device (id<MTLDevice>)
     */
    [[nodiscard]] void* GetDevice() const;

    /**
     * @brief Get command queue (id<MTLCommandQueue>)
     */
    [[nodiscard]] void* GetCommandQueue() const;

    /**
     * @brief Get MTKView
     */
    [[nodiscard]] void* GetView() const;

    /**
     * @brief Get current drawable's texture
     */
    [[nodiscard]] void* GetCurrentDrawableTexture() const;

    /**
     * @brief Get depth stencil texture
     */
    [[nodiscard]] void* GetDepthStencilTexture() const;

    /**
     * @brief Begin frame rendering
     * @return Command buffer for this frame
     */
    void* BeginFrame();

    /**
     * @brief End frame and present
     */
    void EndFrame();

    /**
     * @brief Create render pass encoder
     * @param commandBuffer Command buffer
     * @param desc Render pass descriptor
     * @return Render encoder
     */
    void* CreateRenderEncoder(void* commandBuffer, const MetalRenderPassDesc& desc);

    /**
     * @brief End render pass encoder
     */
    void EndRenderEncoder(void* encoder);

    /**
     * @brief Create compute command encoder
     */
    void* CreateComputeEncoder(void* commandBuffer);

    /**
     * @brief End compute encoder
     */
    void EndComputeEncoder(void* encoder);

    // -------------------------------------------------------------------------
    // Resource Creation
    // -------------------------------------------------------------------------

    /**
     * @brief Create buffer
     * @param size Size in bytes
     * @param usage Buffer usage
     * @param data Initial data (optional)
     * @return Buffer handle (id<MTLBuffer>)
     */
    void* CreateBuffer(size_t size, MetalBufferUsage usage, const void* data = nullptr);

    /**
     * @brief Destroy buffer
     */
    void DestroyBuffer(void* buffer);

    /**
     * @brief Update buffer contents
     */
    void UpdateBuffer(void* buffer, const void* data, size_t size, size_t offset = 0);

    /**
     * @brief Create texture
     * @param width Texture width
     * @param height Texture height
     * @param format Pixel format
     * @param data Initial data (optional)
     * @return Texture handle (id<MTLTexture>)
     */
    void* CreateTexture(int width, int height, MetalTextureFormat format, const void* data = nullptr);

    /**
     * @brief Destroy texture
     */
    void DestroyTexture(void* texture);

    // -------------------------------------------------------------------------
    // Shader Management
    // -------------------------------------------------------------------------

    /**
     * @brief Compile shader from MSL source
     * @param source MSL source code
     * @param functionName Entry point function name
     * @param type Shader type
     * @return Shader function handle (id<MTLFunction>)
     */
    void* CompileShader(const std::string& source, const std::string& functionName, MetalShaderType type);

    /**
     * @brief Load shader from .metallib file
     */
    void* LoadShaderLibrary(const std::string& path);

    /**
     * @brief Get function from library
     */
    void* GetShaderFunction(void* library, const std::string& functionName);

    /**
     * @brief Create render pipeline state
     * @param vertexFunction Vertex shader function
     * @param fragmentFunction Fragment shader function
     * @return Pipeline state (id<MTLRenderPipelineState>)
     */
    void* CreateRenderPipeline(void* vertexFunction, void* fragmentFunction);

    /**
     * @brief Create compute pipeline state
     */
    void* CreateComputePipeline(void* computeFunction);

    /**
     * @brief Destroy pipeline state
     */
    void DestroyPipeline(void* pipeline);

    // -------------------------------------------------------------------------
    // Debug
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable Metal validation
     */
    void SetValidationEnabled(bool enabled);

    /**
     * @brief Enable/disable GPU capture (for debugging)
     */
    void EnableGPUCapture();

    /**
     * @brief Push debug group
     */
    void PushDebugGroup(void* encoder, const std::string& name);

    /**
     * @brief Pop debug group
     */
    void PopDebugGroup(void* encoder);

private:
    void QueryCapabilities();

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_vsync = true;
    bool m_validationEnabled = false;
};

/**
 * @brief Metal utility functions
 */
class MetalUtils {
public:
    /**
     * @brief Check if Metal is available
     */
    static bool IsMetalAvailable();

    /**
     * @brief Get list of Metal-capable devices
     */
    static std::vector<std::string> GetMetalDevices();

    /**
     * @brief Get default Metal device name
     */
    static std::string GetDefaultDeviceName();

    /**
     * @brief Check if device supports Apple Silicon features
     */
    static bool SupportsAppleSilicon();

    /**
     * @brief Get maximum buffer size
     */
    static size_t GetMaxBufferSize();

    /**
     * @brief Get maximum texture dimensions
     */
    static int GetMaxTextureDimension();

    /**
     * @brief Check if feature set is supported
     */
    static bool SupportsFeatureSet(int featureSet);
};

} // namespace Nova

#endif // NOVA_PLATFORM_MACOS
