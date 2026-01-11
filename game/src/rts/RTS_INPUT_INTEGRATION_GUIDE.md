# RTS Input Controller Integration Guide

## Overview
This guide explains how to integrate the RTSInputController into the RTSApplication for Solo Play mode.

## Files Created

### 1. RTSInputController.hpp/cpp
**Location:** `H:/Github/Old3DEngine/game/src/rts/`

Complete input controller for RTS gameplay with:
- Unit selection (click & drag-box)
- Unit commands (move, attack-move, patrol, stop, hold)
- Building placement with preview
- RTS camera controls (WASD pan, edge scrolling, zoom)
- Control groups (1-9) with Ctrl to assign
- Camera bookmarks (F1-F8) with Ctrl to save
- Gamepad support foundation

### 2. RTSGamepadControls.hpp
**Location:** `H:/Github/Old3DEngine/game/src/rts/`

Gamepad control scheme for RTS with:
- Virtual cursor movement (Left Stick)
- Camera pan (Right Stick)
- Zoom (Triggers)
- Button mapping for selection/commands
- Configurable sensitivity and deadzones

## Integration Steps for RTSApplication

### Step 1: Add Includes to RTSApplication.cpp
```cpp
#include "scene/Camera.hpp"
#include "../game/src/rts/RTSInputController.hpp"
#include "../game/src/entities/Player.hpp"
```

### Step 2: Initialize RTS Systems in Initialize()
```cpp
bool RTSApplication::Initialize() {
    // ... existing initialization code ...

    // Create RTS camera (separate from fly camera)
    m_rtsCamera = std::make_unique<Nova::Camera>();
    m_rtsCamera->SetPerspective(45.0f,
        Nova::Engine::Instance().GetWindow().GetAspectRatio(),
        0.1f, 1000.0f);

    // Create player entity
    m_player = std::make_unique<Vehement::Player>();
    m_player->SetPosition(glm::vec3(0, 0, 0));
    m_player->SetName("Player");

    // Create RTS input controller
    m_rtsInputController = std::make_unique<Vehement::RTS::RTSInputController>();
    if (!m_rtsInputController->Initialize(m_rtsCamera.get(), m_player.get())) {
        spdlog::error("Failed to initialize RTS input controller");
        return false;
    }

    // Set up RTS camera bounds (adjust based on your map size)
    m_rtsInputController->GetCamera().SetBounds(
        glm::vec2(-50.0f, -50.0f),
        glm::vec2(50.0f, 50.0f)
    );

    spdlog::info("RTS Application initialized");
    return true;
}
```

### Step 3: Update Camera Logic in Update()
```cpp
void RTSApplication::Update(float deltaTime) {
    auto& engine = Nova::Engine::Instance();
    auto& input = engine.GetInput();

    // Switch camera based on game mode
    if (m_currentMode == GameMode::Solo || m_currentMode == GameMode::Editor) {
        // Use RTS camera for Solo/Editor modes
        if (m_rtsInputController) {
            m_rtsInputController->Update(input, deltaTime);
        }
    } else if (m_currentMode != GameMode::MainMenu) {
        // Use fly camera for other modes
        m_camera->Update(input, deltaTime);

        // Toggle cursor lock with Tab
        if (input.IsKeyPressed(Nova::Key::Tab)) {
            bool locked = !input.IsCursorLocked();
            input.SetCursorLocked(locked);
        }
    } else {
        // In main menu, ensure cursor is not locked
        if (input.IsCursorLocked()) {
            input.SetCursorLocked(false);
        }
    }

    // Escape to return to main menu or quit
    if (input.IsKeyPressed(Nova::Key::Escape)) {
        if (m_currentMode == GameMode::MainMenu) {
            engine.RequestShutdown();
        } else {
            ReturnToMainMenu();
        }
    }

    // Update rotation
    m_rotationAngle += deltaTime * 45.0f;
}
```

### Step 4: Update Render() to Use Appropriate Camera
```cpp
void RTSApplication::Render() {
    auto& engine = Nova::Engine::Instance();
    auto& renderer = engine.GetRenderer();
    auto& debugDraw = renderer.GetDebugDraw();

    // Set camera based on mode
    if (m_currentMode == GameMode::Solo || m_currentMode == GameMode::Editor) {
        renderer.SetCamera(m_rtsCamera.get());
    } else {
        renderer.SetCamera(m_camera.get());
    }

    // ... existing rendering code ...

    // Render RTS input controller overlays
    if (m_rtsInputController &&
        (m_currentMode == GameMode::Solo || m_currentMode == GameMode::Editor)) {
        m_rtsInputController->Render(renderer);
    }
}
```

### Step 5: Update RenderSoloGame() UI
```cpp
void RTSApplication::RenderSoloGame() {
    ImGui::Begin("Solo Game - RTS Controls");

    ImGui::Text("Solo Play Mode");
    ImGui::Separator();

    if (m_rtsInputController) {
        const auto& camera = m_rtsInputController->GetCamera();
        glm::vec3 camPos = camera.GetPosition();

        ImGui::Text("Camera Position: %.1f, %.1f, %.1f", camPos.x, camPos.y, camPos.z);
        ImGui::Text("Camera Zoom: %.1f", camera.GetZoom());

        const auto& selection = m_rtsInputController->GetSelection();
        ImGui::Text("Selected Units: %zu", selection.selectedUnits.size());
    }

    ImGui::Spacing();
    ImGui::Text("RTS Controls:");
    ImGui::BulletText("WASD / Arrow Keys - Pan camera");
    ImGui::BulletText("Mouse Edges - Edge scrolling");
    ImGui::BulletText("Scroll Wheel - Zoom");
    ImGui::BulletText("Left Click - Select units");
    ImGui::BulletText("Left Click + Drag - Box select");
    ImGui::BulletText("Right Click - Move / Command");
    ImGui::BulletText("Shift + Right Click - Attack move");
    ImGui::BulletText("S - Stop units");
    ImGui::BulletText("H - Hold position");
    ImGui::BulletText("Ctrl+1-9 - Assign control group");
    ImGui::BulletText("1-9 - Select control group");
    ImGui::BulletText("Ctrl+F1-F8 - Save camera bookmark");
    ImGui::BulletText("F1-F8 - Restore camera bookmark");
    ImGui::BulletText("Spacebar - Focus on selection");
    ImGui::BulletText("Escape - Return to Main Menu");

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Return to Main Menu")) {
        ReturnToMainMenu();
    }

    ImGui::End();
}
```

