#include "SDFBVH.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>
#include <queue>

namespace Nova {

// ============================================================================
// Construction
// ============================================================================

SDFBVH::SDFBVH(const SDFBVHConfig& config)
    : m_config(config)
{
}

void SDFBVH::Build(std::vector<SDFBVHPrimitive> primitives) {
    auto startTime = std::chrono::high_resolution_clock::now();

    Clear();

    if (primitives.empty()) {
        return;
    }

    m_primitives = std::move(primitives);

    // Initialize primitive indices
    m_primitiveIndices.resize(m_primitives.size());
    std::iota(m_primitiveIndices.begin(), m_primitiveIndices.end(), 0);

    // Pre-calculate centroids if not already set
    for (auto& prim : m_primitives) {
        if (prim.centroid == glm::vec3(0.0f) && prim.bounds.IsValid()) {
            prim.centroid = prim.bounds.GetCenter();
        }
    }

    // Allocate nodes (worst case: 2n-1 nodes for n primitives)
    m_nodes.reserve(2 * m_primitives.size());

    // Build recursively using SAH
    BuildRecursive(0, static_cast<uint32_t>(m_primitives.size()), 0);

    // Calculate statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.nodeCount = static_cast<uint32_t>(m_nodes.size());
    m_stats.totalPrimitives = static_cast<uint32_t>(m_primitives.size());
    m_stats.memoryBytes = GetMemoryUsage();
    m_stats.maxDepth = GetDepth();
    m_stats.sahCost = GetSAHCost();

    // Count leaves and average primitives per leaf
    uint32_t leafCount = 0;
    uint32_t totalLeafPrimitives = 0;
    for (const auto& node : m_nodes) {
        if (node.IsLeaf()) {
            leafCount++;
            totalLeafPrimitives += node.primitiveCount;
        }
    }
    m_stats.leafCount = leafCount;
    m_stats.avgPrimitivesPerLeaf = leafCount > 0 ?
        static_cast<float>(totalLeafPrimitives) / static_cast<float>(leafCount) : 0.0f;

    m_needsRebuild = false;
}

uint32_t SDFBVH::BuildRecursive(uint32_t begin, uint32_t end, uint32_t depth) {
    uint32_t nodeIndex = static_cast<uint32_t>(m_nodes.size());
    m_nodes.push_back(SDFBVHNode{});
    SDFBVHNode& node = m_nodes[nodeIndex];

    uint32_t primCount = end - begin;

    // Calculate bounds
    AABB bounds;
    AABB centroidBounds;
    for (uint32_t i = begin; i < end; ++i) {
        const SDFBVHPrimitive& prim = m_primitives[m_primitiveIndices[i]];
        bounds.Expand(prim.bounds);
        centroidBounds.Expand(prim.centroid);
    }
    node.bounds = bounds;

    // Create leaf if few primitives or max depth reached
    if (primCount <= m_config.maxPrimitivesPerLeaf || depth >= m_config.maxDepth) {
        node.leftFirst = begin;
        node.primitiveCount = primCount;
        node.rightChild = 0;
        return nodeIndex;
    }

    // Find best split using SAH
    SAHSplit split = FindBestSplit(begin, end, centroidBounds);

    // Check if split is worthwhile
    float leafCost = primCount * m_config.intersectionCost;
    if (!split.valid || split.cost >= leafCost) {
        // Create leaf - no beneficial split found
        node.leftFirst = begin;
        node.primitiveCount = primCount;
        node.rightChild = 0;
        return nodeIndex;
    }

    // Partition primitives
    uint32_t mid = begin;
    for (uint32_t i = begin; i < end; ++i) {
        const SDFBVHPrimitive& prim = m_primitives[m_primitiveIndices[i]];
        if (prim.centroid[split.axis] < split.position) {
            std::swap(m_primitiveIndices[i], m_primitiveIndices[mid]);
            ++mid;
        }
    }

    // Fallback if partition failed (all primitives on one side)
    if (mid == begin || mid == end) {
        // Use object median split instead
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
    node.primitiveCount = 0; // Internal node
    node.leftFirst = static_cast<uint32_t>(m_nodes.size());

    BuildRecursive(begin, mid, depth + 1);
    node.rightChild = static_cast<uint32_t>(m_nodes.size());
    BuildRecursive(mid, end, depth + 1);

    // Update node reference (may have been invalidated by vector growth)
    m_nodes[nodeIndex].leftFirst = node.leftFirst;
    m_nodes[nodeIndex].rightChild = node.rightChild;

    return nodeIndex;
}

SDFBVH::SAHSplit SDFBVH::FindBestSplit(uint32_t begin, uint32_t end, const AABB& centroidBounds) {
    if (m_config.useBinnedSAH) {
        return FindBestSplitBinned(begin, end, centroidBounds);
    } else {
        return FindBestSplitFull(begin, end);
    }
}

SDFBVH::SAHSplit SDFBVH::FindBestSplitBinned(uint32_t begin, uint32_t end, const AABB& centroidBounds) {
    SAHSplit best;
    uint32_t numBins = std::min(m_config.sahBuckets, 64u);

    std::vector<SAHBin> bins(numBins);

    for (int axis = 0; axis < 3; ++axis) {
        float axisMin = centroidBounds.min[axis];
        float axisMax = centroidBounds.max[axis];

        // Skip degenerate axis
        if (axisMax - axisMin < 1e-6f) {
            continue;
        }

        float scale = static_cast<float>(numBins) / (axisMax - axisMin);

        // Clear bins
        for (uint32_t i = 0; i < numBins; ++i) {
            bins[i].bounds = AABB();
            bins[i].count = 0;
        }

        // Populate bins
        for (uint32_t i = begin; i < end; ++i) {
            const SDFBVHPrimitive& prim = m_primitives[m_primitiveIndices[i]];
            uint32_t binIdx = std::min(
                static_cast<uint32_t>((prim.centroid[axis] - axisMin) * scale),
                numBins - 1
            );
            bins[binIdx].bounds.Expand(prim.bounds);
            bins[binIdx].count++;
        }

        // Compute prefix sums for left side
        std::vector<float> leftAreas(numBins);
        std::vector<uint32_t> leftCounts(numBins);
        AABB leftBounds;
        uint32_t leftCount = 0;

        for (uint32_t i = 0; i < numBins; ++i) {
            leftBounds.Expand(bins[i].bounds);
            leftCount += bins[i].count;
            leftAreas[i] = leftBounds.GetSurfaceArea();
            leftCounts[i] = leftCount;
        }

        // Compute suffix sums for right side and evaluate splits
        AABB rightBounds;
        uint32_t rightCount = 0;

        for (int i = static_cast<int>(numBins) - 1; i > 0; --i) {
            rightBounds.Expand(bins[i].bounds);
            rightCount += bins[i].count;

            // Skip if either side would be empty
            if (leftCounts[i - 1] == 0 || rightCount == 0) {
                continue;
            }

            float rightArea = rightBounds.GetSurfaceArea();
            float cost = m_config.traversalCost +
                leftAreas[i - 1] * leftCounts[i - 1] * m_config.intersectionCost +
                rightArea * rightCount * m_config.intersectionCost;

            if (cost < best.cost) {
                best.cost = cost;
                best.axis = axis;
                best.position = axisMin + static_cast<float>(i) * (axisMax - axisMin) / static_cast<float>(numBins);
                best.valid = true;
            }
        }
    }

    return best;
}

SDFBVH::SAHSplit SDFBVH::FindBestSplitFull(uint32_t begin, uint32_t end) {
    SAHSplit best;
    uint32_t primCount = end - begin;

    // For small primitive counts, use full SAH evaluation
    for (int axis = 0; axis < 3; ++axis) {
        // Sort by centroid on this axis
        std::sort(
            m_primitiveIndices.begin() + begin,
            m_primitiveIndices.begin() + end,
            [this, axis](uint32_t a, uint32_t b) {
                return m_primitives[a].centroid[axis] < m_primitives[b].centroid[axis];
            }
        );

        // Compute prefix areas from left
        std::vector<float> leftAreas(primCount);
        AABB leftBounds;
        for (uint32_t i = begin; i < end; ++i) {
            leftBounds.Expand(m_primitives[m_primitiveIndices[i]].bounds);
            leftAreas[i - begin] = leftBounds.GetSurfaceArea();
        }

        // Compute suffix areas from right and evaluate splits
        AABB rightBounds;
        for (uint32_t i = end; i > begin + 1; --i) {
            rightBounds.Expand(m_primitives[m_primitiveIndices[i - 1]].bounds);

            uint32_t leftCount = i - 1 - begin;
            uint32_t rightCount = end - i + 1;

            float cost = m_config.traversalCost +
                leftAreas[leftCount - 1] * leftCount * m_config.intersectionCost +
                rightBounds.GetSurfaceArea() * rightCount * m_config.intersectionCost;

            if (cost < best.cost) {
                best.cost = cost;
                best.axis = axis;
                // Split position between the two primitives
                best.position = (m_primitives[m_primitiveIndices[i - 2]].centroid[axis] +
                                m_primitives[m_primitiveIndices[i - 1]].centroid[axis]) * 0.5f;
                best.valid = true;
            }
        }
    }

    return best;
}

void SDFBVH::Rebuild() {
    if (m_primitives.empty()) {
        return;
    }

    // Store primitives and rebuild
    std::vector<SDFBVHPrimitive> primitives = std::move(m_primitives);
    Build(std::move(primitives));
}

void SDFBVH::Update() {
    if (m_nodes.empty()) {
        return;
    }

    // Refit from leaves up
    RefitRecursive(0);
    m_needsRebuild = false;
}

void SDFBVH::UpdatePrimitive(uint32_t primitiveIndex, const AABB& newBounds) {
    if (primitiveIndex >= m_primitives.size()) {
        return;
    }

    m_primitives[primitiveIndex].bounds = newBounds;
    m_primitives[primitiveIndex].centroid = newBounds.GetCenter();
    m_needsRebuild = true;
}

void SDFBVH::RefitRecursive(uint32_t nodeIndex) {
    SDFBVHNode& node = m_nodes[nodeIndex];

    if (node.IsLeaf()) {
        // Recalculate bounds from primitives
        node.bounds = AABB();
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            node.bounds.Expand(m_primitives[m_primitiveIndices[node.leftFirst + i]].bounds);
        }
    } else {
        // Refit children first
        RefitRecursive(node.leftFirst);
        RefitRecursive(node.rightChild);

        // Merge child bounds
        node.bounds = AABB::Merge(
            m_nodes[node.leftFirst].bounds,
            m_nodes[node.rightChild].bounds
        );
    }
}

void SDFBVH::Clear() {
    m_nodes.clear();
    m_primitives.clear();
    m_primitiveIndices.clear();
    m_stats = SDFBVHStats{};
    m_needsRebuild = false;
}

// ============================================================================
// Ray Traversal
// ============================================================================

SDFBVHTraversalResult SDFBVH::Traverse(const Ray& ray, float maxDist) const {
    SDFBVHTraversalResult result;

    if (m_nodes.empty()) {
        return result;
    }

    glm::vec3 invDir = ray.GetInverseDirection();
    TraverseInternal(0, ray, invDir, maxDist, result);

    return result;
}

void SDFBVH::TraverseInternal(
    uint32_t nodeIndex,
    const Ray& ray,
    const glm::vec3& invDir,
    float& maxDist,
    SDFBVHTraversalResult& result) const
{
    result.nodesVisited++;

    const SDFBVHNode& node = m_nodes[nodeIndex];

    // Test ray-AABB intersection
    float tMin, tMax;
    if (!node.bounds.IntersectsRay(ray.origin, invDir, tMin, tMax)) {
        return;
    }

    // Early out if intersection is beyond max distance
    if (tMin > maxDist) {
        return;
    }

    if (node.IsLeaf()) {
        // Add all primitives in this leaf
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            uint32_t primIdx = m_primitiveIndices[node.leftFirst + i];
            result.primitivesTested++;

            // Test against primitive bounds
            const AABB& primBounds = m_primitives[primIdx].bounds;
            float primTMin, primTMax;
            if (primBounds.IntersectsRay(ray.origin, invDir, primTMin, primTMax)) {
                if (primTMin <= maxDist) {
                    result.candidates.push_back(primIdx);
                    result.closestT = std::min(result.closestT, primTMin);
                }
            }
        }
    } else {
        // Order traversal by distance for better early termination
        float leftTMin, leftTMax, rightTMin, rightTMax;
        bool hitLeft = m_nodes[node.leftFirst].bounds.IntersectsRay(ray.origin, invDir, leftTMin, leftTMax);
        bool hitRight = m_nodes[node.rightChild].bounds.IntersectsRay(ray.origin, invDir, rightTMin, rightTMax);

        if (hitLeft && hitRight) {
            // Traverse closer child first
            if (leftTMin < rightTMin) {
                TraverseInternal(node.leftFirst, ray, invDir, maxDist, result);
                if (rightTMin <= maxDist) {
                    TraverseInternal(node.rightChild, ray, invDir, maxDist, result);
                }
            } else {
                TraverseInternal(node.rightChild, ray, invDir, maxDist, result);
                if (leftTMin <= maxDist) {
                    TraverseInternal(node.leftFirst, ray, invDir, maxDist, result);
                }
            }
        } else if (hitLeft) {
            TraverseInternal(node.leftFirst, ray, invDir, maxDist, result);
        } else if (hitRight) {
            TraverseInternal(node.rightChild, ray, invDir, maxDist, result);
        }
    }
}

