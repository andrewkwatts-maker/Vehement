# Nova3D Project Restructuring Plan

## Executive Summary

This document outlines the comprehensive restructuring plan for the Nova3D engine project. Analysis revealed significant organizational issues:

- **Build System Completeness**: Only 17% of engine files and <2% of game files are in CMakeLists.txt
- **412 orphaned engine files** not being compiled
- **656+ orphaned game files** not being compiled
- **112 markdown documentation files** with extensive duplication
- **20+ files over 1300 lines** requiring splitting
- **Code duplication**: Race implementations (~9800 lines of duplicated logic)
- **Missing abstractions**: No IPlatform, IRenderer, ITerrain interfaces

## Priority Levels

### 🔴 IMMEDIATE (Complete within 1 day)

#### 1. Remove Build Artifacts and Backups
**Files to delete** (11 backup files):
- `engine/physics/PhysicsEngine.hpp.bak`
- `engine/networking/NetworkManager.cpp.bak`
- `engine/networking/NetworkManager.hpp.bak`
- `engine/persistence/SaveGameManager.cpp.bak`
- `engine/persistence/SaveGameManager.hpp.bak`
- `engine/ui/ImGuiRenderer.cpp.bak`
- `engine/ui/ImGuiRenderer.hpp.bak`
- `game/logic/GameLogic.cpp.bak`
- `game/logic/GameLogic.hpp.bak`
- `game/ui/GameUI.cpp.bak`
- `game/ui/GameUI.hpp.bak`

**One-time scripts to remove** (6-10 files):
- `tools/integrate_gi.py` ✅ (already removed)
- `tools/integrate_gi_cpp.py` ✅ (already removed)
- `tools/fix_glm_experimental.py` ✅ (already removed)
- `tools/cleanup_and_fix.py` ✅ (already removed)
- Any other `tools/*_fix*.py` or `tools/*_temp*.py` scripts

#### 2. Consolidate Documentation
**Current state**: 112 markdown files with heavy duplication

**Target structure** (~10 organized files):
```
docs/
├── README.md                          # Project overview, quick start
├── ARCHITECTURE.md                    # System design, component overview
├── API_REFERENCE.md                   # Public API documentation
├── RENDERING.md                       # Graphics pipeline, SDF, GI, shaders
├── GAMEPLAY.md                        # Game systems, races, units, campaigns
├── ASSET_PIPELINE.md                  # Import, serialization, formats
├── EDITOR_GUIDE.md                    # Editor usage, tools, workflows
├── BUILD_AND_DEPLOYMENT.md            # CMake, platforms, dependencies
├── CONTRIBUTING.md                    # Code style, PR process, testing
└── PROJECT_RESTRUCTURING_PLAN.md     # This file
```

**Consolidation actions**:
- Merge all `engine/*/README.md` into `ARCHITECTURE.md` by component
- Merge `SHADER_INTEGRATION.md`, `SDF_SYSTEM.md`, `GLOBAL_ILLUMINATION.md` into `RENDERING.md`
- Merge race docs into `GAMEPLAY.md` with data-driven race table
- Delete duplicate `README.md` files after consolidation

### 🟡 HIGH PRIORITY (Complete within 1 week)

#### 3. Fix CMakeLists.txt Build System

**Problem**: Only 86 of 498 engine files are included in build

**Action Plan**:

