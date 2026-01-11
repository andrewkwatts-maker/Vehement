# Editor UI Systems - Current Status and Next Steps

## Executive Summary

The Old3DEngine RTS project has **65+ editor panels and systems** already implemented. The main task is **integration work** - connecting standalone editors to the main Editor framework.

## Priority Integration Tasks

### ✅ IMMEDIATE (This Week)

#### 1. Integrate ShaderGraphEditor
**Why:** Material authoring is critical for game content creation
**Files:** `engine/materials/ShaderGraphEditor.hpp`
**Action:**
- Add to Editor.hpp as member variable
- Add "Shader Editor" to View menu (Ctrl+Shift+S)
- Connect Material::LoadFromFile() to compile .shadergraph files
- Test: Create shader → Compile → Apply to mesh

#### 2. Integrate TerrainEditor
**Why:** World building requires terrain sculpting
**Files:** `game/src/editor/terrain/TerrainEditor.hpp`
**Action:**
- Add to Editor.hpp
- Add "Terrain Editor" to View menu (Ctrl+Shift+T)
- Add terrain edit mode overlay to WorldView
- Test: Sculpt terrain → Save → Reload

#### 3. Replace AssetBrowser with ContentBrowser
**Why:** Enhanced asset management with search, thumbnails, import pipeline
**Files:** Replace `game/src/editor/AssetBrowser.hpp` with `game/src/editor/content/ContentBrowser.hpp`
**Action:**
- Swap AssetBrowser for ContentBrowser in Editor.hpp
- Add specialized browsers (Unit, Building, Tile, Spell) to submenu
- Connect import panels (Model, Texture, Animation)
- Test: Import FBX → Configure → Generate thumbnail

### ⚠️ HIGH PRIORITY (Next Week)

#### 4. Integrate ScriptEditorPanel
**Why:** Python scripting for game logic
**Files:** `game/src/editor/scripting/ScriptEditorPanel.hpp`
**Action:**
- Replace basic ScriptEditor with ScriptEditorPanel
- Add "Script Editor" to View menu (Ctrl+Shift+P)
- Connect to PythonEngine for execution
- Add breakpoint support
- Test: Write script → Run (F5) → See output

#### 5. Integrate StateMachineEditor
**Why:** Animation state machines for characters
**Files:** `game/src/editor/animation/StateMachineEditor.hpp`
**Action:**
- Add View → Animation → State Machine
- Double-click .statemachine file → Open editor
- Connect to runtime AnimationController
- Test: Create FSM → Export → Play in game

#### 6. Integrate UIEditor
**Why:** UI layout design for menus/HUD
**Files:** `engine/ui/editor/UIEditor.hpp`
**Action:**
- Add "UI Designer" to View menu (Ctrl+Shift+U)
- Connect to RuntimeUIManager for preview
- Test: Design UI → Preview → Save template

### 📋 MEDIUM PRIORITY (Week 3-4)

#### 7. Connect Material System to ShaderGraph
**Why:** Complete shader workflow
**Files:** `engine/graphics/Material.cpp`, `engine/graphics/ShaderManager.cpp`
**Action:**
- Material::LoadFromShaderGraph() compiles graphs to shaders
- Add hot-reload for .shadergraph files
- MaterialLibrary shows compiled materials
- Test: Edit graph → Hot-reload → See material update

#### 8. Integrate Animation Suite
**Why:** Complete animation toolset
**Files:** `game/src/editor/animation/BlendTreeEditor.hpp`, etc.
**Action:**
- Add View → Animation submenu
- Add Blend Tree, Events, Curves, Skeleton editors
- Test: Create animation → Add events → Play

#### 9. Integrate ModelImportPanel
**Why:** Import pipeline for 3D assets
**Files:** `game/src/editor/import/ModelImportPanel.hpp`
**Action:**
- Connect to ContentBrowser "Import" action
- Auto-detect file type and open appropriate panel
- Test: Import FBX → Configure LODs → Generate preview

