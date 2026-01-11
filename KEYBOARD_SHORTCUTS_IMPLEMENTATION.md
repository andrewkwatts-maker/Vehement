# Keyboard Shortcuts Implementation Summary

## Overview
This document provides a comprehensive reference for implementing keyboard shortcuts and controls dialog in StandaloneEditor.cpp.

## Current Keyboard Shortcuts Audit

### Edit Mode Shortcuts (only when !ImGui::GetIO().WantTextInput)
- **Q** - Object Select mode (sets EditMode::ObjectSelect, clears transform tool)
- **1** - Terrain Paint mode
- **2** - Terrain Sculpt mode

### Transform Tool Shortcuts (only in ObjectSelect mode)
- **W** - Toggle Move tool
- **E** - Toggle Rotate tool
- **R** - Toggle Scale tool

### Brush Shortcuts (only in TerrainPaint or TerrainSculpt mode)
- **[** - Decrease brush size (min 1)
- **]** - Increase brush size (max 20)

### Camera Controls
- **Left Arrow** - Rotate camera left (90°/sec)
- **Right Arrow** - Rotate camera right (90°/sec)
- **Page Up** - Zoom in (20 units/sec, min 5.0f)
- **Page Down** - Zoom out (20 units/sec, max 100.0f)

### Object Selection (only in ObjectSelect mode)
- **Escape** - Clear selection
- **Delete** - Delete selected objects
- **Left Mouse Button** - Select object at cursor (when !ImGui::GetIO().WantCaptureMouse)

### Menu Shortcuts (labels only, not implemented)
- **Ctrl+N** - New Map (shows in menu, not functional)
- **Ctrl+O** - Open Map (shows in menu, not functional)
- **Ctrl+S** - Save Map (shows in menu, not functional)
- **Ctrl+Shift+S** - Save Map As (shows in menu, not functional)
- **Ctrl+Z** - Undo (disabled in menu)
- **Ctrl+Y** - Redo (disabled in menu)

## Conflicts Found
**NONE** - All shortcuts are properly scoped and don't conflict.

## Implementation Required

### 1. Add Keyboard Shortcut Handling in Update()
Add this code after the existing brush size adjustment (line ~268), before object selection code:

```cpp
        // Global keyboard shortcuts (Ctrl combinations)
        bool ctrlDown = input.IsKeyDown(Nova::Key::LeftControl) || input.IsKeyDown(Nova::Key::RightControl);
        bool shiftDown = input.IsKeyDown(Nova::Key::LeftShift) || input.IsKeyDown(Nova::Key::RightShift);

        if (ctrlDown && !shiftDown) {
            // Ctrl+N - New Map
            if (input.IsKeyPressed(Nova::Key::N)) {
                m_showNewMapDialog = true;
            }
            // Ctrl+O - Open Map
            if (input.IsKeyPressed(Nova::Key::O)) {
                m_showLoadMapDialog = true;
            }
            // Ctrl+S - Save Map
            if (input.IsKeyPressed(Nova::Key::S)) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                } else {
                    m_showSaveMapDialog = true;
                }
            }
            // Ctrl+Z - Undo (placeholder for now)
            if (input.IsKeyPressed(Nova::Key::Z)) {
                // TODO: Implement undo
                spdlog::info("Undo requested (not yet implemented)");
            }
            // Ctrl+Y - Redo (placeholder for now)
            if (input.IsKeyPressed(Nova::Key::Y)) {
                // TODO: Implement redo
                spdlog::info("Redo requested (not yet implemented)");
            }
        }

        if (ctrlDown && shiftDown) {
            // Ctrl+Shift+S - Save As
            if (input.IsKeyPressed(Nova::Key::S)) {
                m_showSaveMapDialog = true;
            }
            // Ctrl+Shift+Z - Alternate Redo
            if (input.IsKeyPressed(Nova::Key::Z)) {
                // TODO: Implement redo
                spdlog::info("Redo requested (not yet implemented)");
            }
        }

        // F1 - Show Controls
        if (input.IsKeyPressed(Nova::Key::F1)) {
            m_showControlsDialog = true;
        }

        // F11 - Toggle Fullscreen (placeholder)
        if (input.IsKeyPressed(Nova::Key::F11)) {
            // TODO: Implement fullscreen toggle
            spdlog::info("Fullscreen toggle requested (not yet implemented)");
        }

        // Home - Reset camera
        if (input.IsKeyPressed(Nova::Key::Home)) {
            m_cameraDistance = 30.0f;
            m_cameraAngle = 45.0f;
        }
    }
```

