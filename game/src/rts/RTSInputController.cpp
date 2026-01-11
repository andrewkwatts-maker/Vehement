#include "RTSInputController.hpp"

#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "input/InputManager.hpp"
#include "scene/Camera.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "../entities/Entity.hpp"
#include "../entities/Player.hpp"
#include "Building.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace Vehement {
namespace RTS {

// ============================================================================
// RTSCamera Implementation
// ============================================================================

RTSCamera::RTSCamera() {
    m_position = glm::vec3(0.0f, m_zoom, 0.0f);
    m_targetPosition = m_position;
}

void RTSCamera::Initialize(Nova::Camera* camera, const glm::vec3& startPosition) {
    m_camera = camera;
    m_position = startPosition;
    m_targetPosition = startPosition;
    m_zoom = startPosition.y;
    m_targetZoom = m_zoom;

    UpdateCameraTransform();
}

void RTSCamera::Update(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize) {
    if (!m_camera) return;

    // Update camera based on mode
    switch (m_mode) {
        case CameraMode::Free:
            UpdateFreeCameraMovement(input, deltaTime, screenSize);
            UpdateEdgeScrolling(input, deltaTime, screenSize);
            UpdateZoom(input, deltaTime);
            break;

        case CameraMode::FollowUnit:
        case CameraMode::FollowGroup:
            // Camera follows target (set externally via MoveToPosition)
            UpdateZoom(input, deltaTime);
            break;

        case CameraMode::Locked:
            // Only allow zoom when locked
            UpdateZoom(input, deltaTime);
            break;
    }

    // Smooth transition to target position
    if (m_isTransitioning) {
        m_transitionTimer += deltaTime;
        float t = std::min(m_transitionTimer / m_transitionDuration, 1.0f);

        // Smooth interpolation
        t = t * t * (3.0f - 2.0f * t); // Smoothstep

        m_position = glm::mix(m_transitionStart, m_transitionEnd, t);

        if (t >= 1.0f) {
            m_isTransitioning = false;
        }
    }

    // Smooth zoom
    if (std::abs(m_zoom - m_targetZoom) > 0.01f) {
        m_zoom = glm::mix(m_zoom, m_targetZoom, deltaTime * 10.0f);
        m_position.y = m_zoom;
    }

    ApplyBounds();
    UpdateCameraTransform();
}

void RTSCamera::UpdateFreeCameraMovement(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize) {
    glm::vec2 movement(0.0f);

    // WASD keys
    if (input.IsKeyDown(Nova::Key::W)) movement.y += 1.0f;
    if (input.IsKeyDown(Nova::Key::S)) movement.y -= 1.0f;
    if (input.IsKeyDown(Nova::Key::A)) movement.x -= 1.0f;
    if (input.IsKeyDown(Nova::Key::D)) movement.x += 1.0f;

    // Arrow keys as alternative
    if (input.IsKeyDown(Nova::Key::Up)) movement.y += 1.0f;
    if (input.IsKeyDown(Nova::Key::Down)) movement.y -= 1.0f;
    if (input.IsKeyDown(Nova::Key::Left)) movement.x -= 1.0f;
    if (input.IsKeyDown(Nova::Key::Right)) movement.x += 1.0f;

    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement);
        Pan(movement * panSpeed * deltaTime);
    }
}

void RTSCamera::UpdateEdgeScrolling(Nova::InputManager& input, float deltaTime, const glm::vec2& screenSize) {
    if (!m_edgeScrollingEnabled) return;
    if (input.IsCursorLocked()) return; // Don't edge scroll when cursor is locked

    glm::vec2 mousePos = input.GetMousePosition();
    glm::vec2 movement(0.0f);

    // Check if mouse is near screen edges
    if (mousePos.x < m_edgeScrollMargin) {
        movement.x = -1.0f;
    } else if (mousePos.x > screenSize.x - m_edgeScrollMargin) {
        movement.x = 1.0f;
    }

    if (mousePos.y < m_edgeScrollMargin) {
        movement.y = 1.0f; // Inverted Y for screen space
    } else if (mousePos.y > screenSize.y - m_edgeScrollMargin) {
        movement.y = -1.0f;
    }

    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement);
        Pan(movement * edgeScrollSpeed * deltaTime);
    }
}

