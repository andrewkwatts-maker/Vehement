# File Menu Polish - Quick Integration Guide

## Files Created
1. **StandaloneEditor_FileMenuImplementations.cpp** - All new function implementations
2. **FILE_MENU_POLISH_SUMMARY.md** - Detailed documentation
3. **INTEGRATION_GUIDE.md** - This file

## Step-by-Step Integration

### Step 1: Update Header File
**File:** `StandaloneEditor.hpp`

The header has already been updated with these function declarations:
```cpp
// Native file dialogs
std::string OpenNativeFileDialog(const char* filter, const char* title);
std::string SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt);

// Recent files management
void LoadRecentFiles();
void SaveRecentFiles();
void AddToRecentFiles(const std::string& path);
void ClearRecentFiles();

// Import/Export
bool ImportHeightmap(const std::string& path);
bool ExportHeightmap(const std::string& path);
```

✅ **Already done!**

### Step 2: Add Includes to StandaloneEditor.cpp
**Location:** Top of file, after existing includes

Add these lines after `#include <algorithm>`:
```cpp
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#endif
```

### Step 3: Copy Function Implementations
**Source:** `StandaloneEditor_FileMenuImplementations.cpp`
**Destination:** End of `StandaloneEditor.cpp`

Copy these complete function implementations:
1. `OpenNativeFileDialog()` (Windows + fallback)
2. `SaveNativeFileDialog()` (Windows + fallback)
3. `LoadRecentFiles()`
4. `SaveRecentFiles()`
5. `AddToRecentFiles()`
6. `ClearRecentFiles()`
7. `ShowNewMapDialog()` (enhanced version - REPLACE existing)
8. `LoadMap()` (complete version - REPLACE existing stub)
9. `SaveMap()` (complete version - REPLACE existing stub)
10. `ImportHeightmap()`
11. `ExportHeightmap()`

### Step 4: Update Initialize() Function
**Location:** `StandaloneEditor::Initialize()`

Add this line before the final `return true;`:
```cpp
// Load recent files
LoadRecentFiles();
```

### Step 5: Replace File Menu in RenderUI()
**Location:** `StandaloneEditor::RenderUI()`

**Find this section:**
```cpp
if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New Map", "Ctrl+N")) {
        m_showNewMapDialog = true;
    }
    // ... existing menu items ...
    ImGui::EndMenu();
}
```

**Replace with:** The enhanced File menu from `StandaloneEditor_FileMenuImplementations.cpp` (search for "// File Menu (replace existing File menu):")

### Step 6: Add Keyboard Shortcuts
**Location:** Beginning of `StandaloneEditor::RenderUI()` function

Add this code BEFORE the `if (ImGui::BeginMainMenuBar())` line:
```cpp
// Handle keyboard shortcuts
auto& input = Nova::Engine::Instance().GetInput();
if (!ImGui::GetIO().WantTextInput) {
    // Ctrl+N - New Map
    if (input.IsKeyDown(Nova::Key::LeftControl) && input.IsKeyPressed(Nova::Key::N)) {
        m_showNewMapDialog = true;
    }
    // Ctrl+O - Open
    if (input.IsKeyDown(Nova::Key::LeftControl) && input.IsKeyPressed(Nova::Key::O)) {
        std::string path = OpenNativeFileDialog("Map Files (*.map)\\0*.map\\0All Files (*.*)\\0*.*\\0", "Open Map");
        if (!path.empty()) {
            if (LoadMap(path)) {
                AddToRecentFiles(path);
            }
        }
    }
    // Ctrl+S - Save
    if (input.IsKeyDown(Nova::Key::LeftControl) && !input.IsKeyDown(Nova::Key::LeftShift) && input.IsKeyPressed(Nova::Key::S)) {
        if (!m_currentMapPath.empty()) {
            SaveMap(m_currentMapPath);
        } else {
            std::string path = SaveNativeFileDialog("Map Files (*.map)\\0*.map\\0All Files (*.*)\\0*.*\\0", "Save Map", "map");
            if (!path.empty()) {
                if (SaveMap(path)) {
                    AddToRecentFiles(path);
                }
            }
        }
    }
    // Ctrl+Shift+S - Save As
    if (input.IsKeyDown(Nova::Key::LeftControl) && input.IsKeyDown(Nova::Key::LeftShift) && input.IsKeyPressed(Nova::Key::S)) {
        std::string path = SaveNativeFileDialog("Map Files (*.map)\\0*.map\\0All Files (*.*)\\0*.*\\0", "Save Map As", "map");
        if (!path.empty()) {
            if (SaveMap(path)) {
                AddToRecentFiles(path);
            }
        }
    }
}
```

### Step 7: Create Config Directory
Create this directory structure:
```
Old3DEngine/
├── assets/
│   └── config/        <- Create this directory
```

The `editor.json` file will be created automatically on first use.

## Quick Test

After integration, test these features:

1. **Press Ctrl+N** - Should open New Map dialog with Flat/Spherical options
2. **Press Ctrl+O** - Should open Windows native file dialog
3. **Create and save a map** - Should create .map file
4. **Close and reopen editor** - Recent file should appear in menu
5. **Import > Heightmap** - Should open file dialog filtered to .png/.raw
6. **Export > Heightmap (RAW)** - Should save .raw file

## Troubleshooting

### Issue: Native file dialog doesn't appear
**Solution:** Make sure Windows headers are included:
```cpp
#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#endif
```

### Issue: Recent files don't persist
**Solution:** Check that `assets/config/` directory exists and is writable

### Issue: Map file won't load
**Solution:** Check console for error messages. Make sure file has "NOVA" magic number.

### Issue: Keyboard shortcuts don't work
**Solution:** Make sure shortcuts code is added BEFORE the menu bar rendering

## What's Different

### Before Integration
- File menu items were stubbed out
- LoadMap/SaveMap only logged warnings
- No native file dialogs
- No recent files
- No import/export
- No keyboard shortcuts

### After Integration
- All menu items functional
- Complete binary file I/O
- Windows native file dialogs
- Recent files with persistence (up to 10)
- RAW heightmap import/export
- 4 keyboard shortcuts (Ctrl+N/O/S/Shift+S)
- Flat vs Spherical world type selection

## Next Steps

After successful integration:
1. Test all File menu operations
2. Create sample .map files
3. Test import/export with RAW heightmaps
4. Verify recent files persistence
5. Consider adding PNG support (requires stb_image.h)

## Support

See **FILE_MENU_POLISH_SUMMARY.md** for:
- Detailed implementation notes
- File format specifications
- Known limitations
- Future enhancement ideas
- Complete testing checklist
