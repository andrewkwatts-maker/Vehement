#pragma once

#include "SpatialIndex.hpp"
#include "AABB.hpp"
#include "Frustum.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <span>
#include <algorithm>
#include <queue>

#ifdef __SSE__
#include <xmmintrin.h>
#include <smmintrin.h>
#endif

namespace Nova {

/**
 * @brief Bounding Volume Hierarchy for ray tracing and spatial queries
 *
 * Features:
 * - SAH (Surface Area Heuristic) construction for optimal tree quality
 * - Top-down and bottom-up builders
 * - Incremental updates for dynamic objects
 * - Ray tracing acceleration with sorted results
 * - Batch ray queries for multiple rays
 * - SIMD 4-wide node traversal
 */
class BVH : public SpatialIndexBase {
public:
    /**
     * @brief BVH node structure
     */
    struct alignas(32) Node {
        AABB bounds;
        uint32_t leftFirst = 0;   // Left child index or first primitive index
        uint32_t count = 0;       // 0 = internal node, >0 = leaf with count primitives
        uint64_t padding = 0;     // Padding for cache alignment

        [[nodiscard]] bool IsLeaf() const noexcept { return count > 0; }
    };

    /**
     * @brief Primitive data stored in BVH
     */
    struct Primitive {
        uint64_t id = 0;
        AABB bounds;
        glm::vec3 centroid{0.0f};
        uint64_t layer = 0;
    };

    /**
     * @brief BVH build quality setting
     */
    enum class BuildQuality {
        Fast,       // O(n log n) binned SAH
        Medium,     // More SAH bins
        High        // Full SAH evaluation
    };

    /**
     * @brief Configuration for BVH construction
     */
    struct Config {
        BuildQuality quality = BuildQuality::Medium;
        uint32_t maxPrimitivesPerLeaf = 4;
        uint32_t sahBins = 16;
        bool useBinnedSAH = true;
        float traversalCost = 1.0f;
        float intersectionCost = 1.0f;
    };

    BVH() = default;
    explicit BVH(const Config& config) : m_config(config) {}

    ~BVH() override = default;

    // Non-copyable, movable
    BVH(const BVH&) = delete;
    BVH& operator=(const BVH&) = delete;
    BVH(BVH&&) noexcept = default;
    BVH& operator=(BVH&&) noexcept = default;

    // =========================================================================
    // ISpatialIndex Implementation
    // =========================================================================

    void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) override;
    bool Remove(uint64_t id) override;
    bool Update(uint64_t id, const AABB& newBounds) override;
    void Clear() override;
    void Rebuild() override;

