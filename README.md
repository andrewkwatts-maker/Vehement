//====== Project/folder/examplefile.txt ======//  
//\!copyright (c) 2025 Andrew Keith Watts. All rights reserved.  
//\!  
//\!This is the intellectual property of Andrew Keith Watts. Unauthorized  
//\!reproduction, distribution, or modification of this code, in whole or in part,  
//\!without the express written permission of Andrew Keith Watts is strictly prohibited.  
//\!  
//\!For inquiries, please contact AndrewKWatts@Gmail.com

# Nova3D Engine

A modern C++23 game engine built for real-time strategy and action games with support for Windows, macOS, Linux, Android, and iOS platforms.

## Features

- **Modern C++23**: Concepts, ranges, modules-ready architecture
- **Cross-Platform**: Windows, macOS, Linux, Android, iOS
- **Flexible Rendering**: OpenGL 4.6 / OpenGL ES 3.2 with Vulkan path (WIP)
- **Animation System**: Skeletal animation, blend trees, state machines
- **Spatial Indexing**: Octree, BVH, and spatial hashing for efficient queries
- **Python Scripting**: Embedded Python with hot-reload support
- **Reflection System**: Runtime type information and property binding
- **Networking**: Firebase integration for multiplayer with matchmaking
- **Editor**: ImGui-based editor with docking and custom panels
- **Data-Driven**: JSON configuration with schema validation

## Quick Start

### Prerequisites

- CMake 3.20+
- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022+)
- Python 3.10+ (for scripting support)

### Building

```bash
# Clone the repository
git clone https://github.com/your-org/Nova3D.git
cd Nova3D

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run the game
./build/bin/Vehement2
```

## AI Content Generation Tools

Nova3D includes powerful AI-powered tools for generating game content using Google Gemini.

### Quick Setup

```bash
# First-time setup (installs packages and configures API key)
first_time_setup.bat

# Or just configure your API key
tools\setup_ai_config.bat
```

### Generate a Character

```bash
# Interactive menu
ai_tools.bat

# Quick generate from command line
generate_hero.bat "dark sorceress with flowing robes" dreadlady
```

### What Gets Generated

A complete character includes:
- **SDF Model** - 25-50 primitives, AAA quality with visual feedback
- **Materials** - 6 PBR materials (skin, cloth, metal, etc.)
- **Animations** - 12 animations (idle, walk, run, attacks, spells, etc.)
- **Scripts** - 4 behavior scripts (movement, combat, abilities, AI)
- **VFX** - Particle effects and visual flourishes
- **Renders** - Icon and 6-view preview images

### Available Tools

| Command | Description |
|---------|-------------|
| `ai_tools.bat` | Interactive AI tools menu |
| `generate_hero.bat` | Quick character generation |
| `tools\aaa_generate.bat` | AAA quality model only |
| `tools\ai_level_design.bat` | Level design assistant |
| `tools\ai_visual_script.bat` | Script generator |
| `help.bat` | Full documentation |

### Minimal Example

```cpp
#include <engine/core/Engine.hpp>

int main() {
    auto& engine = Nova::Engine::Instance();

    if (!engine.Initialize()) {
        return -1;
    }

    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = []() {
        // Initialize game state
        return true;
    };

    callbacks.onUpdate = [](float dt) {
        // Update game logic
    };

    callbacks.onRender = []() {
        // Render game world
    };

    return engine.Run(std::move(callbacks));
}
```

## Documentation

Comprehensive documentation is available in the [docs/](docs/) directory:

### Getting Started
- [Documentation Index](docs/README.md) - Start here
- [Building Guide](docs/BUILDING.md) - Build instructions for all platforms
- [Architecture Overview](docs/ARCHITECTURE.md) - System design and structure

### Developer Guides
- [API Reference](docs/API_REFERENCE.md) - Core API overview
- [Editor Guide](docs/EDITOR_GUIDE.md) - Using the visual editor
- [Scripting Guide](docs/SCRIPTING_GUIDE.md) - Python scripting reference
- [Animation Guide](docs/ANIMATION_GUIDE.md) - Animation system usage
- [Networking Guide](docs/NETWORKING_GUIDE.md) - Multiplayer integration
- [Configuration Reference](docs/CONFIG_REFERENCE.md) - JSON schemas and config

### API Documentation
- [Engine API](docs/api/Engine.md) - Core engine systems
- [Animation API](docs/api/Animation.md) - Animation classes
- [Spatial API](docs/api/Spatial.md) - Spatial indexing
- [Reflection API](docs/api/Reflection.md) - Type registry and events
- [Scripting API](docs/api/Scripting.md) - Python integration
- [UI API](docs/api/UI.md) - Editor and ImGui
- [Network API](docs/api/Network.md) - Firebase and matchmaking

### Tutorials
- [Creating Your First Entity](docs/tutorials/first_entity.md)
- [Creating Custom Abilities](docs/tutorials/custom_ability.md)
- [Creating AI Behaviors](docs/tutorials/ai_behavior.md)
- [Creating Custom UI Panels](docs/tutorials/custom_ui.md)

### Contributing
- [Contribution Guide](docs/CONTRIBUTING.md) - How to contribute

## Project Structure

```
Nova3D/
├── engine/                 # Core engine code
│   ├── core/              # Engine, Window, Time, Input
│   ├── animation/         # Animation system
│   ├── graphics/          # Rendering and debug draw
│   ├── spatial/           # Spatial indexing (Octree, BVH)
│   ├── reflection/        # Type registry and events
│   └── scripting/         # Python integration
├── game/                   # Game-specific code
│   ├── src/               # Game source
│   │   ├── editor/        # Editor panels
│   │   ├── entities/      # Entity types
│   │   ├── network/       # Multiplayer
│   │   └── systems/       # Game systems
│   └── scripts/           # Python scripts
├── config/                 # Configuration files
│   └── schemas/           # JSON schemas
├── assets/                 # Game assets
├── docs/                   # Documentation
└── tests/                  # Unit tests
```

## Technologies

| Component | Technology |
|-----------|------------|
| Graphics | OpenGL 4.6 / ES 3.2 |
| Window | GLFW 3.4 |
| Math | GLM |
| JSON | nlohmann/json |
| UI | Dear ImGui |
| Scripting | Python 3 + pybind11 |
| Networking | Firebase |
| Testing | Google Test |
| Build | CMake |

## Platform Support

| Platform | Status | Graphics API |
|----------|--------|--------------|
| Windows | Supported | OpenGL 4.6 |
| macOS | Supported | OpenGL 4.1 |
| Linux | Supported | OpenGL 4.6 |
| Android | Supported | OpenGL ES 3.2 |
| iOS | Supported | OpenGL ES 3.0 |

## License

//====== Project/folder/examplefile.txt ======//  
//\!copyright (c) 2025 Andrew Keith Watts. All rights reserved.  
//\!  
//\!This is the intellectual property of Andrew Keith Watts. Unauthorized  
//\!reproduction, distribution, or modification of this code, in whole or in part,  
//\!without the express written permission of Andrew Keith Watts is strictly prohibited.  
//\!  
//\!For inquiries, please contact AndrewKWatts@Gmail.com

## Acknowledgments

- [GLFW](https://www.glfw.org/) - Window and input
- [GLM](https://glm.g-truc.net/) - Mathematics
- [Dear ImGui](https://github.com/ocornut/imgui) - UI framework
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing
- [pybind11](https://github.com/pybind/pybind11) - Python bindings
- [stb](https://github.com/nothings/stb) - Image loading
