# Editor Integration Plan

## Overview

This document outlines the plan to integrate all standalone editor systems into the main unified editor framework.

## Current Status

### ✅ Already Integrated
- ConfigEditor - JSON config editing
- AssetBrowser (basic) - File browsing
- Inspector - Property inspector
- Hierarchy - Entity tree
- Console - Debug console
- Toolbar - Main toolbar
- WorldView - Main viewport
- TileInspector - Tile properties
- PCGPanel - Procedural generation
- LocationCrafter - Scene creation
- All in-game editor systems (InGameEditor)

### 🔴 Needs Integration (Priority Systems)
1. **ShaderGraphEditor** - Material/shader authoring
2. **TerrainEditor** - Voxel terrain sculpting
3. **ScriptEditorPanel** - Python IDE
4. **StateMachineEditor** - Animation state machines
5. **ContentBrowser (enhanced)** - Advanced asset management
6. **UIEditor** - UI layout designer
7. **ModelImportPanel** - Model import pipeline
8. **Animation editors** - BlendTree, Events, Blending tools

## Integration Strategy

### Phase 1: Core Editing Tools (Week 1)
**Goal:** Integrate essential content creation tools

#### 1.1 ShaderGraphEditor Integration
**Priority:** HIGHEST
**Files to modify:**
- `game/src/editor/Editor.hpp` - Add member variable
- `game/src/editor/Editor.cpp` - Initialize and render
- `engine/graphics/Material.cpp` - Add ShaderGraph loading
- `engine/graphics/ShaderManager.cpp` - Add graph compilation

**Steps:**
1. Add `std::unique_ptr<ShaderGraphEditor> m_shaderGraphEditor;` to Editor.hpp
2. Initialize in `Editor::Initialize()`:
   ```cpp
   m_shaderGraphEditor = std::make_unique<ShaderGraphEditor>();
   m_shaderGraphEditor->Initialize();
   ```
3. Add View menu item:
   ```cpp
   if (ImGui::MenuItem("Shader Editor", "Ctrl+Shift+S")) {
       m_shaderGraphEditor->Show();
   }
   ```
4. Render in `Editor::RenderPanels()`:
   ```cpp
   if (m_shaderGraphEditor) {
       m_shaderGraphEditor->Render();
   }
   ```
5. Connect file operations:
   - New shader graph → m_shaderGraphEditor->NewGraph()
   - Open shader graph → m_shaderGraphEditor->LoadGraph(path)
   - Save shader graph → m_shaderGraphEditor->SaveGraph(path)
6. Add keyboard shortcut: Ctrl+Shift+S → Toggle shader editor

**Integration with Material System:**
```cpp
// In Material::LoadFromFile(path)
if (path.extension() == ".shadergraph") {
    ShaderGraph graph;
    graph.LoadFromJSON(path);

    MaterialCompiler compiler;
    auto result = compiler.CompileGraph(graph);

    m_shader = ShaderManager::Instance().CreateFromSource(
        result.vertexShader,
        result.fragmentShader
    );

    // Apply material properties
    for (auto& param : result.parameters) {
        SetParameter(param.name, param.value);
    }
}
```

#### 1.2 TerrainEditor Integration
**Priority:** HIGH
**Files to modify:**
- `game/src/editor/Editor.hpp`
- `game/src/editor/Editor.cpp`
- `game/src/editor/WorldView.cpp` - Add terrain editing overlay

**Steps:**
1. Add `std::unique_ptr<TerrainEditor> m_terrainEditor;` to Editor.hpp
2. Initialize in `Editor::Initialize()`
3. Add View menu: "Terrain Editor" (Ctrl+Shift+T)
4. Render terrain editor panel
5. Add terrain editing mode to WorldView:
   ```cpp
   // In WorldView::Render()
   if (m_terrainEditMode && m_terrainEditor) {
       m_terrainEditor->RenderBrushPreview(camera);
       m_terrainEditor->HandleInput(input);
   }
   ```
