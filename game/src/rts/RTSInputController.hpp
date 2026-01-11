#pragma once

/**
 * @file RTSInputController.hpp
 * @brief Player input controller for RTS gameplay
 *
 * Handles all player input for Solo Play mode:
 * - Unit selection (click, drag-box selection)
 * - Unit commands (move, attack-move, patrol)
 * - Building placement and selection
 * - Camera controls (WASD pan, mouse edge scrolling, zoom)
 * - Gamepad support for alternative controls
 */

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace Nova {
    class InputManager;
    class Camera;
    class Renderer;
}

namespace Vehement {

// Forward declarations
class Entity;
class Building;
class Player;

namespace RTS {

// ============================================================================
// Selection Types
// ============================================================================

/**
 * @brief Types of selections the player can make
 */
enum class SelectionType : uint8_t {
    None,           ///< Nothing selected
    Units,          ///< One or more units selected
    Building,       ///< A building selected
    Mixed           ///< Mix of units and buildings
};

/**
 * @brief Command types for selected units
 */
enum class CommandType : uint8_t {
    Move,           ///< Move to location
    AttackMove,     ///< Attack-move to location
    Patrol,         ///< Patrol between points
    Stop,           ///< Stop current action
    Hold,           ///< Hold position and defend
    Gather,         ///< Gather resources
    Build,          ///< Construct building
    Repair,         ///< Repair structure
    Attack          ///< Attack target
};

/**
 * @brief Camera control mode
 */
enum class CameraMode : uint8_t {
    Free,           ///< Free camera movement (WASD + mouse)
    FollowUnit,     ///< Follow selected unit
    FollowGroup,    ///< Follow group center
    Locked          ///< Camera position locked
};

// ============================================================================
// Selection Data
// ============================================================================

/**
 * @brief Selection rectangle for drag-box selection
 */
struct SelectionBox {
    bool active = false;
    glm::vec2 startScreenPos{0.0f};
    glm::vec2 endScreenPos{0.0f};

    /**
     * @brief Get normalized rectangle (min/max)
     */
    void GetNormalized(glm::vec2& min, glm::vec2& max) const {
        min.x = std::min(startScreenPos.x, endScreenPos.x);
        min.y = std::min(startScreenPos.y, endScreenPos.y);
        max.x = std::max(startScreenPos.x, endScreenPos.x);
        max.y = std::max(startScreenPos.y, endScreenPos.y);
    }

    /**
     * @brief Get box dimensions
     */
    glm::vec2 GetSize() const {
        return glm::abs(endScreenPos - startScreenPos);
    }

    /**
     * @brief Check if box is large enough to be valid selection
     */
    bool IsValidSize(float minSize = 5.0f) const {
        glm::vec2 size = GetSize();
        return size.x >= minSize && size.y >= minSize;
    }
};

/**
 * @brief Current player selection state
 */
struct SelectionState {
    SelectionType type = SelectionType::None;
    std::vector<Entity*> selectedUnits;
    Building* selectedBuilding = nullptr;

    /**
     * @brief Clear all selections
     */
    void Clear() {
        type = SelectionType::None;
        selectedUnits.clear();
        selectedBuilding = nullptr;
    }

    /**
     * @brief Check if anything is selected
     */
    bool HasSelection() const {
        return !selectedUnits.empty() || selectedBuilding != nullptr;
    }

    /**
     * @brief Get selection count
     */
    size_t GetSelectionCount() const {
        size_t count = selectedUnits.size();
        if (selectedBuilding) count++;
        return count;
    }
};

/**
 * @brief Building placement preview
 */
struct BuildingPlacementPreview {
    bool active = false;
    int buildingTypeIndex = 0;  // Index into race's building archetypes
    glm::vec3 worldPosition{0.0f};
    glm::ivec2 gridPosition{0, 0};
    float rotation = 0.0f;
    bool isValid = false;

    /**
     * @brief Get preview color based on validity
     */
    glm::vec4 GetColor() const {
        if (isValid) {
            return glm::vec4(0.0f, 1.0f, 0.0f, 0.5f); // Green
        } else {
            return glm::vec4(1.0f, 0.0f, 0.0f, 0.5f); // Red
        }
    }
};

// ============================================================================
// RTS Camera Controller
// ============================================================================

/**
 * @brief RTS-style camera controller
 *
 * Provides:
 * - WASD pan controls
 * - Mouse edge scrolling
 * - Scroll wheel zoom
 * - Minimap camera positioning
 * - Smooth camera transitions
 */
class RTSCamera {
public:
    RTSCamera();
    ~RTSCamera() = default;

    /**
     * @brief Initialize camera with settings
     */
    void Initialize(Nova::Camera* camera, const glm::vec3& startPosition);

    /**
     * @brief Update camera based on input
     */
    void Update(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize);

    /**
     * @brief Set camera mode
     */
    void SetMode(CameraMode mode) { m_mode = mode; }
    CameraMode GetMode() const { return m_mode; }

    /**
     * @brief Pan camera by offset
     */
    void Pan(const glm::vec2& offset);

