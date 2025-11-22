#include "PathCache.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Systems {

// ============================================================================
// PathCache Implementation
// ============================================================================

PathCache::PathCache(const Config& config)
    : m_config(config)
{
    // Start worker threads for async requests
    if (config.asyncThreadCount > 0) {
        for (size_t i = 0; i < config.asyncThreadCount; ++i) {
            m_workers.emplace_back(&PathCache::WorkerThread, this);
        }
    }
}

PathCache::~PathCache() {
    // Signal workers to stop
    m_running = false;
    m_requestCondition.notify_all();

    // Wait for workers to finish
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

PathResult PathCache::GetPath(const glm::vec3& start, const glm::vec3& goal,
                               PathComputeFunction computeFunc) {
    uint64_t key = MakeCacheKey(start, goal);

    // Check cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(key);
        if (it != m_cache.end() && it->second.valid) {
            it->second.lastAccessTime = m_currentTime;
            ++it->second.accessCount;
            ++m_stats.cacheHits;
            return it->second.path;
        }
    }

    // Check shared paths
    PathResult shared = FindSharedPath(start, goal);
    if (shared.IsValid()) {
        ++m_stats.sharedPaths;
        return shared;
    }

    // Compute new path
    ++m_stats.cacheMisses;
    PathResult result = computeFunc(start, goal);

    // Cache the result
    if (result.IsValid()) {
        CachePath(start, goal, result);
    }

    return result;
}

bool PathCache::HasCachedPath(const glm::vec3& start, const glm::vec3& goal) const {
    uint64_t key = MakeCacheKey(start, goal);

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cache.find(key);
    return it != m_cache.end() && it->second.valid;
}

PathResult PathCache::GetCachedPath(const glm::vec3& start, const glm::vec3& goal) const {
    uint64_t key = MakeCacheKey(start, goal);

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cache.find(key);
    if (it != m_cache.end() && it->second.valid) {
        return it->second.path;
    }

    return PathResult{};  // Invalid path
}

PathRequestHandle PathCache::RequestPathAsync(EntityId entityId,
                                               const glm::vec3& start,
                                               const glm::vec3& goal,
                                               PathCompleteCallback callback) {
    PathRequestHandle handle;
    handle.requestId = m_nextRequestId++;
    handle.status = PathRequestStatus::Pending;
    handle.requestingEntity = entityId;
    handle.start = start;
    handle.goal = goal;
    handle.timestamp = m_currentTime;

    // Check if path is already cached
    PathResult cached = GetCachedPath(start, goal);
    if (cached.IsValid()) {
        handle.status = PathRequestStatus::Complete;
        if (callback) {
            callback(entityId, cached);
        }
        return handle;
    }

    // Queue async request
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);

        if (m_pendingRequests.size() >= m_config.maxQueuedRequests) {
            handle.status = PathRequestStatus::Failed;
            return handle;
        }

        AsyncRequest request;
        request.requestId = handle.requestId;
        request.entityId = entityId;
        request.start = start;
        request.goal = goal;
        request.callback = callback;
        request.status = PathRequestStatus::Pending;
        request.submitTime = m_currentTime;

        m_pendingRequests.push(request);
    }

    m_requestCondition.notify_one();
    ++m_stats.pendingRequests;

    return handle;
}

bool PathCache::CancelRequest(uint64_t requestId) {
    std::lock_guard<std::mutex> lock(m_requestMutex);

    // Can't cancel in-progress requests, but mark completed for removal
    auto it = m_completedRequests.find(requestId);
    if (it != m_completedRequests.end()) {
        it->second.status = PathRequestStatus::Cancelled;
        return true;
    }

    return false;
}

void PathCache::CancelEntityRequests(EntityId entityId) {
    std::lock_guard<std::mutex> lock(m_requestMutex);

    for (auto& [id, request] : m_completedRequests) {
        if (request.entityId == entityId) {
            request.status = PathRequestStatus::Cancelled;
        }
    }
}

PathRequestStatus PathCache::GetRequestStatus(uint64_t requestId) const {
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_completedRequests.find(requestId);
    if (it != m_completedRequests.end()) {
        return it->second.status;
    }

    return PathRequestStatus::Pending;
}

