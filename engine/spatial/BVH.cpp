#include "BVH.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace Nova {

void BVH::Insert(uint64_t id, const AABB& bounds, uint64_t layer) {
    Primitive prim;
    prim.id = id;
    prim.bounds = bounds;
    prim.centroid = bounds.GetCenter();
    prim.layer = layer;

    m_idToIndex[id] = static_cast<uint32_t>(m_primitives.size());
    m_primitives.push_back(prim);
    m_needsRebuild = true;
}

bool BVH::Remove(uint64_t id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return false;
    }

    uint32_t index = it->second;

    // Swap with last element
    if (index < m_primitives.size() - 1) {
        m_primitives[index] = m_primitives.back();
        m_idToIndex[m_primitives[index].id] = index;
    }

    m_primitives.pop_back();
    m_idToIndex.erase(it);
    m_needsRebuild = true;

    return true;
}

bool BVH::Update(uint64_t id, const AABB& newBounds) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return false;
    }

    m_primitives[it->second].bounds = newBounds;
    m_primitives[it->second].centroid = newBounds.GetCenter();

    // Check if bounds changed significantly - if so, mark for rebuild
    // Otherwise, just refit
    m_needsRebuild = true;

    return true;
}

void BVH::Clear() {
    m_nodes.clear();
    m_primitives.clear();
    m_primitiveIndices.clear();
    m_idToIndex.clear();
    m_needsRebuild = false;
}

void BVH::Rebuild() {
    if (m_primitives.empty()) {
        m_nodes.clear();
        return;
    }

    BuildTopDownSAH();
    m_needsRebuild = false;
}

void BVH::Build() {
    BuildTopDownSAH();
}

void BVH::BuildTopDownSAH() {
    if (m_primitives.empty()) {
        m_nodes.clear();
        return;
    }

    // Initialize primitive indices
    m_primitiveIndices.resize(m_primitives.size());
    std::iota(m_primitiveIndices.begin(), m_primitiveIndices.end(), 0);

    // Allocate nodes (worst case: 2n-1 nodes for n primitives)
    m_nodes.clear();
    m_nodes.reserve(2 * m_primitives.size());

    // Build recursively
    BuildRecursive(0, static_cast<uint32_t>(m_primitives.size()), 0);

#ifdef __SSE__
    BuildSIMDNodes();
#endif
}

uint32_t BVH::BuildRecursive(uint32_t begin, uint32_t end, int depth) {
    uint32_t nodeIndex = static_cast<uint32_t>(m_nodes.size());
    m_nodes.push_back(Node{});
    Node& node = m_nodes[nodeIndex];

    uint32_t primCount = end - begin;

    // Calculate bounds
    AABB bounds;
    AABB centroidBounds;
    for (uint32_t i = begin; i < end; ++i) {
        const Primitive& prim = m_primitives[m_primitiveIndices[i]];
        bounds.Expand(prim.bounds);
        centroidBounds.Expand(prim.centroid);
    }
    node.bounds = bounds;

    // Create leaf if few primitives or max depth reached
    if (primCount <= m_config.maxPrimitivesPerLeaf || depth > 64) {
        node.leftFirst = begin;
        node.count = primCount;
        return nodeIndex;
    }

    // Find best split using SAH
    SAHSplit split = FindBestSplit(begin, end, centroidBounds);

    // Check if split is worthwhile
    float leafCost = primCount * m_config.intersectionCost;
    if (split.cost >= leafCost) {
        node.leftFirst = begin;
        node.count = primCount;
        return nodeIndex;
    }

    // Partition primitives
    uint32_t mid = begin;
    for (uint32_t i = begin; i < end; ++i) {
        const Primitive& prim = m_primitives[m_primitiveIndices[i]];
        if (prim.centroid[split.axis] < split.position) {
            std::swap(m_primitiveIndices[i], m_primitiveIndices[mid]);
            ++mid;
        }
    }

    // Fallback if partition failed
    if (mid == begin || mid == end) {
        mid = (begin + end) / 2;
        std::nth_element(
            m_primitiveIndices.begin() + begin,
            m_primitiveIndices.begin() + mid,
            m_primitiveIndices.begin() + end,
            [this, axis = split.axis](uint32_t a, uint32_t b) {
                return m_primitives[a].centroid[axis] < m_primitives[b].centroid[axis];
            }
        );
    }

    // Build children
    node.count = 0; // Internal node
    node.leftFirst = static_cast<uint32_t>(m_nodes.size());

    BuildRecursive(begin, mid, depth + 1);
    BuildRecursive(mid, end, depth + 1);

    return nodeIndex;
}

