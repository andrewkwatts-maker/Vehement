#include "EntitySpawner.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

EntitySpawner::EntitySpawner() {
    InitializeDefaultRules();
}

void EntitySpawner::InitializeDefaultRules() {
    // Civilian NPC rule
    EntitySpawnRule civilianRule;
    civilianRule.entityType = "civilian";
    civilianRule.validBiomes = {BiomeType::Urban, BiomeType::Suburban, BiomeType::Commercial,
                                 BiomeType::Residential};
    civilianRule.spawnChance = 0.05f;
    civilianRule.minDistance = 10.0f;
    civilianRule.maxPerArea = 20;
    civilianRule.requiresOutdoor = true;
    civilianRule.requiresRoadAccess = true;
    civilianRule.roadProximity = 5.0f;
    m_params.customRules.push_back(civilianRule);

    // Zombie rule
    EntitySpawnRule zombieRule;
    zombieRule.entityType = "zombie";
    zombieRule.validBiomes = {BiomeType::Urban, BiomeType::Suburban, BiomeType::Industrial};
    zombieRule.spawnChance = 0.03f;
    zombieRule.minDistance = 8.0f;
    zombieRule.maxPerArea = 15;
    zombieRule.requiresOutdoor = true;
    m_params.customRules.push_back(zombieRule);

    // Loot box rule
    EntitySpawnRule lootRule;
    lootRule.entityType = "loot_box";
    lootRule.validBiomes = {BiomeType::Urban, BiomeType::Commercial, BiomeType::Industrial};
    lootRule.validBuildings = {BuildingType::Shop, BuildingType::Warehouse, BuildingType::Factory};
    lootRule.spawnChance = 0.02f;
    lootRule.minDistance = 15.0f;
    lootRule.maxPerArea = 5;
    lootRule.requiresIndoor = true;
    m_params.customRules.push_back(lootRule);

    // Wildlife rule
    EntitySpawnRule wildlifeRule;
    wildlifeRule.entityType = "crow";
    wildlifeRule.validBiomes = {BiomeType::Forest, BiomeType::Park, BiomeType::Rural, BiomeType::Grassland};
    wildlifeRule.spawnChance = 0.04f;
    wildlifeRule.minDistance = 20.0f;
    wildlifeRule.maxPerArea = 10;
    wildlifeRule.requiresOutdoor = true;
    m_params.customRules.push_back(wildlifeRule);
}

