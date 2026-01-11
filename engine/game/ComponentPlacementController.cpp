#include "ComponentPlacementController.hpp"
#include <algorithm>
#include <random>
#include <cmath>

namespace Nova {
namespace Buildings {

// =============================================================================
// ComponentPlacementController Implementation
// =============================================================================

ComponentPlacementController::ComponentPlacementController(BuildingInstancePtr building)
    : m_building(building) {

    // Initialize random seed
    std::random_device rd;
    m_currentVariantSeed = rd();
}

void ComponentPlacementController::SelectComponent(ComponentPtr component) {
    m_selectedComponent = component;

    if (component) {
        // Reset state for new component
        m_currentRotationAngle = 0.0f;
        m_currentScale = 1.0f;
        m_isRotating = false;

        // Generate initial random variant
        RandomizeVariant();

        // Update preview
        UpdatePreview();
    } else {
        m_preview = PreviewState{};
    }
}

void ComponentPlacementController::UpdateMousePosition(const glm::vec3& worldPosition) {
    m_currentMousePosition = worldPosition;

    if (m_selectedComponent && !m_isRotating) {
        UpdatePreview();
    }
}

void ComponentPlacementController::OnMouseScroll(float delta) {
    if (!m_selectedComponent) return;

    if (m_isRotating) {
        // Scale mode: adjust scale when rotating
        float scaleStep = 0.05f;
        m_currentScale += delta * scaleStep;
        m_currentScale = glm::clamp(m_currentScale, m_minScale, m_maxScale);
        UpdatePreview();
    } else {
        // Variant mode: randomize variant on scroll
        if (delta != 0.0f) {
            RandomizeVariant();
            UpdatePreview();
        }
    }
}

void ComponentPlacementController::OnMouseDown(int button) {
    if (button == 0) { // Left mouse button
        m_isMouseDown = true;
        m_isRotating = true;
        m_rotationStartPos = glm::vec2(m_currentMousePosition.x, m_currentMousePosition.z);
    }
}

void ComponentPlacementController::OnMouseUp(int button) {
    if (button == 0) { // Left mouse button
        m_isMouseDown = false;

        if (m_isRotating) {
            m_isRotating = false;

            // Place component on release
            if (m_preview.valid) {
                PlaceComponent();
            }
        }
    }
}

void ComponentPlacementController::OnKeyPress(int key, bool ctrl, bool shift, bool alt) {
    // Ctrl+Z for undo
    if (ctrl && key == 'Z' && !shift) {
        Undo();
    }
    // Ctrl+Shift+Z or Ctrl+Y for redo
    else if (ctrl && ((key == 'Z' && shift) || key == 'Y')) {
        Redo();
    }
    // Escape to cancel
    else if (key == 27) { // ESC
        CancelPlacement();
    }
}

bool ComponentPlacementController::PlaceComponent() {
    if (!m_selectedComponent || !m_preview.valid) {
        return false;
    }

    // Add component to building
    std::string componentId = m_building->AddComponent(m_preview.component);

    if (!componentId.empty()) {
        // Add to undo stack
        PlacementAction action;
        action.componentId = componentId;
        action.component = m_preview.component;
        action.wasPlacement = true;
        m_undoStack.push(action);

        // Clear redo stack on new action
        ClearRedoStack();

        // Reset preview for next placement
        m_currentRotationAngle = 0.0f;
        m_currentScale = 1.0f;
        RandomizeVariant();
        UpdatePreview();

        return true;
    }

    return false;
}

void ComponentPlacementController::CancelPlacement() {
    m_selectedComponent = nullptr;
    m_preview = PreviewState{};
    m_isRotating = false;
    m_isMouseDown = false;
}

void ComponentPlacementController::Undo() {
    if (m_undoStack.empty()) return;

    PlacementAction action = m_undoStack.top();
    m_undoStack.pop();

    if (action.wasPlacement) {
        // Remove the component
        m_building->RemoveComponent(action.componentId);

        // Add to redo stack
        m_redoStack.push(action);
    }
}

void ComponentPlacementController::Redo() {
    if (m_redoStack.empty()) return;

    PlacementAction action = m_redoStack.top();
    m_redoStack.pop();

    if (action.wasPlacement) {
        // Re-add the component
        std::string componentId = m_building->AddComponent(action.component);
        action.componentId = componentId;

        // Add back to undo stack
        m_undoStack.push(action);
    }
}

void ComponentPlacementController::SetSnapToGrid(bool enabled, float gridSize) {
    m_snapToGrid = enabled;
    m_gridSize = gridSize;

    if (m_selectedComponent) {
        UpdatePreview();
    }
}

void ComponentPlacementController::SetSnapToComponents(bool enabled) {
    m_snapToComponents = enabled;

    if (m_selectedComponent) {
        UpdatePreview();
    }
}

void ComponentPlacementController::Update(float deltaTime) {
    // Update rotation angle based on mouse movement when rotating
    if (m_isRotating && m_isMouseDown) {
        glm::vec2 currentPos(m_currentMousePosition.x, m_currentMousePosition.z);
        glm::vec2 delta = currentPos - m_rotationStartPos;

        // Calculate angle from center to current mouse position
        float angle = atan2(delta.y, delta.x);
        m_currentRotationAngle = glm::degrees(angle);

        UpdatePreview();
    }
}

void ComponentPlacementController::UpdatePreview() {
    if (!m_selectedComponent) {
        m_preview = PreviewState{};
        return;
    }

    // Create preview component
    PlacedComponent preview;
    preview.component = m_selectedComponent;

    // Apply snapping to position
    preview.position = ApplySnapping(m_currentMousePosition);

    // Apply rotation
    float rotationRadians = glm::radians(m_currentRotationAngle);
    preview.rotation = glm::angleAxis(rotationRadians, glm::vec3(0, 1, 0));

    // Apply scale
    preview.scale = glm::vec3(m_currentScale);

    // Set random seed
    preview.randomSeed = m_currentVariantSeed;

    m_preview.component = preview;

    // Validate placement
    ValidatePreview();

    // Update glow color based on validity
    UpdateGlowColor();
}

void ComponentPlacementController::ValidatePreview() {
    m_preview.errors.clear();
    m_preview.valid = m_building->ValidateComponentPlacement(m_preview.component, m_preview.errors);
}

void ComponentPlacementController::RandomizeVariant() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 999999);
    m_currentVariantSeed = dist(rng);
}