PathResult PathCache::GetRequestResult(uint64_t requestId) const {
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_completedRequests.find(requestId);
    if (it != m_completedRequests.end() &&
        it->second.status == PathRequestStatus::Complete) {
        return it->second.result;
    }

    return PathResult{};
}

void PathCache::ProcessCompletedRequests() {
    std::vector<AsyncRequest> toProcess;

    {
        std::lock_guard<std::mutex> lock(m_requestMutex);

        for (auto it = m_completedRequests.begin(); it != m_completedRequests.end();) {
            if (it->second.status == PathRequestStatus::Complete ||
                it->second.status == PathRequestStatus::Failed) {
                toProcess.push_back(it->second);
                it = m_completedRequests.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Invoke callbacks outside the lock
    for (const auto& request : toProcess) {
        if (request.callback && request.status != PathRequestStatus::Cancelled) {
            request.callback(request.entityId, request.result);
        }

        if (request.status == PathRequestStatus::Complete) {
            ++m_stats.asyncRequestsCompleted;
        } else if (request.status == PathRequestStatus::Failed) {
            ++m_stats.asyncRequestsFailed;
        }
    }
}

void PathCache::SetPathComputeFunction(PathComputeFunction func) {
    std::lock_guard<std::mutex> lock(m_requestMutex);
    m_pathComputeFunc = func;
}

void PathCache::SharePath(EntityId entityId, const glm::vec3& position,
                           const glm::vec3& goal, const PathResult& path) {
    if (!path.IsValid()) return;

    std::lock_guard<std::mutex> lock(m_shareMutex);

    SharedPathEntry entry;
    entry.entityId = entityId;
    entry.position = position;
    entry.goal = goal;
    entry.path = path;
    entry.timestamp = m_currentTime;

    m_sharedPaths[entityId] = entry;
}

PathResult PathCache::FindSharedPath(const glm::vec3& position,
                                      const glm::vec3& goal) const {
    std::lock_guard<std::mutex> lock(m_shareMutex);

    float shareRadiusSq = m_config.pathShareRadius * m_config.pathShareRadius;
    float goalToleranceSq = m_config.goalTolerance * m_config.goalTolerance;

    for (const auto& [entityId, entry] : m_sharedPaths) {
        // Check if goals match
        glm::vec3 goalDiff = entry.goal - goal;
        if (glm::dot(goalDiff, goalDiff) > goalToleranceSq) {
            continue;
        }

        // Check if position is close enough
        glm::vec3 posDiff = entry.position - position;
        if (glm::dot(posDiff, posDiff) > shareRadiusSq) {
            continue;
        }

        // Found a shareable path
        return entry.path;
    }

    return PathResult{};
}

void PathCache::UnshareEntityPath(EntityId entityId) {
    std::lock_guard<std::mutex> lock(m_shareMutex);
    m_sharedPaths.erase(entityId);
}

void PathCache::InitializeRegions(const glm::vec3& worldMin, const glm::vec3& worldMax) {
    m_worldMin = worldMin;
    m_worldMax = worldMax;
    m_regions.clear();

    if (!m_config.enableHierarchical) return;

    // Create grid of regions
    glm::vec3 worldSize = worldMax - worldMin;
    int regionsX = static_cast<int>(std::ceil(worldSize.x / m_config.regionSize));
    int regionsZ = static_cast<int>(std::ceil(worldSize.z / m_config.regionSize));

    for (int x = 0; x < regionsX; ++x) {
        for (int z = 0; z < regionsZ; ++z) {
            PathRegion region;
            region.regionId = static_cast<uint32_t>(m_regions.size());

            region.center = worldMin + glm::vec3(
                (x + 0.5f) * m_config.regionSize,
                0.0f,
                (z + 0.5f) * m_config.regionSize
            );
            region.radius = m_config.regionSize * 0.5f;

            // Connect to neighbors
            if (x > 0) {
                uint32_t neighborId = static_cast<uint32_t>((x - 1) * regionsZ + z);
                region.neighbors.push_back(neighborId);
                m_regions[neighborId].neighbors.push_back(region.regionId);
            }
            if (z > 0) {
                uint32_t neighborId = static_cast<uint32_t>(x * regionsZ + (z - 1));
                region.neighbors.push_back(neighborId);
                m_regions[neighborId].neighbors.push_back(region.regionId);
            }

            // Entry points at region edges
            region.entryPoints.push_back(region.center + glm::vec3(region.radius, 0, 0));
            region.entryPoints.push_back(region.center - glm::vec3(region.radius, 0, 0));
            region.entryPoints.push_back(region.center + glm::vec3(0, 0, region.radius));
            region.entryPoints.push_back(region.center - glm::vec3(0, 0, region.radius));

            m_regions.push_back(region);
        }
    }
}

uint32_t PathCache::FindRegion(const glm::vec3& position) const {
    if (m_regions.empty()) return 0;

    // Find closest region
    float minDistSq = std::numeric_limits<float>::max();
    uint32_t closest = 0;

    for (const auto& region : m_regions) {
        glm::vec3 diff = position - region.center;
        float distSq = glm::dot(diff, diff);
        if (distSq < minDistSq) {
            minDistSq = distSq;
            closest = region.regionId;
        }
    }

    return closest;
}

RegionPath PathCache::FindRegionPath(const glm::vec3& start,
                                      const glm::vec3& goal) const {
    RegionPath result;
    result.valid = false;

    if (m_regions.empty()) return result;

    uint32_t startRegion = FindRegion(start);
    uint32_t goalRegion = FindRegion(goal);

    if (startRegion == goalRegion) {
        result.regionIds.push_back(startRegion);
        result.valid = true;
        return result;
    }

    // Simple BFS through regions
    std::queue<uint32_t> queue;
    std::unordered_map<uint32_t, uint32_t> cameFrom;
    std::unordered_set<uint32_t> visited;

    queue.push(startRegion);
    visited.insert(startRegion);
    cameFrom[startRegion] = startRegion;

    while (!queue.empty()) {
        uint32_t current = queue.front();
        queue.pop();

        if (current == goalRegion) {
            // Reconstruct path
            uint32_t node = goalRegion;
            while (node != startRegion) {
                result.regionIds.push_back(node);
                node = cameFrom[node];
            }
            result.regionIds.push_back(startRegion);
            std::reverse(result.regionIds.begin(), result.regionIds.end());
            result.valid = true;
            return result;
        }

        for (uint32_t neighbor : m_regions[current].neighbors) {
            if (visited.count(neighbor) > 0) continue;
            if (m_blockedRegions.count(neighbor) > 0) continue;

            visited.insert(neighbor);
            cameFrom[neighbor] = current;
            queue.push(neighbor);
        }
    }

    return result;  // No path found
}

void PathCache::SetRegionBlocked(uint32_t regionId, bool blocked) {
    if (blocked) {
        m_blockedRegions.insert(regionId);
    } else {
        m_blockedRegions.erase(regionId);
    }
}

void PathCache::InvalidateRegion(uint32_t regionId) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Would need to track which paths pass through which regions
    // For now, invalidate all
    for (auto& [key, entry] : m_cache) {
        entry.valid = false;
    }
}

void PathCache::CachePath(const glm::vec3& start, const glm::vec3& goal,
                           const PathResult& path) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Evict if at capacity
    while (m_cache.size() >= m_config.maxCachedPaths) {
        EvictLRU();
    }

    uint64_t key = MakeCacheKey(start, goal);

    CacheEntry entry;
    entry.path = path;
    entry.timestamp = m_currentTime;
    entry.lastAccessTime = m_currentTime;
    entry.accessCount = 1;
    entry.valid = true;

    m_cache[key] = entry;
    m_stats.currentCacheSize = m_cache.size();
}

void PathCache::InvalidateAll() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    m_stats.currentCacheSize = 0;
}

