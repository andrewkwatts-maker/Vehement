#include "BuildingPlacement.hpp"
#include "WorldBuilding.hpp"
#include "../world/TileMap.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <ctime>

namespace Vehement {
namespace RTS {

// ============================================================================
// BuildingPlacement Implementation
// ============================================================================

BuildingPlacement::BuildingPlacement() = default;

BuildingPlacement::~BuildingPlacement() = default;

bool BuildingPlacement::Initialize(WorldBuilding* worldBuilding, Voxel3DMap* voxelMap,
                                    TileMap* tileMap, ResourceStock* resources) {
    m_worldBuilding = worldBuilding;
    m_voxelMap = voxelMap;
    m_tileMap = tileMap;
    m_resources = resources;

    return m_worldBuilding && m_voxelMap && m_tileMap;
}

void BuildingPlacement::Update(float /* deltaTime */) {
    // Update ghost validity each frame
    if (m_ghost.visible) {
        UpdateGhostValidity();
    }
}

void BuildingPlacement::Render(Nova::Renderer& /* renderer */, const Nova::Camera& /* camera */) {
    // Render ghost preview
    if (!m_ghost.visible) return;

    // Would render:
    // - Semi-transparent preview at ghost position
    // - Red/green color based on validity
    // - Preview positions for multi-placement
    // - Resource cost overlay
}

// =========================================================================
// Ghost Preview
// =========================================================================

void BuildingPlacement::ShowPlacementGhost(TileType material) {
    m_ghost.visible = true;
    m_ghost.material = material;
    m_ghost.blueprint = nullptr;
    m_ghost.size = {1, 1, 1};
    m_ghost.rotation = 0.0f;

    UpdateGhostValidity();
}

void BuildingPlacement::ShowPlacementGhost(const Blueprint& bp) {
    m_ghost.visible = true;
    m_ghost.material = TileType::None;
    m_ghost.blueprint = &bp;
    m_ghost.size = bp.size;
    m_ghost.rotation = 0.0f;

    UpdateGhostValidity();
}

void BuildingPlacement::HidePlacementGhost() {
    m_ghost.visible = false;
    m_ghost.previewPositions.clear();
    m_isMultiPlacing = false;
}

void BuildingPlacement::UpdateGhostPosition(const glm::vec3& worldPos) {
    glm::ivec3 gridPos = SnapToGrid(worldPos);

    if (m_autoAlign) {
        AlignToExisting(gridPos);
    }

    m_ghost.position = gridPos;

    // Update multi-placement preview
    if (m_isMultiPlacing) {
        m_multiPlaceEnd = gridPos;

        switch (m_placementMode) {
            case PlacementMode::Line:
                CalculateLinePositions(m_multiPlaceStart, m_multiPlaceEnd);
                break;
            case PlacementMode::Rectangle:
                CalculateRectPositions(
                    glm::min(m_multiPlaceStart, m_multiPlaceEnd),
                    glm::max(m_multiPlaceStart, m_multiPlaceEnd));
                break;
            case PlacementMode::Circle:
                m_multiPlaceRadius = static_cast<int>(
                    glm::distance(glm::vec3(m_multiPlaceStart), glm::vec3(gridPos)));
                CalculateCirclePositions(m_multiPlaceStart, m_multiPlaceRadius);
                break;
            default:
                break;
        }
    }

    UpdateGhostValidity();
}

void BuildingPlacement::RotateGhost() {
    m_ghost.rotation += 90.0f;
    if (m_ghost.rotation >= 360.0f) {
        m_ghost.rotation -= 360.0f;
    }

    // Swap size X and Z
    std::swap(m_ghost.size.x, m_ghost.size.z);

    UpdateGhostValidity();
}

void BuildingPlacement::FlipGhostX() {
    // For blueprint placement, would flip the blueprint data
    UpdateGhostValidity();
}

void BuildingPlacement::FlipGhostZ() {
    UpdateGhostValidity();
}

std::string BuildingPlacement::GetPlacementIssueString() const {
    return PlacementIssueToString(m_ghost.issue);
}

void BuildingPlacement::UpdateGhostValidity() {
    if (m_ghost.blueprint) {
        m_ghost.issue = ValidateBlueprintPlacement(m_ghost.position, *m_ghost.blueprint);
        m_ghost.totalCost = m_ghost.blueprint->totalCost;
    } else if (m_ghost.material != TileType::None) {
        // Calculate total cost for multi-placement
        m_ghost.totalCost = ResourceCost();

        if (m_isMultiPlacing && !m_ghost.previewPositions.empty()) {
            m_ghost.issue = PlacementIssue::None;

            for (const auto& pos : m_ghost.previewPositions) {
                PlacementIssue posIssue = ValidatePlacement(pos, m_ghost.material);
                if (posIssue != PlacementIssue::None) {
                    m_ghost.issue = posIssue;
                }

                // Add cost
                ResourceCost posCost;
                if (m_ghost.material >= TileType::Wood1 && m_ghost.material <= TileType::WoodFlooring2) {
                    posCost.Add(ResourceType::Wood, 2);
                } else if (m_ghost.material >= TileType::StoneBlack) {
                    posCost.Add(ResourceType::Stone, 3);
                } else {
                    posCost.Add(ResourceType::Wood, 1);
                }
                m_ghost.totalCost = m_ghost.totalCost + posCost;
            }
        } else {
            m_ghost.issue = ValidatePlacement(m_ghost.position, m_ghost.material);
        }
    }

    m_ghost.isValid = (m_ghost.issue == PlacementIssue::None);

    // Check affordability
    if (m_ghost.isValid && m_resources && !m_ghost.totalCost.IsEmpty()) {
        if (!m_resources->CanAfford(m_ghost.totalCost)) {
            m_ghost.isValid = false;
            m_ghost.issue = PlacementIssue::InsufficientFunds;
        }
    }
}

// =========================================================================
// Placement Modes
// =========================================================================

void BuildingPlacement::SetPlacementMode(PlacementMode mode) {
    if (m_isMultiPlacing) {
        CancelMultiPlacement();
    }
    m_placementMode = mode;
}

void BuildingPlacement::StartLinePlacement(const glm::ivec3& start) {
    m_placementMode = PlacementMode::Line;
    m_isMultiPlacing = true;
    m_multiPlaceStart = start;
    m_multiPlaceEnd = start;
    m_ghost.previewPositions.clear();
}

void BuildingPlacement::UpdateLinePlacement(const glm::ivec3& end) {
    m_multiPlaceEnd = end;
    CalculateLinePositions(m_multiPlaceStart, m_multiPlaceEnd);
    UpdateGhostValidity();
}

void BuildingPlacement::StartRectPlacement(const glm::ivec3& corner1) {
    m_placementMode = PlacementMode::Rectangle;
    m_isMultiPlacing = true;
    m_multiPlaceStart = corner1;
    m_multiPlaceEnd = corner1;
    m_ghost.previewPositions.clear();
}

void BuildingPlacement::UpdateRectPlacement(const glm::ivec3& corner2) {
    m_multiPlaceEnd = corner2;
    CalculateRectPositions(
        glm::min(m_multiPlaceStart, m_multiPlaceEnd),
        glm::max(m_multiPlaceStart, m_multiPlaceEnd));
    UpdateGhostValidity();
}

void BuildingPlacement::StartCirclePlacement(const glm::ivec3& center) {
    m_placementMode = PlacementMode::Circle;
    m_isMultiPlacing = true;
    m_multiPlaceStart = center;
    m_multiPlaceRadius = 1;
    m_ghost.previewPositions.clear();
}

void BuildingPlacement::UpdateCirclePlacement(int radius) {
    m_multiPlaceRadius = std::max(1, radius);
    CalculateCirclePositions(m_multiPlaceStart, m_multiPlaceRadius);
    UpdateGhostValidity();
}

void BuildingPlacement::StartFillPlacement(const glm::ivec3& start) {
    m_placementMode = PlacementMode::Fill;
    m_isMultiPlacing = true;
    m_multiPlaceStart = start;
    CalculateFillPositions(start);
    UpdateGhostValidity();
}

void BuildingPlacement::CancelMultiPlacement() {
    m_isMultiPlacing = false;
    m_ghost.previewPositions.clear();
    UpdateGhostValidity();
}

bool BuildingPlacement::ConfirmPlacement() {
    if (!m_ghost.visible || !m_ghost.isValid) {
        return false;
    }

    if (!m_worldBuilding) return false;

    BuildOperation op;
    op.type = OperationType::Place;
    op.timestamp = std::time(nullptr);
    op.resourceDelta = m_ghost.totalCost;

    if (m_ghost.blueprint) {
        // Place blueprint
        op.type = OperationType::Blueprint;
        op.blueprintName = m_ghost.blueprint->name;
        op.blueprintPos = m_ghost.position;

        if (!m_worldBuilding->LoadBlueprint(m_ghost.blueprint->name, m_ghost.position)) {
            return false;
        }

        op.voxels = m_ghost.blueprint->voxels;
        for (auto& v : op.voxels) {
            v.position += m_ghost.position;
        }
    } else if (m_isMultiPlacing && !m_ghost.previewPositions.empty()) {
        // Multi-placement
        for (const auto& pos : m_ghost.previewPositions) {
            Voxel v;
            v.position = pos;
            v.type = m_ghost.material;
            v.isWall = true; // Assume wall for line/rect placement

            m_worldBuilding->PlaceWall(pos, {0, 0, 1}, m_ghost.material);
            op.voxels.push_back(v);
        }

        if (m_onPlacement) {
            m_onPlacement(m_ghost.previewPositions, m_ghost.material);
        }
    } else {
        // Single placement
        Voxel v;
        v.position = m_ghost.position;
        v.type = m_ghost.material;
        v.isFloor = true;

        m_worldBuilding->PlaceFloor(m_ghost.position, m_ghost.material);
        op.voxels.push_back(v);

        if (m_onPlacement) {
            m_onPlacement({m_ghost.position}, m_ghost.material);
        }
    }

    // Record for undo
    RecordOperation(op);

    // Reset multi-placement
    if (m_isMultiPlacing) {
        CancelMultiPlacement();
    }

    return true;
}

// =========================================================================
// Alignment and Snapping
// =========================================================================

glm::ivec3 BuildingPlacement::SnapToGrid(const glm::vec3& worldPos) const {
    if (!m_snapToGrid || m_gridSize <= 0) {
        return glm::ivec3(
            static_cast<int>(std::floor(worldPos.x)),
            static_cast<int>(std::floor(worldPos.y)),
            static_cast<int>(std::floor(worldPos.z))
        );
    }

    return glm::ivec3(
        static_cast<int>(std::round(worldPos.x / m_gridSize)) * m_gridSize,
        static_cast<int>(std::round(worldPos.y / m_gridSize)) * m_gridSize,
        static_cast<int>(std::round(worldPos.z / m_gridSize)) * m_gridSize
    );
}

void BuildingPlacement::AlignToExisting(glm::ivec3& pos) {
    if (!m_voxelMap || !m_autoAlign) return;

    // Check nearby positions for existing structures to align to
    const int searchRadius = 3;
    int bestDist = searchRadius + 1;
    glm::ivec3 bestAlign = pos;

    for (int dz = -searchRadius; dz <= searchRadius; ++dz) {
        for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
            glm::ivec3 checkPos = pos + glm::ivec3(dx, 0, dz);

            if (m_voxelMap->IsSolid(checkPos.x, checkPos.y, checkPos.z)) {
                // Found existing structure, check if we can align
                int dist = std::abs(dx) + std::abs(dz);

                if (dist < bestDist) {
                    // Align to edge of existing structure
                    if (dx != 0 || dz != 0) {
                        glm::ivec3 aligned = checkPos;
                        if (std::abs(dx) > std::abs(dz)) {
                            aligned.x = checkPos.x + (dx > 0 ? 1 : -1);
                        } else {
                            aligned.z = checkPos.z + (dz > 0 ? 1 : -1);
                        }

                        if (!m_voxelMap->IsSolid(aligned.x, aligned.y, aligned.z)) {
                            bestAlign = aligned;
                            bestDist = dist;
                        }
                    }
                }
            }
        }
    }

