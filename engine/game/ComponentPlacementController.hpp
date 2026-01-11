#pragma once

#include "BuildingComponentSystem.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <stack>

namespace Nova {
namespace Buildings {

/**
 * @brief Interactive placement controller with mouse-based controls
 *
 * Controls:
 * - Mouse position: Translate component in world space
 * - Mouse scroll (normal): Randomize variant
 * - Click and hold: Enter rotation mode
 * - Mouse scroll (while rotating): Scale component (0.7x - 1.2x)
 * - Ctrl+Z: Undo last placement
 * - Visual feedback: Green glow = valid placement, Red glow = invalid
 */
class ComponentPlacementController {
public:
    ComponentPlacementController(BuildingInstancePtr building);

    // Component selection
    void SelectComponent(ComponentPtr component);
    ComponentPtr GetSelectedComponent() const { return m_selectedComponent; }

    // Mouse input handling
    void UpdateMousePosition(const glm::vec3& worldPosition);
    void OnMouseScroll(float delta);
    void OnMouseDown(int button); // button: 0=left, 1=right, 2=middle
    void OnMouseUp(int button);
    void OnKeyPress(int key, bool ctrl, bool shift, bool alt);

    // Preview state
    struct PreviewState {
        PlacedComponent component;
        bool valid = false;
        std::vector<std::string> errors;
        glm::vec4 glowColor{0.0f, 1.0f, 0.0f, 0.5f}; // Green by default
    };
    const PreviewState& GetPreview() const { return m_preview; }
    bool HasPreview() const { return m_selectedComponent != nullptr; }

    // Placement actions
    bool PlaceComponent(); // Returns true if placement succeeded
    void CancelPlacement();

    // Undo/Redo
    void Undo();
    void Redo();
    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    // Configuration
    void SetSnapToGrid(bool enabled, float gridSize = 0.5f);
    void SetSnapToComponents(bool enabled);
    void SetMinScale(float scale) { m_minScale = scale; }
    void SetMaxScale(float scale) { m_maxScale = scale; }

    float GetMinScale() const { return m_minScale; }
    float GetMaxScale() const { return m_maxScale; }

    // Rotation mode state
    bool IsRotating() const { return m_isRotating; }
    float GetCurrentRotationAngle() const { return m_currentRotationAngle; }
    float GetCurrentScale() const { return m_currentScale; }

    // Update
    void Update(float deltaTime);

private:
    BuildingInstancePtr m_building;
    ComponentPtr m_selectedComponent;

    // Preview state
    PreviewState m_preview;
    glm::vec3 m_currentMousePosition{0, 0, 0};
    int m_currentVariantSeed = 0;

    // Interaction state
    bool m_isRotating = false;
    bool m_isMouseDown = false;
    glm::vec2 m_rotationStartPos{0, 0}; // Screen space position when rotation started
    float m_currentRotationAngle = 0.0f;
    float m_currentScale = 1.0f;

    // Scale limits
    float m_minScale = 0.7f;
    float m_maxScale = 1.2f;

    // Snapping
    bool m_snapToGrid = true;
    float m_gridSize = 0.5f;
    bool m_snapToComponents = true;

    // Undo/Redo
    struct PlacementAction {
        std::string componentId;
        PlacedComponent component;
        bool wasPlacement; // true = placement, false = removal
    };
    std::stack<PlacementAction> m_undoStack;
    std::stack<PlacementAction> m_redoStack;

    // Helper methods
    void UpdatePreview();
    void ValidatePreview();
    void RandomizeVariant();
    glm::vec3 ApplySnapping(const glm::vec3& position);
    void UpdateGlowColor();
    void ClearRedoStack();
};

/**
 * @brief Visual feedback renderer for component placement
 */
class ComponentPlacementVisualizer {
public:
    ComponentPlacementVisualizer() = default;

    // Render preview with glow effect
    void RenderPreview(const ComponentPlacementController::PreviewState& preview,
                      bool isRotating = false,
                      float rotationAngle = 0.0f);

    // Render grid when enabled
    void RenderGrid(const glm::vec3& centerPosition, float gridSize, int gridExtent = 20);

    // Render rotation handle
    void RenderRotationHandle(const glm::vec3& position, float currentAngle, float radius = 1.0f);

    // Render scale indicator
    void RenderScaleIndicator(const glm::vec3& position, float scale, float minScale, float maxScale);

    // Render component bounds (for debugging)
    void RenderComponentBounds(const PlacedComponent& component, const glm::vec4& color);

    // Configuration
    void SetGlowIntensity(float intensity) { m_glowIntensity = intensity; }
    void SetGlowPulseSpeed(float speed) { m_glowPulseSpeed = speed; }

private:
    float m_glowIntensity = 0.5f;
    float m_glowPulseSpeed = 2.0f;
    float m_pulseTime = 0.0f;
};

/**
 * @brief Input manager for placement controller
 */
class PlacementInputManager {
public:
    struct MouseState {
        glm::vec2 position{0, 0};        // Screen space
        glm::vec2 delta{0, 0};           // Frame delta
        glm::vec3 worldPosition{0, 0, 0}; // Raycast to ground plane
        float scrollDelta = 0.0f;
        bool leftButton = false;
        bool rightButton = false;
        bool middleButton = false;
    };

    struct KeyboardState {
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
        std::vector<int> pressedKeys;
    };

    // Update input state (call each frame)
    void UpdateMouseState(const glm::vec2& screenPos, const glm::vec2& screenDelta,
                         bool left, bool right, bool middle);
    void UpdateScrollDelta(float delta);
    void UpdateKeyboardState(bool ctrl, bool shift, bool alt, const std::vector<int>& keys);

    // Convert screen position to world position via raycast to ground plane
    void UpdateWorldPosition(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection,
                            const glm::vec2& screenPos, const glm::vec2& screenSize);

    const MouseState& GetMouseState() const { return m_mouseState; }
    const KeyboardState& GetKeyboardState() const { return m_keyboardState; }

    // Check for specific key presses
    bool IsKeyPressed(int key) const;
    bool WasKeyJustPressed(int key) const;

private:
    MouseState m_mouseState;
    MouseState m_previousMouseState;
    KeyboardState m_keyboardState;
    KeyboardState m_previousKeyboardState;
};

} // namespace Buildings
} // namespace Nova
