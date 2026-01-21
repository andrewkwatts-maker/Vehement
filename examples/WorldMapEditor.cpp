#include "WorldMapEditor.hpp"
#include "ModernUI.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstring>

WorldMapEditor::WorldMapEditor() = default;
WorldMapEditor::~WorldMapEditor() = default;

bool WorldMapEditor::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing World Map Editor");

    // Initialize PCG editor
    m_pcgEditor = std::make_unique<PCGGraphEditor>();
    m_pcgEditor->Initialize();

    // Create empty PCG graph
    m_pcgGraph = std::make_unique<PCG::PCGGraph>();

    m_initialized = true;
    spdlog::info("World Map Editor initialized successfully");
    return true;
}

void WorldMapEditor::Shutdown() {
    spdlog::info("Shutting down World Map Editor");

    if (m_pcgEditor) {
        m_pcgEditor->Shutdown();
    }

    m_initialized = false;
}

void WorldMapEditor::Update(float deltaTime) {
    // Empty for now - will handle camera movement, chunk loading, etc.
}

void WorldMapEditor::RenderUI() {
    if (!m_initialized) {
        return;
    }

    // Main window with glassmorphic styling
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    if (ImGui::Begin("World Map Editor", nullptr, windowFlags)) {
        RenderMenuBar();
        RenderToolbar();

        // Main content area - split into panels
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

        // Left panel - Navigator and Data Sources
        ImGui::BeginChild("LeftPanel", ImVec2(300, 0), true);
        if (m_showNavigator) {
            RenderNavigator();
        }
        if (m_showDataSources) {
            RenderDataSourcesPanel();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel - 3D View (placeholder)
        ImGui::BeginChild("CenterPanel", ImVec2(0, -300), true);
        ModernUI::GradientHeader("3D World View");
        ImGui::Text("3D rendering area - Lat: %.4f, Lon: %.4f", m_currentLatitude, m_currentLongitude);
        ImGui::Text("Altitude: %.2fm, Zoom: %.2fx", m_cameraAltitude, m_cameraZoom);

        // Placeholder for 3D view
        ImVec2 viewSize = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("3DView", viewSize);

        ImGui::EndChild();

        // Bottom panel - Generation and Properties
        ImGui::BeginChild("BottomPanel", ImVec2(0, 0), false);

        ImGui::Columns(3, "BottomColumns", true);

        // PCG Panel
        if (m_showGeneration) {
            RenderPCGPanel();
        }

        ImGui::NextColumn();

        // Generation Panel
        if (m_showGeneration) {
            RenderGenerationPanel();
        }

        ImGui::NextColumn();

        // Properties Panel
        if (m_showProperties) {
            RenderPropertiesPanel();
        }

        ImGui::Columns(1);
        ImGui::EndChild();

        ImGui::EndChild();
    }
    ImGui::End();

    // Render PCG editor if open
    if (m_showPCGEditor && m_pcgEditor) {
        m_pcgEditor->Render(&m_showPCGEditor);
    }
}

void WorldMapEditor::Render3D() {
    // Empty for now - will render world chunks, terrain, etc.
}

void WorldMapEditor::NewWorld(const WorldConfig& config) {
    spdlog::info("Creating new world map");
    m_config = config;
    m_currentWorldPath.clear();

    // Clear existing chunks
    m_loadedChunks.clear();

    // Reset camera to origin
    m_currentLatitude = 0.0;
    m_currentLongitude = 0.0;
    m_cameraAltitude = 1000.0f;
    m_cameraZoom = 1.0f;

    spdlog::info("New world created: Lat [{}, {}], Lon [{}, {}]",
                 config.minLatitude, config.maxLatitude,
                 config.minLongitude, config.maxLongitude);
}

bool WorldMapEditor::LoadWorld(const std::string& path) {
    spdlog::info("Loading world from: {}", path);

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open world file: {}", path);
        return false;
    }

    // Read magic number and version
    char magic[8];
    file.read(magic, 8);
    if (std::string(magic, 8) != "NOVA3DWM") {
        spdlog::error("Invalid world file format: {}", path);
        return false;
    }

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version > 1) {
        spdlog::error("Unsupported world file version: {}", version);
        return false;
    }

    // Read world config
    file.read(reinterpret_cast<char*>(&m_config.minLatitude), sizeof(double));
    file.read(reinterpret_cast<char*>(&m_config.maxLatitude), sizeof(double));
    file.read(reinterpret_cast<char*>(&m_config.minLongitude), sizeof(double));
    file.read(reinterpret_cast<char*>(&m_config.maxLongitude), sizeof(double));
    file.read(reinterpret_cast<char*>(&m_config.tilesPerDegree), sizeof(int));
    file.read(reinterpret_cast<char*>(&m_config.minElevation), sizeof(float));
    file.read(reinterpret_cast<char*>(&m_config.maxElevation), sizeof(float));
    file.read(reinterpret_cast<char*>(&m_config.worldSeed), sizeof(uint64_t));

    // Read PCG graph path
    uint32_t pathLen;
    file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
    if (pathLen > 0) {
        m_config.pcgGraphPath.resize(pathLen);
        file.read(m_config.pcgGraphPath.data(), pathLen);
    }

    // Read number of chunks
    uint32_t numChunks;
    file.read(reinterpret_cast<char*>(&numChunks), sizeof(numChunks));

    // Clear and load chunks
    m_loadedChunks.clear();
    m_loadedChunks.reserve(numChunks);

    for (uint32_t i = 0; i < numChunks; ++i) {
        TerrainChunk chunk;
        file.read(reinterpret_cast<char*>(&chunk.centerLat), sizeof(double));
        file.read(reinterpret_cast<char*>(&chunk.centerLon), sizeof(double));
        file.read(reinterpret_cast<char*>(&chunk.tileSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&chunk.isGenerated), sizeof(bool));

        // Read height data
        uint32_t heightCount;
        file.read(reinterpret_cast<char*>(&heightCount), sizeof(heightCount));
        chunk.heights.resize(heightCount);
        if (heightCount > 0) {
            file.read(reinterpret_cast<char*>(chunk.heights.data()), heightCount * sizeof(float));
        }

        m_loadedChunks.push_back(std::move(chunk));
    }

    m_currentWorldPath = path;
    spdlog::info("Loaded world with {} chunks", numChunks);
    return true;
}

