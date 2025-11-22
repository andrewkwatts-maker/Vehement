#include "ProceduralTown.hpp"
#include <algorithm>
#include <cmath>
#include <queue>

namespace Vehement {

// ============================================================================
// TileMap Implementation
// ============================================================================

TileMap::TileMap(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_tiles(static_cast<size_t>(width) * height)
{
}

TileType TileMap::GetTile(int x, int y) const {
    if (!InBounds(x, y)) return TileType::Empty;
    return m_tiles[GetIndex(x, y)].type;
}

uint8_t TileMap::GetVariant(int x, int y) const {
    if (!InBounds(x, y)) return 0;
    return m_tiles[GetIndex(x, y)].variant;
}

bool TileMap::IsWall(int x, int y) const {
    if (!InBounds(x, y)) return false;
    return m_tiles[GetIndex(x, y)].isWall;
}

float TileMap::GetWallHeight(int x, int y) const {
    if (!InBounds(x, y)) return 0.0f;
    return m_tiles[GetIndex(x, y)].wallHeight;
}

const TileMap::Tile* TileMap::GetTileData(int x, int y) const {
    if (!InBounds(x, y)) return nullptr;
    return &m_tiles[GetIndex(x, y)];
}

void TileMap::SetTile(int x, int y, TileType type, uint8_t variant) {
    if (!InBounds(x, y)) return;
    auto& tile = m_tiles[GetIndex(x, y)];
    tile.type = type;
    tile.variant = variant;
}

void TileMap::SetWall(int x, int y, bool isWall, float height) {
    if (!InBounds(x, y)) return;
    auto& tile = m_tiles[GetIndex(x, y)];
    tile.isWall = isWall;
    tile.wallHeight = isWall ? height : 0.0f;
}

void TileMap::Fill(TileType type, uint8_t variant) {
    for (auto& tile : m_tiles) {
        tile.type = type;
        tile.variant = variant;
        tile.isWall = false;
        tile.wallHeight = 0.0f;
    }
}

void TileMap::FillRect(int x, int y, int width, int height, TileType type, uint8_t variant) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            SetTile(x + dx, y + dy, type, variant);
        }
    }
}

// ============================================================================
// TownParams Implementation
// ============================================================================

bool ProceduralTown::TownParams::IsValid() const noexcept {
    return width >= 20 && width <= 1000 &&
           height >= 20 && height <= 1000 &&
           roadDensity >= 0.0f && roadDensity <= 1.0f &&
           buildingDensity >= 0.0f && buildingDensity <= 1.0f &&
           parkDensity >= 0.0f && parkDensity <= 1.0f &&
           (residentialRatio + commercialRatio + industrialRatio) <= 1.01f;
}

void ProceduralTown::TownParams::Clamp() {
    width = std::clamp(width, 20, 1000);
    height = std::clamp(height, 20, 1000);
    roadDensity = std::clamp(roadDensity, 0.0f, 1.0f);
    buildingDensity = std::clamp(buildingDensity, 0.0f, 1.0f);
    parkDensity = std::clamp(parkDensity, 0.0f, 1.0f);
    waterDensity = std::clamp(waterDensity, 0.0f, 0.3f);
    mainRoadWidth = std::clamp(mainRoadWidth, 2, 5);
    sideRoadWidth = std::clamp(sideRoadWidth, 1, 3);
    blockSizeMin = std::clamp(blockSizeMin, 6, 20);
    blockSizeMax = std::clamp(blockSizeMax, blockSizeMin, 30);
    buildingMinSize = std::clamp(buildingMinSize, 3, 15);
    buildingMaxSize = std::clamp(buildingMaxSize, buildingMinSize, 20);
    wallHeight = std::clamp(wallHeight, 1.0f, 10.0f);

    // Normalize zone ratios
    float total = residentialRatio + commercialRatio + industrialRatio;
    if (total > 0.0f) {
        residentialRatio /= total;
        commercialRatio /= total;
        industrialRatio /= total;
    }
}

// ============================================================================
// ProceduralTown Implementation
// ============================================================================

ProceduralTown::ProceduralTown() = default;
ProceduralTown::~ProceduralTown() = default;

std::unique_ptr<TileMap> ProceduralTown::Generate(const TownParams& params) {
    GenerationResult result;
    return Generate(params, result);
}

