#include "WorldMapEditor.hpp"
#include "ModernUI.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>

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
    // TODO: Implement world loading
    m_currentWorldPath = path;
    return false;
}

bool WorldMapEditor::SaveWorld(const std::string& path) {
    spdlog::info("Saving world to: {}", path);
    // TODO: Implement world saving
    m_currentWorldPath = path;
    return false;
}

void WorldMapEditor::GenerateRegion(double lat, double lon, int radiusTiles) {
    spdlog::info("Generating region at ({}, {}) with radius {} tiles", lat, lon, radiusTiles);
    // TODO: Implement region generation using PCG graph
}

float WorldMapEditor::GetHeightAt(double latitude, double longitude) {
    // TODO: Implement height lookup from terrain chunks
    return 0.0f;
}

void WorldMapEditor::SetHeightAt(double latitude, double longitude, float height) {
    // TODO: Implement height modification
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
                // TODO: Open file dialog
            }
            if (ImGui::MenuItem("Save World", "Ctrl+S", false, !m_currentWorldPath.empty())) {
                if (!m_currentWorldPath.empty()) {
                    SaveWorld(m_currentWorldPath);
                }
            }
            if (ImGui::MenuItem("Save World As...", "Ctrl+Shift+S")) {
                // TODO: Save file dialog
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Export Region...")) {
                // TODO: Export region dialog
            }
            if (ImGui::MenuItem("Import Real-World Data...")) {
                // TODO: Import data dialog
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
                // TODO: World properties dialog
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
            if (ImGui::MenuItem("Generate Entire World", nullptr, false, false)) {
                // TODO: Batch generation
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Clear All Chunks")) {
                m_loadedChunks.clear();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
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
            // TODO: Load graph dialog
        }

        if (ModernUI::GlowButton("Save PCG Graph...", ImVec2(-1, 0))) {
            // TODO: Save graph dialog
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
                // TODO: Load elevation data
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
                // TODO: Load road data
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
                // TODO: Load building data
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
// Private Methods
// ============================================================================

void WorldMapEditor::GenerateTerrain() {
    // TODO: Implement terrain generation
}

void WorldMapEditor::ApplyPCGGraph() {
    // TODO: Apply PCG graph to generate terrain
}

bool WorldMapEditor::LoadElevationData() {
    // TODO: Load elevation data from file
    return false;
}

bool WorldMapEditor::LoadRoadData() {
    // TODO: Load road data from file
    return false;
}

bool WorldMapEditor::LoadBuildingData() {
    // TODO: Load building data from file
    return false;
}

bool WorldMapEditor::LoadBiomeData() {
    // TODO: Load biome data from file
    return false;
}

glm::vec3 WorldMapEditor::LatLongToWorld(double latitude, double longitude, float elevation) {
    // Simple equirectangular projection
    // TODO: Implement proper coordinate conversion
    float x = static_cast<float>(longitude);
    float y = elevation;
    float z = static_cast<float>(latitude);
    return glm::vec3(x, y, z);
}

void WorldMapEditor::WorldToLatLong(const glm::vec3& world, double& latitude, double& longitude) {
    // Inverse of LatLongToWorld
    longitude = static_cast<double>(world.x);
    latitude = static_cast<double>(world.z);
}