BVH::SAHSplit BVH::FindBestSplit(uint32_t begin, uint32_t end, const AABB& centroidBounds) {
    SAHSplit best;

    if (!m_config.useBinnedSAH) {
        // Full SAH evaluation (slower but more accurate)
        for (int axis = 0; axis < 3; ++axis) {
            // Sort by centroid on this axis
            std::sort(
                m_primitiveIndices.begin() + begin,
                m_primitiveIndices.begin() + end,
                [this, axis](uint32_t a, uint32_t b) {
                    return m_primitives[a].centroid[axis] < m_primitives[b].centroid[axis];
                }
            );

            // Evaluate all split positions
            std::vector<float> leftAreas(end - begin);
            std::vector<float> rightAreas(end - begin);

            AABB leftBounds;
            for (uint32_t i = begin; i < end; ++i) {
                leftBounds.Expand(m_primitives[m_primitiveIndices[i]].bounds);
                leftAreas[i - begin] = leftBounds.GetSurfaceArea();
            }

            AABB rightBounds;
            for (uint32_t i = end; i > begin; --i) {
                rightBounds.Expand(m_primitives[m_primitiveIndices[i - 1]].bounds);
                rightAreas[i - 1 - begin] = rightBounds.GetSurfaceArea();
            }

            for (uint32_t i = begin; i < end - 1; ++i) {
                uint32_t leftCount = i - begin + 1;
                uint32_t rightCount = end - i - 1;

                float cost = m_config.traversalCost +
                    leftAreas[i - begin] * leftCount * m_config.intersectionCost +
                    rightAreas[i - begin + 1] * rightCount * m_config.intersectionCost;

                if (cost < best.cost) {
                    best.cost = cost;
                    best.axis = axis;
                    best.position = (m_primitives[m_primitiveIndices[i]].centroid[axis] +
                                    m_primitives[m_primitiveIndices[i + 1]].centroid[axis]) * 0.5f;
                }
            }
        }
    } else {
        // Binned SAH (faster)
        std::array<SAHBin, 64> bins;
        uint32_t numBins = std::min(m_config.sahBins, 64u);

        for (int axis = 0; axis < 3; ++axis) {
            float axisMin = centroidBounds.min[axis];
            float axisMax = centroidBounds.max[axis];

            if (axisMax - axisMin < 1e-6f) continue;

            float scale = numBins / (axisMax - axisMin);

            // Clear bins
            for (uint32_t i = 0; i < numBins; ++i) {
                bins[i].bounds = AABB();
                bins[i].count = 0;
            }

            // Populate bins
            for (uint32_t i = begin; i < end; ++i) {
                const Primitive& prim = m_primitives[m_primitiveIndices[i]];
                uint32_t binIdx = std::min(
                    static_cast<uint32_t>((prim.centroid[axis] - axisMin) * scale),
                    numBins - 1
                );
                bins[binIdx].bounds.Expand(prim.bounds);
                bins[binIdx].count++;
            }

            // Compute costs for splits between bins
            std::array<float, 64> leftAreas, rightAreas;
            std::array<uint32_t, 64> leftCounts, rightCounts;

            AABB leftBounds, rightBounds;
            uint32_t leftCount = 0, rightCount = 0;

            for (uint32_t i = 0; i < numBins; ++i) {
                leftBounds.Expand(bins[i].bounds);
                leftCount += bins[i].count;
                leftAreas[i] = leftBounds.GetSurfaceArea();
                leftCounts[i] = leftCount;
            }

            for (int i = numBins - 1; i >= 0; --i) {
                rightBounds.Expand(bins[i].bounds);
                rightCount += bins[i].count;
                rightAreas[i] = rightBounds.GetSurfaceArea();
                rightCounts[i] = rightCount;
            }

            float binWidth = (axisMax - axisMin) / numBins;

            for (uint32_t i = 0; i < numBins - 1; ++i) {
                if (leftCounts[i] == 0 || rightCounts[i + 1] == 0) continue;

                float cost = m_config.traversalCost +
                    leftAreas[i] * leftCounts[i] * m_config.intersectionCost +
                    rightAreas[i + 1] * rightCounts[i + 1] * m_config.intersectionCost;

                if (cost < best.cost) {
                    best.cost = cost;
                    best.axis = axis;
                    best.position = axisMin + (i + 1) * binWidth;
                }
            }
        }
    }

    return best;
}

