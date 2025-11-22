#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <memory>
#include <functional>
#include <cstdint>
#include <limits>
#include <algorithm>

namespace Vehement {
namespace Systems {

// Forward declarations
class Entity;

/**
 * @brief Entity ID type for spatial structures
 */
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY_ID = 0;

/**
 * @brief Axis-aligned bounding box for spatial queries
 */
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    AABB() = default;
    AABB(const glm::vec3& center, float halfSize)
        : min(center - glm::vec3(halfSize))
        , max(center + glm::vec3(halfSize)) {}
    AABB(const glm::vec3& minPt, const glm::vec3& maxPt)
        : min(minPt), max(maxPt) {}

    [[nodiscard]] bool Contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool Intersects(const AABB& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }

    [[nodiscard]] glm::vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] glm::vec3 GetSize() const {
        return max - min;
    }
};

/**
 * @brief Spatial entry storing entity position and bounds
 */
struct SpatialEntry {
    EntityId entityId = INVALID_ENTITY_ID;
    glm::vec3 position{0.0f};
    float radius = 0.0f;
    bool isStatic = false;      // Static entities update less frequently
    bool isDirty = false;       // Position changed since last update
    uint32_t lastUpdateFrame = 0;
};

// ============================================================================
// Spatial Hash Grid - O(1) Average Lookup
// ============================================================================

/**
 * @brief High-performance spatial hash grid for entity queries
 *
 * Features:
 * - O(1) average insertion and removal
 * - Efficient range and radius queries
 * - Lazy updates for static entities
 * - Configurable cell size for optimal performance
 */
class SpatialHashGrid {
public:
    struct Config {
        float cellSize = 10.0f;
        size_t maxEntitiesPerCell = 100;
        size_t expectedEntityCount = 1000;
        bool trackStaticEntities = true;
    };

    explicit SpatialHashGrid(const Config& config = {});
    ~SpatialHashGrid() = default;

    // Non-copyable, movable
    SpatialHashGrid(const SpatialHashGrid&) = delete;
    SpatialHashGrid& operator=(const SpatialHashGrid&) = delete;
    SpatialHashGrid(SpatialHashGrid&&) noexcept = default;
    SpatialHashGrid& operator=(SpatialHashGrid&&) noexcept = default;

    // =========================================================================
    // Entity Management
    // =========================================================================

    /**
     * @brief Insert an entity into the grid
     * @param id Entity ID
     * @param position World position
     * @param radius Collision radius
     * @param isStatic Is this a static entity (infrequent updates)?
     */
    void Insert(EntityId id, const glm::vec3& position, float radius, bool isStatic = false);

    /**
     * @brief Remove an entity from the grid
     * @param id Entity ID
     * @return true if entity was found and removed
     */
    bool Remove(EntityId id);

    /**
     * @brief Update an entity's position (lazy update for statics)
     * @param id Entity ID
     * @param newPosition New world position
     */
    void Update(EntityId id, const glm::vec3& newPosition);

    /**
     * @brief Force update all dirty static entities
     */
    void FlushDirtyStatics();

    /**
     * @brief Clear all entities from the grid
     */
    void Clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Find all entities within a radius of a point
     * @param center Query center position
     * @param radius Search radius
     * @param result Output vector of entity IDs
     * @param excludeId Optional entity ID to exclude from results
     */
    void QueryRadius(const glm::vec3& center, float radius,
                     std::vector<EntityId>& result,
                     EntityId excludeId = INVALID_ENTITY_ID) const;

    /**
     * @brief Find all entities within an AABB
     * @param bounds Query bounds
     * @param result Output vector of entity IDs
     */
    void QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const;

    /**
     * @brief Find the nearest entity to a point
     * @param position Query position
     * @param maxDistance Maximum search distance
     * @param excludeId Optional entity ID to exclude
     * @return Nearest entity ID or INVALID_ENTITY_ID
     */
    [[nodiscard]] EntityId FindNearest(const glm::vec3& position,
                                       float maxDistance = std::numeric_limits<float>::max(),
                                       EntityId excludeId = INVALID_ENTITY_ID) const;

    /**
     * @brief Find K nearest entities to a point
     * @param position Query position
     * @param k Number of entities to find
     * @param result Output vector of entity IDs (sorted by distance)
     * @param maxDistance Maximum search distance
     * @param excludeId Optional entity ID to exclude
     */
    void FindKNearest(const glm::vec3& position, size_t k,
                      std::vector<EntityId>& result,
                      float maxDistance = std::numeric_limits<float>::max(),
                      EntityId excludeId = INVALID_ENTITY_ID) const;

