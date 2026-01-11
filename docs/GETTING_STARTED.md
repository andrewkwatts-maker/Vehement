# Nova3D/Vehement Engine - Getting Started Guide

This guide will help you get up and running with the Nova3D engine quickly. We'll cover installation, basic setup, and creating your first application.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building the Engine](#building-the-engine)
3. [Project Structure](#project-structure)
4. [Your First Application](#your-first-application)
5. [Core Concepts](#core-concepts)
6. [Next Steps](#next-steps)

---

## Prerequisites

### Required Software

- **C++20 Compiler**: MSVC 2022, GCC 11+, or Clang 14+
- **CMake**: Version 3.20 or higher
- **Git**: For dependency management
- **Python 3.9+**: For scripting support (optional)

### Recommended

- **Vulkan SDK**: For RTX/raytracing features
- **CUDA Toolkit 11+**: For GPU acceleration features

### Dependencies (Auto-fetched by CMake)

The build system automatically downloads:
- GLM (math library)
- GLFW (windowing)
- Dear ImGui (editor UI)
- nlohmann/json (serialization)
- stb_image (texture loading)

---

## Building the Engine

### Quick Build (Windows)

```bash
# Clone the repository
git clone https://github.com/yourusername/Old3DEngine.git
cd Old3DEngine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release
```

### Quick Build (Linux/macOS)

```bash
git clone https://github.com/yourusername/Old3DEngine.git
cd Old3DEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `NOVA_BUILD_EDITOR` | ON | Build the Vehement editor |
| `NOVA_BUILD_EXAMPLES` | ON | Build example applications |
| `NOVA_ENABLE_RTX` | OFF | Enable RTX raytracing support |
| `NOVA_ENABLE_PYTHON` | ON | Enable Python scripting |
| `NOVA_USE_VCPKG` | OFF | Use vcpkg for dependencies |

Example with options:

```bash
cmake .. -DNOVA_ENABLE_RTX=ON -DNOVA_BUILD_EXAMPLES=ON
```

---

## Project Structure

```
Old3DEngine/
+-- engine/                 # Core engine code
|   +-- core/               # Engine, Window, Time, Logger
|   +-- graphics/           # Rendering, SDF, Shaders
|   +-- terrain/            # Terrain generation
|   +-- scene/              # Scene, Camera, Nodes
|   +-- animation/          # Animation system
|   +-- sdf/                # SDF primitives and models
|   +-- audio/              # Audio engine
|   +-- ai/                 # Navigation, AI
|   +-- events/             # Event binding system
|   +-- assets/             # Asset management
|   L-- editor/             # Editor-specific components
|
+-- game/                   # Game-specific code
|   L-- src/
|       +-- editor/         # Vehement editor
|       L-- systems/        # Game systems
|
+-- shaders/                # GLSL shaders
|   +-- sdf/                # SDF raymarching shaders
|   +-- gi/                 # Global illumination
|   L-- terrain/            # Terrain rendering
|
+-- assets/                 # Game assets
|   +-- models/             # 3D models
|   +-- textures/           # Textures
|   L-- configs/            # Configuration files
|
+-- docs/                   # Documentation
+-- examples/               # Example projects
L-- tools/                  # Build tools and utilities
```

---

## Your First Application

### Minimal Example

Create a new file `main.cpp`:

```cpp
#include <engine/core/Engine.hpp>
#include <engine/scene/Camera.hpp>
#include <engine/graphics/SDFRenderer.hpp>
#include <engine/sdf/SDFModel.hpp>

int main() {
    // Get engine singleton
    auto& engine = Nova::Engine::Instance();

    // Configure initialization
    Nova::Engine::InitParams params;
    params.configPath = "config/engine.json";
    params.enableImGui = true;

    if (!engine.Initialize(params)) {
        return -1;
    }

    // Create camera
    Nova::Camera camera;
    camera.SetPerspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    camera.LookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0));

    // Create SDF renderer
    Nova::SDFRenderer sdfRenderer;
    sdfRenderer.Initialize();

    // Create a simple SDF model (sphere)
    Nova::SDFModel model("MySphere");
    auto* sphere = model.CreatePrimitive("Sphere", Nova::SDFPrimitiveType::Sphere);
    sphere->GetParameters().radius = 1.0f;
    sphere->GetMaterial().baseColor = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);

    // Application callbacks
    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = []() {
        std::cout << "Application started!" << std::endl;
        return true;
    };

    callbacks.onUpdate = [&](float deltaTime) {
        // Rotate camera around the model
        static float angle = 0.0f;
        angle += deltaTime * 0.5f;
        float radius = 5.0f;
        glm::vec3 pos(sin(angle) * radius, 2.0f, cos(angle) * radius);
        camera.LookAt(pos, glm::vec3(0, 0, 0));
    };

    callbacks.onRender = [&]() {
        // Clear and set camera
        engine.GetRenderer().Clear();
        engine.GetRenderer().SetCamera(&camera);

        // Render SDF model
        sdfRenderer.Render(model, camera);
    };

    callbacks.onShutdown = [&]() {
        sdfRenderer.Shutdown();
    };

    return engine.Run(std::move(callbacks));
}
```

### CMakeLists.txt for Your Project

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyNovaApp)

# Find Nova3D
find_package(Nova3D REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Nova3D::nova3d)
```

---

## Core Concepts

### 1. The Engine Singleton

The `Engine` class is the heart of Nova3D. It manages all subsystems:

```cpp
auto& engine = Nova::Engine::Instance();

// Access subsystems
auto& window   = engine.GetWindow();    // Window management
auto& time     = engine.GetTime();      // Timing
auto& renderer = engine.GetRenderer();  // Rendering
auto& input    = engine.GetInput();     // Input handling
```

### 2. Signed Distance Fields (SDF)

Nova3D uses SDF-based rendering for high-quality visuals. SDFs represent geometry as distance functions:

```
         +---------------------------------------+
         |                                       |
         |    SDF Value > 0 (Outside Surface)   |
         |                                       |
         |       +-------------------+           |
         |       |                   |           |
         |       |  SDF Value < 0    |           |
         |       |  (Inside Surface) |           |
         |       |                   |           |
         |       +-------------------+           |
         |              ^                        |
         |              | SDF = 0 (Surface)      |
         +---------------------------------------+
```

**Primitive Types:**
- Sphere, Box, Cylinder, Capsule, Cone
- Torus, Plane, Ellipsoid, Pyramid, Prism
- RoundedBox (box with rounded corners)

**CSG Operations:**
```cpp
// Combine shapes
primitive->SetCSGOperation(Nova::CSGOperation::Union);

// Carve out shapes
primitive->SetCSGOperation(Nova::CSGOperation::Subtraction);

// Keep intersection
primitive->SetCSGOperation(Nova::CSGOperation::Intersection);

// Smooth blending variants
primitive->SetCSGOperation(Nova::CSGOperation::SmoothUnion);
```

### 3. Global Illumination

The RadianceCascade system provides real-time global illumination:

```cpp
Nova::RadianceCascade::Config giConfig;
giConfig.numCascades = 4;
giConfig.baseResolution = 32;
giConfig.raysPerProbe = 64;
giConfig.bounces = 2;

Nova::RadianceCascade gi;
gi.Initialize(giConfig);

// In your update loop
gi.Update(camera.GetPosition(), deltaTime);
gi.PropagateLighting();

// Connect to renderer
sdfRenderer.SetRadianceCascade(std::make_shared<Nova::RadianceCascade>(gi));
sdfRenderer.SetGlobalIlluminationEnabled(true);
```

### 4. Scene Hierarchy

Build complex scenes using parent-child relationships:

```cpp
// Create model with hierarchy
Nova::SDFModel character("Robot");

// Body (root)
auto* body = character.CreatePrimitive("Body", Nova::SDFPrimitiveType::Capsule);
body->GetParameters().height = 2.0f;
body->GetParameters().bottomRadius = 0.5f;

// Head (child of body)
auto* head = character.CreatePrimitive("Head", Nova::SDFPrimitiveType::Sphere, body);
head->GetLocalTransform().position = glm::vec3(0, 1.5f, 0);
head->GetParameters().radius = 0.4f;

// Arms (children of body)
auto* leftArm = character.CreatePrimitive("LeftArm", Nova::SDFPrimitiveType::Capsule, body);
leftArm->GetLocalTransform().position = glm::vec3(-0.7f, 0.5f, 0);
leftArm->GetLocalTransform().rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0,0,1));
```

### 5. Camera System

```cpp
Nova::Camera camera;

// Perspective projection
camera.SetPerspective(
    60.0f,    // FOV in degrees
    16.0f/9.0f, // Aspect ratio
    0.1f,     // Near plane
    1000.0f   // Far plane
);

// Position and orientation
camera.LookAt(
    glm::vec3(0, 10, 20),  // Camera position
    glm::vec3(0, 0, 0),    // Look-at target
    glm::vec3(0, 1, 0)     // Up vector
);

// Get matrices for shaders
glm::mat4 view = camera.GetView();
glm::mat4 proj = camera.GetProjection();
glm::mat4 viewProj = camera.GetProjectionView();

// Screen-to-world ray (for picking)
glm::vec3 rayDir = camera.ScreenToWorldRay(mousePos, screenSize);
```

---

## Rendering Pipeline Overview

```
+-------------------+     +------------------+     +-------------------+
|   SDF Evaluation  | --> |   Raymarching    | --> |    Shading        |
|   (GPU Compute)   |     |   (Fragment)     |     |    (PBR + GI)     |
+-------------------+     +------------------+     +-------------------+
                                 |
                                 v
                    +------------------------+
                    |   Radiance Cascades    |
                    |   (Global Illumination)|
                    +------------------------+
                                 |
                                 v
                    +------------------------+
                    |   Post Processing      |
                    |   (Tone mapping, etc.) |
                    +------------------------+
```

---

## Next Steps

### Explore More Documentation

- **[API_REFERENCE.md](API_REFERENCE.md)** - Complete API documentation
- **[SDF_RENDERING_GUIDE.md](SDF_RENDERING_GUIDE.md)** - Deep dive into SDF rendering
- **[ANIMATION_GUIDE.md](ANIMATION_GUIDE.md)** - Animation system tutorial
- **[EDITOR_GUIDE.md](EDITOR_GUIDE.md)** - Using the Vehement editor

### Example Projects

Check the `examples/` directory for:
- `BasicSDF/` - Simple SDF rendering
- `TerrainDemo/` - Procedural terrain with GI
- `CharacterEditor/` - SDF character creation
- `RealTimeGI/` - Advanced lighting demo

### Getting Help

- Check `docs/` for detailed documentation
- Review example code in `examples/`
- Read inline code comments in header files
- Engine header files contain extensive documentation

---

## Quick Reference

### Common Includes

```cpp
// Core engine
#include <engine/core/Engine.hpp>
#include <engine/core/Window.hpp>
#include <engine/core/Time.hpp>

// Graphics
#include <engine/graphics/Renderer.hpp>
#include <engine/graphics/SDFRenderer.hpp>
#include <engine/graphics/RadianceCascade.hpp>

// SDF
#include <engine/sdf/SDFModel.hpp>
#include <engine/sdf/SDFPrimitive.hpp>

// Scene
#include <engine/scene/Camera.hpp>
#include <engine/scene/Scene.hpp>

// Terrain
#include <engine/terrain/TerrainGenerator.hpp>
#include <engine/terrain/NoiseGenerator.hpp>
```

### Common Namespaces

```cpp
using namespace Nova;           // Engine core
using namespace Nova::Events;   // Event system
using namespace Vehement;       // Game/Editor
using namespace Engine;         // Spectral rendering
```

### Keyboard Shortcuts (Editor)

| Key | Action |
|-----|--------|
| W/A/S/D | Camera movement |
| Q/E | Camera up/down |
| Right-click + drag | Camera rotation |
| G | Translate gizmo |
| R | Rotate gizmo |
| S | Scale gizmo |
| Delete | Delete selected |
| Ctrl+D | Duplicate selected |
| Ctrl+S | Save |
| Ctrl+Z | Undo |

---

*Nova3D Engine - High-Performance SDF Rendering with Real-Time Global Illumination*
