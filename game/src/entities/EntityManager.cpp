#include "EntityManager.hpp"
#include "Player.hpp"
#include "Zombie.hpp"
#include "NPC.hpp"
#include <engine/graphics/Renderer.hpp>
#include <engine/core/JobSystem.hpp>
#include <engine/core/Profiler.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Entity base class static member
// ============================================================================

Entity::EntityId Entity::s_nextId = 1;

// ============================================================================
// Entity base class implementation
// ============================================================================

Entity::Entity() : m_id(s_nextId++) {
}

Entity::Entity(EntityType type) : m_id(s_nextId++), m_type(type) {
}

void Entity::Update(float deltaTime) {
    // Base update - apply velocity already handled in derived classes
    // This is a hook for common update logic
}

void Entity::Render(Nova::Renderer& renderer) {
    // Base render - derived classes should implement actual rendering
    // This could draw a placeholder sprite using the texture
}

void Entity::LookAt(const glm::vec3& target) {
    glm::vec3 direction = target - m_position;
    direction.y = 0.0f;  // Keep rotation horizontal for top-down

    if (glm::length(direction) > 0.01f) {
        m_rotation = std::atan2(direction.x, direction.z);
    }
}

void Entity::LookAt2D(float x, float z) {
    LookAt(glm::vec3(x, m_position.y, z));
}

void Entity::SetHealth(float health) noexcept {
    m_health = std::clamp(health, 0.0f, m_maxHealth);
}

void Entity::SetMaxHealth(float maxHealth) noexcept {
    m_maxHealth = std::max(0.0f, maxHealth);
    m_health = std::min(m_health, m_maxHealth);
}

void Entity::Heal(float amount) noexcept {
    m_health = std::min(m_health + amount, m_maxHealth);
}

float Entity::TakeDamage(float amount, EntityId source) {
    if (amount <= 0.0f || !IsAlive()) {
        return 0.0f;
    }

    float actualDamage = std::min(amount, m_health);
    m_health -= actualDamage;

    if (m_health <= 0.0f) {
        Die();
    }

    return actualDamage;
}

void Entity::Die() {
    m_health = 0.0f;
    m_active = false;
    // Don't mark for removal here - let game logic decide
}

bool Entity::CollidesWith(const Entity& other) const {
    if (!m_collidable || !other.m_collidable) {
        return false;
    }

    float distSq = DistanceSquaredTo(other);
    float combinedRadius = m_collisionRadius + other.m_collisionRadius;

    return distSq <= (combinedRadius * combinedRadius);
}

float Entity::DistanceTo(const Entity& other) const {
    return glm::distance(m_position, other.m_position);
}

float Entity::DistanceSquaredTo(const Entity& other) const {
    glm::vec3 diff = m_position - other.m_position;
    return glm::dot(diff, diff);
}

// ============================================================================
// EntityManager Implementation
// ============================================================================

EntityManager::EntityManager() {
    m_spatialConfig.cellSize = 10.0f;
    m_spatialConfig.enabled = true;
}

EntityManager::EntityManager(const SpatialConfig& config)
    : m_spatialConfig(config) {
}

EntityManager::~EntityManager() {
    Clear();
}

Entity::EntityId EntityManager::AddEntity(std::unique_ptr<Entity> entity) {
    if (!entity) {
        return Entity::INVALID_ID;
    }

    Entity::EntityId id = entity->GetId();

    // Add to spatial hash
    if (m_spatialConfig.enabled) {
        AddToSpatialHash(id, entity->GetPosition());
    }

    m_entities[id] = std::move(entity);
    m_renderOrderDirty = true;

    return id;
}

bool EntityManager::RemoveEntity(Entity::EntityId id) {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) {
        return false;
    }

    // Remove from spatial hash
    if (m_spatialConfig.enabled) {
        RemoveFromSpatialHash(id, it->second->GetPosition());
    }

    // Clear player reference if this is the player
    if (m_player && m_player->GetId() == id) {
        m_player = nullptr;
    }

    m_entities.erase(it);
    m_renderOrderDirty = true;

    return true;
}

