#!/bin/bash
# Script to apply menu system patches to StandaloneEditor.cpp

set -e

cd "H:/Github/Old3DEngine/examples"

echo "Creating backup..."
cp StandaloneEditor.cpp StandaloneEditor.cpp.before_patches

echo "Applying patches..."

# First, append the new functions if not already present
if ! grep -q "void StandaloneEditor::SelectAllObjects()" StandaloneEditor.cpp; then
    echo "Appending new functions..."
    cat StandaloneEditor_NewFunctions.cpp >> StandaloneEditor.cpp
fi

# Patch 1: Update Exit Editor menu item
sed -i 's/if (ImGui::MenuItem("Exit Editor", "Esc")) {/if (ImGui::MenuItem("Exit Editor", "Alt+F4")) {/' StandaloneEditor.cpp
sed -i 's|// Will be handled by RTSApplication|Nova::Engine::Instance().Shutdown();|' StandaloneEditor.cpp

echo "Patches applied successfully!"
echo "File backed up to: StandaloneEditor.cpp.before_patches"