### 2. Update Help Menu in RenderUI()
Replace the existing Help menu (line ~356) with:

```cpp
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Controls", "F1")) {
                m_showControlsDialog = true;
            }
            if (ImGui::MenuItem("About")) {
                m_showAboutDialog = true;
            }
            ImGui::EndMenu();
        }
```

### 3. Add ShowControlsDialog() Call in RenderUI()
Add after line ~410 (after ShowAboutDialog):

```cpp
    if (m_showControlsDialog) ShowControlsDialog();
```

### 4. Implement ShowControlsDialog()
Add this function after ShowAboutDialog() implementation (line ~986):

```cpp
void StandaloneEditor::ShowControlsDialog() {
    ImGui::OpenPopup("Editor Controls");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Editor Controls", &m_showControlsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Keyboard & Mouse Reference");
        ImGui::Separator();
        ImGui::Spacing();

        // Search box
        static char searchBuffer[256] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "Search shortcuts...", searchBuffer, 256);
        ImGui::Spacing();

        // Begin scrollable region
        ImGui::BeginChild("ControlsContent", ImVec2(0, 450), true);

        std::string searchStr = searchBuffer;
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

        // Helper lambda to check if text matches search
        auto matches = [&](const char* text) {
            if (searchStr.empty()) return true;
            std::string lowerText = text;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
            return lowerText.find(searchStr) != std::string::npos;
        };

        // File Operations
        if (searchStr.empty() || matches("file new open save")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("FILE OPERATIONS");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("new map")) ImGui::BulletText("Ctrl+N             New Map");
            if (matches("open map")) ImGui::BulletText("Ctrl+O             Open Map");
            if (matches("save map")) ImGui::BulletText("Ctrl+S             Save Map");
            if (matches("save as")) ImGui::BulletText("Ctrl+Shift+S       Save Map As");

            ImGui::Spacing();
        }

        // Edit Operations
        if (searchStr.empty() || matches("edit undo redo delete")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("EDIT OPERATIONS");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("undo")) ImGui::BulletText("Ctrl+Z             Undo (not yet implemented)");
            if (matches("redo")) ImGui::BulletText("Ctrl+Y             Redo (not yet implemented)");
            if (matches("redo")) ImGui::BulletText("Ctrl+Shift+Z       Redo (alternate)");
            if (matches("delete")) ImGui::BulletText("Delete             Delete Selected Object");
            if (matches("escape cancel")) ImGui::BulletText("Escape             Clear Selection / Cancel");

            ImGui::Spacing();
        }

        // Edit Modes
        if (searchStr.empty() || matches("mode select terrain paint sculpt")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("EDIT MODES");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("select object")) ImGui::BulletText("Q                  Object Select Mode");
            if (matches("paint terrain")) ImGui::BulletText("1                  Terrain Paint Mode");
            if (matches("sculpt terrain")) ImGui::BulletText("2                  Terrain Sculpt Mode");

            ImGui::Spacing();
        }

        // Transform Tools
        if (searchStr.empty() || matches("transform move rotate scale")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("TRANSFORM TOOLS (Object Select Mode)");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("move")) ImGui::BulletText("W                  Toggle Move Tool");
            if (matches("rotate")) ImGui::BulletText("E                  Toggle Rotate Tool");
            if (matches("scale")) ImGui::BulletText("R                  Toggle Scale Tool");

            ImGui::Spacing();
        }

        // Brush Controls
        if (searchStr.empty() || matches("brush size terrain paint sculpt")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("BRUSH CONTROLS (Terrain Paint/Sculpt)");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("brush size")) ImGui::BulletText("[                  Decrease Brush Size");
            if (matches("brush size")) ImGui::BulletText("]                  Increase Brush Size");

            ImGui::Spacing();
        }

        // Camera Controls
        if (searchStr.empty() || matches("camera rotate zoom view")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("CAMERA CONTROLS");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("rotate left")) ImGui::BulletText("Left Arrow         Rotate Camera Left");
            if (matches("rotate right")) ImGui::BulletText("Right Arrow        Rotate Camera Right");
            if (matches("zoom")) ImGui::BulletText("Page Up            Zoom In");
            if (matches("zoom")) ImGui::BulletText("Page Down          Zoom Out");
            if (matches("reset home")) ImGui::BulletText("Home               Reset Camera Position");
            if (matches("mouse scroll")) ImGui::BulletText("Mouse Scroll       Zoom (future)");
            if (matches("mouse middle")) ImGui::BulletText("Middle Mouse Drag  Pan Camera (future)");
            if (matches("mouse right")) ImGui::BulletText("Right Mouse Drag   Rotate Camera (future)");

            ImGui::Spacing();
        }

        // Selection
        if (searchStr.empty() || matches("select click mouse")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("SELECTION");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("select click")) ImGui::BulletText("Left Mouse Click   Select Object");
            if (matches("multi select")) ImGui::BulletText("Ctrl+Click         Add to Selection (future)");
            if (matches("clear select")) ImGui::BulletText("Escape             Clear Selection");

            ImGui::Spacing();
        }

        // View & Help
        if (searchStr.empty() || matches("help controls fullscreen view")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("VIEW & HELP");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (matches("help controls")) ImGui::BulletText("F1                 Show This Controls Dialog");
            if (matches("fullscreen")) ImGui::BulletText("F11                Toggle Fullscreen (not yet implemented)");

            ImGui::Spacing();
        }

        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showControlsDialog = false;
        }

        ImGui::EndPopup();
    }
}
```

