#include "SDFSparseOctree.hpp"
#include "../sdf/SDFModel.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Nova {

// =============================================================================
// SDFSparseVoxelOctree Implementation
// =============================================================================

SDFSparseVoxelOctree::SDFSparseVoxelOctree() = default;

SDFSparseVoxelOctree::~SDFSparseVoxelOctree() {
    if (m_gpuTexture) {
        glDeleteTextures(1, &m_gpuTexture);
    }
    if (m_gpuBuffer) {
        glDeleteBuffers(1, &m_gpuBuffer);
    }
}

void SDFSparseVoxelOctree::Voxelize(const SDFModel& model,
                                     const VoxelizationSettings& settings) {
    auto [minBounds, maxBounds] = model.GetBounds();

    auto sdfFunc = [&model](const glm::vec3& pos) {
        return model.EvaluateSDF(pos);
    };

    Voxelize(sdfFunc, minBounds, maxBounds, settings);
}

void SDFSparseVoxelOctree::Voxelize(const std::function<float(const glm::vec3&)>& sdfFunc,
                                     const glm::vec3& boundsMin,
                                     const glm::vec3& boundsMax,
                                     const VoxelizationSettings& settings) {
    auto startTime = std::chrono::high_resolution_clock::now();

    Clear();
    m_boundsMin = boundsMin;
    m_boundsMax = boundsMax;
    m_settings = settings;

    // Build octree
    std::vector<BuildNode> buildNodes;
    buildNodes.reserve(1024);

    BuildRecursive(sdfFunc, boundsMin, boundsMax, 0, buildNodes);

    // Flatten to linear array
    if (!buildNodes.empty()) {
        m_nodes.reserve(buildNodes.size());
        int offset = 0;
        FlattenOctree(buildNodes[0], offset, buildNodes);
    }

    ComputeStats();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.memoryBytes = GetMemoryUsage();

    InvalidateGPU();
}

int SDFSparseVoxelOctree::BuildRecursive(const std::function<float(const glm::vec3&)>& sdfFunc,
                                          const glm::vec3& boundsMin,
                                          const glm::vec3& boundsMax,
                                          int depth,
                                          std::vector<BuildNode>& buildNodes) {
    BuildNode node;
    node.boundsMin = boundsMin;
    node.boundsMax = boundsMax;
    node.depth = depth;

    // Evaluate SDF in this voxel
    EvaluateNode(sdfFunc, boundsMin, boundsMax, node.minDistance, node.maxDistance);

    // Check if we should subdivide
    bool shouldSubdivide = ShouldSubdivide(node.minDistance, node.maxDistance, depth);

    if (!shouldSubdivide || depth >= m_settings.maxDepth) {
        // Create leaf
        node.isLeaf = true;
        int nodeIndex = static_cast<int>(buildNodes.size());
        buildNodes.push_back(node);
        return nodeIndex;
    }

    // Subdivide into 8 children
    node.isLeaf = false;
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;

    for (int i = 0; i < 8; ++i) {
        glm::vec3 childMin, childMax;
        OctreeUtil::ComputeChildBounds(boundsMin, boundsMax, i, childMin, childMax);

        int childIndex = BuildRecursive(sdfFunc, childMin, childMax, depth + 1, buildNodes);
        node.children[i] = childIndex;
    }

    int nodeIndex = static_cast<int>(buildNodes.size());
    buildNodes.push_back(node);
    return nodeIndex;
}

void SDFSparseVoxelOctree::FlattenOctree(const BuildNode& node, int& offset,
                                          const std::vector<BuildNode>& buildNodes) {
    OctreeNode flatNode;
    flatNode.minDistance = node.minDistance;
    flatNode.maxDistance = node.maxDistance;
    flatNode.childMask = 0;
    flatNode.childIndex = 0;

    int myOffset = offset++;
    m_nodes.push_back(flatNode);

    if (!node.isLeaf) {
        // Count valid children
        int childCount = 0;
        for (int i = 0; i < 8; ++i) {
            if (node.children[i] >= 0) {
                childCount++;
                m_nodes[myOffset].SetChild(i, true);
            }
        }

        if (childCount > 0) {
            m_nodes[myOffset].childIndex = offset;

            // Flatten children
            for (int i = 0; i < 8; ++i) {
                if (node.children[i] >= 0) {
                    FlattenOctree(buildNodes[node.children[i]], offset, buildNodes);
                }
            }
        }
    }
}

