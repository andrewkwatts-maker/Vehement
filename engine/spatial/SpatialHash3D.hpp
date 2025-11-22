#pragma once

#include "SpatialIndex.hpp"
#include "AABB.hpp"
#include "Frustum.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace Nova {

/**
 * @brief 3D Spatial Hash Grid for uniform object distributions
 *
 * Features:
 * - O(1) insertion and removal
 * - Configurable cell size
 * - Efficient neighbor queries
 * - Multi-resolution support for objects of varying sizes
 * - Optimized for moving objects
 */
class SpatialHash3D : public SpatialIndexBase {
public:
    /**
     * @brief Object stored in the hash grid
     */
    struct Object {
        uint64_t id = 0;
        AABB bounds;
        uint64_t layer = 0;
        int resolutionLevel = 0;  // For multi-resolution
    };

    /**
     * @brief Cell coordinates
     */
    struct CellCoord {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        [[nodiscard]] bool operator==(const CellCoord& other) const noexcept {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    /**
     * @brief Hash function for cell coordinates
     */
    struct CellHash {
        [[nodiscard]] size_t operator()(const CellCoord& c) const noexcept {
            // Spatial hash using prime numbers
            constexpr size_t p1 = 73856093;
            constexpr size_t p2 = 19349663;
            constexpr size_t p3 = 83492791;
            return (static_cast<size_t>(c.x) * p1) ^
                   (static_cast<size_t>(c.y) * p2) ^
                   (static_cast<size_t>(c.z) * p3);
        }
    };

    /**
     * @brief Configuration for spatial hash
     */
    struct Config {
        float cellSize = 10.0f;
        int numResolutionLevels = 3;  // For multi-resolution support
        size_t expectedObjectCount = 1000;
    };

    explicit SpatialHash3D(float cellSize = 10.0f);
    explicit SpatialHash3D(const Config& config);

    ~SpatialHash3D() override = default;

    // Non-copyable, movable
    SpatialHash3D(const SpatialHash3D&) = delete;
    SpatialHash3D& operator=(const SpatialHash3D&) = delete;
    SpatialHash3D(SpatialHash3D&&) noexcept = default;
    SpatialHash3D& operator=(SpatialHash3D&&) noexcept = default;

    // =========================================================================
    // ISpatialIndex Implementation
    // =========================================================================

    void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) override;
    bool Remove(uint64_t id) override;
    bool Update(uint64_t id, const AABB& newBounds) override;
    void Clear() override;

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
        return m_objects.size();
    }

    [[nodiscard]] AABB GetBounds() const noexcept override;

    [[nodiscard]] size_t GetMemoryUsage() const noexcept override;

    [[nodiscard]] std::string_view GetTypeName() const noexcept override {
        return "SpatialHash3D";
    }

    [[nodiscard]] bool SupportsMovingObjects() const noexcept override {
        return true;  // Spatial hash is excellent for moving objects
    }

    [[nodiscard]] AABB GetObjectBounds(uint64_t id) const noexcept override;
    [[nodiscard]] bool Contains(uint64_t id) const noexcept override;

    // =========================================================================
    // SpatialHash3D-Specific Methods
    // =========================================================================

    /**
     * @brief Get cell size
     */
    [[nodiscard]] float GetCellSize() const noexcept { return m_config.cellSize; }

    /**
     * @brief Set cell size (requires rehashing)
     */
    void SetCellSize(float cellSize);

    /**
     * @brief Get number of occupied cells
     */
    [[nodiscard]] size_t GetCellCount() const noexcept { return m_cells.size(); }

    /**
     * @brief Get objects in a specific cell
     */
    [[nodiscard]] std::vector<uint64_t> GetObjectsInCell(const CellCoord& cell) const;

    /**
     * @brief Get cell coordinate for a position
     */
    [[nodiscard]] CellCoord PositionToCell(const glm::vec3& pos) const noexcept;

    /**
     * @brief Get cell coordinate for a position at specific resolution level
     */
    [[nodiscard]] CellCoord PositionToCell(const glm::vec3& pos, int level) const noexcept;