uint32_t SDFBVH::Traverse(
    const Ray& ray,
    float maxDist,
    const std::function<bool(uint32_t primitiveIndex, float tMin, float tMax)>& callback) const
{
    if (m_nodes.empty()) {
        return 0;
    }

    glm::vec3 invDir = ray.GetInverseDirection();
    uint32_t count = 0;
    TraverseInternalCallback(0, ray, invDir, maxDist, callback, count);
    return count;
}

void SDFBVH::TraverseInternalCallback(
    uint32_t nodeIndex,
    const Ray& ray,
    const glm::vec3& invDir,
    float& maxDist,
    const std::function<bool(uint32_t, float, float)>& callback,
    uint32_t& count) const
{
    const SDFBVHNode& node = m_nodes[nodeIndex];

    float tMin, tMax;
    if (!node.bounds.IntersectsRay(ray.origin, invDir, tMin, tMax)) {
        return;
    }

    if (tMin > maxDist) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            uint32_t primIdx = m_primitiveIndices[node.leftFirst + i];
            const AABB& primBounds = m_primitives[primIdx].bounds;

            float primTMin, primTMax;
            if (primBounds.IntersectsRay(ray.origin, invDir, primTMin, primTMax)) {
                if (primTMin <= maxDist) {
                    count++;
                    if (!callback(primIdx, primTMin, primTMax)) {
                        return; // Early termination
                    }
                }
            }
        }
    } else {
        float leftTMin, leftTMax, rightTMin, rightTMax;
        bool hitLeft = m_nodes[node.leftFirst].bounds.IntersectsRay(ray.origin, invDir, leftTMin, leftTMax);
        bool hitRight = m_nodes[node.rightChild].bounds.IntersectsRay(ray.origin, invDir, rightTMin, rightTMax);

        if (hitLeft && hitRight) {
            if (leftTMin < rightTMin) {
                TraverseInternalCallback(node.leftFirst, ray, invDir, maxDist, callback, count);
                if (rightTMin <= maxDist) {
                    TraverseInternalCallback(node.rightChild, ray, invDir, maxDist, callback, count);
                }
            } else {
                TraverseInternalCallback(node.rightChild, ray, invDir, maxDist, callback, count);
                if (leftTMin <= maxDist) {
                    TraverseInternalCallback(node.leftFirst, ray, invDir, maxDist, callback, count);
                }
            }
        } else if (hitLeft) {
            TraverseInternalCallback(node.leftFirst, ray, invDir, maxDist, callback, count);
        } else if (hitRight) {
            TraverseInternalCallback(node.rightChild, ray, invDir, maxDist, callback, count);
        }
    }
}