std::unique_ptr<TileMap> ProceduralTown::Generate(const TownParams& params, GenerationResult& outResult) {
    TownParams validParams = params;
    validParams.Clamp();

    if (!validParams.IsValid()) {
        outResult.success = false;
        outResult.errorMessage = "Invalid generation parameters";
        return nullptr;
    }

    // Create random generator
    std::mt19937 rng;
    if (validParams.seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(static_cast<unsigned int>(validParams.seed));
    }

    // Create tile map
    auto map = std::make_unique<TileMap>(validParams.width, validParams.height);

    // Generation pipeline
    FillBackground(*map, validParams, rng);
    GenerateRoads(*map, validParams, rng);

    if (validParams.hasTownCenter) {
        GenerateTownCenter(*map, validParams, rng);
    }

    GenerateBuildings(*map, validParams, rng);
    GenerateParks(*map, validParams, rng);

    if (validParams.waterDensity > 0.0f) {
        GenerateWater(*map, validParams, rng);
    }

    GenerateDetails(*map, validParams, rng);

    // Calculate statistics
    outResult.success = true;
    outResult.totalTiles = validParams.width * validParams.height;

    for (int y = 0; y < validParams.height; ++y) {
        for (int x = 0; x < validParams.width; ++x) {
            TileType type = map->GetTile(x, y);
            int category = GetTileCategory(type);

            switch (category) {
                case 2: // Concrete (roads)
                    outResult.roadTiles++;
                    break;
                case 3: // Bricks
                case 4: // Wood
                case 5: // Stone
                case 6: // Metal
                    outResult.buildingTiles++;
                    break;
                case 7: // Foliage
                    outResult.parkTiles++;
                    outResult.treeCount++;
                    break;
                case 8: // Water
                    outResult.waterTiles++;
                    break;
                case 9: // Objects
                    outResult.objectTiles++;
                    outResult.decorationCount++;
                    break;
                default:
                    break;
            }
        }
    }

    return map;
}

ProceduralTown::GenerationResult ProceduralTown::ApplyToMap(TileMap& map, const TownParams& params,
                                                            int offsetX, int offsetY) {
    GenerationResult result;

    TownParams validParams = params;
    validParams.Clamp();

    // Adjust dimensions to fit
    int maxWidth = map.GetWidth() - offsetX;
    int maxHeight = map.GetHeight() - offsetY;

    if (maxWidth <= 0 || maxHeight <= 0) {
        result.success = false;
        result.errorMessage = "Offset is outside map bounds";
        return result;
    }

    validParams.width = std::min(validParams.width, maxWidth);
    validParams.height = std::min(validParams.height, maxHeight);

    // Generate to temporary map
    auto tempMap = Generate(validParams, result);
    if (!tempMap || !result.success) {
        return result;
    }

    // Copy to destination
    for (int y = 0; y < validParams.height; ++y) {
        for (int x = 0; x < validParams.width; ++x) {
            TileType type = tempMap->GetTile(x, y);
            uint8_t variant = tempMap->GetVariant(x, y);
            bool isWall = tempMap->IsWall(x, y);
            float wallHeight = tempMap->GetWallHeight(x, y);

            map.SetTile(offsetX + x, offsetY + y, type, variant);
            map.SetWall(offsetX + x, offsetY + y, isWall, wallHeight);
        }
    }

    return result;
}

// ============================================================================
// Zone Layout Generation
// ============================================================================

void ProceduralTown::GenerateZoneLayout(std::vector<ZoneType>& zoneMap, int width, int height,
                                         const TownParams& params, std::mt19937& rng) {
    zoneMap.resize(static_cast<size_t>(width) * height, ZoneType::Empty);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Place zones based on distance from center
    int centerX = width / 2;
    int centerY = height / 2;
    float maxDist = std::sqrt(static_cast<float>(centerX * centerX + centerY * centerY));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float dx = static_cast<float>(x - centerX);
            float dy = static_cast<float>(y - centerY);
            float dist_from_center = std::sqrt(dx * dx + dy * dy) / maxDist;

            float zoneRoll = dist(rng);
            size_t idx = static_cast<size_t>(y) * width + x;

            // Center area is more likely to be commercial
            if (dist_from_center < 0.2f && params.hasTownCenter) {
                if (zoneRoll < 0.7f) {
                    zoneMap[idx] = ZoneType::Commercial;
                } else if (zoneRoll < 0.9f) {
                    zoneMap[idx] = ZoneType::Plaza;
                } else {
                    zoneMap[idx] = ZoneType::Park;
                }
            }
            // Middle ring is mixed
            else if (dist_from_center < 0.6f) {
                float adjRoll = zoneRoll;
                if (adjRoll < params.commercialRatio) {
                    zoneMap[idx] = ZoneType::Commercial;
                } else if (adjRoll < params.commercialRatio + params.residentialRatio * 0.7f) {
                    zoneMap[idx] = ZoneType::Residential;
                } else if (adjRoll < 0.9f) {
                    zoneMap[idx] = ZoneType::Park;
                } else {
                    zoneMap[idx] = ZoneType::Industrial;
                }
            }
            // Outer ring is more residential/industrial
            else {
                if (zoneRoll < params.residentialRatio) {
                    zoneMap[idx] = ZoneType::Residential;
                } else if (zoneRoll < params.residentialRatio + params.industrialRatio) {
                    zoneMap[idx] = ZoneType::Industrial;
                } else {
                    zoneMap[idx] = ZoneType::Park;
                }
            }
        }
    }
}

