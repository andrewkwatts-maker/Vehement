#include "SDFAcceleration.hpp"
#include "../sdf/SDFModel.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <execution>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>

namespace Nova {

// =============================================================================
// AABB Implementation
// =============================================================================

float AABB::SurfaceArea() const {
    glm::vec3 d = max - min;
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}

float AABB::Volume() const {
    glm::vec3 d = max - min;
    return d.x * d.y * d.z;
}

bool AABB::Contains(const glm::vec3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool AABB::Intersects(const AABB& other) const {
    return (min.x <= other.max.x && max.x >= other.min.x) &&
           (min.y <= other.max.y && max.y >= other.min.y) &&
           (min.z <= other.max.z && max.z >= other.min.z);
}

bool AABB::IntersectsRay(const glm::vec3& origin, const glm::vec3& direction,
                          float& tMin, float& tMax) const {
    glm::vec3 invDir = 1.0f / direction;
    glm::vec3 t0 = (min - origin) * invDir;
    glm::vec3 t1 = (max - origin) * invDir;

    glm::vec3 tNear = glm::min(t0, t1);
    glm::vec3 tFar = glm::max(t0, t1);

    tMin = glm::max(glm::max(tNear.x, tNear.y), tNear.z);
    tMax = glm::min(glm::min(tFar.x, tFar.y), tFar.z);

    return tMax >= tMin && tMax >= 0.0f;
}

void AABB::Expand(const glm::vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void AABB::Expand(const AABB& other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

AABB AABB::Union(const AABB& a, const AABB& b) {
    return AABB(glm::min(a.min, b.min), glm::max(a.max, b.max));
}

AABB AABB::Intersection(const AABB& a, const AABB& b) {
    return AABB(glm::max(a.min, b.min), glm::min(a.max, b.max));
}

AABB AABB::Transform(const AABB& aabb, const glm::mat4& transform) {
    // Transform all 8 corners and compute new AABB
    std::array<glm::vec3, 8> corners = {
        glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
        glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)
    };

    AABB result;
    result.min = glm::vec3(FLT_MAX);
    result.max = glm::vec3(-FLT_MAX);

    for (const auto& corner : corners) {
        glm::vec3 transformed = glm::vec3(transform * glm::vec4(corner, 1.0f));
        result.Expand(transformed);
    }

    return result;
}

// =============================================================================
// Ray Implementation
// =============================================================================

Ray::Ray(const glm::vec3& origin, const glm::vec3& direction)
    : origin(origin), direction(glm::normalize(direction)) {
    invDirection = 1.0f / this->direction;
    sign[0] = (invDirection.x < 0) ? 1 : 0;
    sign[1] = (invDirection.y < 0) ? 1 : 0;
    sign[2] = (invDirection.z < 0) ? 1 : 0;
}

// =============================================================================
// Frustum Implementation
// =============================================================================

Frustum::Frustum(const glm::mat4& projectionView) {
    // Extract frustum planes from projection-view matrix
    // Left plane
    planes[0] = glm::vec4(
        projectionView[0][3] + projectionView[0][0],
        projectionView[1][3] + projectionView[1][0],
        projectionView[2][3] + projectionView[2][0],
        projectionView[3][3] + projectionView[3][0]
    );

    // Right plane
    planes[1] = glm::vec4(
        projectionView[0][3] - projectionView[0][0],
        projectionView[1][3] - projectionView[1][0],
        projectionView[2][3] - projectionView[2][0],
        projectionView[3][3] - projectionView[3][0]
    );

    // Bottom plane
    planes[2] = glm::vec4(
        projectionView[0][3] + projectionView[0][1],
        projectionView[1][3] + projectionView[1][1],
        projectionView[2][3] + projectionView[2][1],
        projectionView[3][3] + projectionView[3][1]
    );

    // Top plane
    planes[3] = glm::vec4(
        projectionView[0][3] - projectionView[0][1],
        projectionView[1][3] - projectionView[1][1],
        projectionView[2][3] - projectionView[2][1],
        projectionView[3][3] - projectionView[3][1]
    );

    // Near plane
    planes[4] = glm::vec4(
        projectionView[0][3] + projectionView[0][2],
        projectionView[1][3] + projectionView[1][2],
        projectionView[2][3] + projectionView[2][2],
        projectionView[3][3] + projectionView[3][2]
    );

    // Far plane
    planes[5] = glm::vec4(
        projectionView[0][3] - projectionView[0][2],
        projectionView[1][3] - projectionView[1][2],
        projectionView[2][3] - projectionView[2][2],
        projectionView[3][3] - projectionView[3][2]
    );

    // Normalize planes
    for (auto& plane : planes) {
        float length = glm::length(glm::vec3(plane));
        plane /= length;
    }
}

bool Frustum::ContainsPoint(const glm::vec3& point) const {
    for (const auto& plane : planes) {
        if (glm::dot(glm::vec3(plane), point) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::IntersectsSphere(const glm::vec3& center, float radius) const {
    for (const auto& plane : planes) {
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::IntersectsAABB(const AABB& aabb) const {
    for (const auto& plane : planes) {
        glm::vec3 normal = glm::vec3(plane);

        // Find positive vertex (furthest in plane direction)
        glm::vec3 pVertex = aabb.min;
        if (normal.x >= 0) pVertex.x = aabb.max.x;
        if (normal.y >= 0) pVertex.y = aabb.max.y;
        if (normal.z >= 0) pVertex.z = aabb.max.z;

        if (glm::dot(normal, pVertex) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// SDFAccelerationStructure Implementation
// =============================================================================

SDFAccelerationStructure::SDFAccelerationStructure() = default;

SDFAccelerationStructure::~SDFAccelerationStructure() {
    if (m_gpuBuffer) {
        glDeleteBuffers(1, &m_gpuBuffer);
    }
}

void SDFAccelerationStructure::Build(const std::vector<SDFInstance>& instances,
                                      const BVHBuildSettings& settings) {
    auto startTime = std::chrono::high_resolution_clock::now();

    Clear();
    m_instances = instances;
    m_settings = settings;

    if (instances.empty()) {
        return;
    }

    // Create primitive info
    std::vector<BVHPrimitiveInfo> primitiveInfo;
    primitiveInfo.reserve(instances.size());

    for (size_t i = 0; i < instances.size(); ++i) {
        BVHPrimitiveInfo info;
        info.primitiveIndex = static_cast<int>(i);
        info.bounds = instances[i].worldBounds;
        info.centroid = info.bounds.Center();
        primitiveInfo.push_back(info);
    }

    // Build BVH
    std::vector<BVHBuildNode> buildNodes;
    buildNodes.reserve(instances.size() * 2);

    int rootIndex;
    switch (settings.strategy) {
        case BVHBuildStrategy::SAH:
            rootIndex = RecursiveBuild(primitiveInfo, 0, primitiveInfo.size(), 0, buildNodes);
            break;
        case BVHBuildStrategy::Middle:
            rootIndex = BuildMiddle(primitiveInfo, 0, primitiveInfo.size(), 0, buildNodes);
            break;
        case BVHBuildStrategy::EqualCounts:
            rootIndex = BuildEqualCounts(primitiveInfo, 0, primitiveInfo.size(), 0, buildNodes);
            break;
        case BVHBuildStrategy::HLBVH:
            rootIndex = BuildHLBVH(primitiveInfo, buildNodes);
            break;
        default:
            rootIndex = RecursiveBuild(primitiveInfo, 0, primitiveInfo.size(), 0, buildNodes);
            break;
    }

    // Flatten to linear array
    m_nodes.reserve(buildNodes.size());
    int offset = 0;
    FlattenBVHTree(buildNodes[rootIndex], offset, buildNodes);

    if (rootIndex >= 0 && rootIndex < buildNodes.size()) {
        m_rootBounds = buildNodes[rootIndex].bounds;
    }

    // Compute statistics
    ComputeStats();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.memoryBytes = GetMemoryUsage();

    InvalidateGPU();
}

void SDFAccelerationStructure::Build(const std::vector<const SDFModel*>& models,
                                      const std::vector<glm::mat4>& transforms,
                                      const BVHBuildSettings& settings) {
    if (models.size() != transforms.size()) {
        return;
    }

    std::vector<SDFInstance> instances;
    instances.reserve(models.size());

    for (size_t i = 0; i < models.size(); ++i) {
        if (!models[i]) continue;

        SDFInstance instance;
        instance.model = models[i];
        instance.transform = transforms[i];
        instance.inverseTransform = glm::inverse(transforms[i]);
        instance.worldBounds = BVHUtil::ComputeSDFBounds(models[i], transforms[i]);
        instance.instanceId = static_cast<int>(i);
        instances.push_back(instance);
    }

    Build(instances, settings);
}

int SDFAccelerationStructure::RecursiveBuild(std::vector<BVHPrimitiveInfo>& primitives,
                                               int start, int end, int depth,
                                               std::vector<BVHBuildNode>& buildNodes) {
    // Delegate to SAH by default
    return BuildSAH(primitives, start, end, depth, buildNodes);
}

int SDFAccelerationStructure::BuildSAH(std::vector<BVHPrimitiveInfo>& primitives,
                                        int start, int end, int depth,
                                        std::vector<BVHBuildNode>& buildNodes) {
    BVHBuildNode node;
    node.depth = depth;

    // Compute bounds of all primitives
    AABB bounds;
    for (int i = start; i < end; ++i) {
        bounds.Expand(primitives[i].bounds);
    }
    node.bounds = bounds;

    int numPrimitives = end - start;

    // Create leaf if criteria met
    if (numPrimitives <= m_settings.maxPrimitivesPerLeaf || depth >= m_settings.maxDepth) {
        node.primitiveStart = start;
        node.primitiveCount = numPrimitives;
        int nodeIndex = static_cast<int>(buildNodes.size());
        buildNodes.push_back(node);
        return nodeIndex;
    }

    // Compute centroid bounds
    AABB centroidBounds;
    for (int i = start; i < end; ++i) {
        centroidBounds.Expand(primitives[i].centroid);
    }

    // Choose split axis
    int axis = BVHUtil::ComputeSplitAxis(centroidBounds);

    // If all centroids are the same, create leaf
    if (centroidBounds.max[axis] == centroidBounds.min[axis]) {
        node.primitiveStart = start;
        node.primitiveCount = numPrimitives;
        int nodeIndex = static_cast<int>(buildNodes.size());
        buildNodes.push_back(node);
        return nodeIndex;
    }

    // SAH split
    constexpr int numBuckets = 12;
    std::array<SAHBucket, numBuckets> buckets;

    // Assign primitives to buckets
    for (int i = start; i < end; ++i) {
        int bucket = numBuckets * (primitives[i].centroid[axis] - centroidBounds.min[axis]) /
                     (centroidBounds.max[axis] - centroidBounds.min[axis]);
        bucket = std::clamp(bucket, 0, numBuckets - 1);
        buckets[bucket].count++;
        buckets[bucket].bounds.Expand(primitives[i].bounds);
    }

    // Compute costs for splitting after each bucket
    std::array<float, numBuckets - 1> costs;
    for (int i = 0; i < numBuckets - 1; ++i) {
        AABB leftBounds, rightBounds;
        int leftCount = 0, rightCount = 0;

        for (int j = 0; j <= i; ++j) {
            leftBounds.Expand(buckets[j].bounds);
            leftCount += buckets[j].count;
        }

        for (int j = i + 1; j < numBuckets; ++j) {
            rightBounds.Expand(buckets[j].bounds);
            rightCount += buckets[j].count;
        }

        costs[i] = BVHUtil::ComputeSAHCost(leftBounds, leftCount,
                                            rightBounds, rightCount,
                                            bounds);
    }

    // Find bucket with minimum cost
    int minCostBucket = 0;
    float minCost = costs[0];
    for (int i = 1; i < numBuckets - 1; ++i) {
        if (costs[i] < minCost) {
            minCost = costs[i];
            minCostBucket = i;
        }
    }

    // Partition primitives
    auto midIter = std::partition(primitives.begin() + start, primitives.begin() + end,
        [=](const BVHPrimitiveInfo& pi) {
            int bucket = numBuckets * (pi.centroid[axis] - centroidBounds.min[axis]) /
                         (centroidBounds.max[axis] - centroidBounds.min[axis]);
            bucket = std::clamp(bucket, 0, numBuckets - 1);
            return bucket <= minCostBucket;
        });

    int mid = static_cast<int>(midIter - primitives.begin());

    // Recursively build children
    node.leftChild = BuildSAH(primitives, start, mid, depth + 1, buildNodes);
    node.rightChild = BuildSAH(primitives, mid, end, depth + 1, buildNodes);

    int nodeIndex = static_cast<int>(buildNodes.size());
    buildNodes.push_back(node);
    return nodeIndex;
}

int SDFAccelerationStructure::BuildMiddle(std::vector<BVHPrimitiveInfo>& primitives,
                                           int start, int end, int depth,
                                           std::vector<BVHBuildNode>& buildNodes) {
    BVHBuildNode node;
    node.depth = depth;

    // Compute bounds
    AABB bounds;
    for (int i = start; i < end; ++i) {
        bounds.Expand(primitives[i].bounds);
    }
    node.bounds = bounds;

    int numPrimitives = end - start;

    // Create leaf if criteria met
    if (numPrimitives <= m_settings.maxPrimitivesPerLeaf || depth >= m_settings.maxDepth) {
        node.primitiveStart = start;
        node.primitiveCount = numPrimitives;
        int nodeIndex = static_cast<int>(buildNodes.size());
        buildNodes.push_back(node);
        return nodeIndex;
    }

    // Choose split axis (longest)
    int axis = BVHUtil::ComputeSplitAxis(bounds);
    float splitPos = (bounds.min[axis] + bounds.max[axis]) * 0.5f;

    // Partition
    auto midIter = std::partition(primitives.begin() + start, primitives.begin() + end,
        [axis, splitPos](const BVHPrimitiveInfo& pi) {
            return pi.centroid[axis] < splitPos;
        });

    int mid = static_cast<int>(midIter - primitives.begin());

    // Handle degenerate case
    if (mid == start || mid == end) {
        mid = (start + end) / 2;
    }

    // Recursively build children
    node.leftChild = BuildMiddle(primitives, start, mid, depth + 1, buildNodes);
    node.rightChild = BuildMiddle(primitives, mid, end, depth + 1, buildNodes);

    int nodeIndex = static_cast<int>(buildNodes.size());
    buildNodes.push_back(node);
    return nodeIndex;
}

int SDFAccelerationStructure::BuildEqualCounts(std::vector<BVHPrimitiveInfo>& primitives,
                                                int start, int end, int depth,
                                                std::vector<BVHBuildNode>& buildNodes) {
    BVHBuildNode node;
    node.depth = depth;

    // Compute bounds
    AABB bounds;
    for (int i = start; i < end; ++i) {
        bounds.Expand(primitives[i].bounds);
    }
    node.bounds = bounds;

    int numPrimitives = end - start;

    // Create leaf if criteria met
    if (numPrimitives <= m_settings.maxPrimitivesPerLeaf || depth >= m_settings.maxDepth) {
        node.primitiveStart = start;
        node.primitiveCount = numPrimitives;
        int nodeIndex = static_cast<int>(buildNodes.size());
        buildNodes.push_back(node);
        return nodeIndex;
    }

    // Choose split axis
    int axis = BVHUtil::ComputeSplitAxis(bounds);

    // Sort along axis
    std::sort(primitives.begin() + start, primitives.begin() + end,
        [axis](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
            return a.centroid[axis] < b.centroid[axis];
        });

    // Split in middle
    int mid = (start + end) / 2;

    // Recursively build children
    node.leftChild = BuildEqualCounts(primitives, start, mid, depth + 1, buildNodes);
    node.rightChild = BuildEqualCounts(primitives, mid, end, depth + 1, buildNodes);

    int nodeIndex = static_cast<int>(buildNodes.size());
    buildNodes.push_back(node);
    return nodeIndex;
}

int SDFAccelerationStructure::BuildHLBVH(std::vector<BVHPrimitiveInfo>& primitives,
                                          std::vector<BVHBuildNode>& buildNodes) {
    // Simplified HLBVH - sort by Morton code then build
    AABB bounds;
    for (const auto& prim : primitives) {
        bounds.Expand(prim.centroid);
    }

    // Compute Morton codes
    std::vector<std::pair<uint32_t, int>> mortonPrims;
    mortonPrims.reserve(primitives.size());

    for (size_t i = 0; i < primitives.size(); ++i) {
        uint32_t code = BVHUtil::MortonEncode(primitives[i].centroid, bounds);
        mortonPrims.push_back({code, static_cast<int>(i)});
    }

    // Sort by Morton code
    if (m_settings.parallelBuild) {
        std::sort(std::execution::par, mortonPrims.begin(), mortonPrims.end());
    } else {
        std::sort(mortonPrims.begin(), mortonPrims.end());
    }

    // Reorder primitives
    std::vector<BVHPrimitiveInfo> orderedPrimitives;
    orderedPrimitives.reserve(primitives.size());
    for (const auto& [code, idx] : mortonPrims) {
        orderedPrimitives.push_back(primitives[idx]);
    }
    primitives = std::move(orderedPrimitives);

    // Build tree with sorted primitives
    return BuildEqualCounts(primitives, 0, primitives.size(), 0, buildNodes);
}

void SDFAccelerationStructure::FlattenBVHTree(const BVHBuildNode& node, int& offset,
                                               const std::vector<BVHBuildNode>& buildNodes) {
    SDFBVHNode flatNode;
    flatNode.aabbMin = node.bounds.min;
    flatNode.aabbMax = node.bounds.max;

    int myOffset = offset++;
    m_nodes.push_back(flatNode);

    if (node.primitiveCount > 0) {
        // Leaf node
        m_nodes[myOffset].leftChild = -1;
        m_nodes[myOffset].rightChild = -1;
        m_nodes[myOffset].primitiveStart = static_cast<int>(m_primitiveIndices.size());
        m_nodes[myOffset].primitiveCount = node.primitiveCount;

        // Copy primitive indices
        for (int i = 0; i < node.primitiveCount; ++i) {
            m_primitiveIndices.push_back(node.primitiveStart + i);
        }
    } else {
        // Internal node
        FlattenBVHTree(buildNodes[node.leftChild], offset, buildNodes);
        int leftChildOffset = myOffset + 1;

        m_nodes[myOffset].leftChild = leftChildOffset;

        FlattenBVHTree(buildNodes[node.rightChild], offset, buildNodes);
        int rightChildOffset = offset - 1;

        m_nodes[myOffset].rightChild = rightChildOffset;
        m_nodes[myOffset].primitiveStart = -1;
        m_nodes[myOffset].primitiveCount = 0;
    }
}

void SDFAccelerationStructure::UpdateDynamic(const std::vector<int>& instanceIds,
                                              const std::vector<glm::mat4>& newTransforms) {
    if (instanceIds.size() != newTransforms.size()) {
        return;
    }

    // Update instance transforms and bounds
    for (size_t i = 0; i < instanceIds.size(); ++i) {
        int id = instanceIds[i];
        if (id >= 0 && id < m_instances.size()) {
            m_instances[id].transform = newTransforms[i];
            m_instances[id].inverseTransform = glm::inverse(newTransforms[i]);
            m_instances[id].worldBounds = BVHUtil::ComputeSDFBounds(
                m_instances[id].model, newTransforms[i]);
        }
    }

    // Refit BVH
    Refit();
}

void SDFAccelerationStructure::Refit() {
    if (m_nodes.empty()) return;

    // Refit from leaves to root
    for (int i = static_cast<int>(m_nodes.size()) - 1; i >= 0; --i) {
        RefitNode(i);
    }

    if (!m_nodes.empty()) {
        m_rootBounds.min = m_nodes[0].aabbMin;
        m_rootBounds.max = m_nodes[0].aabbMax;
    }

    InvalidateGPU();
}

void SDFAccelerationStructure::RefitNode(int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    SDFBVHNode& node = m_nodes[nodeIndex];

    if (node.IsLeaf()) {
        // Recompute bounds from primitives
        AABB bounds;
        for (int i = 0; i < node.primitiveCount; ++i) {
            int primIdx = m_primitiveIndices[node.primitiveStart + i];
            if (primIdx >= 0 && primIdx < m_instances.size()) {
                bounds.Expand(m_instances[primIdx].worldBounds);
            }
        }
        node.aabbMin = bounds.min;
        node.aabbMax = bounds.max;
    } else {
        // Recompute bounds from children
        AABB bounds;
        if (node.leftChild >= 0 && node.leftChild < m_nodes.size()) {
            bounds.Expand(AABB(m_nodes[node.leftChild].aabbMin, m_nodes[node.leftChild].aabbMax));
        }
        if (node.rightChild >= 0 && node.rightChild < m_nodes.size()) {
            bounds.Expand(AABB(m_nodes[node.rightChild].aabbMin, m_nodes[node.rightChild].aabbMax));
        }
        node.aabbMin = bounds.min;
        node.aabbMax = bounds.max;
    }
}

AABB SDFAccelerationStructure::ComputeNodeBounds(int nodeIndex) const {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return AABB();
    }

    return AABB(m_nodes[nodeIndex].aabbMin, m_nodes[nodeIndex].aabbMax);
}

void SDFAccelerationStructure::Clear() {
    m_nodes.clear();
    m_instances.clear();
    m_primitiveIndices.clear();
    m_stats = {};
    m_rootBounds = AABB();
    InvalidateGPU();
}

std::vector<int> SDFAccelerationStructure::QueryFrustum(const Frustum& frustum) const {
    std::vector<int> results;
    if (m_nodes.empty()) {
        return results;
    }

    QueryFrustumRecursive(0, frustum, results);
    return results;
}

void SDFAccelerationStructure::QueryFrustumRecursive(int nodeIndex, const Frustum& frustum,
                                                       std::vector<int>& results) const {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    const SDFBVHNode& node = m_nodes[nodeIndex];
    AABB nodeBounds(node.aabbMin, node.aabbMax);

    if (!frustum.IntersectsAABB(nodeBounds)) {
        return;
    }

    if (node.IsLeaf()) {
        // Add all primitives in leaf
        for (int i = 0; i < node.primitiveCount; ++i) {
            int primIdx = m_primitiveIndices[node.primitiveStart + i];
            results.push_back(m_instances[primIdx].instanceId);
        }
    } else {
        QueryFrustumRecursive(node.leftChild, frustum, results);
        QueryFrustumRecursive(node.rightChild, frustum, results);
    }
}

std::vector<int> SDFAccelerationStructure::QueryRay(const Ray& ray, float maxDistance) const {
    std::vector<std::pair<int, float>> results;
    if (m_nodes.empty()) {
        return {};
    }

    QueryRayRecursive(0, ray, maxDistance, results);

    // Sort by distance
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Extract instance IDs
    std::vector<int> instanceIds;
    instanceIds.reserve(results.size());
    for (const auto& [id, dist] : results) {
        instanceIds.push_back(id);
    }

    return instanceIds;
}

void SDFAccelerationStructure::QueryRayRecursive(int nodeIndex, const Ray& ray, float maxDistance,
                                                   std::vector<std::pair<int, float>>& results) const {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    const SDFBVHNode& node = m_nodes[nodeIndex];
    AABB nodeBounds(node.aabbMin, node.aabbMax);

    float tMin, tMax;
    if (!nodeBounds.IntersectsRay(ray.origin, ray.direction, tMin, tMax)) {
        return;
    }

    if (tMin > maxDistance) {
        return;
    }

    if (node.IsLeaf()) {
        // Add all primitives in leaf
        for (int i = 0; i < node.primitiveCount; ++i) {
            int primIdx = m_primitiveIndices[node.primitiveStart + i];
            const SDFInstance& inst = m_instances[primIdx];

            // Check AABB intersection for distance
            float instTMin, instTMax;
            if (inst.worldBounds.IntersectsRay(ray.origin, ray.direction, instTMin, instTMax)) {
                results.push_back({inst.instanceId, instTMin});
            }
        }
    } else {
        QueryRayRecursive(node.leftChild, ray, maxDistance, results);
        QueryRayRecursive(node.rightChild, ray, maxDistance, results);
    }
}

std::vector<int> SDFAccelerationStructure::QueryAABB(const AABB& aabb) const {
    std::vector<int> results;
    if (m_nodes.empty()) {
        return results;
    }

    QueryAABBRecursive(0, aabb, results);
    return results;
}

void SDFAccelerationStructure::QueryAABBRecursive(int nodeIndex, const AABB& aabb,
                                                    std::vector<int>& results) const {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    const SDFBVHNode& node = m_nodes[nodeIndex];
    AABB nodeBounds(node.aabbMin, node.aabbMax);

    if (!nodeBounds.Intersects(aabb)) {
        return;
    }

    if (node.IsLeaf()) {
        for (int i = 0; i < node.primitiveCount; ++i) {
            int primIdx = m_primitiveIndices[node.primitiveStart + i];
            if (m_instances[primIdx].worldBounds.Intersects(aabb)) {
                results.push_back(m_instances[primIdx].instanceId);
            }
        }
    } else {
        QueryAABBRecursive(node.leftChild, aabb, results);
        QueryAABBRecursive(node.rightChild, aabb, results);
    }
}

std::vector<int> SDFAccelerationStructure::QuerySphere(const glm::vec3& center, float radius) const {
    // Convert to AABB query (conservative)
    AABB sphereBounds(center - glm::vec3(radius), center + glm::vec3(radius));
    return QueryAABB(sphereBounds);
}

int SDFAccelerationStructure::FindClosestInstance(const Ray& ray, float& outDistance) const {
    auto results = QueryRay(ray, FLT_MAX);
    if (results.empty()) {
        return -1;
    }

    // First result is closest due to sorting
    int closestId = results[0];

    // Find actual distance
    for (const auto& inst : m_instances) {
        if (inst.instanceId == closestId) {
            float tMin, tMax;
            inst.worldBounds.IntersectsRay(ray.origin, ray.direction, tMin, tMax);
            outDistance = tMin;
            return closestId;
        }
    }

    return -1;
}

unsigned int SDFAccelerationStructure::UploadToGPU() {
    if (m_nodes.empty()) {
        return 0;
    }

    if (!m_gpuBuffer) {
        glGenBuffers(1, &m_gpuBuffer);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_nodes.size() * sizeof(SDFBVHNode),
                 m_nodes.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_gpuValid = true;
    return m_gpuBuffer;
}

size_t SDFAccelerationStructure::GetMemoryUsage() const {
    size_t total = 0;
    total += m_nodes.size() * sizeof(SDFBVHNode);
    total += m_instances.size() * sizeof(SDFInstance);
    total += m_primitiveIndices.size() * sizeof(int);
    return total;
}

void SDFAccelerationStructure::ComputeStats() {
    m_stats = {};
    m_stats.nodeCount = static_cast<int>(m_nodes.size());

    if (!m_nodes.empty()) {
        TraverseForStats(0, 0);

        if (m_stats.leafCount > 0) {
            m_stats.avgPrimitivesPerLeaf = static_cast<float>(m_instances.size()) /
                                            static_cast<float>(m_stats.leafCount);
        }
    }
}

void SDFAccelerationStructure::TraverseForStats(int nodeIndex, int depth) {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    const SDFBVHNode& node = m_nodes[nodeIndex];
    m_stats.maxDepth = std::max(m_stats.maxDepth, depth);

    if (node.IsLeaf()) {
        m_stats.leafCount++;
    } else {
        TraverseForStats(node.leftChild, depth + 1);
        TraverseForStats(node.rightChild, depth + 1);
    }
}

// =============================================================================
// BVHUtil Implementation
// =============================================================================

namespace BVHUtil {

AABB ComputeSDFBounds(const SDFModel* model, const glm::mat4& transform) {
    if (!model) {
        return AABB();
    }

    // Get model's local bounds
    auto [localMin, localMax] = model->GetBounds();
    AABB localBounds(localMin, localMax);

    // Transform to world space
    return AABB::Transform(localBounds, transform);
}

AABB ComputeCentroidBounds(const std::vector<SDFAccelerationStructure::BVHPrimitiveInfo>& primitives,
                            int start, int end) {
    AABB bounds;
    for (int i = start; i < end; ++i) {
        bounds.Expand(primitives[i].centroid);
    }
    return bounds;
}

float ComputeSAHCost(const AABB& leftBounds, int leftCount,
                      const AABB& rightBounds, int rightCount,
                      const AABB& totalBounds) {
    float leftArea = leftBounds.SurfaceArea();
    float rightArea = rightBounds.SurfaceArea();
    float totalArea = totalBounds.SurfaceArea();

    constexpr float traversalCost = 1.0f;
    constexpr float intersectionCost = 1.0f;

    return traversalCost +
           intersectionCost * (leftArea / totalArea * leftCount +
                               rightArea / totalArea * rightCount);
}

int PartitionPrimitives(std::vector<SDFAccelerationStructure::BVHPrimitiveInfo>& primitives,
                        int start, int end, int axis, float splitPos) {
    auto midIter = std::partition(primitives.begin() + start, primitives.begin() + end,
        [axis, splitPos](const SDFAccelerationStructure::BVHPrimitiveInfo& pi) {
            return pi.centroid[axis] < splitPos;
        });

    return static_cast<int>(midIter - primitives.begin());
}

uint32_t MortonEncode(const glm::vec3& position, const AABB& bounds) {
    // Normalize position to [0, 1]
    glm::vec3 normalized = (position - bounds.min) / (bounds.max - bounds.min);
    normalized = glm::clamp(normalized, 0.0f, 1.0f);

    // Scale to 10-bit integer
    uint32_t x = static_cast<uint32_t>(normalized.x * 1023.0f);
    uint32_t y = static_cast<uint32_t>(normalized.y * 1023.0f);
    uint32_t z = static_cast<uint32_t>(normalized.z * 1023.0f);

    // Interleave bits
    auto expandBits = [](uint32_t v) -> uint32_t {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    };

    return (expandBits(x) << 2) | (expandBits(y) << 1) | expandBits(z);
}

int ComputeSplitAxis(const AABB& bounds) {
    glm::vec3 size = bounds.Size();
    if (size.x > size.y && size.x > size.z) {
        return 0;
    } else if (size.y > size.z) {
        return 1;
    } else {
        return 2;
    }
}

} // namespace BVHUtil

} // namespace Nova