void BVH::Refit() {
    if (m_nodes.empty()) return;

    // Update primitive bounds in leaves first
    for (auto& prim : m_primitives) {
        prim.centroid = prim.bounds.GetCenter();
    }

    // Refit from leaves up
    RefitRecursive(0);
}

void BVH::RefitRecursive(uint32_t nodeIndex) {
    Node& node = m_nodes[nodeIndex];

    if (node.IsLeaf()) {
        node.bounds = AABB();
        for (uint32_t i = 0; i < node.count; ++i) {
            node.bounds.Expand(m_primitives[m_primitiveIndices[node.leftFirst + i]].bounds);
        }
    } else {
        RefitRecursive(node.leftFirst);
        RefitRecursive(node.leftFirst + 1);

        node.bounds = AABB::Merge(
            m_nodes[node.leftFirst].bounds,
            m_nodes[node.leftFirst + 1].bounds
        );
    }
}

int BVH::GetDepth() const noexcept {
    if (m_nodes.empty()) return 0;
    return CalculateDepthRecursive(0);
}

int BVH::CalculateDepthRecursive(uint32_t nodeIndex) const {
    const Node& node = m_nodes[nodeIndex];
    if (node.IsLeaf()) return 1;

    return 1 + std::max(
        CalculateDepthRecursive(node.leftFirst),
        CalculateDepthRecursive(node.leftFirst + 1)
    );
}

float BVH::GetSAHCost() const noexcept {
    if (m_nodes.empty()) return 0.0f;

    float cost = 0.0f;
    float rootArea = m_nodes[0].bounds.GetSurfaceArea();

    for (const auto& node : m_nodes) {
        float nodeArea = node.bounds.GetSurfaceArea();
        float prob = nodeArea / rootArea;

        if (node.IsLeaf()) {
            cost += prob * node.count * m_config.intersectionCost;
        } else {
            cost += prob * m_config.traversalCost;
        }
    }

    return cost;
}

// Query implementations
std::vector<uint64_t> BVH::QueryAABB(const AABB& query, const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    if (!m_nodes.empty() && m_needsRebuild) {
        Rebuild();
    }

    if (!m_nodes.empty()) {
        QueryAABBInternal(0, query, filter, results);
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

void BVH::QueryAABBInternal(uint32_t nodeIndex, const AABB& query,
                           const SpatialQueryFilter& filter,
                           std::vector<uint64_t>& results) const {
    m_lastStats.nodesVisited++;

    const Node& node = m_nodes[nodeIndex];

    if (!node.bounds.Intersects(query)) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];
            m_lastStats.objectsTested++;

            if (filter.PassesFilter(prim.id, prim.layer) && prim.bounds.Intersects(query)) {
                results.push_back(prim.id);
            }
        }
    } else {
        QueryAABBInternal(node.leftFirst, query, filter, results);
        QueryAABBInternal(node.leftFirst + 1, query, filter, results);
    }
}

std::vector<uint64_t> BVH::QuerySphere(const glm::vec3& center, float radius,
                                       const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    if (!m_nodes.empty() && m_needsRebuild) {
        Rebuild();
    }

    if (!m_nodes.empty()) {
        QuerySphereInternal(0, center, radius, radius * radius, filter, results);
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

void BVH::QuerySphereInternal(uint32_t nodeIndex, const glm::vec3& center,
                             float radius, float radius2,
                             const SpatialQueryFilter& filter,
                             std::vector<uint64_t>& results) const {
    m_lastStats.nodesVisited++;

    const Node& node = m_nodes[nodeIndex];

    if (!node.bounds.IntersectsSphere(center, radius)) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];
            m_lastStats.objectsTested++;

            if (filter.PassesFilter(prim.id, prim.layer) &&
                prim.bounds.IntersectsSphere(center, radius)) {
                results.push_back(prim.id);
            }
        }
    } else {
        QuerySphereInternal(node.leftFirst, center, radius, radius2, filter, results);
        QuerySphereInternal(node.leftFirst + 1, center, radius, radius2, filter, results);
    }
}