// ============================================================================
// Road Generation
// ============================================================================

void ProceduralTown::GenerateRoads(TileMap& map, const TownParams& params, std::mt19937& rng) {
    switch (params.roadPattern) {
        case RoadPattern::Grid:
            CreateGridRoads(map, params, rng);
            break;
        case RoadPattern::Organic:
            CreateOrganicRoads(map, params, rng);
            break;
        case RoadPattern::Radial:
            CreateRadialRoads(map, params, rng);
            break;
        case RoadPattern::Mixed:
            CreateGridRoads(map, params, rng);
            CreateRadialRoads(map, params, rng);
            break;
    }
}

void ProceduralTown::CreateGridRoads(TileMap& map, const TownParams& params, std::mt19937& rng) {
    std::uniform_int_distribution<int> blockDist(params.blockSizeMin, params.blockSizeMax);

    // Horizontal roads
    int y = params.mainRoadWidth;
    while (y < params.height - params.mainRoadWidth) {
        bool isMainRoad = (y == params.height / 2) ||
                          (y < params.mainRoadWidth * 2) ||
                          (y > params.height - params.mainRoadWidth * 2);
        int roadWidth = isMainRoad ? params.mainRoadWidth : params.sideRoadWidth;

        CreateRoad(map, 0, y, params.width, true, roadWidth, rng);

        y += blockDist(rng) + roadWidth;
    }

    // Vertical roads
    int x = params.mainRoadWidth;
    while (x < params.width - params.mainRoadWidth) {
        bool isMainRoad = (x == params.width / 2) ||
                          (x < params.mainRoadWidth * 2) ||
                          (x > params.width - params.mainRoadWidth * 2);
        int roadWidth = isMainRoad ? params.mainRoadWidth : params.sideRoadWidth;

        CreateRoad(map, x, 0, params.height, false, roadWidth, rng);

        x += blockDist(rng) + roadWidth;
    }
}

void ProceduralTown::CreateOrganicRoads(TileMap& map, const TownParams& params, std::mt19937& rng) {
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> curveDist(-0.3f, 0.3f);
    std::uniform_int_distribution<int> lengthDist(20, 50);

    int roadCount = static_cast<int>(params.roadDensity * 10);

    for (int i = 0; i < roadCount; ++i) {
        // Start from edge or center
        int startX, startY;
        float angle;

        if (i < roadCount / 2) {
            // From edges
            int edge = i % 4;
            switch (edge) {
                case 0: startX = 0; startY = params.height / 2; angle = 0.0f; break;
                case 1: startX = params.width - 1; startY = params.height / 2; angle = 3.14159f; break;
                case 2: startX = params.width / 2; startY = 0; angle = 1.5708f; break;
                default: startX = params.width / 2; startY = params.height - 1; angle = -1.5708f; break;
            }
        } else {
            // From center with random angle
            startX = params.width / 2;
            startY = params.height / 2;
            angle = angleDist(rng);
        }

        // Draw curved road
        float x = static_cast<float>(startX);
        float y = static_cast<float>(startY);
        int length = lengthDist(rng);

        for (int j = 0; j < length; ++j) {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);

            // Place road tiles
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (map.InBounds(ix + dx, iy + dy)) {
                        map.SetTile(ix + dx, iy + dy, GetRoadTile(rng));
                    }
                }
            }

            // Move forward with curve
            angle += curveDist(rng) * 0.1f;
            x += std::cos(angle) * 2.0f;
            y += std::sin(angle) * 2.0f;

            if (!map.InBounds(static_cast<int>(x), static_cast<int>(y))) {
                break;
            }
        }
    }
}