void EntityManager::RemoveMarkedEntities() {
    std::vector<Entity::EntityId> toRemove;

    for (const auto& [id, entity] : m_entities) {
        if (entity->IsMarkedForRemoval()) {
            toRemove.push_back(id);
        }
    }

    for (Entity::EntityId id : toRemove) {
        RemoveEntity(id);
    }
}

void EntityManager::Clear() {
    m_entities.clear();
    m_spatialHash.clear();
    m_player = nullptr;
    m_renderOrderDirty = true;
}

Entity* EntityManager::GetEntity(Entity::EntityId id) {
    auto it = m_entities.find(id);
    return it != m_entities.end() ? it->second.get() : nullptr;
}

const Entity* EntityManager::GetEntity(Entity::EntityId id) const {
    auto it = m_entities.find(id);
    return it != m_entities.end() ? it->second.get() : nullptr;
}

void EntityManager::Update(float deltaTime) {
    // Store old positions for spatial hash update
    std::vector<std::pair<Entity::EntityId, glm::vec3>> oldPositions;
    if (m_spatialConfig.enabled) {
        oldPositions.reserve(m_entities.size());
        for (const auto& [id, entity] : m_entities) {
            oldPositions.emplace_back(id, entity->GetPosition());
        }
    }

    // Update all entities
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive()) {
            entity->Update(deltaTime);
        }
    }

    // Update spatial hash for moved entities
    if (m_spatialConfig.enabled) {
        for (const auto& [id, oldPos] : oldPositions) {
            Entity* entity = GetEntity(id);
            if (entity) {
                const glm::vec3& newPos = entity->GetPosition();
                if (GetSpatialKey(oldPos) != GetSpatialKey(newPos)) {
                    UpdateSpatialHash(id, oldPos, newPos);
                }
            }
        }
    }

    // Remove dead entities
    RemoveMarkedEntities();

    m_renderOrderDirty = true;
}

void EntityManager::UpdateAI(float deltaTime, Nova::Graph* navGraph) {
    // Update zombie AI
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive() && entity->GetType() == EntityType::Zombie) {
            Zombie* zombie = static_cast<Zombie*>(entity.get());
            zombie->UpdateAI(deltaTime, *this, navGraph);
        }
    }

    // Update NPC AI
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive() && entity->GetType() == EntityType::NPC) {
            NPC* npc = static_cast<NPC*>(entity.get());
            npc->UpdateAI(deltaTime, *this, navGraph);
        }
    }
}

void EntityManager::Render(Nova::Renderer& renderer) {
    // Default sort: by Y position (for proper draw order in top-down view)
    // Higher Y (further back) drawn first
    Render(renderer, [](const Entity& a, const Entity& b) {
        return a.GetPosition().z > b.GetPosition().z;
    });
}

void EntityManager::Render(Nova::Renderer& renderer,
                           std::function<bool(const Entity&, const Entity&)> sortPredicate) {
    // Build render order if dirty
    if (m_renderOrderDirty) {
        m_renderOrder.clear();
        m_renderOrder.reserve(m_entities.size());

        for (auto& [id, entity] : m_entities) {
            if (entity->IsActive()) {
                m_renderOrder.push_back(entity.get());
            }
        }

        m_renderOrderDirty = false;
    }

    // Sort by predicate
    std::sort(m_renderOrder.begin(), m_renderOrder.end(),
              [&sortPredicate](const Entity* a, const Entity* b) {
                  return sortPredicate(*a, *b);
              });

    // Render all entities
    for (Entity* entity : m_renderOrder) {
        entity->Render(renderer);
    }
}

bool EntityManager::CheckCollision(Entity::EntityId a, Entity::EntityId b) const {
    const Entity* entityA = GetEntity(a);
    const Entity* entityB = GetEntity(b);

    if (!entityA || !entityB) {
        return false;
    }

    return entityA->CollidesWith(*entityB);
}

