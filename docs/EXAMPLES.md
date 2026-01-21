# Nova3D Engine Examples Guide

This guide provides practical tutorials for running and understanding the demo projects, creating SDF models, and using the visual scripting system.

## Table of Contents

1. [Building and Running Demos](#building-and-running-demos)
2. [Nova Demo Features](#nova-demo-features)
3. [Creating SDF Models](#creating-sdf-models)
4. [Visual Scripting Basics](#visual-scripting-basics)
5. [Next Steps](#next-steps)

---

## Building and Running Demos

### Prerequisites

Before building the examples, ensure you have:

- **CMake 3.20+**
- **C++20 Compiler**: MSVC 2022, GCC 13+, or Clang 16+
- **Git** (for dependency fetching)

### Quick Build

#### Windows (Visual Studio)

```powershell
# Clone and enter the repository
git clone https://github.com/your-org/Nova3D.git
cd Nova3D

# Configure with CMake
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build in Release mode
cmake --build build --config Release --parallel

# Run the demo
.\build\bin\Release\nova_demo.exe
```

#### Windows (Command Line with MinGW)

```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
.\build\bin\nova_demo.exe
```

#### Linux / macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/nova_demo
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `NOVA_BUILD_EXAMPLES` | ON | Build demo applications |
| `NOVA_BUILD_TESTS` | OFF | Build unit tests |
| `CMAKE_BUILD_TYPE` | - | Debug, Release, RelWithDebInfo |

Example with custom options:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNOVA_BUILD_TESTS=ON
```

### Available Demo Executables

After building, you will find these executables in `build/bin/`:

| Executable | Description |
|------------|-------------|
| `nova_demo` | Basic engine demo with rendering, particles, terrain |
| `nova_editor` | Standalone level editor (if built) |

---

## Nova Demo Features

The `nova_demo` application demonstrates core Nova3D engine capabilities including:

- **3D Rendering**: Cube, sphere, and ground plane with Phong shading
- **Particle System**: Fire-like particles with customizable properties
- **Terrain Generation**: Procedural terrain rendering
- **Pathfinding Graph**: Visual pathfinding node network
- **Camera Controls**: Free-fly camera with mouse look
- **ImGui Interface**: Real-time parameter adjustment

### Demo Controls

| Key | Action |
|-----|--------|
| **W/A/S/D** | Move camera forward/left/back/right |
| **Right Mouse + Drag** | Look around |
| **Tab** | Toggle cursor lock |
| **Shift** | Sprint (faster movement) |
| **Escape** | Quit application |

### Demo UI Panels

The ImGui control panel allows you to:

1. **Render Options**
   - Toggle grid display
   - Toggle particle system
   - Toggle terrain rendering
   - Toggle pathfinding visualization

2. **Lighting Controls**
   - Adjust light direction (X, Y, Z)
   - Change light color (RGB picker)
   - Modify ambient intensity

3. **Particle Settings** (when enabled)
   - Emission rate slider
   - Start/end color pickers

### Demo Source Structure

The demo is organized in `examples/`:

```
examples/
  main.cpp              # Entry point and engine initialization
  DemoApplication.hpp   # Demo class header
  DemoApplication.cpp   # Demo implementation
```

### Entry Point (`main.cpp`)

```cpp
#include "core/Engine.hpp"
#include "DemoApplication.hpp"
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Nova3D Engine Demo");

    Nova::Engine& engine = Nova::Engine::Instance();

    // Configure initialization
    Nova::Engine::InitParams params;
    params.configPath = "assets/config/engine.json";
    params.enableImGui = true;
    params.enableDebugDraw = true;

    if (!engine.Initialize(params)) {
        spdlog::critical("Failed to initialize engine");
        return -1;
    }

    // Create demo application
    DemoApplication demo;

    // Set up callbacks
    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onStartup = [&demo]() { return demo.Initialize(); };
    callbacks.onUpdate = [&demo](float dt) { demo.Update(dt); };
    callbacks.onRender = [&demo]() { demo.Render(); };
    callbacks.onImGui = [&demo]() { demo.OnImGui(); };
    callbacks.onShutdown = [&demo]() { demo.Shutdown(); };

    return engine.Run(callbacks);
}
```

---

## Creating SDF Models

Nova3D uses Signed Distance Fields (SDFs) for high-quality rendering. This section shows how to create SDF models programmatically.

### What is an SDF?

A Signed Distance Field represents geometry as a function that returns the shortest distance from any point to the surface:
- **Positive values**: Outside the surface
- **Negative values**: Inside the surface
- **Zero**: On the surface

### Basic SDF Primitives

Nova3D supports these primitive types:

| Type | Description | Key Parameters |
|------|-------------|----------------|
| `Sphere` | Perfect sphere | `radius` |
| `Box` | Axis-aligned box | `dimensions` (vec3) |
| `RoundedBox` | Box with rounded edges | `dimensions`, `cornerRadius` |
| `Cylinder` | Vertical cylinder | `height`, `bottomRadius` |
| `Capsule` | Cylinder with hemispherical caps | `height`, `bottomRadius` |
| `Cone` | Circular cone | `height`, `bottomRadius`, `topRadius` |
| `Torus` | Donut shape | `majorRadius`, `minorRadius` |

### Example: Creating a Simple Model

```cpp
#include <engine/sdf/SDFModel.hpp>
#include <engine/sdf/SDFPrimitive.hpp>

// Create a model with a sphere
Nova::SDFModel model("MySphere");

// Create the sphere primitive
auto* sphere = model.CreatePrimitive("MainSphere", Nova::SDFPrimitiveType::Sphere);
sphere->GetParameters().radius = 1.0f;

// Set material properties (PBR)
auto& mat = sphere->GetMaterial();
mat.baseColor = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);  // Blue
mat.metallic = 0.0f;   // Non-metallic
mat.roughness = 0.3f;  // Slightly glossy
```

### Example: Creating a Robot Character

```cpp
Nova::SDFModel robot("Robot");

// Body (capsule as root)
auto* body = robot.CreatePrimitive("Body", Nova::SDFPrimitiveType::Capsule);
body->GetParameters().height = 2.0f;
body->GetParameters().bottomRadius = 0.5f;
body->GetMaterial().baseColor = glm::vec4(0.7f, 0.7f, 0.75f, 1.0f);

// Head (child of body)
auto* head = robot.CreatePrimitive("Head", Nova::SDFPrimitiveType::Sphere, body);
head->GetLocalTransform().position = glm::vec3(0, 1.5f, 0);
head->GetParameters().radius = 0.4f;
head->GetMaterial().baseColor = glm::vec4(0.8f, 0.8f, 0.85f, 1.0f);

// Left arm (child of body)
auto* leftArm = robot.CreatePrimitive("LeftArm", Nova::SDFPrimitiveType::Capsule, body);
leftArm->GetLocalTransform().position = glm::vec3(-0.8f, 0.5f, 0);
leftArm->GetLocalTransform().rotation = glm::angleAxis(
    glm::radians(90.0f), glm::vec3(0, 0, 1)
);
leftArm->GetParameters().height = 0.8f;
leftArm->GetParameters().bottomRadius = 0.15f;

// Right arm (child of body)
auto* rightArm = robot.CreatePrimitive("RightArm", Nova::SDFPrimitiveType::Capsule, body);
rightArm->GetLocalTransform().position = glm::vec3(0.8f, 0.5f, 0);
rightArm->GetLocalTransform().rotation = glm::angleAxis(
    glm::radians(-90.0f), glm::vec3(0, 0, 1)
);
rightArm->GetParameters().height = 0.8f;
rightArm->GetParameters().bottomRadius = 0.15f;
```

### CSG Operations

Combine primitives using Constructive Solid Geometry:

| Operation | Description | Use Case |
|-----------|-------------|----------|
| `Union` | Combine shapes | Building complex objects |
| `Subtraction` | Carve out shapes | Creating holes |
| `Intersection` | Keep overlap only | Boolean intersection |
| `SmoothUnion` | Smooth blend | Organic shapes |
| `SmoothSubtraction` | Smooth carve | Soft indentations |

#### Example: Box with a Hole

```cpp
Nova::SDFModel model("BoxWithHole");

// Main box
auto* box = model.CreatePrimitive("Box", Nova::SDFPrimitiveType::Box);
box->GetParameters().dimensions = glm::vec3(2.0f, 2.0f, 2.0f);

// Cylinder to subtract (creates the hole)
auto* hole = model.CreatePrimitive("Hole", Nova::SDFPrimitiveType::Cylinder, box);
hole->GetParameters().height = 3.0f;  // Extend through the box
hole->GetParameters().bottomRadius = 0.5f;
hole->SetCSGOperation(Nova::CSGOperation::Subtraction);
```

#### Example: Smooth Blob Union

```cpp
Nova::SDFModel blob("Blob");

// First sphere
auto* sphere1 = blob.CreatePrimitive("Sphere1", Nova::SDFPrimitiveType::Sphere);
sphere1->GetParameters().radius = 1.0f;

// Second sphere (overlapping)
auto* sphere2 = blob.CreatePrimitive("Sphere2", Nova::SDFPrimitiveType::Sphere);
sphere2->GetLocalTransform().position = glm::vec3(1.2f, 0, 0);
sphere2->GetParameters().radius = 0.8f;

// Enable smooth blending
sphere2->SetCSGOperation(Nova::CSGOperation::SmoothUnion);
sphere2->GetParameters().smoothness = 0.3f;  // Higher = smoother blend
```

### Rendering SDF Models

```cpp
#include <engine/graphics/SDFRenderer.hpp>

// Initialize renderer
Nova::SDFRenderer sdfRenderer;
sdfRenderer.Initialize();

// Configure render settings
auto& settings = sdfRenderer.GetSettings();
settings.maxSteps = 128;            // Raymarching iterations
settings.maxDistance = 100.0f;      // Max ray distance
settings.hitThreshold = 0.001f;     // Surface precision
settings.enableShadows = true;
settings.enableAO = true;           // Ambient occlusion

// In your render loop
sdfRenderer.Render(model, camera);
```

### Saving and Loading Models

```cpp
// Save to file
robot.SaveToFile("assets/models/robot.sdfmodel");

// Load from file
Nova::SDFModel loaded;
loaded.LoadFromFile("assets/models/robot.sdfmodel");
```

---

## Visual Scripting Basics

Nova3D includes a visual scripting system for creating game logic without writing C++ code.

### Core Concepts

1. **Nodes**: Processing blocks with inputs and outputs
2. **Ports**: Connection points (Flow, Data, Event, Binding)
3. **Connections**: Links between ports
4. **Graphs**: Collections of connected nodes

### Node Categories

| Category | Purpose | Example Nodes |
|----------|---------|---------------|
| **Flow** | Control execution order | Branch, Sequence, ForLoop, While |
| **Math** | Mathematical operations | Add, Subtract, Multiply, Clamp, Lerp |
| **Logic** | Boolean operations | AND, OR, NOT, Compare |
| **Data** | Variable management | GetVariable, SetVariable, MakeArray |
| **Event** | Event handling | Timer, PublishEvent, SubscribeEvent |
| **Binding** | Property references | GetProperty, SetProperty |

### Port Types

| Type | Color | Purpose |
|------|-------|---------|
| **Flow** | White | Execution order control |
| **Data** | Blue | Value passing |
| **Event** | Orange | Event triggers |
| **Binding** | Green | Property references |

### Example: Simple Branch Logic

This visual script checks if health is low and triggers a warning:

```
[OnHealthChanged] ──> [Compare] ──> [Branch]
                          │              │
                    health < 25     ┌────┴────┐
                                    │         │
                               [True]    [False]
                                    │
                               [ShowWarning]
```

### Creating a Graph Programmatically

```cpp
#include <engine/scripting/visual/VisualScriptingCore.hpp>
#include <engine/scripting/visual/StandardNodes.hpp>

using namespace Nova::VisualScript;

// Create a graph
auto graph = std::make_shared<Graph>("HealthCheck");

// Create nodes
auto compareNode = std::make_shared<CompareNode>();
compareNode->SetPosition(glm::vec2(100, 100));

auto branchNode = std::make_shared<BranchNode>();
branchNode->SetPosition(glm::vec2(300, 100));

// Add nodes to graph
graph->AddNode(compareNode);
graph->AddNode(branchNode);

// Connect nodes
auto compareResult = compareNode->GetOutputPort("result");
auto branchCondition = branchNode->GetInputPort("condition");
graph->Connect(compareResult, branchCondition);
```

### Using the Visual Script Editor

The `VisualScriptEditor` class provides a full ImGui-based editor:

```cpp
#include <engine/scripting/visual/VisualScriptEditor.hpp>

Nova::VisualScript::VisualScriptEditorWindow editorWindow;

// In your ImGui render loop
editorWindow.Render();

// Access the editor
auto& editor = editorWindow.GetEditor();

// Create a new graph
editor.NewGraph("MyGameLogic");

// Load an existing graph
editor.LoadGraph("assets/scripts/player_health.vsgraph");

// Save the current graph
editor.SaveGraph("assets/scripts/player_health.vsgraph");
```

### Editor Features

- **Node Palette**: Browse and search available nodes by category
- **Canvas**: Drag-and-drop node arrangement with zoom/pan
- **Property Inspector**: Configure selected node properties
- **Binding Browser**: Discover bindable properties from assets
- **Warnings Panel**: View loose or broken bindings

### Editor Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **Ctrl+C** | Copy selected nodes |
| **Ctrl+V** | Paste nodes |
| **Ctrl+D** | Duplicate selected |
| **Delete** | Delete selected nodes |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** | Redo |
| **Ctrl+A** | Select all |
| **F** | Frame selected nodes |

### Binding System

The visual scripting system supports property bindings:

- **Hard Bindings**: Properties with C++ reflection (type-safe)
- **Loose Bindings**: Properties from JSON assets (flexible)

```cpp
// Register properties from reflection
auto& registry = BindingRegistry::Instance();
registry.RefreshFromReflection();

// Register properties from assets
registry.RefreshFromAssets("assets/configs/");

// Search for bindable properties
auto results = registry.Search("health");
for (auto* prop : results) {
    // prop->id, prop->name, prop->typeName, etc.
}
```

### Example: Property Binding Node

```cpp
// Create a GetProperty node
auto getHealthNode = std::make_shared<GetPropertyNode>();
getHealthNode->SetPropertyPath("humans.units.warrior.stats.health");

// Check binding state
auto ref = BindingRegistry::Instance().ResolveBinding(
    "humans.units.warrior.stats.health"
);

if (ref.state == BindingState::HardBinding) {
    // Type-safe, validated binding
} else if (ref.state == BindingState::LooseBinding) {
    // Works but not type-checked
} else if (ref.state == BindingState::BrokenBinding) {
    // Property no longer exists
}
```

---

## Next Steps

After completing these examples, explore:

### Documentation

- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Full getting started guide
- **[SDF_RENDERING_GUIDE.md](SDF_RENDERING_GUIDE.md)** - Deep dive into SDF rendering
- **[SCRIPTING_GUIDE.md](SCRIPTING_GUIDE.md)** - Python scripting reference
- **[ANIMATION_GUIDE.md](ANIMATION_GUIDE.md)** - Animation system tutorial
- **[EDITOR_GUIDE.md](EDITOR_GUIDE.md)** - Using the Vehement editor

### Tutorials

- **[tutorials/first_entity.md](tutorials/first_entity.md)** - Create your first game entity
- **[tutorials/custom_ability.md](tutorials/custom_ability.md)** - Implement custom abilities
- **[tutorials/ai_behavior.md](tutorials/ai_behavior.md)** - AI with Python scripts

### Source Code Examples

Browse the `examples/` directory for additional reference:

- `RTSApplication.cpp` - Full RTS game implementation
- `StandaloneEditor.cpp` - Level editor source
- `RTGIExample.cpp` - Real-time global illumination demo
- `PathTracerDemo.cpp` - Path tracing renderer demo

---

*Nova3D Engine - High-Performance SDF Rendering with Real-Time Global Illumination*