void ProceduralTown::CreateRadialRoads(TileMap& map, const TownParams& params, std::mt19937& rng) {
    int centerX = params.width / 2;
    int centerY = params.height / 2;

    // Create roads radiating from center
    int numRoads = 8;
    float angleStep = 6.28318f / numRoads;

    for (int i = 0; i < numRoads; ++i) {
        float angle = angleStep * i;
        float length = std::min(params.width, params.height) * 0.45f;

        int endX = centerX + static_cast<int>(std::cos(angle) * length);
        int endY = centerY + static_cast<int>(std::sin(angle) * length);

        CreatePath(map, centerX, centerY, endX, endY, params.mainRoadWidth, GetRoadTile(rng), rng);
    }

    // Create circular roads at different radii
    float radii[] = {0.2f, 0.4f, 0.6f};
    for (float radiusFactor : radii) {
        float radius = std::min(params.width, params.height) * radiusFactor * 0.5f;

        for (float angle = 0; angle < 6.28318f; angle += 0.05f) {
            int x = centerX + static_cast<int>(std::cos(angle) * radius);
            int y = centerY + static_cast<int>(std::sin(angle) * radius);

            if (map.InBounds(x, y)) {
                map.SetTile(x, y, GetRoadTile(rng));
            }
        }
    }
}

void ProceduralTown::CreateRoad(TileMap& map, int x, int y, int length, bool horizontal, int width, std::mt19937& rng) {
    for (int i = 0; i < length; ++i) {
        for (int w = 0; w < width; ++w) {
            int tileX = horizontal ? (x + i) : (x + w);
            int tileY = horizontal ? (y + w) : (y + i);

            if (map.InBounds(tileX, tileY)) {
                map.SetTile(tileX, tileY, GetRoadTile(rng));
            }
        }
    }
}

