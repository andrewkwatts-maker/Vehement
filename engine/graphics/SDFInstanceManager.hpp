#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Nova {

// Forward declarations
class SDFGPUEvaluator;
class SDFBrickCache;

/**
 * @brief Per-instance data for SDF rendering
 *
 * GPU-aligned structure (64 bytes)
 */
struct alignas(64) SDFInstance {
    glm::mat4 transform;            // World transform (48 bytes)
    glm::vec4 boundsCenter;         // xyz = center, w = radius (16 bytes)

    uint32_t programOffset;         // Bytecode program start offset
    uint32_t programLength;         // Number of instructions
    uint32_t materialBaseID;        // Base material index
    uint32_t flags;                 // Instance flags (LOD, culling, etc.)

    uint32_t brickCacheID;          // Brick cache reference (-1 if not cached)
    float lodBias;                  // LOD selection bias
    uint16_t lodLevel;              // Current LOD level (0 = highest)
    uint16_t padding;

    SDFInstance()
        : transform(1.0f)
        , boundsCenter(0.0f, 0.0f, 0.0f, 1.0f)
        , programOffset(0)
        , programLength(0)
        , materialBaseID(0)
        , flags(0)
        , brickCacheID(0xFFFFFFFF)
        , lodBias(0.0f)
        , lodLevel(0)
        , padding(0)
    {}
};

static_assert(sizeof(SDFInstance) == 128, "SDFInstance must be 128 bytes for alignment");

/**
 * @brief Instance flags
 */
enum SDFInstanceFlags : uint32_t {
    SDF_INSTANCE_VISIBLE = 1 << 0,          // Visible in current frame
    SDF_INSTANCE_CAST_SHADOW = 1 << 1,      // Casts shadows
    SDF_INSTANCE_USE_CACHE = 1 << 2,        // Use brick cache
    SDF_INSTANCE_DYNAMIC = 1 << 3,          // Transforms change frequently
    SDF_INSTANCE_HIGH_QUALITY = 1 << 4      // Force high quality rendering
};

/**
 * @brief LOD selection parameters
 */
struct SDFLODParams {
    float lodDistances[4] = { 10.0f, 25.0f, 50.0f, 100.0f };  // Distance thresholds
    float lodBias = 0.0f;                                      // Global LOD bias
    bool enableLOD = true;                                     // Enable automatic LOD
    bool enableOcclusion = true;                               // Enable occlusion culling
};

/**
 * @brief SDF Instance Manager
 *
 * Manages thousands of SDF instances with automatic:
 * - Instanced rendering (one SDF program, many transforms)
 * - LOD selection based on distance and screen size
 * - Frustum and occlusion culling
 * - Brick cache management for static geometry
 * - GPU-driven culling and LOD selection
 *
 * Performance:
 * - 10,000+ instances at 60 FPS
 * - Sub-1ms culling time
 * - Automatic quality scaling
 * - Minimal CPU overhead (GPU-driven)
 */
class SDFInstanceManager {
public:
    SDFInstanceManager();
    ~SDFInstanceManager();