void RTSCamera::UpdateZoom(Nova::InputManager& input, float deltaTime) {
    float scrollDelta = input.GetScrollDelta();
    if (std::abs(scrollDelta) > 0.01f) {
        Zoom(-scrollDelta * zoomSpeed);
    }

    // PageUp/PageDown for zoom
    if (input.IsKeyDown(Nova::Key::PageUp)) {
        Zoom(-zoomSpeed * deltaTime);
    }
    if (input.IsKeyDown(Nova::Key::PageDown)) {
        Zoom(zoomSpeed * deltaTime);
    }
}

void RTSCamera::Pan(const glm::vec2& offset) {
    m_position.x += offset.x;
    m_position.z += offset.y;
    m_targetPosition = m_position;
    m_isTransitioning = false; // Cancel any transition
}

void RTSCamera::Zoom(float delta) {
    m_targetZoom = glm::clamp(m_targetZoom + delta, minZoom, maxZoom);
}

void RTSCamera::SetZoom(float zoom) {
    m_targetZoom = glm::clamp(zoom, minZoom, maxZoom);
    m_zoom = m_targetZoom;
    m_position.y = m_zoom;
}

void RTSCamera::MoveToPosition(const glm::vec3& position, float duration) {
    if (duration <= 0.0f) {
        m_position = position;
        m_targetPosition = position;
        m_isTransitioning = false;
    } else {
        m_transitionStart = m_position;
        m_transitionEnd = position;
        m_transitionDuration = duration;
        m_transitionTimer = 0.0f;
        m_isTransitioning = true;
    }
}

void RTSCamera::SetBounds(const glm::vec2& min, const glm::vec2& max) {
    m_hasBounds = true;
    m_boundsMin = min;
    m_boundsMax = max;
}

void RTSCamera::ApplyBounds() {
    if (!m_hasBounds) return;

    m_position.x = glm::clamp(m_position.x, m_boundsMin.x, m_boundsMax.x);
    m_position.z = glm::clamp(m_position.z, m_boundsMin.y, m_boundsMax.y);
}

void RTSCamera::UpdateCameraTransform() {
    if (!m_camera) return;

    // Calculate camera look-at target (slightly ahead of camera position)
    glm::vec3 target = m_position;
    target.y = 0.0f; // Look at ground level

    // Position camera at angle above the ground
    glm::vec3 camPos = m_position;

    m_camera->LookAt(camPos, target, glm::vec3(0, 1, 0));
}

// ============================================================================
// RTSInputController Implementation
// ============================================================================

RTSInputController::RTSInputController() {
    // Initialize control groups (0-9)
    m_controlGroups.resize(10);

    // Initialize camera bookmarks (F1-F8)
    m_cameraBookmarks.resize(8, glm::vec3(0.0f));
}

RTSInputController::~RTSInputController() = default;

bool RTSInputController::Initialize(Nova::Camera* camera, Player* player) {
    if (!camera) {
        spdlog::error("RTSInputController: Cannot initialize with null camera");
        return false;
    }

    m_camera = camera;
    m_player = player;

    // Initialize RTS camera
    glm::vec3 startPos(0.0f, 20.0f, 0.0f);
    if (player) {
        startPos = player->GetPosition();
        startPos.y = 20.0f;
    }

    m_rtsCamera.Initialize(camera, startPos);

    // Set reasonable camera bounds (will be updated based on map size)
    m_rtsCamera.SetBounds(glm::vec2(-100.0f, -100.0f), glm::vec2(100.0f, 100.0f));

    spdlog::info("RTSInputController initialized successfully");
    return true;
}

void RTSInputController::Update(Nova::InputManager& input, float deltaTime) {
    // Get screen size for various calculations
    auto& window = Nova::Engine::Instance().GetWindow();
    glm::vec2 screenSize(window.GetWidth(), window.GetHeight());

    // Update modifier key states
    m_shiftHeld = input.IsShiftDown();
    m_ctrlHeld = input.IsControlDown();
    m_altHeld = input.IsAltDown();

    // Update camera
    m_rtsCamera.Update(input, deltaTime, screenSize);

    // Process input based on enabled modes
    if (m_keyboardMouseEnabled) {
        ProcessMouseInput(input, deltaTime);
        ProcessKeyboardInput(input, deltaTime);
    }

    if (m_gamepadEnabled) {
        ProcessGamepadInput(input, deltaTime);
    }

    // Update building placement preview
    if (m_buildingPreview.active) {
        UpdateBuildingPlacementPreview(input);
    }
}

