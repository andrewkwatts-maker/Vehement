#pragma once

/**
 * @file IRenderPass.hpp
 * @brief Plugin-based render pass system interface for the Vehement SDF Engine
 *
 * This file defines the core interface for modular render passes that can be
 * dynamically registered, configured, and executed in a dependency-aware pipeline.
 *
 * Design Philosophy:
 * - Plugin-based: Passes can be registered/unregistered at runtime
 * - Dependency-aware: Automatic topological sorting based on declared dependencies
 * - Resource sharing: Shared resource pool for inter-pass communication
 * - SDF-first: Native support for SDF raymarching passes
 *
 * @see RenderPassRegistry For pass registration and management
 * @see RenderPipeline For pass execution and dependency graph building
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Texture;
class Framebuffer;
class Shader;
class Camera;
class Scene;
class GBuffer;

// ============================================================================
// Render Pass Priority
// ============================================================================

/**
 * @brief Priority levels for render pass ordering
 *
 * Lower values execute first. Passes with the same priority are ordered
 * by their declared dependencies. Custom passes should use values between
 * the defined priorities to insert themselves at specific points.
 */
enum class RenderPassPriority : uint32_t {
    PreDepth        = 100,      ///< Early depth pass for occlusion
    Shadow          = 200,      ///< Shadow map generation
    GBuffer         = 300,      ///< Deferred geometry pass
    SSAO            = 400,      ///< Screen-space ambient occlusion
    Lighting        = 500,      ///< Lighting calculations
    SDF             = 600,      ///< SDF raymarching passes
    Transparent     = 700,      ///< Alpha-blended geometry
    PostProcess     = 800,      ///< Post-processing effects
    UI              = 900,      ///< User interface overlay
    Debug           = 1000      ///< Debug visualization
};

// ============================================================================
// GPU Buffer Abstraction
// ============================================================================

/**
 * @brief GPU buffer type enumeration
 */
enum class BufferType : uint8_t {
    Vertex,         ///< Vertex buffer object
    Index,          ///< Index buffer object
    Uniform,        ///< Uniform buffer object
    Storage,        ///< Shader storage buffer object
    Indirect,       ///< Indirect draw buffer
    Constant        ///< Constant buffer (alias for Uniform)
};

/**
 * @brief GPU buffer usage hints
 */
enum class BufferUsage : uint8_t {
    Static,         ///< Data set once, used many times
    Dynamic,        ///< Data updated occasionally
    Stream          ///< Data updated every frame
};

/**
 * @brief GPU buffer wrapper for resource sharing between passes
 *
 * Abstracts OpenGL buffer objects to enable resource sharing between
 * render passes without exposing implementation details.
 */
class Buffer {
public:
    Buffer();
    ~Buffer();

    // Non-copyable, movable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    /**
     * @brief Create buffer with specified size and type
     * @param type Buffer type
     * @param size Size in bytes
     * @param data Initial data (optional)
     * @param usage Usage hint
     * @return true on success
     */
    bool Create(BufferType type, size_t size, const void* data = nullptr,
                BufferUsage usage = BufferUsage::Static);

    /**
     * @brief Update buffer data
     * @param data Source data
     * @param size Size in bytes
     * @param offset Offset into buffer
     */
    void Update(const void* data, size_t size, size_t offset = 0);

    /**
     * @brief Bind buffer to a binding point
     * @param bindingPoint Binding index for SSBO/UBO
     */
    void Bind(uint32_t bindingPoint = 0) const;

    /**
     * @brief Unbind buffer
     */
    void Unbind() const;

    /**
     * @brief Cleanup GPU resources
     */
    void Cleanup();

    // Getters
    [[nodiscard]] uint32_t GetID() const { return m_bufferID; }
    [[nodiscard]] BufferType GetType() const { return m_type; }
    [[nodiscard]] size_t GetSize() const { return m_size; }
    [[nodiscard]] bool IsValid() const { return m_bufferID != 0; }

private:
    uint32_t m_bufferID = 0;
    BufferType m_type = BufferType::Vertex;
    size_t m_size = 0;
};

// ============================================================================
// Render Target
// ============================================================================

/**
 * @brief Render target format
 */
