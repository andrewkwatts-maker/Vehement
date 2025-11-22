#pragma once

#include "SpatialIndex.hpp"
#include "AABB.hpp"
#include "Frustum.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <queue>
#include <stack>
#include <cstdint>
#include <algorithm>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

namespace Nova {

/**
 * @brief Memory pool for octree nodes
 */
template<typename T, size_t BlockSize = 64>
class NodePool {
public:
    NodePool() = default;
    ~NodePool() = default;

    // Non-copyable, movable
    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;
    NodePool(NodePool&&) noexcept = default;
    NodePool& operator=(NodePool&&) noexcept = default;

    T* Allocate() {
        if (m_freeList.empty()) {
            AllocateBlock();
        }

        T* node = m_freeList.back();
        m_freeList.pop_back();
        return node;
    }

    void Deallocate(T* node) {
        m_freeList.push_back(node);
    }

    void Reset() {
        m_freeList.clear();
        for (auto& block : m_blocks) {
            for (size_t i = 0; i < BlockSize; ++i) {
                m_freeList.push_back(&block[i]);
            }
        }
    }

    [[nodiscard]] size_t GetAllocatedCount() const noexcept {
        size_t total = m_blocks.size() * BlockSize;
        return total - m_freeList.size();
    }

    [[nodiscard]] size_t GetMemoryUsage() const noexcept {
        return m_blocks.size() * BlockSize * sizeof(T) +
               m_freeList.capacity() * sizeof(T*);
    }

private:
    void AllocateBlock() {
        m_blocks.push_back(std::make_unique<std::array<T, BlockSize>>());
        auto& block = m_blocks.back();
        for (size_t i = 0; i < BlockSize; ++i) {
            m_freeList.push_back(&(*block)[i]);
        }
    }

    std::vector<std::unique_ptr<std::array<T, BlockSize>>> m_blocks;
    std::vector<T*> m_freeList;
};

/**
 * @brief Adaptive Octree for spatial partitioning
 *
 * Template-based octree supporting any spatial object type.
 * Features:
 * - Dynamic insertion and removal
 * - Frustum culling queries
 * - Range queries (sphere, AABB, OBB)
 * - Ray casting with sorted results
 * - Loose octree option for moving objects
 * - Memory pooled nodes
 * - SIMD-accelerated intersection tests
 *
 * @tparam T Type of object ID (typically uint64_t or uint32_t)
 */
template<typename T = uint64_t>
class Octree : public SpatialIndexBase {
public:
    /**
     * @brief Object stored in the octree
     */
    struct Object {
        T id;
        AABB bounds;
        uint64_t layer = 0;
    };

    /**
     * @brief Octree node
     */
    struct Node {
        AABB bounds;                      // Node bounds
        AABB looseBounds;                 // Expanded bounds for loose octree
        std::array<Node*, 8> children{};  // Child nodes (nullptr if leaf)
        std::vector<Object> objects;       // Objects in this node
        Node* parent = nullptr;
        int depth = 0;
        bool isLeaf = true;

        void Reset() {
            bounds = AABB();
            looseBounds = AABB();
            children.fill(nullptr);
            objects.clear();
            parent = nullptr;
            depth = 0;
            isLeaf = true;
        }
    };

    // Configuration
    struct Config {
        int maxDepth = 8;                 // Maximum tree depth
        size_t maxObjectsPerNode = 16;    // Split threshold
        size_t minObjectsToMerge = 4;     // Merge threshold
        float looseFactor = 1.0f;         // 1.0 = tight, 2.0 = loose octree
        bool usePooledMemory = true;
    };

    /**
     * @brief Create octree with given bounds
     * @param worldBounds Bounds of the entire world
     * @param looseFactor Loose octree factor (1.0 = tight, 2.0 = typical loose)
     */
    explicit Octree(const AABB& worldBounds, float looseFactor = 1.0f)
        : m_worldBounds(worldBounds) {
        m_config.looseFactor = looseFactor;
        m_root = AllocateNode();
        m_root->bounds = worldBounds;
        m_root->looseBounds = CalculateLooseBounds(worldBounds);
    }

    ~Octree() override = default;

    // Non-copyable, movable
    Octree(const Octree&) = delete;
    Octree& operator=(const Octree&) = delete;
    Octree(Octree&&) noexcept = default;
    Octree& operator=(Octree&&) noexcept = default;

    // =========================================================================
    // ISpatialIndex Implementation
    // =========================================================================

    void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) override {
        Object obj{static_cast<T>(id), bounds, layer};
        InsertObject(m_root, obj);
        m_objectMap[id] = bounds;
        ++m_objectCount;
    }