    /**
     * @brief Query immediate neighbors of a cell
     */
    [[nodiscard]] std::vector<uint64_t> QueryNeighbors(
        const CellCoord& cell,
        const SpatialQueryFilter& filter = {}) const;

    /**
     * @brief Get all cells that an AABB overlaps
     */
    [[nodiscard]] std::vector<CellCoord> GetOverlappingCells(const AABB& bounds) const;

    /**
     * @brief Get statistics about cell occupancy
     */
    struct CellStats {
        size_t totalCells = 0;
        size_t emptyCells = 0;
        size_t maxObjectsPerCell = 0;
        float avgObjectsPerCell = 0.0f;
    };

    [[nodiscard]] CellStats GetCellStats() const;

    /**
     * @brief Optimize hash table size based on current usage
     */
    void Optimize();

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const Config& GetConfig() const noexcept { return m_config; }

private:
    /**
     * @brief Cell data structure
     */
    struct Cell {
        std::vector<uint64_t> objectIds;
    };

    /**
     * @brief Object tracking data
     */
    struct ObjectData {
        Object object;
        std::vector<CellCoord> cells;  // All cells this object overlaps
    };

    // Helper methods
    void InsertIntoCell(const CellCoord& cell, uint64_t id);
    void RemoveFromCell(const CellCoord& cell, uint64_t id);
    void UpdateObjectCells(uint64_t id, const AABB& newBounds);
    int CalculateResolutionLevel(const AABB& bounds) const;
    float GetCellSizeForLevel(int level) const;

    void QueryCellsForAABB(const AABB& query, const SpatialQueryFilter& filter,
                          std::unordered_set<uint64_t>& results) const;

    // Multi-resolution grid
    using CellMap = std::unordered_map<CellCoord, Cell, CellHash>;

    Config m_config;
    std::vector<CellMap> m_grids;  // One grid per resolution level
    CellMap& m_cells;              // Reference to primary grid
    std::unordered_map<uint64_t, ObjectData> m_objects;
};

/**
 * @brief Hierarchical spatial hash with multiple resolutions
 */
class HierarchicalSpatialHash : public SpatialIndexBase {
public:
    explicit HierarchicalSpatialHash(float baseCellSize = 10.0f, int levels = 4);

    void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) override;
    bool Remove(uint64_t id) override;
    bool Update(uint64_t id, const AABB& newBounds) override;
    void Clear() override;

    [[nodiscard]] std::vector<uint64_t> QueryAABB(
        const AABB& query, const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QuerySphere(
        const glm::vec3& center, float radius,
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QueryFrustum(
        const Frustum& frustum, const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<RayHit> QueryRay(
        const Ray& ray, float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] uint64_t QueryNearest(
        const glm::vec3& point, float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] std::vector<uint64_t> QueryKNearest(
        const glm::vec3& point, size_t k, float maxDist = std::numeric_limits<float>::max(),
        const SpatialQueryFilter& filter = {}) override;

    void QueryAABB(const AABB& query, const VisitorCallback& callback,
                  const SpatialQueryFilter& filter = {}) override;

    void QuerySphere(const glm::vec3& center, float radius,
                    const VisitorCallback& callback,
                    const SpatialQueryFilter& filter = {}) override;

    [[nodiscard]] size_t GetObjectCount() const noexcept override;
    [[nodiscard]] AABB GetBounds() const noexcept override;
    [[nodiscard]] size_t GetMemoryUsage() const noexcept override;
    [[nodiscard]] std::string_view GetTypeName() const noexcept override { return "HierarchicalSpatialHash"; }
    [[nodiscard]] AABB GetObjectBounds(uint64_t id) const noexcept override;
    [[nodiscard]] bool Contains(uint64_t id) const noexcept override;

private:
    int SelectLevel(const AABB& bounds) const;

    std::vector<std::unique_ptr<SpatialHash3D>> m_levels;
    std::unordered_map<uint64_t, int> m_objectLevels;
    float m_baseCellSize;
};

} // namespace Nova