SDFBVHTraversalResult SDFBVH::TraverseSorted(
    const Ray& ray,
    float maxDist,
    uint32_t maxCandidates) const
{
    SDFBVHTraversalResult result = Traverse(ray, maxDist);

    // Sort candidates by entry distance
    glm::vec3 invDir = ray.GetInverseDirection();
    std::sort(result.candidates.begin(), result.candidates.end(),
        [this, &ray, &invDir](uint32_t a, uint32_t b) {
            float tMinA, tMaxA, tMinB, tMaxB;
            m_primitives[a].bounds.IntersectsRay(ray.origin, invDir, tMinA, tMaxA);
            m_primitives[b].bounds.IntersectsRay(ray.origin, invDir, tMinB, tMaxB);
            return tMinA < tMinB;
        }
    );

    // Limit candidates
    if (result.candidates.size() > maxCandidates) {
        result.candidates.resize(maxCandidates);
    }

    return result;
}

// ============================================================================
// Point/Range Queries
// ============================================================================

SDFBVHQueryResult SDFBVH::Query(const glm::vec3& point, float radius) const {
    SDFBVHQueryResult result;

    if (m_nodes.empty()) {
        return result;
    }

    QueryInternal(0, point, radius, radius * radius, result);

    return result;
}

void SDFBVH::QueryInternal(
    uint32_t nodeIndex,
    const glm::vec3& point,
    float radius,
    float radius2,
    SDFBVHQueryResult& result) const
{
    result.nodesVisited++;

    const SDFBVHNode& node = m_nodes[nodeIndex];

    // Test sphere-AABB intersection
    if (!node.bounds.IntersectsSphere(point, radius)) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            uint32_t primIdx = m_primitiveIndices[node.leftFirst + i];
            const AABB& primBounds = m_primitives[primIdx].bounds;

            if (primBounds.IntersectsSphere(point, radius)) {
                result.primitives.push_back(primIdx);

                // Track nearest
                float dist2 = primBounds.DistanceSquared(point);
                if (dist2 < result.nearestDistance * result.nearestDistance) {
                    result.nearestDistance = std::sqrt(dist2);
                    result.nearestPrimitive = static_cast<int32_t>(primIdx);
                }
            }
        }
    } else {
        // Order by distance for better pruning
        float distLeft = m_nodes[node.leftFirst].bounds.DistanceSquared(point);
        float distRight = m_nodes[node.rightChild].bounds.DistanceSquared(point);

        if (distLeft < distRight) {
            QueryInternal(node.leftFirst, point, radius, radius2, result);
            QueryInternal(node.rightChild, point, radius, radius2, result);
        } else {
            QueryInternal(node.rightChild, point, radius, radius2, result);
            QueryInternal(node.leftFirst, point, radius, radius2, result);
        }
    }
}

