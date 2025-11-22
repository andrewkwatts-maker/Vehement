# Nova3D Engine Documentation

Welcome to the Nova3D Engine documentation. Nova3D is a modern C++23 game engine designed for building 3D applications, games, and interactive experiences across multiple platforms.

## Quick Start

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.20 or higher
- Python 3.8+ (for scripting support)
- Platform-specific SDKs (see [Building](BUILDING.md))

### Basic Setup

```bash
# Clone the repository
git clone https://github.com/your-org/Nova3DEngine.git
cd Nova3DEngine

# Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run the demo
./build/bin/nova_demo
```

### Hello World Example

```cpp
#include <engine/core/Engine.hpp>

int main() {
    auto& engine = Nova::Engine::Instance();

    if (!engine.Initialize()) {
        return -1;
    }

    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onUpdate = [](float dt) {
        // Your game logic here
    };
    callbacks.onRender = []() {
        // Your rendering here
    };

    return engine.Run(std::move(callbacks));
}
```

## Documentation Index

### Getting Started

| Document | Description |
|----------|-------------|
| [Building](BUILDING.md) | Complete build instructions for all platforms |
| [Architecture](ARCHITECTURE.md) | High-level system architecture overview |
| [Contributing](CONTRIBUTING.md) | Contribution guidelines and code style |

### User Guides

| Document | Description |
|----------|-------------|
| [Editor Guide](EDITOR_GUIDE.md) | Using the integrated level editor |
| [Scripting Guide](SCRIPTING_GUIDE.md) | Python scripting API and examples |
| [Animation Guide](ANIMATION_GUIDE.md) | Animation system and state machines |
| [Networking Guide](NETWORKING_GUIDE.md) | Multiplayer and Firebase integration |
| [Config Reference](CONFIG_REFERENCE.md) | JSON configuration formats and schemas |

### API Reference

| Document | Description |
|----------|-------------|
| [API Overview](API_REFERENCE.md) | Complete API reference overview |
| [Engine API](api/Engine.md) | Core engine systems |
| [Animation API](api/Animation.md) | Animation and skeletal systems |
| [Spatial API](api/Spatial.md) | Spatial indexing and queries |
| [Reflection API](api/Reflection.md) | Runtime type information |
| [Scripting API](api/Scripting.md) | Python scripting integration |
| [UI API](api/UI.md) | User interface system |
| [Network API](api/Network.md) | Networking and multiplayer |

### Tutorials

| Tutorial | Description |
|----------|-------------|
| [First Entity](tutorials/first_entity.md) | Creating your first game entity |
| [Custom Ability](tutorials/custom_ability.md) | Building a spell or ability |
| [AI Behavior](tutorials/ai_behavior.md) | Creating AI with Python |
| [Custom UI](tutorials/custom_ui.md) | Building UI panels |

## Feature Highlights

### Modern C++23 Engine Core

- **RAII Resource Management**: Automatic cleanup with smart pointers
- **Type-Safe APIs**: Extensive use of `[[nodiscard]]`, `noexcept`, and concepts
- **Compile-Time Features**: `constexpr` and `consteval` for performance
- **Modern Threading**: Job system with work stealing

### Cross-Platform Support

- **Desktop**: Windows, macOS, Linux
- **Mobile**: Android (OpenGL ES/Vulkan), iOS (Metal)
- **Web**: Emscripten/WebAssembly (planned)

### Advanced Graphics

- **OpenGL 4.6 Core** with GLAD loader
- **Optimized Rendering**: Frustum culling, LOD, batching
- **PBR Materials**: Physically-based rendering pipeline
- **Particle Systems**: CPU and GPU-accelerated particles

### Game Systems

- **Animation**: Skeletal animation, blend trees, state machines
- **Physics**: Collision detection, triggers, physics world
- **Pathfinding**: A* with customizable heuristics
- **Terrain**: Procedural terrain with noise generation

### Data-Driven Design

- **JSON Configuration**: Define entities, abilities, and behaviors in JSON
- **Runtime Reflection**: Type information and property editing
- **Event System**: Pub/sub event bus with priority handling
- **Python Scripting**: Hot-reloadable game logic

### Editor Tools

- **ImGui-Based Editor**: Dockable panels, customizable layout
- **Content Browser**: Asset management and thumbnails
- **Config Editor**: Visual JSON editing with schema validation
- **Script Editor**: Python editing with syntax highlighting

## Project Structure

```
Nova3DEngine/
├── engine/                 # Core engine code
│   ├── animation/         # Animation system
│   ├── config/            # Configuration loading
│   ├── core/              # Engine core (Window, Time, etc.)
│   ├── events/            # Event binding system
│   ├── graphics/          # Rendering, shaders, materials
│   ├── input/             # Input handling
│   ├── math/              # Math utilities
│   ├── particles/         # Particle systems
│   ├── pathfinding/       # A* and navigation
│   ├── physics/           # Physics simulation
│   ├── platform/          # Platform abstractions
│   ├── reflection/        # Runtime type info
│   ├── scene/             # Scene graph, cameras
│   ├── scripting/         # Python integration
│   ├── spatial/           # Spatial indexing
│   ├── terrain/           # Terrain generation
│   └── utils/             # Utility functions
├── game/                   # Game-specific code (Vehement2)
│   ├── assets/            # Game assets and schemas
│   └── src/               # Game source code
├── examples/               # Example applications
├── config/                 # Configuration files
├── assets/                 # Shared assets
├── docs/                   # Documentation
└── CMakeLists.txt         # Main build configuration
```

## Version Information

- **Engine Version**: 1.0.0
- **C++ Standard**: C++23
- **Minimum CMake**: 3.20

## External Dependencies

Nova3D uses the following open-source libraries (automatically fetched via CMake):

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.4 | Window and input |
| GLM | 1.0.1 | Mathematics |
| GLAD | 2.0.6 | OpenGL loader |
| Assimp | 5.4.3 | Model loading |
| stb | latest | Image loading |
| nlohmann/json | 3.11.3 | JSON parsing |
| spdlog | 1.14.1 | Logging |
| ImGui | 1.91.5 | Debug UI |
| FastNoise2 | 0.10.0 | Noise generation |
| pybind11 | 2.12.0 | Python bindings |

## Support

- **Issues**: Report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions
- **Wiki**: Additional documentation on the project wiki

## License

Nova3D Engine is released under the MIT License. See LICENSE for details.