void PathCache::InvalidateArea(const glm::vec3& center, float radius) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Mark entries as invalid if any waypoint is in the area
    float radiusSq = radius * radius;

    for (auto& [key, entry] : m_cache) {
        for (const auto& waypoint : entry.path.waypoints) {
            glm::vec3 diff = waypoint.position - center;
            if (glm::dot(diff, diff) <= radiusSq) {
                entry.valid = false;
                break;
            }
        }
    }
}

void PathCache::PruneExpired(float currentTime) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (!it->second.valid ||
            currentTime - it->second.timestamp > m_config.cacheExpirationTime) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }

    m_stats.currentCacheSize = m_cache.size();
}

void PathCache::Update(float currentTime) {
    m_currentTime = currentTime;

    // Periodically prune
    static float lastPrune = 0.0f;
    if (currentTime - lastPrune > 5.0f) {
        PruneExpired(currentTime);
        lastPrune = currentTime;
    }

    // Update pending request count
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_stats.pendingRequests = m_pendingRequests.size();
    }

    // Prune old shared paths
    {
        std::lock_guard<std::mutex> lock(m_shareMutex);
        for (auto it = m_sharedPaths.begin(); it != m_sharedPaths.end();) {
            if (currentTime - it->second.timestamp > 5.0f) {
                it = m_sharedPaths.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void PathCache::ResetStats() {
    m_stats = Stats{};
    m_stats.currentCacheSize = m_cache.size();
}

uint64_t PathCache::MakeCacheKey(const glm::vec3& start, const glm::vec3& goal) const {
    glm::ivec3 qStart = QuantizePosition(start);
    glm::ivec3 qGoal = QuantizePosition(goal);

    // Simple hash combining quantized positions
    uint64_t h1 = (qStart.x * 73856093) ^ (qStart.y * 19349663) ^ (qStart.z * 83492791);
    uint64_t h2 = (qGoal.x * 73856093) ^ (qGoal.y * 19349663) ^ (qGoal.z * 83492791);

    return (h1 << 32) | (h2 & 0xFFFFFFFF);
}

glm::ivec3 PathCache::QuantizePosition(const glm::vec3& pos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(pos.x / m_positionQuantization)),
        static_cast<int>(std::floor(pos.y / m_positionQuantization)),
        static_cast<int>(std::floor(pos.z / m_positionQuantization))
    );
}

void PathCache::EvictLRU() {
    // Find least recently used entry
    auto lru = m_cache.begin();
    float oldest = std::numeric_limits<float>::max();

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.lastAccessTime < oldest) {
            oldest = it->second.lastAccessTime;
            lru = it;
        }
    }

    if (lru != m_cache.end()) {
        m_cache.erase(lru);
    }
}