bool WorldMapEditor::SaveWorld(const std::string& path) {
    spdlog::info("Saving world to: {}", path);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to create world file: {}", path);
        return false;
    }

    // Write magic number and version
    const char magic[8] = "NOVA3DWM";
    file.write(magic, 8);

    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write world config
    file.write(reinterpret_cast<const char*>(&m_config.minLatitude), sizeof(double));
    file.write(reinterpret_cast<const char*>(&m_config.maxLatitude), sizeof(double));
    file.write(reinterpret_cast<const char*>(&m_config.minLongitude), sizeof(double));
    file.write(reinterpret_cast<const char*>(&m_config.maxLongitude), sizeof(double));
    file.write(reinterpret_cast<const char*>(&m_config.tilesPerDegree), sizeof(int));
    file.write(reinterpret_cast<const char*>(&m_config.minElevation), sizeof(float));
    file.write(reinterpret_cast<const char*>(&m_config.maxElevation), sizeof(float));
    file.write(reinterpret_cast<const char*>(&m_config.worldSeed), sizeof(uint64_t));

    // Write PCG graph path
    uint32_t pathLen = static_cast<uint32_t>(m_config.pcgGraphPath.size());
    file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
    if (pathLen > 0) {
        file.write(m_config.pcgGraphPath.data(), pathLen);
    }

    // Write number of chunks
    uint32_t numChunks = static_cast<uint32_t>(m_loadedChunks.size());
    file.write(reinterpret_cast<const char*>(&numChunks), sizeof(numChunks));

    // Write each chunk
    for (const auto& chunk : m_loadedChunks) {
        file.write(reinterpret_cast<const char*>(&chunk.centerLat), sizeof(double));
        file.write(reinterpret_cast<const char*>(&chunk.centerLon), sizeof(double));
        file.write(reinterpret_cast<const char*>(&chunk.tileSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&chunk.isGenerated), sizeof(bool));

        // Write height data
        uint32_t heightCount = static_cast<uint32_t>(chunk.heights.size());
        file.write(reinterpret_cast<const char*>(&heightCount), sizeof(heightCount));
        if (heightCount > 0) {
            file.write(reinterpret_cast<const char*>(chunk.heights.data()), heightCount * sizeof(float));
        }
    }

    m_currentWorldPath = path;
    spdlog::info("Saved world with {} chunks to {}", numChunks, path);
    return true;
}

void WorldMapEditor::GenerateRegion(double lat, double lon, int radiusTiles) {
    spdlog::info("Generating region at ({}, {}) with radius {} tiles", lat, lon, radiusTiles);

    // Calculate degrees per tile
    double degreesPerTile = 1.0 / m_config.tilesPerDegree;

    // Generate chunks in the radius
    for (int dy = -radiusTiles; dy <= radiusTiles; ++dy) {
        for (int dx = -radiusTiles; dx <= radiusTiles; ++dx) {
            // Skip corners to approximate circle
            if (dx * dx + dy * dy > radiusTiles * radiusTiles) {
                continue;
            }

            double chunkLat = lat + dy * degreesPerTile;
            double chunkLon = lon + dx * degreesPerTile;

            // Check bounds
            if (chunkLat < m_config.minLatitude || chunkLat > m_config.maxLatitude ||
                chunkLon < m_config.minLongitude || chunkLon > m_config.maxLongitude) {
                continue;
            }

            // Check if chunk already exists
            bool exists = false;
            for (auto& chunk : m_loadedChunks) {
                if (std::abs(chunk.centerLat - chunkLat) < degreesPerTile * 0.5 &&
                    std::abs(chunk.centerLon - chunkLon) < degreesPerTile * 0.5) {
                    exists = true;
                    if (chunk.needsRegeneration) {
                        chunk.needsRegeneration = false;
                        GenerateChunkTerrain(chunk);
                    }
                    break;
                }
            }

            if (!exists) {
                // Create new chunk
                TerrainChunk chunk;
                chunk.centerLat = chunkLat;
                chunk.centerLon = chunkLon;
                chunk.tileSize = 32; // Default tile size
                chunk.isGenerated = false;
                chunk.needsRegeneration = false;

                // Allocate height data (32x32 grid)
                chunk.heights.resize(chunk.tileSize * chunk.tileSize, 0.0f);

                // Generate terrain for this chunk
                GenerateChunkTerrain(chunk);

                m_loadedChunks.push_back(std::move(chunk));
            }
        }
    }

    spdlog::info("Region generation complete, {} chunks loaded", m_loadedChunks.size());
}

float WorldMapEditor::GetHeightAt(double latitude, double longitude) {
    // Find the chunk containing this coordinate
    TerrainChunk* chunk = FindChunkAt(latitude, longitude);
    if (!chunk || !chunk->isGenerated || chunk->heights.empty()) {
        return 0.0f;
    }

    // Calculate local position within chunk
    double degreesPerTile = 1.0 / m_config.tilesPerDegree;
    double halfChunkSize = (chunk->tileSize * degreesPerTile) * 0.5;

    // Normalize to 0-1 within chunk
    double localLat = (latitude - (chunk->centerLat - halfChunkSize)) / (2.0 * halfChunkSize);
    double localLon = (longitude - (chunk->centerLon - halfChunkSize)) / (2.0 * halfChunkSize);

    // Clamp to valid range
    localLat = std::clamp(localLat, 0.0, 1.0);
    localLon = std::clamp(localLon, 0.0, 1.0);

    // Convert to grid indices
    int maxIdx = chunk->tileSize - 1;
    double gx = localLon * maxIdx;
    double gy = localLat * maxIdx;

    // Bilinear interpolation
    int x0 = static_cast<int>(gx);
    int y0 = static_cast<int>(gy);
    int x1 = std::min(x0 + 1, maxIdx);
    int y1 = std::min(y0 + 1, maxIdx);

    float fx = static_cast<float>(gx - x0);
    float fy = static_cast<float>(gy - y0);

    // Get heights at four corners
    float h00 = chunk->heights[y0 * chunk->tileSize + x0];
    float h10 = chunk->heights[y0 * chunk->tileSize + x1];
    float h01 = chunk->heights[y1 * chunk->tileSize + x0];
    float h11 = chunk->heights[y1 * chunk->tileSize + x1];

    // Bilinear interpolation
    float h0 = h00 * (1.0f - fx) + h10 * fx;
    float h1 = h01 * (1.0f - fx) + h11 * fx;
    return h0 * (1.0f - fy) + h1 * fy;
}

