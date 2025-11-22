#include "FoliageGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

FoliageGenerator::FoliageGenerator() {
    InitializeDefaultTypes();
}

void FoliageGenerator::InitializeDefaultTypes() {
    // Oak trees - common in forests and parks
    FoliageType oak;
    oak.id = "tree_oak";
    oak.name = "Oak Tree";
    oak.validBiomes = {BiomeType::Forest, BiomeType::Park, BiomeType::Grassland, BiomeType::Rural};
    oak.minScale = 0.8f;
    oak.maxScale = 1.3f;
    oak.spacing = 3.0f;
    oak.clustered = true;
    oak.clusterRadius = 8.0f;
    oak.clusterCount = 5;
    m_params.foliageTypes.push_back(oak);

    // Pine trees - mountain and forest areas
    FoliageType pine;
    pine.id = "tree_pine";
    pine.name = "Pine Tree";
    pine.validBiomes = {BiomeType::Forest, BiomeType::Mountain};
    pine.minScale = 0.9f;
    pine.maxScale = 1.4f;
    pine.spacing = 2.5f;
    pine.clustered = true;
    pine.clusterRadius = 6.0f;
    pine.clusterCount = 4;
    m_params.foliageTypes.push_back(pine);

    // Bushes - everywhere except urban
    FoliageType bush;
    bush.id = "bush";
    bush.name = "Bush";
    bush.validBiomes = {BiomeType::Forest, BiomeType::Park, BiomeType::Grassland,
                        BiomeType::Rural, BiomeType::Suburban};
    bush.minScale = 0.6f;
    bush.maxScale = 1.0f;
    bush.spacing = 1.5f;
    bush.clustered = false;
    m_params.foliageTypes.push_back(bush);

    // Grass clumps
    FoliageType grass;
    grass.id = "grass_tall";
    grass.name = "Tall Grass";
    grass.validBiomes = {BiomeType::Grassland, BiomeType::Wetland, BiomeType::Rural};
    grass.minScale = 0.5f;
    grass.maxScale = 0.9f;
    grass.spacing = 1.0f;
    grass.clustered = true;
    grass.clusterRadius = 4.0f;
    grass.clusterCount = 8;
    m_params.foliageTypes.push_back(grass);

    // Desert plants
    FoliageType cactus;
    cactus.id = "cactus";
    cactus.name = "Cactus";
    cactus.validBiomes = {BiomeType::Desert};
    cactus.minScale = 0.7f;
    cactus.maxScale = 1.2f;
    cactus.spacing = 5.0f;
    cactus.clustered = false;
    m_params.foliageTypes.push_back(cactus);
}

PCGStageResult FoliageGenerator::Generate(PCGContext& context, PCGMode mode) {
    PCGStageResult result;
    result.success = true;

    // Update params from string parameters
    m_params.baseDensity = GetParamFloat("baseDensity", m_params.baseDensity);
    m_params.forestDensity = GetParamFloat("forestDensity", m_params.forestDensity);
    m_params.useClustering = GetParamBool("useClustering", m_params.useClustering);

    try {
        // Calculate density map
        std::vector<float> densityMap(context.GetWidth() * context.GetHeight());
        CalculateDensityMap(context, densityMap);

        // Generate placements
        GeneratePlacements(context, densityMap);

        // Apply clustering
        if (m_params.useClustering && mode == PCGMode::Final) {
            ApplyClustering(context);
        }

        result.itemsGenerated = static_cast<int>(context.GetFoliageSpawns().size());

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

void FoliageGenerator::RegisterFoliageType(const FoliageType& type) {
    // Check if type already exists
    for (auto& existing : m_params.foliageTypes) {
        if (existing.id == type.id) {
            existing = type;
            return;
        }
    }
    m_params.foliageTypes.push_back(type);
}

void FoliageGenerator::ClearFoliageTypes() {
    m_params.foliageTypes.clear();
}

void FoliageGenerator::CalculateDensityMap(PCGContext& context, std::vector<float>& densityMap) {
    int width = context.GetWidth();
    int height = context.GetHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            densityMap[y * width + x] = CalculateLocalDensity(context, x, y, context.GetBiome(x, y));
        }
    }
}

float FoliageGenerator::CalculateLocalDensity(PCGContext& context, int x, int y, BiomeType biome) {
    // Base density from biome
    float density = 0.0f;

    switch (biome) {
        case BiomeType::Forest:
            density = m_params.forestDensity;
            break;
        case BiomeType::Park:
            density = m_params.parkDensity;
            break;
        case BiomeType::Urban:
        case BiomeType::Commercial:
        case BiomeType::Industrial:
            density = m_params.urbanDensity;
            break;
        case BiomeType::Grassland:
        case BiomeType::Rural:
            density = m_params.baseDensity;
            break;
        case BiomeType::Desert:
            density = m_params.baseDensity * 0.2f;
            break;
        case BiomeType::Wetland:
            density = m_params.baseDensity * 0.6f;
            break;
        default:
            density = m_params.baseDensity;
            break;
    }

    // Modify by real-world tree density data
    float realDensity = context.GetTreeDensity(x, y);
    if (realDensity > 0.0f) {
        density = (density + realDensity) * 0.5f;
    }

    // Apply noise variation
    if (m_params.useNoiseVariation) {
        float noise = context.SimplexNoise(
            static_cast<float>(x + context.GetWorldX()),
            static_cast<float>(y + context.GetWorldY()),
            m_params.noiseFrequency, 2);
        noise = (noise + 1.0f) * 0.5f;  // Normalize to [0, 1]
        density *= (1.0f - m_params.noiseInfluence) + noise * m_params.noiseInfluence;
    }

    return density;
}