### 🔧 LOWER PRIORITY (Week 5-6)

#### 10. Use PanelRegistry for All Panels
**Why:** Standardize panel management
**Action:**
- Register all panels with PanelRegistry
- Auto-generate View menu from registry
- Save/load panel layouts to JSON
- Test: Customize layout → Save → Reload → Verify positions

## Quick Integration Template

For each editor, follow this pattern:

### Step 1: Add to Editor.hpp
```cpp
// Forward declaration
class ShaderGraphEditor;

class Editor {
private:
    std::unique_ptr<ShaderGraphEditor> m_shaderGraphEditor;

public:
    ShaderGraphEditor* GetShaderGraphEditor() { return m_shaderGraphEditor.get(); }
};
```

### Step 2: Initialize in Editor.cpp
```cpp
#include "engine/materials/ShaderGraphEditor.hpp"

bool Editor::Initialize(Nova::Engine& engine, const EditorConfig& config) {
    // ... existing init ...

    m_shaderGraphEditor = std::make_unique<ShaderGraphEditor>();
    m_shaderGraphEditor->Initialize();

    return true;
}
```

### Step 3: Add to View Menu
```cpp
void Editor::RenderMenuBar() {
    if (ImGui::BeginMenu("View")) {
        // ... existing items ...

        if (ImGui::MenuItem("Shader Editor", "Ctrl+Shift+S")) {
            m_shaderGraphEditor->ToggleVisibility();
        }

        ImGui::EndMenu();
    }
}
```

### Step 4: Render Panel
```cpp
void Editor::RenderPanels() {
    // ... existing panels ...

    if (m_shaderGraphEditor && m_shaderGraphEditor->IsVisible()) {
        m_shaderGraphEditor->Render();
    }
}
```

### Step 5: Update Logic
```cpp
void Editor::Update(float deltaTime) {
    // ... existing update ...

    if (m_shaderGraphEditor) {
        m_shaderGraphEditor->Update(deltaTime);
    }
}
```

### Step 6: Handle Shortcuts
```cpp
void Editor::ProcessInput() {
    auto& input = m_engine.GetInput();

    // Ctrl+Shift+S → Toggle Shader Editor
    if (input.IsKeyDown(Key::LeftControl) &&
        input.IsKeyDown(Key::LeftShift) &&
        input.IsKeyPressed(Key::S)) {
        m_shaderGraphEditor->ToggleVisibility();
    }
}
```

## Integration Checklist

For each editor integration, verify:

- [ ] Added to Editor.hpp as member variable
- [ ] Forward declaration added
- [ ] Initialized in Editor::Initialize()
- [ ] Added to View menu
- [ ] Keyboard shortcut implemented
- [ ] Rendered in Editor::RenderPanels()
- [ ] Updated in Editor::Update()
- [ ] Shutdown in Editor::Shutdown()
- [ ] File operations connected (New/Open/Save)
- [ ] AssetBrowser double-click integration (if applicable)
- [ ] Tested: Create → Edit → Save → Reload

## File Operation Patterns

### New File
```cpp
if (ImGui::MenuItem("New Shader Graph", "Ctrl+N")) {
    m_shaderGraphEditor->NewGraph();
    m_shaderGraphEditor->Show();
}
```

### Open File
```cpp
if (ImGui::MenuItem("Open Shader Graph...", "Ctrl+O")) {
    std::string path = OpenFileDialog("*.shadergraph");
    if (!path.empty()) {
        m_shaderGraphEditor->LoadGraph(path);
        m_shaderGraphEditor->Show();
    }
}
```

### Save File
```cpp
if (ImGui::MenuItem("Save Shader Graph", "Ctrl+S")) {
    if (m_shaderGraphEditor->HasOpenGraph()) {
        m_shaderGraphEditor->SaveGraph();
    }
}
```