6. Add toolbar button for terrain edit mode
7. Connect to World/VoxelTerrain system

#### 1.3 ScriptEditorPanel Integration
**Priority:** HIGH
**Files to modify:**
- `game/src/editor/Editor.hpp` - Replace ScriptEditor with ScriptEditorPanel
- `game/src/editor/Editor.cpp`

**Steps:**
1. Replace `ScriptEditor* m_scriptEditor` with `std::unique_ptr<ScriptEditorPanel> m_scriptEditorPanel`
2. Initialize with Python engine reference
3. Add View menu: "Script Editor" (Ctrl+Shift+P)
4. Connect to AssetBrowser:
   - Double-click .py file → Open in ScriptEditorPanel
5. Add File menu items:
   - New Script
   - Open Script
   - Save Script
   - Run Script (F5)
6. Add keyboard shortcuts:
   - F5 → Run current script
   - Ctrl+B → Set/clear breakpoint
   - F10 → Step over (when debugging)

### Phase 2: Asset Management (Week 2)
**Goal:** Upgrade asset browsing and import pipeline

#### 2.1 Replace AssetBrowser with ContentBrowser
**Priority:** MEDIUM
**Files to modify:**
- `game/src/editor/Editor.hpp` - Replace AssetBrowser
- `game/src/editor/Editor.cpp`

**Steps:**
1. Replace `AssetBrowser* m_assetBrowser` with `std::unique_ptr<ContentBrowser> m_contentBrowser`
2. Initialize ContentBrowser with content database
3. Keep same panel visibility flag for compatibility
4. Update menu references
5. Add specialized browsers to submenu:
   ```cpp
   if (ImGui::BeginMenu("Content Browsers")) {
       if (ImGui::MenuItem("Units")) m_unitBrowser->Show();
       if (ImGui::MenuItem("Buildings")) m_buildingBrowser->Show();
       if (ImGui::MenuItem("Tiles")) m_tileBrowser->Show();
       if (ImGui::MenuItem("Spells")) m_spellBrowser->Show();
       ImGui::EndMenu();
   }
   ```

#### 2.2 Integrate Import Panels
**Priority:** MEDIUM
**Files to modify:**
- `game/src/editor/content/ContentBrowser.cpp` - Add import actions

**Steps:**
1. Add "Import Asset" to ContentBrowser context menu
2. Detect file type and open appropriate import panel:
   ```cpp
   void ContentBrowser::ImportAsset(const std::string& path) {
       std::string ext = GetExtension(path);
       if (ext == ".fbx" || ext == ".obj" || ext == ".gltf") {
           m_modelImportPanel->Show(path);
       } else if (ext == ".png" || ext == ".jpg") {
           m_textureImportPanel->Show(path);
       } else if (ext == ".wav" || ext == ".mp3") {
           m_audioImportPanel->Show(path);
       }
   }
   ```
3. Connect import completion to content database refresh
4. Generate thumbnails for imported assets

### Phase 3: Animation Tools (Week 3)
**Goal:** Integrate animation authoring tools

#### 3.1 StateMachineEditor Integration
**Priority:** MEDIUM
**Files to modify:**
- `game/src/editor/Editor.hpp`
- `game/src/editor/Editor.cpp`

**Steps:**
1. Add `std::unique_ptr<StateMachineEditor> m_stateMachineEditor;`
2. Add View → Animation submenu:
   ```cpp
   if (ImGui::BeginMenu("Animation")) {
       if (ImGui::MenuItem("State Machine", "Ctrl+Shift+A")) {
           m_stateMachineEditor->Show();
       }
       if (ImGui::MenuItem("Blend Tree")) {
           m_blendTreeEditor->Show();
       }
       if (ImGui::MenuItem("Animation Events")) {
           m_animEventEditor->Show();
       }
       ImGui::EndMenu();
   }
   ```
