#pragma once

#include "../../../engine/spatial/SpatialManager.hpp"
#include "../../../engine/spatial/AABB.hpp"
#include "../../../engine/spatial/Frustum.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <shared_mutex>

namespace Vehement {

// Forward declarations
class Entity;

/**
 * @brief Team identifier for filtering queries
 */
enum class TeamId : uint8_t {
    None = 0,
    Player = 1,
    Enemy = 2,
    Neutral = 3,
    AlliedNPC = 4,
    Count
};

/**
 * @brief Game-specific spatial layers
 */
enum class GameSpatialLayer : uint64_t {
    Units = static_cast<uint64_t>(Nova::SpatialLayer::Units),
    Buildings = static_cast<uint64_t>(Nova::SpatialLayer::Buildings),
    Projectiles = static_cast<uint64_t>(Nova::SpatialLayer::Projectiles),
    Terrain = static_cast<uint64_t>(Nova::SpatialLayer::Terrain),
    Triggers = static_cast<uint64_t>(Nova::SpatialLayer::Triggers),
    Pickups = static_cast<uint64_t>(Nova::SpatialLayer::Custom0),
    Effects = static_cast<uint64_t>(Nova::SpatialLayer::Custom1),
    Navigation = static_cast<uint64_t>(Nova::SpatialLayer::Navigation)
};

/**
 * @brief Unit data stored alongside spatial info
 */
struct UnitSpatialData {
    uint64_t entityId = 0;
    TeamId team = TeamId::None;
    float radius = 0.5f;
    bool isAlive = true;
    bool canBeTargeted = true;
};

/**
 * @brief Cone query parameters
 */
struct ConeQuery {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    float angle = 45.0f;  // Half-angle in degrees
    float range = 10.0f;
};

/**
 * @brief Spatial event types
 */
enum class SpatialEventType {
    OnEnterRange,
    OnExitRange,
    OnEnterArea,
    OnExitArea
};

/**
 * @brief Spatial event data
 */
struct SpatialEvent {
    SpatialEventType type;
    uint64_t sourceId = 0;      // Entity that triggered the event
    uint64_t targetId = 0;      // Entity that was detected
    glm::vec3 position{0.0f};
    float distance = 0.0f;
};

/**
 * @brief Spatial event callback
 */
using SpatialEventCallback = std::function<void(const SpatialEvent&)>;

/**
 * @brief Range trigger for proximity detection
 */
struct RangeTrigger {
    uint64_t id = 0;
    uint64_t ownerId = 0;
    glm::vec3 center{0.0f};
    float radius = 0.0f;
    uint64_t layerMask = ~0ULL;
    TeamId teamFilter = TeamId::None;  // None = all teams
    SpatialEventCallback onEnter;
    SpatialEventCallback onExit;
    std::unordered_set<uint64_t> currentlyInRange;
};

/**
 * @brief Terrain raycast result
 */
struct TerrainHit {
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    uint32_t tileId = 0;
    bool hit = false;
};

/**
 * @brief Game-specific spatial system
 *
 * Provides game-specific query methods and integrates with the
 * entity lifecycle and game systems.
 *
 * Features:
 * - Team-filtered unit queries
 * - Building area queries
 * - Cone-based detection (for abilities, vision)
 * - Terrain raycasting
 * - Pathfinding obstacle integration
 * - Spatial events (OnEnterRange, OnExitRange)
 * - Lifecycle integration for auto-registration
 */
class SpatialSystem {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        Nova::AABB worldBounds = Nova::AABB(glm::vec3(-5000), glm::vec3(5000));
        float unitCellSize = 10.0f;
        float buildingCellSize = 50.0f;
        float projectileCellSize = 5.0f;
        bool enableRangeTriggers = true;
        size_t maxRangeTriggers = 1000;
    };

    SpatialSystem();
    explicit SpatialSystem(const Config& config);
    ~SpatialSystem();

    // Non-copyable, movable
    SpatialSystem(const SpatialSystem&) = delete;
    SpatialSystem& operator=(const SpatialSystem&) = delete;
    SpatialSystem(SpatialSystem&&) noexcept = default;
    SpatialSystem& operator=(SpatialSystem&&) noexcept = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the spatial system
     */
    void Initialize();

    /**
     * @brief Update (process range triggers, etc.)
     */
    void Update(float deltaTime);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    // =========================================================================
    // Entity Registration (Lifecycle Integration)
    // =========================================================================

    /**
     * @brief Register a unit entity
     */
    void RegisterUnit(uint64_t entityId, const glm::vec3& position, float radius,
                     TeamId team = TeamId::None);

    /**
     * @brief Register a building entity
     */
    void RegisterBuilding(uint64_t entityId, const Nova::AABB& bounds,
                         TeamId team = TeamId::None);