    // Non-copyable
    SDFInstanceManager(const SDFInstanceManager&) = delete;
    SDFInstanceManager& operator=(const SDFInstanceManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize instance manager
     * @param evaluator SDF GPU evaluator (shared)
     * @param brickCache Brick cache system (optional)
     */
    bool Initialize(SDFGPUEvaluator* evaluator, SDFBrickCache* brickCache = nullptr);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Instance Management
    // =========================================================================

    /**
     * @brief Create new instance
     * @param transform World transform
     * @param programOffset Bytecode program offset
     * @param programLength Number of instructions
     * @param boundingRadius Bounding sphere radius
     * @return Instance handle
     */
    uint32_t CreateInstance(
        const glm::mat4& transform,
        uint32_t programOffset,
        uint32_t programLength,
        float boundingRadius
    );

    /**
     * @brief Remove instance
     */
    void RemoveInstance(uint32_t handle);

    /**
     * @brief Update instance transform
     */
    void UpdateInstanceTransform(uint32_t handle, const glm::mat4& transform);

    /**
     * @brief Set instance visibility
     */
    void SetInstanceVisible(uint32_t handle, bool visible);

    /**
     * @brief Set instance flags
     */
    void SetInstanceFlags(uint32_t handle, uint32_t flags);

    /**
     * @brief Get instance data
     */
    SDFInstance* GetInstance(uint32_t handle);
    const SDFInstance* GetInstance(uint32_t handle) const;

    /**
     * @brief Get instance count
     */
    uint32_t GetInstanceCount() const { return static_cast<uint32_t>(m_instances.size()); }

    /**
     * @brief Clear all instances
     */
    void ClearInstances();

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Update culling and LOD selection
     * @param viewMatrix View matrix
     * @param projMatrix Projection matrix
     * @param cameraPos Camera world position
     */
    void UpdateCullingAndLOD(
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec3& cameraPos
    );

    /**
     * @brief Bind instance buffer for rendering
     * @param binding SSBO binding point
     */
    void BindInstanceBuffer(uint32_t binding = 5);

    /**
     * @brief Get visible instance count (after culling)
     */
    uint32_t GetVisibleInstanceCount() const { return m_visibleInstanceCount; }

    /**
     * @brief Get visible instance buffer (indirect draw args)
     */
    uint32_t GetVisibleInstanceBuffer() const { return m_visibleInstanceSSBO; }

    // =========================================================================
    // LOD Configuration
    // =========================================================================

    /**
     * @brief Set LOD parameters
     */
    void SetLODParams(const SDFLODParams& params) { m_lodParams = params; }

    /**
     * @brief Get LOD parameters
     */
    const SDFLODParams& GetLODParams() const { return m_lodParams; }

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        uint32_t totalInstances = 0;
        uint32_t visibleInstances = 0;
        uint32_t culledByFrustum = 0;
        uint32_t culledByOcclusion = 0;
        uint32_t cachedInstances = 0;
        uint32_t lod0Count = 0;
        uint32_t lod1Count = 0;
        uint32_t lod2Count = 0;
        uint32_t lod3Count = 0;
        float cullingTimeMs = 0.0f;
        float updateTimeMs = 0.0f;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // CPU-side culling (fallback)
    void CPUCulling(
        const glm::mat4& viewProj,
        const glm::vec3& cameraPos
    );

    // GPU-driven culling
    void GPUCulling(
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec3& cameraPos
    );

    // Frustum culling helpers
    bool FrustumCullSphere(
        const glm::vec3& center,
        float radius,
        const glm::mat4& viewProj
    );

    // Calculate LOD level
    uint16_t CalculateLOD(
        const glm::vec3& instancePos,
        const glm::vec3& cameraPos,
        float boundingRadius
    );

    // Upload instances to GPU
    void UploadInstances();

    bool m_initialized = false;

    // SDF systems
    SDFGPUEvaluator* m_evaluator = nullptr;
    SDFBrickCache* m_brickCache = nullptr;

    // Instance data
    std::vector<SDFInstance> m_instances;
    std::vector<uint32_t> m_freeHandles;
    std::unordered_map<uint32_t, size_t> m_handleToIndex;

    // GPU buffers
    uint32_t m_instanceSSBO = 0;            // All instances
    uint32_t m_visibleInstanceSSBO = 0;     // Visible instances (after culling)
    uint32_t m_indirectDrawBuffer = 0;      // Indirect draw commands

    // Culling compute shader
    std::unique_ptr<class Shader> m_cullingShader;

    // LOD parameters
    SDFLODParams m_lodParams;

    // Cached state
    uint32_t m_visibleInstanceCount = 0;
    bool m_instancesDirty = true;

    // Statistics
    Stats m_stats;

    // Handle counter
    uint32_t m_nextHandle = 1;
};

} // namespace Nova
