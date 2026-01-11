# Quick Start: Editor Integration

This guide will get you up and running with the editor integration in under 10 minutes.

## Prerequisites

- StandaloneEditor project compiles successfully
- All editor headers in `game/src/editor/` are accessible
- Basic familiarity with CMake and C++

## Step-by-Step Integration

### Step 1: Verify New Files Exist (Already Created)

Check that these files exist:
- ✅ `examples/StandaloneEditorIntegration.hpp`
- ✅ `examples/StandaloneEditorIntegration.cpp`
- ✅ `examples/EDITOR_INTEGRATION_GUIDE.md`
- ✅ `examples/IMPLEMENTATION_PATCHES.md`
- ✅ `examples/INTEGRATION_SUMMARY.md`

### Step 2: Add Include to StandaloneEditor.hpp

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.hpp`

**Location:** After line 7 (after `#include <imgui.h>`)

**Add:**
```cpp
#include "StandaloneEditorIntegration.hpp"
```

### Step 3: Add Member Variable to StandaloneEditor.hpp

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.hpp`

**Location:** After line 383 (after `std::unique_ptr<CommandHistory> m_commandHistory;`)

**Add:**
```cpp
    // Editor integration for opening assets in specialized editors
    std::unique_ptr<EditorIntegration> m_editorIntegration;
```

### Step 4: Add Include to StandaloneEditor.cpp

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** In the includes section at top (after other includes)

**Add:**
```cpp
#include "StandaloneEditorIntegration.hpp"
```

### Step 5: Initialize in StandaloneEditor::Initialize()

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** In `Initialize()` method, before `m_initialized = true;` (around line 60)

**Add:**
```cpp
    // Initialize editor integration
    m_editorIntegration = std::make_unique<EditorIntegration>();
    if (!m_editorIntegration->Initialize(nullptr)) {
        spdlog::warn("EditorIntegration initialization failed, but continuing");
    }
```

### Step 6: Shutdown in StandaloneEditor::Shutdown()

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** In `Shutdown()` method, before `m_initialized = false;` (around line 215)

**Add:**
```cpp
    if (m_editorIntegration) {
        m_editorIntegration->Shutdown();
    }
```

### Step 7: Update in StandaloneEditor::Update()

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** At end of `Update()` method (around line 410)

**Add:**
```cpp
    // Update all open specialized editors
    if (m_editorIntegration) {
        m_editorIntegration->Update(deltaTime);
    }
```

### Step 8: Render in StandaloneEditor::RenderUI()

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** In `RenderUI()` method, before dialogs section (around line 450)

**Add:**
```cpp
    // Render all open specialized editors
    if (m_editorIntegration) {
        m_editorIntegration->RenderUI();
    }