std::vector<Entity::EntityId> EntityManager::GetCollidingEntities(Entity::EntityId entityId) const {
    std::vector<Entity::EntityId> result;

    const Entity* entity = GetEntity(entityId);
    if (!entity || !entity->IsCollidable()) {
        return result;
    }

    if (m_spatialConfig.enabled) {
        // Use spatial hash for efficient query
        float radius = entity->GetCollisionRadius() * 2.0f;  // Conservative radius
        auto cells = GetNearbyCells(entity->GetPosition(), radius);

        for (int64_t cellKey : cells) {
            auto it = m_spatialHash.find(cellKey);
            if (it != m_spatialHash.end()) {
                for (Entity::EntityId otherId : it->second.entityIds) {
                    if (otherId != entityId) {
                        const Entity* other = GetEntity(otherId);
                        if (other && entity->CollidesWith(*other)) {
                            result.push_back(otherId);
                        }
                    }
                }
            }
        }
    } else {
        // Brute force
        for (const auto& [id, other] : m_entities) {
            if (id != entityId && entity->CollidesWith(*other)) {
                result.push_back(id);
            }
        }
    }

    return result;
}

std::vector<Entity::EntityId> EntityManager::GetCollidingEntities(
    Entity::EntityId entityId, EntityType type) const {

    auto all = GetCollidingEntities(entityId);
    std::vector<Entity::EntityId> result;

    for (Entity::EntityId id : all) {
        const Entity* entity = GetEntity(id);
        if (entity && entity->GetType() == type) {
            result.push_back(id);
        }
    }

    return result;
}

void EntityManager::ProcessCollisions() {
    if (!m_collisionCallback) {
        return;
    }

    // Track processed pairs to avoid duplicate callbacks
    std::vector<std::pair<Entity::EntityId, Entity::EntityId>> processedPairs;

    for (auto& [id, entity] : m_entities) {
        if (!entity->IsActive() || !entity->IsCollidable()) {
            continue;
        }

        auto collisions = GetCollidingEntities(id);
        for (Entity::EntityId otherId : collisions) {
            // Check if pair already processed
            auto pair = std::minmax(id, otherId);
            if (std::find(processedPairs.begin(), processedPairs.end(), pair) == processedPairs.end()) {
                processedPairs.push_back(pair);

                Entity* other = GetEntity(otherId);
                if (other) {
                    m_collisionCallback(*entity, *other);
                }
            }
        }
    }
}

std::vector<Entity*> EntityManager::FindEntitiesInRadius(const glm::vec3& position, float radius) {
    std::vector<Entity*> result;
    float radiusSq = radius * radius;

    if (m_spatialConfig.enabled) {
        auto cells = GetNearbyCells(position, radius);

        for (int64_t cellKey : cells) {
            auto it = m_spatialHash.find(cellKey);
            if (it != m_spatialHash.end()) {
                for (Entity::EntityId id : it->second.entityIds) {
                    Entity* entity = GetEntity(id);
                    if (entity && entity->IsActive()) {
                        glm::vec3 diff = entity->GetPosition() - position;
                        if (glm::dot(diff, diff) <= radiusSq) {
                            result.push_back(entity);
                        }
                    }
                }
            }
        }
    } else {
        for (auto& [id, entity] : m_entities) {
            if (entity->IsActive()) {
                glm::vec3 diff = entity->GetPosition() - position;
                if (glm::dot(diff, diff) <= radiusSq) {
                    result.push_back(entity.get());
                }
            }
        }
    }

    return result;
}

std::vector<Entity*> EntityManager::FindEntitiesInRadius(
    const glm::vec3& position, float radius, EntityType type) {

    auto all = FindEntitiesInRadius(position, radius);
    std::vector<Entity*> result;

    for (Entity* entity : all) {
        if (entity->GetType() == type) {
            result.push_back(entity);
        }
    }

    return result;
}

std::vector<Entity*> EntityManager::FindEntitiesInRadius(
    const glm::vec3& position, float radius, EntityPredicate predicate) {

    auto all = FindEntitiesInRadius(position, radius);
    std::vector<Entity*> result;

    for (Entity* entity : all) {
        if (predicate(*entity)) {
            result.push_back(entity);
        }
    }

    return result;
}

