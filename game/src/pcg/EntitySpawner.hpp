#pragma once

#include "PCGPipeline.hpp"
#include <vector>

namespace Vehement {

/**
 * @brief Entity spawn rule
 */
struct EntitySpawnRule {
    std::string entityType;             // Entity type identifier
    std::vector<BiomeType> validBiomes;
    std::vector<BuildingType> validBuildings;  // Spawn near/in buildings
    float spawnChance = 0.1f;           // Base spawn probability
    float minDistance = 5.0f;           // Minimum distance from same type
    int maxPerArea = 10;                // Max spawns per 100x100 area
    bool requiresIndoor = false;        // Must be inside building
    bool requiresOutdoor = true;        // Must be outside
    bool requiresRoadAccess = false;    // Must be near road
    float roadProximity = 10.0f;        // Distance for road requirement
    std::unordered_map<std::string, std::string> defaultProperties;
};

/**
 * @brief Entity spawner parameters
 */
struct EntitySpawnerParams {
    // Population
    float populationDensity = 1.0f;     // Multiplier for spawn rates
    float urbanMultiplier = 2.0f;       // Extra spawns in urban areas
    float dangerMultiplier = 1.5f;      // Extra enemies in danger zones

    // NPCs
    bool spawnNPCs = true;
    float npcDensity = 0.1f;            // NPCs per tile in valid areas
    std::vector<std::string> npcTypes = {"civilian", "merchant", "guard"};

    // Enemies
    bool spawnEnemies = true;
    float enemyDensity = 0.05f;
    float minPlayerDistance = 20.0f;    // Min distance from player spawn
    std::vector<std::string> enemyTypes = {"zombie", "zombie_runner"};

    // Resources
    bool spawnResources = true;
    float resourceDensity = 0.02f;
    std::vector<std::string> resourceTypes = {"ammo_crate", "health_pack", "loot_box"};

    // Wildlife
    bool spawnWildlife = true;
    float wildlifeDensity = 0.03f;
    std::vector<std::string> wildlifeTypes = {"crow", "rat", "stray_dog"};

    // Spawn rules
    std::vector<EntitySpawnRule> customRules;
};

/**
 * @brief NPC and resource spawning
 *
 * Generates:
 * - Spawn points based on building types
 * - Population density distribution
 * - Resource node placement
 * - Enemy/wildlife spawn zones
 *
 * Python script hook: entity_*.py
 */
class EntitySpawner : public PCGStageGenerator {
public:
    EntitySpawner();
    ~EntitySpawner() override = default;

    // PCGStageGenerator interface
    PCGStageResult Generate(PCGContext& context, PCGMode mode) override;
    PCGStage GetStage() const override { return PCGStage::Entities; }
    const char* GetName() const override { return "EntitySpawner"; }

    // Configuration
    void SetParams(const EntitySpawnerParams& params) { m_params = params; }
    EntitySpawnerParams& GetParams() { return m_params; }
    const EntitySpawnerParams& GetParams() const { return m_params; }

    // Spawn rule management
    void AddSpawnRule(const EntitySpawnRule& rule);
    void ClearSpawnRules();
    const std::vector<EntitySpawnRule>& GetSpawnRules() const { return m_params.customRules; }

    // Individual generation steps

    /**
     * @brief Generate NPC spawn points
     */
    void GenerateNPCSpawns(PCGContext& context);

    /**
     * @brief Generate enemy spawn points
     */
    void GenerateEnemySpawns(PCGContext& context);

    /**
     * @brief Generate resource spawn points
     */
    void GenerateResourceSpawns(PCGContext& context);

    /**
     * @brief Generate wildlife spawn points
     */
    void GenerateWildlifeSpawns(PCGContext& context);

    /**
     * @brief Apply custom spawn rules
     */
    void ApplyCustomRules(PCGContext& context);

    /**
     * @brief Find valid spawn positions for entity type
     */
    std::vector<glm::ivec2> FindSpawnPositions(PCGContext& context,
                                                 const EntitySpawnRule& rule,
                                                 int maxCount);

    /**
     * @brief Check if position is valid for spawn rule
     */
    bool IsValidSpawnPosition(PCGContext& context, int x, int y,
                               const EntitySpawnRule& rule);

    /**
     * @brief Get spawn density at position
     */
    float GetSpawnDensity(PCGContext& context, int x, int y) const;

private:
    EntitySpawnerParams m_params;

    // Default rules
    void InitializeDefaultRules();

    // Helpers
    void SpawnEntitiesWithDensity(PCGContext& context,
                                   const std::vector<std::string>& types,
                                   float density,
                                   const EntitySpawnRule& baseRule);
    bool CheckEntitySpacing(PCGContext& context, int x, int y,
                            const std::string& entityType, float minDist);
    std::string SelectEntityType(PCGContext& context,
                                  const std::vector<std::string>& types);
};

} // namespace Vehement