### Step 6: Cleanup in Shutdown()
```cpp
void RTSApplication::Shutdown() {
    spdlog::info("Shutting down RTS Application");

    m_rtsInputController.reset();
    m_player.reset();
    m_rtsCamera.reset();
    m_camera.reset();
    m_cubeMesh.reset();
    m_sphereMesh.reset();
    m_planeMesh.reset();
    m_basicShader.reset();
}
```

## Key Features Implemented

### Unit Selection
- **Single Click**: Select individual unit
- **Drag Box**: Select multiple units in rectangle
- **Shift + Click**: Add to selection
- **Ctrl + A**: Select all units
- **Escape**: Clear selection

### Unit Commands
- **Right Click Ground**: Move command
- **Shift + Right Click**: Attack-move command
- **S Key**: Stop command
- **H Key**: Hold position command
- **P Key**: Patrol mode (future implementation)

### Camera Controls
- **WASD / Arrow Keys**: Pan camera
- **Mouse at Screen Edges**: Edge scrolling (20px margin)
- **Scroll Wheel**: Zoom in/out
- **Page Up/Down**: Alternative zoom controls
- **Spacebar**: Focus camera on selection

### Control Groups
- **Ctrl + 1-9**: Assign selection to control group
- **1-9**: Select control group
- **Shift + 1-9**: Add selection to control group

### Camera Bookmarks
- **Ctrl + F1-F8**: Save current camera position
- **F1-F8**: Restore saved camera position

### Building Placement
- **StartBuildingPlacement(type)**: Enter placement mode
- **Mouse Movement**: Preview building position
- **Left Click**: Confirm placement
- **Right Click / Escape**: Cancel placement
- **R Key**: Rotate building preview

## Callback System

The RTSInputController provides callbacks for integration:

```cpp
// Selection changed
m_rtsInputController->SetOnSelectionChanged([](const auto& selection) {
    spdlog::info("Selection changed: {} units selected", selection.selectedUnits.size());
});

// Command issued
m_rtsInputController->SetOnCommand([](CommandType type, const glm::vec3& pos) {
    spdlog::info("Command: {} at ({}, {}, {})",
        static_cast<int>(type), pos.x, pos.y, pos.z);
});

// Building placed
m_rtsInputController->SetOnBuildingPlaced([](int typeIndex, const glm::ivec2& gridPos) {
    spdlog::info("Building {} placed at ({}, {})", typeIndex, gridPos.x, gridPos.y);
});
```

## Next Steps

1. **Entity Management**: Connect to EntityManager for actual unit selection
2. **Pathfinding**: Integrate movement commands with pathfinding system
3. **Building System**: Connect building placement to building creation
4. **Unit AI**: Implement unit command execution
5. **Minimap**: Add minimap camera positioning
6. **Selection Visuals**: Add custom selection circle/box rendering
7. **Gamepad Implementation**: Complete gamepad controls implementation
8. **Multiplayer**: Sync commands across network for online mode

## Configuration

Camera settings can be adjusted via the RTSCamera:
```cpp
auto& camera = m_rtsInputController->GetCamera();
camera.panSpeed = 25.0f;           // Camera pan speed
camera.edgeScrollSpeed = 20.0f;    // Edge scrolling speed
camera.zoomSpeed = 8.0f;           // Zoom speed
camera.minZoom = 5.0f;             // Minimum zoom distance
camera.maxZoom = 60.0f;            // Maximum zoom distance
```

Edge scrolling can be toggled:
```cpp
camera.SetEdgeScrollingEnabled(true);  // Enable/disable
camera.SetEdgeScrollMargin(25.0f);     // Pixels from edge
```

## Troubleshooting

### Camera not responding
- Ensure RTSInputController is initialized before Update() is called
- Check that correct camera is being set in Render()
- Verify game mode is Solo or Editor

### Selection not working
- Implement GetEntityAtScreenPosition() with raycasting
- Connect to EntityManager for entity queries
- Ensure entities are in correct spatial data structure

### Commands not executing
- Connect callbacks to actual unit command system
- Implement unit AI to execute commands
- Add pathfinding integration

## Architecture

```
RTSApplication
├── FlyCamera (for non-RTS modes)
├── RTSCamera (for RTS modes)
│   └── Controlled by RTSInputController
├── RTSInputController
│   ├── RTSCamera (camera control)
│   ├── SelectionState (current selection)
│   ├── BuildingPlacementPreview (building mode)
│   └── Control Groups (saved selections)
└── Player (player entity)
```

## Performance Considerations

- Selection box rendering uses debug draw (optimized for debug visualization)
- Entity queries should use spatial partitioning (quadtree/grid)
- Camera culling reduces rendered entities
- Control group updates are O(1) lookup
- Drag selection validates size before processing

## Platform Support

- **Windows/Linux/Mac**: Full keyboard + mouse support
- **Gamepad**: Foundation implemented, needs per-platform input mapping
- **Touch**: Adaptable via mobile input layer (not yet implemented)
