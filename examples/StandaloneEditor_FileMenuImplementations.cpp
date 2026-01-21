// ========================================
// File Menu Polish - Complete Implementations
// To be integrated into StandaloneEditor.cpp
// ========================================

// Add to includes at the top of StandaloneEditor.cpp:
// #include <fstream>
// #include "terrain/HeightmapIO.hpp"
// #ifdef _WIN32
// #include <Windows.h>
// #include <commdlg.h>
// #endif

// ========================================
// Windows Native File Dialog Functions
// ========================================

#ifdef _WIN32
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {0};

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn));
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }

    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    char filename[MAX_PATH] = {0};

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }

    return "";
}
#else
// Fallback for non-Windows platforms
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}
#endif

// ========================================
// Recent Files Management
// ========================================

void StandaloneEditor::LoadRecentFiles() {
    std::string configPath = "assets/config/editor.json";
    std::ifstream file(configPath);

    if (!file.is_open()) {
        spdlog::info("No editor config found, starting with empty recent files");
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
                if (!path.empty()) {
                    m_recentFiles.push_back(path);
                }
            }
        }
    }

    file.close();
    spdlog::info("Loaded {} recent files", m_recentFiles.size());
}

void StandaloneEditor::SaveRecentFiles() {
    std::string configPath = "assets/config/editor.json";
    std::ofstream file(configPath);

    if (!file.is_open()) {
        spdlog::error("Failed to save recent files to {}", configPath);
        return;
    }

    // Write simple JSON structure
    file << "{\n";
    file << "  \"recentFiles\": [\n";

    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        file << "    \"" << m_recentFiles[i] << "\"";
        if (i < m_recentFiles.size() - 1) {
            file << ",";
        }
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
    spdlog::info("Saved {} recent files", m_recentFiles.size());
}

void StandaloneEditor::AddToRecentFiles(const std::string& path) {
    // Remove if already exists
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), path);

    // Keep only last 10
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }

    SaveRecentFiles();
}

void StandaloneEditor::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
}

// ========================================
// Enhanced ShowNewMapDialog with Flat/Spherical Options
// ========================================

void StandaloneEditor::ShowNewMapDialog() {
    ImGui::OpenPopup("New Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Map", &m_showNewMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static int width = 64;
        static int height = 64;
        static int worldTypeIndex = 0;
        const char* worldTypes[] = { "Flat", "Spherical" };

        ImGui::Text("Map Properties");
        ImGui::Separator();

        ImGui::InputInt("Width", &width);
        ImGui::InputInt("Height", &height);

        ImGui::Spacing();
        ImGui::Text("World Type:");
        ImGui::RadioButton("Flat", &worldTypeIndex, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Spherical", &worldTypeIndex, 1);

        if (worldTypeIndex == 1) {
            static float planetRadius = 6371.0f; // Earth radius in km
            ImGui::Spacing();
            ImGui::Text("Spherical World Settings:");
            ImGui::SliderFloat("Planet Radius (km)", &planetRadius, 100.0f, 50000.0f);
            ImGui::TextWrapped("Creates a spherical world with lat/long coordinates");
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            m_worldType = (worldTypeIndex == 0) ? WorldType::Flat : WorldType::Spherical;

            if (m_worldType == WorldType::Spherical) {
                NewWorldMap();
            } else {
                NewLocalMap(width, height);
            }

            m_showNewMapDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewMapDialog = false;
        }

        ImGui::EndPopup();
    }
}

// ========================================
// LoadMap and SaveMap with Proper File I/O
// ========================================

bool StandaloneEditor::LoadMap(const std::string& path) {
    spdlog::info("Loading map from: {}", path);

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open map file: {}", path);
        return false;
    }

    try {
        // Read map header
        char magic[4];
        file.read(magic, 4);
        if (std::string(magic, 4) != "NOVA") {
            spdlog::error("Invalid map file format");
            file.close();
            return false;
        }

        // Read version
        uint32_t version;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));

        // Read map dimensions
        file.read(reinterpret_cast<char*>(&m_mapWidth), sizeof(m_mapWidth));
        file.read(reinterpret_cast<char*>(&m_mapHeight), sizeof(m_mapHeight));

        // Read world type
        int worldType;
        file.read(reinterpret_cast<char*>(&worldType), sizeof(worldType));
        m_worldType = static_cast<WorldType>(worldType);

        // Read terrain tiles
        int numTiles = m_mapWidth * m_mapHeight;
        m_terrainTiles.resize(numTiles);
        file.read(reinterpret_cast<char*>(m_terrainTiles.data()), numTiles * sizeof(int));

        // Read terrain heights
        m_terrainHeights.resize(numTiles);
        file.read(reinterpret_cast<char*>(m_terrainHeights.data()), numTiles * sizeof(float));

        file.close();

        m_currentMapPath = path;
        spdlog::info("Map loaded successfully: {}x{}", m_mapWidth, m_mapHeight);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Exception while loading map: {}", e.what());
        file.close();
        return false;
    }
}