std::vector<uint64_t> BVH::QueryFrustum(const Frustum& frustum,
                                        const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    if (!m_nodes.empty() && m_needsRebuild) {
        Rebuild();
    }

    if (!m_nodes.empty()) {
        QueryFrustumInternal(0, frustum, 0x3F, filter, results);
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

void BVH::QueryFrustumInternal(uint32_t nodeIndex, const Frustum& frustum,
                              uint8_t planeMask,
                              const SpatialQueryFilter& filter,
                              std::vector<uint64_t>& results) const {
    m_lastStats.nodesVisited++;

    const Node& node = m_nodes[nodeIndex];

    uint8_t childMask = planeMask;
    if (!frustum.TestAABBCoherent(node.bounds, childMask)) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];
            m_lastStats.objectsTested++;

            if (filter.PassesFilter(prim.id, prim.layer) &&
                !frustum.IsAABBOutside(prim.bounds)) {
                results.push_back(prim.id);
            }
        }
    } else {
        QueryFrustumInternal(node.leftFirst, frustum, childMask, filter, results);
        QueryFrustumInternal(node.leftFirst + 1, frustum, childMask, filter, results);
    }
}

std::vector<RayHit> BVH::QueryRay(const Ray& ray, float maxDist,
                                  const SpatialQueryFilter& filter) {
    std::vector<RayHit> results;
    m_lastStats.Reset();

    if (!m_nodes.empty() && m_needsRebuild) {
        Rebuild();
    }

    if (!m_nodes.empty()) {
        glm::vec3 invDir = ray.GetInverseDirection();
        QueryRayInternal(0, ray, invDir, maxDist, filter, results);
        std::sort(results.begin(), results.end());
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

void BVH::QueryRayInternal(uint32_t nodeIndex, const Ray& ray,
                          const glm::vec3& invDir, float& maxDist,
                          const SpatialQueryFilter& filter,
                          std::vector<RayHit>& results) const {
    m_lastStats.nodesVisited++;

    const Node& node = m_nodes[nodeIndex];

    float tMin, tMax;
    if (!node.bounds.IntersectsRay(ray.origin, invDir, tMin, tMax)) {
        return;
    }

    if (tMin > maxDist) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];
            m_lastStats.objectsTested++;

            if (!filter.PassesFilter(prim.id, prim.layer)) continue;

            float t = prim.bounds.RayIntersect(ray.origin, ray.direction, maxDist);
            if (t >= 0.0f && t <= maxDist) {
                RayHit hit;
                hit.entityId = prim.id;
                hit.distance = t;
                hit.point = ray.GetPoint(t);
                results.push_back(hit);
            }
        }
    } else {
        // Order children by ray direction for better culling
        uint32_t first = node.leftFirst;
        uint32_t second = node.leftFirst + 1;

        float t1Min, t1Max, t2Min, t2Max;
        bool hit1 = m_nodes[first].bounds.IntersectsRay(ray.origin, invDir, t1Min, t1Max);
        bool hit2 = m_nodes[second].bounds.IntersectsRay(ray.origin, invDir, t2Min, t2Max);

        if (hit1 && hit2 && t2Min < t1Min) {
            std::swap(first, second);
        }

        if (hit1) QueryRayInternal(first, ray, invDir, maxDist, filter, results);
        if (hit2) QueryRayInternal(second, ray, invDir, maxDist, filter, results);
    }
}

uint64_t BVH::QueryNearest(const glm::vec3& point, float maxDist,
                           const SpatialQueryFilter& filter) {
    if (m_nodes.empty()) return 0;

    if (m_needsRebuild) {
        Rebuild();
    }

    uint64_t nearest = 0;
    float nearestDist2 = maxDist * maxDist;

    QueryNearestInternal(0, point, filter, nearest, nearestDist2);

    return nearest;
}

