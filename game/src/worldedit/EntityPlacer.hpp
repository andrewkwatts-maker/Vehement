#pragma once

#include "../entities/Entity.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace Vehement {

// Forward declarations
class World;
class EntityManager;
class TileMap;

/**
 * @brief Entity spawn parameters
 */
struct EntitySpawnParams {
    EntityType type = EntityType::NPC;
    std::string faction;                ///< Faction/team identifier
    int level = 1;                      ///< Entity level
    float health = 100.0f;              ///< Starting health
    std::vector<std::string> inventory; ///< Starting inventory items
    std::string behaviorScript;         ///< AI behavior script name
    bool isHostile = false;             ///< Hostile to player
    bool respawns = false;              ///< Whether entity respawns
    float respawnTime = 60.0f;          ///< Respawn time in seconds
};

/**
 * @brief Patrol waypoint
 */
struct PatrolWaypoint {
    glm::vec3 position{0.0f};
    float waitTime = 0.0f;              ///< Time to wait at waypoint
    std::string action;                 ///< Action to perform at waypoint
};

/**
 * @brief Patrol path definition
 */
struct PatrolPath {
    std::string name;
    std::vector<PatrolWaypoint> waypoints;
    bool loop = true;                   ///< Loop back to start
    bool pingPong = false;              ///< Go back and forth
    float speed = 1.0f;                 ///< Movement speed multiplier
};

/**
 * @brief Spawn zone definition
 */
struct SpawnZone {
    std::string name;
    glm::vec3 center{0.0f};
    glm::vec3 size{10.0f, 5.0f, 10.0f}; ///< Zone dimensions
    int maxEntities = 5;                ///< Maximum entities in zone
    float spawnInterval = 30.0f;        ///< Spawn interval in seconds
    EntitySpawnParams spawnParams;      ///< Parameters for spawned entities
    bool enabled = true;
    std::string triggerCondition;       ///< Condition to activate spawning
};

/**
 * @brief Editor mode for entity placer
 */
enum class EntityPlacerMode : uint8_t {
    PlaceEntity,    ///< Place individual entities
    EditPatrol,     ///< Edit patrol paths
    EditSpawnZone   ///< Edit spawn zones
};

/**
 * @brief Manual entity placement tool for world editing
 *
 * Features:
 * - Select entity type (units, NPCs, resources)
 * - Preview at cursor position
 * - Configure spawn parameters (faction, level, inventory)
 * - Patrol path editor
 * - Spawn zone editor (area that spawns entities)
 *
 * Usage:
 * 1. Select entity type
 * 2. Configure spawn parameters
 * 3. Position preview at desired location
 * 4. Place entity or define patrol/spawn zone
 */
class EntityPlacer {
public:
    EntityPlacer();
    ~EntityPlacer() = default;

    // Allow copy and move
    EntityPlacer(const EntityPlacer&) = default;
    EntityPlacer& operator=(const EntityPlacer&) = default;
    EntityPlacer(EntityPlacer&&) noexcept = default;
    EntityPlacer& operator=(EntityPlacer&&) noexcept = default;

    // =========================================================================
    // Mode Selection
    // =========================================================================

    /** @brief Get current editor mode */
    [[nodiscard]] EntityPlacerMode GetMode() const noexcept { return m_mode; }

    /** @brief Set editor mode */
    void SetMode(EntityPlacerMode mode) noexcept { m_mode = mode; }

    // =========================================================================
    // Entity Type Selection
    // =========================================================================

    /** @brief Get available entity types */
    [[nodiscard]] std::vector<EntityType> GetAvailableTypes() const;

    /** @brief Get selected entity type */
    [[nodiscard]] EntityType GetSelectedType() const noexcept { return m_selectedType; }

    /** @brief Set selected entity type */
    void SetSelectedType(EntityType type);

    /** @brief Get entity templates for selected type */
    [[nodiscard]] std::vector<std::string> GetTemplates() const;

    /** @brief Get selected template name */
    [[nodiscard]] const std::string& GetSelectedTemplate() const noexcept {
        return m_selectedTemplate;
    }

    /** @brief Set selected template */
    void SetSelectedTemplate(const std::string& templateName);

    // =========================================================================
    // Spawn Parameters
    // =========================================================================

    /** @brief Get current spawn parameters */
    [[nodiscard]] const EntitySpawnParams& GetSpawnParams() const noexcept {
        return m_spawnParams;
    }

    /** @brief Get mutable spawn parameters */
    [[nodiscard]] EntitySpawnParams& GetSpawnParams() noexcept { return m_spawnParams; }

    /** @brief Set spawn parameters */
    void SetSpawnParams(const EntitySpawnParams& params) noexcept { m_spawnParams = params; }

    /** @brief Reset spawn parameters to defaults */
    void ResetSpawnParams();

    // =========================================================================
    // Preview Position
    // =========================================================================

    /** @brief Get preview position */
    [[nodiscard]] const glm::vec3& GetPreviewPosition() const noexcept {
        return m_previewPosition;
    }

    /** @brief Set preview position */
    void SetPreviewPosition(const glm::vec3& position);

    /** @brief Get preview rotation */
    [[nodiscard]] float GetPreviewRotation() const noexcept { return m_previewRotation; }

