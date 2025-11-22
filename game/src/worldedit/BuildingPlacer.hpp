#pragma once

#include "../rts/Building.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace Vehement {

// Forward declarations
class TileMap;
class World;

/**
 * @brief Rotation snap angle options
 */
enum class RotationSnap : uint8_t {
    Free,       ///< No snapping
    Snap45,     ///< 45 degree increments
    Snap90,     ///< 90 degree increments
    Snap180     ///< 180 degree increments (front/back only)
};

/**
 * @brief Get display name for rotation snap
 */
inline const char* GetRotationSnapName(RotationSnap snap) {
    switch (snap) {
        case RotationSnap::Free:    return "Free";
        case RotationSnap::Snap45:  return "45 Degrees";
        case RotationSnap::Snap90:  return "90 Degrees";
        case RotationSnap::Snap180: return "180 Degrees";
        default:                    return "Unknown";
    }
}

/**
 * @brief Result of placement validation
 */
struct PlacementValidation {
    bool valid = false;
    bool hasCollision = false;
    bool terrainBlocked = false;
    bool outOfBounds = false;
    bool resourcesInsufficient = false;
    std::string errorMessage;
    std::vector<glm::ivec2> blockedTiles;
};

/**
 * @brief Configuration for multi-placement
 */
struct MultiPlaceConfig {
    int count = 1;                  ///< Number of buildings to place
    float spacingX = 0.0f;          ///< X spacing between buildings (0 = auto)
    float spacingZ = 0.0f;          ///< Z spacing between buildings
    bool randomRotation = false;    ///< Randomize rotation for each
    bool randomVariant = false;     ///< Randomize building variant
    float rotationVariance = 0.0f;  ///< Random rotation variance (degrees)
};

/**
 * @brief Manual building placement tool for world editing
 *
 * Features:
 * - Select building from configuration list
 * - Preview placement with collision checking
 * - Rotation snapping options
 * - Auto-align to nearby roads
 * - Place multiple buildings with spacing
 * - Randomize rotation and variants
 *
 * Usage:
 * 1. Select building type from config
 * 2. Move preview to desired position
 * 3. Validate placement
 * 4. Confirm to place building
 */
class BuildingPlacer {
public:
    BuildingPlacer();
    ~BuildingPlacer() = default;

    // Allow copy and move
    BuildingPlacer(const BuildingPlacer&) = default;
    BuildingPlacer& operator=(const BuildingPlacer&) = default;
    BuildingPlacer(BuildingPlacer&&) noexcept = default;
    BuildingPlacer& operator=(BuildingPlacer&&) noexcept = default;

    // =========================================================================
    // Building Selection
    // =========================================================================

    /** @brief Get available building types */
    [[nodiscard]] std::vector<BuildingType> GetAvailableBuildings() const;

    /** @brief Get selected building type */
    [[nodiscard]] BuildingType GetSelectedBuilding() const noexcept { return m_selectedBuilding; }

    /** @brief Set selected building type */
    void SetSelectedBuilding(BuildingType type);

    /** @brief Get selected building variant */
    [[nodiscard]] int GetSelectedVariant() const noexcept { return m_selectedVariant; }

    /** @brief Set selected building variant */
    void SetSelectedVariant(int variant) noexcept { m_selectedVariant = variant; }

    /** @brief Get available variants for selected building */
    [[nodiscard]] int GetVariantCount() const;

    // =========================================================================
    // Preview Position
    // =========================================================================

    /** @brief Get current preview position */
    [[nodiscard]] const glm::vec3& GetPreviewPosition() const noexcept { return m_previewPosition; }

    /** @brief Set preview position */
    void SetPreviewPosition(const glm::vec3& position);

    /** @brief Set preview position from tile coordinates */
    void SetPreviewTile(int tileX, int tileY);

    /** @brief Get preview grid position */
    [[nodiscard]] glm::ivec2 GetPreviewGridPosition() const noexcept { return m_previewGridPos; }

    /** @brief Check if preview is active */
    [[nodiscard]] bool HasPreview() const noexcept { return m_previewActive; }

    /** @brief Enable/disable preview */
    void SetPreviewActive(bool active) noexcept { m_previewActive = active; }

    // =========================================================================
    // Rotation
    // =========================================================================

    /** @brief Get current rotation (radians) */
    [[nodiscard]] float GetRotation() const noexcept { return m_rotation; }

    /** @brief Set rotation (radians) */
    void SetRotation(float radians);

    /** @brief Set rotation (degrees) */
    void SetRotationDegrees(float degrees);

    /** @brief Rotate by amount (radians) */
    void Rotate(float radians);

    /** @brief Rotate by 90 degrees clockwise */
    void RotateCW();

    /** @brief Rotate by 90 degrees counter-clockwise */
    void RotateCCW();