```

### Step 9: Add Helper Function

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** Add near the top of the file (after includes, before class methods)

**Add:**
```cpp
// Helper function for displaying asset type names
static const char* GetAssetTypeName(AssetType type) {
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

### Step 10: Update Context Menu

**File:** `H:/Github/Old3DEngine/examples/StandaloneEditor.cpp`

**Location:** In `RenderUnifiedContentBrowser()` method, find the context menu section (around line 2162)

**Replace the section from:**
```cpp
    if (ImGui::BeginPopup("AssetContextMenu")) {
        ImGui::Text("Asset: %s", contextMenuPath.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Rename")) {
```

**With:**
```cpp
    if (ImGui::BeginPopup("AssetContextMenu")) {
        ImGui::Text("Asset: %s", contextMenuPath.c_str());
        ImGui::Separator();

        // Open in Editor menu item
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

        // Open With submenu
        if (m_editorIntegration && ImGui::BeginMenu("Open With...")) {
            AssetType detectedType = EditorIntegration::DetectAssetType(contextMenuPath);
            ImGui::Text("Detected: %s", GetAssetTypeName(detectedType));
            ImGui::Separator();

            if (ImGui::MenuItem("Config Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Config);
            }
            if (ImGui::MenuItem("SDF Model Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::SDFModel);
            }
            if (ImGui::MenuItem("Map Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Map);
            }
            if (ImGui::MenuItem("Campaign Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Campaign);
            }
            if (ImGui::MenuItem("Terrain Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Terrain);
            }
            if (ImGui::MenuItem("Script Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::Script);
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename")) {
```

### Step 11: Build

```bash
cd H:/Github/Old3DEngine/build
cmake --build . --target StandaloneEditor
```

### Step 12: Test

1. Run StandaloneEditor
2. Navigate to Content Browser
3. Right-click any asset file
4. Verify "Open in Editor" appears
5. Click it and verify appropriate editor opens

## Verification

After building, check the console output for:
```
[info] EditorIntegration initialized
```

When opening an asset:
```
[info] Opened asset in editor: assets/...
[info] ConfigEditor created
```

## Troubleshooting

### Build Errors

**Error:** "EditorIntegration.hpp not found"
- **Fix:** Verify file exists in `examples/` directory
- **Fix:** Check include paths in CMakeLists.txt

**Error:** "Undefined reference to EditorIntegration::..."
- **Fix:** Add `StandaloneEditorIntegration.cpp` to CMakeLists.txt sources
- **Fix:** Rebuild completely: `cmake --build . --clean-first`

**Error:** "ConfigEditor.hpp not found"
- **Fix:** Add `game/src` to include directories in CMakeLists.txt
- **Fix:** Verify all editor headers exist

### Runtime Errors

**Error:** "EditorIntegration not initialized"
- **Fix:** Check that Initialize() is called in StandaloneEditor::Initialize()
- **Fix:** Verify `m_editorIntegration` is created before use

**Error:** Context menu doesn't appear
- **Fix:** Check that context menu code is in RenderUnifiedContentBrowser()
- **Fix:** Verify right-click detection logic

**Error:** Wrong editor opens
- **Fix:** Review DetectAssetType() logic in StandaloneEditorIntegration.cpp
- **Fix:** Use "Open With..." submenu to force specific editor

### No Editors Opening

1. Check console for error messages
2. Verify `m_editorIntegration` is not null
3. Add debug logging to OpenAssetInEditor()
4. Verify asset file exists and is readable

## Quick Test Cases

Create test files:

```bash
# Create test directory
mkdir -p assets/test

# Create test files
echo '{}' > assets/test/test_config.json
echo '{}' > assets/test/test_map.json
echo '{}' > assets/test/test_campaign.json
echo '{}' > assets/test/test.sdf
echo 'print("test")' > assets/test/test.lua
```

Test each:
- Right-click `test_config.json` → Should open ConfigEditor
- Right-click `test_map.json` → Should open MapEditor
- Right-click `test_campaign.json` → Should open CampaignEditor
- Right-click `test.sdf` → Should open SDFModelEditor
- Right-click `test.lua` → Should open ScriptEditor

## Success Indicators

✅ Build completes without errors
✅ "EditorIntegration initialized" in console
✅ Right-click shows context menu with "Open in Editor"
✅ Clicking menu item opens editor window
✅ Editor window appears and is dockable
✅ Close button on editor window works
✅ No crashes during normal operation

## Performance Check

- Startup should be fast (< 2 seconds)
- Opening first editor may have brief delay (lazy load)
- Subsequent editor openings should be instant
- Multiple open editors should not lag

## Next Steps

After successful integration:

1. **Test thoroughly** with real assets
2. **Create sample assets** for each editor type
3. **Report any issues** found during testing
4. **Read full documentation** in EDITOR_INTEGRATION_GUIDE.md
5. **Implement enhancements** as needed

## Rollback

If problems occur:

```bash
cd H:/Github/Old3DEngine
git checkout -- examples/StandaloneEditor.hpp examples/StandaloneEditor.cpp
git clean -fd examples/StandaloneEditorIntegration.*
cd build
cmake --build . --target StandaloneEditor
```

## Support

- Full documentation: `EDITOR_INTEGRATION_GUIDE.md`
- Implementation patches: `IMPLEMENTATION_PATCHES.md`
- Summary: `INTEGRATION_SUMMARY.md`
- Console logs: Check spdlog output

## Estimated Time

- Reading this guide: 5 minutes
- Making code changes: 10 minutes
- Building: 2-5 minutes
- Testing: 5 minutes
- **Total: ~20-25 minutes**

## Final Checklist

- [ ] All new files exist
- [ ] Include added to StandaloneEditor.hpp
- [ ] Member variable added to StandaloneEditor.hpp
- [ ] Include added to StandaloneEditor.cpp
- [ ] Initialize() updated
- [ ] Shutdown() updated
- [ ] Update() updated
- [ ] RenderUI() updated
- [ ] Helper function added
- [ ] Context menu updated
- [ ] Project builds successfully
- [ ] Context menu appears on right-click
- [ ] "Open in Editor" option visible
- [ ] Clicking opens editor window
- [ ] No console errors

## Done!

You should now have a fully functional editor integration system. Users can right-click any asset and open it in the appropriate specialized editor.

Enjoy your new unified editing environment! 🎉