void RTSInputController::ProcessMouseInput(Nova::InputManager& input, float deltaTime) {
    glm::vec2 mousePos = input.GetMousePosition();

    // Left mouse button - Selection
    if (input.IsMouseButtonPressed(Nova::MouseButton::Left)) {
        if (m_buildingPreview.active) {
            // Confirm building placement
            ConfirmBuildingPlacement();
        } else {
            // Start drag selection
            StartDragSelection(mousePos);
        }
    }

    // Update drag selection
    if (input.IsMouseButtonDown(Nova::MouseButton::Left) && m_isDragging) {
        UpdateDragSelection(mousePos);
    }

    // End drag selection
    if (input.IsMouseButtonReleased(Nova::MouseButton::Left)) {
        if (m_isDragging) {
            EndDragSelection();
        }
    }

    // Right mouse button - Commands
    if (input.IsMouseButtonPressed(Nova::MouseButton::Right)) {
        if (m_buildingPreview.active) {
            // Cancel building placement
            CancelBuildingPlacement();
        } else if (m_selection.HasSelection()) {
            // Issue command to selected units
            glm::vec3 worldPos = ScreenToWorldPosition(mousePos);

            // Check if shift is held for attack-move
            if (m_shiftHeld) {
                CommandAttackMove(worldPos);
            } else {
                CommandMove(worldPos);
            }
        }
    }

    m_lastMousePosition = mousePos;
}

void RTSInputController::ProcessKeyboardInput(Nova::InputManager& input, float deltaTime) {
    // Escape - Clear selection or cancel placement
    if (input.IsKeyPressed(Nova::Key::Escape)) {
        if (m_buildingPreview.active) {
            CancelBuildingPlacement();
        } else {
            ClearSelection();
        }
    }

    // Ctrl+A - Select all units
    if (m_ctrlHeld && input.IsKeyPressed(Nova::Key::A)) {
        SelectAll();
    }

    // Delete - Delete selected buildings (if in editor mode)
    if (input.IsKeyPressed(Nova::Key::Delete)) {
        // TODO: Implement building deletion
    }

    // Control groups (1-9)
    for (int i = 0; i < 9; i++) {
        Nova::Key key = static_cast<Nova::Key>(static_cast<int>(Nova::Key::Num1) + i);
        if (input.IsKeyPressed(key)) {
            if (m_ctrlHeld) {
                // Assign to control group
                AssignControlGroup(i);
            } else if (m_shiftHeld) {
                // Add to control group
                AddToControlGroup(i);
            } else {
                // Select control group
                SelectControlGroup(i);
            }
        }
    }

    // Camera bookmarks (F1-F8)
    for (int i = 0; i < 8; i++) {
        Nova::Key key = static_cast<Nova::Key>(static_cast<int>(Nova::Key::F1) + i);
        if (input.IsKeyPressed(key)) {
            if (m_ctrlHeld) {
                // Save bookmark
                SaveCameraBookmark(i);
            } else {
                // Restore bookmark
                RestoreCameraBookmark(i);
            }
        }
    }

    // Unit commands
    if (m_selection.HasSelection() && !m_selection.selectedUnits.empty()) {
        // S - Stop
        if (input.IsKeyPressed(Nova::Key::S)) {
            CommandStop();
        }

        // H - Hold position
        if (input.IsKeyPressed(Nova::Key::H)) {
            CommandHold();
        }

        // P - Patrol (next right-click sets patrol point)
        if (input.IsKeyPressed(Nova::Key::P)) {
            // TODO: Enter patrol mode
        }
    }

    // Building hotkeys (B for building menu)
    if (input.IsKeyPressed(Nova::Key::B)) {
        // TODO: Open building menu
    }

    // Focus on selection (Spacebar)
    if (input.IsKeyPressed(Nova::Key::Space)) {
        FocusCameraOnSelection();
    }
}

void RTSInputController::ProcessGamepadInput(Nova::InputManager& input, float deltaTime) {
    // TODO: Implement gamepad controls
    // Left stick - Move cursor
    // Right stick - Pan camera
    // Triggers - Zoom
    // A button - Select
    // B button - Cancel/Deselect
    // X button - Command
    // Y button - Special action
}

// ============================================================================
// Selection Methods
// ============================================================================

