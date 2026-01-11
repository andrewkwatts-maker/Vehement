# Implementation Patches for Editor Integration

This file contains the exact code changes needed to integrate the EditorIntegration system into StandaloneEditor.

## Files Created

1. **StandaloneEditorIntegration.hpp** - Already created
2. **StandaloneEditorIntegration.cpp** - Already created
3. **EDITOR_INTEGRATION_GUIDE.md** - Comprehensive documentation

## Patches Required

### Patch 1: StandaloneEditor.hpp

Add after line 383 (after `m_commandHistory` declaration):

```cpp
    // Editor integration for opening assets in specialized editors
    std::unique_ptr<class EditorIntegration> m_editorIntegration;
```

Add at the top with other includes (after line 7):

```cpp
#include "StandaloneEditorIntegration.hpp"
```

### Patch 2: StandaloneEditor.cpp - Includes

Add to includes section at the top:

```cpp
#include "StandaloneEditorIntegration.hpp"
```

### Patch 3: StandaloneEditor.cpp - Initialize()

In the `Initialize()` method, add before the `m_initialized = true;` line (around line 62):

```cpp
    // Initialize editor integration
    m_editorIntegration = std::make_unique<EditorIntegration>();
    if (!m_editorIntegration->Initialize(nullptr)) {
        spdlog::warn("EditorIntegration initialization failed, but continuing");
    }
```

### Patch 4: StandaloneEditor.cpp - Shutdown()

In the `Shutdown()` method, add before `m_initialized = false;` (around line 216):

```cpp
    if (m_editorIntegration) {
        m_editorIntegration->Shutdown();
    }
```

### Patch 5: StandaloneEditor.cpp - Update()

At the end of the `Update()` method, add before the closing brace (around line 413):

```cpp
    // Update all open specialized editors
    if (m_editorIntegration) {
        m_editorIntegration->Update(deltaTime);
    }
```

### Patch 6: StandaloneEditor.cpp - RenderUI()

In the `RenderUI()` method, add before the dialogs section (around line 452):

```cpp
    // Render all open specialized editors
    if (m_editorIntegration) {
        m_editorIntegration->RenderUI();
    }
```

### Patch 7: StandaloneEditor.cpp - Asset Browser Context Menu

Replace the context menu section in `RenderUnifiedContentBrowser()` (lines 2162-2187) with:

```cpp
    // Helper function for asset type names (add at file scope or as static method)
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

        // Display detected asset type and allow explicit editor selection
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
            if (ImGui::MenuItem("Talent Tree Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::TalentTree);
            }
            if (ImGui::MenuItem("State Machine Editor")) {
                m_editorIntegration->OpenAssetInEditor(contextMenuPath, AssetType::StateMachine);
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename")) {
            // Extract filename from path
            size_t lastSlash = contextMenuPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                contextMenuPath.substr(lastSlash + 1) : contextMenuPath;
            strncpy(renameBuffer, filename.c_str(), sizeof(renameBuffer) - 1);
            showRenamePopup = true;
        }

        if (ImGui::MenuItem("Delete")) {
            if (m_assetBrowser->DeleteAsset(contextMenuPath)) {
                spdlog::info("Deleted asset: {}", contextMenuPath);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh")) {
            m_assetBrowser->Refresh();
        }

        ImGui::EndPopup();
    }
```

## CMakeLists.txt Changes

If you have a CMakeLists.txt for the examples, add:

```cmake
# Add new editor integration files
set(STANDALONE_EDITOR_SOURCES
    examples/StandaloneEditor.cpp
    examples/StandaloneEditor.hpp
    examples/StandaloneEditorIntegration.cpp
    examples/StandaloneEditorIntegration.hpp
    # ... other existing files ...
)

# Ensure game editor headers are in include path
target_include_directories(StandaloneEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/game/src
    ${CMAKE_SOURCE_DIR}/game/src/editor
)
```

## Verification Steps

After applying patches:

1. **Build the project:**
   ```bash
   cd build
   cmake --build . --target StandaloneEditor
   ```

2. **Run and test:**
   - Launch StandaloneEditor
   - Navigate to Content Browser
   - Right-click any asset file
   - Verify "Open in Editor" appears in context menu
   - Test opening various asset types

3. **Check logs:**
   - Look for "EditorIntegration initialized" message
   - Verify editor creation messages
   - Check for any error messages

## Minimal Testing Script

Create a test directory structure:

```
assets/
  configs/
    test_unit.json
  models/
    test_model.sdf
  maps/
    test_map.json
  campaigns/
    test_campaign.json
  scripts/
    test_script.lua
  terrain/
    test_terrain.heightmap
```

Test each file type:
1. Right-click test_unit.json → Should open ConfigEditor
2. Right-click test_model.sdf → Should open SDFModelEditor
3. Right-click test_map.json → Should open MapEditor
4. Right-click test_campaign.json → Should open CampaignEditor
5. Right-click test_script.lua → Should open ScriptEditor
6. Right-click test_terrain.heightmap → Should open TerrainEditor

## Rollback Procedure

If issues occur, revert changes:

```bash
# Revert StandaloneEditor changes
git checkout HEAD -- examples/StandaloneEditor.hpp examples/StandaloneEditor.cpp

# Remove new files if needed
rm examples/StandaloneEditorIntegration.hpp
rm examples/StandaloneEditorIntegration.cpp

# Rebuild
cmake --build . --target StandaloneEditor
```

## Known Limitations

1. Some editors may require additional initialization parameters
2. Not all editors have full RenderUI() implementations
3. Some editors depend on InGameEditor parent class
4. Asset format validation is minimal
5. No unsaved changes tracking yet

## Next Steps

1. Test each editor type thoroughly
2. Implement full IAssetEditor interface
3. Add unsaved changes tracking
4. Implement asset preview tooltips
5. Add recent files menu
6. Create unit tests for EditorIntegration
7. Add user preferences for default editors
8. Implement hot reload for modified assets

## Support

For issues or questions:
- Check EDITOR_INTEGRATION_GUIDE.md for detailed documentation
- Review spdlog output for error messages
- Verify all editor headers compile correctly
- Check that editor initialization methods exist