    /**
     * @brief Register a projectile
     */
    void RegisterProjectile(uint64_t entityId, const glm::vec3& position, float radius);

    /**
     * @brief Register terrain chunk
     */
    void RegisterTerrainChunk(uint64_t chunkId, const Nova::AABB& bounds);

    /**
     * @brief Unregister any entity
     */
    void UnregisterEntity(uint64_t entityId);

    /**
     * @brief Update entity position
     */
    void UpdateEntityPosition(uint64_t entityId, const glm::vec3& position);

    /**
     * @brief Update entity bounds
     */
    void UpdateEntityBounds(uint64_t entityId, const Nova::AABB& bounds);

    /**
     * @brief Mark unit as dead (for filtering)
     */
    void SetUnitAlive(uint64_t entityId, bool alive);

    /**
     * @brief Set unit targetability
     */
    void SetUnitTargetable(uint64_t entityId, bool targetable);

    // =========================================================================
    // Game-Specific Queries
    // =========================================================================

    /**
     * @brief Get all units within range of a position
     * @param position Center position
     * @param radius Search radius
     * @param teamFilter Filter by team (None = all teams)
     * @param aliveOnly Only include alive units
     * @param targetableOnly Only include targetable units
     */
    [[nodiscard]] std::vector<uint64_t> GetUnitsInRange(
        const glm::vec3& position,
        float radius,
        TeamId teamFilter = TeamId::None,
        bool aliveOnly = true,
        bool targetableOnly = false);

    /**
     * @brief Get all units within range, sorted by distance
     */
    [[nodiscard]] std::vector<std::pair<uint64_t, float>> GetUnitsInRangeSorted(
        const glm::vec3& position,
        float radius,
        TeamId teamFilter = TeamId::None,
        bool aliveOnly = true);

    /**
     * @brief Get nearest unit to position
     */
    [[nodiscard]] std::optional<uint64_t> GetNearestUnit(
        const glm::vec3& position,
        float maxRange,
        TeamId teamFilter = TeamId::None,
        bool aliveOnly = true,
        uint64_t excludeId = 0);

    /**
     * @brief Get K nearest units
     */
    [[nodiscard]] std::vector<uint64_t> GetKNearestUnits(
        const glm::vec3& position,
        size_t k,
        float maxRange,
        TeamId teamFilter = TeamId::None,
        bool aliveOnly = true);

    /**
     * @brief Get friendly units in range
     */
    [[nodiscard]] std::vector<uint64_t> GetFriendlyUnitsInRange(
        const glm::vec3& position,
        float radius,
        TeamId myTeam,
        bool aliveOnly = true);

    /**
     * @brief Get enemy units in range
     */
    [[nodiscard]] std::vector<uint64_t> GetEnemyUnitsInRange(
        const glm::vec3& position,
        float radius,
        TeamId myTeam,
        bool aliveOnly = true);

    /**
     * @brief Get buildings within an area
     */
    [[nodiscard]] std::vector<uint64_t> GetBuildingsInArea(const Nova::AABB& area);

    /**
     * @brief Get buildings owned by team
     */
    [[nodiscard]] std::vector<uint64_t> GetBuildingsInArea(
        const Nova::AABB& area,
        TeamId team);

    /**
     * @brief Get entities in a cone (for abilities, vision cones, etc.)
     */
    [[nodiscard]] std::vector<uint64_t> GetEntitiesInCone(
        const ConeQuery& cone,
        uint64_t layerMask = ~0ULL);

    /**
     * @brief Get units in a cone
     */
    [[nodiscard]] std::vector<uint64_t> GetUnitsInCone(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float halfAngleDegrees,
        float range,
        TeamId teamFilter = TeamId::None,
        bool aliveOnly = true);

    /**
     * @brief Raycast against terrain
     */
    [[nodiscard]] TerrainHit RaycastTerrain(const Nova::Ray& ray, float maxDistance = 1000.0f);

    /**
     * @brief Raycast against all entities
     */
    [[nodiscard]] std::vector<Nova::RayHit> RaycastEntities(
        const Nova::Ray& ray,
        float maxDistance = 1000.0f,
        uint64_t layerMask = ~0ULL);

    /**
     * @brief Raycast for first hit
     */
    [[nodiscard]] std::optional<Nova::RayHit> RaycastFirst(
        const Nova::Ray& ray,
        float maxDistance = 1000.0f,
        uint64_t layerMask = ~0ULL,
        uint64_t excludeId = 0);

    /**
     * @brief Check line of sight between two points
     */
    [[nodiscard]] bool HasLineOfSight(
        const glm::vec3& from,
        const glm::vec3& to,
        uint64_t excludeIdA = 0,
        uint64_t excludeIdB = 0);