void RTSInputController::StartDragSelection(const glm::vec2& screenPos) {
    m_selectionBox.active = true;
    m_selectionBox.startScreenPos = screenPos;
    m_selectionBox.endScreenPos = screenPos;
    m_isDragging = true;
}

void RTSInputController::UpdateDragSelection(const glm::vec2& screenPos) {
    m_selectionBox.endScreenPos = screenPos;
}

void RTSInputController::EndDragSelection() {
    m_isDragging = false;

    if (m_selectionBox.IsValidSize()) {
        // Drag box selection
        glm::vec2 min, max;
        m_selectionBox.GetNormalized(min, max);

        std::vector<Entity*> entities = GetEntitiesInScreenRect(min, max);

        if (!m_shiftHeld) {
            ClearSelection();
        }

        for (Entity* entity : entities) {
            AddToSelection(entity, false);
        }

        NotifySelectionChanged();
    } else {
        // Single click selection
        SelectAtPosition(m_selectionBox.startScreenPos);
    }

    m_selectionBox.active = false;
}

void RTSInputController::SelectAtPosition(const glm::vec2& screenPos) {
    Entity* entity = GetEntityAtScreenPosition(screenPos);

    if (entity) {
        if (m_shiftHeld) {
            // Add to selection
            AddToSelection(entity, false);
        } else {
            // Replace selection
            AddToSelection(entity, true);
        }
    } else if (!m_shiftHeld) {
        // Clicked empty space, clear selection
        ClearSelection();
    }
}

void RTSInputController::AddToSelection(Entity* entity, bool clearPrevious) {
    if (!entity) return;

    if (clearPrevious) {
        ClearSelection();
    }

    // Check if already selected
    auto it = std::find(m_selection.selectedUnits.begin(), m_selection.selectedUnits.end(), entity);
    if (it != m_selection.selectedUnits.end()) {
        return; // Already selected
    }

    m_selection.selectedUnits.push_back(entity);
    m_selection.type = SelectionType::Units;

    NotifySelectionChanged();
}

void RTSInputController::RemoveFromSelection(Entity* entity) {
    auto it = std::find(m_selection.selectedUnits.begin(), m_selection.selectedUnits.end(), entity);
    if (it != m_selection.selectedUnits.end()) {
        m_selection.selectedUnits.erase(it);

        if (m_selection.selectedUnits.empty() && !m_selection.selectedBuilding) {
            m_selection.type = SelectionType::None;
        }

        NotifySelectionChanged();
    }
}

void RTSInputController::ClearSelection() {
    m_selection.Clear();
    NotifySelectionChanged();
}

void RTSInputController::SelectAll() {
    // TODO: Get all player units from entity manager
    spdlog::info("Select All - not yet implemented");
}

void RTSInputController::SelectAllOfType() {
    // TODO: Select all units of same type as current selection
    spdlog::info("Select All Of Type - not yet implemented");
}

Entity* RTSInputController::GetEntityAtScreenPosition(const glm::vec2& screenPos) {
    // TODO: Implement raycasting to find entity at screen position
    // For now, return nullptr
    return nullptr;
}

std::vector<Entity*> RTSInputController::GetEntitiesInScreenRect(const glm::vec2& min, const glm::vec2& max) {
    // TODO: Implement entity selection within screen rectangle
    // For now, return empty vector
    return {};
}

void RTSInputController::NotifySelectionChanged() {
    if (m_onSelectionChanged) {
        m_onSelectionChanged(m_selection);
    }
}

// ============================================================================
// Unit Command Methods
// ============================================================================

void RTSInputController::CommandMove(const glm::vec3& worldPosition) {
    spdlog::info("Move command to ({}, {}, {}) for {} units",
                 worldPosition.x, worldPosition.y, worldPosition.z,
                 m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Move, worldPosition);
    }

    // TODO: Send move command to selected units
}

void RTSInputController::CommandAttackMove(const glm::vec3& worldPosition) {
    spdlog::info("Attack-move command to ({}, {}, {}) for {} units",
                 worldPosition.x, worldPosition.y, worldPosition.z,
                 m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::AttackMove, worldPosition);
    }

    // TODO: Send attack-move command to selected units
}

void RTSInputController::CommandStop() {
    spdlog::info("Stop command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Stop, glm::vec3(0.0f));
    }

    // TODO: Send stop command to selected units
}

