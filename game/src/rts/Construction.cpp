#include "Construction.hpp"
#include "../world/World.hpp"
#include <algorithm>

namespace Vehement {

// ============================================================================
// Construction System Implementation
// ============================================================================

Construction::Construction() {
    m_ghost.isVisible = false;
}

void Construction::Initialize(World* world, TileMap* tileMap) {
    m_world = world;
    m_tileMap = tileMap;

    if (m_tileMap) {
        m_gridWidth = m_tileMap->GetWidth();
        m_gridHeight = m_tileMap->GetHeight();

        // Initialize occupancy grid
        m_occupancyGrid.resize(m_gridHeight);
        for (auto& row : m_occupancyGrid) {
            row.resize(m_gridWidth, nullptr);
        }
    }
}

void Construction::Update(float deltaTime) {
    // Update all buildings
    for (auto& building : m_buildings) {
        if (building && building->IsActive()) {
            building->Update(deltaTime);

            // Check for construction completion
            if (building->GetState() == BuildingState::UnderConstruction ||
                building->GetState() == BuildingState::Upgrading) {
                if (building->IsConstructionComplete()) {
                    if (m_onConstructionComplete) {
                        m_onConstructionComplete(building.get());
                    }
                }
            }
        }
    }

    // Remove destroyed buildings
    m_buildings.erase(
        std::remove_if(m_buildings.begin(), m_buildings.end(),
            [](const std::shared_ptr<Building>& b) {
                return !b || !b->IsActive() || b->IsMarkedForRemoval();
            }),
        m_buildings.end()
    );
}

void Construction::Render(Nova::Renderer& renderer, const Nova::Camera& camera) {
    // Render building ghost if placing
    if (m_ghost.isVisible && m_tileMap) {
        // Ghost rendering would be done with semi-transparent building texture
        // This integrates with the Nova renderer

        // Get ghost color based on validity
        glm::vec4 ghostColor = m_ghost.GetColor();

        // Calculate world bounds for ghost
        glm::vec3 worldPos = m_ghost.GetWorldPosition();
        glm::vec3 minPos(
            m_ghost.gridPosition.x * m_tileMap->GetTileSize(),
            0.0f,
            m_ghost.gridPosition.y * m_tileMap->GetTileSize()
        );
        glm::vec3 maxPos(
            (m_ghost.gridPosition.x + m_ghost.size.x) * m_tileMap->GetTileSize(),
            2.0f,  // Wall height
            (m_ghost.gridPosition.y + m_ghost.size.y) * m_tileMap->GetTileSize()
        );

        // Actual rendering would use Nova renderer to draw ghost quad
        // renderer.DrawGhost(minPos, maxPos, ghostColor, buildingTextures);
    }

    // Render construction progress bars for buildings under construction
    for (const auto& building : m_buildings) {
        if (building && building->IsUnderConstruction()) {
            float progress = building->GetConstructionProgress();
            glm::vec3 pos = building->GetPosition();

            // Draw progress bar above building
            // renderer.DrawProgressBar(pos + glm::vec3(0, 3, 0), progress, 2.0f, 0.2f);
        }
    }
}

// =========================================================================
// Building Placement
// =========================================================================

void Construction::StartPlacement(BuildingType type) {
    m_ghost.type = type;
    m_ghost.size = GetBuildingSize(type);
    m_ghost.isVisible = true;
    m_ghost.placementResult = PlacementResult::Valid;
}

void Construction::UpdateGhostPosition(const glm::ivec2& gridPos) {
    if (!m_ghost.isVisible) return;

    m_ghost.gridPosition = gridPos;
    m_ghost.placementResult = ValidatePlacement(m_ghost.type, gridPos);
}

void Construction::UpdateGhostPositionWorld(const glm::vec3& worldPos) {
    if (!m_tileMap) return;

    glm::ivec2 gridPos = m_tileMap->WorldToTile(worldPos.x, worldPos.z);
    UpdateGhostPosition(gridPos);
}

void Construction::CancelPlacement() {
    m_ghost.isVisible = false;
}

void Construction::RotateGhost() {
    // Swap width and height for asymmetric buildings
    std::swap(m_ghost.size.x, m_ghost.size.y);

    // Re-validate placement
    m_ghost.placementResult = ValidatePlacement(m_ghost.type, m_ghost.gridPosition);
}

// =========================================================================
// Placement Validation
// =========================================================================

PlacementResult Construction::ValidatePlacement(BuildingType type, const glm::ivec2& gridPos) const {
    glm::ivec2 size = GetBuildingSize(type);

    // Check bounds
    if (gridPos.x < 0 || gridPos.y < 0 ||
        gridPos.x + size.x > m_gridWidth ||
        gridPos.y + size.y > m_gridHeight) {
        return PlacementResult::OutOfBounds;
    }

    // Check if Command Center exists (required for most buildings)
    if (type != BuildingType::CommandCenter && !HasCommandCenter()) {
        return PlacementResult::RequiresCommandCenter;
    }

    // Check all tiles
    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            int tx = gridPos.x + dx;
            int ty = gridPos.y + dy;

            // Check terrain
            if (!IsTerrainBuildable(tx, ty)) {
                return PlacementResult::InvalidTerrain;
            }

            // Check occupancy
            if (m_occupancyGrid[ty][tx] != nullptr) {
                return PlacementResult::Occupied;
            }
        }
    }

    // Check for blocking pathways (optional - expensive check)
    // This would use pathfinding to ensure at least one path exists

    return PlacementResult::Valid;
}

