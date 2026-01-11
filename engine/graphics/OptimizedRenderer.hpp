#pragma once

#include "graphics/Batching.hpp"
#include "graphics/Culler.hpp"
#include "graphics/LODManager.hpp"
#include "graphics/TextureAtlas.hpp"
#include "graphics/RenderQueue.hpp"

#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Renderer;
class Camera;
class Mesh;
class Material;
class SceneNode;

/**
 * @brief Unified graphics settings loaded from config
 */
struct GraphicsSettings {
    // Batching
    BatchConfig batchConfig;

    // Culling
    CullingConfig cullingConfig;

    // LOD
    LODConfig lodConfig;

    // Texture Atlas
    TextureAtlasConfig atlasConfig;

    // Render Queue
    RenderQueueConfig queueConfig;

    // Instancing
    bool instancingEnabled = true;
    int minInstanceCount = 10;

    // Quality preset
    std::string qualityPreset = "high";

    // Debug
    bool showStats = false;
    bool logPerformanceWarnings = true;
    float performanceWarningThresholdMs = 16.67f;
};

/**
 * @brief Combined performance statistics
 */
struct PerformanceStats {
    // Frame timing
    float frameTimeMs = 0.0f;
    float cpuTimeMs = 0.0f;
    float gpuTimeMs = 0.0f;

    // Draw calls
    uint32_t totalDrawCalls = 0;
    uint32_t batchedDrawCalls = 0;
    uint32_t instancedDrawCalls = 0;
    uint32_t drawCallsSaved = 0;

    // Geometry
    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;
    uint32_t verticesAfterLOD = 0;
    uint32_t trianglesAfterLOD = 0;

    // Culling
    uint32_t totalObjects = 0;
    uint32_t visibleObjects = 0;
    uint32_t frustumCulled = 0;
    uint32_t occlusionCulled = 0;
    uint32_t distanceCulled = 0;
    float cullingEfficiency = 0.0f;

    // State changes
    uint32_t shaderChanges = 0;
    uint32_t materialChanges = 0;
    uint32_t textureChanges = 0;
    uint32_t stateChanges = 0;

    // Memory
    uint32_t textureMemoryMB = 0;
    uint32_t meshMemoryMB = 0;
    uint32_t bufferMemoryMB = 0;

    void Reset() {
        frameTimeMs = cpuTimeMs = gpuTimeMs = 0.0f;
        totalDrawCalls = batchedDrawCalls = instancedDrawCalls = drawCallsSaved = 0;
        totalVertices = totalTriangles = verticesAfterLOD = trianglesAfterLOD = 0;
        totalObjects = visibleObjects = frustumCulled = occlusionCulled = distanceCulled = 0;
        cullingEfficiency = 0.0f;
        shaderChanges = materialChanges = textureChanges = stateChanges = 0;
        textureMemoryMB = meshMemoryMB = bufferMemoryMB = 0;
    }

    // Aggregate from subsystems
    void Aggregate(const Batching::Stats& batchStats,
                   const Culler::Stats& cullStats,
                   const LODStats& lodStats,
                   const RenderQueueStats& queueStats);
};

/**
 * @brief Optimized renderer integrating all performance systems
 *
 * Combines batching, culling, LOD, and render queue for
 * maximum rendering performance.
 */
class OptimizedRenderer {
public:
    OptimizedRenderer();
    ~OptimizedRenderer();

    // Non-copyable
    OptimizedRenderer(const OptimizedRenderer&) = delete;
    OptimizedRenderer& operator=(const OptimizedRenderer&) = delete;

    /**
     * @brief Initialize all optimization systems
     * @param renderer The base renderer to use
     * @param configPath Path to graphics config JSON (optional)
     */
    bool Initialize(Renderer* renderer, const std::string& configPath = "");

    /**
     * @brief Shutdown all systems
     */
    void Shutdown();

    /**
     * @brief Load settings from config file
     */
    bool LoadSettings(const std::string& configPath);

    /**
     * @brief Apply a quality preset
     */
    void ApplyQualityPreset(const std::string& preset);

    /**
     * @brief Begin a new optimized frame
     */
    void BeginFrame(const Camera& camera);

    /**
     * @brief End frame and gather statistics
     */
    void EndFrame();

    /**
     * @brief Submit a scene node for rendering
     */
    void Submit(SceneNode* node);

    /**
     * @brief Submit a mesh with material and transform
     */
    void Submit(const std::shared_ptr<Mesh>& mesh,
                const std::shared_ptr<Material>& material,
                const glm::mat4& transform,
                uint32_t objectID = 0);

    /**
     * @brief Submit with LOD group
     */
    void SubmitWithLOD(uint32_t lodGroupID,
                       const std::shared_ptr<Material>& material,
                       const glm::mat4& transform);

    /**
     * @brief Execute all rendering
     */
    void Render();

    /**
     * @brief Render shadows
     */
    void RenderShadows(const glm::mat4& lightViewProjection);

    /**
     * @brief Render opaque objects
     */
    void RenderOpaque();

