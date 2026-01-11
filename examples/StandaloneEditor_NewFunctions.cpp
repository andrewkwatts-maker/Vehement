// Additional menu action functions to be appended to StandaloneEditor.cpp

// ============================================================================
// Recent Files Management
// ============================================================================

void StandaloneEditor::LoadRecentFiles() {
    // TODO: Load recent files from settings/config file
    spdlog::debug("LoadRecentFiles called (not yet implemented)");
}

void StandaloneEditor::SaveRecentFiles() {
    // TODO: Save recent files to settings/config file
    spdlog::debug("SaveRecentFiles called (not yet implemented)");
}

void StandaloneEditor::AddToRecentFiles(const std::string& path) {
    // Remove if already exists
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), path);

    // Limit to 10 recent files
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }

    SaveRecentFiles();
    spdlog::info("Added to recent files: {}", path);
}

void StandaloneEditor::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
    spdlog::info("Recent files list cleared");
}

// ============================================================================
// Import/Export Functions
// ============================================================================

bool StandaloneEditor::ImportHeightmap(const std::string& path) {
    spdlog::info("Importing heightmap from: {}", path);
    // TODO: Implement heightmap import
    // - Load image file (PNG/JPG/TGA/BMP)
    // - Extract height values from grayscale or R channel
    // - Resize terrain if needed or scale heightmap
    // - Apply heights to m_terrainHeights
    // - Mark terrain mesh as dirty
    spdlog::warn("Heightmap import not yet fully implemented");
    return false;
}

bool StandaloneEditor::ExportHeightmap(const std::string& path) {
    spdlog::info("Exporting heightmap to: {}", path);
    // TODO: Implement heightmap export
    // - Create grayscale image from m_terrainHeights
    // - Normalize height values to 0-255 range
    // - Save as PNG file
    spdlog::warn("Heightmap export not yet fully implemented");
    return false;
}

// ============================================================================
// Selection and Clipboard Operations
// ============================================================================

void StandaloneEditor::SelectAllObjects() {
    spdlog::info("Select All Objects");
    m_selectedObjectIndices.clear();

    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        m_selectedObjectIndices.push_back(static_cast<int>(i));
    }

    m_isMultiSelectMode = true;

    if (!m_sceneObjects.empty()) {
        m_selectedObjectIndex = 0;
        auto& obj = m_sceneObjects[0];
        m_selectedObjectPosition = obj.position;
        m_selectedObjectRotation = obj.rotation;
        m_selectedObjectScale = obj.scale;
    }

    spdlog::info("Selected {} objects", m_selectedObjectIndices.size());
}

void StandaloneEditor::CopySelectedObjects() {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        spdlog::warn("No object selected for copy");
        return;
    }

    // TODO: Implement clipboard functionality
    // For now, just log the action
    spdlog::info("Copy operation: Selected object at index {}", m_selectedObjectIndex);
    spdlog::warn("Clipboard copy not yet fully implemented");
}