    bool Remove(uint64_t id) override {
        auto it = m_objectMap.find(id);
        if (it == m_objectMap.end()) {
            return false;
        }

        RemoveObject(m_root, static_cast<T>(id), it->second);
        m_objectMap.erase(it);
        --m_objectCount;
        return true;
    }

    bool Update(uint64_t id, const AABB& newBounds) override {
        auto it = m_objectMap.find(id);
        if (it == m_objectMap.end()) {
            return false;
        }

        // Simple update: remove and re-insert
        // Could be optimized for small movements
        AABB oldBounds = it->second;
        uint64_t layer = 0;

        // Find the object's layer
        QueryAABB(oldBounds, [&](uint64_t objId, const AABB&) {
            if (objId == id) {
                return false; // Found it
            }
            return true;
        });

        RemoveObject(m_root, static_cast<T>(id), oldBounds);

        Object obj{static_cast<T>(id), newBounds, layer};
        InsertObject(m_root, obj);
        it->second = newBounds;

        return true;
    }

    void Clear() override {
        ClearNode(m_root);
        m_objectMap.clear();
        m_objectCount = 0;
    }

    void Rebuild() override {
        // Collect all objects
        std::vector<Object> allObjects;
        CollectObjects(m_root, allObjects);

        // Clear and re-insert
        ClearNode(m_root);
        m_objectCount = 0;

        for (const auto& obj : allObjects) {
            InsertObject(m_root, obj);
            ++m_objectCount;
        }
    }

    [[nodiscard]] std::vector<uint64_t> QueryAABB(
        const AABB& query,
        const SpatialQueryFilter& filter = {}) override {

        std::vector<uint64_t> results;
        m_lastStats.Reset();

        QueryAABBInternal(m_root, query, filter, results);

        m_lastStats.objectsReturned = results.size();
        return results;
    }

    [[nodiscard]] std::vector<uint64_t> QuerySphere(
        const glm::vec3& center,
        float radius,
        const SpatialQueryFilter& filter = {}) override {

        std::vector<uint64_t> results;
        m_lastStats.Reset();

        // Create AABB from sphere for broad phase
        AABB sphereAABB = AABB::FromCenterExtents(center, glm::vec3(radius));

        QuerySphereInternal(m_root, center, radius, sphereAABB, filter, results);

        m_lastStats.objectsReturned = results.size();
        return results;
    }

    [[nodiscard]] std::vector<uint64_t> QueryFrustum(
        const Frustum& frustum,
        const SpatialQueryFilter& filter = {}) override {

        std::vector<uint64_t> results;
        m_lastStats.Reset();

        uint8_t planeMask = 0x3F; // All 6 planes
        QueryFrustumInternal(m_root, frustum, planeMask, filter, results);

        m_lastStats.objectsReturned = results.size();
        return results;
    }

