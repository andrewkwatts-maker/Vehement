#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <future>
#include <cstdint>

namespace Vehement {
namespace Systems {

using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY_ID = 0;

/**
 * @brief Path request status
 */
enum class PathRequestStatus : uint8_t {
    Pending,        // Request queued
    InProgress,     // Currently computing
    Complete,       // Path found
    Failed,         // No path exists
    Cancelled       // Request cancelled
};

/**
 * @brief Single waypoint in a path
 */
struct PathWaypoint {
    glm::vec3 position{0.0f};
    float cost = 0.0f;          // Cost to reach this waypoint
    bool isCorner = false;      // Is this a corner waypoint?
};

/**
 * @brief Complete path result
 */
struct PathResult {
    std::vector<PathWaypoint> waypoints;
    float totalCost = 0.0f;
    bool valid = false;
    uint64_t cacheKey = 0;      // For cache lookup

    [[nodiscard]] bool IsValid() const { return valid && !waypoints.empty(); }
    [[nodiscard]] size_t Size() const { return waypoints.size(); }

    [[nodiscard]] const PathWaypoint& operator[](size_t index) const {
        return waypoints[index];
    }
};

/**
 * @brief Async path request handle
 */
struct PathRequestHandle {
    uint64_t requestId = 0;
    PathRequestStatus status = PathRequestStatus::Pending;
    EntityId requestingEntity = INVALID_ENTITY_ID;
    glm::vec3 start{0.0f};
    glm::vec3 goal{0.0f};
    float timestamp = 0.0f;

    [[nodiscard]] bool IsComplete() const {
        return status == PathRequestStatus::Complete ||
               status == PathRequestStatus::Failed ||
               status == PathRequestStatus::Cancelled;
    }
};

/**
 * @brief Callback for async path completion
 */
using PathCompleteCallback = std::function<void(EntityId, const PathResult&)>;

/**
 * @brief Path computation function (injected dependency)
 */
using PathComputeFunction = std::function<PathResult(const glm::vec3&, const glm::vec3&)>;

// ============================================================================
// Hierarchical Path Cache
// ============================================================================

/**
 * @brief Hierarchical region for high-level pathfinding
 */
struct PathRegion {
    uint32_t regionId = 0;
    glm::vec3 center{0.0f};
    float radius = 0.0f;
    std::vector<uint32_t> neighbors;    // Connected region IDs
    std::vector<glm::vec3> entryPoints; // Points to enter this region
};

/**
 * @brief High-level path through regions
 */
struct RegionPath {
    std::vector<uint32_t> regionIds;
    float totalCost = 0.0f;
    bool valid = false;
};

// ============================================================================
// Path Cache - Caches and Shares Computed Paths
// ============================================================================

/**
 * @brief High-performance path caching system
 *
 * Features:
 * - Caches computed paths for reuse
 * - Shares paths between nearby entities with same goals
 * - Hierarchical pathfinding for distant goals
 * - Async path requests with callback
 * - LRU cache eviction
 */
class PathCache {
public:
    struct Config {
        // Cache settings
        size_t maxCachedPaths = 1000;
        float cacheExpirationTime = 30.0f;  // Seconds before cache entry expires

        // Path sharing settings
        float pathShareRadius = 3.0f;       // Entities within this can share paths
        float goalTolerance = 1.0f;         // Goals within this are considered same

        // Async settings
        size_t asyncThreadCount = 2;
        size_t maxQueuedRequests = 100;
        float requestTimeout = 5.0f;        // Seconds before request times out

        // Hierarchical settings
        bool enableHierarchical = true;
        float regionSize = 50.0f;
        float hierarchicalThreshold = 100.0f; // Use hierarchical for distances > this
    };

    explicit PathCache(const Config& config = {});
    ~PathCache();

    // Non-copyable
    PathCache(const PathCache&) = delete;
    PathCache& operator=(const PathCache&) = delete;

    // =========================================================================
    // Synchronous Path Lookup
    // =========================================================================

    /**
     * @brief Get a cached path or compute new one synchronously
     * @param start Start position
     * @param goal Goal position
     * @param computeFunc Function to compute path if not cached
     * @return Path result
     */
    PathResult GetPath(const glm::vec3& start, const glm::vec3& goal,
                       PathComputeFunction computeFunc);

    /**
     * @brief Check if a path is cached
     */
    [[nodiscard]] bool HasCachedPath(const glm::vec3& start, const glm::vec3& goal) const;

    /**
     * @brief Get a cached path without computing
     * @return Valid path if cached, invalid path otherwise
     */
    [[nodiscard]] PathResult GetCachedPath(const glm::vec3& start, const glm::vec3& goal) const;

    // =========================================================================
    // Asynchronous Path Requests
    // =========================================================================

