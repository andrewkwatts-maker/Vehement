#!/usr/bin/env python3
"""Insert keyboard shortcuts into Update function"""

# Read file
with open('StandaloneEditor.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find the line with F1 shortcut (around line 244-246)
insert_index = -1
for i, line in enumerate(lines):
    if 'if (input.IsKeyPressed(Nova::Key::F1))' in line:
        # Find the closing brace of this if statement
        for j in range(i+1, min(i+10, len(lines))):
            if '}' in lines[j] and 'm_showControlsDialog' in lines[j-1]:
                insert_index = j + 1
                break
        break

if insert_index == -1:
    print("Error: Could not find insertion point")
    exit(1)

print(f"Inserting keyboard shortcuts at line {insert_index}")

# Keyboard shortcuts to insert
shortcuts = '''
        // File menu shortcuts
        if (input.IsKeyDown(Nova::Key::LeftControl) || input.IsKeyDown(Nova::Key::RightControl)) {
            if (input.IsKeyDown(Nova::Key::LeftShift) || input.IsKeyDown(Nova::Key::RightShift)) {
                // Ctrl+Shift+N - New World Map
                if (input.IsKeyPressed(Nova::Key::N)) {
                    NewWorldMap();
                }
                // Ctrl+Shift+S - Save As
                if (input.IsKeyPressed(Nova::Key::S)) {
                    m_showSaveMapDialog = true;
                }
            } else {
                // Ctrl+N - New Local Map
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

                // Edit menu shortcuts
                // Ctrl+Z - Undo
                if (input.IsKeyPressed(Nova::Key::Z)) {
                    if (m_commandHistory && m_commandHistory->CanUndo()) {
                        m_commandHistory->Undo();
                    }
                }
                // Ctrl+Y - Redo
                if (input.IsKeyPressed(Nova::Key::Y)) {
                    if (m_commandHistory && m_commandHistory->CanRedo()) {
                        m_commandHistory->Redo();
                    }
                }
                // Ctrl+X - Cut
                if (input.IsKeyPressed(Nova::Key::X)) {
                    if (m_selectedObjectIndex >= 0) {
                        CopySelectedObjects();
                        DeleteSelectedObjects();
                    }
                }
                // Ctrl+C - Copy
                if (input.IsKeyPressed(Nova::Key::C)) {
                    if (m_selectedObjectIndex >= 0) {
                        CopySelectedObjects();
                    }
                }
                // Ctrl+V - Paste
                if (input.IsKeyPressed(Nova::Key::V)) {
                    spdlog::warn("Paste not yet implemented");
                }
                // Ctrl+A - Select All
                if (input.IsKeyPressed(Nova::Key::A)) {
                    SelectAllObjects();
                }
            }
        }
'''

# Insert the shortcuts
lines.insert(insert_index, shortcuts)

# Write back
with open('StandaloneEditor.cpp', 'w', encoding='utf-8') as f:
    f.writelines(lines)

print("Keyboard shortcuts inserted successfully!")
