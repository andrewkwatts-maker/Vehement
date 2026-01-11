#pragma once

/**
 * @file SDFBVH.hpp
 * @brief Bounding Volume Hierarchy optimized for SDF primitive queries
 *
 * This BVH implementation is specifically designed for accelerating SDF
 * (Signed Distance Field) queries during raymarching and spatial lookups.
 * It uses Surface Area Heuristic (SAH) for optimal tree construction and
 * provides cache-efficient traversal with a flat array layout.
 *
 * Key features:
 * - SAH-based tree construction for optimal ray traversal
 * - AABB node bounds with tight SDF primitive encapsulation
 * - Fast ray-BVH traversal for raymarching acceleration
 * - Nearest primitive query for distance field evaluation
 * - Range queries for finding all primitives within a distance
 * - Dynamic updates via refit (fast) or rebuild (accurate)
 *
 * @section sdfbvh_usage Usage Example
 *
 * @code{.cpp}
 * Nova::SDFBVH bvh;
 *
 * // Build from primitives
 * std::vector<Nova::SDFBVHPrimitive> primitives;
 * primitives.push_back({0, bounds0, center0, &sdfPrim0});
 * primitives.push_back({1, bounds1, center1, &sdfPrim1});
 * bvh.Build(primitives);
 *
 * // Ray traversal for raymarching
 * Nova::Ray ray{origin, direction};
 * auto hitInfo = bvh.Traverse(ray, 100.0f);
 * for (const auto& candidate : hitInfo.candidates) {
 *     // Evaluate SDF for these primitives
 * }
 *
 * // Point query for nearest primitive
 * auto nearby = bvh.Query(point, 5.0f);
 * @endcode
 *
 * @see SDFPrimitive for the SDF primitive types
 * @see BVH for the generic BVH implementation
 */

#include "AABB.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <limits>
#include <span>
#include <functional>

namespace Nova {

// Forward declarations
class SDFPrimitive;

/**
 * @brief Primitive data stored in the SDF BVH
 */
struct SDFBVHPrimitive {
    uint32_t id = 0;                    ///< Unique primitive identifier
    AABB bounds;                         ///< World-space bounding box
    glm::vec3 centroid{0.0f};           ///< Centroid for SAH partitioning
    const SDFPrimitive* primitive = nullptr; ///< Pointer to the actual SDF primitive
    uint32_t userData = 0;               ///< User-defined data (e.g., layer, flags)
};

/**
 * @brief BVH node structure for SDF acceleration
 *
 * Uses a cache-efficient flat array layout where internal nodes store
 * child indices and leaf nodes store primitive ranges.
 */
struct alignas(32) SDFBVHNode {
    AABB bounds;                         ///< Node bounding box
    uint32_t leftFirst = 0;              ///< Left child index OR first primitive index
    uint32_t primitiveCount = 0;         ///< 0 = internal node, >0 = leaf with count primitives
    uint32_t rightChild = 0;             ///< Right child index (only for internal nodes)
    uint32_t padding = 0;                ///< Padding for 32-byte alignment

    /**
     * @brief Check if this is a leaf node
     */
    [[nodiscard]] bool IsLeaf() const noexcept { return primitiveCount > 0; }

    /**
     * @brief Get the left child index (for internal nodes)
     */
    [[nodiscard]] uint32_t GetLeftChild() const noexcept { return leftFirst; }

    /**
     * @brief Get the right child index (for internal nodes)
     */
    [[nodiscard]] uint32_t GetRightChild() const noexcept { return rightChild; }

    /**
     * @brief Get the first primitive index (for leaf nodes)
     */
    [[nodiscard]] uint32_t GetFirstPrimitive() const noexcept { return leftFirst; }

    /**
     * @brief Get the primitive count (for leaf nodes)
     */
    [[nodiscard]] uint32_t GetPrimitiveCount() const noexcept { return primitiveCount; }
};

/**
 * @brief Result of ray traversal through the BVH
 */
struct SDFBVHTraversalResult {
    /// Indices of primitives that the ray potentially intersects
    std::vector<uint32_t> candidates;

    /// Number of nodes visited during traversal
    uint32_t nodesVisited = 0;

    /// Number of primitives tested
    uint32_t primitivesTested = 0;