3. Connect to AssetBrowser:
   - Double-click .statemachine file → Open in StateMachineEditor
4. Add File operations: New/Open/Save state machine

#### 3.2 Animation Panel Suite
**Priority:** LOW
**Files to modify:**
- `game/src/editor/Editor.hpp` - Add animation panels

**Steps:**
1. Add skeleton, keyframe, event, curve panels
2. Show panels when animation is selected
3. Sync panels with active animation clip

### Phase 4: UI Design Tools (Week 4)
**Goal:** Integrate UI designer

#### 4.1 UIEditor Integration
**Priority:** MEDIUM
**Files to modify:**
- `game/src/editor/Editor.hpp`
- `game/src/editor/Editor.cpp`

**Steps:**
1. Add `std::unique_ptr<UIEditor> m_uiEditor;`
2. Add View menu: "UI Designer" (Ctrl+Shift+U)
3. Connect to AssetBrowser:
   - Double-click .ui file → Open in UIEditor
4. Add preview mode toggle
5. Connect to RuntimeUIManager for live preview

### Phase 5: Material Workflow (Week 5)
**Goal:** Complete shader graph to material pipeline

#### 5.1 Material System Integration
**Priority:** HIGH
**Files to modify:**
- `engine/graphics/Material.hpp`
- `engine/graphics/Material.cpp`
- `engine/graphics/ShaderManager.hpp`
- `engine/graphics/ShaderManager.cpp`

**Steps:**
1. Add `Material::LoadFromShaderGraph(path)` method
2. Implement ShaderGraph compilation in ShaderManager
3. Add material parameter binding from graph parameters
4. Add hot-reload support for .shadergraph files
5. Update MaterialLibrary to show shader graph materials

#### 5.2 Material Library Integration
**Priority:** MEDIUM
**Files to modify:**
- `game/src/editor/Editor.hpp` - Add MaterialLibrary panel
- `engine/materials/ShaderGraphEditor.hpp` - Expose MaterialLibrary

**Steps:**
1. Add View menu: "Material Library"
2. Show material thumbnails
3. Double-click material → Open in ShaderGraphEditor
4. Add "Create Material Instance" action
5. Connect to AssetBrowser

### Phase 6: Unified Panel Management (Week 6)
**Goal:** Standardize all panel management

#### 6.1 Panel Registry Implementation
**Priority:** LOW
**Files to modify:**
- `game/src/editor/Editor.hpp` - Use PanelRegistry
- `game/src/editor/Editor.cpp`

**Steps:**
1. Register all panels with PanelRegistry
2. Use `PanelRegistry::RenderViewMenu()` for View menu
3. Standardize visibility toggles
4. Save/load panel layouts to JSON
5. Add "Reset Layout" option

## Detailed Implementation: ShaderGraphEditor

### File Modifications

#### 1. Editor.hpp
```cpp
// Add includes
#include "engine/materials/ShaderGraphEditor.hpp"

class Editor {
private:
    // Add member
    std::unique_ptr<ShaderGraphEditor> m_shaderGraphEditor;

public:
    // Add accessor
    ShaderGraphEditor* GetShaderGraphEditor() { return m_shaderGraphEditor.get(); }
};
```

