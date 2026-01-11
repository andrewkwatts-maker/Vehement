#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

class World;
class AssetEntry;

/**
 * @brief Placement mode for asset placement
 */
enum class PlacementMode : uint8_t {
    Single,         // Place single asset
    Line,           // Place in a line
    Grid,           // Place in a grid pattern
    Circle,         // Place in a circle
    Random,         // Randomly scatter
    Paint           // Paint mode (continuous placement)
};

/**
 * @brief Placement validation result
 */
enum class PlacementIssue : uint8_t {
    None,                   // Valid placement
    OutOfBounds,           // Outside map bounds
    Collision,             // Collides with existing object
    InvalidTerrain,        // Terrain doesn't support this object
    InvalidSlope,          // Slope too steep
    RequiresWater,         // Object needs water
    RequiresLand,          // Object needs land
    BlocksPath,            // Would block pathfinding
    TooClose,              // Too close to other objects
    Custom                 // Custom validation failed
};

/**
 * @brief Placement preview data
 */
struct PlacementPreview {
    std::string assetId;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    bool isValid = false;
    PlacementIssue issue = PlacementIssue::None;

    // For multi-placement modes
    std::vector<glm::vec3> additionalPositions;
    std::vector<glm::quat> additionalRotations;
    std::vector<bool> additionalValidity;
};

/**
 * @brief Placement constraints
 */
struct PlacementConstraints {
    bool snapToGrid = true;
    float gridSize = 1.0f;
    bool snapToTerrain = true;
    bool alignToTerrain = false;  // Rotate to match terrain normal
    float minDistanceFromOthers = 0.0f;
    bool checkCollision = true;
    bool checkPathfinding = false;
    float maxSlope = 45.0f;
    std::function<bool(const glm::vec3&)> customValidator;
};

/**
 * @brief Placement System - Handle asset placement with preview and validation
 *
 * Features:
 * - Ghost preview rendering
 * - Collision detection
 * - Snap to grid
 * - Terrain alignment
 * - Multi-placement modes (line, grid, circle, random)
 * - Rotation controls
 * - Validation feedback
 */
class PlacementSystem {
public:
    PlacementSystem();
    ~PlacementSystem();