    /// Closest t parameter found (for early termination)
    float closestT = std::numeric_limits<float>::max();
};

/**
 * @brief Result of a point/range query
 */
struct SDFBVHQueryResult {
    /// Indices of primitives within query range
    std::vector<uint32_t> primitives;

    /// Number of nodes visited
    uint32_t nodesVisited = 0;

    /// Closest primitive index (-1 if none found)
    int32_t nearestPrimitive = -1;

    /// Distance to nearest primitive
    float nearestDistance = std::numeric_limits<float>::max();
};

/**
 * @brief Configuration for BVH construction
 */
struct SDFBVHConfig {
    /// Maximum primitives per leaf node
    uint32_t maxPrimitivesPerLeaf = 4;

    /// Maximum tree depth
    uint32_t maxDepth = 64;

    /// Number of SAH buckets for binned construction
    uint32_t sahBuckets = 16;

    /// Cost of traversing a node (relative to intersection)
    float traversalCost = 1.0f;

    /// Cost of intersecting a primitive
    float intersectionCost = 1.0f;

    /// Whether to use binned SAH (faster) or full SAH (more accurate)
    bool useBinnedSAH = true;

    /// Threshold for switching from SAH to object median split
    uint32_t sahThreshold = 4;
};

/**
 * @brief Build statistics
 */
struct SDFBVHStats {
    uint32_t nodeCount = 0;
    uint32_t leafCount = 0;
    uint32_t maxDepth = 0;
    uint32_t totalPrimitives = 0;
    float avgPrimitivesPerLeaf = 0.0f;
    float sahCost = 0.0f;
    double buildTimeMs = 0.0;
    size_t memoryBytes = 0;
};

/**
 * @brief Bounding Volume Hierarchy for SDF primitive acceleration
 *
 * This class provides an optimized spatial acceleration structure for
 * SDF primitives, enabling fast raymarching and distance queries.
 *
 * The BVH uses a flat array layout for cache efficiency and supports
 * both static construction (SAH-based) and dynamic updates (refit/rebuild).
 *
 * @section threading Thread Safety
 * - Build/Rebuild/Update operations are NOT thread-safe
 * - Query operations (Traverse, Query) are thread-safe for concurrent reads
 * - Use external synchronization for mixed read/write access
 */
class SDFBVH {
public:
    /**
     * @brief Default constructor
     */
    SDFBVH() = default;

    /**
     * @brief Constructor with configuration
     */
    explicit SDFBVH(const SDFBVHConfig& config);

    /**
     * @brief Destructor
     */
    ~SDFBVH() = default;

    // Non-copyable, movable
    SDFBVH(const SDFBVH&) = delete;
    SDFBVH& operator=(const SDFBVH&) = delete;
    SDFBVH(SDFBVH&&) noexcept = default;
    SDFBVH& operator=(SDFBVH&&) noexcept = default;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Build BVH from a list of primitives
     *
     * Uses SAH (Surface Area Heuristic) for optimal tree construction.
     * This is the primary build method for static scenes.
     *
     * @param primitives Vector of SDF primitives with bounds
     */
    void Build(std::vector<SDFBVHPrimitive> primitives);

    /**
     * @brief Build BVH from primitives with custom bounds calculator
     *
     * @tparam T Primitive type
     * @tparam BoundsFunc Function type: AABB(const T&)
     * @param primitives Span of primitives
     * @param boundsFunc Function to compute bounds for each primitive
     */
    template<typename T, typename BoundsFunc>
    void Build(std::span<const T> primitives, BoundsFunc&& boundsFunc);

    /**
     * @brief Rebuild the entire BVH structure
     *
     * Full reconstruction using SAH. Use when primitive positions have
     * changed significantly or tree quality has degraded.
     */
    void Rebuild();

    /**
     * @brief Update (refit) BVH bounds without restructuring
     *
     * Fast update that only recalculates node bounds from leaves up.
     * Use when primitives have moved slightly but topology is stable.
     * Much faster than Rebuild() but may result in suboptimal tree.
     */
    void Update();

    /**
     * @brief Update bounds for a specific primitive
     *
     * @param primitiveIndex Index of the primitive to update
     * @param newBounds New bounding box for the primitive
     */
    void UpdatePrimitive(uint32_t primitiveIndex, const AABB& newBounds);