    /** @brief Set preview rotation */
    void SetPreviewRotation(float radians) noexcept { m_previewRotation = radians; }

    /** @brief Check if preview is active */
    [[nodiscard]] bool HasPreview() const noexcept { return m_previewActive; }

    /** @brief Set preview active state */
    void SetPreviewActive(bool active) noexcept { m_previewActive = active; }

    // =========================================================================
    // Entity Placement
    // =========================================================================

    /**
     * @brief Validate entity placement
     * @param map Tile map to check
     * @return true if placement is valid
     */
    [[nodiscard]] bool ValidatePlacement(const TileMap& map) const;

    /**
     * @brief Place entity at current preview position
     * @param entityManager Entity manager to add entity to
     * @return Pointer to placed entity, or nullptr on failure
     */
    Entity* PlaceEntity(EntityManager& entityManager);

    /**
     * @brief Create preview entity (for rendering)
     */
    [[nodiscard]] std::unique_ptr<Entity> CreatePreviewEntity() const;

    // =========================================================================
    // Patrol Path Editor
    // =========================================================================

    /** @brief Start new patrol path */
    void StartNewPatrolPath(const std::string& name);

    /** @brief Add waypoint to current patrol path */
    void AddPatrolWaypoint(const glm::vec3& position, float waitTime = 0.0f);

    /** @brief Remove last waypoint */
    void RemoveLastWaypoint();

    /** @brief Clear current patrol path */
    void ClearPatrolPath();

    /** @brief Finish and save current patrol path */
    void FinishPatrolPath();

    /** @brief Get current patrol path being edited */
    [[nodiscard]] const PatrolPath& GetCurrentPatrolPath() const noexcept {
        return m_currentPatrolPath;
    }

    /** @brief Get all defined patrol paths */
    [[nodiscard]] const std::vector<PatrolPath>& GetPatrolPaths() const noexcept {
        return m_patrolPaths;
    }

    /** @brief Delete a patrol path by name */
    bool DeletePatrolPath(const std::string& name);

    /** @brief Assign patrol path to an entity */
    void AssignPatrolPath(Entity& entity, const std::string& pathName);

    // =========================================================================
    // Spawn Zone Editor
    // =========================================================================

    /** @brief Start new spawn zone */
    void StartNewSpawnZone(const std::string& name);

    /** @brief Set spawn zone center */
    void SetSpawnZoneCenter(const glm::vec3& center);

    /** @brief Set spawn zone size */
    void SetSpawnZoneSize(const glm::vec3& size);

    /** @brief Finish and save current spawn zone */
    void FinishSpawnZone();

    /** @brief Get current spawn zone being edited */
    [[nodiscard]] const SpawnZone& GetCurrentSpawnZone() const noexcept {
        return m_currentSpawnZone;
    }

    /** @brief Get mutable current spawn zone */
    [[nodiscard]] SpawnZone& GetCurrentSpawnZone() noexcept { return m_currentSpawnZone; }

    /** @brief Get all defined spawn zones */
    [[nodiscard]] const std::vector<SpawnZone>& GetSpawnZones() const noexcept {
        return m_spawnZones;
    }

    /** @brief Delete a spawn zone by name */
    bool DeleteSpawnZone(const std::string& name);

    /** @brief Enable/disable a spawn zone */
    void SetSpawnZoneEnabled(const std::string& name, bool enabled);

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Save patrol paths to JSON
     */
    [[nodiscard]] std::string SavePatrolPathsToJson() const;

    /**
     * @brief Load patrol paths from JSON
     */
    bool LoadPatrolPathsFromJson(const std::string& json);

    /**
     * @brief Save spawn zones to JSON
     */
    [[nodiscard]] std::string SaveSpawnZonesToJson() const;

    /**
     * @brief Load spawn zones from JSON
     */
    bool LoadSpawnZonesFromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using EntityCallback = std::function<void(Entity&)>;
    using PathCallback = std::function<void(const PatrolPath&)>;
    using ZoneCallback = std::function<void(const SpawnZone&)>;

    void SetOnEntityPlaced(EntityCallback callback) { m_onEntityPlaced = std::move(callback); }
    void SetOnPatrolPathCreated(PathCallback callback) { m_onPatrolPathCreated = std::move(callback); }
    void SetOnSpawnZoneCreated(ZoneCallback callback) { m_onSpawnZoneCreated = std::move(callback); }

private:
    // Mode
    EntityPlacerMode m_mode = EntityPlacerMode::PlaceEntity;

    // Entity selection
    EntityType m_selectedType = EntityType::NPC;
    std::string m_selectedTemplate;
    EntitySpawnParams m_spawnParams;

    // Preview
    glm::vec3 m_previewPosition{0.0f};
    float m_previewRotation = 0.0f;
    bool m_previewActive = false;

    // Patrol paths
    PatrolPath m_currentPatrolPath;
    std::vector<PatrolPath> m_patrolPaths;
    bool m_editingPatrolPath = false;

    // Spawn zones
    SpawnZone m_currentSpawnZone;
    std::vector<SpawnZone> m_spawnZones;
    bool m_editingSpawnZone = false;

    // Callbacks
    EntityCallback m_onEntityPlaced;
    PathCallback m_onPatrolPathCreated;
    ZoneCallback m_onSpawnZoneCreated;
};

} // namespace Vehement
