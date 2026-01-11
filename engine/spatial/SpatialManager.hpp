#pragma once

#include "SpatialIndex.hpp"
#include "Octree.hpp"
#include "BVH.hpp"
#include "SpatialHash3D.hpp"
#include "Frustum.hpp"
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include <array>

namespace Nova {

// Forward declarations
class DebugRenderer;

/**
 * @brief Spatial layer identifiers for game object categories
 */
enum class SpatialLayer : uint64_t {
    Default = 0,
    Units = 1,
    Buildings = 2,
    Projectiles = 3,
    Terrain = 4,
    Triggers = 5,
    Particles = 6,
    Decorations = 7,
    Navigation = 8,
    Physics = 9,
    Custom0 = 10,
    Custom1 = 11,
    Custom2 = 12,
    Custom3 = 13,
    Custom4 = 14,
    Custom5 = 15,
    All = ~0ULL
};

/**
 * @brief Convert layer to bitmask
 */
[[nodiscard]] inline constexpr uint64_t LayerMask(SpatialLayer layer) noexcept {
    return 1ULL << static_cast<uint64_t>(layer);
}

/**
 * @brief Combine multiple layer masks
 */
template<typename... Layers>
[[nodiscard]] constexpr uint64_t LayerMask(SpatialLayer first, Layers... rest) noexcept {
    return LayerMask(first) | LayerMask(rest...);
}

/**
 * @brief Cached query result with invalidation
 */
template<typename T>
struct CachedQuery {
    std::vector<T> results;
    uint64_t frameNumber = 0;
    size_t queryHash = 0;

    [[nodiscard]] bool IsValid(uint64_t currentFrame, size_t hash) const noexcept {
        return frameNumber == currentFrame && queryHash == hash;
    }

    void Update(std::vector<T> newResults, uint64_t frame, size_t hash) {
        results = std::move(newResults);
        frameNumber = frame;
        queryHash = hash;
    }
};

/**
 * @brief Spatial query profiling data
 */
struct SpatialProfileData {
    std::string name;
    size_t queryCount = 0;
    float totalTimeMs = 0.0f;
    size_t totalNodesVisited = 0;
    size_t totalObjectsTested = 0;
    size_t totalObjectsReturned = 0;

    void Reset() noexcept {
        queryCount = 0;
        totalTimeMs = 0.0f;
        totalNodesVisited = 0;
        totalObjectsTested = 0;
        totalObjectsReturned = 0;
    }

    [[nodiscard]] float GetAverageTimeMs() const noexcept {
        return queryCount > 0 ? totalTimeMs / queryCount : 0.0f;
    }
};

/**
 * @brief High-level spatial manager for game engines
 *
 * Features:
 * - Automatic index selection based on object distribution
 * - Layer-based queries (units, buildings, projectiles, terrain)
 * - Cached query results with invalidation
 * - Thread-safe query interface
 * - Statistics and profiling
 * - Debug visualization
 */
class SpatialManager {
public:
    /**
     * @brief Configuration for spatial manager
     */
    struct Config {
        AABB worldBounds = AABB(glm::vec3(-10000), glm::vec3(10000));
        SpatialIndexType defaultIndexType = SpatialIndexType::BVH;
        float spatialHashCellSize = 50.0f;
        bool enableQueryCaching = true;
        bool enableProfiling = false;
        bool threadSafe = true;
        size_t maxCachedQueries = 100;
    };

    explicit SpatialManager(const Config& config = {});
    ~SpatialManager() = default;