Entity* EntityManager::GetNearestEntity(const glm::vec3& position) {
    Entity* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive()) {
            glm::vec3 diff = entity->GetPosition() - position;
            float distSq = glm::dot(diff, diff);

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = entity.get();
            }
        }
    }

    return nearest;
}

Entity* EntityManager::GetNearestEntity(const glm::vec3& position, EntityType type) {
    Entity* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive() && entity->GetType() == type) {
            glm::vec3 diff = entity->GetPosition() - position;
            float distSq = glm::dot(diff, diff);

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = entity.get();
            }
        }
    }

    return nearest;
}

Entity* EntityManager::GetNearestEntity(const glm::vec3& position, EntityPredicate predicate) {
    Entity* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive() && predicate(*entity)) {
            glm::vec3 diff = entity->GetPosition() - position;
            float distSq = glm::dot(diff, diff);

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = entity.get();
            }
        }
    }

    return nearest;
}

void EntityManager::ForEachEntity(EntityCallback callback) {
    for (auto& [id, entity] : m_entities) {
        callback(*entity);
    }
}

void EntityManager::ForEachEntity(EntityType type, EntityCallback callback) {
    for (auto& [id, entity] : m_entities) {
        if (entity->GetType() == type) {
            callback(*entity);
        }
    }
}

std::vector<Entity*> EntityManager::GetEntitiesByType(EntityType type) {
    std::vector<Entity*> result;

    for (auto& [id, entity] : m_entities) {
        if (entity->GetType() == type) {
            result.push_back(entity.get());
        }
    }

    return result;
}

std::vector<Entity*> EntityManager::GetEntities(EntityPredicate predicate) {
    std::vector<Entity*> result;

    for (auto& [id, entity] : m_entities) {
        if (predicate(*entity)) {
            result.push_back(entity.get());
        }
    }

    return result;
}

size_t EntityManager::GetEntityCount(EntityType type) const {
    size_t count = 0;
    for (const auto& [id, entity] : m_entities) {
        if (entity->GetType() == type) {
            count++;
        }
    }
    return count;
}

size_t EntityManager::GetAliveEntityCount() const {
    size_t count = 0;
    for (const auto& [id, entity] : m_entities) {
        if (entity->IsAlive()) {
            count++;
        }
    }
    return count;
}

size_t EntityManager::GetAliveEntityCount(EntityType type) const {
    size_t count = 0;
    for (const auto& [id, entity] : m_entities) {
        if (entity->GetType() == type && entity->IsAlive()) {
            count++;
        }
    }
    return count;
}

void EntityManager::RebuildSpatialHash() {
    m_spatialHash.clear();

    if (!m_spatialConfig.enabled) {
        return;
    }

    for (const auto& [id, entity] : m_entities) {
        AddToSpatialHash(id, entity->GetPosition());
    }
}

void EntityManager::SetSpatialCellSize(float size) {
    m_spatialConfig.cellSize = std::max(1.0f, size);
    RebuildSpatialHash();
}

int64_t EntityManager::GetSpatialKey(const glm::vec3& position) const {
    int x = static_cast<int>(std::floor(position.x / m_spatialConfig.cellSize));
    int z = static_cast<int>(std::floor(position.z / m_spatialConfig.cellSize));
    return GetSpatialKey(x, z);
}

int64_t EntityManager::GetSpatialKey(int x, int z) const {
    // Combine x and z into a single 64-bit key
    return (static_cast<int64_t>(x) << 32) | (static_cast<int64_t>(z) & 0xFFFFFFFF);
}

void EntityManager::AddToSpatialHash(Entity::EntityId id, const glm::vec3& position) {
    int64_t key = GetSpatialKey(position);
    m_spatialHash[key].entityIds.push_back(id);
}

void EntityManager::RemoveFromSpatialHash(Entity::EntityId id, const glm::vec3& position) {
    int64_t key = GetSpatialKey(position);
    auto it = m_spatialHash.find(key);

    if (it != m_spatialHash.end()) {
        auto& ids = it->second.entityIds;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());

        if (ids.empty()) {
            m_spatialHash.erase(it);
        }
    }
}

