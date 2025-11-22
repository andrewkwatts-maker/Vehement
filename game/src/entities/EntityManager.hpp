#pragma once

#include "Entity.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <array>

namespace Nova {
    class Renderer;
    class Graph;
    class JobSystem;
}

namespace Vehement {

// Forward declarations
class Player;
class Zombie;
class NPC;

/**
 * @brief Spatial hash cell for efficient entity queries
 */
struct SpatialHashCell {
    std::vector<Entity::EntityId> entityIds;
};

/**
 * @brief Configuration for spatial partitioning
 */
struct SpatialConfig {
    float cellSize = 10.0f;
    bool enabled = true;
};

/**
 * @brief Entity Manager for Vehement2
 *
 * Manages all game entities including creation, destruction, updating,
 * and rendering. Provides efficient spatial queries for collision detection
 * and AI targeting.
 */
class EntityManager {
public:
    using EntityCallback = std::function<void(Entity&)>;
    using EntityPredicate = std::function<bool(const Entity&)>;

    EntityManager();
    explicit EntityManager(const SpatialConfig& config);
    ~EntityManager();

    // Non-copyable, movable
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = default;
    EntityManager& operator=(EntityManager&&) noexcept = default;

    // =========================================================================
    // Entity Lifecycle
    // =========================================================================

    /**
     * @brief Add an entity to the manager
     * @param entity Unique pointer to entity (ownership transferred)
     * @return Entity ID assigned to the entity
     */
    Entity::EntityId AddEntity(std::unique_ptr<Entity> entity);

    /**
     * @brief Create and add an entity of type T
     * @tparam T Entity type (must derive from Entity)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to created entity (owned by manager)
     */
    template<typename T, typename... Args>
    T* CreateEntity(Args&&... args);

    /**
     * @brief Remove an entity by ID
     * @param id Entity ID to remove
     * @return true if entity was found and removed
     */
    bool RemoveEntity(Entity::EntityId id);

    /**
     * @brief Remove all entities marked for removal
     */
    void RemoveMarkedEntities();

    /**
     * @brief Clear all entities
     */
    void Clear();

    /**
     * @brief Get entity by ID
     * @param id Entity ID
     * @return Pointer to entity or nullptr if not found
     */
    [[nodiscard]] Entity* GetEntity(Entity::EntityId id);
    [[nodiscard]] const Entity* GetEntity(Entity::EntityId id) const;

    /**
     * @brief Get entity by ID, cast to specific type
     * @tparam T Expected entity type
     * @param id Entity ID
     * @return Pointer to entity or nullptr if not found or wrong type
     */
    template<typename T>
    [[nodiscard]] T* GetEntityAs(Entity::EntityId id);

    template<typename T>
    [[nodiscard]] const T* GetEntityAs(Entity::EntityId id) const;

    // =========================================================================
    // Player Access
    // =========================================================================

    /**
     * @brief Set the player entity
     * @param player Pointer to player entity (must be added to manager)
     */
    void SetPlayer(Player* player) { m_player = player; }