void SDFSparseVoxelOctree::EvaluateNode(const std::function<float(const glm::vec3&)>& sdfFunc,
                                         const glm::vec3& boundsMin,
                                         const glm::vec3& boundsMax,
                                         float& minDist, float& maxDist) const {
    if (!m_settings.storeDistances) {
        minDist = 0.0f;
        maxDist = 0.0f;
        return;
    }

    // Sample at corners and center
    std::array<glm::vec3, 9> samplePoints = {
        glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z),
        (boundsMin + boundsMax) * 0.5f  // Center
    };

    minDist = FLT_MAX;
    maxDist = -FLT_MAX;

    for (const auto& point : samplePoints) {
        float dist = sdfFunc(point);
        minDist = std::min(minDist, dist);
        maxDist = std::max(maxDist, dist);
    }
}

bool SDFSparseVoxelOctree::ShouldSubdivide(float minDist, float maxDist, int depth) const {
    if (!m_settings.adaptiveDepth) {
        return true;
    }

    // Don't subdivide if entirely inside or outside
    if (minDist > m_settings.surfaceThickness) {
        return false;  // Far outside surface
    }

    if (maxDist < -m_settings.surfaceThickness) {
        return false;  // Far inside surface
    }

    // Subdivide if near surface
    return true;
}

void SDFSparseVoxelOctree::Clear() {
    m_nodes.clear();
    m_stats = {};
    InvalidateGPU();
}

int SDFSparseVoxelOctree::GetOccupancyAt(const glm::vec3& position) const {
    if (m_nodes.empty()) {
        return 0;
    }

    int nodeIndex = FindNodeAt(position);
    if (nodeIndex < 0) {
        return 0;  // Empty
    }

    const OctreeNode& node = m_nodes[nodeIndex];

    // Check distance range
    if (node.maxDistance < -m_settings.surfaceThickness) {
        return 2;  // Inside
    } else if (node.minDistance > m_settings.surfaceThickness) {
        return 0;  // Outside/empty
    } else {
        return 1;  // Surface
    }
}

float SDFSparseVoxelOctree::GetDistanceEstimate(const glm::vec3& position) const {
    if (m_nodes.empty()) {
        return FLT_MAX;
    }

    int nodeIndex = FindNodeAt(position);
    if (nodeIndex < 0) {
        return FLT_MAX;
    }

    const OctreeNode& node = m_nodes[nodeIndex];
    return (node.minDistance + node.maxDistance) * 0.5f;
}

float SDFSparseVoxelOctree::GetNextOccupiedVoxel(const glm::vec3& position,
                                                  const glm::vec3& direction,
                                                  float maxDistance) const {
    // Simplified implementation - march through voxels
    float t = 0.0f;
    float voxelSize = m_settings.voxelSize;
    glm::vec3 normDir = glm::normalize(direction);

    while (t < maxDistance) {
        glm::vec3 currentPos = position + normDir * t;

        // Check if out of bounds
        if (currentPos.x < m_boundsMin.x || currentPos.x > m_boundsMax.x ||
            currentPos.y < m_boundsMin.y || currentPos.y > m_boundsMax.y ||
            currentPos.z < m_boundsMin.z || currentPos.z > m_boundsMax.z) {
            return -1.0f;
        }

        int occupancy = GetOccupancyAt(currentPos);
        if (occupancy > 0) {
            return t;  // Found occupied voxel
        }

        // Skip to next voxel
        int depth = GetDepthAt(currentPos);
        float stepSize = GetVoxelSizeAtDepth(depth);
        t += stepSize;
    }

    return -1.0f;  // No occupied voxel found
}

OctreeRaymarchResult SDFSparseVoxelOctree::MarchRay(const glm::vec3& origin,
                                                     const glm::vec3& direction,
                                                     const std::function<float(const glm::vec3&)>& sdfFunc,
                                                     float maxDistance,
                                                     int maxSteps) const {
    OctreeRaymarchResult result;
    result.position = origin;
    result.distance = 0.0f;
    result.foundSurface = false;
    result.stepsSkipped = 0;

    glm::vec3 normDir = glm::normalize(direction);
    float t = 0.0f;
    int steps = 0;

    while (t < maxDistance && steps < maxSteps) {
        glm::vec3 currentPos = origin + normDir * t;
        result.position = currentPos;
        result.distance = t;

        // Check octree for empty space
        float nextOccupied = GetNextOccupiedVoxel(currentPos, normDir, maxDistance - t);

        if (nextOccupied > 0.0f && nextOccupied > m_settings.voxelSize * 2.0f) {
            // Skip empty space
            t += nextOccupied * 0.9f;  // Conservative skip
            result.stepsSkipped++;
            steps++;
            continue;
        }

        // Near surface or occupied - evaluate SDF
        float dist = sdfFunc(currentPos);

        if (dist < m_settings.surfaceThickness) {
            result.foundSurface = true;
            return result;
        }

        // Step forward
        t += std::max(dist, m_settings.voxelSize * 0.5f);
        steps++;
    }

    return result;
}