void EntityManager::UpdateSpatialHash(Entity::EntityId id, const glm::vec3& oldPos, const glm::vec3& newPos) {
    RemoveFromSpatialHash(id, oldPos);
    AddToSpatialHash(id, newPos);
}

std::vector<int64_t> EntityManager::GetNearbyCells(const glm::vec3& position, float radius) const {
    std::vector<int64_t> cells;

    int cellRadius = static_cast<int>(std::ceil(radius / m_spatialConfig.cellSize));
    int centerX = static_cast<int>(std::floor(position.x / m_spatialConfig.cellSize));
    int centerZ = static_cast<int>(std::floor(position.z / m_spatialConfig.cellSize));

    for (int x = centerX - cellRadius; x <= centerX + cellRadius; x++) {
        for (int z = centerZ - cellRadius; z <= centerZ + cellRadius; z++) {
            cells.push_back(GetSpatialKey(x, z));
        }
    }

    return cells;
}

// ============================================================================
// ECS-Style Performance Optimizations
// ============================================================================

void EntityManager::UpdateParallel(float deltaTime, bool useParallel) {
    NOVA_PROFILE_SCOPE("EntityManager::UpdateParallel");

    // Store old positions for spatial hash update
    std::vector<std::pair<Entity::EntityId, glm::vec3>> oldPositions;
    if (m_spatialConfig.enabled) {
        oldPositions.reserve(m_entities.size());
        for (const auto& [id, entity] : m_entities) {
            if (entity->IsActive()) {
                oldPositions.emplace_back(id, entity->GetPosition());
            }
        }
    }

    // Build flat array for cache-efficient iteration
    std::vector<Entity*> activeEntities;
    activeEntities.reserve(m_entities.size());
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive()) {
            activeEntities.push_back(entity.get());
        }
    }

    // Update entities
    if (!useParallel || activeEntities.size() < 50 || !Nova::JobSystem::Instance().IsInitialized()) {
        // Sequential update
        for (Entity* entity : activeEntities) {
            entity->Update(deltaTime);
        }
    } else {
        // Parallel update
        Nova::JobSystem::Instance().ParallelFor(0, activeEntities.size(), [&](size_t i) {
            activeEntities[i]->Update(deltaTime);
        });
    }

    // Update spatial hash for moved entities
    if (m_spatialConfig.enabled) {
        NOVA_PROFILE_SCOPE("EntityManager::UpdateSpatialHash");
        for (const auto& [id, oldPos] : oldPositions) {
            Entity* entity = GetEntity(id);
            if (entity) {
                const glm::vec3& newPos = entity->GetPosition();
                if (GetSpatialKey(oldPos) != GetSpatialKey(newPos)) {
                    UpdateSpatialHash(id, oldPos, newPos);
                }
            }
        }
    }

    // Remove dead entities
    RemoveMarkedEntities();

    m_renderOrderDirty = true;
    m_allCachesDirty = true;
}

void EntityManager::ForEachEntityOptimized(EntityType type, EntityCallback callback) {
    NOVA_PROFILE_SCOPE("EntityManager::ForEachEntityOptimized");

    const auto& cached = GetCachedEntitiesByType(type);
    for (Entity* entity : cached) {
        callback(*entity);
    }
}

void EntityManager::BatchProcess(EntityType type, size_t batchSize,
                                  std::function<void(Entity**, size_t)> batchCallback) {
    NOVA_PROFILE_SCOPE("EntityManager::BatchProcess");

    auto& cached = const_cast<std::vector<Entity*>&>(GetCachedEntitiesByType(type));

    for (size_t i = 0; i < cached.size(); i += batchSize) {
        size_t count = std::min(batchSize, cached.size() - i);
        batchCallback(cached.data() + i, count);
    }
}

const std::vector<Entity*>& EntityManager::GetCachedEntitiesByType(EntityType type) {
    size_t typeIndex = static_cast<size_t>(type);
    if (typeIndex >= NUM_ENTITY_TYPES) {
        static std::vector<Entity*> empty;
        return empty;
    }

    // Rebuild cache if dirty
    if (m_allCachesDirty || m_typeCachesDirty[typeIndex]) {
        m_typeCaches[typeIndex].clear();

        for (auto& [id, entity] : m_entities) {
            if (entity->GetType() == type && entity->IsActive()) {
                m_typeCaches[typeIndex].push_back(entity.get());
            }
        }

        m_typeCachesDirty[typeIndex] = false;
    }

    return m_typeCaches[typeIndex];
}