    [[nodiscard]] std::vector<uint64_t> QueryAABB(
        const AABB& query,
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QuerySphere(
        const glm::vec3& center,
        float radius,
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QueryFrustum(
        const Frustum& frustum,
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<RayHit> QueryRay(
        const Ray& ray,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] uint64_t QueryNearest(
        const glm::vec3& point,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QueryKNearest(
        const glm::vec3& point,
        size_t k,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    void QueryAABB(
        const AABB& query,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) override;

    void QuerySphere(
        const glm::vec3& center,
        float radius,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] size_t GetObjectCount() const noexcept override {
        return m_primitives.size();
    }

    [[nodiscard]] AABB GetBounds() const noexcept override {
        return m_nodes.empty() ? AABB::Invalid() : m_nodes[0].bounds;
    }

    [[nodiscard]] size_t GetMemoryUsage() const noexcept override {
        return m_nodes.capacity() * sizeof(Node) +
               m_primitives.capacity() * sizeof(Primitive) +
               m_primitiveIndices.capacity() * sizeof(uint32_t);
    }

    [[nodiscard]] std::string_view GetTypeName() const noexcept override {
        return "BVH";
    }

    [[nodiscard]] AABB GetObjectBounds(uint64_t id) const noexcept override {
        auto it = m_idToIndex.find(id);
        if (it != m_idToIndex.end() && it->second < m_primitives.size()) {
            return m_primitives[it->second].bounds;
        }
        return AABB::Invalid();
    }

    [[nodiscard]] bool Contains(uint64_t id) const noexcept override {
        return m_idToIndex.contains(id);
    }

    // =========================================================================
    // BVH-Specific Methods
    // =========================================================================

    /**
     * @brief Build BVH from scratch with current primitives
     */
    void Build();

    /**
     * @brief Build BVH using top-down SAH construction
     */
    void BuildTopDownSAH();

    /**
     * @brief Build BVH using bottom-up agglomerative construction
     */
    void BuildBottomUp();

    /**
     * @brief Refit BVH bounds without rebuilding structure
     *
     * Useful when objects move but topology doesn't change much.
     * Much faster than full rebuild.
     */
    void Refit();

    /**
     * @brief Get tree depth
     */
    [[nodiscard]] int GetDepth() const noexcept;

    /**
     * @brief Get SAH cost of current tree
     */
    [[nodiscard]] float GetSAHCost() const noexcept;

    /**
     * @brief Get node count
     */
    [[nodiscard]] size_t GetNodeCount() const noexcept { return m_nodes.size(); }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const Config& GetConfig() const noexcept { return m_config; }
    void SetConfig(const Config& config) { m_config = config; }

    /**
     * @brief Batch ray query for multiple rays (SIMD optimized)
     */
    [[nodiscard]] std::vector<std::vector<RayHit>> QueryRayBatch(
        std::span<const Ray> rays,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {});

#ifdef __SSE__
    /**
     * @brief Query 4 rays simultaneously using SIMD
     */
    void QueryRay4(
        const Ray* rays,
        float maxDist,
        const SpatialQueryFilter& filter,
        std::vector<RayHit>* results);
#endif

private:
    // SAH binned construction
    struct SAHBin {
        AABB bounds;
        uint32_t count = 0;
    };

    struct SAHSplit {
        int axis = 0;
        float position = 0.0f;
        float cost = std::numeric_limits<float>::max();
    };

    // Build methods
    uint32_t BuildRecursive(uint32_t begin, uint32_t end, int depth);
    SAHSplit FindBestSplit(uint32_t begin, uint32_t end, const AABB& centroidBounds);
    void UpdateNodeBounds(uint32_t nodeIndex);
    void RefitRecursive(uint32_t nodeIndex);
    int CalculateDepthRecursive(uint32_t nodeIndex) const;

    // Query implementations
    void QueryAABBInternal(uint32_t nodeIndex, const AABB& query,
                          const SpatialQueryFilter& filter,
                          std::vector<uint64_t>& results) const;

    void QuerySphereInternal(uint32_t nodeIndex, const glm::vec3& center,
                            float radius, float radius2,
                            const SpatialQueryFilter& filter,
                            std::vector<uint64_t>& results) const;

    void QueryFrustumInternal(uint32_t nodeIndex, const Frustum& frustum,
                             uint8_t planeMask,
                             const SpatialQueryFilter& filter,
                             std::vector<uint64_t>& results) const;

    void QueryRayInternal(uint32_t nodeIndex, const Ray& ray,
                         const glm::vec3& invDir, float& maxDist,
                         const SpatialQueryFilter& filter,
                         std::vector<RayHit>& results) const;

    void QueryNearestInternal(uint32_t nodeIndex, const glm::vec3& point,
                             const SpatialQueryFilter& filter,
                             uint64_t& nearest, float& nearestDist2) const;

    bool QueryAABBCallback(uint32_t nodeIndex, const AABB& query,
                          const SpatialQueryFilter& filter,
                          const VisitorCallback& callback) const;

    bool QuerySphereCallback(uint32_t nodeIndex, const glm::vec3& center,
                            float radius, float radius2,
                            const SpatialQueryFilter& filter,
                            const VisitorCallback& callback) const;

    // Data
    std::vector<Node> m_nodes;
    std::vector<Primitive> m_primitives;
    std::vector<uint32_t> m_primitiveIndices;
    std::unordered_map<uint64_t, uint32_t> m_idToIndex;
    Config m_config;
    bool m_needsRebuild = false;

#ifdef __SSE__
    // SIMD-friendly node data (SoA layout for 4-wide traversal)
    struct alignas(64) SIMDNodes {
        std::vector<__m128> minX, minY, minZ;
        std::vector<__m128> maxX, maxY, maxZ;
        std::vector<std::array<uint32_t, 4>> children;
    };
    mutable SIMDNodes m_simdNodes;
    void BuildSIMDNodes();
#endif
};

/**
 * @brief BVH builder for external mesh data
 */
class BVHBuilder {
public:
    struct Triangle {
        std::array<glm::vec3, 3> vertices;
        uint32_t materialId = 0;
    };

    /**
     * @brief Build BVH from triangles
     */
    [[nodiscard]] static BVH BuildFromTriangles(
        std::span<const Triangle> triangles,
        const BVH::Config& config = {});

    /**
     * @brief Build BVH from AABBs with IDs
     */
    [[nodiscard]] static BVH BuildFromAABBs(
        std::span<const std::pair<uint64_t, AABB>> objects,
        const BVH::Config& config = {});
};

} // namespace Nova
