#include "RTSInputController.hpp"

#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "input/InputManager.hpp"
#include "scene/Camera.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "../entities/Entity.hpp"
#include "../entities/EntityManager.hpp"
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
        } else if (m_patrolModeActive && m_selection.HasSelection()) {
            // Set patrol waypoint
            glm::vec3 worldPos = ScreenToWorldPosition(mousePos);
            CommandPatrol(worldPos);
            m_patrolModeActive = false; // Exit patrol mode after setting waypoint
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
        // Delete selected building if one is selected
        if (m_selection.selectedBuilding) {
            spdlog::info("Deleting building: {}",
                         GetBuildingTypeName(m_selection.selectedBuilding->GetBuildingType()));

            // Mark building for removal
            m_selection.selectedBuilding->MarkForRemoval();

            // Clear selection
            m_selection.selectedBuilding = nullptr;
            if (m_selection.selectedUnits.empty()) {
                m_selection.type = SelectionType::None;
            }
            NotifySelectionChanged();
        }
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
            // Toggle patrol mode - next right-click will set patrol waypoint
            m_patrolModeActive = !m_patrolModeActive;
            if (m_patrolModeActive) {
                spdlog::info("Patrol mode activated - right-click to set patrol waypoint");
            } else {
                spdlog::info("Patrol mode cancelled");
            }
        }
    }

    // Building hotkeys (B for building menu)
    if (input.IsKeyPressed(Nova::Key::B)) {
        // Toggle building construction menu
        m_buildMenuOpen = !m_buildMenuOpen;
        if (m_buildMenuOpen) {
            spdlog::info("Building menu opened - select a building type to place");
            // Cancel any active building placement or patrol mode
            if (m_buildingPreview.active) {
                CancelBuildingPlacement();
            }
            m_patrolModeActive = false;
        } else {
            spdlog::info("Building menu closed");
        }
    }

    // Focus on selection (Spacebar)
    if (input.IsKeyPressed(Nova::Key::Space)) {
        FocusCameraOnSelection();
    }
}

void RTSInputController::ProcessGamepadInput(Nova::InputManager& input, float deltaTime) {
    // Basic gamepad control implementation
    // Note: Full gamepad state would be accessed via InputRebindingManager in production

    auto& window = Nova::Engine::Instance().GetWindow();
    glm::vec2 screenSize(window.GetWidth(), window.GetHeight());

    // Left stick - Move virtual cursor
    // Using simulated values since InputManager doesn't expose gamepad directly
    // In production, this would use input.GetGamepadAxis() or InputRebindingManager
    glm::vec2 leftStick(0.0f);  // Would be: input.GetGamepadAxis(GamepadAxis::LeftX/LeftY)

    // Apply deadzone
    const float deadzone = 0.15f;
    if (glm::length(leftStick) > deadzone) {
        // Move cursor
        m_gamepadCursorPosition += leftStick * m_gamepadCursorSpeed * deltaTime;

        // Clamp to screen bounds
        m_gamepadCursorPosition.x = glm::clamp(m_gamepadCursorPosition.x, 0.0f, screenSize.x);
        m_gamepadCursorPosition.y = glm::clamp(m_gamepadCursorPosition.y, 0.0f, screenSize.y);
    }

    // Right stick - Pan camera
    glm::vec2 rightStick(0.0f);  // Would be: input.GetGamepadAxis(GamepadAxis::RightX/RightY)
    if (glm::length(rightStick) > deadzone) {
        m_rtsCamera.Pan(rightStick * m_rtsCamera.panSpeed * deltaTime);
    }

    // Triggers - Zoom (would check trigger axes)
    // float leftTrigger = input.GetGamepadAxis(GamepadAxis::LeftTrigger);
    // float rightTrigger = input.GetGamepadAxis(GamepadAxis::RightTrigger);
    // m_rtsCamera.Zoom((rightTrigger - leftTrigger) * m_rtsCamera.zoomSpeed * deltaTime);

    // Gamepad button handling would be:
    // A button - Select at cursor position
    // if (input.IsGamepadButtonPressed(GamepadButton::A)) {
    //     SelectAtPosition(m_gamepadCursorPosition);
    // }

    // B button - Cancel/Deselect
    // if (input.IsGamepadButtonPressed(GamepadButton::B)) {
    //     if (m_buildingPreview.active) {
    //         CancelBuildingPlacement();
    //     } else {
    //         ClearSelection();
    //     }
    // }

    // X button - Issue command at cursor
    // if (input.IsGamepadButtonPressed(GamepadButton::X)) {
    //     if (m_selection.HasSelection()) {
    //         glm::vec3 worldPos = ScreenToWorldPosition(m_gamepadCursorPosition);
    //         CommandMove(worldPos);
    //     }
    // }

    // Y button - Special action (attack-move)
    // if (input.IsGamepadButtonPressed(GamepadButton::Y)) {
    //     if (m_selection.HasSelection()) {
    //         glm::vec3 worldPos = ScreenToWorldPosition(m_gamepadCursorPosition);
    //         CommandAttackMove(worldPos);
    //     }
    // }
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
    if (!m_entityManager) {
        spdlog::warn("SelectAll: No entity manager set");
        return;
    }

    ClearSelection();

    // Get all entities that belong to the player (non-hostile, selectable units)
    // In an RTS, we typically select all units the player controls
    m_entityManager->ForEachEntity([this](Entity& entity) {
        // Select active, alive entities (excluding the player themselves and buildings)
        if (entity.IsActive() && entity.IsAlive() &&
            entity.GetType() != EntityType::Player &&
            entity.GetType() != EntityType::Projectile &&
            entity.GetType() != EntityType::Effect &&
            entity.GetType() != EntityType::Pickup) {

            m_selection.selectedUnits.push_back(&entity);
        }
    });

    if (!m_selection.selectedUnits.empty()) {
        m_selection.type = SelectionType::Units;
        spdlog::info("Selected all {} units", m_selection.selectedUnits.size());
    }

    NotifySelectionChanged();
}