void SDFBVH::Query(
    const glm::vec3& point,
    float radius,
    const std::function<void(uint32_t primitiveIndex, float distance)>& callback) const
{
    SDFBVHQueryResult result = Query(point, radius);

    for (uint32_t primIdx : result.primitives) {
        float dist = m_primitives[primIdx].bounds.Distance(point);
        callback(primIdx, dist);
    }
}

int32_t SDFBVH::QueryNearest(const glm::vec3& point, float maxDist) const {
    if (m_nodes.empty()) {
        return -1;
    }

    int32_t nearest = -1;
    float nearestDist2 = maxDist * maxDist;

    QueryNearestInternal(0, point, nearest, nearestDist2);

    return nearest;
}

void SDFBVH::QueryNearestInternal(
    uint32_t nodeIndex,
    const glm::vec3& point,
    int32_t& nearest,
    float& nearestDist2) const
{
    const SDFBVHNode& node = m_nodes[nodeIndex];

    // Prune if node is farther than current best
    float nodeDist2 = node.bounds.DistanceSquared(point);
    if (nodeDist2 > nearestDist2) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            uint32_t primIdx = m_primitiveIndices[node.leftFirst + i];
            float dist2 = m_primitives[primIdx].bounds.DistanceSquared(point);

            if (dist2 < nearestDist2) {
                nearestDist2 = dist2;
                nearest = static_cast<int32_t>(primIdx);
            }
        }
    } else {
        // Visit closer child first
        float distLeft = m_nodes[node.leftFirst].bounds.DistanceSquared(point);
        float distRight = m_nodes[node.rightChild].bounds.DistanceSquared(point);

        if (distLeft < distRight) {
            QueryNearestInternal(node.leftFirst, point, nearest, nearestDist2);
            QueryNearestInternal(node.rightChild, point, nearest, nearestDist2);
        } else {
            QueryNearestInternal(node.rightChild, point, nearest, nearestDist2);
            QueryNearestInternal(node.leftFirst, point, nearest, nearestDist2);
        }
    }
}