void ProceduralTown::CreatePath(TileMap& map, int x1, int y1, int x2, int y2, int width, TileType tileType, std::mt19937& rng) {
    // Bresenham's line algorithm with width
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int x = x1, y = y1;
    int halfWidth = width / 2;

    while (true) {
        // Place tiles in a square around the path point
        for (int wy = -halfWidth; wy <= halfWidth; ++wy) {
            for (int wx = -halfWidth; wx <= halfWidth; ++wx) {
                if (map.InBounds(x + wx, y + wy)) {
                    map.SetTile(x + wx, y + wy, tileType);
                }
            }
        }

        if (x == x2 && y == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// ============================================================================
// Building Generation
// ============================================================================

void ProceduralTown::GenerateBuildings(TileMap& map, const TownParams& params, std::mt19937& rng) {
    std::uniform_int_distribution<int> sizeDist(params.buildingMinSize, params.buildingMaxSize);
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> styleDist(0, 4);

    // Scan for empty areas adjacent to roads
    for (int y = 2; y < params.height - params.buildingMaxSize - 2; y += params.buildingMinSize) {
        for (int x = 2; x < params.width - params.buildingMaxSize - 2; x += params.buildingMinSize) {
            // Check if this area should have a building
            if (chanceDist(rng) > params.buildingDensity) {
                continue;
            }

            int buildingW = sizeDist(rng);
            int buildingH = sizeDist(rng);

            // Check if area is empty and adjacent to road
            if (!IsAreaEmpty(map, x, y, buildingW, buildingH)) {
                continue;
            }

            if (!HasAdjacentRoad(map, x, y, buildingW, buildingH)) {
                continue;
            }

            // Determine building style
            BuildingStyle style;
            if (params.defaultStyle == BuildingStyle::Mixed) {
                style = static_cast<BuildingStyle>(styleDist(rng));
            } else {
                style = params.defaultStyle;
            }

            // Create and render building
            Building building = PlaceBuilding(map, x, y, buildingW, buildingH, style, params, rng);
            RenderBuilding(map, building, rng);
        }
    }
}

ProceduralTown::Building ProceduralTown::PlaceBuilding(TileMap& map, int x, int y, int maxW, int maxH,
                                                        BuildingStyle style, const TownParams& params, std::mt19937& rng) {
    Building building;
    building.x = x;
    building.y = y;
    building.width = maxW;
    building.height = maxH;
    building.style = style;
    building.wallHeight = params.wallHeight;

    std::uniform_real_distribution<float> tallDist(0.0f, 1.0f);
    if (tallDist(rng) < params.tallBuildingChance) {
        building.wallHeight *= 1.5f;
    }

    // Determine entrance side based on adjacent road
    building.hasEntrance = true;
    building.entranceSide = 0;  // Default to south

    // Check each side for road
    // South
    if (y + maxH < params.height && map.GetTile(x + maxW / 2, y + maxH) == TileType::ConcreteAsphalt1) {
        building.entranceSide = 0;
    }
    // East
    else if (x + maxW < params.width && map.GetTile(x + maxW, y + maxH / 2) == TileType::ConcreteAsphalt1) {
        building.entranceSide = 1;
    }
    // North
    else if (y > 0 && map.GetTile(x + maxW / 2, y - 1) == TileType::ConcreteAsphalt1) {
        building.entranceSide = 2;
    }
    // West
    else if (x > 0 && map.GetTile(x - 1, y + maxH / 2) == TileType::ConcreteAsphalt1) {
        building.entranceSide = 3;
    }

    return building;
}

void ProceduralTown::RenderBuilding(TileMap& map, const Building& building, std::mt19937& rng) {
    RenderBuildingFloor(map, building, rng);
    RenderBuildingWalls(map, building, rng);
    if (building.hasEntrance) {
        RenderBuildingEntrance(map, building, rng);
    }
}

void ProceduralTown::RenderBuildingWalls(TileMap& map, const Building& building, std::mt19937& rng) {
    TileType wallTile = GetWallTile(building.style, rng);

    // North wall
    for (int x = building.x; x < building.x + building.width; ++x) {
        map.SetTile(x, building.y, wallTile);
        map.SetWall(x, building.y, true, building.wallHeight);
    }

    // South wall
    for (int x = building.x; x < building.x + building.width; ++x) {
        map.SetTile(x, building.y + building.height - 1, wallTile);
        map.SetWall(x, building.y + building.height - 1, true, building.wallHeight);
    }

    // West wall
    for (int y = building.y; y < building.y + building.height; ++y) {
        map.SetTile(building.x, y, wallTile);
        map.SetWall(building.x, y, true, building.wallHeight);
    }

    // East wall
    for (int y = building.y; y < building.y + building.height; ++y) {
        map.SetTile(building.x + building.width - 1, y, wallTile);
        map.SetWall(building.x + building.width - 1, y, true, building.wallHeight);
    }
}

void ProceduralTown::RenderBuildingFloor(TileMap& map, const Building& building, std::mt19937& rng) {
    TileType floorTile = GetFloorTile(building.style, rng);

    // Fill interior
    for (int y = building.y + 1; y < building.y + building.height - 1; ++y) {
        for (int x = building.x + 1; x < building.x + building.width - 1; ++x) {
            map.SetTile(x, y, floorTile);
        }
    }
}

void ProceduralTown::RenderBuildingEntrance(TileMap& map, const Building& building, std::mt19937& rng) {
    int entranceX, entranceY;

    switch (building.entranceSide) {
        case 0: // South
            entranceX = building.x + building.width / 2;
            entranceY = building.y + building.height - 1;
            break;
        case 1: // East
            entranceX = building.x + building.width - 1;
            entranceY = building.y + building.height / 2;
            break;
        case 2: // North
            entranceX = building.x + building.width / 2;
            entranceY = building.y;
            break;
        default: // West
            entranceX = building.x;
            entranceY = building.y + building.height / 2;
            break;
    }

    // Clear entrance wall
    map.SetTile(entranceX, entranceY, GetFloorTile(building.style, rng));
    map.SetWall(entranceX, entranceY, false, 0.0f);
}

// ============================================================================
// Park Generation
// ============================================================================

void ProceduralTown::GenerateParks(TileMap& map, const TownParams& params, std::mt19937& rng) {
    std::uniform_int_distribution<int> sizeDist(6, 15);
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    // Scan for empty areas
    for (int y = 2; y < params.height - 15; y += 8) {
        for (int x = 2; x < params.width - 15; x += 8) {
            if (chanceDist(rng) > params.parkDensity) {
                continue;
            }

            int parkW = sizeDist(rng);
            int parkH = sizeDist(rng);

            if (!IsAreaEmpty(map, x, y, parkW, parkH)) {
                continue;
            }

            Park park;
            park.x = x;
            park.y = y;
            park.width = parkW;
            park.height = parkH;
            park.hasFountain = chanceDist(rng) < 0.3f;
            park.hasPaths = chanceDist(rng) < 0.7f;
            park.treeDensity = params.foliageDensity;

            RenderPark(map, park, rng);
        }
    }
}

void ProceduralTown::RenderPark(TileMap& map, const Park& park, std::mt19937& rng) {
    // Fill with grass
    for (int y = park.y; y < park.y + park.height; ++y) {
        for (int x = park.x; x < park.x + park.width; ++x) {
            if (map.InBounds(x, y)) {
                map.SetTile(x, y, GetParkGroundTile(rng));
            }
        }
    }

    // Add paths
    if (park.hasPaths) {
        // Cross paths
        int midX = park.x + park.width / 2;
        int midY = park.y + park.height / 2;

        for (int x = park.x; x < park.x + park.width; ++x) {
            if (map.InBounds(x, midY)) {
                map.SetTile(x, midY, TileType::ConcreteTiles1);
            }
        }
        for (int y = park.y; y < park.y + park.height; ++y) {
            if (map.InBounds(midX, y)) {
                map.SetTile(midX, y, TileType::ConcreteTiles1);
            }
        }
    }

    // Add fountain in center
    if (park.hasFountain) {
        int centerX = park.x + park.width / 2;
        int centerY = park.y + park.height / 2;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (map.InBounds(centerX + dx, centerY + dy)) {
                    map.SetTile(centerX + dx, centerY + dy, TileType::Water1);
                }
            }
        }
    }

    // Add trees
    PlaceTrees(map, park.x, park.y, park.width, park.height, park.treeDensity, rng);
}

void ProceduralTown::GenerateWater(TileMap& map, const TownParams& params, std::mt19937& rng) {
    std::uniform_int_distribution<int> xDist(0, params.width - 10);
    std::uniform_int_distribution<int> yDist(0, params.height - 10);
    std::uniform_int_distribution<int> sizeDist(5, 15);

    int waterFeatures = static_cast<int>(params.waterDensity * 5);

    for (int i = 0; i < waterFeatures; ++i) {
        int x = xDist(rng);
        int y = yDist(rng);
        int size = sizeDist(rng);

        // Create organic water shape
        for (int dy = 0; dy < size; ++dy) {
            for (int dx = 0; dx < size; ++dx) {
                float distFromCenter = std::sqrt(
                    std::pow(dx - size / 2.0f, 2.0f) +
                    std::pow(dy - size / 2.0f, 2.0f)
                );

                if (distFromCenter < size / 2.0f) {
                    if (map.InBounds(x + dx, y + dy) &&
                        map.GetTile(x + dx, y + dy) == TileType::GroundGrass1) {
                        map.SetTile(x + dx, y + dy, TileType::Water1);
                    }
                }
            }
        }
    }
}

void ProceduralTown::GenerateTownCenter(TileMap& map, const TownParams& params, std::mt19937& rng) {
    int centerX = params.width / 2;
    int centerY = params.height / 2;
    int plazaSize = std::min(params.width, params.height) / 8;

    // Create plaza with decorative tiles
    for (int y = centerY - plazaSize; y <= centerY + plazaSize; ++y) {
        for (int x = centerX - plazaSize; x <= centerX + plazaSize; ++x) {
            if (map.InBounds(x, y)) {
                // Checkerboard pattern
                bool checker = ((x + y) % 2 == 0);
                map.SetTile(x, y, checker ? TileType::ConcreteTiles1 : TileType::ConcreteTiles2);
            }
        }
    }

    // Add central fountain
    for (int y = centerY - 2; y <= centerY + 2; ++y) {
        for (int x = centerX - 2; x <= centerX + 2; ++x) {
            if (map.InBounds(x, y)) {
                float dist = std::sqrt(std::pow(x - centerX, 2.0f) + std::pow(y - centerY, 2.0f));
                if (dist <= 2.0f) {
                    map.SetTile(x, y, TileType::Water1);
                }
            }
        }
    }

    // Add benches around plaza
    std::uniform_int_distribution<int> benchDist(0, 3);
    TileType benchTiles[] = {TileType::ObjectBarStool, TileType::FoliagePlanterBox};

    for (int side = 0; side < 4; ++side) {
        int bx, by;
        switch (side) {
            case 0: bx = centerX; by = centerY - plazaSize - 1; break;
            case 1: bx = centerX + plazaSize + 1; by = centerY; break;
            case 2: bx = centerX; by = centerY + plazaSize + 1; break;
            default: bx = centerX - plazaSize - 1; by = centerY; break;
        }

        if (map.InBounds(bx, by)) {
            map.SetTile(bx, by, benchTiles[benchDist(rng) % 2]);
        }
    }
}

// ============================================================================
// Detail Generation
// ============================================================================

void ProceduralTown::GenerateDetails(TileMap& map, const TownParams& params, std::mt19937& rng) {
    // Add decorations to various areas
    for (int y = 0; y < params.height; ++y) {
        for (int x = 0; x < params.width; ++x) {
            TileType currentTile = map.GetTile(x, y);

            // Skip non-ground tiles
            if (currentTile == TileType::Empty ||
                GetTileCategory(currentTile) != 1) { // Not ground
                continue;
            }

            std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

            // Add occasional trees to grass areas
            if (currentTile == TileType::GroundGrass1 ||
                currentTile == TileType::GroundGrass2) {
                if (chanceDist(rng) < params.foliageDensity * 0.1f) {
                    map.SetTile(x, y, GetTreeTile(rng));
                }
            }

            // Add street decorations near roads
            if (chanceDist(rng) < params.decorationDensity * 0.1f) {
                bool nearRoad = false;
                for (int dy = -1; dy <= 1 && !nearRoad; ++dy) {
                    for (int dx = -1; dx <= 1 && !nearRoad; ++dx) {
                        if (map.InBounds(x + dx, y + dy)) {
                            TileType neighbor = map.GetTile(x + dx, y + dy);
                            if (GetTileCategory(neighbor) == 2) { // Concrete
                                nearRoad = true;
                            }
                        }
                    }
                }

                if (nearRoad && currentTile == TileType::GroundGrass1) {
                    TileType decoration = GetDecorationTile(ZoneType::Residential, rng);
                    map.SetTile(x, y, decoration);
                }
            }
        }
    }
}

void ProceduralTown::FillBackground(TileMap& map, const TownParams& params, std::mt19937& rng) {
    for (int y = 0; y < params.height; ++y) {
        for (int x = 0; x < params.width; ++x) {
            map.SetTile(x, y, GetGroundTile(rng));
        }
    }
}

void ProceduralTown::PlaceTrees(TileMap& map, int x, int y, int width, int height, float density, std::mt19937& rng) {
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    for (int dy = 1; dy < height - 1; ++dy) {
        for (int dx = 1; dx < width - 1; ++dx) {
            if (chanceDist(rng) < density * 0.3f) {
                int tx = x + dx;
                int ty = y + dy;

                if (map.InBounds(tx, ty)) {
                    TileType current = map.GetTile(tx, ty);
                    // Only place on grass
                    if (current == TileType::GroundGrass1 ||
                        current == TileType::GroundGrass2) {
                        map.SetTile(tx, ty, GetTreeTile(rng));
                    }
                }
            }
        }
    }
}

void ProceduralTown::PlaceDecorations(TileMap& map, int x, int y, int width, int height, float density, std::mt19937& rng) {
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            if (chanceDist(rng) < density * 0.1f) {
                int tx = x + dx;
                int ty = y + dy;

                if (map.InBounds(tx, ty) && map.GetTile(tx, ty) == TileType::Empty) {
                    map.SetTile(tx, ty, GetDecorationTile(ZoneType::Park, rng));
                }
            }
        }
    }
}