    if (bestDist < searchRadius + 1) {
        pos = bestAlign;
    }
}

// =========================================================================
// Undo/Redo
// =========================================================================

bool BuildingPlacement::Undo() {
    if (m_undoStack.empty()) return false;

    BuildOperation op = m_undoStack.top();
    m_undoStack.pop();

    ApplyOperation(op, true); // Reverse the operation

    m_redoStack.push(op);

    if (m_onUndoRedo) {
        m_onUndoRedo(true, op);
    }

    return true;
}

bool BuildingPlacement::Redo() {
    if (m_redoStack.empty()) return false;

    BuildOperation op = m_redoStack.top();
    m_redoStack.pop();

    ApplyOperation(op, false); // Re-apply the operation

    m_undoStack.push(op);

    if (m_onUndoRedo) {
        m_onUndoRedo(false, op);
    }

    return true;
}

void BuildingPlacement::ClearHistory() {
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();
}

void BuildingPlacement::RecordOperation(const BuildOperation& op) {
    // Clear redo stack on new operation
    while (!m_redoStack.empty()) m_redoStack.pop();

    m_undoStack.push(op);

    // Limit undo history
    if (m_undoStack.size() > m_maxUndoHistory) {
        // Would need to convert to deque to remove from bottom
        // For now, just allow unlimited within stack
    }
}

void BuildingPlacement::ApplyOperation(const BuildOperation& op, bool reverse) {
    if (!m_voxelMap || !m_worldBuilding) return;

    switch (op.type) {
        case OperationType::Place:
            if (reverse) {
                // Remove placed voxels
                for (const auto& v : op.voxels) {
                    m_voxelMap->RemoveVoxel(v.position);
                }
                // Refund resources
                if (m_resources) {
                    for (const auto& [type, amount] : op.resourceDelta.costs) {
                        m_resources->Add(type, amount);
                    }
                }
            } else {
                // Re-place voxels
                for (const auto& v : op.voxels) {
                    m_voxelMap->SetVoxel(v.position, v);
                }
                // Spend resources
                if (m_resources) {
                    m_resources->Spend(op.resourceDelta);
                }
            }
            break;

        case OperationType::Remove:
            if (reverse) {
                // Restore removed voxels
                for (const auto& v : op.voxels) {
                    m_voxelMap->SetVoxel(v.position, v);
                }
            } else {
                // Remove again
                for (const auto& v : op.voxels) {
                    m_voxelMap->RemoveVoxel(v.position);
                }
            }
            break;

        case OperationType::Terraform:
            // Would restore terrain heights
            break;

        case OperationType::Blueprint:
            if (reverse) {
                for (const auto& v : op.voxels) {
                    m_voxelMap->RemoveVoxel(v.position);
                }
                if (m_resources) {
                    for (const auto& [type, amount] : op.resourceDelta.costs) {
                        m_resources->Add(type, amount);
                    }
                }
            } else {
                for (const auto& v : op.voxels) {
                    m_voxelMap->SetVoxel(v.position, v);
                }
                if (m_resources) {
                    m_resources->Spend(op.resourceDelta);
                }
            }
            break;
    }
}

// =========================================================================
// Validation
// =========================================================================

PlacementIssue BuildingPlacement::ValidatePlacement(const glm::ivec3& pos, TileType /* type */) const {
    if (!m_voxelMap) return PlacementIssue::OutOfBounds;

    // Check bounds
    if (!m_voxelMap->IsInBounds(pos)) {
        return PlacementIssue::OutOfBounds;
    }

    // Check occupied
    if (m_voxelMap->IsSolid(pos.x, pos.y, pos.z)) {
        return PlacementIssue::Occupied;
    }

    // Check height limit
    if (pos.y > 64) {
        return PlacementIssue::TooHigh;
    }

    // Check support for non-ground placements
    if (pos.y > 0) {
        bool hasSupport = m_voxelMap->IsSolid(pos.x, pos.y - 1, pos.z) ||
                          m_voxelMap->IsSolid(pos.x - 1, pos.y, pos.z) ||
                          m_voxelMap->IsSolid(pos.x + 1, pos.y, pos.z) ||
                          m_voxelMap->IsSolid(pos.x, pos.y, pos.z - 1) ||
                          m_voxelMap->IsSolid(pos.x, pos.y, pos.z + 1);

        if (!hasSupport) {
            return PlacementIssue::NoSupport;
        }
    }

    // Custom validation rules
    for (const auto& rule : m_validationRules) {
        PlacementIssue issue = rule(pos);
        if (issue != PlacementIssue::None) {
            return issue;
        }
    }

    return PlacementIssue::None;
}

PlacementIssue BuildingPlacement::ValidateBlueprintPlacement(const glm::ivec3& pos,
                                                              const Blueprint& bp) const {
    if (!m_voxelMap) return PlacementIssue::OutOfBounds;

    for (const auto& v : bp.voxels) {
        glm::ivec3 worldPos = v.position + pos;

        if (!m_voxelMap->IsInBounds(worldPos)) {
            return PlacementIssue::OutOfBounds;
        }

        if (m_voxelMap->IsSolid(worldPos.x, worldPos.y, worldPos.z)) {
            return PlacementIssue::Occupied;
        }
    }

    return PlacementIssue::None;
}

bool BuildingPlacement::IsAreaClear(const glm::ivec3& min, const glm::ivec3& max) const {
    if (!m_voxelMap) return false;

    for (int y = min.y; y <= max.y; ++y) {
        for (int z = min.z; z <= max.z; ++z) {
            for (int x = min.x; x <= max.x; ++x) {
                if (m_voxelMap->IsSolid(x, y, z)) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::vector<glm::ivec3> BuildingPlacement::GetBlockedPositions(
    const glm::ivec3& pos, const Blueprint& bp) const {

    std::vector<glm::ivec3> blocked;

    if (!m_voxelMap) return blocked;

    for (const auto& v : bp.voxels) {
        glm::ivec3 worldPos = v.position + pos;

        if (m_voxelMap->IsSolid(worldPos.x, worldPos.y, worldPos.z)) {
            blocked.push_back(worldPos);
        }
    }

    return blocked;
}

// =========================================================================
// Position Calculation Helpers
// =========================================================================

void BuildingPlacement::CalculateLinePositions(const glm::ivec3& start, const glm::ivec3& end) {
    m_ghost.previewPositions.clear();

    // Bresenham's line in 3D (XZ plane)
    int dx = std::abs(end.x - start.x);
    int dz = std::abs(end.z - start.z);
    int sx = (start.x < end.x) ? 1 : -1;
    int sz = (start.z < end.z) ? 1 : -1;
    int err = dx - dz;

    int x = start.x;
    int z = start.z;

    while (true) {
        m_ghost.previewPositions.push_back({x, start.y, z});

        if (x == end.x && z == end.z) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }
}

void BuildingPlacement::CalculateRectPositions(const glm::ivec3& min, const glm::ivec3& max) {
    m_ghost.previewPositions.clear();

    // Just the perimeter
    for (int x = min.x; x <= max.x; ++x) {
        m_ghost.previewPositions.push_back({x, min.y, min.z});
        if (max.z != min.z) {
            m_ghost.previewPositions.push_back({x, min.y, max.z});
        }
    }

    for (int z = min.z + 1; z < max.z; ++z) {
        m_ghost.previewPositions.push_back({min.x, min.y, z});
        if (max.x != min.x) {
            m_ghost.previewPositions.push_back({max.x, min.y, z});
        }
    }
}

void BuildingPlacement::CalculateCirclePositions(const glm::ivec3& center, int radius) {
    m_ghost.previewPositions.clear();

    // Midpoint circle algorithm
    int x = radius;
    int z = 0;
    int err = 0;

    while (x >= z) {
        m_ghost.previewPositions.push_back({center.x + x, center.y, center.z + z});
        m_ghost.previewPositions.push_back({center.x + z, center.y, center.z + x});
        m_ghost.previewPositions.push_back({center.x - z, center.y, center.z + x});
        m_ghost.previewPositions.push_back({center.x - x, center.y, center.z + z});
        m_ghost.previewPositions.push_back({center.x - x, center.y, center.z - z});
        m_ghost.previewPositions.push_back({center.x - z, center.y, center.z - x});
        m_ghost.previewPositions.push_back({center.x + z, center.y, center.z - x});
        m_ghost.previewPositions.push_back({center.x + x, center.y, center.z - z});

        if (err <= 0) {
            z++;
            err += 2 * z + 1;
        }

        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }

    // Remove duplicates
    std::sort(m_ghost.previewPositions.begin(), m_ghost.previewPositions.end(),
        [](const glm::ivec3& a, const glm::ivec3& b) {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });

    m_ghost.previewPositions.erase(
        std::unique(m_ghost.previewPositions.begin(), m_ghost.previewPositions.end(),
            [](const glm::ivec3& a, const glm::ivec3& b) {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            }),
        m_ghost.previewPositions.end());
}

void BuildingPlacement::CalculateFillPositions(const glm::ivec3& start) {
    m_ghost.previewPositions.clear();

    if (!m_voxelMap) return;

    // Flood fill from start position
    std::queue<glm::ivec3> queue;
    std::vector<std::vector<bool>> visited(
        m_voxelMap->GetWidth(),
        std::vector<bool>(m_voxelMap->GetDepth(), false));

    queue.push(start);

    const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    while (!queue.empty() && m_ghost.previewPositions.size() < 1000) {
        glm::ivec3 pos = queue.front();
        queue.pop();

        if (pos.x < 0 || pos.x >= m_voxelMap->GetWidth() ||
            pos.z < 0 || pos.z >= m_voxelMap->GetDepth()) {
            continue;
        }

        if (visited[pos.x][pos.z]) continue;
        visited[pos.x][pos.z] = true;

        if (m_voxelMap->IsSolid(pos.x, pos.y, pos.z)) {
            continue; // Hit boundary
        }

        m_ghost.previewPositions.push_back(pos);

        for (const auto& dir : directions) {
            queue.push(pos + dir);
        }
    }
}

// ============================================================================
// WallPlacer Implementation
// ============================================================================

WallPlacer::WallPlacer(BuildingPlacement* placement)
    : m_placement(placement) {}

void WallPlacer::SetMaterial(TileType type) {
    m_material = type;
}

void WallPlacer::SetHeight(int height) {
    m_height = std::max(1, height);
}

void WallPlacer::SetThickness(int thickness) {
    m_thickness = std::max(1, thickness);
}

void WallPlacer::PlaceStraightWall(const glm::ivec3& start, const glm::ivec3& end) {
    if (!m_placement) return;

    m_placement->ShowPlacementGhost(m_material);
    m_placement->StartLinePlacement(start);
    m_placement->UpdateLinePlacement(end);
    m_placement->ConfirmPlacement();
}

void WallPlacer::PlaceRectangularWall(const glm::ivec3& min, const glm::ivec3& max) {
    if (!m_placement) return;

    m_placement->ShowPlacementGhost(m_material);
    m_placement->StartRectPlacement(min);
    m_placement->UpdateRectPlacement(max);
    m_placement->ConfirmPlacement();
}

void WallPlacer::PlaceCircularWall(const glm::ivec3& center, int radius) {
    if (!m_placement) return;

    m_placement->ShowPlacementGhost(m_material);
    m_placement->StartCirclePlacement(center);
    m_placement->UpdateCirclePlacement(radius);
    m_placement->ConfirmPlacement();
}

// ============================================================================
// FloorPlacer Implementation
// ============================================================================

FloorPlacer::FloorPlacer(BuildingPlacement* placement)
    : m_placement(placement) {}

void FloorPlacer::SetMaterial(TileType type) {
    m_material = type;
}

void FloorPlacer::PlaceRectangle(const glm::ivec3& min, const glm::ivec3& max) {
    if (!m_placement) return;

    m_placement->ShowPlacementGhost(m_material);
    m_placement->SetPlacementMode(PlacementMode::Rectangle);
    m_placement->StartRectPlacement(min);
    m_placement->UpdateRectPlacement(max);
    m_placement->ConfirmPlacement();
}

void FloorPlacer::PlaceCircle(const glm::ivec3& center, int radius) {
    if (!m_placement) return;

    m_placement->ShowPlacementGhost(m_material);
    m_placement->StartCirclePlacement(center);
    m_placement->UpdateCirclePlacement(radius);
    m_placement->ConfirmPlacement();
}

void FloorPlacer::PlacePolygon(const std::vector<glm::ivec3>& /* vertices */) {
    // Would implement polygon fill algorithm
}

// ============================================================================
// RoomPlacer Implementation
// ============================================================================

RoomPlacer::RoomPlacer(BuildingPlacement* placement)
    : m_placement(placement) {}

void RoomPlacer::SetWallMaterial(TileType type) {
    m_wallMaterial = type;
}

void RoomPlacer::SetFloorMaterial(TileType type) {
    m_floorMaterial = type;
}

void RoomPlacer::SetRoofMaterial(TileType type) {
    m_roofMaterial = type;
}

void RoomPlacer::SetWallHeight(int height) {
    m_wallHeight = std::max(1, height);
}

void RoomPlacer::PlaceRoom(const glm::ivec3& min, const glm::ivec3& max,
                           bool /* addDoor */, bool /* addWindows */) {
    if (!m_placement) return;

    // Place floor
    FloorPlacer floor(m_placement);
    floor.SetMaterial(m_floorMaterial);
    floor.PlaceRectangle(min, max);

    // Place walls
    WallPlacer walls(m_placement);
    walls.SetMaterial(m_wallMaterial);
    walls.SetHeight(m_wallHeight);
    walls.PlaceRectangularWall(min, max);

    // Would add door and windows here
}

} // namespace RTS
} // namespace Vehement
