#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Shader type enumeration
 */
enum class ShaderType {
    Vertex,
    Fragment,
    Compute
};

/**
 * @brief Metal pipeline state descriptor
 */
struct MetalPipelineDesc {
    std::string vertexFunction;
    std::string fragmentFunction;
    uint32_t colorPixelFormat = 80;  // MTLPixelFormatBGRA8Unorm
    uint32_t depthPixelFormat = 252; // MTLPixelFormatDepth32Float
    uint32_t stencilPixelFormat = 0;
    bool depthWriteEnabled = true;
    bool blendingEnabled = false;
    int sampleCount = 1;
};

/**
 * @brief Metal buffer wrapper
 */
struct MetalBuffer {
    void* buffer = nullptr;         // id<MTLBuffer>
    size_t size = 0;
    bool isPrivate = false;
};

/**
 * @brief Metal texture wrapper
 */
struct MetalTexture {
    void* texture = nullptr;        // id<MTLTexture>
    int width = 0;
    int height = 0;
    int format = 0;
};

/**
 * @brief Metal render pipeline wrapper
 */
struct MetalPipeline {
    void* pipelineState = nullptr;  // id<MTLRenderPipelineState>
    void* depthState = nullptr;     // id<MTLDepthStencilState>
    std::string name;
};

/**
 * @brief Metal renderer for iOS
 *
 * Provides high-performance rendering using Apple's Metal API.
 * Supports shader compilation, buffer management, and render pipeline creation.
 */
class MetalRenderer {
public:
    MetalRenderer();
    ~MetalRenderer();

    // Non-copyable
    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize Metal renderer
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if renderer is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Get the Metal device (id<MTLDevice>)
     */
    [[nodiscard]] void* GetDevice() const { return m_device; }

    /**
     * @brief Get the command queue (id<MTLCommandQueue>)
     */
    [[nodiscard]] void* GetCommandQueue() const { return m_commandQueue; }

    // =========================================================================
    // Render Pipeline
    // =========================================================================

    /**
     * @brief Create a render pipeline from shaders
     * @param name Pipeline name for caching
     * @param desc Pipeline descriptor
     * @return true if pipeline was created
     */
    bool CreatePipeline(const std::string& name, const MetalPipelineDesc& desc);

    /**
     * @brief Create a pipeline from vertex and fragment shader source
     * @param name Pipeline name
     * @param vertexShader Metal shader source for vertex function
     * @param fragmentShader Metal shader source for fragment function
     * @return true if pipeline was created
     */
    bool CreatePipeline(const std::string& name,
                        const std::string& vertexShader,
                        const std::string& fragmentShader);

    /**
     * @brief Get a cached pipeline by name
     */
    [[nodiscard]] MetalPipeline* GetPipeline(const std::string& name);

    /**
     * @brief Destroy a pipeline
     */
    void DestroyPipeline(const std::string& name);

    // =========================================================================
    // Shader Compilation
    // =========================================================================

    /**
     * @brief Compile Metal shader source
     * @param source Metal shading language source
     * @return Compiled library (id<MTLLibrary>), or nullptr on failure
     */
    void* CompileShader(const std::string& source);

    /**
     * @brief Convert GLSL shader to Metal shading language
     * @param glsl GLSL shader source
     * @param type Shader type (vertex/fragment)
     * @return Metal shading language source
     */
    std::string ConvertGLSLToMetal(const std::string& glsl, ShaderType type);

    /**
     * @brief Load a precompiled Metal library from a file
     * @param path Path to .metallib file
     * @return Library handle (id<MTLLibrary>)
     */
    void* LoadMetalLibrary(const std::string& path);

    // =========================================================================
    // Buffer Management
    // =========================================================================

    /**
     * @brief Create a buffer with data
     * @param data Pointer to data
     * @param size Size in bytes
     * @param isPrivate Use private (GPU-only) storage
     * @return Buffer handle
     */
    MetalBuffer CreateBuffer(const void* data, size_t size, bool isPrivate = false);

    /**
     * @brief Create an empty buffer
     * @param size Size in bytes
     * @param isPrivate Use private (GPU-only) storage
     * @return Buffer handle
     */
    MetalBuffer CreateBuffer(size_t size, bool isPrivate = false);

    /**
     * @brief Update buffer contents
     * @param buffer Buffer to update
     * @param data New data
     * @param size Data size
     * @param offset Offset in buffer
     */
    void UpdateBuffer(MetalBuffer& buffer, const void* data, size_t size, size_t offset = 0);

    /**
     * @brief Destroy a buffer
     */
    void DestroyBuffer(MetalBuffer& buffer);

    // =========================================================================
    // Texture Management
    // =========================================================================