### Save As
```cpp
if (ImGui::MenuItem("Save Shader Graph As...", "Ctrl+Shift+S")) {
    std::string path = SaveFileDialog("*.shadergraph");
    if (!path.empty()) {
        m_shaderGraphEditor->SaveGraph(path);
    }
}
```

## Asset Browser Integration

Connect double-click in AssetBrowser/ContentBrowser:

```cpp
// In AssetBrowser::OnAssetDoubleClicked(path)
std::filesystem::path p(path);
std::string ext = p.extension().string();

if (ext == ".shadergraph") {
    m_editor->GetShaderGraphEditor()->LoadGraph(path);
    m_editor->GetShaderGraphEditor()->Show();
} else if (ext == ".statemachine") {
    m_editor->GetStateMachineEditor()->LoadStateMachine(path);
    m_editor->GetStateMachineEditor()->Show();
} else if (ext == ".ui") {
    m_editor->GetUIEditor()->LoadTemplate(path);
    m_editor->GetUIEditor()->Show();
} else if (ext == ".py") {
    m_editor->GetScriptEditor()->OpenFile(path);
    m_editor->GetScriptEditor()->Show();
}
```

## Keyboard Shortcuts Reference

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+S | Toggle Shader Editor |
| Ctrl+Shift+T | Toggle Terrain Editor |
| Ctrl+Shift+P | Toggle Script Editor |
| Ctrl+Shift+A | Toggle Animation State Machine |
| Ctrl+Shift+U | Toggle UI Designer |
| Ctrl+Shift+M | Toggle Material Library |
| Ctrl+N | New (context-dependent) |
| Ctrl+O | Open (context-dependent) |
| Ctrl+S | Save (context-dependent) |
| Ctrl+Shift+S | Save As (context-dependent) |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| F5 | Run/Play (Script Editor, Animation Preview) |
| F10 | Step Over (Debugger) |
| F11 | Step Into (Debugger) |
| Delete | Delete selected (context-dependent) |

## Testing Commands

After each integration, test with:

```bash
# Build editor
cd build
cmake --build . --config Release

# Run editor standalone
./bin/Release/editor_standalone.exe

# Or run integrated with game
./bin/Release/nova_rts.exe --editor
```

## Success Criteria

✅ **Integration Complete** when:
1. Editor opens from View menu
2. Keyboard shortcut works
3. Can create new content
4. Can save and reload
5. Double-click from asset browser opens editor
6. Hot-reload works (where applicable)
7. No crashes or errors in console

## Estimated Timeline

- **Week 1:** ShaderGraphEditor, TerrainEditor, ContentBrowser (3 systems)
- **Week 2:** ScriptEditorPanel, StateMachineEditor, UIEditor (3 systems)
- **Week 3-4:** Material integration, Animation suite, Import panels (6 systems)
- **Week 5-6:** PanelRegistry, polish, testing (1 system + QA)

**Total: 6 weeks for complete integration**

## Current Blockers

None! All systems exist and are functional. This is purely **integration work** - connecting existing systems to the main editor.

## Resources

- [Editor Integration Plan](./EDITOR_INTEGRATION_PLAN.md) - Detailed phase-by-phase plan
- [UI Systems Catalog](./UI_SYSTEMS_CATALOG.md) - Complete list of all 65+ systems
- [ShaderGraph Documentation](../engine/materials/README.md) - Shader graph system docs
- [Animation System Documentation](../game/src/editor/animation/README.md) - Animation tools

## Quick Start Command

To begin integration, run:

```bash
# Start with ShaderGraphEditor (highest priority)
cd game/src/editor
# Edit Editor.hpp - add member variable
# Edit Editor.cpp - add initialization
# Test and commit
```

## Contact

For questions about integration:
- See existing code patterns in Editor.cpp
- Check similar panel integrations (ConfigEditor, WorldView)
- Refer to this document for standard patterns

---

**Last Updated:** 2025-11-29
**Status:** Ready to begin integration
**Priority:** HIGH - Content creation depends on these editors