void BVH::QueryNearestInternal(uint32_t nodeIndex, const glm::vec3& point,
                              const SpatialQueryFilter& filter,
                              uint64_t& nearest, float& nearestDist2) const {
    const Node& node = m_nodes[nodeIndex];

    float nodeDist2 = node.bounds.DistanceSquared(point);
    if (nodeDist2 > nearestDist2) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];

            if (!filter.PassesFilter(prim.id, prim.layer)) continue;

            float dist2 = prim.bounds.DistanceSquared(point);
            if (dist2 < nearestDist2) {
                nearestDist2 = dist2;
                nearest = prim.id;
            }
        }
    } else {
        // Visit closer child first
        float dist1 = m_nodes[node.leftFirst].bounds.DistanceSquared(point);
        float dist2 = m_nodes[node.leftFirst + 1].bounds.DistanceSquared(point);

        if (dist1 < dist2) {
            QueryNearestInternal(node.leftFirst, point, filter, nearest, nearestDist2);
            QueryNearestInternal(node.leftFirst + 1, point, filter, nearest, nearestDist2);
        } else {
            QueryNearestInternal(node.leftFirst + 1, point, filter, nearest, nearestDist2);
            QueryNearestInternal(node.leftFirst, point, filter, nearest, nearestDist2);
        }
    }
}

std::vector<uint64_t> BVH::QueryKNearest(const glm::vec3& point, size_t k,
                                         float maxDist,
                                         const SpatialQueryFilter& filter) {
    // Simple implementation - query sphere and sort
    auto results = QuerySphere(point, maxDist, filter);

    // Sort by distance
    std::sort(results.begin(), results.end(),
        [this, &point](uint64_t a, uint64_t b) {
            float distA = m_primitives[m_idToIndex.at(a)].bounds.DistanceSquared(point);
            float distB = m_primitives[m_idToIndex.at(b)].bounds.DistanceSquared(point);
            return distA < distB;
        }
    );

    if (results.size() > k) {
        results.resize(k);
    }

    return results;
}

void BVH::QueryAABB(const AABB& query, const VisitorCallback& callback,
                   const SpatialQueryFilter& filter) {
    if (m_nodes.empty()) return;
    if (m_needsRebuild) Rebuild();

    QueryAABBCallback(0, query, filter, callback);
}

bool BVH::QueryAABBCallback(uint32_t nodeIndex, const AABB& query,
                           const SpatialQueryFilter& filter,
                           const VisitorCallback& callback) const {
    const Node& node = m_nodes[nodeIndex];

    if (!node.bounds.Intersects(query)) {
        return true;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];

            if (filter.PassesFilter(prim.id, prim.layer) && prim.bounds.Intersects(query)) {
                if (!callback(prim.id, prim.bounds)) {
                    return false;
                }
            }
        }
    } else {
        if (!QueryAABBCallback(node.leftFirst, query, filter, callback)) return false;
        if (!QueryAABBCallback(node.leftFirst + 1, query, filter, callback)) return false;
    }

    return true;
}

void BVH::QuerySphere(const glm::vec3& center, float radius,
                     const VisitorCallback& callback,
                     const SpatialQueryFilter& filter) {
    if (m_nodes.empty()) return;
    if (m_needsRebuild) Rebuild();

    QuerySphereCallback(0, center, radius, radius * radius, filter, callback);
}

bool BVH::QuerySphereCallback(uint32_t nodeIndex, const glm::vec3& center,
                             float radius, float radius2,
                             const SpatialQueryFilter& filter,
                             const VisitorCallback& callback) const {
    const Node& node = m_nodes[nodeIndex];

    if (!node.bounds.IntersectsSphere(center, radius)) {
        return true;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.count; ++i) {
            const Primitive& prim = m_primitives[m_primitiveIndices[node.leftFirst + i]];

            if (filter.PassesFilter(prim.id, prim.layer) &&
                prim.bounds.IntersectsSphere(center, radius)) {
                if (!callback(prim.id, prim.bounds)) {
                    return false;
                }
            }
        }
    } else {
        if (!QuerySphereCallback(node.leftFirst, center, radius, radius2, filter, callback)) return false;
        if (!QuerySphereCallback(node.leftFirst + 1, center, radius, radius2, filter, callback)) return false;
    }

    return true;
}