    /**
     * @brief Zoom in/out by delta
     */
    void Zoom(float delta);

    /**
     * @brief Set zoom level (distance from ground)
     */
    void SetZoom(float zoom);
    float GetZoom() const { return m_zoom; }

    /**
     * @brief Move camera to position (smooth transition)
     */
    void MoveToPosition(const glm::vec3& position, float duration = 0.5f);

    /**
     * @brief Set camera bounds (min/max XZ)
     */
    void SetBounds(const glm::vec2& min, const glm::vec2& max);

    /**
     * @brief Enable/disable edge scrolling
     */
    void SetEdgeScrollingEnabled(bool enabled) { m_edgeScrollingEnabled = enabled; }
    bool IsEdgeScrollingEnabled() const { return m_edgeScrollingEnabled; }

    /**
     * @brief Set edge scrolling margin (pixels from screen edge)
     */
    void SetEdgeScrollMargin(float margin) { m_edgeScrollMargin = margin; }

    /**
     * @brief Get current camera position
     */
    glm::vec3 GetPosition() const { return m_position; }

    /**
     * @brief Settings
     */
    float panSpeed = 20.0f;
    float edgeScrollSpeed = 15.0f;
    float zoomSpeed = 5.0f;
    float minZoom = 5.0f;
    float maxZoom = 50.0f;

private:
    void UpdateFreeCameraMovement(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize);
    void UpdateEdgeScrolling(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize);
    void UpdateZoom(Nova::InputManager& input, float deltaTime);
    void ApplyBounds();
    void UpdateCameraTransform();

    Nova::Camera* m_camera = nullptr;
    CameraMode m_mode = CameraMode::Free;

    glm::vec3 m_position{0.0f, 20.0f, 0.0f};
    glm::vec3 m_targetPosition{0.0f};
    float m_zoom = 20.0f;
    float m_targetZoom = 20.0f;
    float m_pitch = -45.0f;  // Look down angle
    float m_yaw = 0.0f;

    // Camera bounds
    bool m_hasBounds = false;
    glm::vec2 m_boundsMin{-100.0f, -100.0f};
    glm::vec2 m_boundsMax{100.0f, 100.0f};

    // Edge scrolling
    bool m_edgeScrollingEnabled = true;
    float m_edgeScrollMargin = 20.0f;

    // Smooth movement
    bool m_isTransitioning = false;
    float m_transitionTimer = 0.0f;
    float m_transitionDuration = 0.5f;
    glm::vec3 m_transitionStart{0.0f};
    glm::vec3 m_transitionEnd{0.0f};
};

// ============================================================================
// RTS Input Controller
// ============================================================================

/**
 * @brief Main input controller for RTS gameplay
 *
 * Handles all player interactions:
 * - Unit selection (single click, drag-box)
 * - Unit commands (move, attack, patrol)
 * - Building placement and construction
 * - Camera controls
 * - Keyboard shortcuts
 * - Gamepad controls
 */
class RTSInputController {
public:
    RTSInputController();
    ~RTSInputController();

    // Non-copyable
    RTSInputController(const RTSInputController&) = delete;
    RTSInputController& operator=(const RTSInputController&) = delete;

    /**
     * @brief Initialize the input controller
     */
    bool Initialize(Nova::Camera* camera, Player* player);

    /**
     * @brief Update input handling
     */
    void Update(Nova::InputManager& input, float deltaTime);

    /**
     * @brief Render selection boxes, placement previews, etc.
     */
    void Render(Nova::Renderer& renderer);

    // =========================================================================
    // Selection Management
    // =========================================================================

    /**
     * @brief Get current selection state
     */
    const SelectionState& GetSelection() const { return m_selection; }

    /**
     * @brief Select single entity at screen position
     */
    void SelectAtPosition(const glm::vec2& screenPos);

    /**
     * @brief Add entity to selection
     */
    void AddToSelection(Entity* entity, bool clearPrevious = false);

    /**
     * @brief Remove entity from selection
     */
    void RemoveFromSelection(Entity* entity);

    /**
     * @brief Clear all selections
     */
    void ClearSelection();

    /**
     * @brief Select all units on screen
     */
    void SelectAll();

    /**
     * @brief Select all units of same type as current selection
     */
    void SelectAllOfType();

    /**
     * @brief Get entities in screen rectangle
     */
    std::vector<Entity*> GetEntitiesInScreenRect(const glm::vec2& min, const glm::vec2& max);

    // =========================================================================
    // Unit Commands
    // =========================================================================

    /**
     * @brief Issue move command to selected units
     */
    void CommandMove(const glm::vec3& worldPosition);

    /**
     * @brief Issue attack-move command
     */
    void CommandAttackMove(const glm::vec3& worldPosition);

    /**
     * @brief Issue stop command
     */
    void CommandStop();

    /**
     * @brief Issue hold position command
     */
    void CommandHold();

    /**
     * @brief Issue patrol command
     */
    void CommandPatrol(const glm::vec3& worldPosition);

