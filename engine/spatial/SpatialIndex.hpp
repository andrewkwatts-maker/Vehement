/**
 * @file SpatialIndex.hpp
 * @brief Spatial acceleration structures for efficient geometric queries
 *
 * This file defines the ISpatialIndex interface and related types for
 * efficient spatial queries in 3D space. Implementations include:
 * - Octree: Hierarchical spatial partitioning for static scenes
 * - LooseOctree: Variant with oversized nodes for dynamic objects
 * - BVH: Bounding Volume Hierarchy for ray tracing
 * - SpatialHash: Grid-based hashing for uniform object distributions
 *
 * @section spatial_concepts Key Concepts
 *
 * **Spatial Index**: A data structure that organizes objects by their
 * spatial location to accelerate queries like "find all objects near X"
 * or "what objects does this ray hit?".
 *
 * **AABB (Axis-Aligned Bounding Box)**: The simplest bounding volume,
 * used for fast intersection tests and as the primary query primitive.
 *
 * **Layers**: Objects can be assigned to layers (64 available) for
 * filtering queries. Use layer masks to include/exclude specific layers.
 *
 * @section spatial_usage Basic Usage
 *
 * @code{.cpp}
 * #include <engine/spatial/SpatialIndex.hpp>
 *
 * // Create an octree spatial index
 * auto spatialIndex = Nova::CreateSpatialIndex(
 *     Nova::SpatialIndexType::Octree,
 *     Nova::AABB(glm::vec3(-500), glm::vec3(500))  // World bounds
 * );
 *
 * // Insert objects
 * spatialIndex->Insert(entityId, entityAABB, layer);
 *
 * // Query objects in an area
 * auto nearbyEntities = spatialIndex->QueryAABB(queryArea);
 *
 * // Query with filtering
 * Nova::SpatialQueryFilter filter;
 * filter.layerMask = LAYER_ENEMIES | LAYER_DESTRUCTIBLES;
 * filter.excludeId = playerId;  // Don't include self
 * auto enemies = spatialIndex->QuerySphere(position, radius, filter);
 *
 * // Ray casting
 * Nova::Ray ray{cameraPos, cameraForward};
 * auto hits = spatialIndex->QueryRay(ray, 100.0f);
 * if (!hits.empty()) {
 *     // Process hit: hits[0] is closest
 * }
 *
 * // Update when objects move
 * spatialIndex->Update(entityId, newAABB);
 * @endcode
 *
 * @section spatial_performance Performance Considerations
 *
 * - **Octree**: Best for static scenes with varying object sizes
 * - **LooseOctree**: Best for dynamic scenes with frequent updates
 * - **BVH**: Best for ray tracing and complex mesh queries
 * - **SpatialHash**: Best for many uniformly-sized objects
 *
 * Use `SpatialIndexType::Auto` to let the system choose based on
 * object distribution and update patterns.
 *
 * @section spatial_callbacks Callback Queries
 *
 * For hot paths, use callback-based queries to avoid allocations:
 *
 * @code{.cpp}
 * spatialIndex->QueryAABB(area, [&](uint64_t id, const AABB& bounds) {
 *     ProcessEntity(id);
 *     return true;  // Continue searching, false to stop early
 * });
 * @endcode
 *
 * @see AABB, Ray, Frustum for query primitives
 * @see docs/api/Spatial.md for complete API documentation
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include "AABB.hpp"
#include "Frustum.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <string_view>

namespace Nova {

/**
 * @brief Query filter for spatial queries
 */
struct SpatialQueryFilter {
    uint64_t layerMask = ~0ULL;        // Bitmask of layers to include
    uint64_t excludeId = 0;            // ID to exclude from results
    bool sortByDistance = false;       // Sort results by distance (for ray queries)

    [[nodiscard]] bool PassesFilter(uint64_t id, uint64_t layer) const noexcept {
        if (id == excludeId) return false;
        return (layerMask & (1ULL << layer)) != 0;
    }
};

/**
 * @brief Query statistics for profiling
 */
struct SpatialQueryStats {
    size_t nodesVisited = 0;
    size_t objectsTested = 0;
    size_t objectsReturned = 0;
    float queryTimeMs = 0.0f;

    void Reset() noexcept {
        nodesVisited = 0;
        objectsTested = 0;
        objectsReturned = 0;
        queryTimeMs = 0.0f;
    }
};

/**
 * @brief Unified interface for spatial indices
 *
 * Abstract base class that defines the common interface for all
 * spatial acceleration structures (Octree, BVH, SpatialHash, etc.)
 */
