#pragma once

#include "Tile.hpp"
#include "TileMap.hpp"
#include "TileAtlas.hpp"
#include "TileRenderer.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Camera;
    class Graph;
}

namespace Vehement {

// Forward declarations
class Entity;

/**
 * @brief Zone type for gameplay areas
 */
enum class ZoneType : uint8_t {
    None = 0,
    SafeZone,       // Players cannot be attacked here
    SpawnZone,      // Enemies spawn here
    DangerZone,     // Increased enemy spawn rate
    LootZone,       // Better loot drops
    ObjectiveZone,  // Mission objectives
    ExitZone        // Level exit
};

/**
 * @brief Spawn point configuration
 */
struct SpawnPoint {
    glm::vec3 position{0.0f};
    float radius = 1.0f;           // Spawn radius for random offset
    std::string tag;               // Identifier tag (e.g., "player", "zombie", "item")
    bool enabled = true;
    int maxSpawns = -1;            // Max entities to spawn (-1 = unlimited)
    float respawnTime = 0.0f;      // Time between spawns
    float lastSpawnTime = 0.0f;    // Time since last spawn

    /**
     * @brief Get a random position within the spawn radius
     */
    glm::vec3 GetRandomPosition() const;
};

/**
 * @brief Zone definition for gameplay areas
 */
struct Zone {
    std::string name;
    ZoneType type = ZoneType::None;
    glm::vec3 min{0.0f};           // AABB min corner
    glm::vec3 max{0.0f};           // AABB max corner
    bool active = true;

    // Zone-specific properties
    float dangerLevel = 0.0f;      // 0-1 danger multiplier
    float lootMultiplier = 1.0f;   // Loot quality/quantity multiplier

    /**
     * @brief Check if a point is inside the zone
     */
    bool Contains(const glm::vec3& point) const;

    /**
     * @brief Check if a sphere intersects the zone
     */
    bool Intersects(const glm::vec3& center, float radius) const;

    /**
     * @brief Get zone center
     */
    glm::vec3 GetCenter() const { return (min + max) * 0.5f; }

    /**
     * @brief Get zone size
     */
    glm::vec3 GetSize() const { return max - min; }
};

/**
 * @brief Configuration for world creation
 */
struct WorldConfig {
    int mapWidth = 64;
    int mapHeight = 64;
    float tileSize = 1.0f;
    std::string textureBasePath = "Vehement2/images/";
    bool enableChunks = false;
    TileType defaultGroundTile = TileType::GroundGrass1;
};

/**
 * @brief Collision result from world queries
 */
struct CollisionResult {
    bool hit = false;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    float distance = 0.0f;
    int tileX = -1;
    int tileY = -1;
    const Tile* tile = nullptr;
};

/**
 * @brief Main world container
 *
 * Contains and manages:
 * - TileMap for terrain
 * - Entity list
 * - Spawn points
 * - Zone definitions
 * - World update (entity movement, collisions)
 */
class World {
public:
    World();
    ~World();