bool Construction::IsTerrainBuildable(int x, int y) const {
    if (!m_tileMap) return false;

    const Tile* tile = m_tileMap->GetTile(x, y);
    if (!tile) return false;

    // Cannot build on water
    if (IsWaterTile(tile->type)) {
        return false;
    }

    // Cannot build on existing walls
    if (tile->isWall) {
        return false;
    }

    // Check tile walkability (some tiles might be impassable rocks, etc.)
    // For construction purposes, we allow building on non-walkable ground
    // as the building itself defines walkability

    return true;
}

bool Construction::AreTilesAvailable(const glm::ivec2& pos, const glm::ivec2& size) const {
    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            int tx = pos.x + dx;
            int ty = pos.y + dy;

            if (tx < 0 || tx >= m_gridWidth || ty < 0 || ty >= m_gridHeight) {
                return false;
            }

            if (m_occupancyGrid[ty][tx] != nullptr) {
                return false;
            }
        }
    }
    return true;
}

// =========================================================================
// Construction Actions
// =========================================================================

Building* Construction::ConfirmPlacement(ResourceStockpile& resources) {
    if (!m_ghost.isVisible) return nullptr;

    // Validate placement one more time
    PlacementResult result = ValidatePlacement(m_ghost.type, m_ghost.gridPosition);

    // Check resources
    BuildingCost cost = GetBuildingCost(m_ghost.type);
    if (result == PlacementResult::Valid && !resources.CanAfford(cost)) {
        result = PlacementResult::InsufficientResources;
    }

    if (m_onPlacement) {
        m_onPlacement(nullptr, result);
    }

    if (result != PlacementResult::Valid) {
        return nullptr;
    }

    // Spend resources
    if (!resources.Spend(cost)) {
        return nullptr;
    }

    // Create building
    auto building = std::make_shared<Building>(m_ghost.type);
    building->SetGridPosition(m_ghost.gridPosition);
    building->SetState(BuildingState::UnderConstruction);
    building->SetConstructionProgress(0.0f);

    // Update tile map occupancy
    UpdateTileMap(building.get(), true);

    // Add to buildings list
    m_buildings.push_back(building);

    // Set up completion callback
    building->SetOnConstructionComplete([this](Building& b) {
        RebuildNavigationGraph();
    });

    // Trigger callback
    if (m_onConstructionStart) {
        m_onConstructionStart(building.get());
    }

    // Hide ghost
    m_ghost.isVisible = false;

    return building.get();
}