    /**
     * @brief Request a path asynchronously
     * @param entityId Requesting entity
     * @param start Start position
     * @param goal Goal position
     * @param callback Function to call when complete
     * @return Request handle
     */
    PathRequestHandle RequestPathAsync(EntityId entityId,
                                        const glm::vec3& start,
                                        const glm::vec3& goal,
                                        PathCompleteCallback callback);

    /**
     * @brief Cancel a pending path request
     * @param requestId Request ID from handle
     * @return true if request was cancelled
     */
    bool CancelRequest(uint64_t requestId);

    /**
     * @brief Cancel all requests for an entity
     */
    void CancelEntityRequests(EntityId entityId);

    /**
     * @brief Get status of a path request
     */
    [[nodiscard]] PathRequestStatus GetRequestStatus(uint64_t requestId) const;

    /**
     * @brief Get result of completed request
     */
    [[nodiscard]] PathResult GetRequestResult(uint64_t requestId) const;

    /**
     * @brief Process completed async requests (call from main thread)
     * Invokes callbacks for completed requests
     */
    void ProcessCompletedRequests();

    /**
     * @brief Set the pathfinding function for async requests
     */
    void SetPathComputeFunction(PathComputeFunction func);

    // =========================================================================
    // Path Sharing
    // =========================================================================

    /**
     * @brief Register an entity's current path for sharing
     * @param entityId Entity with the path
     * @param position Entity's current position
     * @param goal Path goal
     * @param path The path to share
     */
    void SharePath(EntityId entityId, const glm::vec3& position,
                   const glm::vec3& goal, const PathResult& path);

    /**
     * @brief Find a shared path that can be used by an entity
     * @param position Entity's current position
     * @param goal Desired goal
     * @return Shared path or invalid path if none found
     */
    [[nodiscard]] PathResult FindSharedPath(const glm::vec3& position,
                                            const glm::vec3& goal) const;

    /**
     * @brief Unregister an entity's shared path
     */
    void UnshareEntityPath(EntityId entityId);

    // =========================================================================
    // Hierarchical Pathfinding
    // =========================================================================

    /**
     * @brief Initialize hierarchical regions
     * @param worldMin World minimum bounds
     * @param worldMax World maximum bounds
     */
    void InitializeRegions(const glm::vec3& worldMin, const glm::vec3& worldMax);

    /**
     * @brief Find region containing a point
     */
    [[nodiscard]] uint32_t FindRegion(const glm::vec3& position) const;

    /**
     * @brief Get high-level path through regions
     */
    [[nodiscard]] RegionPath FindRegionPath(const glm::vec3& start,
                                            const glm::vec3& goal) const;

    /**
     * @brief Mark a region as blocked (e.g., door closed)
     */
    void SetRegionBlocked(uint32_t regionId, bool blocked);

    /**
     * @brief Invalidate cached paths through a region
     */
    void InvalidateRegion(uint32_t regionId);

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Add a path to the cache
     */
    void CachePath(const glm::vec3& start, const glm::vec3& goal,
                   const PathResult& path);

    /**
     * @brief Invalidate all cached paths (e.g., after world change)
     */
    void InvalidateAll();

    /**
     * @brief Invalidate paths passing through an area
     */
    void InvalidateArea(const glm::vec3& center, float radius);

    /**
     * @brief Prune expired cache entries
     */
    void PruneExpired(float currentTime);

    /**
     * @brief Update cache (call periodically)
     */
    void Update(float currentTime);

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        uint64_t sharedPaths = 0;
        uint64_t asyncRequestsCompleted = 0;
        uint64_t asyncRequestsFailed = 0;
        size_t currentCacheSize = 0;
        size_t pendingRequests = 0;

        [[nodiscard]] float GetHitRate() const {
            uint64_t total = cacheHits + cacheMisses;
            return total > 0 ? static_cast<float>(cacheHits) / total : 0.0f;
        }
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }
    void ResetStats();

private:
    // Cache entry
    struct CacheEntry {
        PathResult path;
        float timestamp = 0.0f;
        float lastAccessTime = 0.0f;
        uint64_t accessCount = 0;
        bool valid = true;
    };

    // Async request
    struct AsyncRequest {
        uint64_t requestId = 0;
        EntityId entityId = INVALID_ENTITY_ID;
        glm::vec3 start{0.0f};
        glm::vec3 goal{0.0f};
        PathCompleteCallback callback;
        PathRequestStatus status = PathRequestStatus::Pending;
        PathResult result;
        float submitTime = 0.0f;
    };

    // Shared path entry
    struct SharedPathEntry {
        EntityId entityId = INVALID_ENTITY_ID;
        glm::vec3 position{0.0f};
        glm::vec3 goal{0.0f};
        PathResult path;
        float timestamp = 0.0f;
    };