**Phase 3.1**: Add all orphaned source files systematically by directory:
```cmake
# Core systems (already included - verify completeness)
engine/core/Engine.cpp
engine/core/Window.cpp
engine/core/Input.cpp
engine/core/Logger.cpp

# Graphics (many missing)
engine/graphics/Shader.cpp                    # ✅ included
engine/graphics/Texture.cpp                   # ✅ included
engine/graphics/Mesh.cpp                      # ✅ included
engine/graphics/Model.cpp                     # ✅ included
engine/graphics/Camera.cpp                    # ✅ included
engine/graphics/Framebuffer.cpp               # ✅ included
engine/graphics/SDFRenderer.cpp               # ✅ included
engine/graphics/RadianceCascade.cpp           # ✅ included
engine/graphics/SpectralRenderer.cpp          # ✅ included
engine/graphics/VolumetricRenderer.cpp        # ❌ MISSING
engine/graphics/ParticleSystem.cpp            # ❌ MISSING
engine/graphics/debug/DebugDraw.cpp           # ❌ MISSING
engine/graphics/pipeline/RenderPipeline.cpp   # ❌ MISSING
# ... 30+ more graphics files

# Physics (ENTIRE SYSTEM MISSING)
engine/physics/PhysicsEngine.cpp              # ❌ MISSING
engine/physics/RigidBody.cpp                  # ❌ MISSING
engine/physics/Collider.cpp                   # ❌ MISSING
# ... all physics files

# Networking (ENTIRE SYSTEM MISSING)
engine/networking/NetworkManager.cpp          # ❌ MISSING
engine/networking/Packet.cpp                  # ❌ MISSING
# ... all networking files

# UI (ENTIRE SYSTEM MISSING except ImGuiRenderer)
engine/ui/UISystem.cpp                        # ❌ MISSING
engine/ui/Widget.cpp                          # ❌ MISSING
engine/ui/Panel.cpp                           # ❌ MISSING
# ... all UI files

# Audio (ENTIRE SYSTEM MISSING)
engine/audio/AudioEngine.cpp                  # ❌ MISSING
engine/audio/AudioSource.cpp                  # ❌ MISSING
# ... all audio files

# Persistence (ENTIRE SYSTEM MISSING)
engine/persistence/SaveGameManager.cpp        # ❌ MISSING
engine/persistence/Serializer.cpp             # ❌ MISSING
# ... all persistence files

# Game logic (MINIMAL INCLUDED)
game/logic/GameLogic.cpp                      # ❌ MISSING
game/logic/UnitController.cpp                 # ❌ MISSING
game/logic/BuildingController.cpp             # ❌ MISSING
# ... 656+ game files
```

**Phase 3.2**: Organize CMakeLists.txt by system:
```cmake
# Proposed structure:
set(NOVA3D_CORE_SOURCES ...)
set(NOVA3D_GRAPHICS_SOURCES ...)
set(NOVA3D_PHYSICS_SOURCES ...)
set(NOVA3D_NETWORKING_SOURCES ...)
set(NOVA3D_AUDIO_SOURCES ...)
set(NOVA3D_UI_SOURCES ...)
set(NOVA3D_PERSISTENCE_SOURCES ...)
set(NOVA3D_PATHFINDING_SOURCES ...)

add_library(nova3d
    ${NOVA3D_CORE_SOURCES}
    ${NOVA3D_GRAPHICS_SOURCES}
    ${NOVA3D_PHYSICS_SOURCES}
    # ...
)
```

**TODO**: Create script to auto-generate CMakeLists.txt from directory scan with manual review

#### 4. Split Large Files (20+ files over 1300 lines)

**Priority split targets**:

