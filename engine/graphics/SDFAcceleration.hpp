#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <array>

namespace Nova {

class SDFModel;
class Camera;

/**
 * @brief Axis-aligned bounding box
 */
struct AABB {
    glm::vec3 min{FLT_MAX};
    glm::vec3 max{-FLT_MAX};

    AABB() = default;
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    [[nodiscard]] glm::vec3 Center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 Extents() const { return (max - min) * 0.5f; }
    [[nodiscard]] glm::vec3 Size() const { return max - min; }

    [[nodiscard]] inline float SurfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    [[nodiscard]] inline float Volume() const {
        glm::vec3 d = max - min;
        return d.x * d.y * d.z;
    }

    [[nodiscard]] inline bool Contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] inline bool Intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    [[nodiscard]] inline bool IntersectsRay(const glm::vec3& origin, const glm::vec3& direction,
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

    inline void Expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    inline void Expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    [[nodiscard]] static inline AABB Union(const AABB& a, const AABB& b) {
        return AABB(glm::min(a.min, b.min), glm::max(a.max, b.max));
    }

    [[nodiscard]] static inline AABB Intersection(const AABB& a, const AABB& b) {
        return AABB(glm::max(a.min, b.min), glm::min(a.max, b.max));
    }

    [[nodiscard]] static inline AABB Transform(const AABB& aabb, const glm::mat4& transform) {
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
};

/**
 * @brief Ray structure for intersection tests
 */
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 invDirection;  // 1.0 / direction (for fast AABB tests)
    std::array<int, 3> sign;  // direction sign for AABB tests

    Ray(const glm::vec3& origin, const glm::vec3& direction);

    [[nodiscard]] glm::vec3 At(float t) const { return origin + direction * t; }
};

/**
 * @brief Frustum for camera culling
 */
struct Frustum {
    std::array<glm::vec4, 6> planes;  // left, right, bottom, top, near, far

    Frustum() = default;

    explicit inline Frustum(const glm::mat4& projectionView) {
        // Extract frustum planes from projection-view matrix
        planes[0] = glm::vec4(
            projectionView[0][3] + projectionView[0][0],
            projectionView[1][3] + projectionView[1][0],
            projectionView[2][3] + projectionView[2][0],
            projectionView[3][3] + projectionView[3][0]
        );

        planes[1] = glm::vec4(
            projectionView[0][3] - projectionView[0][0],
            projectionView[1][3] - projectionView[1][0],
            projectionView[2][3] - projectionView[2][0],
            projectionView[3][3] - projectionView[3][0]
        );

        planes[2] = glm::vec4(
            projectionView[0][3] + projectionView[0][1],
            projectionView[1][3] + projectionView[1][1],
            projectionView[2][3] + projectionView[2][1],
            projectionView[3][3] + projectionView[3][1]
        );

        planes[3] = glm::vec4(
            projectionView[0][3] - projectionView[0][1],
            projectionView[1][3] - projectionView[1][1],
            projectionView[2][3] - projectionView[2][1],
            projectionView[3][3] - projectionView[3][1]
        );

        planes[4] = glm::vec4(
            projectionView[0][3] + projectionView[0][2],
            projectionView[1][3] + projectionView[1][2],
            projectionView[2][3] + projectionView[2][2],
            projectionView[3][3] + projectionView[3][2]
        );

        planes[5] = glm::vec4(
            projectionView[0][3] - projectionView[0][2],
            projectionView[1][3] - projectionView[1][2],
            projectionView[2][3] - projectionView[2][2],
            projectionView[3][3] - projectionView[3][2]
        );

        for (auto& plane : planes) {
            float length = glm::length(glm::vec3(plane));
            plane /= length;
        }
    }