Building* Construction::PlaceBlueprint(BuildingType type, const glm::ivec2& gridPos) {
    // Validate placement
    PlacementResult result = ValidatePlacement(type, gridPos);
    if (result != PlacementResult::Valid) {
        if (m_onPlacement) {
            m_onPlacement(nullptr, result);
        }
        return nullptr;
    }

    // Create building as blueprint
    auto building = std::make_shared<Building>(type);
    building->SetGridPosition(gridPos);
    building->SetState(BuildingState::Blueprint);
    building->SetConstructionProgress(0.0f);

    // Update tile map occupancy
    UpdateTileMap(building.get(), true);

    // Add to buildings list
    m_buildings.push_back(building);

    return building.get();
}

bool Construction::StartConstruction(Building* building, ResourceStockpile& resources) {
    if (!building) return false;
    if (building->GetState() != BuildingState::Blueprint) return false;

    // Check resources
    BuildingCost cost = GetBuildingCost(building->GetBuildingType());
    if (!resources.CanAfford(cost)) {
        return false;
    }

    // Spend resources
    if (!resources.Spend(cost)) {
        return false;
    }

    // Start construction
    building->SetState(BuildingState::UnderConstruction);

    if (m_onConstructionStart) {
        m_onConstructionStart(building);
    }

    return true;
}

void Construction::AddProgress(Building* building, int workerCount, float totalSkill, float deltaTime) {
    if (!building) return;
    if (building->GetState() != BuildingState::UnderConstruction &&
        building->GetState() != BuildingState::Upgrading) {
        return;
    }

    // Calculate progress
    BuildingCost cost = GetBuildingCost(building->GetBuildingType());
    float buildTime = cost.buildTime;

    // Base progress per second (100% / buildTime)
    float baseProgress = 100.0f / buildTime;

    // Worker multiplier: more workers = faster, but diminishing returns
    // 1 worker = 1x, 2 workers = 1.8x, 3 workers = 2.4x, etc.
    float workerMultiplier = 1.0f;
    if (workerCount > 0) {
        workerMultiplier = 1.0f + (workerCount - 1) * 0.6f;
    }

    // Skill multiplier (average skill of workers)
    float skillMultiplier = totalSkill / std::max(1, workerCount);

    // Final progress
    float progress = baseProgress * workerMultiplier * skillMultiplier * deltaTime;

    building->AddConstructionProgress(progress);
}

// =========================================================================
// Repair and Upgrade
// =========================================================================

bool Construction::RepairBuilding(Building* building, ResourceStockpile& resources) {
    if (!building) return false;
    if (building->GetState() != BuildingState::Damaged) return false;

    // Calculate damage percent
    float damagePercent = 1.0f - building->GetHealthPercent();
    if (damagePercent <= 0.0f) return false;

    // Get repair cost
    BuildingCost repairCost = GetRepairCost(building->GetBuildingType(), damagePercent);

    // Check and spend resources
    if (!resources.CanAfford(repairCost)) {
        return false;
    }

    resources.Spend(repairCost);

    // Fully heal building
    building->SetHealth(building->GetMaxHealth());
    building->SetState(BuildingState::Operational);

    return true;
}

bool Construction::UpgradeBuilding(Building* building, ResourceStockpile& resources) {
    if (!building) return false;
    if (!building->CanUpgrade()) return false;
    if (!building->IsOperational()) return false;

    // Get upgrade cost
    BuildingCost upgradeCost = GetUpgradeCost(building->GetBuildingType(), building->GetLevel());

    // Check and spend resources
    if (!resources.CanAfford(upgradeCost)) {
        return false;
    }

    resources.Spend(upgradeCost);

    // Start upgrade
    building->SetState(BuildingState::Upgrading);
    building->SetConstructionProgress(0.0f);

    return true;
}

bool Construction::DemolishBuilding(Building* building, ResourceStockpile& resources) {
    if (!building) return false;

    // Calculate refund
    BuildingCost refund = GetDemolitionRefund(building->GetBuildingType(), building->GetLevel());

    // Apply damage penalty (less refund for damaged buildings)
    float healthPercent = building->GetHealthPercent();
    refund = refund * healthPercent;

    // Give refund
    resources.AddRefund(refund);

    // Update tile map
    UpdateTileMap(building, false);

    // Clear workers
    building->ClearWorkers();

    // Mark for removal
    building->MarkForRemoval();

    // Callback
    if (m_onDemolition) {
        m_onDemolition(building, refund);
    }

    // Rebuild navigation
    RebuildNavigationGraph();

    return true;
}