void PathCache::WorkerThread() {
    while (m_running) {
        AsyncRequest request;

        {
            std::unique_lock<std::mutex> lock(m_requestMutex);

            m_requestCondition.wait(lock, [this] {
                return !m_running || !m_pendingRequests.empty();
            });

            if (!m_running) break;

            if (m_pendingRequests.empty()) continue;

            request = m_pendingRequests.front();
            m_pendingRequests.pop();
        }

        // Check for timeout
        if (m_currentTime - request.submitTime > m_config.requestTimeout) {
            request.status = PathRequestStatus::Failed;
        } else {
            // Compute path
            request.status = PathRequestStatus::InProgress;

            if (m_pathComputeFunc) {
                request.result = m_pathComputeFunc(request.start, request.goal);
                request.status = request.result.IsValid() ?
                    PathRequestStatus::Complete : PathRequestStatus::Failed;
            } else {
                request.status = PathRequestStatus::Failed;
            }
        }

        // Store completed request
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_completedRequests[request.requestId] = request;
        }
    }
}

// ============================================================================
// PathUtils Implementation
// ============================================================================

PathResult PathUtils::SmoothPath(const PathResult& path, float segmentLength) {
    if (path.waypoints.size() < 3) return path;

    PathResult smoothed;
    smoothed.valid = true;

    // Use Catmull-Rom spline interpolation
    for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
        const glm::vec3& p0 = path.waypoints[i > 0 ? i - 1 : 0].position;
        const glm::vec3& p1 = path.waypoints[i].position;
        const glm::vec3& p2 = path.waypoints[i + 1].position;
        const glm::vec3& p3 = path.waypoints[i + 2 < path.waypoints.size() ? i + 2 : path.waypoints.size() - 1].position;

        float segmentDist = glm::distance(p1, p2);
        int numSegments = std::max(1, static_cast<int>(segmentDist / segmentLength));

        for (int j = 0; j < numSegments; ++j) {
            float t = static_cast<float>(j) / numSegments;

            // Catmull-Rom formula
            glm::vec3 pos = 0.5f * (
                (2.0f * p1) +
                (-p0 + p2) * t +
                (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
                (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t
            );

            PathWaypoint wp;
            wp.position = pos;
            smoothed.waypoints.push_back(wp);
        }
    }

    // Add final point
    PathWaypoint wp;
    wp.position = path.waypoints.back().position;
    smoothed.waypoints.push_back(wp);

    smoothed.totalCost = CalculatePathLength(smoothed);
    return smoothed;
}

PathResult PathUtils::SimplifyPath(const PathResult& path,
                                    std::function<bool(const glm::vec3&, const glm::vec3&)> losCheck) {
    if (path.waypoints.size() < 3) return path;

    PathResult simplified;
    simplified.valid = true;
    simplified.waypoints.push_back(path.waypoints[0]);

    size_t i = 0;
    while (i < path.waypoints.size() - 1) {
        // Find furthest visible point
        size_t furthest = i + 1;
        for (size_t j = i + 2; j < path.waypoints.size(); ++j) {
            if (losCheck(path.waypoints[i].position, path.waypoints[j].position)) {
                furthest = j;
            }
        }

        simplified.waypoints.push_back(path.waypoints[furthest]);
        i = furthest;
    }

    simplified.totalCost = CalculatePathLength(simplified);
    return simplified;
}

size_t PathUtils::FindClosestWaypoint(const PathResult& path, const glm::vec3& position) {
    if (path.waypoints.empty()) return 0;

    size_t closest = 0;
    float minDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < path.waypoints.size(); ++i) {
        glm::vec3 diff = path.waypoints[i].position - position;
        float distSq = glm::dot(diff, diff);
        if (distSq < minDistSq) {
            minDistSq = distSq;
            closest = i;
        }
    }

    return closest;
}