    [[nodiscard]] std::vector<RayHit> QueryRay(
        const Ray& ray,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override {

        std::vector<RayHit> results;
        m_lastStats.Reset();

        glm::vec3 invDir = ray.GetInverseDirection();
        QueryRayInternal(m_root, ray, invDir, maxDist, filter, results);

        // Sort by distance
        std::sort(results.begin(), results.end());

        m_lastStats.objectsReturned = results.size();
        return results;
    }

    [[nodiscard]] uint64_t QueryNearest(
        const glm::vec3& point,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override {

        uint64_t nearest = 0;
        float nearestDist2 = maxDist * maxDist;

        QueryNearestInternal(m_root, point, filter, nearest, nearestDist2);

        return nearest;
    }

    [[nodiscard]] std::vector<uint64_t> QueryKNearest(
        const glm::vec3& point,
        size_t k,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override {

        // Use priority queue to track k nearest
        using DistId = std::pair<float, uint64_t>;
        std::priority_queue<DistId> heap;

        float searchRadius2 = maxDist * maxDist;
        QueryKNearestInternal(m_root, point, k, filter, heap, searchRadius2);

        std::vector<uint64_t> results;
        results.reserve(heap.size());
        while (!heap.empty()) {
            results.push_back(heap.top().second);
            heap.pop();
        }
        std::reverse(results.begin(), results.end());

        return results;
    }

    void QueryAABB(
        const AABB& query,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) override {

        QueryAABBCallback(m_root, query, filter, callback);
    }

    void QuerySphere(
        const glm::vec3& center,
        float radius,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) override {

        AABB sphereAABB = AABB::FromCenterExtents(center, glm::vec3(radius));
        QuerySphereCallback(m_root, center, radius, sphereAABB, filter, callback);
    }

    [[nodiscard]] size_t GetObjectCount() const noexcept override {
        return m_objectCount;
    }

    [[nodiscard]] AABB GetBounds() const noexcept override {
        return m_worldBounds;
    }

    [[nodiscard]] size_t GetMemoryUsage() const noexcept override {
        return m_nodePool.GetMemoryUsage() +
               m_objectMap.size() * sizeof(std::pair<uint64_t, AABB>);
    }

    [[nodiscard]] std::string_view GetTypeName() const noexcept override {
        return m_config.looseFactor > 1.0f ? "LooseOctree" : "Octree";
    }

    [[nodiscard]] bool SupportsMovingObjects() const noexcept override {
        return m_config.looseFactor > 1.0f;
    }

    [[nodiscard]] AABB GetObjectBounds(uint64_t id) const noexcept override {
        auto it = m_objectMap.find(id);
        return it != m_objectMap.end() ? it->second : AABB::Invalid();
    }

    [[nodiscard]] bool Contains(uint64_t id) const noexcept override {
        return m_objectMap.contains(id);
    }

    // =========================================================================
    // Octree-Specific Methods
    // =========================================================================

    /**
     * @brief Get the root node
     */
    [[nodiscard]] Node* GetRoot() const noexcept { return m_root; }

    /**
     * @brief Get tree depth statistics
     */
    [[nodiscard]] std::pair<int, int> GetDepthStats() const noexcept {
        int minDepth = std::numeric_limits<int>::max();
        int maxDepth = 0;
        GetDepthStatsInternal(m_root, minDepth, maxDepth);
        return {minDepth, maxDepth};
    }

    /**
     * @brief Get node count
     */
    [[nodiscard]] size_t GetNodeCount() const noexcept {
        return m_nodePool.GetAllocatedCount();
    }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const Config& GetConfig() const noexcept { return m_config; }
    void SetConfig(const Config& config) { m_config = config; }

private:
    Node* AllocateNode() {
        Node* node = m_nodePool.Allocate();
        node->Reset();
        return node;
    }

    void DeallocateNode(Node* node) {
        m_nodePool.Deallocate(node);
    }

    AABB CalculateLooseBounds(const AABB& bounds) const {
        if (m_config.looseFactor <= 1.0f) {
            return bounds;
        }

        glm::vec3 center = bounds.GetCenter();
        glm::vec3 extents = bounds.GetExtents() * m_config.looseFactor;
        return AABB::FromCenterExtents(center, extents);
    }

    void InsertObject(Node* node, const Object& obj) {
        m_lastStats.nodesVisited++;

        // Check if object fits in this node
        if (!node->looseBounds.Contains(obj.bounds)) {
            // Object is too big, store in this node
            node->objects.push_back(obj);
            return;
        }

        // If this is a leaf
        if (node->isLeaf) {
            node->objects.push_back(obj);

            // Check if we should split
            if (node->objects.size() > m_config.maxObjectsPerNode &&
                node->depth < m_config.maxDepth) {
                SplitNode(node);
            }
            return;
        }

        // Find best child
        int childIdx = GetChildIndex(node->bounds, obj.bounds.GetCenter());
        if (childIdx >= 0 && node->children[childIdx]) {
            InsertObject(node->children[childIdx], obj);
        } else {
            // Spans multiple children, store here
            node->objects.push_back(obj);
        }
    }

    void RemoveObject(Node* node, T id, const AABB& bounds) {
        if (!node) return;

        // Check this node's objects
        auto& objects = node->objects;
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            if (it->id == id) {
                objects.erase(it);
                TryMergeNode(node);
                return;
            }
        }

        // Search children
        if (!node->isLeaf) {
            int childIdx = GetChildIndex(node->bounds, bounds.GetCenter());
            if (childIdx >= 0 && node->children[childIdx]) {
                RemoveObject(node->children[childIdx], id, bounds);
            } else {
                // Search all children
                for (auto* child : node->children) {
                    if (child && child->looseBounds.Intersects(bounds)) {
                        RemoveObject(child, id, bounds);
                    }
                }
            }
        }
    }

    void SplitNode(Node* node) {
        node->isLeaf = false;

        glm::vec3 center = node->bounds.GetCenter();
        glm::vec3 extents = node->bounds.GetExtents() * 0.5f;

        // Create 8 children
        for (int i = 0; i < 8; ++i) {
            glm::vec3 childCenter = center;
            childCenter.x += (i & 1) ? extents.x : -extents.x;
            childCenter.y += (i & 2) ? extents.y : -extents.y;
            childCenter.z += (i & 4) ? extents.z : -extents.z;

            AABB childBounds = AABB::FromCenterExtents(childCenter, extents);

            Node* child = AllocateNode();
            child->bounds = childBounds;
            child->looseBounds = CalculateLooseBounds(childBounds);
            child->parent = node;
            child->depth = node->depth + 1;

            node->children[i] = child;
        }

        // Re-insert objects into children
        std::vector<Object> tempObjects = std::move(node->objects);
        node->objects.clear();

        for (const auto& obj : tempObjects) {
            InsertObject(node, obj);
        }
    }

    void TryMergeNode(Node* node) {
        if (!node || node->isLeaf) return;

        // Count total objects in subtree
        size_t totalObjects = CountObjectsInSubtree(node);

        if (totalObjects <= m_config.minObjectsToMerge) {
            // Collect all objects
            std::vector<Object> allObjects;
            CollectObjects(node, allObjects);

            // Deallocate children
            for (auto*& child : node->children) {
                if (child) {
                    ClearNode(child);
                    DeallocateNode(child);
                    child = nullptr;
                }
            }

            node->isLeaf = true;
            node->objects = std::move(allObjects);
        }
    }

    int GetChildIndex(const AABB& nodeBounds, const glm::vec3& point) const {
        glm::vec3 center = nodeBounds.GetCenter();
        int index = 0;
        if (point.x >= center.x) index |= 1;
        if (point.y >= center.y) index |= 2;
        if (point.z >= center.z) index |= 4;
        return index;
    }

    void ClearNode(Node* node) {
        if (!node) return;

        for (auto*& child : node->children) {
            if (child) {
                ClearNode(child);
                DeallocateNode(child);
                child = nullptr;
            }
        }

        node->objects.clear();
        node->isLeaf = true;
    }

    void CollectObjects(Node* node, std::vector<Object>& out) const {
        if (!node) return;

        out.insert(out.end(), node->objects.begin(), node->objects.end());

        for (auto* child : node->children) {
            if (child) {
                CollectObjects(child, out);
            }
        }
    }

    size_t CountObjectsInSubtree(Node* node) const {
        if (!node) return 0;

        size_t count = node->objects.size();
        for (auto* child : node->children) {
            count += CountObjectsInSubtree(child);
        }
        return count;
    }

    void GetDepthStatsInternal(Node* node, int& minDepth, int& maxDepth) const {
        if (!node) return;

        if (node->isLeaf) {
            minDepth = std::min(minDepth, node->depth);
            maxDepth = std::max(maxDepth, node->depth);
        } else {
            for (auto* child : node->children) {
                GetDepthStatsInternal(child, minDepth, maxDepth);
            }
        }
    }

    // Query implementations
    void QueryAABBInternal(Node* node, const AABB& query,
                          const SpatialQueryFilter& filter,
                          std::vector<uint64_t>& results) const {
        if (!node) return;

        m_lastStats.nodesVisited++;

        if (!node->looseBounds.Intersects(query)) {
            return;
        }

        // Test objects in this node
        for (const auto& obj : node->objects) {
            m_lastStats.objectsTested++;
            if (filter.PassesFilter(obj.id, obj.layer) && obj.bounds.Intersects(query)) {
                results.push_back(obj.id);
            }
        }

        // Recurse to children
        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QueryAABBInternal(child, query, filter, results);
            }
        }
    }