    [[nodiscard]] inline bool ContainsPoint(const glm::vec3& point) const {
        for (const auto& plane : planes) {
            if (glm::dot(glm::vec3(plane), point) + plane.w < 0.0f) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool IntersectsSphere(const glm::vec3& center, float radius) const {
        for (const auto& plane : planes) {
            float distance = glm::dot(glm::vec3(plane), center) + plane.w;
            if (distance < -radius) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool IntersectsAABB(const AABB& aabb) const {
        for (const auto& plane : planes) {
            glm::vec3 normal = glm::vec3(plane);

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
};

/**
 * @brief BVH node for GPU upload (std430 layout)
 */
struct alignas(16) SDFBVHNode {
    glm::vec3 aabbMin;
    float padding0;
    glm::vec3 aabbMax;
    float padding1;

    int leftChild;       // Index to left child (-1 if leaf)
    int rightChild;      // Index to right child (-1 if leaf)
    int primitiveStart;  // First primitive index (for leaves)
    int primitiveCount;  // Number of primitives (for leaves)

    [[nodiscard]] bool IsLeaf() const { return leftChild == -1; }
};

/**
 * @brief Build statistics for BVH construction
 */
struct BVHBuildStats {
    int nodeCount = 0;
    int leafCount = 0;
    int maxDepth = 0;
    float avgPrimitivesPerLeaf = 0.0f;
    double buildTimeMs = 0.0;
    size_t memoryBytes = 0;
};

/**
 * @brief SDF instance for acceleration structure
 */
struct SDFInstance {
    const SDFModel* model = nullptr;
    glm::mat4 transform{1.0f};
    glm::mat4 inverseTransform{1.0f};
    AABB worldBounds;
    int instanceId = -1;
    bool dynamic = false;  // Requires frequent updates
};

/**
 * @brief BVH construction strategy
 */
enum class BVHBuildStrategy {
    SAH,              // Surface Area Heuristic (best quality, slower build)
    Middle,           // Split at midpoint (fast, lower quality)
    EqualCounts,      // Equal number of primitives (balanced)
    HLBVH             // Hierarchical Linear BVH (very fast, good quality)
};

/**
 * @brief BVH construction settings
 */
struct BVHBuildSettings {
    BVHBuildStrategy strategy = BVHBuildStrategy::SAH;
    int maxPrimitivesPerLeaf = 4;
    int maxDepth = 32;
    int sahBuckets = 12;  // For SAH splitting
    bool parallelBuild = true;
    bool compactNodes = true;  // Remove unused nodes after build
};

/**
 * @brief Hierarchical Bounding Volume Hierarchy for SDF acceleration
 *
 * Features:
 * - Surface Area Heuristic (SAH) based construction
 * - GPU-friendly linear memory layout
 * - Support for dynamic objects with incremental updates
 * - Parallel construction using std::execution
 * - Frustum and ray culling
 */
class SDFAccelerationStructure {
public:
    SDFAccelerationStructure();
    ~SDFAccelerationStructure();

    // Non-copyable, movable
    SDFAccelerationStructure(const SDFAccelerationStructure&) = delete;
    SDFAccelerationStructure& operator=(const SDFAccelerationStructure&) = delete;
    SDFAccelerationStructure(SDFAccelerationStructure&&) noexcept = default;
    SDFAccelerationStructure& operator=(SDFAccelerationStructure&&) noexcept = default;

    // =========================================================================
    // Building
    // =========================================================================

    /**
     * @brief Build BVH from SDF instances
     */
    void Build(const std::vector<SDFInstance>& instances,
               const BVHBuildSettings& settings = {});

    /**
     * @brief Build BVH from single models with transforms
     */
    void Build(const std::vector<const SDFModel*>& models,
               const std::vector<glm::mat4>& transforms,
               const BVHBuildSettings& settings = {});

    /**
     * @brief Update dynamic instances (partial rebuild)
     */
    void UpdateDynamic(const std::vector<int>& instanceIds,
                       const std::vector<glm::mat4>& newTransforms);

    /**
     * @brief Refit BVH (faster than rebuild, but lower quality)
     * Use for moving objects when topology doesn't change
     */
    void Refit();

    /**
     * @brief Clear all data
     */
    void Clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Query instances visible in camera frustum
     * @return List of instance indices
     */
    [[nodiscard]] std::vector<int> QueryFrustum(const Frustum& frustum) const;

    /**
     * @brief Query instances intersecting ray
     * @return List of instance indices sorted by distance
     */
    [[nodiscard]] std::vector<int> QueryRay(const Ray& ray, float maxDistance = FLT_MAX) const;

    /**
     * @brief Query instances intersecting AABB
     */
    [[nodiscard]] std::vector<int> QueryAABB(const AABB& aabb) const;

    /**
     * @brief Query instances intersecting sphere
     */
    [[nodiscard]] std::vector<int> QuerySphere(const glm::vec3& center, float radius) const;

    /**
     * @brief Find closest instance to ray
     * @return Instance index or -1 if none found
     */
    [[nodiscard]] int FindClosestInstance(const Ray& ray, float& outDistance) const;

    // =========================================================================
    // GPU Synchronization
    // =========================================================================

    /**
     * @brief Upload BVH to GPU buffer
     * @return GPU buffer handle (SSBO)
     */
    unsigned int UploadToGPU();

    /**
     * @brief Get GPU buffer handle
     */
    [[nodiscard]] unsigned int GetGPUBuffer() const { return m_gpuBuffer; }

    /**
     * @brief Check if GPU buffer is valid
     */
    [[nodiscard]] bool IsGPUValid() const { return m_gpuValid; }

    /**
     * @brief Invalidate GPU buffer (requires re-upload)
     */
    void InvalidateGPU() { m_gpuValid = false; }

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get BVH nodes (CPU side)
     */
    [[nodiscard]] const std::vector<SDFBVHNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Get instances
     */
    [[nodiscard]] const std::vector<SDFInstance>& GetInstances() const { return m_instances; }

    /**
     * @brief Get build statistics
     */
    [[nodiscard]] const BVHBuildStats& GetStats() const { return m_stats; }

    /**
     * @brief Get build settings
     */
    [[nodiscard]] const BVHBuildSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Check if built
     */
    [[nodiscard]] bool IsBuilt() const { return !m_nodes.empty(); }

    /**
     * @brief Get memory usage in bytes
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

private:
    // Internal build structures
    struct BVHBuildNode {
        AABB bounds;
        int leftChild = -1;
        int rightChild = -1;
        int primitiveStart = -1;
        int primitiveCount = 0;
        int depth = 0;
    };

    struct BVHPrimitiveInfo {
        int primitiveIndex;
        AABB bounds;
        glm::vec3 centroid;
    };

    struct SAHBucket {
        int count = 0;
        AABB bounds;
    };

    // Build methods
    int RecursiveBuild(std::vector<BVHPrimitiveInfo>& primitives,
                       int start, int end, int depth,
                       std::vector<BVHBuildNode>& buildNodes);

    int BuildSAH(std::vector<BVHPrimitiveInfo>& primitives,
                 int start, int end, int depth,
                 std::vector<BVHBuildNode>& buildNodes);

    int BuildMiddle(std::vector<BVHPrimitiveInfo>& primitives,
                    int start, int end, int depth,
                    std::vector<BVHBuildNode>& buildNodes);

    int BuildEqualCounts(std::vector<BVHPrimitiveInfo>& primitives,
                         int start, int end, int depth,
                         std::vector<BVHBuildNode>& buildNodes);

    int BuildHLBVH(std::vector<BVHPrimitiveInfo>& primitives,
                   std::vector<BVHBuildNode>& buildNodes);

    // Flatten tree for GPU
    void FlattenBVHTree(const BVHBuildNode& node, int& offset,
                        const std::vector<BVHBuildNode>& buildNodes);

    // Refit helpers
    void RefitNode(int nodeIndex);
    AABB ComputeNodeBounds(int nodeIndex) const;

    // Query helpers
    void QueryFrustumRecursive(int nodeIndex, const Frustum& frustum,
                               std::vector<int>& results) const;

    void QueryRayRecursive(int nodeIndex, const Ray& ray, float maxDistance,
                           std::vector<std::pair<int, float>>& results) const;

    void QueryAABBRecursive(int nodeIndex, const AABB& aabb,
                            std::vector<int>& results) const;

    // Statistics
    void ComputeStats();
    void TraverseForStats(int nodeIndex, int depth);

    // Data
    std::vector<SDFBVHNode> m_nodes;
    std::vector<SDFInstance> m_instances;
    std::vector<int> m_primitiveIndices;  // Mapping from leaf primitives to instances

    BVHBuildSettings m_settings;
    BVHBuildStats m_stats;

    // GPU data
    unsigned int m_gpuBuffer = 0;
    bool m_gpuValid = false;

    // Bounds for entire structure
    AABB m_rootBounds;
};

/**
 * @brief Helper functions for BVH construction
 */
namespace BVHUtil {
    /**
     * @brief Compute tight AABB for SDF model with transform
     */
    [[nodiscard]] AABB ComputeSDFBounds(const SDFModel* model, const glm::mat4& transform);

    /**
     * @brief Compute centroid AABB (for SAH splitting)
     */
    [[nodiscard]] AABB ComputeCentroidBounds(const std::vector<SDFAccelerationStructure::BVHPrimitiveInfo>& primitives,
                                             int start, int end);

    /**
     * @brief Estimate SAH cost for a split
     */
    [[nodiscard]] float ComputeSAHCost(const AABB& leftBounds, int leftCount,
                                        const AABB& rightBounds, int rightCount,
                                        const AABB& totalBounds);

    /**
     * @brief Partition primitives for split
     */
    int PartitionPrimitives(std::vector<SDFAccelerationStructure::BVHPrimitiveInfo>& primitives,
                            int start, int end, int axis, float splitPos);

    /**
     * @brief Morton encode position for HLBVH
     */
    [[nodiscard]] uint32_t MortonEncode(const glm::vec3& position, const AABB& bounds);

    /**
     * @brief Compute optimal axis for split
     */
    [[nodiscard]] int ComputeSplitAxis(const AABB& bounds);
}

} // namespace Nova