    // Cache key generation
    [[nodiscard]] uint64_t MakeCacheKey(const glm::vec3& start, const glm::vec3& goal) const;
    [[nodiscard]] glm::ivec3 QuantizePosition(const glm::vec3& pos) const;

    // LRU eviction
    void EvictLRU();

    // Worker thread function
    void WorkerThread();

    Config m_config;
    Stats m_stats;
    float m_currentTime = 0.0f;

    // Path cache (LRU map)
    std::unordered_map<uint64_t, CacheEntry> m_cache;
    mutable std::mutex m_cacheMutex;

    // Async request handling
    std::queue<AsyncRequest> m_pendingRequests;
    std::unordered_map<uint64_t, AsyncRequest> m_completedRequests;
    std::vector<std::thread> m_workers;
    std::mutex m_requestMutex;
    std::condition_variable m_requestCondition;
    std::atomic<bool> m_running{true};
    std::atomic<uint64_t> m_nextRequestId{1};
    PathComputeFunction m_pathComputeFunc;

    // Path sharing
    std::unordered_map<EntityId, SharedPathEntry> m_sharedPaths;
    mutable std::mutex m_shareMutex;

    // Hierarchical regions
    std::vector<PathRegion> m_regions;
    std::unordered_set<uint32_t> m_blockedRegions;
    glm::vec3 m_worldMin{0.0f};
    glm::vec3 m_worldMax{0.0f};

    // Position quantization for cache keys
    float m_positionQuantization = 0.5f;
};

// ============================================================================
// Path Smoothing Utilities
// ============================================================================

/**
 * @brief Utilities for path post-processing
 */
class PathUtils {
public:
    /**
     * @brief Smooth a path using Catmull-Rom splines
     */
    static PathResult SmoothPath(const PathResult& path, float segmentLength = 1.0f);

    /**
     * @brief Reduce path waypoints using line-of-sight
     * @param path Original path
     * @param losCheck Function to check line of sight
     */
    static PathResult SimplifyPath(const PathResult& path,
                                   std::function<bool(const glm::vec3&, const glm::vec3&)> losCheck);

    /**
     * @brief Get the closest point on a path to a position
     * @return Index of closest waypoint
     */
    static size_t FindClosestWaypoint(const PathResult& path, const glm::vec3& position);

    /**
     * @brief Get remaining path from current position
     */
    static PathResult GetRemainingPath(const PathResult& path, size_t startIndex);

    /**
     * @brief Check if position is on the path within tolerance
     */
    static bool IsOnPath(const PathResult& path, const glm::vec3& position,
                         float tolerance = 1.0f);

    /**
     * @brief Merge two paths at their closest points
     */
    static PathResult MergePaths(const PathResult& pathA, const PathResult& pathB);

    /**
     * @brief Calculate total path length
     */
    static float CalculatePathLength(const PathResult& path);

    /**
     * @brief Offset path perpendicular to direction (for formation movement)
     */
    static PathResult OffsetPath(const PathResult& path, float offset);
};

// ============================================================================
// Path Following Helper
// ============================================================================

/**
 * @brief Helper class for following a path
 */
class PathFollower {
public:
    struct Config {
        float waypointRadius = 0.5f;    // Distance to consider waypoint reached
        float lookAheadDistance = 2.0f; // How far ahead to look for steering
        float slowdownDistance = 3.0f;  // Start slowing down at this distance
        float arrivalDistance = 0.5f;   // Consider arrived at this distance
    };

    explicit PathFollower(const Config& config = {});

    /**
     * @brief Set the path to follow
     */
    void SetPath(const PathResult& path);

    /**
     * @brief Clear the current path
     */
    void ClearPath();

    /**
     * @brief Check if following a path
     */
    [[nodiscard]] bool HasPath() const { return m_path.IsValid(); }

    /**
     * @brief Check if path is complete
     */
    [[nodiscard]] bool IsComplete() const;

    /**
     * @brief Update path following
     * @param currentPosition Current entity position
     * @param deltaTime Time since last update
     * @return Desired velocity vector
     */
    glm::vec3 Update(const glm::vec3& currentPosition, float deltaTime);

    /**
     * @brief Get the current target waypoint
     */
    [[nodiscard]] const PathWaypoint* GetCurrentWaypoint() const;

    /**
     * @brief Get current waypoint index
     */
    [[nodiscard]] size_t GetCurrentIndex() const { return m_currentIndex; }

    /**
     * @brief Get remaining distance to goal
     */
    [[nodiscard]] float GetRemainingDistance() const;

    /**
     * @brief Get progress along path (0.0 to 1.0)
     */
    [[nodiscard]] float GetProgress() const;

private:
    Config m_config;
    PathResult m_path;
    size_t m_currentIndex = 0;
    float m_totalLength = 0.0f;
    float m_traveledLength = 0.0f;
};

} // namespace Systems
} // namespace Vehement