enum class RenderTargetFormat : uint8_t {
    RGBA8,          ///< 8-bit RGBA (sRGB)
    RGBA16F,        ///< 16-bit float RGBA (HDR)
    RGBA32F,        ///< 32-bit float RGBA (high precision)
    RG16F,          ///< 16-bit float RG (motion vectors)
    R32F,           ///< 32-bit float R (depth)
    Depth24,        ///< 24-bit depth
    Depth32F,       ///< 32-bit float depth
    DepthStencil    ///< Depth + stencil combined
};

/**
 * @brief Render target for pass output
 *
 * Encapsulates a framebuffer with color and optional depth attachments.
 * Supports multiple render targets (MRT) for G-Buffer style rendering.
 */
class RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    // Non-copyable, movable
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    /**
     * @brief Create render target with specified dimensions
     * @param width Width in pixels
     * @param height Height in pixels
     * @param colorFormat Color attachment format
     * @param hasDepth Whether to create depth attachment
     * @param numColorAttachments Number of MRT color attachments
     * @return true on success
     */
    bool Create(int width, int height,
                RenderTargetFormat colorFormat = RenderTargetFormat::RGBA16F,
                bool hasDepth = true,
                int numColorAttachments = 1);

    /**
     * @brief Resize render target
     */
    void Resize(int width, int height);

    /**
     * @brief Bind for rendering
     */
    void Bind() const;

    /**
     * @brief Unbind (bind default framebuffer)
     */
    static void Unbind();

    /**
     * @brief Clear color and depth
     * @param color Clear color
     */
    void Clear(const glm::vec4& color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    /**
     * @brief Get color texture for a specific attachment
     * @param index Attachment index (0 for first)
     */
    [[nodiscard]] std::shared_ptr<Texture> GetColorTexture(int index = 0) const;

    /**
     * @brief Get depth texture
     */
    [[nodiscard]] std::shared_ptr<Texture> GetDepthTexture() const;

    /**
     * @brief Cleanup GPU resources
     */
    void Cleanup();

    // Getters
    [[nodiscard]] uint32_t GetFramebufferID() const { return m_fbo; }
    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }
    [[nodiscard]] bool IsValid() const { return m_fbo != 0; }
    [[nodiscard]] int GetColorAttachmentCount() const { return static_cast<int>(m_colorTextures.size()); }

private:
    uint32_t m_fbo = 0;
    int m_width = 0;
    int m_height = 0;
    RenderTargetFormat m_colorFormat = RenderTargetFormat::RGBA16F;
    std::vector<std::shared_ptr<Texture>> m_colorTextures;
    std::shared_ptr<Texture> m_depthTexture;
};

// ============================================================================
// Render Context
// ============================================================================

/**
 * @brief Render context providing access to rendering state and resources
 *
 * Passed to render passes during execution, providing access to camera,
 * viewport, and global rendering state.
 */
struct RenderContext {
    // Camera and viewport
    Camera* camera = nullptr;
    int viewportWidth = 1920;
    int viewportHeight = 1080;
    float deltaTime = 0.016f;
    float totalTime = 0.0f;

    // Frame information
    uint64_t frameNumber = 0;

    // View matrices (cached for convenience)
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};
    glm::mat4 inverseViewMatrix{1.0f};
    glm::mat4 inverseProjectionMatrix{1.0f};
    glm::mat4 previousViewProjectionMatrix{1.0f};

    // Camera properties
    glm::vec3 cameraPosition{0.0f};
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // Scene reference (optional)
    Scene* scene = nullptr;

    // Default render target (screen or main framebuffer)
    RenderTarget* defaultTarget = nullptr;

    // G-Buffer reference (for deferred passes)
    GBuffer* gBuffer = nullptr;
};

// ============================================================================
// Render Data
// ============================================================================

/**
 * @brief Data passed to render passes during execution
 *
 * Contains scene data, visibility information, and per-frame render data.
 */
struct RenderData {
    // Scene objects (visible after culling)
    struct DrawCall {
        uint32_t meshID = 0;
        uint32_t materialID = 0;
        glm::mat4 transform{1.0f};
        glm::mat4 previousTransform{1.0f};
        uint32_t objectID = 0;
        float depth = 0.0f;
        int lodLevel = 0;
    };

