#!/usr/bin/env python3
"""Patch the StandaloneEditor.cpp menu system"""

import re

# Read the file
with open('StandaloneEditor.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# ==============================================================================
# Patch 1: File Menu
# ==============================================================================
file_menu_old = r'''        if \(ImGui::BeginMenu\("File"\)\) \{
            if \(ImGui::MenuItem\("New Map", "Ctrl\+N"\)\) \{
                m_showNewMapDialog = true;
            \}
            if \(ImGui::MenuItem\("Open Map", "Ctrl\+O"\)\) \{
                m_showLoadMapDialog = true;
            \}
            if \(ImGui::MenuItem\("Save Map", "Ctrl\+S"\)\) \{
                if \(!m_currentMapPath\.empty\(\)\) \{
                    SaveMap\(m_currentMapPath\);
                \} else \{
                    m_showSaveMapDialog = true;
                \}
            \}
            if \(ImGui::MenuItem\("Save Map As", "Ctrl\+Shift\+S"\)\) \{
                m_showSaveMapDialog = true;
            \}
            ImGui::Separator\(\);
            if \(ImGui::MenuItem\("Exit Editor", "Esc"\)\) \{
                // Will be handled by RTSApplication
            \}
            ImGui::EndMenu\(\);
        \}'''

file_menu_new = '''        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("New")) {
                if (ImGui::MenuItem("World Map", "Ctrl+Shift+N")) {
                    NewWorldMap();
                }
                if (ImGui::MenuItem("Local Map", "Ctrl+N")) {
                    m_showNewMapDialog = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Open Map", "Ctrl+O")) {
                m_showLoadMapDialog = true;
            }
            if (ImGui::MenuItem("Save Map", "Ctrl+S")) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                } else {
                    m_showSaveMapDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Map As", "Ctrl+Shift+S")) {
                m_showSaveMapDialog = true;
            }
            ImGui::Separator();

            // Import/Export submenu
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = OpenNativeFileDialog("Image Files (*.png;*.jpg;*.tga;*.bmp)\\0*.png;*.jpg;*.tga;*.bmp\\0All Files\\0*.*\\0", "Import Heightmap");
                    if (!path.empty()) {
                        ImportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = SaveNativeFileDialog("PNG Image (*.png)\\0*.png\\0All Files\\0*.*\\0", "Export Heightmap", ".png");
                    if (!path.empty()) {
                        ExportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            // Recent files
            if (ImGui::BeginMenu("Recent Files")) {
                if (m_recentFiles.empty()) {
                    ImGui::MenuItem("(No recent files)", nullptr, false, false);
                } else {
                    for (const auto& recentFile : m_recentFiles) {
                        if (ImGui::MenuItem(recentFile.c_str())) {
                            LoadMap(recentFile);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        ClearRecentFiles();
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit Editor", "Alt+F4")) {
                Nova::Engine::Instance().Shutdown();
            }
            ImGui::EndMenu();
        }'''

# Replace File menu
content = re.sub(file_menu_old, file_menu_new, content, flags=re.DOTALL)

# ==============================================================================
# Patch 2: Edit Menu
# ==============================================================================
edit_menu_old = r'''        if \(ImGui::BeginMenu\("Edit"\)\) \{
            if \(ImGui::MenuItem\("Undo", "Ctrl\+Z", false, false\)\) \{\}
            if \(ImGui::MenuItem\("Redo", "Ctrl\+Y", false, false\)\) \{\}
            ImGui::Separator\(\);
            if \(ImGui::MenuItem\("Map Properties"\)\) \{\}
            ImGui::EndMenu\(\);
        \}'''

edit_menu_new = '''        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = m_commandHistory && m_commandHistory->CanUndo();
            bool canRedo = m_commandHistory && m_commandHistory->CanRedo();

            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                if (m_commandHistory) {
                    m_commandHistory->Undo();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
                if (m_commandHistory) {
                    m_commandHistory->Redo();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
                DeleteSelectedObjects();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {
                // Clipboard paste not implemented yet
                spdlog::warn("Paste not yet implemented");
            }
            if (ImGui::MenuItem("Delete", "Del", false, m_selectedObjectIndex >= 0)) {
                DeleteSelectedObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                SelectAllObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Map Properties")) {
                m_showMapPropertiesDialog = true;
            }
            if (ImGui::MenuItem("Preferences...")) {
                m_showSettingsDialog = true;
            }
            ImGui::EndMenu();
        }'''

# Replace Edit menu
content = re.sub(edit_menu_old, edit_menu_new, content, flags=re.DOTALL)

# Write the patched file
with open('StandaloneEditor.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("File menu patched successfully")
print("Edit menu patched successfully")