std::vector<uint32_t> SDFBVH::QueryKNearest(
    const glm::vec3& point,
    uint32_t k,
    float maxDist) const
{
    // Use a priority queue to track k nearest
    using PrimDist = std::pair<float, uint32_t>;
    std::priority_queue<PrimDist> heap;  // Max-heap by distance

    SDFBVHQueryResult result = Query(point, maxDist);

    for (uint32_t primIdx : result.primitives) {
        float dist = m_primitives[primIdx].bounds.Distance(point);

        if (heap.size() < k) {
            heap.push({dist, primIdx});
        } else if (dist < heap.top().first) {
            heap.pop();
            heap.push({dist, primIdx});
        }
    }

    // Extract results in sorted order
    std::vector<uint32_t> neighbors;
    neighbors.reserve(heap.size());
    while (!heap.empty()) {
        neighbors.push_back(heap.top().second);
        heap.pop();
    }

    // Reverse to get ascending order by distance
    std::reverse(neighbors.begin(), neighbors.end());

    return neighbors;
}

std::vector<uint32_t> SDFBVH::QueryAABB(const AABB& queryBounds) const {
    std::vector<uint32_t> result;

    if (m_nodes.empty()) {
        return result;
    }

    QueryAABBInternal(0, queryBounds, result);

    return result;
}