bool StandaloneEditor::SaveMap(const std::string& path) {
    spdlog::info("Saving map to: {}", path);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to create map file: {}", path);
        return false;
    }

    try {
        // Write map header
        const char* magic = "NOVA";
        file.write(magic, 4);

        // Write version
        uint32_t version = 1;
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));

        // Write map dimensions
        file.write(reinterpret_cast<const char*>(&m_mapWidth), sizeof(m_mapWidth));
        file.write(reinterpret_cast<const char*>(&m_mapHeight), sizeof(m_mapHeight));

        // Write world type
        int worldType = static_cast<int>(m_worldType);
        file.write(reinterpret_cast<const char*>(&worldType), sizeof(worldType));

        // Write terrain tiles
        int numTiles = m_mapWidth * m_mapHeight;
        file.write(reinterpret_cast<const char*>(m_terrainTiles.data()), numTiles * sizeof(int));

        // Write terrain heights
        file.write(reinterpret_cast<const char*>(m_terrainHeights.data()), numTiles * sizeof(float));

        file.close();

        m_currentMapPath = path;
        spdlog::info("Map saved successfully");
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Exception while saving map: {}", e.what());
        file.close();
        return false;
    }
}

// ========================================
// Import/Export Heightmap Functions
// ========================================

bool StandaloneEditor::ImportHeightmap(const std::string& path) {
    spdlog::info("Importing heightmap from: {}", path);

    // Check file extension
    std::string ext = path.substr(path.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "raw") {
        // Import RAW heightmap (16-bit grayscale)
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open RAW file: {}", path);
            return false;
        }

        // Assume square heightmap for now
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Calculate dimensions (assuming 16-bit per pixel)
        int dimension = static_cast<int>(std::sqrt(fileSize / 2));
        m_mapWidth = dimension;
        m_mapHeight = dimension;

        // Read heightmap data
        std::vector<uint16_t> heightData(dimension * dimension);
        file.read(reinterpret_cast<char*>(heightData.data()), fileSize);
        file.close();

        // Convert to float heights (normalize to 0-100 range)
        m_terrainHeights.resize(dimension * dimension);
        for (size_t i = 0; i < heightData.size(); ++i) {
            m_terrainHeights[i] = (heightData[i] / 65535.0f) * 100.0f;
        }

        // Initialize terrain tiles
        m_terrainTiles.resize(dimension * dimension, 0);

        spdlog::info("Imported RAW heightmap: {}x{}", dimension, dimension);
        return true;
    }
    else if (ext == "png") {
        // Use Nova's HeightmapIO for PNG loading (it already uses stb_image internally)
        Nova::HeightmapImportOptions importOptions;
        importOptions.normalizeHeight = true;
        importOptions.targetMinHeight = 0.0f;
        importOptions.targetMaxHeight = 100.0f;

        Nova::HeightmapResult result = Nova::HeightmapIO::LoadFromPNG(path, importOptions);
        if (!result.success) {
            spdlog::error("Failed to load PNG heightmap: {}", result.errorMessage);
            return false;
        }

        // Copy loaded data to our terrain arrays
        m_mapWidth = result.heightmap.width;
        m_mapHeight = result.heightmap.height;
        m_terrainHeights.resize(m_mapWidth * m_mapHeight);

        // Convert normalized heights to our scale (0-100 range)
        for (size_t i = 0; i < result.heightmap.data.size(); ++i) {
            m_terrainHeights[i] = result.heightmap.GetWorldHeight(
                static_cast<int>(i % m_mapWidth),
                static_cast<int>(i / m_mapWidth)
            );
        }

        // Initialize terrain tiles
        m_terrainTiles.resize(m_mapWidth * m_mapHeight, 0);

        spdlog::info("Imported PNG heightmap: {}x{}", m_mapWidth, m_mapHeight);
        return true;
    }
    else {
        spdlog::error("Unsupported heightmap format: {}", ext);
        return false;
    }
}