    std::vector<DrawCall> opaqueDrawCalls;
    std::vector<DrawCall> transparentDrawCalls;
    std::vector<DrawCall> sdfDrawCalls;

    // Light data
    struct LightData {
        glm::vec4 position;         // xyz = position, w = type
        glm::vec4 direction;        // xyz = direction, w = inner angle
        glm::vec4 color;            // rgb = color, a = intensity
        glm::vec4 params;           // x = outer angle, y = radius, z = shadow index, w = flags
    };

    std::vector<LightData> lights;
    glm::vec3 ambientLight{0.1f};

    // Shadow data
    struct ShadowData {
        glm::mat4 lightViewProjection;
        std::shared_ptr<Texture> shadowMap;
        float bias = 0.001f;
        float normalBias = 0.01f;
    };

    std::vector<ShadowData> shadows;

    // Environment
    std::shared_ptr<Texture> environmentMap;
    std::shared_ptr<Texture> irradianceMap;
    std::shared_ptr<Texture> prefilteredMap;
    std::shared_ptr<Texture> brdfLUT;

    // Post-processing parameters
    float exposure = 1.0f;
    float gamma = 2.2f;
    bool enableBloom = true;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.5f;

    // Debug flags
    bool showWireframe = false;
    bool showBoundingBoxes = false;
    bool showNormals = false;
};

// ============================================================================
// Render Pass Resources
// ============================================================================

/**
 * @brief Shared resources between render passes
 *
 * Provides a key-value store for textures, buffers, and render targets
 * that can be produced by one pass and consumed by another.
 */
struct RenderPassResources {
    /// Named texture resources (e.g., "SceneColor", "SceneDepth", "SSAO")
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;

    /// Named buffer resources (e.g., "LightBuffer", "CullingResults")
    std::unordered_map<std::string, std::shared_ptr<Buffer>> buffers;

    /// Current render target for the pass
    std::shared_ptr<RenderTarget> renderTarget;

    // Convenience accessors with null checking
    [[nodiscard]] std::shared_ptr<Texture> GetTexture(const std::string& name) const {
        auto it = textures.find(name);
        return (it != textures.end()) ? it->second : nullptr;
    }

    [[nodiscard]] std::shared_ptr<Buffer> GetBuffer(const std::string& name) const {
        auto it = buffers.find(name);
        return (it != buffers.end()) ? it->second : nullptr;
    }

    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture) {
        textures[name] = std::move(texture);
    }

    void SetBuffer(const std::string& name, std::shared_ptr<Buffer> buffer) {
        buffers[name] = std::move(buffer);
    }

    void Clear() {
        textures.clear();
        buffers.clear();
        renderTarget.reset();
    }
};

// ============================================================================
// Render Pass Interface
// ============================================================================