void RTSInputController::SelectAllOfType() {
    if (m_selection.selectedUnits.empty()) {
        spdlog::info("SelectAllOfType: No units currently selected");
        return;
    }

    if (!m_entityManager) {
        spdlog::warn("SelectAllOfType: No entity manager set");
        return;
    }

    // Get the type of the first selected unit
    EntityType targetType = m_selection.selectedUnits[0]->GetType();

    // Clear and re-select all units of the same type
    m_selection.selectedUnits.clear();

    m_entityManager->ForEachEntity(targetType, [this](Entity& entity) {
        if (entity.IsActive() && entity.IsAlive()) {
            m_selection.selectedUnits.push_back(&entity);
        }
    });

    if (!m_selection.selectedUnits.empty()) {
        m_selection.type = SelectionType::Units;
        spdlog::info("Selected all {} units of type {}",
                     m_selection.selectedUnits.size(),
                     EntityTypeToString(targetType));
    }

    NotifySelectionChanged();
}

Entity* RTSInputController::GetEntityAtScreenPosition(const glm::vec2& screenPos) {
    if (!m_entityManager || !m_camera) {
        return nullptr;
    }

    // Convert screen position to world position on ground plane
    glm::vec3 worldPos = ScreenToWorldPosition(screenPos);

    // Find the closest entity within a selection radius
    const float selectionRadius = 1.5f;  // World units
    Entity* closestEntity = nullptr;
    float closestDistSq = selectionRadius * selectionRadius;

    // Find entities near the click position
    auto nearbyEntities = m_entityManager->FindEntitiesInRadius(worldPos, selectionRadius);

    for (Entity* entity : nearbyEntities) {
        if (!entity->IsActive() || !entity->IsAlive()) {
            continue;
        }

        // Skip non-selectable entity types
        if (entity->GetType() == EntityType::Projectile ||
            entity->GetType() == EntityType::Effect) {
            continue;
        }

        // Calculate distance squared to entity
        glm::vec3 entityPos = entity->GetPosition();
        float dx = entityPos.x - worldPos.x;
        float dz = entityPos.z - worldPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestEntity = entity;
        }
    }

    return closestEntity;
}