bool StandaloneEditor::ExportHeightmap(const std::string& path) {
    spdlog::info("Exporting heightmap to: {}", path);

    // Check file extension
    std::string ext = path.substr(path.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "raw") {
        // Export RAW heightmap (16-bit grayscale)
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to create RAW file: {}", path);
            return false;
        }

        // Convert float heights to 16-bit (assuming 0-100 range)
        std::vector<uint16_t> heightData(m_terrainHeights.size());
        for (size_t i = 0; i < m_terrainHeights.size(); ++i) {
            float normalized = std::clamp(m_terrainHeights[i] / 100.0f, 0.0f, 1.0f);
            heightData[i] = static_cast<uint16_t>(normalized * 65535.0f);
        }

        file.write(reinterpret_cast<const char*>(heightData.data()),
                   heightData.size() * sizeof(uint16_t));
        file.close();

        spdlog::info("Exported RAW heightmap: {}x{}", m_mapWidth, m_mapHeight);
        return true;
    }
    else if (ext == "png") {
        // Use Nova's HeightmapIO for PNG saving (it already uses stb_image_write internally)
        Nova::HeightmapData heightmap;
        heightmap.width = m_mapWidth;
        heightmap.height = m_mapHeight;
        heightmap.data.resize(m_terrainHeights.size());
        heightmap.minHeight = 0.0f;
        heightmap.maxHeight = 100.0f;

        // Convert our height data to normalized format for HeightmapIO
        for (size_t i = 0; i < m_terrainHeights.size(); ++i) {
            // Normalize from 0-100 range to 0-1 range
            heightmap.data[i] = std::clamp(m_terrainHeights[i] / 100.0f, 0.0f, 1.0f);
        }

        Nova::HeightmapExportOptions exportOptions;
        exportOptions.normalize = true;

        if (!Nova::HeightmapIO::SaveToPNG(heightmap, path, 16, exportOptions)) {
            spdlog::error("Failed to export PNG heightmap: {}", Nova::HeightmapIO::GetLastError());
            return false;
        }

        spdlog::info("Exported PNG heightmap: {}x{}", m_mapWidth, m_mapHeight);
        return true;
    }
    else {
        spdlog::error("Unsupported heightmap format: {}", ext);
        return false;
    }
}

// ========================================
// Enhanced File Menu in RenderUI()
// Replace the existing File menu in RenderUI() with:
// ========================================

/*
    // Handle keyboard shortcuts (add at the beginning of RenderUI())
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

    // File Menu (replace existing File menu):
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Map", "Ctrl+N")) {
            m_showNewMapDialog = true;
        }
        if (ImGui::MenuItem("Open", "Ctrl+O")) {
            std::string path = OpenNativeFileDialog("Map Files (*.map)\\0*.map\\0All Files (*.*)\\0*.*\\0", "Open Map");
            if (!path.empty()) {
                if (LoadMap(path)) {
                    AddToRecentFiles(path);
                }
            }
        }

        // Open Recent submenu
        if (ImGui::BeginMenu("Open Recent", !m_recentFiles.empty())) {
            for (const auto& file : m_recentFiles) {
                if (ImGui::MenuItem(file.c_str())) {
                    if (LoadMap(file)) {
                        AddToRecentFiles(file);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recent Files")) {
                ClearRecentFiles();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
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
        if (ImGui::MenuItem("Save As", "Ctrl+Shift+S")) {
            std::string path = SaveNativeFileDialog("Map Files (*.map)\\0*.map\\0All Files (*.*)\\0*.*\\0", "Save Map As", "map");
            if (!path.empty()) {
                if (SaveMap(path)) {
                    AddToRecentFiles(path);
                }
            }
        }

        ImGui::Separator();

        // Import submenu
        if (ImGui::BeginMenu("Import")) {
            if (ImGui::MenuItem("Heightmap (PNG/RAW)")) {
                std::string path = OpenNativeFileDialog("Heightmap Files (*.png;*.raw)\\0*.png;*.raw\\0All Files (*.*)\\0*.*\\0", "Import Heightmap");
                if (!path.empty()) {
                    ImportHeightmap(path);
                }
            }
            ImGui::EndMenu();
        }

        // Export submenu
        if (ImGui::BeginMenu("Export")) {
            if (ImGui::MenuItem("Heightmap (PNG)")) {
                std::string path = SaveNativeFileDialog("PNG Files (*.png)\\0*.png\\0All Files (*.*)\\0*.*\\0", "Export Heightmap", "png");
                if (!path.empty()) {
                    ExportHeightmap(path);
                }
            }
            if (ImGui::MenuItem("Heightmap (RAW)")) {
                std::string path = SaveNativeFileDialog("RAW Files (*.raw)\\0*.raw\\0All Files (*.*)\\0*.*\\0", "Export Heightmap", "raw");
                if (!path.empty()) {
                    ExportHeightmap(path);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "")) {
            // Will be handled by RTSApplication
        }
        ImGui::EndMenu();
    }
*/

// ========================================
// Initialize call in Initialize() function
// Add to the Initialize() function:
// ========================================

/*
    // Load recent files
    LoadRecentFiles();
*/