class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    // =========================================================================
    // Object Management
    // =========================================================================

    /**
     * @brief Insert an object into the index
     * @param id Unique identifier for the object
     * @param bounds Axis-aligned bounding box of the object
     * @param layer Optional layer for filtering (default 0)
     */
    virtual void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) = 0;

    /**
     * @brief Remove an object from the index
     * @param id Identifier of the object to remove
     * @return true if object was found and removed
     */
    virtual bool Remove(uint64_t id) = 0;

    /**
     * @brief Update an object's bounds
     * @param id Identifier of the object
     * @param newBounds New bounding box
     * @return true if object was found and updated
     */
    virtual bool Update(uint64_t id, const AABB& newBounds) = 0;

    /**
     * @brief Clear all objects from the index
     */
    virtual void Clear() = 0;

    /**
     * @brief Rebuild the index structure
     *
     * Some indices (like BVH) benefit from periodic rebuilding
     * when objects have moved significantly.
     */
    virtual void Rebuild() {}

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Query objects intersecting an AABB
     * @param query The query AABB
     * @param filter Optional query filter
     * @return Vector of object IDs that intersect the query AABB
     */
    [[nodiscard]] virtual std::vector<uint64_t> QueryAABB(
        const AABB& query,
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Query objects intersecting a sphere
     * @param center Sphere center
     * @param radius Sphere radius
     * @param filter Optional query filter
     * @return Vector of object IDs that intersect the sphere
     */
    [[nodiscard]] virtual std::vector<uint64_t> QuerySphere(
        const glm::vec3& center,
        float radius,
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Query objects inside a frustum
     * @param frustum The view frustum
     * @param filter Optional query filter
     * @return Vector of object IDs inside the frustum
     */
    [[nodiscard]] virtual std::vector<uint64_t> QueryFrustum(
        const Frustum& frustum,
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Cast a ray and find intersecting objects
     * @param ray The ray to cast
     * @param maxDist Maximum distance to search
     * @param filter Optional query filter
     * @return Vector of ray hits, sorted by distance
     */
    [[nodiscard]] virtual std::vector<RayHit> QueryRay(
        const Ray& ray,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Find the nearest object to a point
     * @param point Query point
     * @param maxDist Maximum search distance
     * @param filter Optional query filter
     * @return ID of nearest object, or 0 if none found
     */
    [[nodiscard]] virtual uint64_t QueryNearest(
        const glm::vec3& point,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Find K nearest objects to a point
     */
    [[nodiscard]] virtual std::vector<uint64_t> QueryKNearest(
        const glm::vec3& point,
        size_t k,
        float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) = 0;

    // =========================================================================
    // Callback-based Queries (for avoiding allocations)
    // =========================================================================

    /**
     * @brief Visitor callback type
     */
    using VisitorCallback = std::function<bool(uint64_t id, const AABB& bounds)>;

    /**
     * @brief Query with callback instead of returning vector
     * @param query The query AABB
     * @param callback Called for each intersecting object, return false to stop
     * @param filter Optional query filter
     */
    virtual void QueryAABB(
        const AABB& query,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) = 0;

    /**
     * @brief Query sphere with callback
     */
    virtual void QuerySphere(
        const glm::vec3& center,
        float radius,
        const VisitorCallback& callback,
        const SpatialQueryFilter& filter = {}) = 0;

    // =========================================================================
    // Information
    // =========================================================================

    /**
     * @brief Get number of objects in the index
     */
    [[nodiscard]] virtual size_t GetObjectCount() const noexcept = 0;

    /**
     * @brief Get the overall bounds of all objects
     */
    [[nodiscard]] virtual AABB GetBounds() const noexcept = 0;

    /**
     * @brief Get memory usage in bytes
     */
    [[nodiscard]] virtual size_t GetMemoryUsage() const noexcept = 0;

    /**
     * @brief Get the type name of this index
     */
    [[nodiscard]] virtual std::string_view GetTypeName() const noexcept = 0;

    /**
     * @brief Get statistics from the last query
     */
    [[nodiscard]] virtual const SpatialQueryStats& GetLastQueryStats() const noexcept = 0;

    /**
     * @brief Check if index supports efficient moving object tracking
     */
    [[nodiscard]] virtual bool SupportsMovingObjects() const noexcept { return false; }

    /**
     * @brief Get the bounds of an object by ID
     * @return Object bounds, or invalid AABB if not found
     */
    [[nodiscard]] virtual AABB GetObjectBounds(uint64_t id) const noexcept = 0;

    /**
     * @brief Check if object exists in index
     */
    [[nodiscard]] virtual bool Contains(uint64_t id) const noexcept = 0;
};

/**
 * @brief Base implementation with common functionality
 */
class SpatialIndexBase : public ISpatialIndex {
public:
    [[nodiscard]] const SpatialQueryStats& GetLastQueryStats() const noexcept override {
        return m_lastStats;
    }

protected:
    mutable SpatialQueryStats m_lastStats;
};

/**
 * @brief Factory for creating spatial indices
 */
enum class SpatialIndexType {
    Octree,
    LooseOctree,
    BVH,
    SpatialHash,
    Auto  // Automatically choose based on object distribution
};

/**
 * @brief Create a spatial index of the specified type
 * @param type Type of index to create
 * @param worldBounds Bounds of the world (for octree/hash)
 * @param cellSize Cell size (for spatial hash)
 * @return Unique pointer to the created index
 */
[[nodiscard]] std::unique_ptr<ISpatialIndex> CreateSpatialIndex(
    SpatialIndexType type,
    const AABB& worldBounds = AABB(glm::vec3(-1000), glm::vec3(1000)),
    float cellSize = 10.0f);

} // namespace Nova