bool SDFSparseVoxelOctree::IsEmpty(const glm::vec3& position) const {
    return GetOccupancyAt(position) == 0;
}

bool SDFSparseVoxelOctree::IsNearSurface(const glm::vec3& position) const {
    return GetOccupancyAt(position) == 1;
}

int SDFSparseVoxelOctree::FindNodeAt(const glm::vec3& position) const {
    if (m_nodes.empty()) {
        return -1;
    }

    return FindNodeAtRecursive(0, position, m_boundsMin, m_boundsMax);
}

int SDFSparseVoxelOctree::FindNodeAtRecursive(int nodeIndex, const glm::vec3& position,
                                                const glm::vec3& nodeMin,
                                                const glm::vec3& nodeMax) const {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return -1;
    }

    const OctreeNode& node = m_nodes[nodeIndex];

    if (node.IsLeaf()) {
        return nodeIndex;
    }

    // Find child containing position
    glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
    int childIdx = ComputeChildIndex(position, center);

    if (!node.HasChild(childIdx)) {
        return nodeIndex;  // Child doesn't exist, return this node
    }

    // Compute child bounds
    glm::vec3 childMin, childMax;
    OctreeUtil::ComputeChildBounds(nodeMin, nodeMax, childIdx, childMin, childMax);

    // Get actual child node index
    int childNodeIdx = node.childIndex;
    for (int i = 0; i < childIdx; ++i) {
        if (node.HasChild(i)) {
            childNodeIdx++;
        }
    }

    return FindNodeAtRecursive(childNodeIdx, position, childMin, childMax);
}

int SDFSparseVoxelOctree::ComputeChildIndex(const glm::vec3& position,
                                             const glm::vec3& center) const {
    int index = 0;
    if (position.x >= center.x) index |= 1;
    if (position.y >= center.y) index |= 2;
    if (position.z >= center.z) index |= 4;
    return index;
}

float SDFSparseVoxelOctree::GetVoxelSizeAtDepth(int depth) const {
    return m_settings.voxelSize * std::pow(2.0f, m_settings.maxDepth - depth);
}

int SDFSparseVoxelOctree::GetDepthAt(const glm::vec3& position) const {
    // Approximate depth based on distance to nearest node
    int nodeIndex = FindNodeAt(position);
    if (nodeIndex < 0) {
        return 0;
    }

    // Traverse back up to find depth (simplified - would need parent pointers)
    return m_settings.maxDepth;  // Conservative estimate
}

unsigned int SDFSparseVoxelOctree::UploadToGPU() {
    if (m_nodes.empty()) {
        return 0;
    }

    // Create 3D texture with reasonable resolution
    int resolution = std::min(256, 1 << m_settings.maxDepth);
    m_gpuTextureResolution = resolution;

    std::vector<uint8_t> textureData;
    CreateDenseTexture(resolution, textureData);

    if (!m_gpuTexture) {
        glGenTextures(1, &m_gpuTexture);
    }

    glBindTexture(GL_TEXTURE_3D, m_gpuTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, resolution, resolution, resolution,
                 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    m_gpuValid = true;
    return m_gpuTexture;
}

unsigned int SDFSparseVoxelOctree::UploadToGPUBuffer() {
    if (m_nodes.empty()) {
        return 0;
    }

    if (!m_gpuBuffer) {
        glGenBuffers(1, &m_gpuBuffer);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_nodes.size() * sizeof(OctreeNode),
                 m_nodes.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_gpuValid = true;
    return m_gpuBuffer;
}

void SDFSparseVoxelOctree::CreateDenseTexture(int resolution,
                                               std::vector<uint8_t>& outData) const {
    outData.resize(resolution * resolution * resolution);
    std::fill(outData.begin(), outData.end(), 0);

    glm::vec3 boundsSize = m_boundsMax - m_boundsMin;
    glm::vec3 voxelSize = boundsSize / static_cast<float>(resolution);

    for (int z = 0; z < resolution; ++z) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                glm::vec3 pos = m_boundsMin + glm::vec3(x, y, z) * voxelSize;
                int occupancy = GetOccupancyAt(pos);

                int index = x + y * resolution + z * resolution * resolution;
                outData[index] = static_cast<uint8_t>(occupancy * 127);  // 0, 127, or 254
            }
        }
    }
}

std::vector<uint8_t> SDFSparseVoxelOctree::ExportDenseGrid(int resolution) const {
    std::vector<uint8_t> data;
    CreateDenseTexture(resolution, data);
    return data;
}

size_t SDFSparseVoxelOctree::GetMemoryUsage() const {
    return m_nodes.size() * sizeof(OctreeNode);
}