    /**
     * @brief Render transparent objects
     */
    void RenderTransparent();

    // Subsystem access
    Batching& GetBatching() { return *m_batching; }
    Culler& GetCuller() { return *m_culler; }
    LODManager& GetLODManager() { return *m_lodManager; }
    TextureAtlas& GetTextureAtlas() { return *m_textureAtlas; }
    RenderQueue& GetRenderQueue() { return *m_renderQueue; }

    const Batching& GetBatching() const { return *m_batching; }
    const Culler& GetCuller() const { return *m_culler; }
    const LODManager& GetLODManager() const { return *m_lodManager; }
    const TextureAtlas& GetTextureAtlas() const { return *m_textureAtlas; }
    const RenderQueue& GetRenderQueue() const { return *m_renderQueue; }

    /**
     * @brief Get performance statistics
     */
    const PerformanceStats& GetStats() const { return m_stats; }

    /**
     * @brief Get current settings
     */
    const GraphicsSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Update settings
     */
    void SetSettings(const GraphicsSettings& settings);

    /**
     * @brief Check if system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Register a static batch
     */
    int RegisterStaticBatch(const std::vector<std::shared_ptr<Mesh>>& meshes,
                            const std::vector<std::shared_ptr<Material>>& materials,
                            const std::vector<glm::mat4>& transforms);

    /**
     * @brief Create LOD group
     */
    uint32_t CreateLODGroup(const std::string& name = "");

    /**
     * @brief Add LOD level to group
     */
    bool AddLODLevel(uint32_t groupID, std::shared_ptr<Mesh> mesh, float maxDistance = 0.0f);

    /**
     * @brief Add texture to atlas
     */
    bool AddTextureToAtlas(const std::string& name, const std::shared_ptr<Texture>& texture);

    /**
     * @brief Build texture atlas
     */
    bool BuildTextureAtlas();

    /**
     * @brief Register cullable object
     */
    uint32_t RegisterCullable(const AABB& bounds, void* userData = nullptr);

    /**
     * @brief Update cullable bounds
     */
    void UpdateCullableBounds(uint32_t id, const AABB& newBounds);

private:
    void DrawItem(const RenderItem& item);
    void CollectSceneNode(SceneNode* node, const glm::mat4& parentTransform);
    void UpdatePerformanceStats();

    // Subsystems
    std::unique_ptr<Batching> m_batching;
    std::unique_ptr<Culler> m_culler;
    std::unique_ptr<LODManager> m_lodManager;
    std::unique_ptr<TextureAtlas> m_textureAtlas;
    std::unique_ptr<RenderQueue> m_renderQueue;

    // Base renderer
    Renderer* m_renderer = nullptr;

    // Camera data
    const Camera* m_activeCamera = nullptr;
    glm::mat4 m_viewProjection{1.0f};

    // Settings and stats
    GraphicsSettings m_settings;
    PerformanceStats m_stats;

    // Frame timing
    uint64_t m_frameStartTime = 0;
    uint64_t m_frameNumber = 0;

    // GPU timer queries
    uint32_t m_gpuTimerQuery = 0;
    bool m_gpuTimerAvailable = false;

    bool m_initialized = false;
};

/**
 * @brief RAII helper for scoped render passes
 */
class ScopedRenderPass {
public:
    ScopedRenderPass(OptimizedRenderer& renderer, RenderPass pass,
                     const std::string& name = "");
    ~ScopedRenderPass();

private:
    OptimizedRenderer& m_renderer;
    RenderPass m_pass;
    uint64_t m_startTime;
};

/**
 * @brief GPU timer for performance measurement
 */
class GPUTimer {
public:
    GPUTimer();
    ~GPUTimer();

    void Begin();
    void End();
    float GetElapsedMs() const;
    bool IsResultAvailable() const;

private:
    uint32_t m_queryStart = 0;
    uint32_t m_queryEnd = 0;
    mutable float m_cachedResult = 0.0f;
    mutable bool m_resultCached = false;
};

/**
 * @brief Performance profiler for render optimization
 */
class RenderProfiler {
public:
    static RenderProfiler& Instance();

    void BeginFrame();
    void EndFrame();

    void BeginSection(const std::string& name);
    void EndSection(const std::string& name);

    void RecordDrawCall(uint32_t triangles);
    void RecordStateChange();
    void RecordTextureChange();

    struct SectionStats {
        std::string name;
        float totalTimeMs = 0.0f;
        float avgTimeMs = 0.0f;
        int callCount = 0;
    };

    const std::vector<SectionStats>& GetSections() const { return m_sections; }

    void Reset();

private:
    RenderProfiler() = default;

    std::vector<SectionStats> m_sections;
    std::unordered_map<std::string, int> m_sectionIndices;
    std::vector<uint64_t> m_sectionStartTimes;

    uint32_t m_drawCalls = 0;
    uint32_t m_stateChanges = 0;
    uint32_t m_triangles = 0;
};

} // namespace Nova
