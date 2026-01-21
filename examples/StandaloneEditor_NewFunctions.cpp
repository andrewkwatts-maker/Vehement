// Additional menu action functions to be appended to StandaloneEditor.cpp

// ============================================================================
// Recent Files Management
// ============================================================================

void StandaloneEditor::LoadRecentFiles() {
    std::string configPath = "assets/config/editor.json";
    std::ifstream file(configPath);

    if (!file.is_open()) {
        spdlog::info("No editor config found at {}, starting with empty recent files", configPath);
        return;
    }

    m_recentFiles.clear();
    std::string line;
    bool inRecentFiles = false;

    while (std::getline(file, line)) {
        // Simple JSON parsing - look for "recentFiles" array
        if (line.find("\"recentFiles\"") != std::string::npos) {
            inRecentFiles = true;
            continue;
        }

        if (inRecentFiles) {
            if (line.find("]") != std::string::npos) {
                break;
            }

            // Extract file path from JSON string
            size_t start = line.find('"');
            size_t end = line.rfind('"');
            if (start != std::string::npos && end != std::string::npos && start != end) {
                std::string path = line.substr(start + 1, end - start - 1);
                if (!path.empty() && std::filesystem::exists(path)) {
                    m_recentFiles.push_back(path);
                }
            }
        }
    }

    file.close();
    spdlog::info("Loaded {} recent files from config", m_recentFiles.size());
}

void StandaloneEditor::SaveRecentFiles() {
    std::string configDir = "assets/config";
    std::string configPath = configDir + "/editor.json";

    // Ensure config directory exists
    std::filesystem::create_directories(configDir);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        spdlog::error("Failed to save recent files to {}", configPath);
        return;
    }

    // Write simple JSON structure
    file << "{\n";
    file << "  \"recentFiles\": [\n";

    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        // Escape backslashes for JSON
        std::string escapedPath = m_recentFiles[i];
        size_t pos = 0;
        while ((pos = escapedPath.find('\\', pos)) != std::string::npos) {
            escapedPath.replace(pos, 1, "\\\\");
            pos += 2;
        }
        file << "    \"" << escapedPath << "\"";
        if (i < m_recentFiles.size() - 1) {
            file << ",";
        }
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
    spdlog::info("Saved {} recent files to config", m_recentFiles.size());
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

    if (!std::filesystem::exists(path)) {
        spdlog::error("Heightmap file does not exist: {}", path);
        return false;
    }

    // Load image using stb_image
    int imgWidth, imgHeight, channels;
    unsigned char* imageData = stbi_load(path.c_str(), &imgWidth, &imgHeight, &channels, 1);

    if (!imageData) {
        spdlog::error("Failed to load heightmap image: {}", stbi_failure_reason());
        return false;
    }

    spdlog::info("Loaded heightmap: {}x{}, {} channels", imgWidth, imgHeight, channels);

    // Resize terrain to match heightmap dimensions or resample heightmap to terrain size
    bool resizeMap = false;
    if (imgWidth != m_mapWidth || imgHeight != m_mapHeight) {
        spdlog::info("Heightmap size ({}x{}) differs from map size ({}x{})",
                     imgWidth, imgHeight, m_mapWidth, m_mapHeight);

        // Use heightmap dimensions if terrain is empty or small
        if (m_terrainHeights.empty() || (m_mapWidth <= 64 && m_mapHeight <= 64)) {
            m_mapWidth = imgWidth;
            m_mapHeight = imgHeight;
            resizeMap = true;
            spdlog::info("Resizing map to match heightmap: {}x{}", m_mapWidth, m_mapHeight);
        }
    }

    // Resize terrain arrays
    size_t terrainSize = static_cast<size_t>(m_mapWidth * m_mapHeight);
    m_terrainHeights.resize(terrainSize);
    m_terrainTiles.resize(terrainSize);

    // Calculate height range
    float heightRange = m_maxHeight - m_minHeight;

    // Apply heightmap data to terrain
    for (int y = 0; y < m_mapHeight; ++y) {
        for (int x = 0; x < m_mapWidth; ++x) {
            // Sample from image with bilinear interpolation if sizes differ
            float u = static_cast<float>(x) / (m_mapWidth - 1);
            float v = static_cast<float>(y) / (m_mapHeight - 1);

            int srcX = static_cast<int>(u * (imgWidth - 1));
            int srcY = static_cast<int>(v * (imgHeight - 1));

            srcX = std::clamp(srcX, 0, imgWidth - 1);
            srcY = std::clamp(srcY, 0, imgHeight - 1);

            // Get grayscale value (0-255) and normalize to 0-1
            unsigned char pixel = imageData[srcY * imgWidth + srcX];
            float normalizedHeight = static_cast<float>(pixel) / 255.0f;

            // Scale to world height range
            float worldHeight = m_minHeight + normalizedHeight * heightRange;

            // Store in terrain array
            int terrainIndex = y * m_mapWidth + x;
            m_terrainHeights[terrainIndex] = worldHeight;
        }
    }

    stbi_image_free(imageData);

    // Mark terrain mesh as dirty for regeneration
    m_terrainMeshDirty = true;

    spdlog::info("Heightmap imported successfully. Height range: {} to {}",
                 m_minHeight, m_maxHeight);
    return true;
}