    /**
     * @brief Clear all data
     */
    void Clear();

    // =========================================================================
    // Ray Traversal
    // =========================================================================

    /**
     * @brief Traverse BVH with a ray to find potential intersections
     *
     * Returns all primitives whose bounds the ray intersects within maxDist.
     * The actual SDF evaluation should be performed on the candidates.
     *
     * @param ray Ray to trace (origin + normalized direction)
     * @param maxDist Maximum traversal distance
     * @return Traversal result with candidate primitives
     */
    [[nodiscard]] SDFBVHTraversalResult Traverse(
        const Ray& ray,
        float maxDist = std::numeric_limits<float>::max()) const;

    /**
     * @brief Traverse with callback for each candidate primitive
     *
     * More efficient than returning all candidates when you can
     * terminate early based on SDF evaluation.
     *
     * @param ray Ray to trace
     * @param maxDist Maximum distance
     * @param callback Called for each candidate, return false to stop
     * @return Number of candidates processed
     */
    uint32_t Traverse(
        const Ray& ray,
        float maxDist,
        const std::function<bool(uint32_t primitiveIndex, float tMin, float tMax)>& callback) const;

    /**
     * @brief Traverse for raymarching with distance-sorted candidates
     *
     * Returns candidates sorted by their entry distance, optimized for
     * sphere-tracing style raymarching.
     *
     * @param ray Ray to trace
     * @param maxDist Maximum distance
     * @param maxCandidates Maximum number of candidates to return
     * @return Sorted traversal result
     */
    [[nodiscard]] SDFBVHTraversalResult TraverseSorted(
        const Ray& ray,
        float maxDist,
        uint32_t maxCandidates = 64) const;

    // =========================================================================
    // Point/Range Queries
    // =========================================================================

    /**
     * @brief Query all primitives within a radius of a point
     *
     * @param point Query center point
     * @param radius Search radius
     * @return Query result with nearby primitives
     */
    [[nodiscard]] SDFBVHQueryResult Query(
        const glm::vec3& point,
        float radius) const;

    /**
     * @brief Query with callback for each nearby primitive
     *
     * @param point Query center
     * @param radius Search radius
     * @param callback Called for each primitive in range
     */
    void Query(
        const glm::vec3& point,
        float radius,
        const std::function<void(uint32_t primitiveIndex, float distance)>& callback) const;

    /**
     * @brief Find the nearest primitive to a point
     *
     * @param point Query point
     * @param maxDist Maximum search distance
     * @return Index of nearest primitive, or -1 if none found
     */
    [[nodiscard]] int32_t QueryNearest(
        const glm::vec3& point,
        float maxDist = std::numeric_limits<float>::max()) const;

    /**
     * @brief Find K nearest primitives to a point
     *
     * @param point Query point
     * @param k Number of neighbors to find
     * @param maxDist Maximum search distance
     * @return Vector of primitive indices sorted by distance
     */
    [[nodiscard]] std::vector<uint32_t> QueryKNearest(
        const glm::vec3& point,
        uint32_t k,
        float maxDist = std::numeric_limits<float>::max()) const;

    /**
     * @brief Query primitives intersecting an AABB
     *
     * @param queryBounds AABB to test against
     * @return Vector of intersecting primitive indices
     */
    [[nodiscard]] std::vector<uint32_t> QueryAABB(const AABB& queryBounds) const;

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Check if BVH has been built
     */
    [[nodiscard]] bool IsBuilt() const noexcept { return !m_nodes.empty(); }

    /**
     * @brief Get the root bounds
     */
    [[nodiscard]] AABB GetBounds() const noexcept {
        return m_nodes.empty() ? AABB() : m_nodes[0].bounds;
    }

    /**
     * @brief Get number of primitives
     */
    [[nodiscard]] size_t GetPrimitiveCount() const noexcept { return m_primitives.size(); }

    /**
     * @brief Get number of nodes
     */
    [[nodiscard]] size_t GetNodeCount() const noexcept { return m_nodes.size(); }

    /**
     * @brief Get tree depth
     */
    [[nodiscard]] uint32_t GetDepth() const noexcept;