    /** @brief Get rotation snap mode */
    [[nodiscard]] RotationSnap GetRotationSnap() const noexcept { return m_rotationSnap; }

    /** @brief Set rotation snap mode */
    void SetRotationSnap(RotationSnap snap) noexcept { m_rotationSnap = snap; }

    // =========================================================================
    // Road Alignment
    // =========================================================================

    /** @brief Check if auto-align to roads is enabled */
    [[nodiscard]] bool IsRoadAlignEnabled() const noexcept { return m_roadAlignEnabled; }

    /** @brief Enable/disable road alignment */
    void SetRoadAlignEnabled(bool enabled) noexcept { m_roadAlignEnabled = enabled; }

    /** @brief Get road alignment distance */
    [[nodiscard]] float GetRoadAlignDistance() const noexcept { return m_roadAlignDistance; }

    /** @brief Set road alignment distance */
    void SetRoadAlignDistance(float distance) noexcept { m_roadAlignDistance = distance; }

    /**
     * @brief Attempt to align building to nearest road
     * @param map Tile map to check
     * @return true if alignment was performed
     */
    bool AlignToRoad(const TileMap& map);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate current placement
     * @param map Tile map to check
     * @param existingBuildings Existing buildings to check collision with
     * @return Validation result
     */
    [[nodiscard]] PlacementValidation ValidatePlacement(
        const TileMap& map,
        const std::vector<Building*>& existingBuildings) const;

    /**
     * @brief Check if placement is currently valid
     */
    [[nodiscard]] bool IsPlacementValid() const noexcept { return m_lastValidation.valid; }

    /**
     * @brief Get last validation result
     */
    [[nodiscard]] const PlacementValidation& GetLastValidation() const noexcept {
        return m_lastValidation;
    }

    /**
     * @brief Get tiles that would be occupied
     */
    [[nodiscard]] std::vector<glm::ivec2> GetOccupiedTiles() const;

    // =========================================================================
    // Placement
    // =========================================================================

    /**
     * @brief Place building at current preview position
     * @param world World to place building in
     * @return Pointer to placed building, or nullptr on failure
     */
    Building* PlaceBuilding(World& world);

    /**
     * @brief Place multiple buildings
     * @param world World to place buildings in
     * @param config Multi-placement configuration
     * @return Vector of placed buildings
     */
    std::vector<Building*> PlaceMultiple(World& world, const MultiPlaceConfig& config);

    /**
     * @brief Create building preview object (for rendering)
     * @return Building object configured as preview
     */
    [[nodiscard]] std::unique_ptr<Building> CreatePreviewBuilding() const;

    // =========================================================================
    // Multi-Placement
    // =========================================================================

    /** @brief Get multi-place configuration */
    [[nodiscard]] const MultiPlaceConfig& GetMultiPlaceConfig() const noexcept {
        return m_multiPlaceConfig;
    }

    /** @brief Get mutable multi-place configuration */
    [[nodiscard]] MultiPlaceConfig& GetMultiPlaceConfig() noexcept { return m_multiPlaceConfig; }

    /** @brief Set multi-place configuration */
    void SetMultiPlaceConfig(const MultiPlaceConfig& config) noexcept {
        m_multiPlaceConfig = config;
    }

    /**
     * @brief Get preview positions for multi-placement
     * @return Vector of preview positions
     */
    [[nodiscard]] std::vector<glm::vec3> GetMultiPlacePreviewPositions() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using PlaceCallback = std::function<void(Building&)>;
    using ValidationCallback = std::function<void(const PlacementValidation&)>;

    /** @brief Set callback for building placed */
    void SetOnBuildingPlaced(PlaceCallback callback) { m_onBuildingPlaced = std::move(callback); }

    /** @brief Set callback for validation changed */
    void SetOnValidationChanged(ValidationCallback callback) {
        m_onValidationChanged = std::move(callback);
    }

private:
    float SnapRotation(float radians) const;
    void UpdateValidation(const TileMap& map, const std::vector<Building*>& buildings);
    glm::vec2 FindNearestRoadDirection(const TileMap& map, int tileX, int tileY) const;

    // Selection
    BuildingType m_selectedBuilding = BuildingType::Shelter;
    int m_selectedVariant = 0;

    // Preview
    glm::vec3 m_previewPosition{0.0f};
    glm::ivec2 m_previewGridPos{0, 0};
    bool m_previewActive = false;

    // Rotation
    float m_rotation = 0.0f;
    RotationSnap m_rotationSnap = RotationSnap::Snap90;

    // Road alignment
    bool m_roadAlignEnabled = true;
    float m_roadAlignDistance = 5.0f;

    // Validation
    PlacementValidation m_lastValidation;

    // Multi-placement
    MultiPlaceConfig m_multiPlaceConfig;

    // Callbacks
    PlaceCallback m_onBuildingPlaced;
    ValidationCallback m_onValidationChanged;
};

} // namespace Vehement