    // Delete copy, allow move
    PlacementSystem(const PlacementSystem&) = delete;
    PlacementSystem& operator=(const PlacementSystem&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize placement system
     * @param world Game world for placement
     * @return true if successful
     */
    bool Initialize(World& world);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update placement preview
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render placement preview
     * @param renderer Renderer
     * @param camera Camera for rendering
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Placement Control
    // =========================================================================

    /**
     * @brief Start placement mode with asset
     * @param assetId Asset to place
     * @param asset Asset entry data
     */
    void StartPlacement(const std::string& assetId, const AssetEntry& asset);

    /**
     * @brief Stop placement mode
     */
    void StopPlacement();

    /**
     * @brief Check if placement is active
     */
    [[nodiscard]] bool IsPlacing() const noexcept { return m_isPlacing; }

    /**
     * @brief Confirm and place the asset
     * @return true if placement succeeded
     */
    bool ConfirmPlacement();

    /**
     * @brief Cancel current placement
     */
    void CancelPlacement();

    // =========================================================================
    // Placement Mode
    // =========================================================================

    /**
     * @brief Set placement mode
     */
    void SetPlacementMode(PlacementMode mode);

    /**
     * @brief Get current placement mode
     */
    [[nodiscard]] PlacementMode GetPlacementMode() const noexcept { return m_placementMode; }

    /**
     * @brief Get mode name
     */
    [[nodiscard]] static const char* GetPlacementModeName(PlacementMode mode) noexcept;

    // =========================================================================
    // Transform Controls
    // =========================================================================

    /**
     * @brief Update preview position (from mouse/cursor)
     * @param position World position
     */
    void UpdatePosition(const glm::vec3& position);

    /**
     * @brief Rotate preview (increment)
     * @param angleDegrees Rotation amount in degrees
     */
    void Rotate(float angleDegrees);

    /**
     * @brief Set absolute rotation
     * @param angleDegrees Rotation in degrees
     */
    void SetRotation(float angleDegrees);

    /**
     * @brief Get current rotation
     */
    [[nodiscard]] float GetRotation() const noexcept { return m_rotation; }

    /**
     * @brief Set scale
     * @param scale Uniform scale factor
     */
    void SetScale(float scale);

    /**
     * @brief Set non-uniform scale
     * @param scale Scale vector
     */
    void SetScale(const glm::vec3& scale);

    /**
     * @brief Get current scale
     */
    [[nodiscard]] const glm::vec3& GetScale() const noexcept { return m_scale; }

    // =========================================================================
    // Multi-Placement
    // =========================================================================

    /**
     * @brief Start line placement
     * @param start Start position
     */
    void StartLinePlacement(const glm::vec3& start);

    /**
     * @brief Update line endpoint
     * @param end End position
     */
    void UpdateLinePlacement(const glm::vec3& end);

    /**
     * @brief Start grid placement
     * @param corner First corner
     */
    void StartGridPlacement(const glm::vec3& corner);

    /**
     * @brief Update grid opposite corner
     * @param corner Opposite corner
     */
    void UpdateGridPlacement(const glm::vec3& corner);

    /**
     * @brief Set grid spacing
     */
    void SetGridSpacing(float spacing);

    /**
     * @brief Start circle placement
     * @param center Circle center
     */
    void StartCirclePlacement(const glm::vec3& center);

    /**
     * @brief Update circle radius
     * @param radius Circle radius
     */
    void UpdateCircleRadius(float radius);

    /**
     * @brief Set number of objects in circle
     */
    void SetCircleCount(int count);

    // =========================================================================
    // Constraints
    // =========================================================================

    /**
     * @brief Set placement constraints
     */
    void SetConstraints(const PlacementConstraints& constraints);

    /**
     * @brief Get current constraints
     */
    [[nodiscard]] const PlacementConstraints& GetConstraints() const noexcept { return m_constraints; }

    /**
     * @brief Toggle snap to grid
     */
    void ToggleSnapToGrid();

    /**
     * @brief Set grid size
     */
    void SetGridSize(float size);

    /**
     * @brief Toggle terrain alignment
     */
    void ToggleTerrainAlignment();

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Get current placement preview
     */
    [[nodiscard]] const PlacementPreview& GetPreview() const noexcept { return m_preview; }

    /**
     * @brief Check if current placement is valid
     */
    [[nodiscard]] bool IsPlacementValid() const noexcept { return m_preview.isValid; }

    /**
     * @brief Get placement issue description
     */
    [[nodiscard]] std::string GetPlacementIssueString() const;

    /**
     * @brief Validate position for placement
     */
    [[nodiscard]] PlacementIssue ValidatePlacement(const glm::vec3& position) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&, const glm::vec3&, const glm::quat&, const glm::vec3&)> OnAssetPlaced;
    std::function<void()> OnPlacementStarted;
    std::function<void()> OnPlacementCancelled;

private:
    // Validation helpers
    void UpdatePreviewValidity();
    bool CheckCollision(const glm::vec3& position, const glm::vec3& scale) const;
    bool CheckTerrainValidity(const glm::vec3& position) const;
    float GetTerrainSlope(const glm::vec3& position) const;

    // Transform helpers
    glm::vec3 SnapToGrid(const glm::vec3& position) const;
    glm::vec3 SnapToTerrain(const glm::vec3& position) const;
    glm::quat AlignToTerrain(const glm::vec3& position) const;

    // Multi-placement helpers
    void CalculateLinePositions();
    void CalculateGridPositions();
    void CalculateCirclePositions();
    void CalculateRandomPositions();

    // Rendering helpers
    void RenderGhostPreview(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderValidationFeedback(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderMultiPlacementGuides(Nova::Renderer& renderer, const Nova::Camera& camera);

    // State
    bool m_initialized = false;
    World* m_world = nullptr;

    // Placement state
    bool m_isPlacing = false;
    std::string m_currentAssetId;
    PlacementMode m_placementMode = PlacementMode::Single;
    PlacementPreview m_preview;
    PlacementConstraints m_constraints;

    // Transform
    glm::vec3 m_position{0.0f};
    float m_rotation = 0.0f;  // Degrees around Y axis
    glm::vec3 m_scale{1.0f};

    // Multi-placement state
    glm::vec3 m_multiStart{0.0f};
    glm::vec3 m_multiEnd{0.0f};
    float m_gridSpacing = 2.0f;
    float m_circleRadius = 5.0f;
    int m_circleCount = 8;
    bool m_isMultiPlacing = false;

    // Visual
    glm::vec4 m_validColor{0.0f, 1.0f, 0.0f, 0.4f};
    glm::vec4 m_invalidColor{1.0f, 0.0f, 0.0f, 0.4f};
};

/**
 * @brief Get placement issue description
 */
[[nodiscard]] const char* GetPlacementIssueString(PlacementIssue issue);

} // namespace Vehement