// =========================================================================
// Building Queries
// =========================================================================

std::vector<Building*> Construction::GetBuildingsByType(BuildingType type) const {
    std::vector<Building*> result;
    for (const auto& building : m_buildings) {
        if (building && building->GetBuildingType() == type) {
            result.push_back(building.get());
        }
    }
    return result;
}

std::vector<Building*> Construction::GetBuildingsUnderConstruction() const {
    std::vector<Building*> result;
    for (const auto& building : m_buildings) {
        if (building && building->IsUnderConstruction()) {
            result.push_back(building.get());
        }
    }
    return result;
}

Building* Construction::GetBuildingAt(int x, int y) const {
    if (x < 0 || x >= m_gridWidth || y < 0 || y >= m_gridHeight) {
        return nullptr;
    }
    return m_occupancyGrid[y][x];
}

Building* Construction::GetBuildingAtWorld(const glm::vec3& worldPos) const {
    if (!m_tileMap) return nullptr;

    glm::ivec2 gridPos = m_tileMap->WorldToTile(worldPos.x, worldPos.z);
    return GetBuildingAt(gridPos.x, gridPos.y);
}

bool Construction::HasCommandCenter() const {
    for (const auto& building : m_buildings) {
        if (building && building->GetBuildingType() == BuildingType::CommandCenter &&
            !building->IsMarkedForRemoval()) {
            return true;
        }
    }
    return false;
}

Building* Construction::GetCommandCenter() const {
    for (const auto& building : m_buildings) {
        if (building && building->GetBuildingType() == BuildingType::CommandCenter &&
            !building->IsMarkedForRemoval()) {
            return building.get();
        }
    }
    return nullptr;
}

// =========================================================================
// Statistics
// =========================================================================

int Construction::GetTotalHousingCapacity() const {
    int total = 0;
    for (const auto& building : m_buildings) {
        if (building && building->IsOperational()) {
            total += building->GetHousingCapacity();
        }
    }
    return total;
}

int Construction::GetTotalWorkerCapacity() const {
    int total = 0;
    for (const auto& building : m_buildings) {
        if (building && building->IsOperational()) {
            total += building->GetWorkerCapacity();
        }
    }
    return total;
}

int Construction::GetBuildingCount(BuildingType type) const {
    int count = 0;
    for (const auto& building : m_buildings) {
        if (building && building->GetBuildingType() == type &&
            !building->IsMarkedForRemoval()) {
            count++;
        }
    }
    return count;
}

// =========================================================================
// Private Methods
// =========================================================================

void Construction::UpdateTileMap(Building* building, bool occupied) {
    if (!building || !m_tileMap) return;

    std::vector<glm::ivec2> tiles = building->GetOccupiedTiles();

    for (const auto& tilePos : tiles) {
        if (tilePos.x >= 0 && tilePos.x < m_gridWidth &&
            tilePos.y >= 0 && tilePos.y < m_gridHeight) {

            // Update occupancy grid
            m_occupancyGrid[tilePos.y][tilePos.x] = occupied ? building : nullptr;

            // Update tile map walkability
            Tile* tile = m_tileMap->GetTile(tilePos.x, tilePos.y);
            if (tile) {
                if (occupied) {
                    // Buildings block movement except gates when open
                    bool blocksMovement = true;
                    if (building->GetBuildingType() == BuildingType::Gate) {
                        blocksMovement = !building->IsGateOpen();
                    }
                    tile->isWalkable = !blocksMovement;
                    tile->blocksSight = building->GetBuildingType() != BuildingType::Farm;
                } else {
                    // Restore original tile properties
                    tile->isWalkable = true;
                    tile->blocksSight = false;
                }
            }
        }
    }

    m_tileMap->MarkDirty(building->GetGridPosition().x, building->GetGridPosition().y,
                          building->GetSize().x, building->GetSize().y);
}

void Construction::RebuildNavigationGraph() {
    if (m_world) {
        m_world->RebuildNavigationGraph();
    }
}

} // namespace Vehement