PCGStageResult EntitySpawner::Generate(PCGContext& context, PCGMode mode) {
    PCGStageResult result;
    result.success = true;

    // Update params from string parameters
    m_params.populationDensity = GetParamFloat("populationDensity", m_params.populationDensity);
    m_params.spawnNPCs = GetParamBool("spawnNPCs", m_params.spawnNPCs);
    m_params.spawnEnemies = GetParamBool("spawnEnemies", m_params.spawnEnemies);
    m_params.spawnResources = GetParamBool("spawnResources", m_params.spawnResources);

    try {
        // Generate spawns by category
        if (m_params.spawnNPCs) {
            GenerateNPCSpawns(context);
        }

        if (m_params.spawnEnemies) {
            GenerateEnemySpawns(context);
        }

        if (m_params.spawnResources) {
            GenerateResourceSpawns(context);
        }

        if (m_params.spawnWildlife && mode == PCGMode::Final) {
            GenerateWildlifeSpawns(context);
        }

        // Apply custom rules
        ApplyCustomRules(context);

        result.itemsGenerated = static_cast<int>(context.GetEntitySpawns().size());

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

void EntitySpawner::AddSpawnRule(const EntitySpawnRule& rule) {
    m_params.customRules.push_back(rule);
}

void EntitySpawner::ClearSpawnRules() {
    m_params.customRules.clear();
}

void EntitySpawner::GenerateNPCSpawns(PCGContext& context) {
    EntitySpawnRule npcRule;
    npcRule.validBiomes = {BiomeType::Urban, BiomeType::Suburban, BiomeType::Commercial,
                           BiomeType::Residential};
    npcRule.spawnChance = m_params.npcDensity * m_params.populationDensity;
    npcRule.minDistance = 8.0f;
    npcRule.requiresOutdoor = true;
    npcRule.requiresRoadAccess = true;
    npcRule.roadProximity = 5.0f;

    SpawnEntitiesWithDensity(context, m_params.npcTypes, m_params.npcDensity, npcRule);
}

void EntitySpawner::GenerateEnemySpawns(PCGContext& context) {
    EntitySpawnRule enemyRule;
    enemyRule.validBiomes = {BiomeType::Urban, BiomeType::Suburban, BiomeType::Industrial,
                             BiomeType::Commercial};
    enemyRule.spawnChance = m_params.enemyDensity * m_params.populationDensity;
    enemyRule.minDistance = 10.0f;
    enemyRule.requiresOutdoor = true;

    SpawnEntitiesWithDensity(context, m_params.enemyTypes, m_params.enemyDensity, enemyRule);
}

void EntitySpawner::GenerateResourceSpawns(PCGContext& context) {
    EntitySpawnRule resourceRule;
    resourceRule.validBiomes = {BiomeType::Urban, BiomeType::Commercial, BiomeType::Industrial,
                                BiomeType::Residential};
    resourceRule.spawnChance = m_params.resourceDensity;
    resourceRule.minDistance = 20.0f;

    SpawnEntitiesWithDensity(context, m_params.resourceTypes, m_params.resourceDensity, resourceRule);
}

void EntitySpawner::GenerateWildlifeSpawns(PCGContext& context) {
    EntitySpawnRule wildlifeRule;
    wildlifeRule.validBiomes = {BiomeType::Forest, BiomeType::Park, BiomeType::Rural,
                                BiomeType::Grassland, BiomeType::Wetland};
    wildlifeRule.spawnChance = m_params.wildlifeDensity;
    wildlifeRule.minDistance = 15.0f;
    wildlifeRule.requiresOutdoor = true;

    SpawnEntitiesWithDensity(context, m_params.wildlifeTypes, m_params.wildlifeDensity, wildlifeRule);
}

void EntitySpawner::ApplyCustomRules(PCGContext& context) {
    for (const auto& rule : m_params.customRules) {
        // Skip rules for entity types already handled
        bool alreadyHandled = false;
        for (const auto& type : m_params.npcTypes) {
            if (type == rule.entityType) { alreadyHandled = true; break; }
        }
        for (const auto& type : m_params.enemyTypes) {
            if (type == rule.entityType) { alreadyHandled = true; break; }
        }

        if (alreadyHandled) continue;

        // Find positions and spawn
        auto positions = FindSpawnPositions(context, rule, rule.maxPerArea);
        for (const auto& pos : positions) {
            if (context.Random() < rule.spawnChance * m_params.populationDensity) {
                context.SpawnEntity(pos.x, pos.y, rule.entityType, rule.defaultProperties);
            }
        }
    }
}

void EntitySpawner::SpawnEntitiesWithDensity(PCGContext& context,
                                               const std::vector<std::string>& types,
                                               float density,
                                               const EntitySpawnRule& baseRule) {
    if (types.empty()) return;

    int width = context.GetWidth();
    int height = context.GetHeight();

    // Calculate target spawn count
    float area = static_cast<float>(width * height);
    int targetCount = static_cast<int>(area * density * m_params.populationDensity * 0.01f);

    int spawned = 0;
    int attempts = 0;
    int maxAttempts = targetCount * 10;

    while (spawned < targetCount && attempts < maxAttempts) {
        attempts++;

        int x = context.RandomInt(0, width - 1);
        int y = context.RandomInt(0, height - 1);

        if (!IsValidSpawnPosition(context, x, y, baseRule)) continue;

        std::string entityType = SelectEntityType(context, types);
        if (!CheckEntitySpacing(context, x, y, entityType, baseRule.minDistance)) continue;

        context.SpawnEntity(x, y, entityType, baseRule.defaultProperties);
        spawned++;
    }
}

std::vector<glm::ivec2> EntitySpawner::FindSpawnPositions(PCGContext& context,
                                                            const EntitySpawnRule& rule,
                                                            int maxCount) {
    std::vector<glm::ivec2> positions;
    int width = context.GetWidth();
    int height = context.GetHeight();

    int attempts = 0;
    int maxAttempts = maxCount * 20;

    while (static_cast<int>(positions.size()) < maxCount && attempts < maxAttempts) {
        attempts++;

        int x = context.RandomInt(0, width - 1);
        int y = context.RandomInt(0, height - 1);

        if (IsValidSpawnPosition(context, x, y, rule)) {
            // Check spacing with already found positions
            bool tooClose = false;
            for (const auto& pos : positions) {
                float dist = context.Distance(x, y, pos.x, pos.y);
                if (dist < rule.minDistance) {
                    tooClose = true;
                    break;
                }
            }

            if (!tooClose) {
                positions.push_back({x, y});
            }
        }
    }

    return positions;
}

bool EntitySpawner::IsValidSpawnPosition(PCGContext& context, int x, int y,
                                           const EntitySpawnRule& rule) {
    // Check bounds
    if (!context.InBounds(x, y)) return false;

    // Check biome
    if (!rule.validBiomes.empty()) {
        BiomeType biome = context.GetBiome(x, y);
        bool validBiome = false;
        for (BiomeType valid : rule.validBiomes) {
            if (valid == biome) {
                validBiome = true;
                break;
            }
        }
        if (!validBiome) return false;
    }

    // Check indoor/outdoor requirements
    const GeoBuilding* building = context.GetBuilding(x, y);
    bool isIndoor = (building != nullptr);

    if (rule.requiresIndoor && !isIndoor) return false;
    if (rule.requiresOutdoor && isIndoor) return false;

    // Check building type requirements
    if (!rule.validBuildings.empty() && building) {
        bool validBuilding = false;
        for (BuildingType valid : rule.validBuildings) {
            if (valid == building->type) {
                validBuilding = true;
                break;
            }
        }
        if (!validBuilding) return false;
    }

    // Check road access
    if (rule.requiresRoadAccess) {
        bool hasRoadAccess = false;
        int checkRadius = static_cast<int>(rule.roadProximity);

        for (int dy = -checkRadius; dy <= checkRadius && !hasRoadAccess; ++dy) {
            for (int dx = -checkRadius; dx <= checkRadius && !hasRoadAccess; ++dx) {
                if (context.InBounds(x + dx, y + dy) && context.IsRoad(x + dx, y + dy)) {
                    hasRoadAccess = true;
                }
            }
        }

        if (!hasRoadAccess) return false;
    }

    // Check for water
    if (context.IsWater(x, y)) return false;

    // Check walkability
    if (!context.IsWalkable(x, y)) return false;

    return true;
}

float EntitySpawner::GetSpawnDensity(PCGContext& context, int x, int y) const {
    if (!context.InBounds(x, y)) return 0.0f;

    BiomeType biome = context.GetBiome(x, y);
    float density = m_params.populationDensity;

    // Modify by biome
    switch (biome) {
        case BiomeType::Urban:
        case BiomeType::Commercial:
            density *= m_params.urbanMultiplier;
            break;
        case BiomeType::Industrial:
            density *= m_params.dangerMultiplier;
            break;
        case BiomeType::Forest:
        case BiomeType::Rural:
            density *= 0.5f;
            break;
        default:
            break;
    }

    // Modify by population density data
    float popDensity = context.GetPopulationDensity(x, y);
    if (popDensity > 0.0f) {
        density *= (1.0f + popDensity * 0.001f);  // Scale factor for population
    }

    return density;
}

bool EntitySpawner::CheckEntitySpacing(PCGContext& context, int x, int y,
                                         const std::string& entityType, float minDist) {
    const auto& spawns = context.GetEntitySpawns();

    for (const auto& spawn : spawns) {
        // Check same type spacing
        if (spawn.entityType == entityType) {
            float dx = spawn.position.x - (static_cast<float>(x) + 0.5f);
            float dy = spawn.position.z - (static_cast<float>(y) + 0.5f);
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < minDist) return false;
        }
    }

    return true;
}

std::string EntitySpawner::SelectEntityType(PCGContext& context,
                                             const std::vector<std::string>& types) {
    if (types.empty()) return "";
    int idx = context.RandomInt(0, static_cast<int>(types.size()) - 1);
    return types[idx];
}

} // namespace Vehement