void RTSInputController::CommandHold() {
    spdlog::info("Hold position command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Hold, glm::vec3(0.0f));
    }

    // TODO: Send hold command to selected units
}

void RTSInputController::CommandPatrol(const glm::vec3& worldPosition) {
    spdlog::info("Patrol command to ({}, {}, {}) for {} units",
                 worldPosition.x, worldPosition.y, worldPosition.z,
                 m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Patrol, worldPosition);
    }

    // TODO: Send patrol command to selected units
}

void RTSInputController::CommandAttack(Entity* target) {
    if (!target) return;

    spdlog::info("Attack target command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Attack, target->GetPosition());
    }

    // TODO: Send attack target command to selected units
}

// ============================================================================
// Building Placement Methods
// ============================================================================

void RTSInputController::StartBuildingPlacement(int buildingTypeIndex) {
    m_buildingPreview.active = true;
    m_buildingPreview.buildingTypeIndex = buildingTypeIndex;
    m_buildingPreview.rotation = 0.0f;
    m_buildingPreview.isValid = false;

    spdlog::info("Started building placement mode for building type {}", buildingTypeIndex);
}

void RTSInputController::CancelBuildingPlacement() {
    m_buildingPreview.active = false;
    spdlog::info("Cancelled building placement");
}

bool RTSInputController::ConfirmBuildingPlacement() {
    if (!m_buildingPreview.active || !m_buildingPreview.isValid) {
        return false;
    }

    spdlog::info("Placing building type {} at grid position ({}, {})",
                 m_buildingPreview.buildingTypeIndex,
                 m_buildingPreview.gridPosition.x,
                 m_buildingPreview.gridPosition.y);

    if (m_onBuildingPlaced) {
        m_onBuildingPlaced(m_buildingPreview.buildingTypeIndex, m_buildingPreview.gridPosition);
    }

    // Keep placement mode active for quick placement of multiple buildings
    // User can press Escape or right-click to exit

    return true;
}

void RTSInputController::UpdateBuildingPlacementPreview(Nova::InputManager& input) {
    // Update preview position based on mouse
    glm::vec2 mousePos = input.GetMousePosition();
    glm::vec3 worldPos = ScreenToWorldPosition(mousePos);

    m_buildingPreview.worldPosition = worldPos;

    // Snap to grid
    m_buildingPreview.gridPosition.x = static_cast<int>(std::floor(worldPos.x));
    m_buildingPreview.gridPosition.y = static_cast<int>(std::floor(worldPos.z));

    // Validate placement
    m_buildingPreview.isValid = ValidateBuildingPlacement(
        m_buildingPreview.gridPosition,
        m_buildingPreview.buildingTypeIndex
    );

    // Rotate with R key
    if (input.IsKeyPressed(Nova::Key::R)) {
        m_buildingPreview.rotation += 90.0f;
        if (m_buildingPreview.rotation >= 360.0f) {
            m_buildingPreview.rotation = 0.0f;
        }
    }
}

bool RTSInputController::ValidateBuildingPlacement(const glm::ivec2& gridPos, int buildingTypeIndex) {
    // TODO: Implement actual validation
    // Check if position is valid, not occupied, player has resources, etc.
    return true;
}

// ============================================================================
// Camera Control Methods
// ============================================================================

void RTSInputController::FocusCameraOnSelection() {
    if (m_selection.selectedUnits.empty()) return;

    // Calculate center of selected units
    glm::vec3 center(0.0f);
    for (Entity* entity : m_selection.selectedUnits) {
        center += entity->GetPosition();
    }
    center /= static_cast<float>(m_selection.selectedUnits.size());

    // Keep current zoom level
    center.y = m_rtsCamera.GetZoom();

    m_rtsCamera.MoveToPosition(center, 0.5f);

    spdlog::info("Focused camera on selection at ({}, {}, {})", center.x, center.y, center.z);
}

void RTSInputController::SaveCameraBookmark(int index) {
    if (index < 0 || index >= static_cast<int>(m_cameraBookmarks.size())) return;

    m_cameraBookmarks[index] = m_rtsCamera.GetPosition();
    spdlog::info("Saved camera bookmark {} at position ({}, {}, {})",
                 index + 1,
                 m_cameraBookmarks[index].x,
                 m_cameraBookmarks[index].y,
                 m_cameraBookmarks[index].z);
}