std::vector<std::vector<RayHit>> BVH::QueryRayBatch(std::span<const Ray> rays,
                                                    float maxDist,
                                                    const SpatialQueryFilter& filter) {
    std::vector<std::vector<RayHit>> results(rays.size());

    if (m_needsRebuild) {
        Rebuild();
    }

#ifdef __SSE__
    // Process 4 rays at a time
    size_t i = 0;
    for (; i + 4 <= rays.size(); i += 4) {
        QueryRay4(rays.data() + i, maxDist, filter, results.data() + i);
    }

    // Handle remaining rays
    for (; i < rays.size(); ++i) {
        results[i] = QueryRay(rays[i], maxDist, filter);
    }
#else
    for (size_t i = 0; i < rays.size(); ++i) {
        results[i] = QueryRay(rays[i], maxDist, filter);
    }
#endif

    return results;
}

#ifdef __SSE__
void BVH::BuildSIMDNodes() {
    // Build SoA layout for SIMD traversal
    size_t nodeCount = m_nodes.size();
    size_t packedCount = (nodeCount + 3) / 4;

    m_simdNodes.minX.resize(packedCount);
    m_simdNodes.minY.resize(packedCount);
    m_simdNodes.minZ.resize(packedCount);
    m_simdNodes.maxX.resize(packedCount);
    m_simdNodes.maxY.resize(packedCount);
    m_simdNodes.maxZ.resize(packedCount);
    m_simdNodes.children.resize(packedCount);

    for (size_t i = 0; i < packedCount; ++i) {
        float minX[4], minY[4], minZ[4];
        float maxX[4], maxY[4], maxZ[4];

        for (int j = 0; j < 4; ++j) {
            size_t nodeIdx = i * 4 + j;
            if (nodeIdx < nodeCount) {
                const Node& node = m_nodes[nodeIdx];
                minX[j] = node.bounds.min.x;
                minY[j] = node.bounds.min.y;
                minZ[j] = node.bounds.min.z;
                maxX[j] = node.bounds.max.x;
                maxY[j] = node.bounds.max.y;
                maxZ[j] = node.bounds.max.z;
                m_simdNodes.children[i][j] = node.leftFirst;
            } else {
                minX[j] = minY[j] = minZ[j] = std::numeric_limits<float>::max();
                maxX[j] = maxY[j] = maxZ[j] = std::numeric_limits<float>::lowest();
                m_simdNodes.children[i][j] = 0;
            }
        }

        m_simdNodes.minX[i] = _mm_loadu_ps(minX);
        m_simdNodes.minY[i] = _mm_loadu_ps(minY);
        m_simdNodes.minZ[i] = _mm_loadu_ps(minZ);
        m_simdNodes.maxX[i] = _mm_loadu_ps(maxX);
        m_simdNodes.maxY[i] = _mm_loadu_ps(maxY);
        m_simdNodes.maxZ[i] = _mm_loadu_ps(maxZ);
    }
}

void BVH::QueryRay4(const Ray* rays, float maxDist,
                   const SpatialQueryFilter& filter,
                   std::vector<RayHit>* results) {
    // Simplified SIMD ray query - processes 4 rays through tree
    for (int r = 0; r < 4; ++r) {
        results[r] = QueryRay(rays[r], maxDist, filter);
    }
}
#endif

// Builder implementations
BVH BVHBuilder::BuildFromTriangles(std::span<const Triangle> triangles,
                                   const BVH::Config& config) {
    BVH bvh(config);

    for (size_t i = 0; i < triangles.size(); ++i) {
        const Triangle& tri = triangles[i];
        AABB bounds;
        bounds.Expand(tri.vertices[0]);
        bounds.Expand(tri.vertices[1]);
        bounds.Expand(tri.vertices[2]);
        bvh.Insert(i, bounds, tri.materialId);
    }

    bvh.Build();
    return bvh;
}

BVH BVHBuilder::BuildFromAABBs(std::span<const std::pair<uint64_t, AABB>> objects,
                               const BVH::Config& config) {
    BVH bvh(config);

    for (const auto& [id, bounds] : objects) {
        bvh.Insert(id, bounds);
    }

    bvh.Build();
    return bvh;
}

} // namespace Nova
