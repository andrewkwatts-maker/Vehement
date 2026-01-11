# Standalone Editor Integration Guide

## Overview

This guide documents the integration of all existing editor implementations into the StandaloneEditor. The integration creates a unified editing environment where users can open any asset type in its appropriate specialized editor.

## Files Created

### 1. StandaloneEditorIntegration.hpp
Location: `H:/Github/Old3DEngine/examples/StandaloneEditorIntegration.hpp`

This header defines:
- `AssetType` enum for classifying different asset types
- `IAssetEditor` interface (optional base for editors)
- `EditorIntegration` class that manages all specialized editors

Key features:
- Lazy loading of editors (only created when needed)
- Automatic asset type detection based on file extension
- Management of open editor windows
- Centralized editor lifecycle management

### 2. StandaloneEditorIntegration.cpp
Location: `H:/Github/Old3DEngine/examples/StandaloneEditorIntegration.cpp`

Implementation includes:
- Auto-detection of asset types from file paths/extensions
- Lazy initialization of each specialized editor
- Window management for open editors
- Routing of assets to appropriate editors
- Update and render loops for all active editors

## Integrated Editors

The following editors from `game/src/editor/` are integrated:

### Core Editors
1. **ConfigEditor** (`editor/ConfigEditor.hpp`)
   - JSON configuration files
   - Entity configs (units, buildings, tiles)
   - Property inspector with typed fields

2. **SDFModelEditor** (`editor/sdf/SDFModelEditor.cpp`)
   - SDF-based 3D models
   - Primitive hierarchy manipulation
   - Animation and pose library

### Game Content Editors
3. **MapEditor** (`editor/ingame/MapEditor.hpp`)
   - Game map creation
   - Terrain painting and sculpting
   - Object placement

4. **CampaignEditor** (`editor/ingame/CampaignEditor.hpp`)
   - Campaign mission sequences
   - Story/dialog editing
   - Victory/defeat conditions

5. **TriggerEditor** (`editor/ingame/TriggerEditor.hpp`)
   - Event triggers
   - Conditional logic

6. **ObjectEditor** (`editor/ingame/ObjectEditor.hpp`)
   - Game object properties
   - Component editing

7. **AIEditor** (`editor/ingame/AIEditor.hpp`)
   - AI behavior editing
   - Strategy configuration

### Animation Editors
8. **StateMachineEditor** (`editor/animation/StateMachineEditor.hpp`)
   - Animation state machines
   - Transition conditions
   - Visual node graph

9. **BlendTreeEditor** (`editor/animation/BlendTreeEditor.hpp`)
   - Animation blending
   - Node-based animation mixing

10. **KeyframeEditor** (`editor/animation/KeyframeEditor.hpp`)
    - Keyframe animation editing
    - Timeline manipulation

11. **BoneAnimationEditor** (`editor/animation/BoneAnimationEditor.hpp`)
    - Skeletal animation
    - Bone hierarchy

### Specialized Editors
12. **TalentTreeEditor** (`editor/race/TalentTreeEditor.hpp`)
    - Skill/talent trees
    - Node connections
    - Progression paths

13. **TerrainEditor** (`editor/terrain/TerrainEditor.hpp`)
    - 3D terrain sculpting
    - Voxel-based terrain
    - Caves and tunnels

14. **WorldTerrainEditor** (`editor/terrain/WorldTerrainEditor.hpp`)
    - Large-scale terrain
    - World generation

15. **ScriptEditor** (`editor/ScriptEditor.hpp`)
    - Script file editing
    - Syntax highlighting

16. **LevelEditor** (`editor/LevelEditor.hpp`)
    - Level/scene editing
    - Entity placement

## Asset Type Detection

The `DetectAssetType()` function automatically determines the appropriate editor based on:

### File Extensions
- `.json` → ConfigEditor (or specific editor based on filename)
- `.sdf`, `.sdfmodel` → SDFModelEditor
- `.map`, `.tmx` → MapEditor
- `.campaign` → CampaignEditor
- `.anim`, `.animation` → Animation editors
- `.statemachine`, `.asm` → StateMachineEditor
- `.blendtree` → BlendTreeEditor
- `.talent`, `.tree` → TalentTreeEditor
- `.terrain`, `.heightmap` → TerrainEditor
- `.lua`, `.py`, `.js`, `.cpp`, `.hpp` → ScriptEditor
- `.level`, `.scene` → LevelEditor

### Filename Patterns (for .json files)
- Contains "campaign" → CampaignEditor
- Contains "map" → MapEditor
- Contains "trigger" → TriggerEditor
- Contains "talent" or "skill" → TalentTreeEditor
- Contains "statemachine" → StateMachineEditor
- Contains "blendtree" → BlendTreeEditor
- Default → ConfigEditor

