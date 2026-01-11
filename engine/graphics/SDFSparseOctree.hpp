#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <array>
#include <cstdint>

namespace Nova {

class SDFModel;

/**
 * @brief Octree voxel node (GPU-friendly format)
 */
struct alignas(16) OctreeNode {
    uint32_t childMask;      // Bitmask indicating which children exist (8 bits)
    uint32_t childIndex;     // Index to first child in node array
    float minDistance;       // Minimum distance in this node
    float maxDistance;       // Maximum distance in this node

    [[nodiscard]] bool HasChild(int childIdx) const {
        return (childMask & (1u << childIdx)) != 0;
    }

    void SetChild(int childIdx, bool value) {
        if (value) {
            childMask |= (1u << childIdx);
        } else {
            childMask &= ~(1u << childIdx);
        }
    }

    [[nodiscard]] bool IsLeaf() const {
        return childMask == 0;
    }

    [[nodiscard]] int GetChildCount() const {
        int count = 0;
        for (int i = 0; i < 8; ++i) {
            if (HasChild(i)) count++;
        }
        return count;
    }
};

/**
 * @brief Voxelization settings
 */
struct VoxelizationSettings {
    int maxDepth = 6;              // Maximum octree depth (6 = 64^3 resolution)
    float voxelSize = 0.1f;        // Finest voxel size
    float surfaceThickness = 0.05f; // Distance threshold for surface voxels
    bool adaptiveDepth = true;     // Stop subdividing empty/full voxels
    bool storeDistances = true;    // Store min/max distances per node
    bool compactStorage = true;    // Remove empty branches
};

/**
 * @brief Sparse Voxel Octree statistics
 */
struct OctreeStats {
    int nodeCount = 0;
    int leafCount = 0;
    int maxDepth = 0;
    int totalVoxels = 0;       // Total voxels if it were dense
    float sparsityRatio = 0.0f; // Ratio of sparse to dense storage
    size_t memoryBytes = 0;
    double buildTimeMs = 0.0;
};

/**
 * @brief Ray marching result through octree
 */
struct OctreeRaymarchResult {
    glm::vec3 position;        // Current position
    float distance;            // Distance traveled
    bool foundSurface;         // Hit surface
    int stepsSkipped;          // Number of empty voxels skipped
};

/**
 * @brief Sparse Voxel Octree for SDF acceleration
 *
 * Features:
 * - Hierarchical empty space skipping
 * - Adaptive subdivision based on distance field
 * - GPU texture upload (3D texture)
 * - Fast ray marching with large step sizes in empty regions
 * - Memory-efficient sparse storage
 */
class SDFSparseVoxelOctree {
public:
    SDFSparseVoxelOctree();
    ~SDFSparseVoxelOctree();

    // Non-copyable, movable
    SDFSparseVoxelOctree(const SDFSparseVoxelOctree&) = delete;
    SDFSparseVoxelOctree& operator=(const SDFSparseVoxelOctree&) = delete;
    SDFSparseVoxelOctree(SDFSparseVoxelOctree&&) noexcept = default;
    SDFSparseVoxelOctree& operator=(SDFSparseVoxelOctree&&) noexcept = default;

    // =========================================================================
    // Building
    // =========================================================================

    /**
     * @brief Voxelize SDF model into octree
     */
    void Voxelize(const SDFModel& model, const VoxelizationSettings& settings = {});

    /**
     * @brief Voxelize from SDF function
     */
    void Voxelize(const std::function<float(const glm::vec3&)>& sdfFunc,
                  const glm::vec3& boundsMin,
                  const glm::vec3& boundsMax,
                  const VoxelizationSettings& settings = {});

    /**
     * @brief Update octree for modified SDF (incremental)
     */
    void Update(const std::function<float(const glm::vec3&)>& sdfFunc,
                const glm::vec3& modifiedMin,
                const glm::vec3& modifiedMax);

    /**
     * @brief Clear all data
     */
    void Clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Get occupancy at position (0 = empty, 1 = surface, 2 = inside)
     */
    [[nodiscard]] int GetOccupancyAt(const glm::vec3& position) const;

    /**
     * @brief Get distance estimate at position (from stored min/max)
     */
    [[nodiscard]] float GetDistanceEstimate(const glm::vec3& position) const;

    /**
     * @brief Get next occupied voxel along ray (for ray marching)
     * @return Distance to next occupied voxel, or -1 if none found
     */
    [[nodiscard]] float GetNextOccupiedVoxel(const glm::vec3& position,
                                              const glm::vec3& direction,
                                              float maxDistance) const;

    /**
     * @brief March ray through octree with acceleration
     */
    [[nodiscard]] OctreeRaymarchResult MarchRay(const glm::vec3& origin,
                                                 const glm::vec3& direction,
                                                 const std::function<float(const glm::vec3&)>& sdfFunc,
                                                 float maxDistance,
                                                 int maxSteps) const;

    /**
     * @brief Check if position is in empty space
     */
    [[nodiscard]] bool IsEmpty(const glm::vec3& position) const;

    /**
     * @brief Check if position is near surface
     */
    [[nodiscard]] bool IsNearSurface(const glm::vec3& position) const;

    // =========================================================================
    // GPU Synchronization
    // =========================================================================

    /**
     * @brief Upload octree to GPU as 3D texture
     * Creates a 3D texture where each voxel stores occupancy info
     */
    unsigned int UploadToGPU();