#### 2. Editor.cpp
```cpp
bool Editor::Initialize(Nova::Engine& engine, const EditorConfig& config) {
    // ... existing initialization ...

    // Initialize shader graph editor
    m_shaderGraphEditor = std::make_unique<ShaderGraphEditor>();
    m_shaderGraphEditor->Initialize();

    return true;
}

void Editor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // ... existing menu items ...

            if (ImGui::MenuItem("New Shader Graph", "Ctrl+Shift+N")) {
                m_shaderGraphEditor->NewGraph();
                m_shaderGraphEditor->Show();
            }
            if (ImGui::MenuItem("Open Shader Graph...", "Ctrl+Shift+O")) {
                // Open file dialog
                std::string path = OpenFileDialog("Shader Graph (*.shadergraph)\0*.shadergraph\0");
                if (!path.empty()) {
                    m_shaderGraphEditor->LoadGraph(path);
                    m_shaderGraphEditor->Show();
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            // ... existing panels ...

            ImGui::Separator();
            if (ImGui::MenuItem("Shader Editor", "Ctrl+Shift+S", m_shaderGraphEditor->IsVisible())) {
                m_shaderGraphEditor->ToggleVisibility();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void Editor::RenderPanels() {
    // ... existing panels ...

    // Render shader graph editor
    if (m_shaderGraphEditor && m_shaderGraphEditor->IsVisible()) {
        m_shaderGraphEditor->Render();
    }
}

void Editor::Update(float deltaTime) {
    // ... existing update ...

    // Update shader graph editor
    if (m_shaderGraphEditor) {
        m_shaderGraphEditor->Update(deltaTime);
    }

    // Handle keyboard shortcuts
    auto& input = m_engine.GetInput();
    if (input.IsKeyDown(Nova::Key::LeftControl) && input.IsKeyDown(Nova::Key::LeftShift)) {
        if (input.IsKeyPressed(Nova::Key::S)) {
            m_shaderGraphEditor->ToggleVisibility();
        }
    }
}
```

#### 3. Material.cpp
```cpp
#include "engine/materials/ShaderGraph.hpp"
#include "engine/materials/MaterialCompiler.hpp"

bool Material::LoadFromFile(const std::string& path) {
    std::filesystem::path filePath(path);

    if (filePath.extension() == ".shadergraph") {
        return LoadFromShaderGraph(path);
    } else if (filePath.extension() == ".mat") {
        return LoadFromJSON(path);
    }

    return false;
}

bool Material::LoadFromShaderGraph(const std::string& path) {
    // Load shader graph
    ShaderGraph graph;
    if (!graph.LoadFromJSON(path)) {
        spdlog::error("Failed to load shader graph: {}", path);
        return false;
    }

    // Compile to GLSL
    MaterialCompiler compiler;
    auto result = compiler.CompileGraph(graph);

    if (!result.success) {
        spdlog::error("Failed to compile shader graph: {}", result.error);
        return false;
    }

    // Create shader
    auto& shaderMgr = ShaderManager::Instance();
    m_shader = shaderMgr.CreateFromSource(
        result.vertexShader,
        result.fragmentShader,
        path + ".vert",
        path + ".frag"
    );

    if (!m_shader) {
        spdlog::error("Failed to create shader from graph");
        return false;
    }

    // Apply material properties from graph
    m_domain = graph.GetDomain();
    m_blendMode = graph.GetBlendMode();
    m_shadingModel = graph.GetShadingModel();

    // Set parameters from graph
    for (const auto& param : graph.GetParameters()) {
        switch (param.type) {
            case ParameterType::Float:
                SetFloat(param.name, param.defaultValue.floatValue);
                break;
            case ParameterType::Vector2:
                SetVec2(param.name, param.defaultValue.vec2Value);
                break;
            case ParameterType::Vector3:
                SetVec3(param.name, param.defaultValue.vec3Value);
                break;
            case ParameterType::Vector4:
                SetVec4(param.name, param.defaultValue.vec4Value);
                break;
            case ParameterType::Color:
                SetVec4(param.name, param.defaultValue.colorValue);
                break;
            case ParameterType::Texture2D:
                SetTexture(param.name, param.defaultValue.texture);
                break;
        }
    }

    m_shaderGraphPath = path;
    return true;
}
```

## Testing Plan

### Phase 1 Testing
- [ ] Create new shader graph
- [ ] Add nodes and connect them
- [ ] Compile to shader
- [ ] Apply to material
- [ ] Render with material
- [ ] Save and reload shader graph
- [ ] Hot-reload shader graph
- [ ] Edit parameters in material inspector