// ============================================================================
// Tile Selection Helpers
// ============================================================================

TileType ProceduralTown::GetGroundTile(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 10);
    int roll = dist(rng);

    if (roll < 7) return TileType::GroundGrass1;
    if (roll < 9) return TileType::GroundGrass2;
    return TileType::GroundDirt;
}

TileType ProceduralTown::GetRoadTile(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 10);
    int roll = dist(rng);

    if (roll < 8) return TileType::ConcreteAsphalt1;
    return TileType::ConcreteAsphalt2;
}

TileType ProceduralTown::GetWallTile(BuildingStyle style, std::mt19937& rng) {
    std::uniform_int_distribution<int> variantDist(0, 2);

    switch (style) {
        case BuildingStyle::Brick:
            switch (variantDist(rng)) {
                case 0: return TileType::BricksRock;
                case 1: return TileType::BricksGrey;
                default: return TileType::BricksBlack;
            }
        case BuildingStyle::Stone:
            switch (variantDist(rng)) {
                case 0: return TileType::StoneMarble1;
                case 1: return TileType::StoneMarble2;
                default: return TileType::StoneRaw;
            }
        case BuildingStyle::Wood:
            return TileType::Wood1;
        case BuildingStyle::Metal:
            switch (variantDist(rng)) {
                case 0: return TileType::Metal1;
                case 1: return TileType::Metal2;
                default: return TileType::Metal3;
            }
        case BuildingStyle::Mixed:
        default:
            return GetWallTile(static_cast<BuildingStyle>(variantDist(rng)), rng);
    }
}

