#include "BuildingGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

BuildingGenerator::BuildingGenerator() {
}

PCGStageResult BuildingGenerator::Generate(PCGContext& context, PCGMode mode) {
    PCGStageResult result;
    result.success = true;

    // Update params from string parameters
    m_params.useRealData = GetParamBool("useRealData", m_params.useRealData);
    m_params.generateProcedural = GetParamBool("generateProcedural", m_params.generateProcedural);
    m_params.urbanDensity = GetParamFloat("urbanDensity", m_params.urbanDensity);
    m_params.defaultWallHeight = GetParamFloat("defaultWallHeight", m_params.defaultWallHeight);

    try {
        std::vector<BuildingFootprint> buildings;

        // Convert real-world building data
        if (m_params.useRealData) {
            ConvertGeoBuildings(context, buildings);
        }

        // Generate procedural buildings
        if (m_params.generateProcedural) {
            GenerateProceduralBuildings(context, buildings);
        }

        // Add entrances
        if (m_params.addEntrances) {
            AddEntrances(context, buildings);
        }

        // Rasterize buildings to tiles
        RasterizeBuildings(context, buildings);

        result.itemsGenerated = static_cast<int>(buildings.size());

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

void BuildingGenerator::ConvertGeoBuildings(PCGContext& context,
                                             std::vector<BuildingFootprint>& buildings) {
    // Get buildings from geo data via context
    auto nearbyBuildings = context.GetNearbyBuildings(
        context.GetWidth() / 2, context.GetHeight() / 2,
        static_cast<float>(std::max(context.GetWidth(), context.GetHeight())));

    for (const auto* geoBuilding : nearbyBuildings) {
        if (geoBuilding->footprint.empty()) continue;

        // Find bounding box of footprint
        float minX = geoBuilding->footprint[0].x, maxX = geoBuilding->footprint[0].x;
        float minY = geoBuilding->footprint[0].y, maxY = geoBuilding->footprint[0].y;

        for (const auto& pt : geoBuilding->footprint) {
            minX = std::min(minX, pt.x);
            maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
        }

        BuildingFootprint building;
        building.position = glm::ivec2(static_cast<int>(minX), static_cast<int>(minY));
        building.width = static_cast<int>(maxX - minX) + 1;
        building.height = static_cast<int>(maxY - minY) + 1;
        building.type = geoBuilding->type;
        building.wallHeight = (geoBuilding->height > 0.0f) ?
            geoBuilding->height : m_params.defaultWallHeight;
        building.fromRealData = true;

        // Check if building is within bounds and valid size
        if (context.InBounds(building.position.x, building.position.y) &&
            building.width >= m_params.minBuildingSize &&
            building.height >= m_params.minBuildingSize) {
            buildings.push_back(building);
        }
    }
}

void BuildingGenerator::GenerateProceduralBuildings(PCGContext& context,
                                                      std::vector<BuildingFootprint>& buildings) {
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Calculate target building count based on density
    int cellSize = 20;  // Check every N tiles
    int buildingsPlaced = 0;

    for (int cy = 0; cy < height; cy += cellSize) {
        for (int cx = 0; cx < width; cx += cellSize) {
            // Check biome/density at this cell
            BiomeType biome = context.GetBiome(cx, cy);
            float density = GetDensityForBiome(biome);

            // Skip if too low density
            if (context.Random() > density) continue;

            // Try to place a building in this cell
            int buildingWidth = context.RandomInt(m_params.minBuildingSize, m_params.maxBuildingSize);
            int buildingHeight = context.RandomInt(m_params.minBuildingSize, m_params.maxBuildingSize);

            glm::ivec2 position;
            if (FindBuildingLocation(context, buildingWidth, buildingHeight, position)) {
                BuildingFootprint building;
                building.position = position;
                building.width = buildingWidth;
                building.height = buildingHeight;
                building.type = InferBuildingType(biome, context);
                building.wallHeight = context.Random(m_params.minWallHeight, m_params.maxWallHeight);
                building.fromRealData = false;

                buildings.push_back(building);
                buildingsPlaced++;

                // Mark area as occupied
                for (int y = 0; y < building.height; ++y) {
                    for (int x = 0; x < building.width; ++x) {
                        context.MarkOccupied(building.position.x + x, building.position.y + y);
                    }
                }
            }
        }
    }
}

bool BuildingGenerator::FindBuildingLocation(PCGContext& context, int width, int height,
                                               glm::ivec2& outPosition) {
    // Try random positions
    int maxAttempts = 20;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        int x = context.RandomInt(1, context.GetWidth() - width - 1);
        int y = context.RandomInt(1, context.GetHeight() - height - 1);

        if (IsValidBuildingArea(context, x, y, width, height)) {
            outPosition = glm::ivec2(x, y);
            return true;
        }
    }

    return false;
}

bool BuildingGenerator::IsValidBuildingArea(PCGContext& context, int x, int y,
                                              int width, int height) {
    // Check if area is clear
    for (int dy = -1; dy <= height; ++dy) {
        for (int dx = -1; dx <= width; ++dx) {
            int px = x + dx;
            int py = y + dy;

            if (!context.InBounds(px, py)) return false;
            if (context.IsOccupied(px, py)) return false;

            // Check for water
            if (context.IsWater(px, py)) return false;
        }
    }

    // Check for nearby road (building should have road access)
    bool hasRoadAccess = false;
    int checkRadius = static_cast<int>(m_params.maxRoadDistance);

    for (int dy = -checkRadius; dy <= height + checkRadius && !hasRoadAccess; ++dy) {
        for (int dx = -checkRadius; dx <= width + checkRadius && !hasRoadAccess; ++dx) {
            int px = x + dx;
            int py = y + dy;

            if (context.InBounds(px, py) && context.IsRoad(px, py)) {
                float dist = std::min({
                    static_cast<float>(std::abs(dx)),
                    static_cast<float>(std::abs(dy)),
                    static_cast<float>(std::abs(dx - width)),
                    static_cast<float>(std::abs(dy - height))
                });

                if (dist >= m_params.minRoadDistance && dist <= m_params.maxRoadDistance) {
                    hasRoadAccess = true;
                }
            }
        }
    }

    return hasRoadAccess || (m_params.maxRoadDistance == 0.0f);
}

void BuildingGenerator::RasterizeBuildings(PCGContext& context,
                                            const std::vector<BuildingFootprint>& buildings) {
    for (const auto& building : buildings) {
        RasterizeBuilding(context, building);
    }
}

void BuildingGenerator::RasterizeBuilding(PCGContext& context, const BuildingFootprint& building) {
    TileType floorTile = GetFloorTile(building.type);
    TileType wallTile = GetWallTile(building.type);

    // Draw floor
    for (int y = 1; y < building.height - 1; ++y) {
        for (int x = 1; x < building.width - 1; ++x) {
            int px = building.position.x + x;
            int py = building.position.y + y;
            if (context.InBounds(px, py)) {
                context.SetTile(px, py, floorTile);
                context.MarkOccupied(px, py);
            }
        }
    }

    // Draw walls
    for (int x = 0; x < building.width; ++x) {
        // Top wall
        int px = building.position.x + x;
        int py = building.position.y;
        if (context.InBounds(px, py)) {
            context.SetWall(px, py, wallTile, building.wallHeight);
            context.MarkOccupied(px, py);
        }

        // Bottom wall
        py = building.position.y + building.height - 1;
        if (context.InBounds(px, py)) {
            context.SetWall(px, py, wallTile, building.wallHeight);
            context.MarkOccupied(px, py);
        }
    }

    for (int y = 1; y < building.height - 1; ++y) {
        // Left wall
        int px = building.position.x;
        int py = building.position.y + y;
        if (context.InBounds(px, py)) {
            context.SetWall(px, py, wallTile, building.wallHeight);
            context.MarkOccupied(px, py);
        }

        // Right wall
        px = building.position.x + building.width - 1;
        if (context.InBounds(px, py)) {
            context.SetWall(px, py, wallTile, building.wallHeight);
            context.MarkOccupied(px, py);
        }
    }

    // Add entrance
    if (m_params.addEntrances && building.width > 2 && building.height > 2) {
        int entranceX, entranceY;
        switch (building.entranceSide) {
            case 0:  // South
                entranceX = building.position.x + building.width / 2;
                entranceY = building.position.y + building.height - 1;
                break;
            case 1:  // East
                entranceX = building.position.x + building.width - 1;
                entranceY = building.position.y + building.height / 2;
                break;
            case 2:  // North
                entranceX = building.position.x + building.width / 2;
                entranceY = building.position.y;
                break;
            default:  // West
                entranceX = building.position.x;
                entranceY = building.position.y + building.height / 2;
                break;
        }

        if (context.InBounds(entranceX, entranceY)) {
            context.SetTile(entranceX, entranceY, floorTile);
        }
    }
}

void BuildingGenerator::AddEntrances(PCGContext& context, std::vector<BuildingFootprint>& buildings) {
    for (auto& building : buildings) {
        building.entranceSide = FindBestEntranceSide(context, building);
    }
}

int BuildingGenerator::FindBestEntranceSide(PCGContext& context, const BuildingFootprint& building) {
    // Find side closest to road
    int bestSide = 0;
    float bestDist = 1000000.0f;

    struct SideInfo {
        int side;
        int centerX, centerY;
        int checkX, checkY;
    };

    SideInfo sides[] = {
        {0, building.position.x + building.width / 2, building.position.y + building.height,
         building.position.x + building.width / 2, building.position.y + building.height + 1},
        {1, building.position.x + building.width, building.position.y + building.height / 2,
         building.position.x + building.width + 1, building.position.y + building.height / 2},
        {2, building.position.x + building.width / 2, building.position.y - 1,
         building.position.x + building.width / 2, building.position.y - 2},
        {3, building.position.x - 1, building.position.y + building.height / 2,
         building.position.x - 2, building.position.y + building.height / 2}
    };

    for (const auto& side : sides) {
        // Check if road is on this side
        for (int radius = 1; radius <= 5; ++radius) {
            for (int offset = -radius; offset <= radius; ++offset) {
                int checkX = (side.side % 2 == 0) ? side.checkX + offset : side.checkX;
                int checkY = (side.side % 2 == 1) ? side.checkY + offset : side.checkY;

                if (context.InBounds(checkX, checkY) && context.IsRoad(checkX, checkY)) {
                    float dist = static_cast<float>(radius);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestSide = side.side;
                    }
                }
            }
        }
    }

    return bestSide;
}

