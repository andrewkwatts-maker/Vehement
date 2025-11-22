#include "BuildingPlacer.hpp"
#include "../world/TileMap.hpp"
#include "../world/World.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

BuildingPlacer::BuildingPlacer() = default;

// =============================================================================
// Building Selection
// =============================================================================

std::vector<BuildingType> BuildingPlacer::GetAvailableBuildings() const {
    std::vector<BuildingType> buildings;

    for (int i = 0; i < static_cast<int>(BuildingType::COUNT); ++i) {
        buildings.push_back(static_cast<BuildingType>(i));
    }

    return buildings;
}

void BuildingPlacer::SetSelectedBuilding(BuildingType type) {
    m_selectedBuilding = type;
    m_selectedVariant = 0;  // Reset variant when changing building
}

int BuildingPlacer::GetVariantCount() const {
    // Most buildings have 1-3 variants
    switch (m_selectedBuilding) {
        case BuildingType::Wall:
        case BuildingType::Gate:
            return 3;
        case BuildingType::House:
        case BuildingType::Shelter:
            return 2;
        default:
            return 1;
    }
}

// =============================================================================
// Preview Position
// =============================================================================

void BuildingPlacer::SetPreviewPosition(const glm::vec3& position) {
    m_previewPosition = position;
    m_previewGridPos.x = static_cast<int>(std::floor(position.x));
    m_previewGridPos.y = static_cast<int>(std::floor(position.z));
}

void BuildingPlacer::SetPreviewTile(int tileX, int tileY) {
    m_previewGridPos.x = tileX;
    m_previewGridPos.y = tileY;
    m_previewPosition = glm::vec3(
        static_cast<float>(tileX) + 0.5f,
        0.0f,
        static_cast<float>(tileY) + 0.5f
    );
}

// =============================================================================
// Rotation
// =============================================================================

void BuildingPlacer::SetRotation(float radians) {
    m_rotation = SnapRotation(radians);
}

void BuildingPlacer::SetRotationDegrees(float degrees) {
    SetRotation(glm::radians(degrees));
}

void BuildingPlacer::Rotate(float radians) {
    SetRotation(m_rotation + radians);
}

void BuildingPlacer::RotateCW() {
    Rotate(glm::radians(90.0f));
}

void BuildingPlacer::RotateCCW() {
    Rotate(glm::radians(-90.0f));
}

float BuildingPlacer::SnapRotation(float radians) const {
    float snapAngle;

    switch (m_rotationSnap) {
        case RotationSnap::Snap45:
            snapAngle = glm::radians(45.0f);
            break;
        case RotationSnap::Snap90:
            snapAngle = glm::radians(90.0f);
            break;
        case RotationSnap::Snap180:
            snapAngle = glm::radians(180.0f);
            break;
        case RotationSnap::Free:
        default:
            return radians;
    }

    return std::round(radians / snapAngle) * snapAngle;
}

// =============================================================================
// Road Alignment
// =============================================================================

bool BuildingPlacer::AlignToRoad(const TileMap& map) {
    if (!m_roadAlignEnabled) return false;

    glm::vec2 roadDir = FindNearestRoadDirection(
        map, m_previewGridPos.x, m_previewGridPos.y);

    if (glm::length(roadDir) > 0.0f) {
        float angle = std::atan2(roadDir.x, roadDir.y);
        SetRotation(angle);
        return true;
    }

    return false;
}

glm::vec2 BuildingPlacer::FindNearestRoadDirection(
    const TileMap& map, int tileX, int tileY) const {

    int searchRadius = static_cast<int>(m_roadAlignDistance);
    float nearestDist = std::numeric_limits<float>::max();
    glm::vec2 nearestDir{0.0f};

    for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
        for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
            int x = tileX + dx;
            int y = tileY + dy;

            if (!map.IsValidPosition(x, y)) continue;

            const Tile& tile = map.GetTile(x, y);

            // Check if tile is a road type (concrete/asphalt)
            bool isRoad = (tile.type >= TileType::ConcreteAsphalt1 &&
                          tile.type <= TileType::ConcreteTiles2);

            if (isRoad) {
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist < nearestDist && dist > 0.0f) {
                    nearestDist = dist;
                    nearestDir = glm::normalize(glm::vec2(
                        static_cast<float>(dx),
                        static_cast<float>(dy)
                    ));
                }
            }
        }
    }

    return nearestDir;
}

// =============================================================================
// Validation
// =============================================================================

PlacementValidation BuildingPlacer::ValidatePlacement(
    const TileMap& map,
    const std::vector<Building*>& existingBuildings) const {

    PlacementValidation result;
    result.valid = true;

    glm::ivec2 size = GetBuildingSize(m_selectedBuilding);

    // Check each tile the building would occupy
    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            int x = m_previewGridPos.x + dx;
            int y = m_previewGridPos.y + dy;

            // Check bounds
            if (!map.IsValidPosition(x, y)) {
                result.valid = false;
                result.outOfBounds = true;
                result.blockedTiles.emplace_back(x, y);
                continue;
            }

            // Check terrain
            const Tile& tile = map.GetTile(x, y);
            if (!tile.isWalkable || tile.isWall || IsWaterTile(tile.type)) {
                result.valid = false;
                result.terrainBlocked = true;
                result.blockedTiles.emplace_back(x, y);
            }
        }
    }

    // Check collision with existing buildings
    for (const auto* building : existingBuildings) {
        if (!building) continue;

        auto occupiedTiles = building->GetOccupiedTiles();
        for (const auto& occupied : occupiedTiles) {
            for (int dy = 0; dy < size.y; ++dy) {
                for (int dx = 0; dx < size.x; ++dx) {
                    int x = m_previewGridPos.x + dx;
                    int y = m_previewGridPos.y + dy;

                    if (occupied.x == x && occupied.y == y) {
                        result.valid = false;
                        result.hasCollision = true;
                        result.blockedTiles.emplace_back(x, y);
                    }
                }
            }
        }
    }

    // Build error message
    if (!result.valid) {
        if (result.outOfBounds) {
            result.errorMessage = "Placement is out of bounds";
        } else if (result.terrainBlocked) {
            result.errorMessage = "Terrain blocks placement";
        } else if (result.hasCollision) {
            result.errorMessage = "Collides with existing building";
        } else if (result.resourcesInsufficient) {
            result.errorMessage = "Insufficient resources";
        }
    }

    return result;
}