    // Non-copyable, movable
    SpatialManager(const SpatialManager&) = delete;
    SpatialManager& operator=(const SpatialManager&) = delete;
    SpatialManager(SpatialManager&&) noexcept = default;
    SpatialManager& operator=(SpatialManager&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the spatial manager
     */
    void Initialize();

    /**
     * @brief Update per-frame (invalidates caches, rebuilds if needed)
     */
    void Update(float deltaTime);

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    // =========================================================================
    // Object Registration
    // =========================================================================

    /**
     * @brief Register an object in the spatial index
     */
    void RegisterObject(uint64_t id, const AABB& bounds, SpatialLayer layer = SpatialLayer::Default);

    /**
     * @brief Unregister an object
     */
    void UnregisterObject(uint64_t id);

    /**
     * @brief Update object bounds
     */
    void UpdateObject(uint64_t id, const AABB& newBounds);

    /**
     * @brief Batch register multiple objects
     */
    void RegisterObjects(std::span<const std::pair<uint64_t, AABB>> objects,
                        SpatialLayer layer = SpatialLayer::Default);

    /**
     * @brief Check if object is registered
     */
    [[nodiscard]] bool IsRegistered(uint64_t id) const;

    /**
     * @brief Get object bounds
     */
    [[nodiscard]] AABB GetObjectBounds(uint64_t id) const;

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Query objects in AABB
     */
    [[nodiscard]] std::vector<uint64_t> QueryAABB(
        const AABB& query,
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    /**
     * @brief Query objects in sphere
     */
    [[nodiscard]] std::vector<uint64_t> QuerySphere(
        const glm::vec3& center,
        float radius,
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    /**
     * @brief Query objects in frustum
     */
    [[nodiscard]] std::vector<uint64_t> QueryFrustum(
        const Frustum& frustum,
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    /**
     * @brief Cast ray and get hits
     */
    [[nodiscard]] std::vector<RayHit> QueryRay(
        const Ray& ray,
        float maxDist = std::numeric_limits<float>::max(),
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    /**
     * @brief Find nearest object
     */
    [[nodiscard]] uint64_t QueryNearest(
        const glm::vec3& point,
        float maxDist = std::numeric_limits<float>::max(),
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    /**
     * @brief Find K nearest objects
     */
    [[nodiscard]] std::vector<uint64_t> QueryKNearest(
        const glm::vec3& point,
        size_t k,
        float maxDist = std::numeric_limits<float>::max(),
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    // =========================================================================
    // Callback-based Queries
    // =========================================================================

    /**
     * @brief Query with callback
     */
    void QueryAABB(
        const AABB& query,
        const ISpatialIndex::VisitorCallback& callback,
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    void QuerySphere(
        const glm::vec3& center,
        float radius,
        const ISpatialIndex::VisitorCallback& callback,
        uint64_t layerMask = static_cast<uint64_t>(SpatialLayer::All));

    // =========================================================================
    // Layer Management
    // =========================================================================

    /**
     * @brief Get the index for a specific layer
     */
    [[nodiscard]] ISpatialIndex* GetLayerIndex(SpatialLayer layer);

    /**
     * @brief Set custom index for a layer
     */
    void SetLayerIndex(SpatialLayer layer, std::unique_ptr<ISpatialIndex> index);

    /**
     * @brief Get all objects in a layer
     */
    [[nodiscard]] std::vector<uint64_t> GetLayerObjects(SpatialLayer layer) const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Invalidate all cached queries
     */
    void InvalidateCache();

    /**
     * @brief Get cache hit ratio
     */
    [[nodiscard]] float GetCacheHitRatio() const noexcept;

    // =========================================================================
    // Statistics and Profiling
    // =========================================================================

    /**
     * @brief Get total object count
     */
    [[nodiscard]] size_t GetObjectCount() const;

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

    /**
     * @brief Get profiling data
     */
    [[nodiscard]] const std::unordered_map<std::string, SpatialProfileData>& GetProfilingData() const {
        return m_profilingData;
    }

    /**
     * @brief Reset profiling data
     */
    void ResetProfilingData();

    /**
     * @brief Get frame statistics
     */
    struct FrameStats {
        size_t queriesThisFrame = 0;
        size_t objectsUpdated = 0;
        size_t objectsInserted = 0;
        size_t objectsRemoved = 0;
        float totalQueryTimeMs = 0.0f;
    };

    [[nodiscard]] const FrameStats& GetFrameStats() const noexcept { return m_frameStats; }

    // =========================================================================
    // Debug Visualization
    // =========================================================================

    /**
     * @brief Enable/disable debug visualization
     */
    void SetDebugVisualization(bool enabled) noexcept { m_debugVisualization = enabled; }
    [[nodiscard]] bool IsDebugVisualizationEnabled() const noexcept { return m_debugVisualization; }

    /**
     * @brief Draw debug visualization
     */
    void DrawDebug();

    /**
     * @brief Draw specific layer
     */
    void DrawDebugLayer(SpatialLayer layer, const glm::vec4& color);

    /**
     * @brief Draw query result
     */
    void DrawDebugQuery(const AABB& query, const std::vector<uint64_t>& results);

    // =========================================================================
    // Index Management
    // =========================================================================

    /**
     * @brief Force rebuild all indices
     */
    void RebuildAllIndices();

    /**
     * @brief Optimize indices based on current object distribution
     */
    void OptimizeIndices();

    /**
     * @brief Get optimal index type for current distribution
     */
    [[nodiscard]] SpatialIndexType GetOptimalIndexType() const;

private:
    // Thread safety
    mutable std::shared_mutex m_mutex;

    // Configuration
    Config m_config;

    // Primary spatial index
    std::unique_ptr<ISpatialIndex> m_primaryIndex;

    // Per-layer indices (optional, for layer-specific optimizations)
    std::array<std::unique_ptr<ISpatialIndex>, 16> m_layerIndices;

    // Object layer tracking
    std::unordered_map<uint64_t, SpatialLayer> m_objectLayers;

    // Query caching
    uint64_t m_currentFrame = 0;
    mutable std::unordered_map<size_t, CachedQuery<uint64_t>> m_queryCache;
    mutable size_t m_cacheHits = 0;
    mutable size_t m_cacheMisses = 0;

    // Profiling
    std::unordered_map<std::string, SpatialProfileData> m_profilingData;
    FrameStats m_frameStats;

    // Debug
    bool m_debugVisualization = false;

    // Helper methods
    SpatialQueryFilter CreateFilter(uint64_t layerMask, uint64_t excludeId = 0) const;
    size_t ComputeQueryHash(const AABB& query, uint64_t layerMask) const;
    size_t ComputeQueryHash(const glm::vec3& center, float radius, uint64_t layerMask) const;

    void RecordQueryStats(const std::string& queryType, const SpatialQueryStats& stats,
                         float timeMs);

    template<typename QueryFunc>
    auto ProfiledQuery(const std::string& name, QueryFunc&& func);
};

/**
 * @brief Global spatial manager singleton
 */
class SpatialManagerSingleton {
public:
    [[nodiscard]] static SpatialManager& Instance() {
        static SpatialManager instance;
        return instance;
    }

    SpatialManagerSingleton(const SpatialManagerSingleton&) = delete;
    SpatialManagerSingleton& operator=(const SpatialManagerSingleton&) = delete;

private:
    SpatialManagerSingleton() = default;
};

// Convenience macro
#define g_SpatialManager SpatialManagerSingleton::Instance()

} // namespace Nova