    /**
     * @brief Issue attack target command
     */
    void CommandAttack(Entity* target);

    // =========================================================================
    // Building Placement
    // =========================================================================

    /**
     * @brief Enter building placement mode
     */
    void StartBuildingPlacement(int buildingTypeIndex);

    /**
     * @brief Cancel building placement
     */
    void CancelBuildingPlacement();

    /**
     * @brief Confirm building placement at current preview position
     */
    bool ConfirmBuildingPlacement();

    /**
     * @brief Check if in building placement mode
     */
    bool IsPlacingBuilding() const { return m_buildingPreview.active; }

    /**
     * @brief Get building placement preview
     */
    const BuildingPlacementPreview& GetBuildingPreview() const { return m_buildingPreview; }

    // =========================================================================
    // Camera Control
    // =========================================================================

    /**
     * @brief Get RTS camera controller
     */
    RTSCamera& GetCamera() { return m_rtsCamera; }
    const RTSCamera& GetCamera() const { return m_rtsCamera; }

    /**
     * @brief Focus camera on selected units
     */
    void FocusCameraOnSelection();

    /**
     * @brief Save camera bookmark
     */
    void SaveCameraBookmark(int index);

    /**
     * @brief Restore camera bookmark
     */
    void RestoreCameraBookmark(int index);

    // =========================================================================
    // Control Groups
    // =========================================================================

    /**
     * @brief Assign selection to control group (Ctrl+1-9)
     */
    void AssignControlGroup(int groupIndex);

    /**
     * @brief Select control group (1-9)
     */
    void SelectControlGroup(int groupIndex);

    /**
     * @brief Add selection to control group (Shift+1-9)
     */
    void AddToControlGroup(int groupIndex);

    // =========================================================================
    // Input Mode
    // =========================================================================

    /**
     * @brief Enable/disable keyboard/mouse controls
     */
    void SetKeyboardMouseEnabled(bool enabled) { m_keyboardMouseEnabled = enabled; }
    bool IsKeyboardMouseEnabled() const { return m_keyboardMouseEnabled; }

    /**
     * @brief Enable/disable gamepad controls
     */
    void SetGamepadEnabled(bool enabled) { m_gamepadEnabled = enabled; }
    bool IsGamepadEnabled() const { return m_gamepadEnabled; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using SelectionCallback = std::function<void(const SelectionState&)>;
    using CommandCallback = std::function<void(CommandType, const glm::vec3&)>;
    using BuildingPlacedCallback = std::function<void(int buildingTypeIndex, const glm::ivec2& gridPos)>;

    void SetOnSelectionChanged(SelectionCallback callback) { m_onSelectionChanged = std::move(callback); }
    void SetOnCommand(CommandCallback callback) { m_onCommand = std::move(callback); }
    void SetOnBuildingPlaced(BuildingPlacedCallback callback) { m_onBuildingPlaced = std::move(callback); }

private:
    // Input processing
    void ProcessMouseInput(Nova::InputManager& input, float deltaTime);
    void ProcessKeyboardInput(Nova::InputManager& input, float deltaTime);
    void ProcessGamepadInput(Nova::InputManager& input, float deltaTime);

    // Selection helpers
    void StartDragSelection(const glm::vec2& screenPos);
    void UpdateDragSelection(const glm::vec2& screenPos);
    void EndDragSelection();
    Entity* GetEntityAtScreenPosition(const glm::vec2& screenPos);

    // World position helpers
    glm::vec3 ScreenToWorldPosition(const glm::vec2& screenPos);
    bool RaycastGround(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec3& hitPoint);

    // Building placement helpers
    void UpdateBuildingPlacementPreview(Nova::InputManager& input);
    bool ValidateBuildingPlacement(const glm::ivec2& gridPos, int buildingTypeIndex);

    // Selection notification
    void NotifySelectionChanged();

    // Core references
    Nova::Camera* m_camera = nullptr;
    Player* m_player = nullptr;

    // RTS camera
    RTSCamera m_rtsCamera;

    // Selection state
    SelectionState m_selection;
    SelectionBox m_selectionBox;

    // Building placement
    BuildingPlacementPreview m_buildingPreview;

    // Control groups (1-9)
    std::vector<std::vector<Entity*>> m_controlGroups;

    // Camera bookmarks (F1-F8)
    std::vector<glm::vec3> m_cameraBookmarks;

    // Input state
    bool m_keyboardMouseEnabled = true;
    bool m_gamepadEnabled = true;
    bool m_isDragging = false;
    glm::vec2 m_lastMousePosition{0.0f};

    // Modifier keys
    bool m_shiftHeld = false;
    bool m_ctrlHeld = false;
    bool m_altHeld = false;

    // Callbacks
    SelectionCallback m_onSelectionChanged;
    CommandCallback m_onCommand;
    BuildingPlacedCallback m_onBuildingPlaced;

    // Gamepad state
    glm::vec2 m_gamepadCursorPosition{0.0f};
    float m_gamepadCursorSpeed = 500.0f;
};

} // namespace RTS
} // namespace Vehement