void SDFSparseVoxelOctree::ComputeStats() {
    m_stats = {};
    m_stats.nodeCount = static_cast<int>(m_nodes.size());

    if (!m_nodes.empty()) {
        TraverseForStats(0, 0);

        int totalVoxels = 1 << (m_settings.maxDepth * 3);  // 8^maxDepth
        m_stats.totalVoxels = totalVoxels;
        m_stats.sparsityRatio = static_cast<float>(m_stats.nodeCount) / static_cast<float>(totalVoxels);
    }
}

void SDFSparseVoxelOctree::TraverseForStats(int nodeIndex, int depth) {
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    const OctreeNode& node = m_nodes[nodeIndex];
    m_stats.maxDepth = std::max(m_stats.maxDepth, depth);

    if (node.IsLeaf()) {
        m_stats.leafCount++;
    } else {
        int childIdx = node.childIndex;
        for (int i = 0; i < 8; ++i) {
            if (node.HasChild(i)) {
                TraverseForStats(childIdx++, depth + 1);
            }
        }
    }
}

void SDFSparseVoxelOctree::Update(const std::function<float(const glm::vec3&)>& sdfFunc,
                                   const glm::vec3& modifiedMin,
                                   const glm::vec3& modifiedMax) {
    // Simplified incremental update - rebuild affected region
    // For production, would implement more sophisticated partial updates
    Voxelize(sdfFunc, m_boundsMin, m_boundsMax, m_settings);
}

// =============================================================================
// OctreeUtil Implementation
// =============================================================================

namespace OctreeUtil {

void ComputeChildBounds(const glm::vec3& parentMin, const glm::vec3& parentMax,
                        int childIndex, glm::vec3& childMin, glm::vec3& childMax) {
    glm::vec3 center = (parentMin + parentMax) * 0.5f;

    childMin = parentMin;
    childMax = center;

    if (childIndex & 1) {
        childMin.x = center.x;
        childMax.x = parentMax.x;
    }
    if (childIndex & 2) {
        childMin.y = center.y;
        childMax.y = parentMax.y;
    }
    if (childIndex & 4) {
        childMin.z = center.z;
        childMax.z = parentMax.z;
    }
}

void SampleVoxel(const std::function<float(const glm::vec3&)>& sdfFunc,
                 const glm::vec3& voxelMin, const glm::vec3& voxelMax,
                 int samplesPerAxis, float& outMinDist, float& outMaxDist) {
    outMinDist = FLT_MAX;
    outMaxDist = -FLT_MAX;

    glm::vec3 step = (voxelMax - voxelMin) / static_cast<float>(samplesPerAxis - 1);

    for (int z = 0; z < samplesPerAxis; ++z) {
        for (int y = 0; y < samplesPerAxis; ++y) {
            for (int x = 0; x < samplesPerAxis; ++x) {
                glm::vec3 pos = voxelMin + glm::vec3(x, y, z) * step;
                float dist = sdfFunc(pos);
                outMinDist = std::min(outMinDist, dist);
                outMaxDist = std::max(outMaxDist, dist);
            }
        }
    }
}

bool VoxelIntersectsSurface(float minDist, float maxDist, float threshold) {
    return minDist <= threshold && maxDist >= -threshold;
}

float EstimateOptimalVoxelSize(const SDFModel& model) {
    auto [minBounds, maxBounds] = model.GetBounds();
    glm::vec3 size = maxBounds - minBounds;
    float maxSize = std::max(std::max(size.x, size.y), size.z);

    // Aim for 64-128 voxels along longest axis
    return maxSize / 128.0f;
}

int ComputeDepthFromVoxelSize(float voxelSize, const glm::vec3& bounds) {
    float maxBound = std::max(std::max(bounds.x, bounds.y), bounds.z);
    return static_cast<int>(std::ceil(std::log2(maxBound / voxelSize)));
}

bool RayVoxelIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                          const glm::vec3& voxelMin, const glm::vec3& voxelMax,
                          float& tMin, float& tMax) {
    glm::vec3 invDir = 1.0f / rayDir;
    glm::vec3 t0 = (voxelMin - rayOrigin) * invDir;
    glm::vec3 t1 = (voxelMax - rayOrigin) * invDir;

    glm::vec3 tNear = glm::min(t0, t1);
    glm::vec3 tFar = glm::max(t0, t1);

    tMin = std::max(std::max(tNear.x, tNear.y), tNear.z);
    tMax = std::min(std::min(tFar.x, tFar.y), tFar.z);

    return tMax >= tMin && tMax >= 0.0f;
}

} // namespace OctreeUtil

} // namespace Nova