std::vector<glm::ivec2> BuildingPlacer::GetOccupiedTiles() const {
    std::vector<glm::ivec2> tiles;
    glm::ivec2 size = GetBuildingSize(m_selectedBuilding);

    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            tiles.emplace_back(m_previewGridPos.x + dx, m_previewGridPos.y + dy);
        }
    }

    return tiles;
}

// =============================================================================
// Placement
// =============================================================================

Building* BuildingPlacer::PlaceBuilding(World& world) {
    if (!m_lastValidation.valid) {
        return nullptr;
    }

    // Create and configure building
    auto building = std::make_unique<Building>(m_selectedBuilding);
    building->SetGridPosition(m_previewGridPos);
    building->SetRotation(m_rotation);
    building->SetState(BuildingState::Blueprint);

    Building* ptr = building.get();

    // Add to world (world takes ownership)
    // Note: Actual implementation depends on World interface
    // world.AddBuilding(std::move(building));

    if (m_onBuildingPlaced) {
        m_onBuildingPlaced(*ptr);
    }

    return ptr;
}

std::vector<Building*> BuildingPlacer::PlaceMultiple(
    World& world, const MultiPlaceConfig& config) {

    std::vector<Building*> placed;
    std::mt19937 rng(std::random_device{}());

    glm::ivec2 size = GetBuildingSize(m_selectedBuilding);

    float spacingX = config.spacingX;
    float spacingZ = config.spacingZ;

    if (spacingX <= 0.0f) spacingX = static_cast<float>(size.x) + 1.0f;
    if (spacingZ <= 0.0f) spacingZ = static_cast<float>(size.y) + 1.0f;

    glm::vec3 startPos = m_previewPosition;

    for (int i = 0; i < config.count; ++i) {
        // Calculate position
        int row = i / 5;  // 5 buildings per row
        int col = i % 5;

        m_previewPosition = startPos + glm::vec3(
            col * spacingX,
            0.0f,
            row * spacingZ
        );

        m_previewGridPos.x = static_cast<int>(std::floor(m_previewPosition.x));
        m_previewGridPos.y = static_cast<int>(std::floor(m_previewPosition.z));

        // Randomize rotation if enabled
        if (config.randomRotation) {
            float randomAngle = std::uniform_real_distribution<float>(0.0f, 360.0f)(rng);
            SetRotationDegrees(randomAngle);
        } else if (config.rotationVariance > 0.0f) {
            float variance = std::uniform_real_distribution<float>(
                -config.rotationVariance, config.rotationVariance)(rng);
            SetRotationDegrees(glm::degrees(m_rotation) + variance);
        }

        // Try to place
        Building* building = PlaceBuilding(world);
        if (building) {
            placed.push_back(building);
        }
    }

    // Restore original position
    m_previewPosition = startPos;
    m_previewGridPos.x = static_cast<int>(std::floor(startPos.x));
    m_previewGridPos.y = static_cast<int>(std::floor(startPos.z));

    return placed;
}

std::unique_ptr<Building> BuildingPlacer::CreatePreviewBuilding() const {
    auto building = std::make_unique<Building>(m_selectedBuilding);
    building->SetGridPosition(m_previewGridPos);
    building->SetRotation(m_rotation);
    building->SetState(BuildingState::Blueprint);
    return building;
}

// =============================================================================
// Multi-Placement Preview
// =============================================================================

std::vector<glm::vec3> BuildingPlacer::GetMultiPlacePreviewPositions() const {
    std::vector<glm::vec3> positions;

    glm::ivec2 size = GetBuildingSize(m_selectedBuilding);

    float spacingX = m_multiPlaceConfig.spacingX;
    float spacingZ = m_multiPlaceConfig.spacingZ;

    if (spacingX <= 0.0f) spacingX = static_cast<float>(size.x) + 1.0f;
    if (spacingZ <= 0.0f) spacingZ = static_cast<float>(size.y) + 1.0f;

    for (int i = 0; i < m_multiPlaceConfig.count; ++i) {
        int row = i / 5;
        int col = i % 5;

        positions.push_back(m_previewPosition + glm::vec3(
            col * spacingX,
            0.0f,
            row * spacingZ
        ));
    }

    return positions;
}

// =============================================================================
// Private Helpers
// =============================================================================

void BuildingPlacer::UpdateValidation(
    const TileMap& map,
    const std::vector<Building*>& buildings) {

    m_lastValidation = ValidatePlacement(map, buildings);

    if (m_onValidationChanged) {
        m_onValidationChanged(m_lastValidation);
    }
}

} // namespace Vehement