std::vector<Entity*> RTSInputController::GetEntitiesInScreenRect(const glm::vec2& min, const glm::vec2& max) {
    std::vector<Entity*> result;

    if (!m_entityManager || !m_camera) {
        return result;
    }

    auto& window = Nova::Engine::Instance().GetWindow();
    glm::vec2 screenSize(window.GetWidth(), window.GetHeight());

    // Convert screen rectangle corners to world positions
    glm::vec3 worldMin = ScreenToWorldPosition(min);
    glm::vec3 worldMax = ScreenToWorldPosition(max);

    // Calculate world-space bounding box (account for axis-aligned swap)
    float worldMinX = std::min(worldMin.x, worldMax.x);
    float worldMaxX = std::max(worldMin.x, worldMax.x);
    float worldMinZ = std::min(worldMin.z, worldMax.z);
    float worldMaxZ = std::max(worldMin.z, worldMax.z);

    // Calculate center and radius for spatial query
    glm::vec3 center((worldMinX + worldMaxX) * 0.5f, 0.0f, (worldMinZ + worldMaxZ) * 0.5f);
    float radius = glm::length(glm::vec2(worldMaxX - worldMinX, worldMaxZ - worldMinZ)) * 0.5f + 2.0f;

    // Find entities in the expanded radius then filter by actual bounds
    auto nearbyEntities = m_entityManager->FindEntitiesInRadius(center, radius);

    for (Entity* entity : nearbyEntities) {
        if (!entity->IsActive() || !entity->IsAlive()) {
            continue;
        }

        // Skip non-selectable entity types
        if (entity->GetType() == EntityType::Projectile ||
            entity->GetType() == EntityType::Effect) {
            continue;
        }

        // Check if entity position is within world-space bounds
        glm::vec3 pos = entity->GetPosition();
        if (pos.x >= worldMinX && pos.x <= worldMaxX &&
            pos.z >= worldMinZ && pos.z <= worldMaxZ) {
            result.push_back(entity);
        }
    }

    return result;
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

    // Send move command to selected units
    // Each unit calculates its own path to the target
    // Using basic steering behavior: set velocity toward target
    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive() && unit->IsAlive()) {
            // Calculate direction to target
            glm::vec3 unitPos = unit->GetPosition();
            glm::vec3 direction = worldPosition - unitPos;
            direction.y = 0.0f;  // Keep on ground plane

            if (glm::length(direction) > 0.1f) {
                direction = glm::normalize(direction);
                // Set velocity toward target (unit AI should handle pathfinding)
                unit->SetVelocity(direction * unit->GetMoveSpeed());
                unit->LookAt(worldPosition);
            }
        }
    }
}

void RTSInputController::CommandAttackMove(const glm::vec3& worldPosition) {
    spdlog::info("Attack-move command to ({}, {}, {}) for {} units",
                 worldPosition.x, worldPosition.y, worldPosition.z,
                 m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::AttackMove, worldPosition);
    }

    // Send attack-move command to selected units
    // Units move toward target but engage enemies along the way
    // The unit AI system handles attack logic; here we set the destination
    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive() && unit->IsAlive()) {
            glm::vec3 unitPos = unit->GetPosition();
            glm::vec3 direction = worldPosition - unitPos;
            direction.y = 0.0f;

            if (glm::length(direction) > 0.1f) {
                direction = glm::normalize(direction);
                // Set velocity (unit AI should check for enemies while moving)
                unit->SetVelocity(direction * unit->GetMoveSpeed());
                unit->LookAt(worldPosition);
            }
        }
    }
}

void RTSInputController::CommandStop() {
    spdlog::info("Stop command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Stop, glm::vec3(0.0f));
    }

    // Send stop command to selected units - halt all movement
    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive()) {
            unit->SetVelocity(glm::vec3(0.0f));
        }
    }
}

void RTSInputController::CommandHold() {
    spdlog::info("Hold position command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Hold, glm::vec3(0.0f));
    }

    // Send hold position command to selected units
    // Units stop moving and defend their current position
    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive()) {
            // Stop movement - unit AI should engage enemies in range but not pursue
            unit->SetVelocity(glm::vec3(0.0f));
            // Note: Full hold behavior requires unit AI state machine to track "hold" mode
        }
    }
}

void RTSInputController::CommandPatrol(const glm::vec3& worldPosition) {
    spdlog::info("Patrol command to ({}, {}, {}) for {} units",
                 worldPosition.x, worldPosition.y, worldPosition.z,
                 m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Patrol, worldPosition);
    }

    // Send patrol command to selected units
    // Units move between their current position and the patrol point
    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive() && unit->IsAlive()) {
            // Start moving toward patrol point
            // Full patrol behavior requires unit AI to track patrol waypoints
            glm::vec3 unitPos = unit->GetPosition();
            glm::vec3 direction = worldPosition - unitPos;
            direction.y = 0.0f;

            if (glm::length(direction) > 0.1f) {
                direction = glm::normalize(direction);
                unit->SetVelocity(direction * unit->GetMoveSpeed());
                unit->LookAt(worldPosition);
            }
        }
    }
}