float BuildingGenerator::GetDensityForBiome(BiomeType biome) const {
    switch (biome) {
        case BiomeType::Urban:
        case BiomeType::Commercial:
            return m_params.urbanDensity;
        case BiomeType::Suburban:
        case BiomeType::Residential:
            return m_params.suburbanDensity;
        case BiomeType::Rural:
            return m_params.ruralDensity;
        case BiomeType::Industrial:
            return m_params.urbanDensity * 0.8f;
        default:
            return 0.0f;  // No buildings in natural areas
    }
}

TileType BuildingGenerator::GetFloorTile(BuildingType type) const {
    switch (type) {
        case BuildingType::House:
        case BuildingType::Apartment:
            return m_params.residentialFloor;
        case BuildingType::Office:
        case BuildingType::Shop:
            return m_params.commercialFloor;
        case BuildingType::Factory:
        case BuildingType::Warehouse:
            return m_params.industrialFloor;
        default:
            return m_params.residentialFloor;
    }
}

TileType BuildingGenerator::GetWallTile(BuildingType type) const {
    switch (type) {
        case BuildingType::House:
        case BuildingType::Apartment:
            return m_params.residentialWall;
        case BuildingType::Office:
        case BuildingType::Shop:
            return m_params.commercialWall;
        case BuildingType::Factory:
        case BuildingType::Warehouse:
            return m_params.industrialWall;
        default:
            return m_params.residentialWall;
    }
}

BuildingType BuildingGenerator::InferBuildingType(BiomeType biome, PCGContext& context) {
    switch (biome) {
        case BiomeType::Urban:
        case BiomeType::Commercial:
            return context.RandomBool(0.6f) ? BuildingType::Shop : BuildingType::Office;
        case BiomeType::Industrial:
            return context.RandomBool(0.5f) ? BuildingType::Factory : BuildingType::Warehouse;
        case BiomeType::Residential:
        case BiomeType::Suburban:
            return context.RandomBool(0.7f) ? BuildingType::House : BuildingType::Apartment;
        default:
            return BuildingType::House;
    }
}

} // namespace Vehement