TileType ProceduralTown::GetFloorTile(BuildingStyle style, std::mt19937& rng) {
    std::uniform_int_distribution<int> variantDist(0, 2);

    switch (style) {
        case BuildingStyle::Brick:
            return TileType::ConcreteTiles1;
        case BuildingStyle::Stone:
            switch (variantDist(rng)) {
                case 0: return TileType::StoneMarble1;
                default: return TileType::StoneMarble2;
            }
        case BuildingStyle::Wood:
            switch (variantDist(rng)) {
                case 0: return TileType::WoodFlooring1;
                default: return TileType::WoodFlooring2;
            }
        case BuildingStyle::Metal:
            switch (variantDist(rng)) {
                case 0: return TileType::MetalTile1;
                case 1: return TileType::MetalTile2;
                default: return TileType::MetalTile3;
            }
        case BuildingStyle::Mixed:
        default:
            return GetFloorTile(static_cast<BuildingStyle>(variantDist(rng)), rng);
    }
}

TileType ProceduralTown::GetTreeTile(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 10);
    int roll = dist(rng);

    if (roll < 3) return TileType::FoliageTree1;
    if (roll < 5) return TileType::FoliageTree2;
    if (roll < 7) return TileType::FoliageSilverOak;
    if (roll < 8) return TileType::FoliageCherryTree;
    if (roll < 9) return TileType::FoliagePalm1;
    return TileType::FoliageShrub1;
}