bool StandaloneEditor::ExportHeightmap(const std::string& path) {
    spdlog::info("Exporting heightmap to: {}", path);

    if (m_terrainHeights.empty()) {
        spdlog::error("No terrain data to export");
        return false;
    }

    if (m_mapWidth <= 0 || m_mapHeight <= 0) {
        spdlog::error("Invalid terrain dimensions: {}x{}", m_mapWidth, m_mapHeight);
        return false;
    }

    // Calculate actual min/max heights in terrain
    float minH = std::numeric_limits<float>::max();
    float maxH = std::numeric_limits<float>::lowest();

    for (float h : m_terrainHeights) {
        minH = std::min(minH, h);
        maxH = std::max(maxH, h);
    }

    // Handle flat terrain case
    float heightRange = maxH - minH;
    if (heightRange < 0.001f) {
        heightRange = 1.0f;
        minH = 0.0f;
        spdlog::info("Terrain is flat, using default range");
    }

    spdlog::info("Terrain height range: {} to {}", minH, maxH);

    // Create grayscale image data
    std::vector<unsigned char> imageData(m_mapWidth * m_mapHeight);

    for (int y = 0; y < m_mapHeight; ++y) {
        for (int x = 0; x < m_mapWidth; ++x) {
            int index = y * m_mapWidth + x;
            float height = m_terrainHeights[index];

            // Normalize to 0-1 range
            float normalized = (height - minH) / heightRange;
            normalized = std::clamp(normalized, 0.0f, 1.0f);

            // Convert to 0-255
            imageData[index] = static_cast<unsigned char>(normalized * 255.0f);
        }
    }

    // Determine output format from extension
    std::filesystem::path filePath(path);
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool success = false;

    if (ext == ".png") {
        success = stbi_write_png(path.c_str(), m_mapWidth, m_mapHeight, 1,
                                 imageData.data(), m_mapWidth) != 0;
    } else if (ext == ".bmp") {
        success = stbi_write_bmp(path.c_str(), m_mapWidth, m_mapHeight, 1,
                                 imageData.data()) != 0;
    } else if (ext == ".tga") {
        success = stbi_write_tga(path.c_str(), m_mapWidth, m_mapHeight, 1,
                                 imageData.data()) != 0;
    } else if (ext == ".jpg" || ext == ".jpeg") {
        success = stbi_write_jpg(path.c_str(), m_mapWidth, m_mapHeight, 1,
                                 imageData.data(), 95) != 0;
    } else {
        // Default to PNG
        std::string pngPath = path + ".png";
        success = stbi_write_png(pngPath.c_str(), m_mapWidth, m_mapHeight, 1,
                                 imageData.data(), m_mapWidth) != 0;
        if (success) {
            spdlog::info("Saved as PNG (unrecognized extension): {}", pngPath);
        }
    }

    if (success) {
        spdlog::info("Heightmap exported successfully: {}x{} pixels", m_mapWidth, m_mapHeight);
    } else {
        spdlog::error("Failed to write heightmap image to: {}", path);
    }

    return success;
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

// Static clipboard storage for scene objects
static std::vector<StandaloneEditor::SceneObject> s_objectClipboard;
static glm::vec3 s_clipboardCenterOffset{0.0f};

void StandaloneEditor::CopySelectedObjects() {
    // Clear previous clipboard
    s_objectClipboard.clear();

    // Handle multi-selection mode
    if (m_isMultiSelectMode && !m_selectedObjectIndices.empty()) {
        // Calculate center of selection for offset calculation
        glm::vec3 selectionCenter{0.0f};
        for (int idx : m_selectedObjectIndices) {
            if (idx >= 0 && idx < static_cast<int>(m_sceneObjects.size())) {
                selectionCenter += m_sceneObjects[idx].position;
            }
        }
        selectionCenter /= static_cast<float>(m_selectedObjectIndices.size());
        s_clipboardCenterOffset = selectionCenter;

        // Copy all selected objects
        for (int idx : m_selectedObjectIndices) {
            if (idx >= 0 && idx < static_cast<int>(m_sceneObjects.size())) {
                s_objectClipboard.push_back(m_sceneObjects[idx]);
            }
        }

        spdlog::info("Copied {} objects to clipboard", s_objectClipboard.size());
    }
    // Handle single selection
    else if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size())) {
        const auto& obj = m_sceneObjects[m_selectedObjectIndex];
        s_objectClipboard.push_back(obj);
        s_clipboardCenterOffset = obj.position;

        spdlog::info("Copied object '{}' to clipboard", obj.name);
    }
    else {
        spdlog::warn("No object selected for copy");
        return;
    }
}