## Integration into StandaloneEditor

### Required Changes to StandaloneEditor.hpp

Add to private members:
```cpp
// Editor integration for opening assets in specialized editors
std::unique_ptr<EditorIntegration> m_editorIntegration;
```

Add include at the top:
```cpp
#include "StandaloneEditorIntegration.hpp"
```

### Required Changes to StandaloneEditor.cpp

#### 1. Initialize in Initialize()
```cpp
bool StandaloneEditor::Initialize() {
    // ... existing initialization code ...

    // Initialize editor integration
    m_editorIntegration = std::make_unique<EditorIntegration>();
    if (!m_editorIntegration->Initialize(nullptr)) {  // Pass main editor if available
        spdlog::warn("EditorIntegration initialization failed");
    }

    // ... rest of initialization ...
}
```

#### 2. Shutdown in Shutdown()
```cpp
void StandaloneEditor::Shutdown() {
    // ... existing shutdown code ...

    if (m_editorIntegration) {
        m_editorIntegration->Shutdown();
    }

    // ... rest of shutdown ...
}
```

#### 3. Update in Update()
```cpp
void StandaloneEditor::Update(float deltaTime) {
    // ... existing update code ...

    if (m_editorIntegration) {
        m_editorIntegration->Update(deltaTime);
    }

    // ... rest of update ...
}
```

#### 4. Render in RenderUI()
Add before the end of RenderUI():
```cpp
void StandaloneEditor::RenderUI() {
    // ... existing UI rendering ...

    // Render all open specialized editors
    if (m_editorIntegration) {
        m_editorIntegration->RenderUI();
    }

    // ... dialogs and other UI ...
}
```

### Enhanced Asset Browser Context Menu

In `RenderUnifiedContentBrowser()`, update the context menu (around line 2162):

```cpp
if (ImGui::BeginPopup("AssetContextMenu")) {
    ImGui::Text("Asset: %s", contextMenuPath.c_str());
    ImGui::Separator();

    // NEW: Open in Editor menu item
    if (ImGui::MenuItem("Open in Editor")) {
        if (m_editorIntegration) {
            AssetType assetType = EditorIntegration::DetectAssetType(contextMenuPath);
            if (m_editorIntegration->OpenAssetInEditor(contextMenuPath, assetType)) {
                spdlog::info("Opened asset in editor: {}", contextMenuPath);
            } else {
                spdlog::warn("Failed to open asset in editor: {}", contextMenuPath);
            }
        }
    }

    // Display detected asset type in submenu
    if (ImGui::BeginMenu("Open With...")) {
        AssetType detectedType = EditorIntegration::DetectAssetType(contextMenuPath);
        ImGui::Text("Detected type: %s", GetAssetTypeName(detectedType));
        ImGui::Separator();

        // Allow opening with specific editors
        if (ImGui::MenuItem("Config Editor")) {
            m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Config);
        }
        if (ImGui::MenuItem("Script Editor")) {
            m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Script);
        }
        // Add more as needed...

        ImGui::EndMenu();
    }

    ImGui::Separator();

    // Existing menu items...
    if (ImGui::MenuItem("Rename")) {
        // ... existing code ...
    }

    if (ImGui::MenuItem("Delete")) {
        // ... existing code ...
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Refresh")) {
        m_assetBrowser->Refresh();
    }

    ImGui::EndPopup();
}
```

### Helper Function for Asset Type Names

Add to StandaloneEditor.cpp:

```cpp
const char* GetAssetTypeName(AssetType type) {
    switch (type) {
        case AssetType::Config: return "Config";
        case AssetType::SDFModel: return "SDF Model";
        case AssetType::Campaign: return "Campaign";
        case AssetType::Map: return "Map";
        case AssetType::Trigger: return "Trigger";
        case AssetType::Animation: return "Animation";
        case AssetType::StateMachine: return "State Machine";
        case AssetType::BlendTree: return "Blend Tree";
        case AssetType::TalentTree: return "Talent Tree";
        case AssetType::Terrain: return "Terrain";
        case AssetType::Script: return "Script";
        case AssetType::Level: return "Level";
        default: return "Unknown";
    }
}
```

## Usage

### For End Users

1. **Browse assets** in the Content Browser panel
2. **Right-click** any asset file
3. **Select "Open in Editor"** from context menu
4. The appropriate specialized editor opens in a new dockable window
5. Edit the asset with full editor features
6. Close the editor window when done

### For Developers

#### Opening an Asset Programmatically

```cpp
// Auto-detect and open
m_editorIntegration->OpenAssetInEditor("assets/units/warrior.json");

// Explicitly specify type
m_editorIntegration->OpenAssetInEditor("assets/maps/level1.json", AssetType::Map);
```