    /**
     * @brief Check if any entity exists within radius
     * @param center Query center
     * @param radius Search radius
     * @param excludeId Optional entity ID to exclude
     * @return true if any entity found
     */
    [[nodiscard]] bool HasEntityInRadius(const glm::vec3& center, float radius,
                                         EntityId excludeId = INVALID_ENTITY_ID) const;

    /**
     * @brief Get all entities potentially colliding with given entity
     * @param id Entity ID to check
     * @param result Output vector of potentially colliding entity IDs
     */
    void GetPotentialCollisions(EntityId id, std::vector<EntityId>& result) const;

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * @brief Iterate over all entities in a radius
     * @param center Query center
     * @param radius Search radius
     * @param callback Function to call for each entity (return false to stop)
     */
    void ForEachInRadius(const glm::vec3& center, float radius,
                         std::function<bool(EntityId, const glm::vec3&, float)> callback) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t GetEntityCount() const { return m_entries.size(); }
    [[nodiscard]] size_t GetCellCount() const { return m_cells.size(); }
    [[nodiscard]] float GetCellSize() const { return m_config.cellSize; }
    [[nodiscard]] float GetAverageCellOccupancy() const;

    /**
     * @brief Optimize grid based on current entity distribution
     * May rebuild internal structures for better performance
     */
    void Optimize();

private:
    using CellKey = int64_t;

    struct Cell {
        std::vector<EntityId> entityIds;
    };

    [[nodiscard]] CellKey GetCellKey(const glm::vec3& position) const;
    [[nodiscard]] CellKey GetCellKey(int x, int y, int z) const;
    void GetCellRange(const glm::vec3& center, float radius,
                      int& minX, int& maxX, int& minY, int& maxY, int& minZ, int& maxZ) const;

    Config m_config;
    float m_invCellSize;
    std::unordered_map<EntityId, SpatialEntry> m_entries;
    std::unordered_map<CellKey, Cell> m_cells;
    std::vector<EntityId> m_dirtyStatics;
};

// ============================================================================
// Quadtree - Hierarchical Spatial Structure
// ============================================================================

/**
 * @brief Quadtree node for hierarchical spatial partitioning
 *
 * Optimized for 2D top-down games where Y (height) is less important.
 * Uses XZ plane for partitioning.
 */
class Quadtree {
public:
    struct Config {
        float minNodeSize = 5.0f;       // Minimum node dimension
        size_t maxEntitiesPerNode = 8;  // Split threshold
        size_t maxDepth = 8;            // Maximum tree depth
    };

    /**
     * @brief Construct a quadtree covering a region
     * @param bounds World bounds (uses X and Z dimensions)
     * @param config Configuration parameters
     */
    Quadtree(const AABB& bounds, const Config& config = {});
    ~Quadtree();

    // Non-copyable, movable
    Quadtree(const Quadtree&) = delete;
    Quadtree& operator=(const Quadtree&) = delete;
    Quadtree(Quadtree&&) noexcept = default;
    Quadtree& operator=(Quadtree&&) noexcept = default;

    // =========================================================================
    // Entity Management
    // =========================================================================

    void Insert(EntityId id, const glm::vec3& position, float radius);
    bool Remove(EntityId id);
    void Update(EntityId id, const glm::vec3& newPosition);
    void Clear();

    // =========================================================================
    // Queries
    // =========================================================================

    void QueryRadius(const glm::vec3& center, float radius,
                     std::vector<EntityId>& result) const;
    void QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const;
    [[nodiscard]] EntityId FindNearest(const glm::vec3& position,
                                       float maxDistance = std::numeric_limits<float>::max()) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t GetEntityCount() const { return m_entityCount; }
    [[nodiscard]] size_t GetNodeCount() const;
    [[nodiscard]] size_t GetMaxDepth() const;

    /**
     * @brief Rebuild the tree (call after many updates)
     */
    void Rebuild();

private:
    struct Node {
        AABB bounds;
        std::vector<EntityId> entities;
        std::array<std::unique_ptr<Node>, 4> children;  // NW, NE, SW, SE
        size_t depth = 0;

        [[nodiscard]] bool IsLeaf() const { return !children[0]; }
    };

    void InsertIntoNode(Node* node, EntityId id, const glm::vec3& position, float radius);
    void RemoveFromNode(Node* node, EntityId id);
    void QueryRadiusNode(const Node* node, const glm::vec3& center,
                         float radiusSq, std::vector<EntityId>& result) const;
    void QueryAABBNode(const Node* node, const AABB& bounds,
                       std::vector<EntityId>& result) const;
    void SplitNode(Node* node);
    void MergeNode(Node* node);
    size_t CountNodes(const Node* node) const;
    size_t GetMaxDepthRecursive(const Node* node) const;