/**
 * @brief Abstract interface for render passes
 *
 * Implement this interface to create custom render passes that can be
 * registered with the RenderPassRegistry and executed by the RenderPipeline.
 *
 * Lifecycle:
 * 1. Initialize() - Called once when pass is registered
 * 2. Setup() - Called before Execute() to prepare resources
 * 3. Execute() - Called to perform actual rendering
 * 4. Cleanup() - Called after Execute() to release temporary resources
 * 5. Shutdown() - Called once when pass is unregistered
 *
 * Example Implementation:
 * @code
 *     class MyCustomPass : public IRenderPass {
 *     public:
 *         bool Initialize(RenderContext& ctx) override {
 *             m_shader = std::make_unique<Shader>();
 *             return m_shader->Load("shaders/my_pass.vert", "shaders/my_pass.frag");
 *         }
 *
 *         void Execute(RenderContext& ctx, const RenderData& data) override {
 *             m_shader->Use();
 *             // Render...
 *         }
 *
 *         std::string_view GetName() const override { return "MyCustomPass"; }
 *         RenderPassPriority GetPriority() const override { return RenderPassPriority::PostProcess; }
 *         std::vector<std::string> GetDependencies() const override { return {"Lighting"}; }
 *         std::vector<std::string> GetOutputs() const override { return {"MyOutput"}; }
 *         // ...
 *     };
 * @endcode
 */
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize the render pass
     *
     * Called once when the pass is registered. Use this to create shaders,
     * allocate GPU resources, and set up initial state.
     *
     * @param ctx Render context
     * @return true on success, false on failure (pass will not be registered)
     */
    virtual bool Initialize(RenderContext& ctx) = 0;

    /**
     * @brief Shutdown the render pass
     *
     * Called once when the pass is unregistered. Use this to release all
     * GPU resources created during Initialize().
     */
    virtual void Shutdown() = 0;

    // ========================================================================
    // Execution
    // ========================================================================

    /**
     * @brief Setup resources before execution
     *
     * Called before Execute() to prepare resources. Use this to bind
     * input textures from the shared resource pool and configure
     * the render target.
     *
     * @param ctx Render context
     * @param resources Shared resource pool
     */
    virtual void Setup(RenderContext& ctx, RenderPassResources& resources) = 0;

    /**
     * @brief Execute the render pass
     *
     * Perform actual rendering. This is where draw calls and compute
     * dispatches happen.
     *
     * @param ctx Render context
     * @param data Render data (scene objects, lights, etc.)
     */
    virtual void Execute(RenderContext& ctx, const RenderData& data) = 0;

    /**
     * @brief Cleanup after execution
     *
     * Called after Execute() to release temporary resources or restore
     * GPU state. Output textures should be added to the resource pool here.
     *
     * @param ctx Render context
     */
    virtual void Cleanup(RenderContext& ctx) = 0;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Enable or disable the pass
     *
     * Disabled passes are skipped during pipeline execution.
     *
     * @param enabled true to enable, false to disable
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * @brief Check if the pass is enabled
     * @return true if enabled
     */
    [[nodiscard]] virtual bool IsEnabled() const = 0;

    // ========================================================================
    // Information
    // ========================================================================

    /**
     * @brief Get the unique name of this pass
     *
     * Used for dependency resolution and debugging. Must be unique among
     * all registered passes.
     *
     * @return Pass name as string_view
     */
    [[nodiscard]] virtual std::string_view GetName() const = 0;

    /**
     * @brief Get the execution priority
     *
     * Passes are sorted by priority first, then by dependencies.
     *
     * @return Priority level
     */
    [[nodiscard]] virtual RenderPassPriority GetPriority() const = 0;

    /**
     * @brief Get names of passes this pass depends on
     *
     * The pipeline ensures all dependencies are executed before this pass.
     * Return an empty vector if there are no dependencies.
     *
     * @return Vector of dependency pass names
     */
    [[nodiscard]] virtual std::vector<std::string> GetDependencies() const = 0;

    /**
     * @brief Get names of resources this pass outputs
     *
     * Used for dependency graph construction and resource lifetime management.
     *
     * @return Vector of output resource names
     */
    [[nodiscard]] virtual std::vector<std::string> GetOutputs() const = 0;

    // ========================================================================
    // Debug
    // ========================================================================

    /**
     * @brief Render debug UI for this pass
     *
     * Called when the debug overlay is visible. Use ImGui to render
     * pass-specific controls and statistics.
     */
    virtual void RenderDebugUI() = 0;
};

// ============================================================================
// Base Render Pass Implementation
// ============================================================================

/**
 * @brief Base class providing common functionality for render passes
 *
 * Inherit from this class instead of IRenderPass directly for convenience.
 * Provides default implementations for common methods.
 */
class RenderPassBase : public IRenderPass {
public:
    RenderPassBase(std::string_view name, RenderPassPriority priority)
        : m_name(name), m_priority(priority) {}

    ~RenderPassBase() override = default;

    // Default lifecycle implementations
    void Setup(RenderContext& ctx, RenderPassResources& resources) override {
        (void)ctx;
        (void)resources;
    }

    void Cleanup(RenderContext& ctx) override {
        (void)ctx;
    }