    /**
     * @brief Get the player entity
     */
    [[nodiscard]] Player* GetPlayer() { return m_player; }
    [[nodiscard]] const Player* GetPlayer() const { return m_player; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update all entities
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Update AI for zombies and NPCs
     * @param deltaTime Time since last frame
     * @param navGraph Navigation graph for pathfinding (can be null)
     */
    void UpdateAI(float deltaTime, Nova::Graph* navGraph);

    /**
     * @brief Render all entities sorted by Y position
     * @param renderer The renderer to use
     */
    void Render(Nova::Renderer& renderer);

    /**
     * @brief Render all entities with custom sort predicate
     * @param renderer The renderer to use
     * @param sortPredicate Function to determine draw order
     */
    void Render(Nova::Renderer& renderer,
                std::function<bool(const Entity&, const Entity&)> sortPredicate);

    // =========================================================================
    // Collision Detection
    // =========================================================================

    /**
     * @brief Check collision between two entities
     * @param a First entity ID
     * @param b Second entity ID
     * @return true if entities collide
     */
    [[nodiscard]] bool CheckCollision(Entity::EntityId a, Entity::EntityId b) const;

    /**
     * @brief Find all entities colliding with a given entity
     * @param entityId Entity to check
     * @return Vector of colliding entity IDs
     */
    [[nodiscard]] std::vector<Entity::EntityId> GetCollidingEntities(Entity::EntityId entityId) const;

    /**
     * @brief Find all entities colliding with a given entity, filtered by type
     * @param entityId Entity to check
     * @param type Entity type filter
     * @return Vector of colliding entity IDs
     */
    [[nodiscard]] std::vector<Entity::EntityId> GetCollidingEntities(
        Entity::EntityId entityId, EntityType type) const;

    /**
     * @brief Perform collision response between all entities
     * Calls OnCollision callback for each collision pair
     */
    void ProcessCollisions();

    /**
     * @brief Set collision callback
     * @param callback Function called for each collision (entity1, entity2)
     */
    using CollisionCallback = std::function<void(Entity&, Entity&)>;
    void SetCollisionCallback(CollisionCallback callback) { m_collisionCallback = std::move(callback); }

    // =========================================================================
    // Spatial Queries
    // =========================================================================

    /**
     * @brief Find all entities within radius of a position
     * @param position Center position
     * @param radius Search radius
     * @return Vector of entity pointers
     */
    [[nodiscard]] std::vector<Entity*> FindEntitiesInRadius(
        const glm::vec3& position, float radius);

    /**
     * @brief Find all entities within radius, filtered by type
     * @param position Center position
     * @param radius Search radius
     * @param type Entity type filter
     * @return Vector of entity pointers
     */
    [[nodiscard]] std::vector<Entity*> FindEntitiesInRadius(
        const glm::vec3& position, float radius, EntityType type);

    /**
     * @brief Find all entities matching a predicate within radius
     * @param position Center position
     * @param radius Search radius
     * @param predicate Filter function
     * @return Vector of entity pointers
     */
    [[nodiscard]] std::vector<Entity*> FindEntitiesInRadius(
        const glm::vec3& position, float radius, EntityPredicate predicate);

    /**
     * @brief Get nearest entity to a position
     * @param position Reference position
     * @return Pointer to nearest entity, or nullptr if none exist
     */
    [[nodiscard]] Entity* GetNearestEntity(const glm::vec3& position);

    /**
     * @brief Get nearest entity of a specific type
     * @param position Reference position
     * @param type Entity type filter
     * @return Pointer to nearest entity of type, or nullptr if none exist
     */
    [[nodiscard]] Entity* GetNearestEntity(const glm::vec3& position, EntityType type);

    /**
     * @brief Get nearest entity matching predicate
     * @param position Reference position
     * @param predicate Filter function
     * @return Pointer to nearest matching entity, or nullptr
     */
    [[nodiscard]] Entity* GetNearestEntity(const glm::vec3& position, EntityPredicate predicate);

    // =========================================================================
    // Entity Iteration
    // =========================================================================

    /**
     * @brief Iterate over all entities
     * @param callback Function to call for each entity
     */
    void ForEachEntity(EntityCallback callback);

    /**
     * @brief Iterate over entities of a specific type
     * @param type Entity type filter
     * @param callback Function to call for each matching entity
     */
    void ForEachEntity(EntityType type, EntityCallback callback);

    /**
     * @brief Get all entities of a type
     * @param type Entity type
     * @return Vector of entity pointers
     */
    [[nodiscard]] std::vector<Entity*> GetEntitiesByType(EntityType type);

    /**
     * @brief Get all entities matching predicate
     * @param predicate Filter function
     * @return Vector of entity pointers
     */
    [[nodiscard]] std::vector<Entity*> GetEntities(EntityPredicate predicate);

    // =========================================================================
    // Statistics
    // =========================================================================

    /** @brief Get total entity count */
    [[nodiscard]] size_t GetEntityCount() const noexcept { return m_entities.size(); }

    /** @brief Get count of entities by type */
    [[nodiscard]] size_t GetEntityCount(EntityType type) const;

    /** @brief Get count of alive entities */
    [[nodiscard]] size_t GetAliveEntityCount() const;

    /** @brief Get count of alive entities by type */
    [[nodiscard]] size_t GetAliveEntityCount(EntityType type) const;

    // =========================================================================
    // Spatial Partitioning
    // =========================================================================

    /** @brief Rebuild spatial hash (call after bulk entity additions/moves) */
    void RebuildSpatialHash();

    /** @brief Enable or disable spatial partitioning */
    void SetSpatialPartitioningEnabled(bool enabled) { m_spatialConfig.enabled = enabled; }

    /** @brief Check if spatial partitioning is enabled */
    [[nodiscard]] bool IsSpatialPartitioningEnabled() const noexcept { return m_spatialConfig.enabled; }

    /** @brief Set spatial cell size */
    void SetSpatialCellSize(float size);

    // =========================================================================
    // ECS-Style Performance Optimizations
    // =========================================================================

    /**
     * @brief Update all entities with parallel execution
     * @param deltaTime Time since last frame
     * @param useParallel Use job system for parallel updates
     */
    void UpdateParallel(float deltaTime, bool useParallel = true);

    /**
     * @brief Iterate over entities of a type with cache-efficient access
     * Builds a contiguous array for iteration
     * @param type Entity type filter
     * @param callback Function to call for each matching entity
     */
    void ForEachEntityOptimized(EntityType type, EntityCallback callback);

    /**
     * @brief Batch process entities by type
     * @param type Entity type
     * @param batchSize Number of entities per batch
     * @param batchCallback Function called with (entities, count)
     */
    void BatchProcess(EntityType type, size_t batchSize,
                      std::function<void(Entity**, size_t)> batchCallback);

    /**
     * @brief Get cached entity arrays by type (avoid repeated filtering)
     */
    [[nodiscard]] const std::vector<Entity*>& GetCachedEntitiesByType(EntityType type);

    /**
     * @brief Invalidate entity caches (call after add/remove)
     */
    void InvalidateEntityCaches();

    /**
     * @brief Pre-build type caches for faster iteration
     */
    void BuildEntityCaches();

    /**
     * @brief Parallel collision detection
     * @param callback Function called for each collision pair
     */
    void ProcessCollisionsParallel(CollisionCallback callback);

    /**
     * @brief Get entity positions as contiguous array (SoA style)
     * @param type Entity type filter
     * @param positions Output vector of positions
     * @param entityIds Output vector of corresponding entity IDs
     */
    void GetPositionsSoA(EntityType type, std::vector<glm::vec3>& positions,
                         std::vector<Entity::EntityId>& entityIds);

    /**
     * @brief Apply position updates from contiguous array
     * @param entityIds Entity IDs
     * @param positions New positions
     */
    void SetPositionsSoA(const std::vector<Entity::EntityId>& entityIds,
                         const std::vector<glm::vec3>& positions);

private:
    // Internal entity ID counter
    Entity::EntityId m_nextId = 1;

    // Entity storage
    std::unordered_map<Entity::EntityId, std::unique_ptr<Entity>> m_entities;
    Player* m_player = nullptr;

    // Spatial partitioning
    SpatialConfig m_spatialConfig;
    std::unordered_map<int64_t, SpatialHashCell> m_spatialHash;

    // Callbacks
    CollisionCallback m_collisionCallback;

    // Render sorting cache
    mutable std::vector<Entity*> m_renderOrder;
    mutable bool m_renderOrderDirty = true;

    // Type-based entity caches for ECS-style iteration
    static constexpr size_t NUM_ENTITY_TYPES = 8;  // Adjust based on EntityType enum
    mutable std::array<std::vector<Entity*>, NUM_ENTITY_TYPES> m_typeCaches;
    mutable std::array<bool, NUM_ENTITY_TYPES> m_typeCachesDirty;
    mutable bool m_allCachesDirty = true;

    // Spatial hash helpers
    [[nodiscard]] int64_t GetSpatialKey(const glm::vec3& position) const;
    [[nodiscard]] int64_t GetSpatialKey(int x, int z) const;
    void AddToSpatialHash(Entity::EntityId id, const glm::vec3& position);
    void RemoveFromSpatialHash(Entity::EntityId id, const glm::vec3& position);
    void UpdateSpatialHash(Entity::EntityId id, const glm::vec3& oldPos, const glm::vec3& newPos);

    // Get nearby cells for query
    [[nodiscard]] std::vector<int64_t> GetNearbyCells(const glm::vec3& position, float radius) const;
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T, typename... Args>
T* EntityManager::CreateEntity(Args&&... args) {
    static_assert(std::is_base_of_v<Entity, T>, "T must derive from Entity");

    auto entity = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = entity.get();

    AddEntity(std::move(entity));

    return ptr;
}

template<typename T>
T* EntityManager::GetEntityAs(Entity::EntityId id) {
    static_assert(std::is_base_of_v<Entity, T>, "T must derive from Entity");

    Entity* entity = GetEntity(id);
    if (entity) {
        return dynamic_cast<T*>(entity);
    }
    return nullptr;
}

template<typename T>
const T* EntityManager::GetEntityAs(Entity::EntityId id) const {
    static_assert(std::is_base_of_v<Entity, T>, "T must derive from Entity");

    const Entity* entity = GetEntity(id);
    if (entity) {
        return dynamic_cast<const T*>(entity);
    }
    return nullptr;
}

} // namespace Vehement