void RTSInputController::CommandAttack(Entity* target) {
    if (!target) return;

    spdlog::info("Attack target command for {} units", m_selection.selectedUnits.size());

    if (m_onCommand) {
        m_onCommand(CommandType::Attack, target->GetPosition());
    }

    // Send attack command to selected units - target a specific entity
    glm::vec3 targetPos = target->GetPosition();

    for (Entity* unit : m_selection.selectedUnits) {
        if (unit && unit->IsActive() && unit->IsAlive()) {
            // Move toward target and attack when in range
            glm::vec3 unitPos = unit->GetPosition();
            glm::vec3 direction = targetPos - unitPos;
            direction.y = 0.0f;

            if (glm::length(direction) > 0.1f) {
                direction = glm::normalize(direction);
                unit->SetVelocity(direction * unit->GetMoveSpeed());
                unit->LookAt(targetPos);
            }
            // Note: Full attack behavior requires unit AI to track target entity
            // and execute attack when in range
        }
    }
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
    // Validate building placement
    // Check if position is valid, not occupied, player has resources, etc.

    // Get building type from index
    if (buildingTypeIndex < 0 || buildingTypeIndex >= static_cast<int>(BuildingType::COUNT)) {
        return false;
    }

    BuildingType buildingType = static_cast<BuildingType>(buildingTypeIndex);
    glm::ivec2 buildingSize = GetBuildingSize(buildingType);

    // Check bounds - ensure building fits within world bounds
    // Using a simple -100 to 100 world bound as default
    const int worldMin = -100;
    const int worldMax = 100;

    if (gridPos.x < worldMin || gridPos.x + buildingSize.x > worldMax ||
        gridPos.y < worldMin || gridPos.y + buildingSize.y > worldMax) {
        return false;  // Out of bounds
    }

    // Check for collision with existing buildings
    // This would require access to a building manager or tilemap
    // For now, we do a simple spatial check using the entity manager
    if (m_entityManager) {
        glm::vec3 center(
            gridPos.x + buildingSize.x * 0.5f,
            0.0f,
            gridPos.y + buildingSize.y * 0.5f
        );

        float checkRadius = std::max(buildingSize.x, buildingSize.y) * 0.7f;
        auto nearbyEntities = m_entityManager->FindEntitiesInRadius(center, checkRadius);

        for (Entity* entity : nearbyEntities) {
            // Check if any existing entity would overlap
            // Buildings and static objects block placement
            if (entity->IsCollidable()) {
                glm::vec3 entityPos = entity->GetPosition();
                // Simple AABB check
                float halfSizeX = buildingSize.x * 0.5f;
                float halfSizeY = buildingSize.y * 0.5f;

                if (std::abs(entityPos.x - center.x) < halfSizeX + entity->GetCollisionRadius() &&
                    std::abs(entityPos.z - center.z) < halfSizeY + entity->GetCollisionRadius()) {
                    return false;  // Collision with existing entity
                }
            }
        }
    }

    // Additional checks could include:
    // - Resource cost validation (does player have enough resources?)
    // - Tech tree requirements (is the building unlocked?)
    // - Terrain type validation (can this building be placed on this terrain?)
    // For now, placement is valid if no collisions detected

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

    // Draw selection box in world space (project corners to ground plane)
    if (m_selectionBox.active && m_selectionBox.IsValidSize(1.0f)) {
        glm::vec2 min, max;
        m_selectionBox.GetNormalized(min, max);

        // Convert screen corners to world positions for 3D visualization
        glm::vec3 worldCorners[4];
        worldCorners[0] = ScreenToWorldPosition(glm::vec2(min.x, min.y));  // Top-left
        worldCorners[1] = ScreenToWorldPosition(glm::vec2(max.x, min.y));  // Top-right
        worldCorners[2] = ScreenToWorldPosition(glm::vec2(max.x, max.y));  // Bottom-right
        worldCorners[3] = ScreenToWorldPosition(glm::vec2(min.x, max.y));  // Bottom-left

        // Lift slightly above ground for visibility
        for (int i = 0; i < 4; i++) {
            worldCorners[i].y = 0.15f;
        }

        // Draw selection rectangle as lines on ground plane
        glm::vec4 selectionColor(0.0f, 1.0f, 0.0f, 0.8f);
        debugDraw.AddLine(worldCorners[0], worldCorners[1], selectionColor);
        debugDraw.AddLine(worldCorners[1], worldCorners[2], selectionColor);
        debugDraw.AddLine(worldCorners[2], worldCorners[3], selectionColor);
        debugDraw.AddLine(worldCorners[3], worldCorners[0], selectionColor);
    }

    // Draw building placement preview
    if (m_buildingPreview.active) {
        glm::vec4 color = m_buildingPreview.GetColor();

        // Get building size for accurate preview
        BuildingType buildingType = static_cast<BuildingType>(m_buildingPreview.buildingTypeIndex);
        glm::ivec2 buildingSize = GetBuildingSize(buildingType);

        // Calculate center position for the building
        glm::vec3 centerPos(
            m_buildingPreview.gridPosition.x + buildingSize.x * 0.5f,
            buildingSize.y * 0.25f,  // Half of wall height for box center
            m_buildingPreview.gridPosition.y + buildingSize.y * 0.5f
        );

        // Draw building footprint box with correct size
        glm::vec3 boxSize(
            static_cast<float>(buildingSize.x),
            1.0f,  // Placeholder wall height
            static_cast<float>(buildingSize.y)
        );
        debugDraw.AddBox(centerPos, boxSize, color);

        // Draw grid cells the building will occupy
        glm::vec4 gridColor = color;
        gridColor.a = 0.3f;

        for (int x = 0; x < buildingSize.x; x++) {
            for (int z = 0; z < buildingSize.y; z++) {
                glm::vec3 cellPos(
                    m_buildingPreview.gridPosition.x + x + 0.5f,
                    0.05f,
                    m_buildingPreview.gridPosition.y + z + 0.5f
                );
                debugDraw.AddBox(cellPos, glm::vec3(0.95f, 0.1f, 0.95f), gridColor);
            }
        }

        // Draw rotation indicator (forward direction)
        float rotRad = glm::radians(m_buildingPreview.rotation);
        glm::vec3 forward(std::sin(rotRad), 0.0f, std::cos(rotRad));
        glm::vec3 arrowStart = centerPos;
        arrowStart.y = 0.5f;
        glm::vec3 arrowEnd = arrowStart + forward * 2.0f;
        debugDraw.AddLine(arrowStart, arrowEnd, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    // Draw selection circles for selected units
    for (Entity* entity : m_selection.selectedUnits) {
        if (!entity) continue;

        glm::vec3 pos = entity->GetPosition();
        pos.y = 0.1f; // Slightly above ground

        // Draw selection circle with radius based on entity collision radius
        float radius = entity->GetCollisionRadius() * 1.5f;
        debugDraw.AddCircle(pos, radius, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 16);

        // Draw health bar above entity if damaged
        if (entity->GetHealthPercent() < 1.0f) {
            glm::vec3 healthBarPos = entity->GetPosition();
            healthBarPos.y = 1.5f;  // Above entity

            float healthWidth = 1.0f;
            float healthHeight = 0.1f;

            // Background (red)
            glm::vec3 bgLeft = healthBarPos - glm::vec3(healthWidth * 0.5f, 0, 0);
            glm::vec3 bgRight = healthBarPos + glm::vec3(healthWidth * 0.5f, 0, 0);
            debugDraw.AddLine(bgLeft, bgRight, glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));

            // Foreground (green, based on health)
            float healthLength = healthWidth * entity->GetHealthPercent();
            glm::vec3 fgRight = bgLeft + glm::vec3(healthLength, 0, 0);
            debugDraw.AddLine(bgLeft, fgRight, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
    }

    // Draw selected building indicator
    if (m_selection.selectedBuilding) {
        glm::vec3 pos = m_selection.selectedBuilding->GetPosition();
        pos.y = 0.1f;

        // Draw selection box around building
        glm::ivec2 size = m_selection.selectedBuilding->GetSize();
        glm::vec3 boxSize(static_cast<float>(size.x), 0.2f, static_cast<float>(size.y));
        debugDraw.AddBox(pos, boxSize, glm::vec4(0.0f, 0.8f, 1.0f, 0.8f));
    }
}

} // namespace RTS
} // namespace Vehement
