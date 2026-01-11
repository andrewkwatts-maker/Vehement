# Instance Property System - Complete File List

## Core System Files

### Engine Files (Required)

| File | Location | Description |
|------|----------|-------------|
| `InstanceData.hpp` | `H:/Github/Old3DEngine/engine/scene/` | Instance data structure with transform, overrides, and custom data |
| `InstanceData.cpp` | `H:/Github/Old3DEngine/engine/scene/` | Serialization, file I/O, and property override logic |
| `InstanceManager.hpp` | `H:/Github/Old3DEngine/engine/scene/` | Instance and archetype management interface |
| `InstanceManager.cpp` | `H:/Github/Old3DEngine/engine/scene/` | Loading, saving, merging, and dirty tracking implementation |

### Editor Files (Required)

| File | Location | Description |
|------|----------|-------------|
| `PropertyEditor.hpp` | `H:/Github/Old3DEngine/examples/` | ImGui property tree rendering utilities |
| `PropertyEditor.cpp` | `H:/Github/Old3DEngine/examples/` | Type-specific widgets and UI helpers |
| `InstancePropertyEditor.hpp` | `H:/Github/Old3DEngine/examples/` | High-level property editor with auto-save |
| `InstancePropertyEditor.cpp` | `H:/Github/Old3DEngine/examples/` | Property panel rendering and debounced saving |

### Documentation Files

| File | Location | Description |
|------|----------|-------------|
| `InstancePropertySystem.md` | `H:/Github/Old3DEngine/docs/` | Complete system documentation |
| `InstanceSystem_QuickStart.md` | `H:/Github/Old3DEngine/docs/` | Quick start guide (5 minutes) |
| `InstanceSystem_IntegrationExample.cpp` | `H:/Github/Old3DEngine/examples/` | Detailed integration example with StandaloneEditor |
| `InstanceSystem_CMake_Integration.txt` | `H:/Github/Old3DEngine/docs/` | CMakeLists.txt integration snippets |
| `InstanceSystem_FileList.md` | `H:/Github/Old3DEngine/docs/` | This file - complete file listing |

## Example/Test Files

### Archetype Examples

| File | Location | Description |
|------|----------|-------------|
| `footman.json` | `H:/Github/Old3DEngine/assets/config/humans/units/` | Basic infantry unit archetype |
| `archer.json` | `H:/Github/Old3DEngine/assets/config/humans/units/` | Ranged unit archetype |
| `barracks.json` | `H:/Github/Old3DEngine/assets/config/humans/buildings/` | Military building archetype |

### Instance Examples

| File | Location | Description |
|------|----------|-------------|
| `example-footman-001.json` | `H:/Github/Old3DEngine/assets/maps/test_map/instances/` | Example instance with overrides and custom data |

## Dependencies

The system requires these external libraries (likely already in your project):

- **nlohmann/json** - JSON parsing and serialization
- **GLM** - Math library for vectors and quaternions
- **spdlog** - Logging
- **ImGui** - UI rendering (editor only)

## File Statistics

- **Total Implementation Files:** 8 (4 engine + 4 editor)
- **Total Documentation Files:** 5
- **Total Example Files:** 4 (3 archetypes + 1 instance)
- **Total Lines of Code:** ~3,500
- **Header Files:** 4
- **Source Files:** 4
- **JSON Files:** 4

## File Sizes (Approximate)

| File | Lines | Size |
|------|-------|------|
| InstanceData.hpp | 175 | 6 KB |
| InstanceData.cpp | 200 | 7 KB |
| InstanceManager.hpp | 150 | 6 KB |
| InstanceManager.cpp | 280 | 10 KB |
| PropertyEditor.hpp | 100 | 4 KB |
| PropertyEditor.cpp | 350 | 13 KB |
| InstancePropertyEditor.hpp | 100 | 4 KB |
| InstancePropertyEditor.cpp | 300 | 11 KB |
| InstanceSystem_IntegrationExample.cpp | 250 | 10 KB |
| **Total** | **~1,905** | **~71 KB** |

## Directory Structure

```
Old3DEngine/
│
├── engine/
│   └── scene/
│       ├── InstanceData.hpp
│       ├── InstanceData.cpp
│       ├── InstanceManager.hpp
│       └── InstanceManager.cpp
│
├── examples/
│   ├── PropertyEditor.hpp
│   ├── PropertyEditor.cpp
│   ├── InstancePropertyEditor.hpp
│   ├── InstancePropertyEditor.cpp
│   └── InstanceSystem_IntegrationExample.cpp
│
├── docs/
│   ├── InstancePropertySystem.md
│   ├── InstanceSystem_QuickStart.md
│   ├── InstanceSystem_CMake_Integration.txt
│   └── InstanceSystem_FileList.md
│
└── assets/
    ├── config/
    │   └── humans/
    │       ├── units/
    │       │   ├── footman.json
    │       │   └── archer.json
    │       └── buildings/
    │           └── barracks.json
    │
    └── maps/
        └── test_map/
            └── instances/
                └── example-footman-001.json
```

## Integration Checklist

Use this checklist to integrate the system into your project:

- [ ] Copy 8 implementation files to project
- [ ] Update CMakeLists.txt with new sources
- [ ] Add nlohmann/json dependency if not present
- [ ] Add forward declarations to StandaloneEditor.hpp
- [ ] Add member variables to StandaloneEditor class
- [ ] Initialize in StandaloneEditor::Initialize()
- [ ] Update in StandaloneEditor::Update()
- [ ] Replace RenderDetailsPanel() implementation
- [ ] Update SceneObject struct with instanceId/archetypeId
- [ ] Create test archetype JSON files
- [ ] Build and test
- [ ] Create instances and verify saving/loading
- [ ] Test property overrides
- [ ] Test custom data
- [ ] Test auto-save

## Quick Access

### To get started:
Read `InstanceSystem_QuickStart.md`

### For full documentation:
Read `InstancePropertySystem.md`

### For integration help:
See `InstanceSystem_IntegrationExample.cpp`

### For build setup:
See `InstanceSystem_CMake_Integration.txt`

## Version History

- **v1.0** (2024-12-03) - Initial implementation
  - Core instance data structure
  - Instance manager with load/save
  - Property editor UI
  - Auto-save with debouncing
  - Complete documentation

## Support

For issues or questions:
1. Check documentation in `docs/`
2. Review integration example
3. Verify file paths and JSON format
4. Check CMakeLists.txt configuration
5. Ensure all dependencies are installed

## License

Part of the Old3DEngine project.