PathResult PathUtils::GetRemainingPath(const PathResult& path, size_t startIndex) {
    PathResult remaining;
    remaining.valid = path.valid;

    if (startIndex >= path.waypoints.size()) {
        return remaining;
    }

    remaining.waypoints.assign(path.waypoints.begin() + startIndex,
                                path.waypoints.end());
    remaining.totalCost = CalculatePathLength(remaining);

    return remaining;
}

bool PathUtils::IsOnPath(const PathResult& path, const glm::vec3& position,
                          float tolerance) {
    float toleranceSq = tolerance * tolerance;

    for (const auto& waypoint : path.waypoints) {
        glm::vec3 diff = waypoint.position - position;
        if (glm::dot(diff, diff) <= toleranceSq) {
            return true;
        }
    }

    return false;
}

PathResult PathUtils::MergePaths(const PathResult& pathA, const PathResult& pathB) {
    PathResult merged;
    merged.valid = pathA.valid && pathB.valid;

    if (!merged.valid) return merged;

    // Find closest points between paths
    float minDistSq = std::numeric_limits<float>::max();
    size_t mergeIndexA = 0;
    size_t mergeIndexB = 0;

    for (size_t i = 0; i < pathA.waypoints.size(); ++i) {
        for (size_t j = 0; j < pathB.waypoints.size(); ++j) {
            glm::vec3 diff = pathA.waypoints[i].position - pathB.waypoints[j].position;
            float distSq = glm::dot(diff, diff);
            if (distSq < minDistSq) {
                minDistSq = distSq;
                mergeIndexA = i;
                mergeIndexB = j;
            }
        }
    }

    // Take first part of path A, then path B from merge point
    merged.waypoints.assign(pathA.waypoints.begin(),
                            pathA.waypoints.begin() + mergeIndexA + 1);
    merged.waypoints.insert(merged.waypoints.end(),
                            pathB.waypoints.begin() + mergeIndexB,
                            pathB.waypoints.end());

    merged.totalCost = CalculatePathLength(merged);
    return merged;
}

float PathUtils::CalculatePathLength(const PathResult& path) {
    float length = 0.0f;

    for (size_t i = 1; i < path.waypoints.size(); ++i) {
        length += glm::distance(path.waypoints[i - 1].position,
                                 path.waypoints[i].position);
    }

    return length;
}

