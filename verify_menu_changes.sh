#!/bin/bash
# Verification script for menu system changes

echo "=== StandaloneEditor Menu System Verification ==="
echo

cd "H:/Github/Old3DEngine/examples"

echo "Checking for new functions in StandaloneEditor.cpp..."
echo -n "  SelectAllObjects: "
grep -q "void StandaloneEditor::SelectAllObjects()" StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  CopySelectedObjects: "
grep -q "void StandaloneEditor::CopySelectedObjects()" StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  ImportHeightmap: "
grep -q "bool StandaloneEditor::ImportHeightmap" StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  ExportHeightmap: "
grep -q "bool StandaloneEditor::ExportHeightmap" StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  ClearRecentFiles: "
grep -q "void StandaloneEditor::ClearRecentFiles()" StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo
echo "Checking File menu changes..."
echo -n "  New submenu: "
grep -q 'if (ImGui::BeginMenu("New"))' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Import submenu: "
grep -q 'if (ImGui::BeginMenu("Import"))' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Export submenu: "
grep -q 'if (ImGui::BeginMenu("Export"))' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Recent Files submenu: "
grep -q 'if (ImGui::BeginMenu("Recent Files"))' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Exit calls Shutdown: "
grep -q 'Nova::Engine::Instance().Shutdown()' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo
echo "Checking Edit menu changes..."
echo -n "  Undo with CanUndo check: "
grep -q 'bool canUndo = m_commandHistory && m_commandHistory->CanUndo()' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Cut menu item: "
grep -q 'MenuItem("Cut", "Ctrl+X"' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Copy menu item: "
grep -q 'MenuItem("Copy", "Ctrl+C"' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Paste menu item: "
grep -q 'MenuItem("Paste", "Ctrl+V"' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Select All menu item: "
grep -q 'MenuItem("Select All", "Ctrl+A"' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Preferences menu item: "
grep -q 'MenuItem("Preferences..."' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo
echo "Checking keyboard shortcuts..."
echo -n "  File menu shortcuts: "
grep -q '// File menu shortcuts' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Ctrl+Z Undo: "
grep -q 'if (input.IsKeyPressed(Nova::Key::Z))' StandaloneEditor.cpp && grep -q 'm_commandHistory->Undo()' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  Ctrl+A Select All: "
grep -q 'if (input.IsKeyPressed(Nova::Key::A))' StandaloneEditor.cpp && grep -q 'SelectAllObjects()' StandaloneEditor.cpp && echo "✓ Found" || echo "✗ Missing"

echo
echo "Checking header file..."
echo -n "  SelectAllObjects declaration: "
grep -q 'void SelectAllObjects();' StandaloneEditor.hpp && echo "✓ Found" || echo "✗ Missing"

echo -n "  CopySelectedObjects declaration: "
grep -q 'void CopySelectedObjects();' StandaloneEditor.hpp && echo "✓ Found" || echo "✗ Missing"

echo
echo "=== Verification Complete ==="
