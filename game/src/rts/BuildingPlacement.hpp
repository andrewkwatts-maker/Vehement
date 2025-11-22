#pragma once

/**
 * @file BuildingPlacement.hpp
 * @brief Smart building placement system
 *
 * Provides intelligent placement tools:
 * - Ghost preview with validity feedback
 * - Snap-to-grid and alignment to existing structures
 * - Multi-placement modes (line, rectangle, fill)
 * - Undo/redo for building operations
 */

#include "../world/Tile.hpp"
#include "Resource.hpp"
#include "Blueprint.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <stack>
#include <memory>
#include <functional>
#include <optional>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

class TileMap;

namespace RTS {

// Forward declarations
class Voxel3DMap;
class WorldBuilding;
struct Blueprint;

// ============================================================================
// Placement Mode
// ============================================================================

/**
 * @brief Multi-placement modes for efficient building
 */
enum class PlacementMode : uint8_t {
    Single,         ///< Place single elements
    Line,           ///< Place in a line (walls)
    Rectangle,      ///< Fill a rectangle
    Circle,         ///< Fill/outline a circle
    Fill,           ///< Flood fill enclosed area
    Blueprint,      ///< Place entire blueprint
    Free            ///< Free-form painting mode
};

/**
 * @brief Get placement mode name
 */
inline const char* PlacementModeToString(PlacementMode mode) {
    switch (mode) {
        case PlacementMode::Single:     return "Single";
        case PlacementMode::Line:       return "Line";
        case PlacementMode::Rectangle:  return "Rectangle";
        case PlacementMode::Circle:     return "Circle";
        case PlacementMode::Fill:       return "Fill";
        case PlacementMode::Blueprint:  return "Blueprint";
        case PlacementMode::Free:       return "Free Paint";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Placement Validity
// ============================================================================

/**
 * @brief Why a placement is invalid
 */
enum class PlacementIssue : uint8_t {
    None,               ///< No issues, placement valid
    OutOfBounds,        ///< Outside world boundaries
    Occupied,           ///< Position already has structure
    NoSupport,          ///< Would lack structural support
    BlocksPath,         ///< Would block all pathfinding
    InsufficientFunds,  ///< Cannot afford
    RequiresFloor,      ///< Needs floor beneath
    RequiresWall,       ///< Must attach to wall
    InvalidTerrain,     ///< Terrain doesn't support building
    TooHigh,            ///< Exceeds max build height
    Restricted          ///< Building restricted in this area
};

/**
 * @brief Get issue description
 */
inline const char* PlacementIssueToString(PlacementIssue issue) {
    switch (issue) {
        case PlacementIssue::None:              return "Valid";
        case PlacementIssue::OutOfBounds:       return "Out of bounds";
        case PlacementIssue::Occupied:          return "Position occupied";
        case PlacementIssue::NoSupport:         return "No structural support";
        case PlacementIssue::BlocksPath:        return "Would block pathways";
        case PlacementIssue::InsufficientFunds: return "Cannot afford";
        case PlacementIssue::RequiresFloor:     return "Requires floor";
        case PlacementIssue::RequiresWall:      return "Must attach to wall";
        case PlacementIssue::InvalidTerrain:    return "Invalid terrain";
        case PlacementIssue::TooHigh:           return "Too high";
        case PlacementIssue::Restricted:        return "Restricted area";
        default:                                return "Unknown issue";
    }
}

// ============================================================================
// Placement Ghost
// ============================================================================

/**
 * @brief Preview of pending placement
 */
struct PlacementGhost {
    bool visible = false;
    glm::ivec3 position{0, 0, 0};
    glm::ivec3 size{1, 1, 1};
    float rotation = 0.0f;

    TileType material = TileType::None;
    const Blueprint* blueprint = nullptr;

    bool isValid = false;
    PlacementIssue issue = PlacementIssue::None;

    // For multi-placement
    std::vector<glm::ivec3> previewPositions;

    // Resource cost preview
    ResourceCost totalCost;

    /**
     * @brief Get ghost color based on validity
     */
    [[nodiscard]] glm::vec4 GetColor() const {
        if (isValid) {
            return glm::vec4(0.0f, 1.0f, 0.0f, 0.5f); // Green
        } else {
            return glm::vec4(1.0f, 0.0f, 0.0f, 0.5f); // Red
        }
    }

    /**
     * @brief Get world position for rendering
     */
    [[nodiscard]] glm::vec3 GetWorldPosition() const {
        return glm::vec3(
            position.x + size.x * 0.5f,
            position.y + size.y * 0.5f,
            position.z + size.z * 0.5f
        );
    }
};

// ============================================================================
// Undo/Redo System
// ============================================================================

/**
 * @brief Types of building operations for undo
 */
enum class OperationType : uint8_t {
    Place,          ///< Placed voxel(s)
    Remove,         ///< Removed voxel(s)
    Terraform,      ///< Modified terrain
    Blueprint       ///< Placed blueprint
};

/**
 * @brief Single building operation (for undo/redo)
 */
struct BuildOperation {
    OperationType type;
    int64_t timestamp;

    // For Place/Remove
    std::vector<Voxel> voxels;

    // For Terraform
    std::vector<std::pair<glm::ivec3, int>> terrainChanges; // position, old height

    // For Blueprint
    std::string blueprintName;
    glm::ivec3 blueprintPos;

    // Resource cost/refund
    ResourceCost resourceDelta;
};

// ============================================================================
// Building Placement System
// ============================================================================

/**
 * @brief Smart building placement with preview and undo support
 */
class BuildingPlacement {
public:
    BuildingPlacement();
    ~BuildingPlacement();

    // Non-copyable
    BuildingPlacement(const BuildingPlacement&) = delete;
    BuildingPlacement& operator=(const BuildingPlacement&) = delete;

    /**
     * @brief Initialize the placement system
     */
    bool Initialize(WorldBuilding* worldBuilding, Voxel3DMap* voxelMap,
                    TileMap* tileMap, ResourceStock* resources);

    /**
     * @brief Update placement preview based on input
     */
    void Update(float deltaTime);

    /**
     * @brief Render placement ghost and UI
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Ghost Preview
    // =========================================================================

    /**
     * @brief Show placement ghost for material
     */
    void ShowPlacementGhost(TileType material);

    /**
     * @brief Show placement ghost for blueprint
     */
    void ShowPlacementGhost(const Blueprint& bp);

    /**
     * @brief Hide placement ghost
     */
    void HidePlacementGhost();

    /**
     * @brief Update ghost position
     */
    void UpdateGhostPosition(const glm::vec3& worldPos);

    /**
     * @brief Rotate ghost 90 degrees
     */
    void RotateGhost();

    /**
     * @brief Flip ghost X axis
     */
    void FlipGhostX();

    /**
     * @brief Flip ghost Z axis
     */
    void FlipGhostZ();

    /**
     * @brief Get current ghost
     */
    [[nodiscard]] const PlacementGhost& GetGhost() const { return m_ghost; }

    /**
     * @brief Check if placement is valid
     */
    [[nodiscard]] bool IsPlacementValid() const { return m_ghost.isValid; }

    /**
     * @brief Get placement issue description
     */
    [[nodiscard]] std::string GetPlacementIssueString() const;

    // =========================================================================
    // Placement Modes
    // =========================================================================

    /**
     * @brief Set placement mode
     */
    void SetPlacementMode(PlacementMode mode);

    /**
     * @brief Get current placement mode
     */
    [[nodiscard]] PlacementMode GetPlacementMode() const { return m_placementMode; }

    /**
     * @brief Start line placement
     */
    void StartLinePlacement(const glm::ivec3& start);

    /**
     * @brief Update line placement endpoint
     */
    void UpdateLinePlacement(const glm::ivec3& end);

    /**
     * @brief Start rectangle placement
     */
    void StartRectPlacement(const glm::ivec3& corner1);

    /**
     * @brief Update rectangle second corner
     */
    void UpdateRectPlacement(const glm::ivec3& corner2);

    /**
     * @brief Start circle placement
     */
    void StartCirclePlacement(const glm::ivec3& center);

    /**
     * @brief Update circle radius
     */
    void UpdateCirclePlacement(int radius);

    /**
     * @brief Start fill placement
     */
    void StartFillPlacement(const glm::ivec3& start);

    /**
     * @brief Cancel current multi-placement
     */
    void CancelMultiPlacement();

    /**
     * @brief Confirm current placement
     */
    bool ConfirmPlacement();

    // =========================================================================
    // Alignment and Snapping
    // =========================================================================

    /**
     * @brief Snap world position to grid
     */
    [[nodiscard]] glm::ivec3 SnapToGrid(const glm::vec3& worldPos) const;

    /**
     * @brief Enable/disable snap to grid
     */
    void SetSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

    /**
     * @brief Check if snap to grid enabled
     */
    [[nodiscard]] bool IsSnapToGrid() const { return m_snapToGrid; }

    /**
     * @brief Set grid snap size
     */
    void SetGridSize(int size) { m_gridSize = size; }

    /**
     * @brief Get grid snap size
     */
    [[nodiscard]] int GetGridSize() const { return m_gridSize; }

    /**
     * @brief Align position to existing structures
     */
    void AlignToExisting(glm::ivec3& pos);

    /**
     * @brief Enable/disable auto-alignment
     */
    void SetAutoAlign(bool enabled) { m_autoAlign = enabled; }

    /**
     * @brief Check if auto-alignment enabled
     */
    [[nodiscard]] bool IsAutoAlign() const { return m_autoAlign; }

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last operation
     */
    bool Undo();

    /**
     * @brief Redo last undone operation
     */
    bool Redo();

    /**
     * @brief Check if can undo
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if can redo
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief Get undo stack size
     */
    [[nodiscard]] size_t GetUndoStackSize() const { return m_undoStack.size(); }

    /**
     * @brief Get redo stack size
     */
    [[nodiscard]] size_t GetRedoStackSize() const { return m_redoStack.size(); }

    /**
     * @brief Clear undo/redo history
     */
    void ClearHistory();

    /**
     * @brief Set max undo history size
     */
    void SetMaxUndoHistory(size_t max) { m_maxUndoHistory = max; }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate placement at position
     */
    [[nodiscard]] PlacementIssue ValidatePlacement(const glm::ivec3& pos,
                                                    TileType type) const;

    /**
     * @brief Validate blueprint placement
     */
    [[nodiscard]] PlacementIssue ValidateBlueprintPlacement(const glm::ivec3& pos,
                                                             const Blueprint& bp) const;

    /**
     * @brief Check if area is clear
     */
    [[nodiscard]] bool IsAreaClear(const glm::ivec3& min, const glm::ivec3& max) const;

    /**
     * @brief Get positions that would be blocked
     */
    [[nodiscard]] std::vector<glm::ivec3> GetBlockedPositions(
        const glm::ivec3& pos, const Blueprint& bp) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using PlacementCallback = std::function<void(const std::vector<glm::ivec3>& positions,
                                                  TileType type)>;
    using UndoRedoCallback = std::function<void(bool isUndo, const BuildOperation& op)>;
    using ValidationCallback = std::function<PlacementIssue(const glm::ivec3& pos)>;

    void SetOnPlacement(PlacementCallback cb) { m_onPlacement = std::move(cb); }
    void SetOnUndoRedo(UndoRedoCallback cb) { m_onUndoRedo = std::move(cb); }
    void AddValidationRule(ValidationCallback rule) { m_validationRules.push_back(std::move(rule)); }

private:
    void UpdateGhostValidity();
    void CalculateLinePositions(const glm::ivec3& start, const glm::ivec3& end);
    void CalculateRectPositions(const glm::ivec3& min, const glm::ivec3& max);
    void CalculateCirclePositions(const glm::ivec3& center, int radius);
    void CalculateFillPositions(const glm::ivec3& start);
    void RecordOperation(const BuildOperation& op);
    void ApplyOperation(const BuildOperation& op, bool reverse);

    WorldBuilding* m_worldBuilding = nullptr;
    Voxel3DMap* m_voxelMap = nullptr;
    TileMap* m_tileMap = nullptr;
    ResourceStock* m_resources = nullptr;

    // Ghost
    PlacementGhost m_ghost;

    // Mode and state
    PlacementMode m_placementMode = PlacementMode::Single;
    bool m_isMultiPlacing = false;
    glm::ivec3 m_multiPlaceStart{0};
    glm::ivec3 m_multiPlaceEnd{0};
    int m_multiPlaceRadius = 1;

    // Alignment
    bool m_snapToGrid = true;
    int m_gridSize = 1;
    bool m_autoAlign = true;

    // Undo/Redo
    std::stack<BuildOperation> m_undoStack;
    std::stack<BuildOperation> m_redoStack;
    size_t m_maxUndoHistory = 100;

    // Callbacks
    PlacementCallback m_onPlacement;
    UndoRedoCallback m_onUndoRedo;
    std::vector<ValidationCallback> m_validationRules;
};

// ============================================================================
// Quick Placement Helpers
// ============================================================================

/**
 * @brief Helper for quick wall placement
 */
class WallPlacer {
public:
    WallPlacer(BuildingPlacement* placement);

    void SetMaterial(TileType type);
    void SetHeight(int height);
    void SetThickness(int thickness);

    void PlaceStraightWall(const glm::ivec3& start, const glm::ivec3& end);
    void PlaceRectangularWall(const glm::ivec3& min, const glm::ivec3& max);
    void PlaceCircularWall(const glm::ivec3& center, int radius);

private:
    BuildingPlacement* m_placement;
    TileType m_material = TileType::BricksGrey;
    int m_height = 3;
    int m_thickness = 1;
};

/**
 * @brief Helper for quick floor placement
 */
class FloorPlacer {
public:
    FloorPlacer(BuildingPlacement* placement);

    void SetMaterial(TileType type);

    void PlaceRectangle(const glm::ivec3& min, const glm::ivec3& max);
    void PlaceCircle(const glm::ivec3& center, int radius);
    void PlacePolygon(const std::vector<glm::ivec3>& vertices);

private:
    BuildingPlacement* m_placement;
    TileType m_material = TileType::WoodFlooring1;
};

/**
 * @brief Helper for quick room placement
 */
class RoomPlacer {
public:
    RoomPlacer(BuildingPlacement* placement);

    void SetWallMaterial(TileType type);
    void SetFloorMaterial(TileType type);
    void SetRoofMaterial(TileType type);
    void SetWallHeight(int height);

    void PlaceRoom(const glm::ivec3& min, const glm::ivec3& max,
                   bool addDoor = true, bool addWindows = true);

private:
    BuildingPlacement* m_placement;
    TileType m_wallMaterial = TileType::BricksGrey;
    TileType m_floorMaterial = TileType::WoodFlooring1;
    TileType m_roofMaterial = TileType::Wood1;
    int m_wallHeight = 3;
};

} // namespace RTS
} // namespace Vehement