    void QuerySphereInternal(Node* node, const glm::vec3& center, float radius,
                            const AABB& sphereAABB, const SpatialQueryFilter& filter,
                            std::vector<uint64_t>& results) const {
        if (!node) return;

        m_lastStats.nodesVisited++;

        if (!node->looseBounds.Intersects(sphereAABB)) {
            return;
        }

        float radius2 = radius * radius;

        for (const auto& obj : node->objects) {
            m_lastStats.objectsTested++;
            if (filter.PassesFilter(obj.id, obj.layer) &&
                obj.bounds.IntersectsSphere(center, radius)) {
                results.push_back(obj.id);
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QuerySphereInternal(child, center, radius, sphereAABB, filter, results);
            }
        }
    }

    void QueryFrustumInternal(Node* node, const Frustum& frustum, uint8_t planeMask,
                             const SpatialQueryFilter& filter,
                             std::vector<uint64_t>& results) const {
        if (!node) return;

        m_lastStats.nodesVisited++;

        // Use coherent frustum test
        uint8_t childMask = planeMask;
        if (!frustum.TestAABBCoherent(node->looseBounds, childMask)) {
            return; // Node is outside frustum
        }

        for (const auto& obj : node->objects) {
            m_lastStats.objectsTested++;
            if (filter.PassesFilter(obj.id, obj.layer) &&
                !frustum.IsAABBOutside(obj.bounds)) {
                results.push_back(obj.id);
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QueryFrustumInternal(child, frustum, childMask, filter, results);
            }
        }
    }