void FoliageGenerator::GeneratePlacements(PCGContext& context, const std::vector<float>& densityMap) {
    int width = context.GetWidth();
    int height = context.GetHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Skip invalid positions
            if (!IsValidPosition(context, x, y)) continue;

            float density = densityMap[y * width + x];

            // Check if we should place foliage here
            if (context.Random() > density) continue;

            // Select foliage type
            const FoliageType* type = SelectFoliageType(context, x, y);
            if (!type) continue;

            // Check spacing
            if (!CheckSpacing(context, x, y, type->spacing)) continue;

            // Calculate scale
            float scale = context.Random(type->minScale, type->maxScale);

            // Place foliage
            context.SpawnFoliage(x, y, type->id, scale);
        }
    }
}

const FoliageType* FoliageGenerator::SelectFoliageType(PCGContext& context, int x, int y) {
    BiomeType biome = context.GetBiome(x, y);

    // Build list of valid types for this biome
    std::vector<const FoliageType*> validTypes;

    for (const auto& type : m_params.foliageTypes) {
        bool validForBiome = false;
        for (BiomeType validBiome : type.validBiomes) {
            if (validBiome == biome) {
                validForBiome = true;
                break;
            }
        }
        if (validForBiome) {
            validTypes.push_back(&type);
        }
    }

    if (validTypes.empty()) return nullptr;

    // Random selection
    int idx = context.RandomInt(0, static_cast<int>(validTypes.size()) - 1);
    return validTypes[idx];
}

bool FoliageGenerator::IsValidPosition(PCGContext& context, int x, int y) const {
    // Check bounds
    if (!context.InBounds(x, y)) return false;

    // Check if occupied (road, building, etc.)
    if (context.IsOccupied(x, y)) return false;

    // Check for water
    if (context.IsWater(x, y)) return false;

    // Check distances
    // Road distance
    if (context.IsRoad(x, y)) return false;
    for (int dy = -static_cast<int>(m_params.minRoadDistance);
         dy <= static_cast<int>(m_params.minRoadDistance); ++dy) {
        for (int dx = -static_cast<int>(m_params.minRoadDistance);
             dx <= static_cast<int>(m_params.minRoadDistance); ++dx) {
            if (context.InBounds(x + dx, y + dy) && context.IsRoad(x + dx, y + dy)) {
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist < m_params.minRoadDistance) return false;
            }
        }
    }

    return true;
}

bool FoliageGenerator::CheckSpacing(PCGContext& context, int x, int y, float minDist) {
    const auto& spawns = context.GetFoliageSpawns();

    for (const auto& spawn : spawns) {
        float dx = spawn.position.x - (static_cast<float>(x) + 0.5f);
        float dy = spawn.position.z - (static_cast<float>(y) + 0.5f);
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < minDist) return false;
    }

    return true;
}

void FoliageGenerator::ApplyClustering(PCGContext& context) {
    // Get existing spawns
    auto spawns = context.GetFoliageSpawns();  // Copy
    context.ClearFoliageSpawns();

    for (const auto& spawn : spawns) {
        // Re-add original
        context.SpawnFoliageWorld(spawn.position.x, spawn.position.z, spawn.foliageType, spawn.scale);

        // Find foliage type
        const FoliageType* type = nullptr;
        for (const auto& t : m_params.foliageTypes) {
            if (t.id == spawn.foliageType) {
                type = &t;
                break;
            }
        }

        if (!type || !type->clustered) continue;

        // Check if this should start a cluster
        if (context.Random() > m_params.clusterChance) continue;

        // Add cluster members
        int clusterSize = context.RandomInt(1, type->clusterCount);
        for (int i = 0; i < clusterSize; ++i) {
            float angle = context.Random(0.0f, 6.28318f);
            float dist = context.Random(type->spacing, type->clusterRadius);

            float newX = spawn.position.x + std::cos(angle) * dist;
            float newY = spawn.position.z + std::sin(angle) * dist;

            int tileX = static_cast<int>(newX);
            int tileY = static_cast<int>(newY);

            if (IsValidPosition(context, tileX, tileY)) {
                float scale = context.Random(type->minScale, type->maxScale);
                context.SpawnFoliageWorld(newX, newY, type->id, scale);
            }
        }
    }
}

float FoliageGenerator::GetDensityAt(PCGContext& context, int x, int y) const {
    if (!context.InBounds(x, y)) return 0.0f;
    BiomeType biome = context.GetBiome(x, y);
    return const_cast<FoliageGenerator*>(this)->CalculateLocalDensity(context, x, y, biome);
}

} // namespace Vehement