    /**
     * @brief Get SAH cost of current tree
     */
    [[nodiscard]] float GetSAHCost() const noexcept;

    /**
     * @brief Get build statistics
     */
    [[nodiscard]] const SDFBVHStats& GetStats() const noexcept { return m_stats; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const SDFBVHConfig& GetConfig() const noexcept { return m_config; }

    /**
     * @brief Set configuration (takes effect on next build)
     */
    void SetConfig(const SDFBVHConfig& config) { m_config = config; }

    /**
     * @brief Get all nodes (for debugging/visualization)
     */
    [[nodiscard]] const std::vector<SDFBVHNode>& GetNodes() const noexcept { return m_nodes; }

    /**
     * @brief Get all primitives
     */
    [[nodiscard]] const std::vector<SDFBVHPrimitive>& GetPrimitives() const noexcept { return m_primitives; }

    /**
     * @brief Get primitive by index
     */
    [[nodiscard]] const SDFBVHPrimitive& GetPrimitive(uint32_t index) const { return m_primitives[m_primitiveIndices[index]]; }

    /**
     * @brief Get memory usage in bytes
     */
    [[nodiscard]] size_t GetMemoryUsage() const noexcept;

private:
    // SAH bin for binned construction
    struct SAHBin {
        AABB bounds;
        uint32_t count = 0;
    };

    // SAH split result
    struct SAHSplit {
        int axis = 0;
        float position = 0.0f;
        float cost = std::numeric_limits<float>::max();
        bool valid = false;
    };

    // Build methods
    uint32_t BuildRecursive(uint32_t begin, uint32_t end, uint32_t depth);
    SAHSplit FindBestSplit(uint32_t begin, uint32_t end, const AABB& centroidBounds);
    SAHSplit FindBestSplitBinned(uint32_t begin, uint32_t end, const AABB& centroidBounds);
    SAHSplit FindBestSplitFull(uint32_t begin, uint32_t end);
    void UpdateNodeBounds(uint32_t nodeIndex);
    void RefitRecursive(uint32_t nodeIndex);
    uint32_t CalculateDepthRecursive(uint32_t nodeIndex) const;
    float CalculateSAHCostRecursive(uint32_t nodeIndex, float rootArea) const;

    // Traversal helpers
    void TraverseInternal(
        uint32_t nodeIndex,
        const Ray& ray,
        const glm::vec3& invDir,
        float& maxDist,
        SDFBVHTraversalResult& result) const;

    void TraverseInternalCallback(
        uint32_t nodeIndex,
        const Ray& ray,
        const glm::vec3& invDir,
        float& maxDist,
        const std::function<bool(uint32_t, float, float)>& callback,
        uint32_t& count) const;

    // Query helpers
    void QueryInternal(
        uint32_t nodeIndex,
        const glm::vec3& point,
        float radius,
        float radius2,
        SDFBVHQueryResult& result) const;

    void QueryNearestInternal(
        uint32_t nodeIndex,
        const glm::vec3& point,
        int32_t& nearest,
        float& nearestDist2) const;

    void QueryAABBInternal(
        uint32_t nodeIndex,
        const AABB& queryBounds,
        std::vector<uint32_t>& result) const;

    // Data
    std::vector<SDFBVHNode> m_nodes;
    std::vector<SDFBVHPrimitive> m_primitives;
    std::vector<uint32_t> m_primitiveIndices;  // Permutation for leaf ordering

    SDFBVHConfig m_config;
    SDFBVHStats m_stats;
    bool m_needsRebuild = false;
};

// Template implementation
template<typename T, typename BoundsFunc>
void SDFBVH::Build(std::span<const T> primitives, BoundsFunc&& boundsFunc) {
    std::vector<SDFBVHPrimitive> sdfPrimitives;
    sdfPrimitives.reserve(primitives.size());

    for (uint32_t i = 0; i < primitives.size(); ++i) {
        SDFBVHPrimitive prim;
        prim.id = i;
        prim.bounds = boundsFunc(primitives[i]);
        prim.centroid = prim.bounds.GetCenter();
        prim.primitive = nullptr;
        prim.userData = 0;
        sdfPrimitives.push_back(prim);
    }

    Build(std::move(sdfPrimitives));
}

} // namespace Nova