PathResult PathUtils::OffsetPath(const PathResult& path, float offset) {
    if (path.waypoints.size() < 2) return path;

    PathResult offsetPath;
    offsetPath.valid = true;

    for (size_t i = 0; i < path.waypoints.size(); ++i) {
        glm::vec3 direction;

        if (i == 0) {
            direction = path.waypoints[1].position - path.waypoints[0].position;
        } else if (i == path.waypoints.size() - 1) {
            direction = path.waypoints[i].position - path.waypoints[i - 1].position;
        } else {
            direction = path.waypoints[i + 1].position - path.waypoints[i - 1].position;
        }

        direction.y = 0.0f;  // Keep horizontal
        direction = glm::normalize(direction);

        // Perpendicular vector
        glm::vec3 perpendicular(-direction.z, 0.0f, direction.x);

        PathWaypoint wp = path.waypoints[i];
        wp.position += perpendicular * offset;
        offsetPath.waypoints.push_back(wp);
    }

    offsetPath.totalCost = CalculatePathLength(offsetPath);
    return offsetPath;
}

// ============================================================================
// PathFollower Implementation
// ============================================================================

PathFollower::PathFollower(const Config& config)
    : m_config(config)
{
}

void PathFollower::SetPath(const PathResult& path) {
    m_path = path;
    m_currentIndex = 0;
    m_traveledLength = 0.0f;
    m_totalLength = PathUtils::CalculatePathLength(path);
}

void PathFollower::ClearPath() {
    m_path = PathResult{};
    m_currentIndex = 0;
    m_traveledLength = 0.0f;
    m_totalLength = 0.0f;
}

bool PathFollower::IsComplete() const {
    return !m_path.IsValid() ||
           m_currentIndex >= m_path.waypoints.size();
}

glm::vec3 PathFollower::Update(const glm::vec3& currentPosition, float deltaTime) {
    if (IsComplete()) {
        return glm::vec3(0.0f);
    }

    // Check if we've reached current waypoint
    const PathWaypoint& current = m_path.waypoints[m_currentIndex];
    glm::vec3 toWaypoint = current.position - currentPosition;
    float distToWaypoint = glm::length(toWaypoint);

    if (distToWaypoint < m_config.waypointRadius) {
        // Track traveled distance
        if (m_currentIndex > 0) {
            m_traveledLength += glm::distance(
                m_path.waypoints[m_currentIndex - 1].position,
                current.position
            );
        }

        ++m_currentIndex;

        if (IsComplete()) {
            return glm::vec3(0.0f);
        }
    }

    // Calculate steering toward target
    glm::vec3 targetPos;

    // Look ahead for smoother movement
    float lookAhead = m_config.lookAheadDistance;
    size_t lookIndex = m_currentIndex;

    while (lookIndex < m_path.waypoints.size() - 1 && lookAhead > 0.0f) {
        float segmentLen = glm::distance(
            m_path.waypoints[lookIndex].position,
            m_path.waypoints[lookIndex + 1].position
        );

        if (segmentLen < lookAhead) {
            lookAhead -= segmentLen;
            ++lookIndex;
        } else {
            break;
        }
    }

    targetPos = m_path.waypoints[lookIndex].position;

    // Calculate desired velocity
    glm::vec3 desired = glm::normalize(targetPos - currentPosition);

    // Apply arrival slowdown near goal
    if (m_currentIndex == m_path.waypoints.size() - 1) {
        float distToGoal = glm::distance(currentPosition, targetPos);
        if (distToGoal < m_config.slowdownDistance) {
            float slowdown = distToGoal / m_config.slowdownDistance;
            desired *= slowdown;
        }
    }

    return desired;
}

const PathWaypoint* PathFollower::GetCurrentWaypoint() const {
    if (m_currentIndex < m_path.waypoints.size()) {
        return &m_path.waypoints[m_currentIndex];
    }
    return nullptr;
}

float PathFollower::GetRemainingDistance() const {
    if (IsComplete()) return 0.0f;

    return m_totalLength - m_traveledLength;
}

float PathFollower::GetProgress() const {
    if (m_totalLength <= 0.0f) return 1.0f;

    return std::clamp(m_traveledLength / m_totalLength, 0.0f, 1.0f);
}

} // namespace Systems
} // namespace Vehement