    /**
     * @brief Create a 2D texture
     * @param width Texture width
     * @param height Texture height
     * @param format Pixel format
     * @param data Optional initial data
     * @return Texture handle
     */
    MetalTexture CreateTexture2D(int width, int height, int format, const void* data = nullptr);

    /**
     * @brief Update texture contents
     */
    void UpdateTexture(MetalTexture& texture, const void* data, int width, int height);

    /**
     * @brief Destroy a texture
     */
    void DestroyTexture(MetalTexture& texture);

    // =========================================================================
    // Frame Rendering
    // =========================================================================

    /**
     * @brief Set the drawable for rendering (CAMetalDrawable)
     */
    void SetDrawable(void* drawable);

    /**
     * @brief Begin a new frame
     */
    void BeginFrame();

    /**
     * @brief End the current frame and present
     */
    void EndFrame();

    /**
     * @brief Get the current command buffer (id<MTLCommandBuffer>)
     */
    [[nodiscard]] void* GetCurrentCommandBuffer() const { return m_currentCommandBuffer; }

    /**
     * @brief Get the current render encoder (id<MTLRenderCommandEncoder>)
     */
    [[nodiscard]] void* GetCurrentRenderEncoder() const { return m_currentRenderEncoder; }

    // =========================================================================
    // Drawing
    // =========================================================================

    /**
     * @brief Set the current render pipeline
     */
    void SetPipeline(const std::string& name);

    /**
     * @brief Set vertex buffer at index
     */
    void SetVertexBuffer(const MetalBuffer& buffer, size_t offset, int index);

    /**
     * @brief Set vertex bytes (small data, copied directly)
     */
    void SetVertexBytes(const void* data, size_t size, int index);

    /**
     * @brief Set fragment buffer at index
     */
    void SetFragmentBuffer(const MetalBuffer& buffer, size_t offset, int index);

    /**
     * @brief Set fragment texture at index
     */
    void SetFragmentTexture(const MetalTexture& texture, int index);

    /**
     * @brief Draw primitives
     */
    void Draw(int primitiveType, int vertexStart, int vertexCount);

    /**
     * @brief Draw indexed primitives
     */
    void DrawIndexed(int primitiveType, int indexCount, int indexType,
                     const MetalBuffer& indexBuffer, size_t indexBufferOffset);

    /**
     * @brief Draw instanced primitives
     */
    void DrawInstanced(int primitiveType, int vertexStart, int vertexCount, int instanceCount);

    // =========================================================================
    // State
    // =========================================================================

    /**
     * @brief Set viewport
     */
    void SetViewport(float x, float y, float width, float height,
                     float nearDepth = 0.0f, float farDepth = 1.0f);

    /**
     * @brief Set scissor rectangle
     */
    void SetScissorRect(int x, int y, int width, int height);

    /**
     * @brief Set cull mode (0=None, 1=Front, 2=Back)
     */
    void SetCullMode(int mode);

    /**
     * @brief Set front face winding (0=Clockwise, 1=CounterClockwise)
     */
    void SetFrontFace(int winding);

    /**
     * @brief Set depth bias
     */
    void SetDepthBias(float depthBias, float slopeScale, float clamp);

    /**
     * @brief Set blend color
     */
    void SetBlendColor(float r, float g, float b, float a);

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Get Metal device name
     */
    [[nodiscard]] std::string GetDeviceName() const;

    /**
     * @brief Check if device supports feature set
     */
    [[nodiscard]] bool SupportsFeatureSet(int featureSet) const;

    /**
     * @brief Get maximum texture size
     */
    [[nodiscard]] int GetMaxTextureSize() const;

    /**
     * @brief Get maximum buffer size
     */
    [[nodiscard]] size_t GetMaxBufferSize() const;

private:
    // Metal objects (Objective-C types stored as void*)
    void* m_device = nullptr;           // id<MTLDevice>
    void* m_commandQueue = nullptr;     // id<MTLCommandQueue>
    void* m_defaultLibrary = nullptr;   // id<MTLLibrary>

    // Current frame state
    void* m_currentDrawable = nullptr;      // id<CAMetalDrawable>
    void* m_currentCommandBuffer = nullptr; // id<MTLCommandBuffer>
    void* m_currentRenderEncoder = nullptr; // id<MTLRenderCommandEncoder>
    void* m_depthTexture = nullptr;         // id<MTLTexture>

    // Cached pipelines
    std::unordered_map<std::string, MetalPipeline> m_pipelines;

    // State
    bool m_initialized = false;
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;
    float m_displayScale = 1.0f;

    // Helpers
    void CreateDepthTexture(int width, int height);
    void* CreateDepthStencilState(bool depthWrite, int compareFunc);
};

} // namespace Nova