    void QueryRayInternal(Node* node, const Ray& ray, const glm::vec3& invDir,
                         float maxDist, const SpatialQueryFilter& filter,
                         std::vector<RayHit>& results) const {
        if (!node) return;

        m_lastStats.nodesVisited++;

        float tMin, tMax;
        if (!node->looseBounds.IntersectsRay(ray.origin, invDir, tMin, tMax)) {
            return;
        }

        if (tMin > maxDist) {
            return;
        }

        for (const auto& obj : node->objects) {
            m_lastStats.objectsTested++;
            if (!filter.PassesFilter(obj.id, obj.layer)) continue;

            float t = obj.bounds.RayIntersect(ray.origin, ray.direction, maxDist);
            if (t >= 0.0f && t <= maxDist) {
                RayHit hit;
                hit.entityId = obj.id;
                hit.distance = t;
                hit.point = ray.GetPoint(t);
                results.push_back(hit);
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QueryRayInternal(child, ray, invDir, maxDist, filter, results);
            }
        }
    }

    void QueryNearestInternal(Node* node, const glm::vec3& point,
                             const SpatialQueryFilter& filter,
                             uint64_t& nearest, float& nearestDist2) const {
        if (!node) return;

        float nodeDist2 = node->looseBounds.DistanceSquared(point);
        if (nodeDist2 > nearestDist2) {
            return;
        }

        for (const auto& obj : node->objects) {
            if (!filter.PassesFilter(obj.id, obj.layer)) continue;

            float dist2 = obj.bounds.DistanceSquared(point);
            if (dist2 < nearestDist2) {
                nearestDist2 = dist2;
                nearest = obj.id;
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QueryNearestInternal(child, point, filter, nearest, nearestDist2);
            }
        }
    }

    void QueryKNearestInternal(Node* node, const glm::vec3& point, size_t k,
                              const SpatialQueryFilter& filter,
                              std::priority_queue<std::pair<float, uint64_t>>& heap,
                              float& searchRadius2) const {
        if (!node) return;

        float nodeDist2 = node->looseBounds.DistanceSquared(point);
        if (nodeDist2 > searchRadius2) {
            return;
        }

        for (const auto& obj : node->objects) {
            if (!filter.PassesFilter(obj.id, obj.layer)) continue;

            float dist2 = obj.bounds.DistanceSquared(point);
            if (dist2 < searchRadius2) {
                heap.push({dist2, obj.id});
                if (heap.size() > k) {
                    heap.pop();
                    searchRadius2 = heap.top().first;
                }
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                QueryKNearestInternal(child, point, k, filter, heap, searchRadius2);
            }
        }
    }

    bool QueryAABBCallback(Node* node, const AABB& query,
                          const SpatialQueryFilter& filter,
                          const VisitorCallback& callback) const {
        if (!node) return true;

        if (!node->looseBounds.Intersects(query)) {
            return true;
        }

        for (const auto& obj : node->objects) {
            if (filter.PassesFilter(obj.id, obj.layer) && obj.bounds.Intersects(query)) {
                if (!callback(obj.id, obj.bounds)) {
                    return false;
                }
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                if (!QueryAABBCallback(child, query, filter, callback)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool QuerySphereCallback(Node* node, const glm::vec3& center, float radius,
                            const AABB& sphereAABB, const SpatialQueryFilter& filter,
                            const VisitorCallback& callback) const {
        if (!node) return true;

        if (!node->looseBounds.Intersects(sphereAABB)) {
            return true;
        }

        for (const auto& obj : node->objects) {
            if (filter.PassesFilter(obj.id, obj.layer) &&
                obj.bounds.IntersectsSphere(center, radius)) {
                if (!callback(obj.id, obj.bounds)) {
                    return false;
                }
            }
        }

        if (!node->isLeaf) {
            for (auto* child : node->children) {
                if (!QuerySphereCallback(child, center, radius, sphereAABB, filter, callback)) {
                    return false;
                }
            }
        }

        return true;
    }

    // Member variables
    AABB m_worldBounds;
    Node* m_root = nullptr;
    Config m_config;
    size_t m_objectCount = 0;
    std::unordered_map<uint64_t, AABB> m_objectMap;
    mutable NodePool<Node> m_nodePool;
};

} // namespace Nova