    // Configuration
    void SetEnabled(bool enabled) override { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const override { return m_enabled; }

    // Information
    [[nodiscard]] std::string_view GetName() const override { return m_name; }
    [[nodiscard]] RenderPassPriority GetPriority() const override { return m_priority; }
    [[nodiscard]] std::vector<std::string> GetDependencies() const override { return m_dependencies; }
    [[nodiscard]] std::vector<std::string> GetOutputs() const override { return m_outputs; }

    // Debug (default empty implementation)
    void RenderDebugUI() override {}

protected:
    void AddDependency(const std::string& passName) {
        m_dependencies.push_back(passName);
    }

    void AddOutput(const std::string& resourceName) {
        m_outputs.push_back(resourceName);
    }

    std::string m_name;
    RenderPassPriority m_priority;
    bool m_enabled = true;
    std::vector<std::string> m_dependencies;
    std::vector<std::string> m_outputs;
};

// ============================================================================
// SDF-Specific Render Pass Interface
// ============================================================================

/**
 * @brief Extended interface for SDF raymarching passes
 *
 * Provides additional methods specific to SDF rendering, such as
 * acceleration structure access and ray configuration.
 */
class ISDFRenderPass : public IRenderPass {
public:
    ~ISDFRenderPass() override = default;

    /**
     * @brief Set the maximum raymarching steps
     * @param steps Maximum number of raymarching iterations
     */
    virtual void SetMaxRaymarchSteps(int steps) = 0;

    /**
     * @brief Get the maximum raymarching steps
     */
    [[nodiscard]] virtual int GetMaxRaymarchSteps() const = 0;

    /**
     * @brief Set the hit threshold for raymarching
     * @param threshold Distance threshold for surface hits
     */
    virtual void SetHitThreshold(float threshold) = 0;

    /**
     * @brief Get the hit threshold
     */
    [[nodiscard]] virtual float GetHitThreshold() const = 0;

    /**
     * @brief Set the maximum ray distance
     * @param distance Maximum distance rays can travel
     */
    virtual void SetMaxRayDistance(float distance) = 0;

    /**
     * @brief Get the maximum ray distance
     */
    [[nodiscard]] virtual float GetMaxRayDistance() const = 0;

    /**
     * @brief Enable/disable SDF acceleration structures
     * @param enabled true to use acceleration (octree, BVH)
     */
    virtual void SetAccelerationEnabled(bool enabled) = 0;

    /**
     * @brief Check if acceleration is enabled
     */
    [[nodiscard]] virtual bool IsAccelerationEnabled() const = 0;
};

// ============================================================================
// Render Pass Event Callbacks
// ============================================================================

/**
 * @brief Callback types for render pass events
 */
using RenderPassCallback = std::function<void(IRenderPass*)>;
using RenderPassResourceCallback = std::function<void(IRenderPass*, RenderPassResources&)>;

/**
 * @brief Event dispatcher for render pass lifecycle events
 */
class RenderPassEventDispatcher {
public:
    void OnPassRegistered(RenderPassCallback callback) {
        m_onRegistered.push_back(std::move(callback));
    }

    void OnPassUnregistered(RenderPassCallback callback) {
        m_onUnregistered.push_back(std::move(callback));
    }

    void OnPassExecuted(RenderPassCallback callback) {
        m_onExecuted.push_back(std::move(callback));
    }

    void OnResourcesReady(RenderPassResourceCallback callback) {
        m_onResourcesReady.push_back(std::move(callback));
    }

    // Internal dispatch methods
    void DispatchRegistered(IRenderPass* pass) {
        for (auto& cb : m_onRegistered) cb(pass);
    }

    void DispatchUnregistered(IRenderPass* pass) {
        for (auto& cb : m_onUnregistered) cb(pass);
    }

    void DispatchExecuted(IRenderPass* pass) {
        for (auto& cb : m_onExecuted) cb(pass);
    }

    void DispatchResourcesReady(IRenderPass* pass, RenderPassResources& resources) {
        for (auto& cb : m_onResourcesReady) cb(pass, resources);
    }

private:
    std::vector<RenderPassCallback> m_onRegistered;
    std::vector<RenderPassCallback> m_onUnregistered;
    std::vector<RenderPassCallback> m_onExecuted;
    std::vector<RenderPassResourceCallback> m_onResourcesReady;
};

} // namespace Nova