### Phase 2 Testing
- [ ] Browse assets in ContentBrowser
- [ ] Search and filter assets
- [ ] Import FBX model
- [ ] Configure import settings
- [ ] Generate thumbnails
- [ ] Drag asset to scene

### Phase 3 Testing
- [ ] Create animation state machine
- [ ] Add states and transitions
- [ ] Edit transition conditions
- [ ] Preview state machine
- [ ] Export to runtime format

### Phase 4 Testing
- [ ] Design UI layout
- [ ] Add widgets and configure properties
- [ ] Preview UI with test data
- [ ] Save UI template
- [ ] Load UI in game runtime

### Phase 5 Testing
- [ ] Compile shader graph to material
- [ ] Use material on mesh
- [ ] Edit material parameters
- [ ] Hot-reload shader changes
- [ ] Create material instance
- [ ] Browse material library

### Phase 6 Testing
- [ ] Save panel layout
- [ ] Load panel layout
- [ ] Reset to default layout
- [ ] Dock/undock panels
- [ ] Toggle panel visibility

## Success Criteria

✅ **Phase 1 Complete** when:
- ShaderGraphEditor opens from View menu
- Can create, edit, save shader graphs
- Compiled shaders work in Material system
- TerrainEditor sculpts voxel terrain
- ScriptEditorPanel edits and runs Python scripts

✅ **Phase 2 Complete** when:
- ContentBrowser replaces AssetBrowser
- Can import models/textures/audio
- Thumbnails generate automatically
- Asset actions work (delete, rename, etc.)

✅ **Phase 3 Complete** when:
- StateMachineEditor creates animation FSMs
- Can preview state transitions
- Exports work with runtime AnimationController

✅ **Phase 4 Complete** when:
- UIEditor creates UI layouts
- Previews work with test data
- Exported UIs render in game

✅ **Phase 5 Complete** when:
- Materials load from .shadergraph files
- Hot-reload updates materials in real-time
- MaterialLibrary shows all available materials

✅ **Phase 6 Complete** when:
- All panels use PanelRegistry
- Layouts save/load correctly
- View menu auto-generates from registry

## Maintenance

### Documentation
- Update user manual with new editor features
- Create video tutorials for each major editor
- Document keyboard shortcuts
- Create example shader graphs, UIs, animations

### Performance Monitoring
- Profile editor frame time (target <16ms)
- Monitor memory usage (target <500MB)
- Track asset compilation times
- Optimize thumbnail generation

### Bug Tracking
- Create GitHub issues for integration bugs
- Tag issues by phase (integration-phase1, etc.)
- Assign priority labels
- Track progress on project board

## Timeline

| Phase | Duration | Completion Date |
|-------|----------|-----------------|
| Phase 1: Core Tools | 1 week | Week 1 |
| Phase 2: Asset Management | 1 week | Week 2 |
| Phase 3: Animation | 1 week | Week 3 |
| Phase 4: UI Design | 1 week | Week 4 |
| Phase 5: Material Workflow | 1 week | Week 5 |
| Phase 6: Panel Management | 1 week | Week 6 |
| **Total** | **6 weeks** | **End of Week 6** |

## Resources Required

- 1 senior developer (full-time)
- Access to all editor source code
- Test assets (models, textures, animations)
- Bug tracking system
- Version control (Git)

## Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| API incompatibilities | High | Medium | Create adapter layers |
| Performance regression | Medium | Low | Profile before/after |
| UI layout conflicts | Low | High | Use dockspace system |
| Shader compilation bugs | High | Medium | Extensive shader testing |
| Asset import failures | Medium | Medium | Add error recovery |

## Conclusion

This integration plan will unify all standalone editors into a cohesive, professional-grade game editor comparable to Unreal Engine or Unity. The phased approach ensures stability and allows for iterative testing.
