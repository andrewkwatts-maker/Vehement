#include "SpatialManager.hpp"
#include <chrono>
#include <cmath>

namespace Nova {

SpatialManager::SpatialManager(const Config& config)
    : m_config(config) {
    Initialize();
}

void SpatialManager::Initialize() {
    // Create primary index
    m_primaryIndex = CreateSpatialIndex(
        m_config.defaultIndexType,
        m_config.worldBounds,
        m_config.spatialHashCellSize
    );

    // Initialize layer indices (optional - can be configured per-layer)
    // By default, all layers use the primary index
}

void SpatialManager::Update(float deltaTime) {
    // Increment frame counter for cache invalidation
    ++m_currentFrame;

    // Reset frame stats
    m_frameStats = FrameStats{};

    // Clear old cache entries
    if (m_config.enableQueryCaching && m_queryCache.size() > m_config.maxCachedQueries) {
        // Remove oldest entries
        std::vector<size_t> toRemove;
        for (const auto& [hash, cache] : m_queryCache) {
            if (m_currentFrame - cache.frameNumber > 2) {
                toRemove.push_back(hash);
            }
        }
        for (size_t hash : toRemove) {
            m_queryCache.erase(hash);
        }
    }
}

void SpatialManager::Shutdown() {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);
        m_primaryIndex.reset();
        for (auto& index : m_layerIndices) {
            index.reset();
        }
        m_objectLayers.clear();
        m_queryCache.clear();
    } else {
        m_primaryIndex.reset();
        for (auto& index : m_layerIndices) {
            index.reset();
        }
        m_objectLayers.clear();
        m_queryCache.clear();
    }
}

void SpatialManager::RegisterObject(uint64_t id, const AABB& bounds, SpatialLayer layer) {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);
        m_primaryIndex->Insert(id, bounds, static_cast<uint64_t>(layer));
        m_objectLayers[id] = layer;

        // If layer has dedicated index, insert there too
        size_t layerIdx = static_cast<size_t>(layer);
        if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
            m_layerIndices[layerIdx]->Insert(id, bounds);
        }
    } else {
        m_primaryIndex->Insert(id, bounds, static_cast<uint64_t>(layer));
        m_objectLayers[id] = layer;

        size_t layerIdx = static_cast<size_t>(layer);
        if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
            m_layerIndices[layerIdx]->Insert(id, bounds);
        }
    }

    ++m_frameStats.objectsInserted;
    InvalidateCache();
}

void SpatialManager::UnregisterObject(uint64_t id) {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);

        auto it = m_objectLayers.find(id);
        if (it != m_objectLayers.end()) {
            size_t layerIdx = static_cast<size_t>(it->second);
            if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
                m_layerIndices[layerIdx]->Remove(id);
            }
            m_objectLayers.erase(it);
        }

        m_primaryIndex->Remove(id);
    } else {
        auto it = m_objectLayers.find(id);
        if (it != m_objectLayers.end()) {
            size_t layerIdx = static_cast<size_t>(it->second);
            if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
                m_layerIndices[layerIdx]->Remove(id);
            }
            m_objectLayers.erase(it);
        }

        m_primaryIndex->Remove(id);
    }

    ++m_frameStats.objectsRemoved;
    InvalidateCache();
}

void SpatialManager::UpdateObject(uint64_t id, const AABB& newBounds) {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);

        m_primaryIndex->Update(id, newBounds);

        auto it = m_objectLayers.find(id);
        if (it != m_objectLayers.end()) {
            size_t layerIdx = static_cast<size_t>(it->second);
            if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
                m_layerIndices[layerIdx]->Update(id, newBounds);
            }
        }
    } else {
        m_primaryIndex->Update(id, newBounds);

        auto it = m_objectLayers.find(id);
        if (it != m_objectLayers.end()) {
            size_t layerIdx = static_cast<size_t>(it->second);
            if (layerIdx < m_layerIndices.size() && m_layerIndices[layerIdx]) {
                m_layerIndices[layerIdx]->Update(id, newBounds);
            }
        }
    }

    ++m_frameStats.objectsUpdated;
}

void SpatialManager::RegisterObjects(std::span<const std::pair<uint64_t, AABB>> objects,
                                     SpatialLayer layer) {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);
        for (const auto& [id, bounds] : objects) {
            m_primaryIndex->Insert(id, bounds, static_cast<uint64_t>(layer));
            m_objectLayers[id] = layer;
        }
    } else {
        for (const auto& [id, bounds] : objects) {
            m_primaryIndex->Insert(id, bounds, static_cast<uint64_t>(layer));
            m_objectLayers[id] = layer;
        }
    }

    m_frameStats.objectsInserted += objects.size();
    InvalidateCache();
}