void WorldMapEditor::SetHeightAt(double latitude, double longitude, float height) {
    // Find the chunk containing this coordinate
    TerrainChunk* chunk = FindChunkAt(latitude, longitude);
    if (!chunk || chunk->heights.empty()) {
        spdlog::warn("SetHeightAt: No chunk found at ({}, {})", latitude, longitude);
        return;
    }

    // Calculate local position within chunk
    double degreesPerTile = 1.0 / m_config.tilesPerDegree;
    double halfChunkSize = (chunk->tileSize * degreesPerTile) * 0.5;

    // Normalize to 0-1 within chunk
    double localLat = (latitude - (chunk->centerLat - halfChunkSize)) / (2.0 * halfChunkSize);
    double localLon = (longitude - (chunk->centerLon - halfChunkSize)) / (2.0 * halfChunkSize);

    // Clamp to valid range
    localLat = std::clamp(localLat, 0.0, 1.0);
    localLon = std::clamp(localLon, 0.0, 1.0);

    // Convert to grid index
    int x = static_cast<int>(localLon * (chunk->tileSize - 1));
    int y = static_cast<int>(localLat * (chunk->tileSize - 1));

    // Clamp indices
    x = std::clamp(x, 0, chunk->tileSize - 1);
    y = std::clamp(y, 0, chunk->tileSize - 1);

    // Set height value, clamped to valid elevation range
    height = std::clamp(height, m_config.minElevation, m_config.maxElevation);
    chunk->heights[y * chunk->tileSize + x] = height;
}

void WorldMapEditor::NavigateTo(double latitude, double longitude) {
    m_currentLatitude = std::clamp(latitude, m_config.minLatitude, m_config.maxLatitude);
    m_currentLongitude = std::clamp(longitude, m_config.minLongitude, m_config.maxLongitude);
    spdlog::info("Navigated to ({}, {})", m_currentLatitude, m_currentLongitude);
}

// ============================================================================
// UI Panels
// ============================================================================

void WorldMapEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New World", "Ctrl+N")) {
                WorldConfig config;
                NewWorld(config);
            }
            if (ImGui::MenuItem("Open World...", "Ctrl+O")) {
                m_showOpenDialog = true;
                std::memset(m_dialogPathBuffer, 0, sizeof(m_dialogPathBuffer));
            }
            if (ImGui::MenuItem("Save World", "Ctrl+S", false, !m_currentWorldPath.empty())) {
                if (!m_currentWorldPath.empty()) {
                    SaveWorld(m_currentWorldPath);
                }
            }
            if (ImGui::MenuItem("Save World As...", "Ctrl+Shift+S")) {
                m_showSaveDialog = true;
                std::memset(m_dialogPathBuffer, 0, sizeof(m_dialogPathBuffer));
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Export Region...")) {
                m_showExportDialog = true;
                std::memset(m_exportPathBuffer, 0, sizeof(m_exportPathBuffer));
            }
            if (ImGui::MenuItem("Import Real-World Data...")) {
                m_showImportDialog = true;
                std::memset(m_dialogPathBuffer, 0, sizeof(m_dialogPathBuffer));
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Close", "Ctrl+W")) {
                // Will close window
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("World Properties...")) {
                m_showWorldPropertiesDialog = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Navigator", nullptr, &m_showNavigator);
            ImGui::MenuItem("Data Sources", nullptr, &m_showDataSources);
            ImGui::MenuItem("Generation", nullptr, &m_showGeneration);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Reset Camera")) {
                m_currentLatitude = 0.0;
                m_currentLongitude = 0.0;
                m_cameraAltitude = 1000.0f;
                m_cameraZoom = 1.0f;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("PCG Graph Editor")) {
                m_showPCGEditor = true;
            }
            if (ImGui::MenuItem("Generate Entire World")) {
                GenerateEntireWorld();
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Clear All Chunks")) {
                m_loadedChunks.clear();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Render dialogs
    RenderFileDialogs();
}

void WorldMapEditor::RenderToolbar() {
    ModernUI::BeginGlassCard("Toolbar", ImVec2(0, 40));

    ImGui::BeginDisabled(true);
    if (ModernUI::GlowButton("Back", ImVec2(60, 0))) {}
    ImGui::SameLine();
    if (ModernUI::GlowButton("Forward", ImVec2(60, 0))) {}
    ImGui::SameLine();
    if (ModernUI::GlowButton("Refresh", ImVec2(60, 0))) {}
    ImGui::EndDisabled();

    ImGui::SameLine();
    ModernUI::GradientSeparator();
    ImGui::SameLine();

    ImGui::Text("Altitude:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##Altitude", &m_cameraAltitude, 10.0f, 50000.0f, "%.0fm");

    ImGui::SameLine();
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##Zoom", &m_cameraZoom, 0.1f, 10.0f, "%.2fx");

    ModernUI::EndGlassCard();
}

