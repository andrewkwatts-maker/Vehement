#pragma once

#include "../../../../engine/terrain/VoxelTerrain.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

namespace Vehement {

/**
 * @brief Terrain edit job for async processing
 */
struct TerrainEditJob {
    enum class Type {
        ApplyBrush,
        RebuildMesh,
        ApplyRegion,
        Smooth,
        Flatten
    };

    Type type;
    glm::vec3 position;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    std::function<void()> operation;
    uint64_t timestamp = 0;
    int priority = 0;  // Higher = more important
};

/**
 * @brief Performance optimization for large terrain edits
 *
 * Features:
 * - Batch processing of edits
 * - Async mesh rebuilding
 * - Spatial partitioning for affected chunks
 * - Deferred mesh updates
 * - Edit coalescing (merge nearby edits)
 * - Level-of-detail for distant edits
 */
class TerrainEditOptimizer {
public:
    struct Config {
        bool enableBatching = true;
        bool enableAsyncProcessing = true;
        bool enableEditCoalescing = true;
        bool enableLOD = true;

        float batchInterval = 0.1f;        // Seconds between batch processing
        int maxEditsPerBatch = 50;
        float coalescingRadius = 2.0f;     // Merge edits within this radius
        int workerThreads = 2;
        int maxJobQueueSize = 1000;

        // LOD settings
        float lodDistance0 = 50.0f;        // Full detail
        float lodDistance1 = 100.0f;       // Medium detail
        float lodDistance2 = 200.0f;       // Low detail
    };

    TerrainEditOptimizer();
    ~TerrainEditOptimizer();

    /**
     * @brief Initialize optimizer
     */
    void Initialize(Nova::VoxelTerrain* terrain, const Config& config = {});

    /**
     * @brief Shutdown optimizer
     */
    void Shutdown();

    /**
     * @brief Queue terrain edit
     */
    void QueueEdit(const TerrainEditJob& job);

    /**
     * @brief Process queued edits
     */
    void ProcessEdits(float deltaTime, const glm::vec3& cameraPosition);

    /**
     * @brief Flush all pending edits immediately
     */
    void FlushEdits();

    /**
     * @brief Get number of pending edits
     */
    [[nodiscard]] size_t GetPendingEditCount() const;

    /**
     * @brief Get affected chunks that need mesh rebuild
     */
    [[nodiscard]] const std::unordered_set<glm::ivec3>& GetAffectedChunks() const { return m_affectedChunks; }

    /**
     * @brief Clear affected chunks list
     */
    void ClearAffectedChunks();

    /**
     * @brief Calculate optimal LOD level for distance
     */
    [[nodiscard]] int CalculateLOD(float distance) const;

    /**
     * @brief Check if position should use LOD
     */
    [[nodiscard]] bool ShouldUseLOD(const glm::vec3& position, const glm::vec3& cameraPosition) const;

private:
    // Process a single edit
    void ProcessEdit(const TerrainEditJob& job);

    // Coalesce nearby edits
    void CoalesceEdits();

    // Worker thread function
    void WorkerThreadFunc();

    // Get chunk position for world position
    glm::ivec3 WorldToChunk(const glm::vec3& worldPos) const;

    Config m_config;
    Nova::VoxelTerrain* m_terrain = nullptr;

    // Edit queue
    std::queue<TerrainEditJob> m_editQueue;
    std::mutex m_queueMutex;

    // Batching
    float m_batchTimer = 0.0f;
    std::vector<TerrainEditJob> m_currentBatch;

    // Affected chunks tracking
    std::unordered_set<glm::ivec3> m_affectedChunks;

    // Worker threads
    std::vector<std::thread> m_workerThreads;
    bool m_running = false;

    bool m_initialized = false;
};

/**
 * @brief Spatial hash grid for efficient edit coalescing
 */
class EditSpatialHash {
public:
    EditSpatialHash(float cellSize = 5.0f);

    /**
     * @brief Add edit to spatial hash
     */
    void AddEdit(const glm::vec3& position, const TerrainEditJob& job);

    /**
     * @brief Get nearby edits
     */
    [[nodiscard]] std::vector<TerrainEditJob*> GetNearbyEdits(const glm::vec3& position, float radius);

    /**
     * @brief Clear all edits
     */
    void Clear();

private:
    struct Cell {
        std::vector<TerrainEditJob> edits;
    };

    [[nodiscard]] glm::ivec3 WorldToCell(const glm::vec3& position) const;
    [[nodiscard]] uint64_t HashCell(const glm::ivec3& cell) const;

    float m_cellSize;
    std::unordered_map<uint64_t, Cell> m_cells;
};

/**
 * @brief Chunk dirty tracking for mesh rebuilds
 */
class ChunkDirtyTracker {
public:
    /**
     * @brief Mark chunk as dirty
     */
    void MarkDirty(const glm::ivec3& chunkPos);

    /**
     * @brief Mark region as dirty
     */
    void MarkRegionDirty(const glm::vec3& minBounds, const glm::vec3& maxBounds);

    /**
     * @brief Get dirty chunks
     */
    [[nodiscard]] const std::unordered_set<glm::ivec3>& GetDirtyChunks() const { return m_dirtyChunks; }

    /**
     * @brief Clear dirty chunks
     */
    void Clear();

    /**
     * @brief Get dirty chunks sorted by priority (distance to camera)
     */
    [[nodiscard]] std::vector<glm::ivec3> GetSortedDirtyChunks(const glm::vec3& cameraPosition) const;

private:
    std::unordered_set<glm::ivec3> m_dirtyChunks;
};

} // namespace Vehement

// Hash function for glm::ivec3
namespace std {
    template<>
    struct hash<glm::ivec3> {
        size_t operator()(const glm::ivec3& v) const {
            return ((std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1)) >> 1) ^ (std::hash<int>()(v.z) << 1);
        }
    };
}