glm::vec3 ComponentPlacementController::ApplySnapping(const glm::vec3& position) {
    glm::vec3 snapped = position;

    if (m_snapToGrid) {
        // Snap to grid
        snapped.x = std::round(position.x / m_gridSize) * m_gridSize;
        snapped.z = std::round(position.z / m_gridSize) * m_gridSize;

        // Keep Y as is for ground placement
        snapped.y = position.y;
    }

    if (m_snapToComponents) {
        // Check for nearby components and snap to them
        const auto& components = m_building->GetAllComponents();
        float snapDistance = 0.5f; // Snap within 0.5 units

        for (const auto& comp : components) {
            glm::vec3 compPos = comp.position;
            float distance = glm::distance(glm::vec2(snapped.x, snapped.z),
                                          glm::vec2(compPos.x, compPos.z));

            if (distance < snapDistance) {
                // Snap to component position
                snapped.x = compPos.x;
                snapped.z = compPos.z;
                break;
            }
        }
    }

    return snapped;
}

void ComponentPlacementController::UpdateGlowColor() {
    if (m_preview.valid) {
        // Green glow for valid placement
        m_preview.glowColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.5f);
    } else {
        // Red glow for invalid placement
        m_preview.glowColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
    }
}

void ComponentPlacementController::ClearRedoStack() {
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }
}

// =============================================================================
// ComponentPlacementVisualizer Implementation
// =============================================================================