    // Non-copyable
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /**
     * @brief Initialize the world
     * @param renderer The Nova renderer
     * @param config World configuration
     */
    bool Initialize(Nova::Renderer& renderer, const WorldConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update world state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the world
     * @param camera The camera for rendering
     */
    void Render(const Nova::Camera& camera);

    // ========== Tile Map Access ==========

    TileMap& GetTileMap() { return m_tileMap; }
    const TileMap& GetTileMap() const { return m_tileMap; }

    TileAtlas& GetTileAtlas() { return m_tileAtlas; }
    const TileAtlas& GetTileAtlas() const { return m_tileAtlas; }

    TileRenderer& GetTileRenderer() { return m_tileRenderer; }
    const TileRenderer& GetTileRenderer() const { return m_tileRenderer; }

    // ========== Entity Management ==========

    /**
     * @brief Add an entity to the world
     * @return Entity ID
     */
    uint32_t AddEntity(std::shared_ptr<Entity> entity);

    /**
     * @brief Remove an entity from the world
     */
    void RemoveEntity(uint32_t entityId);

    /**
     * @brief Get entity by ID
     */
    std::shared_ptr<Entity> GetEntity(uint32_t entityId);

    /**
     * @brief Get all entities
     */
    const std::vector<std::shared_ptr<Entity>>& GetEntities() const { return m_entities; }

    /**
     * @brief Get entities in radius
     */
    std::vector<std::shared_ptr<Entity>> GetEntitiesInRadius(
        const glm::vec3& center, float radius) const;

    /**
     * @brief Get entities in zone
     */
    std::vector<std::shared_ptr<Entity>> GetEntitiesInZone(const Zone& zone) const;

    // ========== Spawn Points ==========

    /**
     * @brief Add a spawn point
     */
    void AddSpawnPoint(const SpawnPoint& spawnPoint);

    /**
     * @brief Get spawn points by tag
     */
    std::vector<SpawnPoint*> GetSpawnPoints(const std::string& tag = "");

    /**
     * @brief Get random spawn point
     */
    SpawnPoint* GetRandomSpawnPoint(const std::string& tag = "");

    /**
     * @brief Clear all spawn points
     */
    void ClearSpawnPoints();

    // ========== Zones ==========

    /**
     * @brief Add a zone
     */
    void AddZone(const Zone& zone);

    /**
     * @brief Get zone at position
     */
    Zone* GetZoneAt(const glm::vec3& position);

    /**
     * @brief Get all zones of a type
     */
    std::vector<Zone*> GetZones(ZoneType type = ZoneType::None);

    /**
     * @brief Check if position is in safe zone
     */
    bool IsInSafeZone(const glm::vec3& position) const;

    /**
     * @brief Get danger level at position (0-1)
     */
    float GetDangerLevel(const glm::vec3& position) const;

    /**
     * @brief Clear all zones
     */
    void ClearZones();

    // ========== Collision & Physics ==========

    /**
     * @brief Check if a position is walkable
     */
    bool IsWalkable(const glm::vec3& position) const;

    /**
     * @brief Check collision with world geometry
     */
    CollisionResult CheckCollision(const glm::vec3& start, const glm::vec3& end) const;

    /**
     * @brief Check collision with a sphere
     */
    CollisionResult CheckSphereCollision(const glm::vec3& center, float radius) const;

    /**
     * @brief Resolve movement collision (slide along walls)
     * @param position Current position
     * @param velocity Desired velocity
     * @param radius Entity radius
     * @return Resolved velocity after collision
     */
    glm::vec3 ResolveCollision(const glm::vec3& position, const glm::vec3& velocity,
                                float radius) const;

    /**
     * @brief Cast a ray through the world
     * @param origin Ray origin
     * @param direction Ray direction (normalized)
     * @param maxDistance Maximum ray distance
     * @return Collision result
     */
    CollisionResult Raycast(const glm::vec3& origin, const glm::vec3& direction,
                           float maxDistance = 1000.0f) const;

    /**
     * @brief Check line of sight between two points
     */
    bool HasLineOfSight(const glm::vec3& from, const glm::vec3& to) const;

    // ========== Pathfinding ==========

    /**
     * @brief Get the navigation graph
     */
    Nova::Graph& GetNavigationGraph() { return *m_navGraph; }
    const Nova::Graph& GetNavigationGraph() const { return *m_navGraph; }

    /**
     * @brief Rebuild navigation graph
     */
    void RebuildNavigationGraph();

    /**
     * @brief Find path between world positions
     * @param from Start position
     * @param to End position
     * @return Vector of waypoints, empty if no path
     */
    std::vector<glm::vec3> FindPath(const glm::vec3& from, const glm::vec3& to) const;

    // ========== Serialization ==========

    /**
     * @brief Save world to JSON
     */
    std::string SaveToJson() const;

    /**
     * @brief Load world from JSON
     */
    bool LoadFromJson(const std::string& json);

    /**
     * @brief Save world to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Load world from file
     */
    bool LoadFromFile(const std::string& filepath);

    // ========== Utility ==========

    /**
     * @brief Get world bounds
     */
    glm::vec2 GetWorldMin() const { return m_tileMap.GetWorldMin(); }
    glm::vec2 GetWorldMax() const { return m_tileMap.GetWorldMax(); }

    /**
     * @brief Clamp position to world bounds
     */
    glm::vec3 ClampToWorld(const glm::vec3& position) const;

    /**
     * @brief Get random walkable position
     */
    glm::vec3 GetRandomWalkablePosition() const;

    /**
     * @brief Set entity update callback
     */
    using EntityUpdateCallback = std::function<void(Entity&, float)>;
    void SetEntityUpdateCallback(EntityUpdateCallback callback) {
        m_entityUpdateCallback = callback;
    }

private:
    /**
     * @brief Update entity positions with collision
     */
    void UpdateEntities(float deltaTime);

    /**
     * @brief Process spawns
     */
    void UpdateSpawns(float deltaTime);

    /**
     * @brief Check collision between entity and tile grid
     */
    bool CheckTileCollision(const glm::vec3& position, float radius) const;

    Nova::Renderer* m_renderer = nullptr;
    WorldConfig m_config;
    bool m_initialized = false;

    // Core components
    TileMap m_tileMap;
    TileAtlas m_tileAtlas;
    TileRenderer m_tileRenderer;

    // Navigation
    std::unique_ptr<Nova::Graph> m_navGraph;
    bool m_navGraphDirty = true;

    // Entities
    std::vector<std::shared_ptr<Entity>> m_entities;
    uint32_t m_nextEntityId = 1;

    // Spawn points
    std::vector<SpawnPoint> m_spawnPoints;

    // Zones
    std::vector<Zone> m_zones;

    // Callbacks
    EntityUpdateCallback m_entityUpdateCallback;

    // Time tracking
    float m_totalTime = 0.0f;
};

} // namespace Vehement