### 5. Enhance ShowAboutDialog()
Replace the existing implementation (line ~966) with:

```cpp
void StandaloneEditor::ShowAboutDialog() {
    ImGui::OpenPopup("About Editor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About Editor", &m_showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("Nova3D RTS Editor");
        ImGui::PopStyleColor();
        ImGui::Text("Version 1.0.0");
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::Text("A standalone level editor for creating custom maps");
        ImGui::Text("and scenarios for real-time strategy games.");
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Text("FEATURES");
        ImGui::BulletText("Terrain painting and sculpting");
        ImGui::BulletText("Object placement and transformation");
        ImGui::BulletText("Material editor with PBR workflow");
        ImGui::BulletText("PCG-based procedural generation");
        ImGui::BulletText("World map and local map support");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("BUILT WITH");
        ImGui::BulletText("Nova3D Engine");
        ImGui::BulletText("Dear ImGui for UI");
        ImGui::BulletText("GLM for mathematics");
        ImGui::BulletText("spdlog for logging");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("CREDITS");
        ImGui::TextWrapped("Developed with Claude Code assistance");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("LICENSE");
        ImGui::TextWrapped("This software is provided as-is for educational purposes.");

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Documentation", ImVec2(120, 0))) {
            // TODO: Open documentation
            spdlog::info("Opening documentation (not yet implemented)");
        }
        ImGui::SameLine();
        if (ImGui::Button("GitHub", ImVec2(120, 0))) {
            // TODO: Open GitHub repository
            spdlog::info("Opening GitHub (not yet implemented)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showAboutDialog = false;
        }

        ImGui::EndPopup();
    }
}
```

## Complete Keyboard Shortcuts Reference

### File Operations
- Ctrl+N - New Map
- Ctrl+O - Open Map
- Ctrl+S - Save Map
- Ctrl+Shift+S - Save Map As

### Edit Operations
- Ctrl+Z - Undo
- Ctrl+Y / Ctrl+Shift+Z - Redo
- Delete - Delete Selected Objects
- Escape - Clear Selection / Cancel

### Edit Modes
- Q - Object Select Mode
- 1 - Terrain Paint Mode
- 2 - Terrain Sculpt Mode

### Transform Tools (Object Select Mode)
- W - Toggle Move Tool
- E - Toggle Rotate Tool
- R - Toggle Scale Tool

### Brush Controls (Terrain Paint/Sculpt)
- [ - Decrease Brush Size
- ] - Increase Brush Size

### Camera Controls
- Left/Right Arrow - Rotate Camera
- Page Up/Down - Zoom In/Out
- Home - Reset Camera Position
- Mouse Scroll - Zoom (planned)
- Middle Mouse Drag - Pan Camera (planned)
- Right Mouse Drag - Rotate Camera (planned)

### Selection
- Left Mouse Click - Select Object
- Ctrl+Click - Add to Selection (planned)
- Escape - Clear Selection

### View & Help
- F1 - Show Controls Dialog
- F11 - Toggle Fullscreen (planned)

## Notes
- All Ctrl+key shortcuts check for Ctrl modifier to avoid conflicts with text input
- Shortcuts respect ImGui::GetIO().WantTextInput to avoid triggering while typing
- Camera and mouse shortcuts respect ImGui::GetIO().WantCaptureMouse
- Transform tool shortcuts only work in ObjectSelect mode
- Brush shortcuts only work in TerrainPaint or TerrainSculpt modes
- No conflicts between shortcuts - all are properly scoped