void WorldMapEditor::RenderNavigator() {
    if (ModernUI::GradientHeader("Navigator", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("NavigatorContent");

        ImGui::Text("Current Position");
        ModernUI::GradientSeparator(0.3f);

        // Latitude input
        ImGui::Text("Latitude:");
        ImGui::SetNextItemWidth(-1);
        double tempLat = m_currentLatitude;
        if (ImGui::InputDouble("##Latitude", &tempLat, 0.1, 1.0, "%.6f")) {
            m_currentLatitude = std::clamp(tempLat, m_config.minLatitude, m_config.maxLatitude);
        }

        // Longitude input
        ImGui::Text("Longitude:");
        ImGui::SetNextItemWidth(-1);
        double tempLon = m_currentLongitude;
        if (ImGui::InputDouble("##Longitude", &tempLon, 0.1, 1.0, "%.6f")) {
            m_currentLongitude = std::clamp(tempLon, m_config.minLongitude, m_config.maxLongitude);
        }

        if (ModernUI::GlowButton("Go To Location", ImVec2(-1, 0))) {
            NavigateTo(m_currentLatitude, m_currentLongitude);
        }

        ModernUI::GradientSeparator(0.3f);

        // Quick navigation presets
        ImGui::Text("Quick Navigation");
        if (ModernUI::GlowButton("Equator (0, 0)", ImVec2(-1, 0))) {
            NavigateTo(0.0, 0.0);
        }
        if (ModernUI::GlowButton("North Pole", ImVec2(-1, 0))) {
            NavigateTo(90.0, 0.0);
        }
        if (ModernUI::GlowButton("South Pole", ImVec2(-1, 0))) {
            NavigateTo(-90.0, 0.0);
        }

        ModernUI::EndGlassCard();
    }
}

void WorldMapEditor::RenderPCGPanel() {
    if (ModernUI::GradientHeader("PCG Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PCGContent");

        ImGui::Text("Procedural Generation");
        ModernUI::GradientSeparator(0.3f);

        if (!m_config.pcgGraphPath.empty()) {
            ImGui::Text("Current Graph:");
            ImGui::TextWrapped("%s", m_config.pcgGraphPath.c_str());
        } else {
            ImGui::TextDisabled("No PCG graph loaded");
        }

        if (ModernUI::GlowButton("Open PCG Graph Editor", ImVec2(-1, 0))) {
            m_showPCGEditor = true;
            if (!m_pcgGraph) {
                m_pcgGraph = std::make_unique<PCG::PCGGraph>();
            }
        }

        if (ModernUI::GlowButton("Load PCG Graph...", ImVec2(-1, 0))) {
            m_showLoadPCGDialog = true;
            std::memset(m_dialogPathBuffer, 0, sizeof(m_dialogPathBuffer));
        }

        if (ModernUI::GlowButton("Save PCG Graph...", ImVec2(-1, 0))) {
            m_showSavePCGDialog = true;
            std::memset(m_dialogPathBuffer, 0, sizeof(m_dialogPathBuffer));
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("World Seed:");
        ImGui::SetNextItemWidth(-1);
        uint64_t seed = m_config.worldSeed;
        if (ImGui::InputScalar("##Seed", ImGuiDataType_U64, &seed)) {
            m_config.worldSeed = seed;
        }

        ModernUI::EndGlassCard();
    }
}

void WorldMapEditor::RenderDataSourcesPanel() {
    if (ModernUI::GradientHeader("Real-World Data Sources", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("DataSourcesContent");

        ImGui::Checkbox("Use Real-World Elevation", &m_useRealWorldElevation);
        if (m_useRealWorldElevation) {
            ImGui::Indent();
            ImGui::TextDisabled("Elevation data: %s",
                              m_hasElevationData ? "Loaded" : "Not loaded");
            if (ModernUI::GlowButton("Load Elevation Data...", ImVec2(-1, 0))) {
                if (LoadElevationData()) {
                    m_hasElevationData = true;
                    spdlog::info("Elevation data loaded successfully");
                }
            }
            ImGui::Unindent();
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Checkbox("Avoid Roads", &m_avoidRoads);
        if (m_avoidRoads) {
            ImGui::Indent();
            ImGui::TextDisabled("Road data: %s",
                              m_hasRoadData ? "Loaded" : "Not loaded");
            if (ModernUI::GlowButton("Load Road Data...", ImVec2(-1, 0))) {
                if (LoadRoadData()) {
                    m_hasRoadData = true;
                    spdlog::info("Road data loaded successfully");
                }
            }
            ImGui::Unindent();
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Checkbox("Avoid Buildings", &m_avoidBuildings);
        if (m_avoidBuildings) {
            ImGui::Indent();
            ImGui::TextDisabled("Building data: %s",
                              m_hasBuildingData ? "Loaded" : "Not loaded");
            if (ModernUI::GlowButton("Load Building Data...", ImVec2(-1, 0))) {
                if (LoadBuildingData()) {
                    m_hasBuildingData = true;
                    spdlog::info("Building data loaded successfully");
                }
            }
            ImGui::Unindent();
        }

        ModernUI::EndGlassCard();
    }
}

void WorldMapEditor::RenderGenerationPanel() {
    if (ModernUI::GradientHeader("Terrain Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("GenerationContent");

        ImGui::Text("Generate Region");
        ModernUI::GradientSeparator(0.3f);

        static int radiusTiles = 10;
        ImGui::Text("Radius (tiles):");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##Radius", &radiusTiles, 1, 100);

        ImGui::Text("Generation Scale:");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##Scale", &m_generationScale, 0.1f, 10.0f);

        if (ModernUI::GlowButton("Generate Region", ImVec2(-1, 0))) {
            GenerateRegion(m_currentLatitude, m_currentLongitude, radiusTiles);
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Loaded Chunks: %zu", m_loadedChunks.size());

        if (ModernUI::GlowButton("Clear All Chunks", ImVec2(-1, 0))) {
            m_loadedChunks.clear();
        }

        ModernUI::EndGlassCard();
    }
}

void WorldMapEditor::RenderPropertiesPanel() {
    if (ModernUI::GradientHeader("World Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PropertiesContent");

        ImGui::Text("World Bounds");
        ModernUI::GradientSeparator(0.3f);

        ModernUI::CompactStat("Min Latitude",
                             std::to_string(m_config.minLatitude).c_str());
        ModernUI::CompactStat("Max Latitude",
                             std::to_string(m_config.maxLatitude).c_str());
        ModernUI::CompactStat("Min Longitude",
                             std::to_string(m_config.minLongitude).c_str());
        ModernUI::CompactStat("Max Longitude",
                             std::to_string(m_config.maxLongitude).c_str());

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Resolution");
        ModernUI::CompactStat("Tiles/Degree",
                             std::to_string(m_config.tilesPerDegree).c_str());

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Elevation Range");
        ModernUI::CompactStat("Min Elevation",
                             (std::to_string((int)m_config.minElevation) + "m").c_str());
        ModernUI::CompactStat("Max Elevation",
                             (std::to_string((int)m_config.maxElevation) + "m").c_str());

        ModernUI::EndGlassCard();
    }
}

// ============================================================================
// File Dialogs
// ============================================================================

void WorldMapEditor::RenderFileDialogs() {
    // Open World Dialog
    if (m_showOpenDialog) {
        ImGui::OpenPopup("Open World");
    }
    if (ImGui::BeginPopupModal("Open World", &m_showOpenDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter world file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##OpenPath", m_dialogPathBuffer, sizeof(m_dialogPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (LoadWorld(m_dialogPathBuffer)) {
                m_showOpenDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showOpenDialog = false;
        }
        ImGui::EndPopup();
    }

    // Save World As Dialog
    if (m_showSaveDialog) {
        ImGui::OpenPopup("Save World As");
    }
    if (ImGui::BeginPopupModal("Save World As", &m_showSaveDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter world file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##SavePath", m_dialogPathBuffer, sizeof(m_dialogPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (SaveWorld(m_dialogPathBuffer)) {
                m_showSaveDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSaveDialog = false;
        }
        ImGui::EndPopup();
    }

    // Export Region Dialog
    if (m_showExportDialog) {
        ImGui::OpenPopup("Export Region");
    }
    if (ImGui::BeginPopupModal("Export Region", &m_showExportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export current view region as heightmap");
        ImGui::Separator();
        static int exportRadius = 10;
        ImGui::Text("Radius (tiles):"); ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("##ExportRadius", &exportRadius, 1, 50);
        ImGui::Text("Output path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##ExportPath", m_exportPathBuffer, sizeof(m_exportPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            ExportRegion(m_currentLatitude, m_currentLongitude, exportRadius, m_exportPathBuffer);
            m_showExportDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showExportDialog = false;
        }
        ImGui::EndPopup();
    }

    // Import Data Dialog
    if (m_showImportDialog) {
        ImGui::OpenPopup("Import Real-World Data");
    }
    if (ImGui::BeginPopupModal("Import Real-World Data", &m_showImportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import real-world data from file");
        ImGui::Separator();
        static int importType = 0;
        ImGui::Combo("Data Type", &importType, "Elevation\0Roads\0Buildings\0Biome\0");
        ImGui::Text("File path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##ImportPath", m_dialogPathBuffer, sizeof(m_dialogPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Import", ImVec2(120, 0))) {
            switch (importType) {
                case 0: m_hasElevationData = LoadElevationData(); break;
                case 1: m_hasRoadData = LoadRoadData(); break;
                case 2: m_hasBuildingData = LoadBuildingData(); break;
                case 3: m_hasBiomeData = LoadBiomeData(); break;
            }
            m_showImportDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showImportDialog = false;
        }
        ImGui::EndPopup();
    }

    // World Properties Dialog
    if (m_showWorldPropertiesDialog) {
        ImGui::OpenPopup("World Properties");
    }
    if (ImGui::BeginPopupModal("World Properties", &m_showWorldPropertiesDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("World Configuration");
        ImGui::Separator();

        ImGui::Text("Latitude Range:");
        ImGui::SetNextItemWidth(150);
        ImGui::InputDouble("Min##Lat", &m_config.minLatitude, 1.0, 10.0, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputDouble("Max##Lat", &m_config.maxLatitude, 1.0, 10.0, "%.2f");

        ImGui::Text("Longitude Range:");
        ImGui::SetNextItemWidth(150);
        ImGui::InputDouble("Min##Lon", &m_config.minLongitude, 1.0, 10.0, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputDouble("Max##Lon", &m_config.maxLongitude, 1.0, 10.0, "%.2f");

        ImGui::Text("Resolution:");
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("Tiles per Degree", &m_config.tilesPerDegree);

        ImGui::Text("Elevation Range (meters):");
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Min##Elev", &m_config.minElevation, 10.0f, 100.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Max##Elev", &m_config.maxElevation, 10.0f, 100.0f, "%.1f");

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_showWorldPropertiesDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showWorldPropertiesDialog = false;
        }
        ImGui::EndPopup();
    }

    // Load PCG Graph Dialog
    if (m_showLoadPCGDialog) {
        ImGui::OpenPopup("Load PCG Graph");
    }
    if (ImGui::BeginPopupModal("Load PCG Graph", &m_showLoadPCGDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter PCG graph file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##PCGLoadPath", m_dialogPathBuffer, sizeof(m_dialogPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (m_pcgGraph && m_pcgGraph->LoadFromFile(m_dialogPathBuffer)) {
                m_config.pcgGraphPath = m_dialogPathBuffer;
                spdlog::info("Loaded PCG graph: {}", m_dialogPathBuffer);
            }
            m_showLoadPCGDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showLoadPCGDialog = false;
        }
        ImGui::EndPopup();
    }

    // Save PCG Graph Dialog
    if (m_showSavePCGDialog) {
        ImGui::OpenPopup("Save PCG Graph");
    }
    if (ImGui::BeginPopupModal("Save PCG Graph", &m_showSavePCGDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter PCG graph file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##PCGSavePath", m_dialogPathBuffer, sizeof(m_dialogPathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (m_pcgGraph) {
                m_pcgGraph->SaveToFile(m_dialogPathBuffer);
                m_config.pcgGraphPath = m_dialogPathBuffer;
                spdlog::info("Saved PCG graph: {}", m_dialogPathBuffer);
            }
            m_showSavePCGDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSavePCGDialog = false;
        }
        ImGui::EndPopup();
    }
}

void WorldMapEditor::GenerateEntireWorld() {
    spdlog::info("Generating entire world...");

    // Calculate total tiles
    double latRange = m_config.maxLatitude - m_config.minLatitude;
    double lonRange = m_config.maxLongitude - m_config.minLongitude;
    int totalLatTiles = static_cast<int>(latRange * m_config.tilesPerDegree);
    int totalLonTiles = static_cast<int>(lonRange * m_config.tilesPerDegree);

    spdlog::info("World size: {}x{} tiles", totalLatTiles, totalLonTiles);

    // Generate in chunks to avoid overwhelming memory
    int chunkSize = 32;  // Generate 32x32 regions at a time
    for (double lat = m_config.minLatitude; lat < m_config.maxLatitude; lat += chunkSize / static_cast<double>(m_config.tilesPerDegree)) {
        for (double lon = m_config.minLongitude; lon < m_config.maxLongitude; lon += chunkSize / static_cast<double>(m_config.tilesPerDegree)) {
            GenerateRegion(lat, lon, chunkSize / 2);
        }
    }

    spdlog::info("World generation complete, {} chunks generated", m_loadedChunks.size());
}

void WorldMapEditor::ExportRegion(double lat, double lon, int radiusTiles, const std::string& path) {
    spdlog::info("Exporting region at ({}, {}) radius {} to {}", lat, lon, radiusTiles, path);

    // Collect heights for the region
    int size = radiusTiles * 2 + 1;
    std::vector<float> heights(size * size, 0.0f);

    double degreesPerTile = 1.0 / m_config.tilesPerDegree;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double tileLat = lat + (y - radiusTiles) * degreesPerTile;
            double tileLon = lon + (x - radiusTiles) * degreesPerTile;
            heights[y * size + x] = GetHeightAt(tileLat, tileLon);
        }
    }

    // Write as raw heightmap
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        // Write header
        file.write(reinterpret_cast<const char*>(&size), sizeof(int));
        file.write(reinterpret_cast<const char*>(&size), sizeof(int));
        // Write height data
        file.write(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
        spdlog::info("Exported {}x{} heightmap", size, size);
    } else {
        spdlog::error("Failed to create export file: {}", path);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void WorldMapEditor::GenerateTerrain() {
    // Apply PCG graph if available, otherwise use noise-based generation
    if (m_pcgGraph && !m_pcgGraph->GetNodes().empty()) {
        ApplyPCGGraph();
    } else {
        // Generate procedural terrain for all loaded chunks
        for (auto& chunk : m_loadedChunks) {
            if (!chunk.isGenerated || chunk.needsRegeneration) {
                GenerateChunkTerrain(chunk);
            }
        }
    }
}

void WorldMapEditor::ApplyPCGGraph() {
    if (!m_pcgGraph) {
        spdlog::warn("No PCG graph loaded");
        return;
    }

    spdlog::info("Applying PCG graph to {} chunks", m_loadedChunks.size());

    for (auto& chunk : m_loadedChunks) {
        if (chunk.isGenerated && !chunk.needsRegeneration) {
            continue;
        }

        double degreesPerTile = 1.0 / m_config.tilesPerDegree;
        double chunkSizeDegrees = chunk.tileSize * degreesPerTile;
        double startLat = chunk.centerLat - chunkSizeDegrees * 0.5;
        double startLon = chunk.centerLon - chunkSizeDegrees * 0.5;

        for (int y = 0; y < chunk.tileSize; ++y) {
            for (int x = 0; x < chunk.tileSize; ++x) {
                double lat = startLat + y * degreesPerTile;
                double lon = startLon + x * degreesPerTile;

                // Create PCG context for this position
                PCG::PCGContext context;
                context.latitude = lat;
                context.longitude = lon;
                context.position = LatLongToWorld(lat, lon, 0.0f);
                context.seed = m_config.worldSeed;

                // Add real-world data if available
                if (m_hasElevationData) {
                    context.elevation = GetRealWorldElevation(lat, lon);
                }
                if (m_hasRoadData) {
                    context.roadDistance = GetRoadDistance(lat, lon);
                }
                if (m_hasBuildingData) {
                    context.buildingDistance = GetBuildingDistance(lat, lon);
                }

                // Execute the PCG graph
                m_pcgGraph->Execute(context);

                // Get height from output node (assuming node 0 or find terrain output)
                float height = 0.0f;
                for (const auto& [id, node] : m_pcgGraph->GetNodes()) {
                    if (node->GetCategory() == PCG::NodeCategory::Output ||
                        node->GetCategory() == PCG::NodeCategory::Terrain) {
                        height = node->GetFloatOutput(0);
                        break;
                    }
                }

                // Scale and clamp height
                height = height * m_generationScale;
                height = std::clamp(height, m_config.minElevation, m_config.maxElevation);

                chunk.heights[y * chunk.tileSize + x] = height;
            }
        }

        chunk.isGenerated = true;
        chunk.needsRegeneration = false;
    }

    spdlog::info("PCG graph application complete");
}

bool WorldMapEditor::LoadElevationData() {
    // Load elevation data from file path in config
    if (m_config.elevationDataPath.empty()) {
        spdlog::warn("No elevation data path specified");
        return false;
    }

    std::ifstream file(m_config.elevationDataPath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open elevation data: {}", m_config.elevationDataPath);
        return false;
    }

    // Read header (simple format: width, height, min_lat, max_lat, min_lon, max_lon)
    int width, height;
    file.read(reinterpret_cast<char*>(&width), sizeof(int));
    file.read(reinterpret_cast<char*>(&height), sizeof(int));

    // Read bounds
    double minLat, maxLat, minLon, maxLon;
    file.read(reinterpret_cast<char*>(&minLat), sizeof(double));
    file.read(reinterpret_cast<char*>(&maxLat), sizeof(double));
    file.read(reinterpret_cast<char*>(&minLon), sizeof(double));
    file.read(reinterpret_cast<char*>(&maxLon), sizeof(double));

    // Read elevation values
    m_elevationData.resize(width * height);
    file.read(reinterpret_cast<char*>(m_elevationData.data()), width * height * sizeof(float));

    m_elevationWidth = width;
    m_elevationHeight = height;
    m_elevationBounds = {minLat, maxLat, minLon, maxLon};

    spdlog::info("Loaded elevation data: {}x{} from {}", width, height, m_config.elevationDataPath);
    return true;
}

bool WorldMapEditor::LoadRoadData() {
    // Load road network data from file path in config
    if (m_config.roadDataPath.empty()) {
        spdlog::warn("No road data path specified");
        return false;
    }

    std::ifstream file(m_config.roadDataPath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open road data: {}", m_config.roadDataPath);
        return false;
    }

    // Read number of road segments
    uint32_t numSegments;
    file.read(reinterpret_cast<char*>(&numSegments), sizeof(numSegments));

    m_roadSegments.clear();
    m_roadSegments.reserve(numSegments);

    for (uint32_t i = 0; i < numSegments; ++i) {
        RoadSegment seg;
        file.read(reinterpret_cast<char*>(&seg.startLat), sizeof(double));
        file.read(reinterpret_cast<char*>(&seg.startLon), sizeof(double));
        file.read(reinterpret_cast<char*>(&seg.endLat), sizeof(double));
        file.read(reinterpret_cast<char*>(&seg.endLon), sizeof(double));
        file.read(reinterpret_cast<char*>(&seg.roadType), sizeof(int));
        m_roadSegments.push_back(seg);
    }

    spdlog::info("Loaded {} road segments from {}", numSegments, m_config.roadDataPath);
    return true;
}

bool WorldMapEditor::LoadBuildingData() {
    // Load building footprint data from file path in config
    if (m_config.buildingDataPath.empty()) {
        spdlog::warn("No building data path specified");
        return false;
    }

    std::ifstream file(m_config.buildingDataPath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open building data: {}", m_config.buildingDataPath);
        return false;
    }

    // Read number of buildings
    uint32_t numBuildings;
    file.read(reinterpret_cast<char*>(&numBuildings), sizeof(numBuildings));

    m_buildingFootprints.clear();
    m_buildingFootprints.reserve(numBuildings);

    for (uint32_t i = 0; i < numBuildings; ++i) {
        BuildingFootprint bldg;
        file.read(reinterpret_cast<char*>(&bldg.centerLat), sizeof(double));
        file.read(reinterpret_cast<char*>(&bldg.centerLon), sizeof(double));
        file.read(reinterpret_cast<char*>(&bldg.radius), sizeof(float));
        file.read(reinterpret_cast<char*>(&bldg.height), sizeof(float));
        m_buildingFootprints.push_back(bldg);
    }

    spdlog::info("Loaded {} building footprints from {}", numBuildings, m_config.buildingDataPath);
    return true;
}

bool WorldMapEditor::LoadBiomeData() {
    // Load biome classification data from file path in config
    if (m_config.biomeDataPath.empty()) {
        spdlog::warn("No biome data path specified");
        return false;
    }

    std::ifstream file(m_config.biomeDataPath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open biome data: {}", m_config.biomeDataPath);
        return false;
    }

    // Read header
    int width, height;
    file.read(reinterpret_cast<char*>(&width), sizeof(int));
    file.read(reinterpret_cast<char*>(&height), sizeof(int));

    // Read bounds
    double minLat, maxLat, minLon, maxLon;
    file.read(reinterpret_cast<char*>(&minLat), sizeof(double));
    file.read(reinterpret_cast<char*>(&maxLat), sizeof(double));
    file.read(reinterpret_cast<char*>(&minLon), sizeof(double));
    file.read(reinterpret_cast<char*>(&maxLon), sizeof(double));

    // Read biome IDs (as uint8)
    m_biomeData.resize(width * height);
    file.read(reinterpret_cast<char*>(m_biomeData.data()), width * height);

    m_biomeWidth = width;
    m_biomeHeight = height;
    m_biomeBounds = {minLat, maxLat, minLon, maxLon};

    spdlog::info("Loaded biome data: {}x{} from {}", width, height, m_config.biomeDataPath);
    return true;
}

glm::vec3 WorldMapEditor::LatLongToWorld(double latitude, double longitude, float elevation) {
    // Equirectangular projection with proper scaling
    // Earth radius approximately 6371 km
    // At the equator, 1 degree = ~111.32 km
    // At latitude L, 1 degree longitude = 111.32 * cos(L) km

    constexpr double EARTH_RADIUS_KM = 6371.0;
    constexpr double DEGREES_TO_RAD = 3.14159265358979323846 / 180.0;
    constexpr double KM_PER_DEGREE_LAT = 111.32;  // km per degree latitude (constant)

    // Scale factor - world units per km (adjustable for desired world scale)
    constexpr double WORLD_SCALE = 1.0;  // 1 world unit = 1 km

    // Convert latitude to world Z coordinate (north-south)
    // Use latitude directly scaled by km/degree
    double z = latitude * KM_PER_DEGREE_LAT * WORLD_SCALE;

    // Convert longitude to world X coordinate (east-west)
    // Account for latitude-dependent scaling
    double cosLat = std::cos(latitude * DEGREES_TO_RAD);
    double kmPerDegreeLon = KM_PER_DEGREE_LAT * cosLat;
    double x = longitude * kmPerDegreeLon * WORLD_SCALE;

    // Y is elevation (meters converted to km, then scaled)
    double y = (elevation / 1000.0) * WORLD_SCALE;

    return glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

void WorldMapEditor::WorldToLatLong(const glm::vec3& world, double& latitude, double& longitude) {
    // Inverse of LatLongToWorld
    constexpr double DEGREES_TO_RAD = 3.14159265358979323846 / 180.0;
    constexpr double KM_PER_DEGREE_LAT = 111.32;
    constexpr double WORLD_SCALE = 1.0;

    // Convert Z back to latitude
    latitude = world.z / (KM_PER_DEGREE_LAT * WORLD_SCALE);

    // Convert X back to longitude (accounting for latitude)
    double cosLat = std::cos(latitude * DEGREES_TO_RAD);
    double kmPerDegreeLon = KM_PER_DEGREE_LAT * cosLat;
    if (kmPerDegreeLon > 0.001) {
        longitude = world.x / (kmPerDegreeLon * WORLD_SCALE);
    } else {
        longitude = 0.0;  // Near poles
    }
}

WorldMapEditor::TerrainChunk* WorldMapEditor::FindChunkAt(double latitude, double longitude) {
    double degreesPerTile = 1.0 / m_config.tilesPerDegree;

    for (auto& chunk : m_loadedChunks) {
        double halfSize = (chunk.tileSize * degreesPerTile) * 0.5;
        if (latitude >= chunk.centerLat - halfSize && latitude <= chunk.centerLat + halfSize &&
            longitude >= chunk.centerLon - halfSize && longitude <= chunk.centerLon + halfSize) {
            return &chunk;
        }
    }
    return nullptr;
}

void WorldMapEditor::GenerateChunkTerrain(TerrainChunk& chunk) {
    // Generate terrain using noise if no PCG graph, otherwise use real-world data
    double degreesPerTile = 1.0 / m_config.tilesPerDegree;
    double chunkSizeDegrees = chunk.tileSize * degreesPerTile;
    double startLat = chunk.centerLat - chunkSizeDegrees * 0.5;
    double startLon = chunk.centerLon - chunkSizeDegrees * 0.5;

    // Simple multi-octave noise for procedural terrain
    auto noise = [](double x, double y, uint64_t seed) -> float {
        // Simple hash-based noise
        auto hash = [](int x, int y, uint64_t seed) -> float {
            uint64_t h = seed;
            h ^= x * 374761393ULL;
            h ^= y * 668265263ULL;
            h = (h ^ (h >> 13)) * 1274126177ULL;
            return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
        };

        int xi = static_cast<int>(std::floor(x));
        int yi = static_cast<int>(std::floor(y));
        float xf = static_cast<float>(x - xi);
        float yf = static_cast<float>(y - yi);

        // Smooth interpolation
        float u = xf * xf * (3.0f - 2.0f * xf);
        float v = yf * yf * (3.0f - 2.0f * yf);

        // Bilinear interpolation of corner values
        float n00 = hash(xi, yi, seed);
        float n10 = hash(xi + 1, yi, seed);
        float n01 = hash(xi, yi + 1, seed);
        float n11 = hash(xi + 1, yi + 1, seed);

        float nx0 = n00 * (1.0f - u) + n10 * u;
        float nx1 = n01 * (1.0f - u) + n11 * u;
        return nx0 * (1.0f - v) + nx1 * v;
    };

    for (int y = 0; y < chunk.tileSize; ++y) {
        for (int x = 0; x < chunk.tileSize; ++x) {
            double lat = startLat + y * degreesPerTile;
            double lon = startLon + x * degreesPerTile;

            float height = 0.0f;

            // If we have real-world elevation data, use it
            if (m_useRealWorldElevation && m_hasElevationData) {
                height = GetRealWorldElevation(lat, lon);
            } else {
                // Generate procedural terrain using multi-octave noise
                float amplitude = 1.0f;
                float frequency = 0.01f * m_generationScale;
                float total = 0.0f;
                float maxValue = 0.0f;

                for (int octave = 0; octave < 6; ++octave) {
                    total += noise(lon * frequency, lat * frequency, m_config.worldSeed + octave) * amplitude;
                    maxValue += amplitude;
                    amplitude *= 0.5f;
                    frequency *= 2.0f;
                }

                // Normalize and scale to elevation range
                height = (total / maxValue) * (m_config.maxElevation - m_config.minElevation) + m_config.minElevation;
            }

            // Apply road avoidance
            if (m_avoidRoads && m_hasRoadData) {
                float roadDist = GetRoadDistance(lat, lon);
                if (roadDist < 0.001) {  // On road
                    height = std::min(height, 0.0f);  // Flatten to road level
                }
            }

            // Apply building avoidance
            if (m_avoidBuildings && m_hasBuildingData) {
                float buildingDist = GetBuildingDistance(lat, lon);
                if (buildingDist < 0.0001) {  // Inside building
                    height = 0.0f;  // Flatten building area
                }
            }

            chunk.heights[y * chunk.tileSize + x] = height;
        }
    }

    chunk.isGenerated = true;
    chunk.needsRegeneration = false;
}

float WorldMapEditor::GetRealWorldElevation(double latitude, double longitude) {
    if (m_elevationData.empty() || m_elevationWidth == 0 || m_elevationHeight == 0) {
        return 0.0f;
    }

    // Check if position is within elevation data bounds
    if (latitude < m_elevationBounds.minLat || latitude > m_elevationBounds.maxLat ||
        longitude < m_elevationBounds.minLon || longitude > m_elevationBounds.maxLon) {
        return 0.0f;
    }

    // Normalize to 0-1 within bounds
    double normLat = (latitude - m_elevationBounds.minLat) / (m_elevationBounds.maxLat - m_elevationBounds.minLat);
    double normLon = (longitude - m_elevationBounds.minLon) / (m_elevationBounds.maxLon - m_elevationBounds.minLon);

    // Convert to grid indices
    double gx = normLon * (m_elevationWidth - 1);
    double gy = normLat * (m_elevationHeight - 1);

    // Bilinear interpolation
    int x0 = static_cast<int>(gx);
    int y0 = static_cast<int>(gy);
    int x1 = std::min(x0 + 1, m_elevationWidth - 1);
    int y1 = std::min(y0 + 1, m_elevationHeight - 1);

    float fx = static_cast<float>(gx - x0);
    float fy = static_cast<float>(gy - y0);

    float e00 = m_elevationData[y0 * m_elevationWidth + x0];
    float e10 = m_elevationData[y0 * m_elevationWidth + x1];
    float e01 = m_elevationData[y1 * m_elevationWidth + x0];
    float e11 = m_elevationData[y1 * m_elevationWidth + x1];

    float e0 = e00 * (1.0f - fx) + e10 * fx;
    float e1 = e01 * (1.0f - fx) + e11 * fx;
    return e0 * (1.0f - fy) + e1 * fy;
}

float WorldMapEditor::GetRoadDistance(double latitude, double longitude) {
    if (m_roadSegments.empty()) {
        return 999.0f;  // No roads = very far
    }

    float minDist = 999.0f;

    for (const auto& seg : m_roadSegments) {
        // Calculate distance from point to line segment
        // Using simple Euclidean approximation (valid for small areas)
        double dx = seg.endLon - seg.startLon;
        double dy = seg.endLat - seg.startLat;
        double segLenSq = dx * dx + dy * dy;

        float dist;
        if (segLenSq < 1e-10) {
            // Segment is a point
            double pdx = longitude - seg.startLon;
            double pdy = latitude - seg.startLat;
            dist = static_cast<float>(std::sqrt(pdx * pdx + pdy * pdy));
        } else {
            // Project point onto segment
            double t = ((longitude - seg.startLon) * dx + (latitude - seg.startLat) * dy) / segLenSq;
            t = std::clamp(t, 0.0, 1.0);

            double projLon = seg.startLon + t * dx;
            double projLat = seg.startLat + t * dy;

            double pdx = longitude - projLon;
            double pdy = latitude - projLat;
            dist = static_cast<float>(std::sqrt(pdx * pdx + pdy * pdy));
        }

        // Convert degrees to approximate km (rough approximation)
        dist *= 111.0f;  // ~111 km per degree

        minDist = std::min(minDist, dist);
    }

    return minDist;
}

float WorldMapEditor::GetBuildingDistance(double latitude, double longitude) {
    if (m_buildingFootprints.empty()) {
        return 999.0f;  // No buildings = very far
    }

    float minDist = 999.0f;

    for (const auto& bldg : m_buildingFootprints) {
        double dx = longitude - bldg.centerLon;
        double dy = latitude - bldg.centerLat;
        float dist = static_cast<float>(std::sqrt(dx * dx + dy * dy));

        // Convert to km and subtract building radius
        dist = dist * 111.0f - bldg.radius;
        if (dist < 0) dist = 0;

        minDist = std::min(minDist, dist);
    }

    return minDist;
}