    // =========================================================================
    // Pathfinding Integration
    // =========================================================================

    /**
     * @brief Get obstacles for pathfinding
     */
    [[nodiscard]] std::vector<Nova::AABB> GetPathfindingObstacles(const Nova::AABB& area);

    /**
     * @brief Check if a position is walkable
     */
    [[nodiscard]] bool IsPositionWalkable(const glm::vec3& position, float radius = 0.5f);

    /**
     * @brief Get navigable positions around a point
     */
    [[nodiscard]] std::vector<glm::vec3> GetNavigablePositions(
        const glm::vec3& center,
        float radius,
        float spacing = 1.0f);

    // =========================================================================
    // Range Triggers (Spatial Events)
    // =========================================================================

    /**
     * @brief Create a range trigger
     * @return Trigger ID
     */
    uint64_t CreateRangeTrigger(
        uint64_t ownerId,
        const glm::vec3& center,
        float radius,
        SpatialEventCallback onEnter,
        SpatialEventCallback onExit = nullptr,
        uint64_t layerMask = Nova::LayerMask(Nova::SpatialLayer::Units),
        TeamId teamFilter = TeamId::None);

    /**
     * @brief Update range trigger position
     */
    void UpdateRangeTrigger(uint64_t triggerId, const glm::vec3& center);

    /**
     * @brief Update range trigger radius
     */
    void UpdateRangeTriggerRadius(uint64_t triggerId, float radius);

    /**
     * @brief Remove a range trigger
     */
    void RemoveRangeTrigger(uint64_t triggerId);

    /**
     * @brief Get entities currently in trigger range
     */
    [[nodiscard]] std::vector<uint64_t> GetEntitiesInTrigger(uint64_t triggerId) const;

    // =========================================================================
    // Frustum Culling (for Rendering)
    // =========================================================================

    /**
     * @brief Get visible entities in camera frustum
     */
    [[nodiscard]] std::vector<uint64_t> GetVisibleEntities(
        const Nova::Frustum& frustum,
        uint64_t layerMask = ~0ULL);

    /**
     * @brief Get visible units for rendering
     */
    [[nodiscard]] std::vector<uint64_t> GetVisibleUnits(const Nova::Frustum& frustum);

    /**
     * @brief Get visible buildings for rendering
     */
    [[nodiscard]] std::vector<uint64_t> GetVisibleBuildings(const Nova::Frustum& frustum);

    // =========================================================================
    // Statistics and Debug
    // =========================================================================

    /**
     * @brief Get total registered unit count
     */
    [[nodiscard]] size_t GetUnitCount() const;

    /**
     * @brief Get total registered building count
     */
    [[nodiscard]] size_t GetBuildingCount() const;

    /**
     * @brief Get total registered projectile count
     */
    [[nodiscard]] size_t GetProjectileCount() const;

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

    /**
     * @brief Get underlying spatial manager
     */
    [[nodiscard]] Nova::SpatialManager& GetSpatialManager() { return m_spatialManager; }

    /**
     * @brief Enable debug visualization
     */
    void SetDebugVisualization(bool enabled);

    /**
     * @brief Draw debug info
     */
    void DrawDebug();

private:
    // Helper methods
    bool PassesTeamFilter(uint64_t entityId, TeamId filter) const;
    bool IsInCone(const glm::vec3& point, const ConeQuery& cone) const;
    void ProcessRangeTriggers();
    uint64_t GenerateTriggerId();

    // Configuration
    Config m_config;

    // Spatial manager
    Nova::SpatialManager m_spatialManager;

    // Unit data
    std::unordered_map<uint64_t, UnitSpatialData> m_unitData;
    std::unordered_map<uint64_t, TeamId> m_buildingTeams;

    // Range triggers
    std::unordered_map<uint64_t, RangeTrigger> m_rangeTriggers;
    uint64_t m_nextTriggerId = 1;

    // Statistics
    size_t m_unitCount = 0;
    size_t m_buildingCount = 0;
    size_t m_projectileCount = 0;

    // Thread safety
    mutable std::shared_mutex m_mutex;

    // Debug
    bool m_debugVisualization = false;
};

/**
 * @brief Global spatial system singleton for game
 */
class GameSpatialSystem {
public:
    [[nodiscard]] static SpatialSystem& Instance() {
        static SpatialSystem instance;
        return instance;
    }

    GameSpatialSystem(const GameSpatialSystem&) = delete;
    GameSpatialSystem& operator=(const GameSpatialSystem&) = delete;

private:
    GameSpatialSystem() = default;
};

#define g_GameSpatial GameSpatialSystem::Instance()

} // namespace Vehement