void EntityManager::InvalidateEntityCaches() {
    m_allCachesDirty = true;
    m_renderOrderDirty = true;
}

void EntityManager::BuildEntityCaches() {
    NOVA_PROFILE_SCOPE("EntityManager::BuildEntityCaches");

    // Clear all caches
    for (auto& cache : m_typeCaches) {
        cache.clear();
    }

    // Build all caches in single pass
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive()) {
            size_t typeIndex = static_cast<size_t>(entity->GetType());
            if (typeIndex < NUM_ENTITY_TYPES) {
                m_typeCaches[typeIndex].push_back(entity.get());
            }
        }
    }

    m_allCachesDirty = false;
    for (auto& dirty : m_typeCachesDirty) {
        dirty = false;
    }
}

void EntityManager::ProcessCollisionsParallel(CollisionCallback callback) {
    NOVA_PROFILE_SCOPE("EntityManager::ProcessCollisionsParallel");

    if (!callback) {
        return;
    }

    // Build list of collidable entities
    std::vector<Entity*> collidables;
    collidables.reserve(m_entities.size());
    for (auto& [id, entity] : m_entities) {
        if (entity->IsActive() && entity->IsCollidable()) {
            collidables.push_back(entity.get());
        }
    }

    // For smaller sets, use sequential processing
    if (collidables.size() < 100 || !Nova::JobSystem::Instance().IsInitialized()) {
        ProcessCollisions();
        return;
    }

    // Thread-local collision pairs
    struct CollisionPair {
        Entity* a;
        Entity* b;
    };

    // Use spatial partitioning for broad phase
    std::vector<CollisionPair> collisionPairs;
    std::mutex pairsMutex;

    Nova::JobSystem::Instance().ParallelFor(0, collidables.size(), [&](size_t i) {
        Entity* entity = collidables[i];
        auto collisions = GetCollidingEntities(entity->GetId());

        for (Entity::EntityId otherId : collisions) {
            // Only add pair once (smaller id first)
            if (entity->GetId() < otherId) {
                Entity* other = GetEntity(otherId);
                if (other) {
                    std::lock_guard lock(pairsMutex);
                    collisionPairs.push_back({entity, other});
                }
            }
        }
    });

    // Process collision callbacks sequentially to avoid races
    for (auto& pair : collisionPairs) {
        callback(*pair.a, *pair.b);
    }
}

void EntityManager::GetPositionsSoA(EntityType type, std::vector<glm::vec3>& positions,
                                     std::vector<Entity::EntityId>& entityIds) {
    NOVA_PROFILE_SCOPE("EntityManager::GetPositionsSoA");

    positions.clear();
    entityIds.clear();

    const auto& cached = GetCachedEntitiesByType(type);
    positions.reserve(cached.size());
    entityIds.reserve(cached.size());

    for (Entity* entity : cached) {
        positions.push_back(entity->GetPosition());
        entityIds.push_back(entity->GetId());
    }
}

void EntityManager::SetPositionsSoA(const std::vector<Entity::EntityId>& entityIds,
                                     const std::vector<glm::vec3>& positions) {
    NOVA_PROFILE_SCOPE("EntityManager::SetPositionsSoA");

    if (entityIds.size() != positions.size()) {
        return;
    }

    for (size_t i = 0; i < entityIds.size(); ++i) {
        Entity* entity = GetEntity(entityIds[i]);
        if (entity) {
            glm::vec3 oldPos = entity->GetPosition();
            entity->SetPosition(positions[i]);

            // Update spatial hash
            if (m_spatialConfig.enabled && GetSpatialKey(oldPos) != GetSpatialKey(positions[i])) {
                UpdateSpatialHash(entityIds[i], oldPos, positions[i]);
            }
        }
    }
}

} // namespace Vehement
