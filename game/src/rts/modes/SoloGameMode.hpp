#pragma once

#include "../../world/World.hpp"
#include "../../pcg/TerrainGenerator.hpp"
#include "../../pcg/EntitySpawner.hpp"
#include "../Resource.hpp"
#include <memory>
#include <glm/glm.hpp>

namespace Vehement {

/**
 * @brief Configuration for Solo Game 1v1 match
 */
struct SoloGameConfig {
    int mapWidth = 128;
    int mapHeight = 128;
    float tileSize = 1.0f;

    // Map generation
    uint64_t seed = 0;  // 0 = random
    bool generateTerrain = true;
    bool generateResources = true;

    // Resource density
    float treeDensity = 0.15f;        // 15% of walkable tiles
    float rockDensity = 0.08f;        // 8% of walkable tiles
    float goldDensity = 0.03f;        // 3% of walkable tiles - rare

    // Starting resources per player
    int startingFood = 200;
    int startingWood = 150;
    int startingStone = 100;
    int startingMetal = 50;
    int startingCoins = 0;

    // Player spawn distance
    float minPlayerDistance = 60.0f;  // Minimum distance between spawns

    // AI difficulty (for player 2)
    std::string aiDifficulty = "medium";  // easy, medium, hard
};

/**
 * @brief Resource node on the map
 */
struct ResourceNode {
    glm::vec3 position;
    RTS::ResourceType type;
    int amount;
    float gatherRate;
    std::string entityType;  // For spawning visual representation
};

/**
 * @brief Player spawn point with initial resources
 */
struct PlayerSpawn {
    int playerId;
    glm::vec3 position;
    float radius = 5.0f;
    RTS::ResourceStock startingResources;
    std::vector<std::string> startingUnits;  // Unit types to spawn
    std::vector<std::string> startingBuildings;  // Building types to spawn
};

/**
 * @brief Solo Game Mode - 1v1 Human vs AI
 *
 * Features:
 * - Procedurally generated map
 * - Resource placement (trees, rocks, gold)
 * - Two starting positions (human on one side, AI on other)
 * - Basic RTS setup with starting resources
 *
 * Map Layout:
 * - Flat terrain with grass and dirt variation
 * - Resources scattered across map
 * - Extra resources near player starting areas
 * - Clear paths between bases
 */
class SoloGameMode {
public:
    SoloGameMode();
    ~SoloGameMode();

    /**
     * @brief Initialize the game mode with config
     */
    bool Initialize(const SoloGameConfig& config = SoloGameConfig());

    /**
     * @brief Generate the 1v1 map
     */
    bool GenerateMap(Nova::Renderer& renderer);

    /**
     * @brief Update the game state
     */
    void Update(float deltaTime);

    /**
     * @brief Render the game world
     */
    void Render(const Nova::Camera& camera);

    /**
     * @brief Cleanup and shutdown
     */
    void Shutdown();

    // Accessors
    World& GetWorld() { return *m_world; }
    const World& GetWorld() const { return *m_world; }

    const std::vector<PlayerSpawn>& GetPlayerSpawns() const { return m_playerSpawns; }
    const std::vector<ResourceNode>& GetResourceNodes() const { return m_resourceNodes; }

    const SoloGameConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get player spawn position
     */
    glm::vec3 GetPlayerSpawnPosition(int playerId) const;

    /**
     * @brief Check if map is generated
     */
    bool IsMapGenerated() const { return m_mapGenerated; }

private:
    /**
     * @brief Generate flat terrain with variation
     */
    void GenerateTerrain();

    /**
     * @brief Place resource nodes on the map
     */
    void PlaceResources();

    /**
     * @brief Setup player spawn points
     */
    void SetupPlayerSpawns();

    /**
     * @brief Place resources near a spawn point
     */
    void PlaceStartingResources(const glm::vec3& spawnPos);

    /**
     * @brief Place a specific resource cluster
     */
    void PlaceResourceCluster(const glm::vec3& center, RTS::ResourceType type,
                              int count, float radius);

    /**
     * @brief Check if position is valid for resource placement
     */
    bool IsValidResourcePosition(int x, int y) const;

    /**
     * @brief Get random walkable position
     */
    glm::ivec2 GetRandomWalkablePosition() const;

    /**
     * @brief Calculate distance between two tile positions
     */
    float TileDistance(int x1, int y1, int x2, int y2) const;

    SoloGameConfig m_config;
    std::unique_ptr<World> m_world;

    // PCG generators
    std::unique_ptr<TerrainGenerator> m_terrainGenerator;
    std::unique_ptr<EntitySpawner> m_entitySpawner;

    // Game data
    std::vector<PlayerSpawn> m_playerSpawns;
    std::vector<ResourceNode> m_resourceNodes;

    // State
    bool m_initialized = false;
    bool m_mapGenerated = false;
    float m_gameTime = 0.0f;

    // Random number generation (mutable since used by const methods like GetRandomWalkablePosition)
    mutable std::mt19937 m_rng;
};

} // namespace Vehement
