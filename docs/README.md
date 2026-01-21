# Nova3D Engine Documentation

Welcome to the Nova3D Engine documentation. This guide will help you navigate the extensive documentation for our modern C++23 game engine featuring state-of-the-art SDF rendering and real-time global illumination.

## Quick Navigation

| I want to... | Go to... |
|-------------|----------|
| Build and run the engine | [Getting Started](#getting-started) |
| Learn the engine architecture | [Engine Architecture](#engine-architecture) |
| Use AI-powered tools | [AI Tools Guide](#ai-tools-guide) |
| Look up API details | [API Reference](#api-reference) |
| Fix build problems | [Troubleshooting](#troubleshooting) |
| Follow a tutorial | [Tutorials](#tutorials) |

---

## Getting Started

New to Nova3D? Start here to get up and running quickly.

### Essential Reading

| Document | Description |
|----------|-------------|
| [GETTING_STARTED.md](GETTING_STARTED.md) | Prerequisites, installation, and your first application |
| [BUILDING.md](BUILDING.md) | Complete build instructions for all platforms |
| [EXAMPLES.md](EXAMPLES.md) | Runnable demos and code examples |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Solutions for common build and runtime issues |

### Quick Start (5 Minutes)

**Prerequisites:**
- CMake 3.20+
- C++23 compiler (MSVC 2022+, GCC 13+, Clang 16+)
- Python 3.10+ (optional, for scripting)

**Build:**
```bash
git clone https://github.com/your-org/Nova3D.git
cd Nova3D
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/nova_demo
```

**Minimal Code:**
```cpp
#include <engine/core/Engine.hpp>

int main() {
    auto& engine = Nova::Engine::Instance();
    if (!engine.Initialize()) return -1;

    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onUpdate = [](float dt) { /* game logic */ };
    callbacks.onRender = []() { /* rendering */ };

    return engine.Run(std::move(callbacks));
}
```

### Platform Support

| Platform | Status | Graphics API |
|----------|--------|--------------|
| Windows | Supported | OpenGL 4.6 |
| macOS | Supported | OpenGL 4.1 |
| Linux | Supported | OpenGL 4.6 |
| Android | Supported | OpenGL ES 3.2 |
| iOS | Supported | OpenGL ES 3.0 |

---

## Engine Architecture

Understand how Nova3D is structured and how its systems work together.

### Core Architecture

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | High-level system design, module structure, threading model |
| [CONFIG_REFERENCE.md](CONFIG_REFERENCE.md) | JSON configuration file reference |

### Rendering System

| Document | Description |
|----------|-------------|
| [SDF_RENDERING_GUIDE.md](SDF_RENDERING_GUIDE.md) | Complete SDF rendering guide with primitives, CSG, and raymarching |
| [SDF_SYSTEM_DOCUMENTATION.md](SDF_SYSTEM_DOCUMENTATION.md) | SDF system internals |
| [RTGI_Integration_Guide.md](RTGI_Integration_Guide.md) | Real-time global illumination integration |
| [RTGI_README.md](RTGI_README.md) | RadianceCascade GI system overview |
| [TERRAIN_SDF_GI.md](TERRAIN_SDF_GI.md) | Terrain rendering with SDF and GI |
| [LIGHTING_IMPLEMENTATION.md](LIGHTING_IMPLEMENTATION.md) | Lighting system details |

### Game Systems

| Document | Description |
|----------|-------------|
| [ANIMATION_GUIDE.md](ANIMATION_GUIDE.md) | Skeletal animation, blend trees, state machines |
| [NETWORKING_GUIDE.md](NETWORKING_GUIDE.md) | Multiplayer networking and Firebase integration |
| [SCRIPTING_GUIDE.md](SCRIPTING_GUIDE.md) | Python scripting system |

### Editor

| Document | Description |
|----------|-------------|
| [EDITOR_GUIDE.md](EDITOR_GUIDE.md) | Complete editor user guide |
| [THUMBNAIL_SYSTEM.md](THUMBNAIL_SYSTEM.md) | Asset thumbnail generation |
| [ASSET_ICON_SYSTEM.md](ASSET_ICON_SYSTEM.md) | Icon generation pipeline |

### Instance and Archetype Systems

| Document | Description |
|----------|-------------|
| [ArchetypeSystemGuide.md](ArchetypeSystemGuide.md) | Entity archetype system |
| [InstanceSystem_QuickStart.md](InstanceSystem_QuickStart.md) | Quick start for instance system |
| [InstanceSystem_Architecture.md](InstanceSystem_Architecture.md) | Instance system architecture |
| [InstancePropertySystem.md](InstancePropertySystem.md) | Property overrides for instances |

---

## AI Tools Guide

Nova3D integrates AI-powered tools for asset creation, level design, and visual scripting.

### Getting Started with AI Tools

| Document | Description |
|----------|-------------|
| [AI_TOOLS_ROLLOUT_PLAN.md](AI_TOOLS_ROLLOUT_PLAN.md) | Overview of all AI tools and configuration |
| [AI_EDITOR_TOOLS_PLAN.md](AI_EDITOR_TOOLS_PLAN.md) | Complete list of AI batch file tools |
| [AI_INTEGRATION_IMPLEMENTATION_PLAN.md](AI_INTEGRATION_IMPLEMENTATION_PLAN.md) | AI integration roadmap |

### Configuration

AI tools require a Gemini API key. Configure via:

**Config File** (`%APPDATA%\Nova3D\gemini_config.json` on Windows):
```json
{
  "gemini_api_key": "YOUR_API_KEY",
  "model_thinking": "gemini-2.0-flash-thinking-exp",
  "model_fast": "gemini-2.0-flash",
  "max_refinements": 5,
  "quality_threshold": 0.85
}
```

**Or Environment Variable:**
```bash
set GEMINI_API_KEY=YOUR_API_KEY
```

### Available AI Tools

| Tool | Purpose | Status |
|------|---------|--------|
| `concept_to_sdf.bat` | Generate SDF models from concepts | Complete |
| `ai_asset_helper.bat` | Analyze and optimize assets | Complete |
| `ai_level_design.bat` | AI-assisted level layout | Complete |
| `ai_visual_script.bat` | Generate visual scripts | Complete |
| `aaa_generate.bat` | AAA-quality SDF generation | Complete |
| `generate_character.bat` | Full character pipeline | Complete |
| `validate_and_report.bat` | Asset validation | Complete |

### Usage Example

```batch
cd tools
setup_ai_config.bat
concept_to_sdf.bat --prompt "heroic knight in shining armor" --name "knight"
```

Output structure:
```
generated_assets/knight/
  svgs/           # Orthographic SVG views
  pngs/           # Converted PNGs
  previews/       # Iteration previews
  final/          # Final icon and video
  knight_sdf.json # SDF model file
```

---

## API Reference

Detailed API documentation for all engine systems.

### Core API

| Document | Description |
|----------|-------------|
| [API_REFERENCE.md](API_REFERENCE.md) | Comprehensive API reference |
| [api/Engine.md](api/Engine.md) | Engine, Window, Time, Input, Logger |
| [api/Animation.md](api/Animation.md) | Animation system API |
| [api/Network.md](api/Network.md) | Networking API |
| [api/Reflection.md](api/Reflection.md) | Type registry and reflection |
| [api/Scripting.md](api/Scripting.md) | Python scripting API |
| [api/Spatial.md](api/Spatial.md) | Spatial indexing (Octree, BVH) |
| [api/UI.md](api/UI.md) | ImGui integration |

### Key Classes

| Class | Header | Description |
|-------|--------|-------------|
| `Engine` | `engine/core/Engine.hpp` | Main engine singleton |
| `SDFRenderer` | `engine/graphics/SDFRenderer.hpp` | SDF raymarching renderer |
| `RadianceCascade` | `engine/graphics/RadianceCascade.hpp` | Global illumination |
| `SDFModel` | `engine/sdf/SDFModel.hpp` | SDF model container |
| `TerrainGenerator` | `engine/terrain/TerrainGenerator.hpp` | Procedural terrain |
| `Camera` | `engine/scene/Camera.hpp` | Camera management |

### Common Includes

```cpp
// Core
#include <engine/core/Engine.hpp>
#include <engine/core/Window.hpp>
#include <engine/core/Time.hpp>

// Graphics
#include <engine/graphics/SDFRenderer.hpp>
#include <engine/graphics/RadianceCascade.hpp>

// SDF
#include <engine/sdf/SDFModel.hpp>
#include <engine/sdf/SDFPrimitive.hpp>

// Scene
#include <engine/scene/Camera.hpp>
```

---

## Tutorials

Step-by-step guides for common tasks.

| Tutorial | Description |
|----------|-------------|
| [tutorials/first_entity.md](tutorials/first_entity.md) | Create your first game entity |
| [tutorials/custom_ability.md](tutorials/custom_ability.md) | Implement a custom ability |
| [tutorials/ai_behavior.md](tutorials/ai_behavior.md) | Create AI behavior with Python |
| [tutorials/custom_ui.md](tutorials/custom_ui.md) | Build custom UI panels |

---

## Troubleshooting

Solutions for common issues.

### Build Issues

#### nlohmann/json C++20 Ranges Conflict (MSVC)

**Symptoms:** 100+ template compilation errors in `<algorithm>` header involving `std::ranges`.

**Cause:** nlohmann/json v3.11.x creates `nlohmann::std::ranges` namespace which pollutes MSVC's `std::ranges`.

**Status:** Known issue, work in progress.

**Attempted Fixes:**
- `JSON_HAS_CPP_20=0 JSON_HAS_RANGES=0` compile definitions (set too late in build)
- Setting defines before FetchContent
- Downgrading to v3.10.5 or v3.11.2

**Workarounds to Try:**
1. Use single-include json.hpp with manual defines before include
2. Fork and patch nlohmann/json to disable ranges support
3. Use alternative JSON library (RapidJSON, simdjson)
4. Update MSVC to latest version
5. Force-include a header with JSON defines before all compilation

See [CLAUDE.md](../CLAUDE.md) for detailed build blocker documentation.

#### Missing Headers

**Symptom:** `error: 'string' is not a member of 'std'`

**Solution:** Add `#include <string>` to the affected header.

Affected files that have been fixed:
- `engine/graphics/Renderer.hpp`
- `engine/graphics/LODManager.hpp`
- `engine/graphics/Batching.hpp`

#### GLM Constants Not Found

**Symptom:** Undefined reference to GLM constants like `glm::pi`.

**Solution:** Add `#include <glm/gtc/constants.hpp>` to the source file.

### Runtime Issues

#### Python Not Found

```bash
# Specify Python path explicitly
cmake -B build -DPython3_EXECUTABLE=/usr/bin/python3
```

#### OpenGL Headers Not Found (Linux)

```bash
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
```

#### GLFW Build Fails (Linux)

```bash
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### Audio System

Audio is disabled by default. To enable:

```bash
cmake -DNOVA_ENABLE_AUDIO=ON ..
```

When disabled, a stub implementation provides the same API but logs warnings when called.

---

## Contributing

| Document | Description |
|----------|-------------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Code style, PR process, testing requirements |

### Quick Guidelines

- **Code Style:** PascalCase for classes/methods, camelCase for variables, m_ prefix for members
- **C++ Standard:** C++23 with modern features (concepts, ranges, coroutines)
- **Philosophy:** Always fix C++ issues properly; never create Python workarounds for C++ bugs
- **Documentation:** Doxygen comments for public APIs

---

## Project Status

| Document | Description |
|----------|-------------|
| [CURRENT_PROJECT_STATE.md](CURRENT_PROJECT_STATE.md) | Current project status |
| [CURRENT_STATUS.md](CURRENT_STATUS.md) | Implementation status |
| [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) | Feature completion tracking |
| [PRODUCT_BACKLOG.md](PRODUCT_BACKLOG.md) | Product backlog |

---

## Research and Technical Notes

These documents contain research, implementation details, and technical deep-dives.

### SDF Research

| Document | Description |
|----------|-------------|
| [SDF_RESEARCH_CONSOLIDATION.md](SDF_RESEARCH_CONSOLIDATION.md) | SDF research summary |
| [SDF_IMPROVEMENTS_RESEARCH.md](SDF_IMPROVEMENTS_RESEARCH.md) | SDF improvement research |
| [SDF_PAINTING_WITH_MATHS.md](SDF_PAINTING_WITH_MATHS.md) | Mathematical SDF techniques |
| [SDF_ACCELERATION_IMPLEMENTATION.md](SDF_ACCELERATION_IMPLEMENTATION.md) | BVH acceleration |
| [SDF_IMPLEMENTATION_SUMMARY.md](SDF_IMPLEMENTATION_SUMMARY.md) | Implementation summary |
| [SDF_RADIANCE_CASCADE_INTEGRATION.md](SDF_RADIANCE_CASCADE_INTEGRATION.md) | GI integration details |

### RTX and Hardware Acceleration

| Document | Description |
|----------|-------------|
| [RTX_IMPLEMENTATION_GUIDE.md](RTX_IMPLEMENTATION_GUIDE.md) | RTX hardware ray tracing |
| [RTX_QUICK_START.md](RTX_QUICK_START.md) | RTX quick start |

### Session and Implementation Notes

| Document | Description |
|----------|-------------|
| [GI_INTEGRATION_COMPLETE.md](GI_INTEGRATION_COMPLETE.md) | GI integration completion notes |
| [SESSION_SUMMARY_GI_IMPLEMENTATION.md](SESSION_SUMMARY_GI_IMPLEMENTATION.md) | GI implementation session |
| [IMPLEMENTATION_COMPLETE_SUMMARY.md](IMPLEMENTATION_COMPLETE_SUMMARY.md) | Implementation completion summary |

---

## Document Index

### By Category

**Core Documentation**
- [README.md](README.md) - This file
- [GETTING_STARTED.md](GETTING_STARTED.md)
- [BUILDING.md](BUILDING.md)
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- [ARCHITECTURE.md](ARCHITECTURE.md)
- [API_REFERENCE.md](API_REFERENCE.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)

**Guides**
- [EDITOR_GUIDE.md](EDITOR_GUIDE.md)
- [ANIMATION_GUIDE.md](ANIMATION_GUIDE.md)
- [NETWORKING_GUIDE.md](NETWORKING_GUIDE.md)
- [SCRIPTING_GUIDE.md](SCRIPTING_GUIDE.md)
- [SDF_RENDERING_GUIDE.md](SDF_RENDERING_GUIDE.md)
- [CONFIG_REFERENCE.md](CONFIG_REFERENCE.md)

**AI Tools**
- [AI_TOOLS_ROLLOUT_PLAN.md](AI_TOOLS_ROLLOUT_PLAN.md)
- [AI_EDITOR_TOOLS_PLAN.md](AI_EDITOR_TOOLS_PLAN.md)
- [AI_INTEGRATION_IMPLEMENTATION_PLAN.md](AI_INTEGRATION_IMPLEMENTATION_PLAN.md)
- [AI_Decision_Tree_Report.md](AI_Decision_Tree_Report.md)

**Tutorials**
- [tutorials/first_entity.md](tutorials/first_entity.md)
- [tutorials/custom_ability.md](tutorials/custom_ability.md)
- [tutorials/ai_behavior.md](tutorials/ai_behavior.md)
- [tutorials/custom_ui.md](tutorials/custom_ui.md)

**API Reference**
- [api/Engine.md](api/Engine.md)
- [api/Animation.md](api/Animation.md)
- [api/Network.md](api/Network.md)
- [api/Reflection.md](api/Reflection.md)
- [api/Scripting.md](api/Scripting.md)
- [api/Spatial.md](api/Spatial.md)
- [api/UI.md](api/UI.md)

**Instance System**
- [ArchetypeSystemGuide.md](ArchetypeSystemGuide.md)
- [InstanceSystem_QuickStart.md](InstanceSystem_QuickStart.md)
- [InstanceSystem_Architecture.md](InstanceSystem_Architecture.md)
- [InstancePropertySystem.md](InstancePropertySystem.md)
- [InstanceSystem_FileList.md](InstanceSystem_FileList.md)

**Rendering**
- [SDF_SYSTEM_DOCUMENTATION.md](SDF_SYSTEM_DOCUMENTATION.md)
- [RTGI_Integration_Guide.md](RTGI_Integration_Guide.md)
- [RTGI_README.md](RTGI_README.md)
- [TERRAIN_SDF_GI.md](TERRAIN_SDF_GI.md)
- [LIGHTING_IMPLEMENTATION.md](LIGHTING_IMPLEMENTATION.md)
- [RENDERING_PIPELINE_FIX_PLAN.md](RENDERING_PIPELINE_FIX_PLAN.md)
- [ENGINE_RENDERING_FIX_PLAN.md](ENGINE_RENDERING_FIX_PLAN.md)
- [SPHERICAL_WORLD_ARCHITECTURE.md](SPHERICAL_WORLD_ARCHITECTURE.md)

**Editor**
- [THUMBNAIL_SYSTEM.md](THUMBNAIL_SYSTEM.md)
- [ASSET_ICON_SYSTEM.md](ASSET_ICON_SYSTEM.md)
- [EDITOR_INTEGRATION_PLAN.md](EDITOR_INTEGRATION_PLAN.md)
- [EDITOR_MENU_POLISH_SUMMARY.md](EDITOR_MENU_POLISH_SUMMARY.md)
- [EDITOR_PCG_SYSTEM_DESIGN.md](EDITOR_PCG_SYSTEM_DESIGN.md)
- [EDITOR_UI_STATUS_AND_NEXT_STEPS.md](EDITOR_UI_STATUS_AND_NEXT_STEPS.md)
- [BuildingPlacementControls.md](BuildingPlacementControls.md)

**Project Planning**
- [CURRENT_PROJECT_STATE.md](CURRENT_PROJECT_STATE.md)
- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
- [PRODUCT_BACKLOG.md](PRODUCT_BACKLOG.md)
- [PROJECT_RESTRUCTURING_PLAN.md](PROJECT_RESTRUCTURING_PLAN.md)
- [AAA_QUALITY_PLAN.md](AAA_QUALITY_PLAN.md)
- [ASSET_POLISH_PLAN.md](ASSET_POLISH_PLAN.md)
- [SPRINT_001.md](SPRINT_001.md)
- [SPRINT_BACKLOG_SDF_IMPROVEMENTS.md](SPRINT_BACKLOG_SDF_IMPROVEMENTS.md)

**Technical Research**
- [SDF_RESEARCH_CONSOLIDATION.md](SDF_RESEARCH_CONSOLIDATION.md)
- [SDF_IMPROVEMENTS_RESEARCH.md](SDF_IMPROVEMENTS_RESEARCH.md)
- [SDF_PAINTING_WITH_MATHS.md](SDF_PAINTING_WITH_MATHS.md)
- [SDF_ACCELERATION_IMPLEMENTATION.md](SDF_ACCELERATION_IMPLEMENTATION.md)
- [RTX_IMPLEMENTATION_GUIDE.md](RTX_IMPLEMENTATION_GUIDE.md)
- [RTX_QUICK_START.md](RTX_QUICK_START.md)

**Miscellaneous**
- [EXAMPLES.md](EXAMPLES.md)
- [TODO_COMPLETION_REPORT.md](TODO_COMPLETION_REPORT.md)
- [FULL_ASSET_PIPELINE.md](FULL_ASSET_PIPELINE.md)
- [RACE_ASSET_CREATION_SUMMARY.md](RACE_ASSET_CREATION_SUMMARY.md)
- [PCG_SYSTEM_SUMMARY.md](PCG_SYSTEM_SUMMARY.md)
- [FREE_GEOSPATIAL_DATA_SOURCES.md](FREE_GEOSPATIAL_DATA_SOURCES.md)

---

## Performance Highlights

- **285 FPS** global illumination @ 1080p (target was 120 FPS)
- **3.5ms** frame time (target was 8.3ms)
- **20x** SDF acceleration over naive raymarching
- **3.6x** RTX speedup over compute shaders (optional)

---

## Technology Stack

| Component | Technology |
|-----------|------------|
| Graphics | OpenGL 4.6 / ES 3.2 |
| Window | GLFW 3.4 |
| Math | GLM 1.0.1 |
| UI | Dear ImGui 1.91.5 (docking) |
| JSON | nlohmann/json 3.11.3 |
| Scripting | Python 3 + pybind11 |
| Audio | OpenAL-soft (optional) |

---

*Nova3D Engine v1.0.0 - High-Performance SDF Rendering with Real-Time Global Illumination*

*Last Updated: 2026-01-21*
