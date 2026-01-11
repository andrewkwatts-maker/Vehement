#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Mesh;
class Material;
class Shader;
class Texture;

/**
 * @brief Key for batching draw calls by material state
 */
struct BatchKey {
    uint32_t shaderID = 0;
    uint32_t textureID = 0;      // Primary texture
    uint32_t normalMapID = 0;
    bool transparent = false;
    bool twoSided = false;

    bool operator==(const BatchKey& other) const {
        return shaderID == other.shaderID &&
               textureID == other.textureID &&
               normalMapID == other.normalMapID &&
               transparent == other.transparent &&
               twoSided == other.twoSided;
    }
};

struct BatchKeyHash {
    size_t operator()(const BatchKey& key) const {
        size_t h1 = std::hash<uint32_t>{}(key.shaderID);
        size_t h2 = std::hash<uint32_t>{}(key.textureID);
        size_t h3 = std::hash<uint32_t>{}(key.normalMapID);
        size_t h4 = std::hash<bool>{}(key.transparent);
        size_t h5 = std::hash<bool>{}(key.twoSided);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
    }
};

/**
 * @brief Instance data for instanced rendering
 */
struct InstanceData {
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;  // Pre-computed transpose(inverse(mat3(model)))
    glm::vec4 color;         // Per-instance color/tint
    uint32_t objectID;       // For picking/identification
    uint32_t padding[3];     // Alignment padding
};

/**
 * @brief A batch of objects to be rendered together
 */
struct RenderBatch {
    BatchKey key;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::vector<InstanceData> instances;

    // GPU buffers for instancing
    uint32_t instanceVBO = 0;
    uint32_t instanceCount = 0;
    bool dirty = true;  // Needs buffer update

    // Sorting key for state-based ordering
    uint64_t sortKey = 0;

    void Clear() {
        instances.clear();
        instanceCount = 0;
        dirty = true;
    }
};

/**
 * @brief Static geometry batch - pre-merged meshes
 */
struct StaticBatch {
    std::shared_ptr<Mesh> mergedMesh;
    std::shared_ptr<Material> material;
    glm::mat4 transform;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool valid = false;
};

/**
 * @brief Configuration for the batching system
 */
struct BatchConfig {
    bool enabled = true;
    uint32_t maxBatchSize = 1000;           // Max instances per batch
    uint32_t minInstancesForBatching = 2;   // Min objects to batch
    uint32_t maxVerticesPerStaticBatch = 65535;
    bool useInstancedRendering = true;
    bool usePersistentMapping = true;       // GL 4.4+
    bool useIndirectRendering = false;      // GL 4.3+ indirect draw
    float batchRebuildInterval = 1.0f;      // Seconds between batch rebuilds
};

/**
 * @brief Draw call batching system
 *
 * Automatically batches draw calls by material/texture to reduce
 * GPU state changes and improve rendering performance.
 */
class Batching {
public:
    Batching();
    ~Batching();

    // Non-copyable
    Batching(const Batching&) = delete;
    Batching& operator=(const Batching&) = delete;

    /**
     * @brief Initialize the batching system
     */
    bool Initialize(const BatchConfig& config = BatchConfig{});

    /**
     * @brief Shutdown and cleanup GPU resources
     */
    void Shutdown();

    /**
     * @brief Begin a new batch collection frame
     */
    void BeginFrame();

    /**
     * @brief End batch collection and prepare for rendering
     */
    void EndFrame();

    /**
     * @brief Submit an object for batching
     * @param mesh The mesh to render
     * @param material The material to use
     * @param transform World transform matrix
     * @param objectID Optional object ID for picking
     * @param color Optional per-instance color
     */
    void Submit(const std::shared_ptr<Mesh>& mesh,
                const std::shared_ptr<Material>& material,
                const glm::mat4& transform,
                uint32_t objectID = 0,
                const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Flush and render all collected batches
     * @param viewProjection The view-projection matrix
     */
    void Flush(const glm::mat4& viewProjection);

    /**
     * @brief Create a static batch from multiple meshes
     * @param meshes Vector of meshes to merge
     * @param materials Corresponding materials (first one used)
     * @param transforms World transforms for each mesh
     * @return Index of created static batch, or -1 on failure
     */
    int CreateStaticBatch(const std::vector<std::shared_ptr<Mesh>>& meshes,
                          const std::vector<std::shared_ptr<Material>>& materials,
                          const std::vector<glm::mat4>& transforms);

    /**
     * @brief Render a static batch
     */
    void RenderStaticBatch(int batchIndex, const glm::mat4& viewProjection);

    /**
     * @brief Remove a static batch
     */
    void RemoveStaticBatch(int batchIndex);

    /**
     * @brief Get batching statistics
     */
    struct Stats {
        uint32_t totalObjects = 0;
        uint32_t totalBatches = 0;
        uint32_t drawCallsSaved = 0;
        uint32_t instancedDrawCalls = 0;
        uint32_t staticBatches = 0;
        uint32_t verticesBatched = 0;
        float batchEfficiency = 0.0f;  // Percentage reduction

        void Reset() {
            totalObjects = totalBatches = drawCallsSaved = 0;
            instancedDrawCalls = staticBatches = verticesBatched = 0;
            batchEfficiency = 0.0f;
        }
    };

    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const BatchConfig& config);
    const BatchConfig& GetConfig() const { return m_config; }

    /**
     * @brief Check if instanced rendering is supported
     */
    bool IsInstancingSupported() const { return m_instancingSupported; }

    /**
     * @brief Check if indirect rendering is supported
     */
    bool IsIndirectSupported() const { return m_indirectSupported; }

private:
    struct MeshBatchGroup {
        std::unordered_map<BatchKey, RenderBatch, BatchKeyHash> batches;
    };

    // Batches grouped by mesh (for instancing)
    std::unordered_map<const Mesh*, MeshBatchGroup> m_meshBatches;

    // Static pre-merged batches
    std::vector<StaticBatch> m_staticBatches;

    // Indirect draw buffer (GL 4.3+)
    struct IndirectDrawCommand {
        uint32_t count;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t baseVertex;
        uint32_t baseInstance;
    };
    std::vector<IndirectDrawCommand> m_indirectCommands;
    uint32_t m_indirectBuffer = 0;

    // Persistent mapped buffer for instances (GL 4.4+)
    void* m_persistentBuffer = nullptr;
    uint32_t m_persistentBufferSize = 0;
    uint32_t m_persistentBufferOffset = 0;

    // Uniform Buffer Object for per-frame data
    uint32_t m_frameUBO = 0;
    struct FrameUniforms {
        glm::mat4 viewProjection;
        glm::vec4 cameraPosition;
        glm::vec4 time;  // x=time, y=deltaTime, z=frameCount, w=unused
    };

    BatchConfig m_config;
    Stats m_stats;

    bool m_initialized = false;
    bool m_instancingSupported = false;
    bool m_indirectSupported = false;
    bool m_persistentMappingSupported = false;

    void CreateInstanceBuffer(RenderBatch& batch);
    void UpdateInstanceBuffer(RenderBatch& batch);
    void DrawBatch(RenderBatch& batch, const glm::mat4& viewProjection);
    void DrawBatchInstanced(RenderBatch& batch, const glm::mat4& viewProjection);
    void SortBatches(std::vector<RenderBatch*>& batches);
    uint64_t ComputeSortKey(const BatchKey& key, float depth);

    // Mesh merging for static batches
    std::shared_ptr<Mesh> MergeMeshes(
        const std::vector<std::shared_ptr<Mesh>>& meshes,
        const std::vector<glm::mat4>& transforms);
};

} // namespace Nova
