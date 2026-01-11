# Building Component Placement Controls

## Overview

The Component Placement Controller provides an intuitive mouse-based interface for placing building components with visual feedback.

## Control Scheme

### Component Translation
- **Mouse Movement**: Component follows mouse cursor in world space
- **Visual Feedback**:
  - **Green glow**: Valid placement location
  - **Red glow**: Invalid placement (intersections, missing support, etc.)

### Variant Selection
- **Mouse Scroll (normal mode)**: Randomize component variant
  - Each scroll generates a new random seed
  - Cycles through different visual styles based on building level

### Rotation
- **Click and Hold (Left Mouse)**: Enter rotation mode
  - Component rotates based on mouse position relative to start point
  - Angle calculated from center to current mouse position
  - Visual rotation handle appears

### Scaling
- **Mouse Scroll (while rotating)**: Adjust component scale
  - Scale range: **0.7x to 1.2x** (configurable)
  - Step size: 0.05x per scroll increment
  - Clamped to min/max bounds
  - Visual scale indicator appears

### Placement Confirmation
- **Release Left Mouse**: Place component at current position/rotation/scale
  - Only places if position is valid (green glow)
  - Automatically adds to undo stack

### Undo/Redo
- **Ctrl+Z**: Undo last placement
  - Removes most recently placed component
  - Can undo multiple placements
- **Ctrl+Shift+Z** or **Ctrl+Y**: Redo placement
  - Restores undone component
  - Redo stack clears on new placement

### Cancellation
- **ESC**: Cancel current placement operation
  - Deselects component
  - Clears preview

## Snapping Behavior

### Grid Snapping (enabled by default)
- Snaps component position to grid (default: 0.5 unit grid)
- Only affects X and Z axes (horizontal plane)
- Configurable grid size

### Component Snapping (enabled by default)
- Snaps to nearby existing components
- Snap distance: 0.5 units (configurable)
- Helps align components

## Visual Feedback

### Preview Component
- Rendered at mouse position with transparency
- Shows exact placement with rotation and scale applied
- Updates in real-time

### Glow Effect
- **Green (0, 1, 0, 0.5)**: Valid placement
- **Red (1, 0, 0, 0.5)**: Invalid placement
  - Shows validation errors in UI

### Rotation Handle
- Circular indicator showing rotation angle
- Visible during click-hold rotation mode

### Scale Indicator
- Visual ring or bar showing current scale
- Displays percentage between min/max scale

### Grid Overlay
- Optional grid lines for reference
- Centered on mouse position
- Extends ±20 units (configurable)

## Usage Example

```cpp
// Initialize controller with building instance
auto building = std::make_shared<BuildingInstance>(farmTemplate);
ComponentPlacementController controller(building);

// Select component to place
auto barnComponent = ComponentLibrary::Instance().GetComponent("human_farm_barn");
controller.SelectComponent(barnComponent);

// In game update loop:
void Update(float deltaTime) {
    // Update mouse position (from raycast to ground)
    glm::vec3 worldPos = RaycastToGround(mouseScreenPos);
    controller.UpdateMousePosition(worldPos);

    // Handle scroll
    controller.OnMouseScroll(scrollDelta);

    // Handle mouse buttons
    if (mouseJustPressed) {
        controller.OnMouseDown(0); // Left button
    }
    if (mouseJustReleased) {
        controller.OnMouseUp(0);
    }

    // Handle keyboard
    if (keyPressed) {
        controller.OnKeyPress(key, ctrlPressed, shiftPressed, altPressed);
    }

    // Update controller
    controller.Update(deltaTime);

    // Render preview
    if (controller.HasPreview()) {
        visualizer.RenderPreview(controller.GetPreview(),
                                controller.IsRotating(),
                                controller.GetCurrentRotationAngle());
    }
}
```

## Configuration

```cpp
// Set scale limits
controller.SetMinScale(0.7f);  // 70% of original size
controller.SetMaxScale(1.2f);  // 120% of original size

// Configure snapping
controller.SetSnapToGrid(true, 0.5f);  // 0.5 unit grid
controller.SetSnapToComponents(true);

// Visualizer settings
visualizer.SetGlowIntensity(0.5f);
visualizer.SetGlowPulseSpeed(2.0f);
```

## Implementation Details

### Classes

- **ComponentPlacementController**: Main controller class
  - Manages input state and placement logic
  - Handles undo/redo stack
  - Validates placement positions

- **ComponentPlacementVisualizer**: Rendering helper
  - Renders preview with glow effects
  - Renders rotation handles and scale indicators
  - Renders optional grid overlay

- **PlacementInputManager**: Input abstraction
  - Manages mouse and keyboard state
  - Converts screen space to world space
  - Tracks state changes for event detection

### Key Features

1. **Real-time validation**: Components validated every frame
2. **Undo/Redo stack**: Full history of placements
3. **Variant randomization**: Infinite visual variety
4. **Smooth rotation**: Angle based on mouse vector
5. **Constrained scaling**: Clamped to sensible bounds
6. **Dual snapping**: Both grid and component snapping
7. **Visual feedback**: Clear indication of valid/invalid placement

## Performance Considerations

- Validation only runs when preview changes
- Snapping uses spatial queries (optimized for nearby components)
- Rendering uses instancing for efficiency
- Undo stack has configurable max size (default: 100 actions)

## Future Enhancements

- Multi-select for moving multiple components
- Copy/paste component configurations
- Symmetry mode for mirrored placement
- Custom snap points on components
- Placement presets/templates
- Keyboard shortcuts for common operations