void ComponentPlacementVisualizer::RenderPreview(const ComponentPlacementController::PreviewState& preview,
                                                bool isRotating,
                                                float rotationAngle) {
    // This would render the component with glow effect using the rendering system
    // Implementation depends on the rendering backend (OpenGL, Vulkan, etc.)

    // Pseudo-code:
    // 1. Render component SDF model at preview.component.position
    // 2. Apply preview.component.rotation and preview.component.scale
    // 3. Apply glow shader with preview.glowColor
    // 4. If isRotating, render rotation handle
}

void ComponentPlacementVisualizer::RenderGrid(const glm::vec3& centerPosition, float gridSize, int gridExtent) {
    // Render grid lines for visual reference
    // Implementation depends on rendering backend
}

void ComponentPlacementVisualizer::RenderRotationHandle(const glm::vec3& position, float currentAngle, float radius) {
    // Render circular rotation handle with current angle indicator
    // Shows user the rotation amount visually
}

void ComponentPlacementVisualizer::RenderScaleIndicator(const glm::vec3& position, float scale,
                                                       float minScale, float maxScale) {
    // Render visual indicator showing current scale
    // Could be a ring or bar showing scale percentage
}

void ComponentPlacementVisualizer::RenderComponentBounds(const PlacedComponent& component,
                                                        const glm::vec4& color) {
    // Render bounding box for debugging/visualization
}

// =============================================================================
// PlacementInputManager Implementation
// =============================================================================

void PlacementInputManager::UpdateMouseState(const glm::vec2& screenPos, const glm::vec2& screenDelta,
                                            bool left, bool right, bool middle) {
    m_previousMouseState = m_mouseState;

    m_mouseState.position = screenPos;
    m_mouseState.delta = screenDelta;
    m_mouseState.leftButton = left;
    m_mouseState.rightButton = right;
    m_mouseState.middleButton = middle;
}

void PlacementInputManager::UpdateScrollDelta(float delta) {
    m_mouseState.scrollDelta = delta;
}

void PlacementInputManager::UpdateKeyboardState(bool ctrl, bool shift, bool alt,
                                               const std::vector<int>& keys) {
    m_previousKeyboardState = m_keyboardState;

    m_keyboardState.ctrl = ctrl;
    m_keyboardState.shift = shift;
    m_keyboardState.alt = alt;
    m_keyboardState.pressedKeys = keys;
}

void PlacementInputManager::UpdateWorldPosition(const glm::vec3& cameraPosition,
                                               const glm::vec3& cameraDirection,
                                               const glm::vec2& screenPos,
                                               const glm::vec2& screenSize) {
    // Raycast from camera through screen position to ground plane (y=0)
    // This is a simplified implementation - actual raycast would be more complex

    // Convert screen to normalized device coordinates
    glm::vec2 ndc;
    ndc.x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    ndc.y = 1.0f - (2.0f * screenPos.y) / screenSize.y;

    // Ray direction (simplified - would need view/projection matrices)
    glm::vec3 rayDir = glm::normalize(cameraDirection);

    // Intersect with ground plane (y = 0)
    if (rayDir.y != 0.0f) {
        float t = -cameraPosition.y / rayDir.y;
        if (t > 0.0f) {
            m_mouseState.worldPosition = cameraPosition + rayDir * t;
        }
    }
}

bool PlacementInputManager::IsKeyPressed(int key) const {
    return std::find(m_keyboardState.pressedKeys.begin(),
                    m_keyboardState.pressedKeys.end(),
                    key) != m_keyboardState.pressedKeys.end();
}

bool PlacementInputManager::WasKeyJustPressed(int key) const {
    bool currentlyPressed = IsKeyPressed(key);
    bool previouslyPressed = std::find(m_previousKeyboardState.pressedKeys.begin(),
                                      m_previousKeyboardState.pressedKeys.end(),
                                      key) != m_previousKeyboardState.pressedKeys.end();

    return currentlyPressed && !previouslyPressed;
}

} // namespace Buildings
} // namespace Nova