bool SpatialManager::IsRegistered(uint64_t id) const {
    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        return m_primaryIndex->Contains(id);
    }
    return m_primaryIndex->Contains(id);
}

AABB SpatialManager::GetObjectBounds(uint64_t id) const {
    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        return m_primaryIndex->GetObjectBounds(id);
    }
    return m_primaryIndex->GetObjectBounds(id);
}

SpatialQueryFilter SpatialManager::CreateFilter(uint64_t layerMask, uint64_t excludeId) const {
    SpatialQueryFilter filter;
    filter.layerMask = layerMask;
    filter.excludeId = excludeId;
    return filter;
}

size_t SpatialManager::ComputeQueryHash(const AABB& query, uint64_t layerMask) const {
    size_t hash = 0;
    hash ^= std::hash<float>{}(query.min.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(query.min.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(query.min.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(query.max.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(query.max.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(query.max.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint64_t>{}(layerMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
}

size_t SpatialManager::ComputeQueryHash(const glm::vec3& center, float radius, uint64_t layerMask) const {
    size_t hash = 0;
    hash ^= std::hash<float>{}(center.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(center.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(center.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(radius) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint64_t>{}(layerMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
}

void SpatialManager::RecordQueryStats(const std::string& queryType,
                                      const SpatialQueryStats& stats,
                                      float timeMs) {
    if (!m_config.enableProfiling) return;

    auto& profile = m_profilingData[queryType];
    profile.name = queryType;
    profile.queryCount++;
    profile.totalTimeMs += timeMs;
    profile.totalNodesVisited += stats.nodesVisited;
    profile.totalObjectsTested += stats.objectsTested;
    profile.totalObjectsReturned += stats.objectsReturned;

    m_frameStats.queriesThisFrame++;
    m_frameStats.totalQueryTimeMs += timeMs;
}

std::vector<uint64_t> SpatialManager::QueryAABB(const AABB& query, uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    // Check cache
    if (m_config.enableQueryCaching) {
        size_t hash = ComputeQueryHash(query, layerMask);

        if (m_config.threadSafe) {
            std::shared_lock lock(m_mutex);
            auto it = m_queryCache.find(hash);
            if (it != m_queryCache.end() && it->second.IsValid(m_currentFrame, hash)) {
                ++m_cacheHits;
                return it->second.results;
            }
        }
    }

    ++m_cacheMisses;

    std::vector<uint64_t> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        results = m_primaryIndex->QueryAABB(query, CreateFilter(layerMask));
    } else {
        results = m_primaryIndex->QueryAABB(query, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QueryAABB", m_primaryIndex->GetLastQueryStats(), timeMs);

    // Update cache
    if (m_config.enableQueryCaching) {
        size_t hash = ComputeQueryHash(query, layerMask);
        if (m_config.threadSafe) {
            std::unique_lock lock(m_mutex);
            m_queryCache[hash].Update(results, m_currentFrame, hash);
        } else {
            m_queryCache[hash].Update(results, m_currentFrame, hash);
        }
    }

    return results;
}

std::vector<uint64_t> SpatialManager::QuerySphere(const glm::vec3& center, float radius,
                                                   uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    if (m_config.enableQueryCaching) {
        size_t hash = ComputeQueryHash(center, radius, layerMask);

        if (m_config.threadSafe) {
            std::shared_lock lock(m_mutex);
            auto it = m_queryCache.find(hash);
            if (it != m_queryCache.end() && it->second.IsValid(m_currentFrame, hash)) {
                ++m_cacheHits;
                return it->second.results;
            }
        }
    }

    ++m_cacheMisses;

    std::vector<uint64_t> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        results = m_primaryIndex->QuerySphere(center, radius, CreateFilter(layerMask));
    } else {
        results = m_primaryIndex->QuerySphere(center, radius, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QuerySphere", m_primaryIndex->GetLastQueryStats(), timeMs);

    if (m_config.enableQueryCaching) {
        size_t hash = ComputeQueryHash(center, radius, layerMask);
        if (m_config.threadSafe) {
            std::unique_lock lock(m_mutex);
            m_queryCache[hash].Update(results, m_currentFrame, hash);
        } else {
            m_queryCache[hash].Update(results, m_currentFrame, hash);
        }
    }

    return results;
}

std::vector<uint64_t> SpatialManager::QueryFrustum(const Frustum& frustum, uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<uint64_t> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        results = m_primaryIndex->QueryFrustum(frustum, CreateFilter(layerMask));
    } else {
        results = m_primaryIndex->QueryFrustum(frustum, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QueryFrustum", m_primaryIndex->GetLastQueryStats(), timeMs);

    return results;
}

std::vector<RayHit> SpatialManager::QueryRay(const Ray& ray, float maxDist, uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<RayHit> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        results = m_primaryIndex->QueryRay(ray, maxDist, CreateFilter(layerMask));
    } else {
        results = m_primaryIndex->QueryRay(ray, maxDist, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QueryRay", m_primaryIndex->GetLastQueryStats(), timeMs);

    return results;
}

uint64_t SpatialManager::QueryNearest(const glm::vec3& point, float maxDist, uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t result;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        result = m_primaryIndex->QueryNearest(point, maxDist, CreateFilter(layerMask));
    } else {
        result = m_primaryIndex->QueryNearest(point, maxDist, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QueryNearest", m_primaryIndex->GetLastQueryStats(), timeMs);

    return result;
}

std::vector<uint64_t> SpatialManager::QueryKNearest(const glm::vec3& point, size_t k,
                                                     float maxDist, uint64_t layerMask) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<uint64_t> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        results = m_primaryIndex->QueryKNearest(point, k, maxDist, CreateFilter(layerMask));
    } else {
        results = m_primaryIndex->QueryKNearest(point, k, maxDist, CreateFilter(layerMask));
    }

    auto end = std::chrono::high_resolution_clock::now();
    float timeMs = std::chrono::duration<float, std::milli>(end - start).count();

    RecordQueryStats("QueryKNearest", m_primaryIndex->GetLastQueryStats(), timeMs);

    return results;
}

void SpatialManager::QueryAABB(const AABB& query, const ISpatialIndex::VisitorCallback& callback,
                               uint64_t layerMask) {
    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        m_primaryIndex->QueryAABB(query, callback, CreateFilter(layerMask));
    } else {
        m_primaryIndex->QueryAABB(query, callback, CreateFilter(layerMask));
    }
}

void SpatialManager::QuerySphere(const glm::vec3& center, float radius,
                                 const ISpatialIndex::VisitorCallback& callback,
                                 uint64_t layerMask) {
    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        m_primaryIndex->QuerySphere(center, radius, callback, CreateFilter(layerMask));
    } else {
        m_primaryIndex->QuerySphere(center, radius, callback, CreateFilter(layerMask));
    }
}

ISpatialIndex* SpatialManager::GetLayerIndex(SpatialLayer layer) {
    size_t idx = static_cast<size_t>(layer);
    if (idx < m_layerIndices.size() && m_layerIndices[idx]) {
        return m_layerIndices[idx].get();
    }
    return m_primaryIndex.get();
}

void SpatialManager::SetLayerIndex(SpatialLayer layer, std::unique_ptr<ISpatialIndex> index) {
    size_t idx = static_cast<size_t>(layer);
    if (idx < m_layerIndices.size()) {
        if (m_config.threadSafe) {
            std::unique_lock lock(m_mutex);
            m_layerIndices[idx] = std::move(index);
        } else {
            m_layerIndices[idx] = std::move(index);
        }
    }
}

std::vector<uint64_t> SpatialManager::GetLayerObjects(SpatialLayer layer) const {
    std::vector<uint64_t> results;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        for (const auto& [id, objLayer] : m_objectLayers) {
            if (objLayer == layer) {
                results.push_back(id);
            }
        }
    } else {
        for (const auto& [id, objLayer] : m_objectLayers) {
            if (objLayer == layer) {
                results.push_back(id);
            }
        }
    }

    return results;
}

void SpatialManager::InvalidateCache() {
    m_queryCache.clear();
}

float SpatialManager::GetCacheHitRatio() const noexcept {
    size_t total = m_cacheHits + m_cacheMisses;
    return total > 0 ? static_cast<float>(m_cacheHits) / total : 0.0f;
}

size_t SpatialManager::GetObjectCount() const {
    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        return m_primaryIndex->GetObjectCount();
    }
    return m_primaryIndex->GetObjectCount();
}

size_t SpatialManager::GetMemoryUsage() const {
    size_t memory = 0;

    if (m_config.threadSafe) {
        std::shared_lock lock(m_mutex);
        memory += m_primaryIndex->GetMemoryUsage();
        for (const auto& index : m_layerIndices) {
            if (index) {
                memory += index->GetMemoryUsage();
            }
        }
    } else {
        memory += m_primaryIndex->GetMemoryUsage();
        for (const auto& index : m_layerIndices) {
            if (index) {
                memory += index->GetMemoryUsage();
            }
        }
    }

    memory += m_objectLayers.size() * sizeof(std::pair<uint64_t, SpatialLayer>);
    memory += m_queryCache.size() * sizeof(std::pair<size_t, CachedQuery<uint64_t>>);

    return memory;
}

void SpatialManager::ResetProfilingData() {
    for (auto& [name, data] : m_profilingData) {
        data.Reset();
    }
}

void SpatialManager::RebuildAllIndices() {
    if (m_config.threadSafe) {
        std::unique_lock lock(m_mutex);
        m_primaryIndex->Rebuild();
        for (auto& index : m_layerIndices) {
            if (index) {
                index->Rebuild();
            }
        }
    } else {
        m_primaryIndex->Rebuild();
        for (auto& index : m_layerIndices) {
            if (index) {
                index->Rebuild();
            }
        }
    }

    InvalidateCache();
}

void SpatialManager::OptimizeIndices() {
    // Analyze current object distribution
    SpatialIndexType optimal = GetOptimalIndexType();

    if (optimal != m_config.defaultIndexType) {
        // Rebuild with optimal type
        // Collect all objects
        std::vector<std::tuple<uint64_t, AABB, SpatialLayer>> objects;

        for (const auto& [id, layer] : m_objectLayers) {
            AABB bounds = m_primaryIndex->GetObjectBounds(id);
            if (bounds.IsValid()) {
                objects.push_back({id, bounds, layer});
            }
        }

        // Create new index
        auto newIndex = CreateSpatialIndex(optimal, m_config.worldBounds, m_config.spatialHashCellSize);

        // Re-insert all objects
        for (const auto& [id, bounds, layer] : objects) {
            newIndex->Insert(id, bounds, static_cast<uint64_t>(layer));
        }

        // Swap indices
        if (m_config.threadSafe) {
            std::unique_lock lock(m_mutex);
            m_primaryIndex = std::move(newIndex);
        } else {
            m_primaryIndex = std::move(newIndex);
        }

        m_config.defaultIndexType = optimal;
    }
}

SpatialIndexType SpatialManager::GetOptimalIndexType() const {
    size_t objectCount = GetObjectCount();

    if (objectCount < 100) {
        return SpatialIndexType::BVH;  // BVH is good for small counts
    }

    // Analyze object distribution
    AABB worldBounds = m_primaryIndex->GetBounds();
    glm::vec3 worldSize = worldBounds.GetSize();
    float avgSize = (worldSize.x + worldSize.y + worldSize.z) / 3.0f;

    // Calculate average object size
    float totalObjSize = 0.0f;
    size_t sampledCount = 0;

    for (const auto& [id, layer] : m_objectLayers) {
        if (sampledCount >= 100) break;  // Sample first 100 objects

        AABB bounds = m_primaryIndex->GetObjectBounds(id);
        if (bounds.IsValid()) {
            glm::vec3 size = bounds.GetSize();
            totalObjSize += (size.x + size.y + size.z) / 3.0f;
            ++sampledCount;
        }
    }

    float avgObjSize = sampledCount > 0 ? totalObjSize / sampledCount : 1.0f;
    float sizeRatio = avgObjSize / avgSize;

    // If objects are roughly uniform size and small, spatial hash is good
    if (sizeRatio < 0.01f && objectCount > 1000) {
        return SpatialIndexType::SpatialHash;
    }

    // If many objects of varying sizes, use loose octree
    if (objectCount > 500) {
        return SpatialIndexType::LooseOctree;
    }

    // Default to BVH for general case
    return SpatialIndexType::BVH;
}

void SpatialManager::DrawDebug() {
    if (!m_debugVisualization) return;

    // Would integrate with debug renderer
    // For now, this is a placeholder
}

void SpatialManager::DrawDebugLayer(SpatialLayer layer, const glm::vec4& color) {
    if (!m_debugVisualization) return;

    auto objects = GetLayerObjects(layer);
    for (uint64_t id : objects) {
        AABB bounds = GetObjectBounds(id);
        // Would draw bounds with color using debug renderer
    }
}

void SpatialManager::DrawDebugQuery(const AABB& query, const std::vector<uint64_t>& results) {
    if (!m_debugVisualization) return;

    // Draw query bounds
    // Draw result bounds
}

} // namespace Nova