void RTSInputController::RestoreCameraBookmark(int index) {
    if (index < 0 || index >= static_cast<int>(m_cameraBookmarks.size())) return;

    glm::vec3 pos = m_cameraBookmarks[index];
    if (glm::length(pos) < 0.01f) {
        spdlog::warn("Camera bookmark {} is not set", index + 1);
        return;
    }

    m_rtsCamera.MoveToPosition(pos, 0.3f);
    spdlog::info("Restored camera bookmark {}", index + 1);
}

// ============================================================================
// Control Group Methods
// ============================================================================

void RTSInputController::AssignControlGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_controlGroups.size())) return;

    m_controlGroups[groupIndex] = m_selection.selectedUnits;
    spdlog::info("Assigned {} units to control group {}",
                 m_selection.selectedUnits.size(), groupIndex);
}

void RTSInputController::SelectControlGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_controlGroups.size())) return;

    const auto& group = m_controlGroups[groupIndex];
    if (group.empty()) {
        spdlog::info("Control group {} is empty", groupIndex);
        return;
    }

    ClearSelection();
    for (Entity* entity : group) {
        AddToSelection(entity, false);
    }

    spdlog::info("Selected control group {} with {} units", groupIndex, group.size());
}

void RTSInputController::AddToControlGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_controlGroups.size())) return;

    auto& group = m_controlGroups[groupIndex];
    for (Entity* entity : m_selection.selectedUnits) {
        // Add if not already in group
        if (std::find(group.begin(), group.end(), entity) == group.end()) {
            group.push_back(entity);
        }
    }

    spdlog::info("Added {} units to control group {} (now {} units total)",
                 m_selection.selectedUnits.size(), groupIndex, group.size());
}

// ============================================================================
// Helper Methods
// ============================================================================

glm::vec3 RTSInputController::ScreenToWorldPosition(const glm::vec2& screenPos) {
    if (!m_camera) return glm::vec3(0.0f);

    auto& window = Nova::Engine::Instance().GetWindow();
    glm::vec2 screenSize(window.GetWidth(), window.GetHeight());

    // Get ray from screen position
    glm::vec3 rayDir = m_camera->ScreenToWorldRay(screenPos, screenSize);
    glm::vec3 rayOrigin = m_camera->GetPosition();

    // Raycast against ground plane (y=0)
    glm::vec3 hitPoint;
    if (RaycastGround(rayOrigin, rayDir, hitPoint)) {
        return hitPoint;
    }

    return glm::vec3(0.0f);
}

bool RTSInputController::RaycastGround(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec3& hitPoint) {
    // Ground plane at y = 0
    float planeY = 0.0f;

    // Check if ray is pointing down
    if (rayDirection.y >= 0.0f) {
        return false;
    }

    // Calculate intersection with ground plane
    float t = (planeY - rayOrigin.y) / rayDirection.y;
    if (t < 0.0f) {
        return false;
    }

    hitPoint = rayOrigin + rayDirection * t;
    return true;
}

void RTSInputController::Render(Nova::Renderer& renderer) {
    auto& debugDraw = renderer.GetDebugDraw();

    // Draw selection box
    if (m_selectionBox.active && m_selectionBox.IsValidSize(1.0f)) {
        glm::vec2 min, max;
        m_selectionBox.GetNormalized(min, max);

        // TODO: Draw 2D selection rectangle on screen
        // For now, just log it
    }

    // Draw building placement preview
    if (m_buildingPreview.active) {
        glm::vec4 color = m_buildingPreview.GetColor();
        glm::vec3 pos = m_buildingPreview.worldPosition;

        // Draw a box at the placement position
        // TODO: Draw actual building preview mesh
        debugDraw.AddBox(pos, glm::vec3(2.0f, 1.0f, 2.0f), color);

        // Draw grid position indicator
        glm::vec3 gridPos(
            m_buildingPreview.gridPosition.x + 0.5f,
            0.1f,
            m_buildingPreview.gridPosition.y + 0.5f
        );
        debugDraw.AddBox(gridPos, glm::vec3(1.0f, 0.1f, 1.0f), color);
    }

    // Draw selection circles for selected units
    for (Entity* entity : m_selection.selectedUnits) {
        glm::vec3 pos = entity->GetPosition();
        pos.y = 0.1f; // Slightly above ground

        // Draw selection circle
        debugDraw.AddCircle(pos, 1.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 16);
    }
}

} // namespace RTS
} // namespace Vehement