    Config m_config;
    std::unique_ptr<Node> m_root;
    std::unordered_map<EntityId, glm::vec3> m_entityPositions;
    std::unordered_map<EntityId, float> m_entityRadii;
    size_t m_entityCount = 0;
};

// ============================================================================
// Hybrid Spatial System - Best of Both
// ============================================================================

/**
 * @brief Hybrid spatial system combining hash grid and quadtree
 *
 * Uses hash grid for dynamic entities (fast updates)
 * Uses quadtree for static entities (efficient queries over large areas)
 */
class HybridSpatialSystem {
public:
    struct Config {
        SpatialHashGrid::Config gridConfig;
        Quadtree::Config treeConfig;
        AABB worldBounds;
        bool useQuadtreeForStatics = true;
    };

    explicit HybridSpatialSystem(const Config& config);
    ~HybridSpatialSystem() = default;

    // =========================================================================
    // Entity Management
    // =========================================================================

    void Insert(EntityId id, const glm::vec3& position, float radius, bool isStatic = false);
    bool Remove(EntityId id);
    void Update(EntityId id, const glm::vec3& newPosition);
    void MarkStatic(EntityId id);
    void MarkDynamic(EntityId id);
    void Clear();

    // =========================================================================
    // Queries (combines results from both structures)
    // =========================================================================

    void QueryRadius(const glm::vec3& center, float radius,
                     std::vector<EntityId>& result,
                     EntityId excludeId = INVALID_ENTITY_ID) const;

    void QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const;

    [[nodiscard]] EntityId FindNearest(const glm::vec3& position,
                                       float maxDistance = std::numeric_limits<float>::max(),
                                       EntityId excludeId = INVALID_ENTITY_ID) const;

    void GetPotentialCollisions(EntityId id, std::vector<EntityId>& result) const;

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /**
     * @brief Batch insert multiple entities
     * More efficient than individual inserts
     */
    void BatchInsert(const std::vector<std::tuple<EntityId, glm::vec3, float, bool>>& entities);

    /**
     * @brief Batch update multiple entity positions
     */
    void BatchUpdate(const std::vector<std::pair<EntityId, glm::vec3>>& updates);

    // =========================================================================
    // Maintenance
    // =========================================================================

    /**
     * @brief Optimize internal structures (call periodically)
     */
    void Optimize();

    /**
     * @brief Flush all pending updates
     */
    void Flush();

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t GetDynamicEntityCount() const { return m_dynamicGrid.GetEntityCount(); }
    [[nodiscard]] size_t GetStaticEntityCount() const { return m_staticTree ? m_staticTree->GetEntityCount() : 0; }
    [[nodiscard]] size_t GetTotalEntityCount() const { return GetDynamicEntityCount() + GetStaticEntityCount(); }

private:
    Config m_config;
    SpatialHashGrid m_dynamicGrid;
    std::unique_ptr<Quadtree> m_staticTree;
    std::unordered_set<EntityId> m_staticEntities;
};

// ============================================================================
// Collision Layer System
// ============================================================================

/**
 * @brief Collision layer mask for filtering collision checks
 */
class CollisionLayers {
public:
    using LayerMask = uint32_t;

    // Predefined layers
    static constexpr LayerMask LAYER_NONE = 0;
    static constexpr LayerMask LAYER_DEFAULT = 1 << 0;
    static constexpr LayerMask LAYER_PLAYER = 1 << 1;
    static constexpr LayerMask LAYER_ENEMY = 1 << 2;
    static constexpr LayerMask LAYER_PROJECTILE = 1 << 3;
    static constexpr LayerMask LAYER_PICKUP = 1 << 4;
    static constexpr LayerMask LAYER_STATIC = 1 << 5;
    static constexpr LayerMask LAYER_TRIGGER = 1 << 6;
    static constexpr LayerMask LAYER_ALL = 0xFFFFFFFF;

    /**
     * @brief Set which layers a layer can collide with
     */
    void SetLayerCollision(LayerMask layer, LayerMask canCollideWith);

    /**
     * @brief Check if two layers can collide
     */
    [[nodiscard]] bool CanCollide(LayerMask layerA, LayerMask layerB) const;

    /**
     * @brief Get collision mask for a layer
     */
    [[nodiscard]] LayerMask GetCollisionMask(LayerMask layer) const;

private:
    std::unordered_map<LayerMask, LayerMask> m_collisionMatrix;
};

} // namespace Systems
} // namespace Vehement