void SDFBVH::QueryAABBInternal(
    uint32_t nodeIndex,
    const AABB& queryBounds,
    std::vector<uint32_t>& result) const
{
    const SDFBVHNode& node = m_nodes[nodeIndex];

    if (!node.bounds.Intersects(queryBounds)) {
        return;
    }

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.primitiveCount; ++i) {
            uint32_t primIdx = m_primitiveIndices[node.leftFirst + i];
            if (m_primitives[primIdx].bounds.Intersects(queryBounds)) {
                result.push_back(primIdx);
            }
        }
    } else {
        QueryAABBInternal(node.leftFirst, queryBounds, result);
        QueryAABBInternal(node.rightChild, queryBounds, result);
    }
}

// ============================================================================
// Access / Statistics
// ============================================================================

uint32_t SDFBVH::GetDepth() const noexcept {
    if (m_nodes.empty()) {
        return 0;
    }
    return CalculateDepthRecursive(0);
}

uint32_t SDFBVH::CalculateDepthRecursive(uint32_t nodeIndex) const {
    const SDFBVHNode& node = m_nodes[nodeIndex];
    if (node.IsLeaf()) {
        return 1;
    }

    return 1 + std::max(
        CalculateDepthRecursive(node.leftFirst),
        CalculateDepthRecursive(node.rightChild)
    );
}

float SDFBVH::GetSAHCost() const noexcept {
    if (m_nodes.empty()) {
        return 0.0f;
    }

    float rootArea = m_nodes[0].bounds.GetSurfaceArea();
    if (rootArea <= 0.0f) {
        return 0.0f;
    }

    return CalculateSAHCostRecursive(0, rootArea);
}

float SDFBVH::CalculateSAHCostRecursive(uint32_t nodeIndex, float rootArea) const {
    const SDFBVHNode& node = m_nodes[nodeIndex];
    float nodeArea = node.bounds.GetSurfaceArea();
    float prob = nodeArea / rootArea;

    if (node.IsLeaf()) {
        return prob * node.primitiveCount * m_config.intersectionCost;
    }

    return prob * m_config.traversalCost +
           CalculateSAHCostRecursive(node.leftFirst, rootArea) +
           CalculateSAHCostRecursive(node.rightChild, rootArea);
}

size_t SDFBVH::GetMemoryUsage() const noexcept {
    return m_nodes.capacity() * sizeof(SDFBVHNode) +
           m_primitives.capacity() * sizeof(SDFBVHPrimitive) +
           m_primitiveIndices.capacity() * sizeof(uint32_t);
}

} // namespace Nova