#### Checking if Asset is Open

```cpp
if (m_editorIntegration->IsAssetOpen("assets/maps/level1.json")) {
    // Asset is currently being edited
}
```

#### Getting an Editor Instance

```cpp
// Get or create Config Editor
auto* configEditor = m_editorIntegration->GetConfigEditor();
if (configEditor) {
    configEditor->SelectConfig("warrior");
}
```

#### Closing Editors

```cpp
// Close specific editor
m_editorIntegration->CloseEditor("assets/maps/level1.json");

// Close all open editors
m_editorIntegration->CloseAllEditors();
```

## Editor Window Management

Each opened editor creates a separate ImGui window with:
- Title format: "Editor: [filename]"
- Close button (X) to close the editor
- Dockable within the main editor layout
- Independent state and UI

The `EditorIntegration` class tracks:
- Which assets are currently open
- Which editor type each asset is using
- The lifecycle of each editor instance

## Performance Considerations

### Lazy Loading
- Editors are only instantiated when first needed
- Unused editors don't consume memory
- First-time creation may have a brief delay

### Memory Management
- Editors are stored as unique_ptr for automatic cleanup
- Closing an editor window removes it from the open list
- Shutting down the integration destroys all editor instances

### Update/Render Efficiency
- Only initialized editors are updated
- Only open editor windows are rendered
- No overhead for unused editor types

## Future Enhancements

### Planned Features
1. **Recent Files** menu for each editor type
2. **Asset Preview** tooltips in the browser
3. **Drag and Drop** assets between editors
4. **Multi-tab** editor windows for same editor type
5. **Split View** for comparing assets side-by-side
6. **Hot Reload** when assets change on disk
7. **Undo/Redo** across all editors
8. **Save All** command for modified assets

### Editor Improvements
1. Implement full IAssetEditor interface for all editors
2. Add unsaved changes tracking
3. Prompt before closing editors with unsaved changes
4. Auto-save functionality
5. Asset dependency tracking
6. Validation and error checking
7. Real-time collaboration features

## Build Integration

### CMakeLists.txt Changes

Add the new source files:

```cmake
set(EXAMPLE_SOURCES
    # ... existing sources ...
    examples/StandaloneEditorIntegration.cpp
    examples/StandaloneEditorIntegration.hpp
)
```

Ensure all editor headers are in the include path:

```cmake
include_directories(
    ${CMAKE_SOURCE_DIR}/game/src
    ${CMAKE_SOURCE_DIR}/game/src/editor
    ${CMAKE_SOURCE_DIR}/examples
)
```

Link required libraries:

```cmake
target_link_libraries(StandaloneEditor
    # ... existing libraries ...
    GameEditors  # If editors are in a separate library
)
```

## Testing

### Unit Tests
Create tests for:
1. Asset type detection from various file paths
2. Editor creation and initialization
3. Opening and closing editors
4. Multiple editors of same type
5. Error handling for invalid assets

### Integration Tests
1. Open each asset type through the browser
2. Verify correct editor opens
3. Edit and save changes
4. Close and reopen editors
5. Test with many editors open simultaneously

### Manual Testing Checklist
- [ ] Right-click assets in Content Browser shows context menu
- [ ] "Open in Editor" opens appropriate editor
- [ ] Editor windows are dockable
- [ ] Multiple assets can be open simultaneously
- [ ] Closing editor window works properly
- [ ] Editors maintain state when window is reopened
- [ ] No crashes with unknown file types
- [ ] Performance is acceptable with many editors open

## Troubleshooting

### Common Issues

**Editor doesn't open:**
- Check spdlog output for error messages
- Verify asset file exists and is readable
- Ensure editor initialization succeeded
- Check asset type detection logic

**Wrong editor opens:**
- Verify file extension mapping in `DetectAssetType()`
- Use "Open With..." submenu for explicit editor selection
- Add custom detection logic for ambiguous files

**Editor window is blank:**
- Check if editor's `RenderUI()` is implemented
- Verify editor was initialized properly
- Look for ImGui errors in console

**Memory leaks:**
- Ensure `Shutdown()` is called
- Verify unique_ptr cleanup
- Check for circular references in editors

**Performance issues:**
- Profile with many editors open
- Consider limiting number of simultaneous editors
- Optimize heavy editor Update() loops

## Conclusion

This integration provides a seamless, unified editing experience by bringing all specialized editors together under the StandaloneEditor umbrella. Users can now edit any asset type without leaving the editor, while developers benefit from centralized editor management and a clear extension point for new editor types.

The modular design allows easy addition of new editors in the future, and the lazy-loading approach ensures good performance even with many available editor types.