    /**
     * @brief Upload as buffer (for deep octrees)
     */
    unsigned int UploadToGPUBuffer();

    /**
     * @brief Get GPU texture handle
     */
    [[nodiscard]] unsigned int GetGPUTexture() const { return m_gpuTexture; }

    /**
     * @brief Get GPU buffer handle
     */
    [[nodiscard]] unsigned int GetGPUBuffer() const { return m_gpuBuffer; }

    /**
     * @brief Check if GPU data is valid
     */
    [[nodiscard]] bool IsGPUValid() const { return m_gpuValid; }

    /**
     * @brief Invalidate GPU data
     */
    void InvalidateGPU() { m_gpuValid = false; }

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get octree nodes
     */
    [[nodiscard]] const std::vector<OctreeNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Get bounds
     */
    [[nodiscard]] const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
    [[nodiscard]] const glm::vec3& GetBoundsMax() const { return m_boundsMax; }
    [[nodiscard]] glm::vec3 GetBoundsSize() const { return m_boundsMax - m_boundsMin; }

    /**
     * @brief Get settings
     */
    [[nodiscard]] const VoxelizationSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Get statistics
     */
    [[nodiscard]] const OctreeStats& GetStats() const { return m_stats; }

    /**
     * @brief Check if built
     */
    [[nodiscard]] bool IsBuilt() const { return !m_nodes.empty(); }

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Export octree to dense voxel grid (for visualization)
     */
    [[nodiscard]] std::vector<uint8_t> ExportDenseGrid(int resolution) const;

    /**
     * @brief Get voxel size at depth
     */
    [[nodiscard]] float GetVoxelSizeAtDepth(int depth) const;

    /**
     * @brief Get octree depth at position
     */
    [[nodiscard]] int GetDepthAt(const glm::vec3& position) const;

private:
    // Build structures
    struct BuildNode {
        glm::vec3 boundsMin;
        glm::vec3 boundsMax;
        int depth;
        std::array<int, 8> children;  // -1 if no child
        float minDistance;
        float maxDistance;
        bool isLeaf;

        BuildNode() : depth(0), children{-1, -1, -1, -1, -1, -1, -1, -1},
                      minDistance(FLT_MAX), maxDistance(-FLT_MAX), isLeaf(true) {}
    };

    // Build helpers
    int BuildRecursive(const std::function<float(const glm::vec3&)>& sdfFunc,
                       const glm::vec3& boundsMin,
                       const glm::vec3& boundsMax,
                       int depth,
                       std::vector<BuildNode>& buildNodes);

    void FlattenOctree(const BuildNode& node, int& offset,
                       const std::vector<BuildNode>& buildNodes);

    // Query helpers
    int FindNodeAt(const glm::vec3& position) const;
    int FindNodeAtRecursive(int nodeIndex, const glm::vec3& position,
                            const glm::vec3& nodeMin, const glm::vec3& nodeMax) const;

    glm::vec3 WorldToLocal(const glm::vec3& worldPos) const;
    glm::vec3 LocalToWorld(const glm::vec3& localPos) const;

    int ComputeChildIndex(const glm::vec3& position, const glm::vec3& center) const;

    // Evaluation
    void EvaluateNode(const std::function<float(const glm::vec3&)>& sdfFunc,
                      const glm::vec3& boundsMin,
                      const glm::vec3& boundsMax,
                      float& minDist, float& maxDist) const;

    bool ShouldSubdivide(float minDist, float maxDist, int depth) const;

    // Dense grid helpers
    void CreateDenseTexture(int resolution, std::vector<uint8_t>& outData) const;

    // Statistics
    void ComputeStats();
    void TraverseForStats(int nodeIndex, int depth);

    // Data
    std::vector<OctreeNode> m_nodes;
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{1.0f};

    VoxelizationSettings m_settings;
    OctreeStats m_stats;

    // GPU data
    unsigned int m_gpuTexture = 0;
    unsigned int m_gpuBuffer = 0;
    bool m_gpuValid = false;
    int m_gpuTextureResolution = 0;
};

/**
 * @brief Utility functions for octree operations
 */
namespace OctreeUtil {
    /**
     * @brief Compute child bounds
     */
    void ComputeChildBounds(const glm::vec3& parentMin, const glm::vec3& parentMax,
                            int childIndex, glm::vec3& childMin, glm::vec3& childMax);

    /**
     * @brief Sample SDF in voxel (multiple points)
     */
    void SampleVoxel(const std::function<float(const glm::vec3&)>& sdfFunc,
                     const glm::vec3& voxelMin, const glm::vec3& voxelMax,
                     int samplesPerAxis, float& outMinDist, float& outMaxDist);

    /**
     * @brief Check if voxel intersects surface
     */
    [[nodiscard]] bool VoxelIntersectsSurface(float minDist, float maxDist, float threshold);

    /**
     * @brief Estimate optimal voxel size for SDF
     */
    [[nodiscard]] float EstimateOptimalVoxelSize(const SDFModel& model);

    /**
     * @brief Compute octree depth from voxel size
     */
    [[nodiscard]] int ComputeDepthFromVoxelSize(float voxelSize, const glm::vec3& bounds);

    /**
     * @brief Ray-voxel intersection
     */
    [[nodiscard]] bool RayVoxelIntersection(const glm::vec3& rayOrigin,
                                             const glm::vec3& rayDir,
                                             const glm::vec3& voxelMin,
                                             const glm::vec3& voxelMax,
                                             float& tMin, float& tMax);
}

} // namespace Nova