| File | Lines | Proposed Split |
|------|-------|----------------|
| `examples/StandaloneEditor.cpp` | 3109 | → Editor.cpp (core), EditorMenus.cpp, EditorPanels.cpp, EditorTools.cpp |
| `engine/graphics/SDFModel.cpp` | 2147 | → SDFModel.cpp (core), SDFPrimitives.cpp, SDFOperations.cpp |
| `game/races/HumanRace.cpp` | ~1400 | → Data-driven approach (see #7) |
| `game/races/OrcRace.cpp` | ~1400 | → Data-driven approach (see #7) |
| (5 more race files) | ~1400 each | → Data-driven approach (see #7) |
| `engine/ui/UISystem.cpp` | 1800+ | → UISystem.cpp (core), UILayout.cpp, UIEvents.cpp |
| `engine/graphics/Renderer.cpp` | 1600+ | → Renderer.cpp (core), RenderCommands.cpp, RenderState.cpp |

**Split strategy**:
1. Keep public API in original file
2. Move implementation details to new files
3. Update includes and CMakeLists.txt
4. Maintain git history with `git mv` where possible

#### 5. Editor Menu Implementation (15 unimplemented handlers)

**File menu gaps**:
- [ ] `LoadMap()` - Load JSON map file using JsonAssetSerializer
- [ ] `SaveMap()` - Save current map to JSON

**Edit menu gaps**:
- [ ] `Paste()` - Implement clipboard paste from ClipboardManager

**Heightmap import/export**:
- [ ] `ImportHeightmap()` - Use `stb_image.h` to load PNG/JPG
- [ ] `ExportHeightmap()` - Use `stb_image_write.h` to save PNG

**Object selection and manipulation**:
- [ ] Implement ray-casting for object picking
- [ ] Make transform gizmos interactive (TranslateGizmo, RotateGizmo, ScaleGizmo)
- [ ] Connect gizmo to selected object transforms

**Settings menu**:
- [ ] Remove "Settings" menu or implement SettingsDialog

**Agent tasks** (to be spawned):
- Agent 1: Implement File > LoadMap/SaveMap
- Agent 2: Complete Edit > Paste clipboard operation
- Agent 3: Implement heightmap import/export
- Agent 4: Add ray-casting for object selection
- Agent 5: Make transform gizmos interactive

### 🟢 MEDIUM PRIORITY (Complete within 2 weeks)

#### 6. Create Abstraction Interfaces

**Missing platform abstraction**:
```cpp
// engine/platform/IPlatform.hpp
class IPlatform {
public:
    virtual ~IPlatform() = default;
    virtual bool Initialize() = 0;
    virtual void Update() = 0;
    virtual void* GetNativeWindow() = 0;
    virtual std::string GetPlatformName() = 0;
};

// Implementations:
// engine/platform/WindowsPlatform.hpp
// engine/platform/LinuxPlatform.hpp
// engine/platform/MacOSPlatform.hpp
// engine/platform/IOSPlatform.hpp
// engine/platform/AndroidPlatform.hpp
```

**Missing renderer abstraction**:
```cpp
// engine/graphics/IRenderer.hpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Initialize() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void RenderMesh(const Mesh& mesh, const Material& material) = 0;
};

// Implementations:
// engine/graphics/OpenGLRenderer.hpp
// engine/graphics/VulkanRenderer.hpp (future)
// engine/graphics/MetalRenderer.hpp (future)
```

**Missing terrain abstraction**:
```cpp
// game/terrain/ITerrain.hpp
class ITerrain {
public:
    virtual ~ITerrain() = default;
    virtual float GetHeightAt(float x, float z) const = 0;
    virtual void SetHeightAt(float x, float z, float height) = 0;
    virtual void Smooth(float x, float z, float radius) = 0;
    virtual void Flatten(float x, float z, float radius, float targetHeight) = 0;
};

// Implementations:
// game/terrain/HeightmapTerrain.hpp
// game/terrain/VoxelTerrain.hpp
// game/terrain/SDFTerrain.hpp
```

**Missing serializer abstraction**:
```cpp
// engine/persistence/ISerializer.hpp
class ISerializer {
public:
    virtual ~ISerializer() = default;
    virtual bool Serialize(const std::string& filepath, const void* data) = 0;
    virtual bool Deserialize(const std::string& filepath, void* outData) = 0;
};

// Implementations:
// engine/persistence/JsonSerializer.hpp
// engine/persistence/BinarySerializer.hpp
// engine/persistence/XMLSerializer.hpp
```

#### 7. Consolidate Race Implementations (Data-Driven Approach)

**Problem**: 7 race files × ~1400 lines = ~9800 lines of duplicated logic

**Current structure** (duplicated):
```cpp
// game/races/HumanRace.cpp, OrcRace.cpp, ElfRace.cpp, etc.
class HumanRace {
    std::vector<UnitType> m_units;
    std::vector<BuildingType> m_buildings;
    void LoadUnits() { /* hardcoded */ }
    void LoadBuildings() { /* hardcoded */ }
};
```

**Proposed data-driven structure**:
```cpp
// game/races/RaceBase.hpp (~200 lines)
class RaceBase {
protected:
    std::string m_raceId;
    std::vector<UnitType> m_units;
    std::vector<BuildingType> m_buildings;
    std::vector<TechTree> m_tech;

public:
    virtual bool LoadFromJson(const std::string& filepath);
    virtual UnitType* GetUnit(const std::string& unitId);
    virtual BuildingType* GetBuilding(const std::string& buildingId);
};

// game/races/RaceManager.hpp (~100 lines)
class RaceManager {
    std::map<std::string, std::unique_ptr<RaceBase>> m_races;
public:
    bool LoadRace(const std::string& raceId, const std::string& jsonPath);
    RaceBase* GetRace(const std::string& raceId);
};
```

**Race data files**:
```
game/data/races/
├── humans.json       # All human race data
├── orcs.json         # All orc race data
├── elves.json        # All elf race data
├── undead.json       # All undead race data
├── demons.json       # All demon race data
├── dragons.json      # All dragon race data
└── celestials.json   # All celestial race data
```

**Benefits**:
- Reduce code from ~9800 lines to ~300 lines (97% reduction)
- Allow game designers to modify races without C++ recompilation
- Easier to balance and add new races
- Centralized validation and error handling

#### 8. Reorganize Directory Structure

**Current issues**:
- `game/` directory is inside `engine/` (confusing separation)
- Tools scattered between `tools/` and `examples/`
- Assets in multiple locations

**Proposed structure**:
```
Old3DEngine/
├── engine/                    # Core engine library
│   ├── core/
│   ├── graphics/
│   ├── physics/
│   ├── audio/
│   ├── networking/
│   ├── ui/
│   ├── persistence/
│   ├── platform/
│   └── CMakeLists.txt
├── game/                      # Move from engine/game/ to top level
│   ├── src/                   # Game source code
│   │   ├── logic/
│   │   ├── races/
│   │   ├── units/
│   │   ├── buildings/
│   │   ├── terrain/
│   │   └── ui/
│   ├── assets/                # All game assets
│   │   ├── heroes/
│   │   ├── models/
│   │   ├── textures/
│   │   ├── shaders/
│   │   ├── audio/
│   │   └── data/             # JSON data files
│   └── CMakeLists.txt
├── tools/                     # Standalone tools
│   ├── asset_icon_renderer_gi/
│   ├── model_converter/
│   ├── texture_compressor/
│   └── CMakeLists.txt
├── editor/                    # Move from examples/
│   ├── StandaloneEditor.cpp
│   ├── StandaloneEditor.hpp
│   └── CMakeLists.txt
├── examples/                  # Simple usage examples
│   ├── minimal/
│   ├── rts_demo/
│   └── CMakeLists.txt
├── docs/                      # Consolidated documentation
├── build/                     # Build output (gitignored)
└── CMakeLists.txt            # Root build configuration
```

**Migration strategy**:
1. Use `git mv` to preserve history
2. Update all include paths
3. Update CMakeLists.txt files
4. Test build on all platforms
5. Update documentation

### 🔵 LOW PRIORITY (Complete within 1 month)

#### 9. Code Quality Improvements

**Apply consistent code style**:
- Run clang-format on all files
- Add `.clang-format` configuration
- Setup pre-commit hooks

**Add missing const correctness**:
- Audit getter methods
- Add `[[nodiscard]]` attributes
- Mark immutable parameters as `const&`

**Improve error handling**:
- Replace `assert()` with proper error reporting
- Add error codes/enums
- Implement error callback system

**Add unit tests**:
- Create `tests/` directory
- Add Google Test framework
- Test core systems (math, serialization, etc.)

#### 10. Performance Profiling

**Add performance instrumentation**:
- Integrate Tracy profiler or similar
- Add scope timers to critical paths
- Generate performance reports

**Optimize hot paths**:
- Profile rendering pipeline
- Optimize SDF raymarching
- Cache frequently-used calculations

## Implementation Timeline

| Week | Tasks |
|------|-------|
| 1 | IMMEDIATE: Remove backups, consolidate docs |
| 2-3 | HIGH: Fix CMakeLists.txt (systematic file addition) |
| 3-4 | HIGH: Split large files, implement editor menus |
| 5-6 | MEDIUM: Create abstraction interfaces |
| 7-8 | MEDIUM: Consolidate race implementations, reorganize directories |
| 9-12 | LOW: Code quality, performance profiling |

## Success Metrics

- [ ] 100% of source files in CMakeLists.txt
- [ ] Documentation reduced to ~10 well-organized files
- [ ] No files over 1000 lines (except generated code)
- [ ] All editor menu items functional
- [ ] Race code reduced by >90% through data-driven approach
- [ ] All major systems have interface abstractions
- [ ] Build time improved by >20% through better organization
- [ ] Zero compiler warnings on all platforms

## Notes

**Breaking changes**: This restructuring will require:
- All developers to pull fresh and update include paths
- CI/CD pipeline updates
- Documentation updates
- Asset path updates in existing maps/saves

**Risk mitigation**:
- Create `restructuring` branch for changes
- Keep `main` branch stable
- Test thoroughly before merging
- Provide migration guide for existing projects

**Generated**: 2025-12-06
**Author**: Claude (Nova3D Restructuring Analysis)
**Status**: Living document - update as restructuring progresses