void StandaloneEditor::PasteObjects() {
    if (s_objectClipboard.empty()) {
        spdlog::warn("Clipboard is empty, nothing to paste");
        return;
    }

    // Calculate paste offset (offset from original position)
    const glm::vec3 pasteOffset{2.0f, 0.0f, 2.0f};

    // Clear current selection
    ClearSelection();

    // Paste objects with offset
    int firstPastedIndex = static_cast<int>(m_sceneObjects.size());

    for (const auto& clipObj : s_objectClipboard) {
        SceneObject newObj = clipObj;

        // Apply offset relative to clipboard center
        glm::vec3 relativePos = clipObj.position - s_clipboardCenterOffset;
        newObj.position = s_clipboardCenterOffset + relativePos + pasteOffset;

        // Append "_copy" to name if it doesn't already have it
        if (newObj.name.find("_copy") == std::string::npos) {
            newObj.name += "_copy";
        }

        m_sceneObjects.push_back(newObj);
        m_selectedObjectIndices.push_back(static_cast<int>(m_sceneObjects.size()) - 1);
    }

    // Select the newly pasted objects
    if (!m_selectedObjectIndices.empty()) {
        m_selectedObjectIndex = firstPastedIndex;
        m_isMultiSelectMode = (m_selectedObjectIndices.size() > 1);

        // Update transform widgets with first selected object
        const auto& firstObj = m_sceneObjects[firstPastedIndex];
        m_selectedObjectPosition = firstObj.position;
        m_selectedObjectRotation = firstObj.rotation;
        m_selectedObjectScale = firstObj.scale;
    }

    // Update clipboard center for subsequent pastes
    s_clipboardCenterOffset += pasteOffset;

    spdlog::info("Pasted {} objects", s_objectClipboard.size());
}