TileType ProceduralTown::GetParkGroundTile(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 10);
    int roll = dist(rng);

    if (roll < 7) return TileType::GroundGrass1;
    if (roll < 9) return TileType::GroundGrass2;
    return TileType::GroundForrest1;
}

TileType ProceduralTown::GetDecorationTile(ZoneType zone, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 10);
    int roll = dist(rng);

    switch (zone) {
        case ZoneType::Residential:
            if (roll < 3) return TileType::FoliagePlanterBox;
            if (roll < 5) return TileType::FoliagePotPlant;
            if (roll < 7) return TileType::FoliageBonsai;
            return TileType::ObjectBarStool;

        case ZoneType::Commercial:
            if (roll < 4) return TileType::ObjectClothesStand;
            if (roll < 7) return TileType::ObjectDeskTop;
            return TileType::FoliagePlanterBox2;

        case ZoneType::Industrial:
            if (roll < 3) return TileType::ObjectGenerator;
            if (roll < 5) return TileType::ObjectPiping1;
            if (roll < 7) return TileType::ObjectGarbage1;
            return TileType::WoodCrate1;

        case ZoneType::Park:
        default:
            if (roll < 4) return TileType::FoliagePlanterBox;
            if (roll < 7) return TileType::FoliageShrub1;
            return TileType::FoliagePotPlant;
    }
}

TileType ProceduralTown::GetCornerTile(int cornerType, std::mt19937& rng) {
    // cornerType: 0=TL, 1=TR, 2=BL, 3=BR
    switch (cornerType) {
        case 0: return TileType::BricksCornerTL;
        case 1: return TileType::BricksCornerTR;
        case 2: return TileType::BricksCornerBL;
        case 3: return TileType::BricksCornerBR;
        default: return TileType::BricksRock;
    }
}

// ============================================================================
// Area Checking Helpers
// ============================================================================

bool ProceduralTown::IsAreaEmpty(const TileMap& map, int x, int y, int width, int height) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            if (!map.InBounds(x + dx, y + dy)) {
                return false;
            }

            TileType tile = map.GetTile(x + dx, y + dy);
            int category = GetTileCategory(tile);

            // Only allow ground tiles
            if (category != 1 && tile != TileType::Empty) {
                return false;
            }
        }
    }
    return true;
}

bool ProceduralTown::IsAreaType(const TileMap& map, int x, int y, int width, int height, TileType type) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            if (!map.InBounds(x + dx, y + dy)) {
                return false;
            }
            if (map.GetTile(x + dx, y + dy) != type) {
                return false;
            }
        }
    }
    return true;
}

bool ProceduralTown::HasAdjacentRoad(const TileMap& map, int x, int y, int width, int height) {
    // Check all edges for road tiles
    // Top edge
    for (int dx = 0; dx < width; ++dx) {
        if (map.InBounds(x + dx, y - 1)) {
            int cat = GetTileCategory(map.GetTile(x + dx, y - 1));
            if (cat == 2) return true; // Concrete category
        }
    }

    // Bottom edge
    for (int dx = 0; dx < width; ++dx) {
        if (map.InBounds(x + dx, y + height)) {
            int cat = GetTileCategory(map.GetTile(x + dx, y + height));
            if (cat == 2) return true;
        }
    }

    // Left edge
    for (int dy = 0; dy < height; ++dy) {
        if (map.InBounds(x - 1, y + dy)) {
            int cat = GetTileCategory(map.GetTile(x - 1, y + dy));
            if (cat == 2) return true;
        }
    }

    // Right edge
    for (int dy = 0; dy < height; ++dy) {
        if (map.InBounds(x + width, y + dy)) {
            int cat = GetTileCategory(map.GetTile(x + width, y + dy));
            if (cat == 2) return true;
        }
    }

    return false;
}

} // namespace Vehement
