#include "LocalMapEditor.hpp"
#include "ModernUI.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>

LocalMapEditor::LocalMapEditor() = default;
LocalMapEditor::~LocalMapEditor() = default;

bool LocalMapEditor::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing Local Map Editor");

    // Create empty PCG graph
    m_pcgGraph = std::make_unique<PCG::PCGGraph>();

    m_initialized = true;
    spdlog::info("Local Map Editor initialized successfully");
    return true;
}

void LocalMapEditor::Shutdown() {
    spdlog::info("Shutting down Local Map Editor");
    m_initialized = false;
}

void LocalMapEditor::Update(float deltaTime) {
    // Empty for now - will handle camera movement, tool updates, etc.
}

void LocalMapEditor::RenderUI() {
    if (!m_initialized) {
        return;
    }

    // Main window with glassmorphic styling
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    if (ImGui::Begin("Local Map Editor", nullptr, windowFlags)) {
        RenderMenuBar();
        RenderToolbar();

        // Main content area - split into panels
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

        // Left panel - Tools
        ImGui::BeginChild("LeftPanel", ImVec2(300, 0), true);
        if (m_showTerrainPanel) {
            RenderTerrainPanel();
        }
        if (m_showAssetPanel) {
            RenderAssetPanel();
        }
        if (m_showPCGPanel) {
            RenderPCGPanel();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel - 3D View
        ImGui::BeginChild("CenterPanel", ImVec2(0, -300), true);
        ModernUI::GradientHeader("3D Map View");

        const char* modeNames[] = {
            "Terrain Paint", "Terrain Sculpt", "Asset Place", "Spawn Place", "Objective Place"
        };
        ImGui::Text("Edit Mode: %s", modeNames[(int)m_editMode]);
        ImGui::Text("Map Size: %dx%d, Camera: (%.1f, %.1f, %.1f)",
                   m_config.width, m_config.height,
                   m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);

        // Placeholder for 3D view
        ImVec2 viewSize = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("3DView", viewSize);

        ImGui::EndChild();

        // Bottom panel - Spawn Points, Objectives, Properties
        ImGui::BeginChild("BottomPanel", ImVec2(0, 0), false);

        ImGui::Columns(3, "BottomColumns", true);

        // Spawn Points Panel
        if (m_showSpawnPoints) {
            RenderSpawnPointsPanel();
        }

        ImGui::NextColumn();

        // Objectives Panel
        if (m_showObjectives) {
            RenderObjectivesPanel();
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
}

void LocalMapEditor::Render3D() {
    // Empty for now - will render map, terrain, assets, etc.
}

void LocalMapEditor::NewMap(const LocalMapConfig& config) {
    spdlog::info("Creating new local map: {}x{}", config.width, config.height);
    m_config = config;
    m_currentMapPath.clear();

    // Initialize terrain data
    int totalTiles = config.width * config.height;
    m_heights.resize(totalTiles, 0.0f);
    m_terrainTypes.resize(totalTiles, 0);

    // Clear assets and objects
    m_assets.clear();
    m_spawnPoints.clear();
    m_objectives.clear();

    // Reset camera
    m_cameraPos = glm::vec3(config.width * 0.5f, 20.0f, config.height * 0.5f);
    m_cameraTarget = glm::vec3(config.width * 0.5f, 0.0f, config.height * 0.5f);

    spdlog::info("New local map created");
}

bool LocalMapEditor::LoadMap(const std::string& path) {
    spdlog::info("Loading local map from: {}", path);
    // TODO: Implement map loading
    m_currentMapPath = path;
    return false;
}

bool LocalMapEditor::SaveMap(const std::string& path) {
    spdlog::info("Saving local map to: {}", path);
    // TODO: Implement map saving
    m_currentMapPath = path;
    return false;
}

void LocalMapEditor::GenerateTerrain() {
    spdlog::info("Generating terrain using PCG graph");
    // TODO: Implement PCG-based terrain generation
}

void LocalMapEditor::PaintTerrain(int x, int y, const std::string& terrainType) {
    // TODO: Implement terrain painting
}

void LocalMapEditor::SculptTerrain(int x, int y, float strength, bool raise) {
    // TODO: Implement terrain sculpting
}

void LocalMapEditor::PlaceAsset(const glm::vec3& position, const std::string& assetType) {
    PlacedAsset asset;
    asset.type = assetType;
    asset.position = position;
    asset.rotation = glm::vec3(0.0f);
    asset.scale = glm::vec3(1.0f);
    m_assets.push_back(asset);
    spdlog::info("Placed {} at ({}, {}, {})", assetType, position.x, position.y, position.z);
}

float LocalMapEditor::GetHeightAt(int x, int y) {
    if (x < 0 || x >= m_config.width || y < 0 || y >= m_config.height) {
        return 0.0f;
    }
    int index = y * m_config.width + x;
    if (index >= 0 && index < m_heights.size()) {
        return m_heights[index];
    }
    return 0.0f;
}

// ============================================================================
// UI Panels
// ============================================================================

void LocalMapEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Map", "Ctrl+N")) {
                LocalMapConfig config;
                NewMap(config);
            }
            if (ImGui::MenuItem("Open Map...", "Ctrl+O")) {
                // TODO: Open file dialog
            }
            if (ImGui::MenuItem("Save Map", "Ctrl+S", false, !m_currentMapPath.empty())) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                }
            }
            if (ImGui::MenuItem("Save Map As...", "Ctrl+Shift+S")) {
                // TODO: Save file dialog
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Export...", "")) {
                // TODO: Export dialog
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
            if (ImGui::MenuItem("Map Properties...")) {
                // TODO: Map properties dialog
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Terrain Panel", nullptr, &m_showTerrainPanel);
            ImGui::MenuItem("Asset Panel", nullptr, &m_showAssetPanel);
            ImGui::MenuItem("PCG Panel", nullptr, &m_showPCGPanel);
            ImGui::MenuItem("Spawn Points", nullptr, &m_showSpawnPoints);
            ImGui::MenuItem("Objectives", nullptr, &m_showObjectives);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Reset Camera")) {
                m_cameraPos = glm::vec3(m_config.width * 0.5f, 20.0f, m_config.height * 0.5f);
                m_cameraTarget = glm::vec3(m_config.width * 0.5f, 0.0f, m_config.height * 0.5f);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Generate Terrain", nullptr, false)) {
                GenerateTerrain();
            }
            if (ImGui::MenuItem("Flatten Terrain")) {
                FlattenTerrain();
            }
            if (ImGui::MenuItem("Smooth Terrain")) {
                SmoothTerrain();
            }
            if (ImGui::MenuItem("Add Noise to Terrain")) {
                NoiseTerrain();
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Clear All Assets")) {
                m_assets.clear();
            }
            if (ImGui::MenuItem("Clear All Spawns")) {
                m_spawnPoints.clear();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void LocalMapEditor::RenderToolbar() {
    ModernUI::BeginGlassCard("Toolbar", ImVec2(0, 40));

    if (ModernUI::GlowButton("New", ImVec2(60, 0))) {
        LocalMapConfig config;
        NewMap(config);
    }
    ImGui::SameLine();
    if (ModernUI::GlowButton("Save", ImVec2(60, 0))) {
        if (!m_currentMapPath.empty()) {
            SaveMap(m_currentMapPath);
        }
    }
    ImGui::SameLine();
    if (ModernUI::GlowButton("Load", ImVec2(60, 0))) {
        // TODO: Load dialog
    }

    ImGui::SameLine();
    ModernUI::GradientSeparator();
    ImGui::SameLine();

    ImGui::Text("Brush Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("##BrushSize", &m_brushSize, 1, 10);

    ImGui::SameLine();
    ImGui::Text("Strength:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##BrushStrength", &m_brushStrength, 0.1f, 2.0f);

    ModernUI::EndGlassCard();
}

void LocalMapEditor::RenderTerrainPanel() {
    if (ModernUI::GradientHeader("Terrain Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("TerrainContent");

        ImGui::Text("Edit Mode");
        ModernUI::GradientSeparator(0.3f);

        if (ModernUI::GlowButton("Paint Terrain", ImVec2(-1, 0))) {
            m_editMode = EditMode::TerrainPaint;
        }
        if (ModernUI::GlowButton("Sculpt Terrain", ImVec2(-1, 0))) {
            m_editMode = EditMode::TerrainSculpt;
        }

        ModernUI::GradientSeparator(0.3f);

        if (m_editMode == EditMode::TerrainPaint) {
            ImGui::Text("Terrain Types");
            const char* terrainTypes[] = {"Grass", "Dirt", "Stone", "Sand", "Water"};
            for (int i = 0; i < 5; i++) {
                bool selected = (m_selectedTerrainType == terrainTypes[i]);
                if (ModernUI::GlowSelectable(terrainTypes[i], selected)) {
                    m_selectedTerrainType = terrainTypes[i];
                }
            }
        } else if (m_editMode == EditMode::TerrainSculpt) {
            ImGui::Text("Sculpt Tools");
            if (ModernUI::GlowButton("Raise", ImVec2(-1, 0))) {
                // Set raise mode
            }
            if (ModernUI::GlowButton("Lower", ImVec2(-1, 0))) {
                // Set lower mode
            }
            if (ModernUI::GlowButton("Flatten", ImVec2(-1, 0))) {
                FlattenTerrain();
            }
            if (ModernUI::GlowButton("Smooth", ImVec2(-1, 0))) {
                SmoothTerrain();
            }
        }

        ModernUI::EndGlassCard();
    }
}

void LocalMapEditor::RenderAssetPanel() {
    if (ModernUI::GradientHeader("Asset Placement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("AssetContent");

        if (ModernUI::GlowButton("Place Mode", ImVec2(-1, 0))) {
            m_editMode = EditMode::AssetPlace;
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Asset Types");
        const char* assetTypes[] = {"Tree", "Rock", "Building", "Resource Node", "Decoration"};
        for (int i = 0; i < 5; i++) {
            bool selected = (m_selectedAssetType == assetTypes[i]);
            if (ModernUI::GlowSelectable(assetTypes[i], selected)) {
                m_selectedAssetType = assetTypes[i];
            }
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Batch Placement");
        if (ModernUI::GlowButton("Place 10 Trees", ImVec2(-1, 0))) {
            PlaceTrees(10);
        }
        if (ModernUI::GlowButton("Place 20 Rocks", ImVec2(-1, 0))) {
            PlaceRocks(20);
        }
        if (ModernUI::GlowButton("Place 5 Resources", ImVec2(-1, 0))) {
            PlaceResources(5);
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Total Assets: %zu", m_assets.size());

        ModernUI::EndGlassCard();
    }
}

void LocalMapEditor::RenderPCGPanel() {
    if (ModernUI::GradientHeader("PCG Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PCGContent");

        ImGui::Text("Procedural Generation");
        ModernUI::GradientSeparator(0.3f);

        if (!m_config.pcgGraphPath.empty()) {
            ImGui::Text("Graph:");
            ImGui::TextWrapped("%s", m_config.pcgGraphPath.c_str());
        } else {
            ImGui::TextDisabled("No PCG graph loaded");
        }

        if (ModernUI::GlowButton("Load PCG Graph...", ImVec2(-1, 0))) {
            // TODO: Load graph dialog
        }

        if (ModernUI::GlowButton("Generate Terrain", ImVec2(-1, 0))) {
            GenerateTerrain();
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Seed:");
        ImGui::SetNextItemWidth(-1);
        uint64_t seed = m_config.seed;
        if (ImGui::InputScalar("##Seed", ImGuiDataType_U64, &seed)) {
            m_config.seed = seed;
        }

        ModernUI::EndGlassCard();
    }
}

void LocalMapEditor::RenderSpawnPointsPanel() {
    if (ModernUI::GradientHeader("Spawn Points", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("SpawnContent");

        if (ModernUI::GlowButton("Spawn Mode", ImVec2(-1, 0))) {
            m_editMode = EditMode::SpawnPlace;
        }

        ModernUI::GradientSeparator(0.3f);

        if (ModernUI::GlowButton("Add Player Spawn", ImVec2(-1, 0))) {
            AddPlayerSpawn(glm::vec3(m_config.width * 0.5f, 0, m_config.height * 0.5f));
        }

        if (ModernUI::GlowButton("Add Enemy Spawn", ImVec2(-1, 0))) {
            AddEnemySpawn(glm::vec3(m_config.width * 0.5f, 0, m_config.height * 0.5f));
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Spawn List (%zu)", m_spawnPoints.size());

        ImGui::BeginChild("SpawnList", ImVec2(0, 100), true);
        for (int i = 0; i < m_spawnPoints.size(); i++) {
            const auto& spawn = m_spawnPoints[i];
            const char* typeNames[] = {"Player", "Enemy", "NPC", "Boss"};
            ImGui::PushID(i);
            if (ModernUI::GlowSelectable(
                (std::string(typeNames[(int)spawn.type]) + " ##" + std::to_string(i)).c_str(),
                m_selectedSpawnIndex == i)) {
                m_selectedSpawnIndex = i;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (m_selectedSpawnIndex >= 0 && m_selectedSpawnIndex < m_spawnPoints.size()) {
            if (ModernUI::GlowButton("Remove Selected", ImVec2(-1, 0))) {
                RemoveSpawn(m_selectedSpawnIndex);
                m_selectedSpawnIndex = -1;
            }
        }

        ModernUI::EndGlassCard();
    }
}

void LocalMapEditor::RenderObjectivesPanel() {
    if (ModernUI::GradientHeader("Objectives", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("ObjectiveContent");

        if (ModernUI::GlowButton("Objective Mode", ImVec2(-1, 0))) {
            m_editMode = EditMode::ObjectivePlace;
        }

        ModernUI::GradientSeparator(0.3f);

        const char* objectiveTypes[] = {"Capture", "Defend", "Escort", "Collect", "Destroy"};

        for (const char* type : objectiveTypes) {
            if (ModernUI::GlowButton(type, ImVec2(-1, 0))) {
                glm::vec3 pos(m_config.width * 0.5f, 0, m_config.height * 0.5f);
                AddObjective(type, pos);
            }
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Objective List (%zu)", m_objectives.size());

        ImGui::BeginChild("ObjectiveList", ImVec2(0, 100), true);
        for (int i = 0; i < m_objectives.size(); i++) {
            const auto& obj = m_objectives[i];
            ImGui::PushID(i);
            if (ModernUI::GlowSelectable(
                (obj.type + " ##" + std::to_string(i)).c_str(), false)) {
                // Select objective
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ModernUI::EndGlassCard();
    }
}

void LocalMapEditor::RenderPropertiesPanel() {
    if (ModernUI::GradientHeader("Map Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PropertiesContent");

        ImGui::Text("Map Type");
        ModernUI::GradientSeparator(0.3f);

        const char* mapTypeNames[] = {"Arena", "Dungeon", "Housing", "Scenario", "Tutorial"};
        int currentType = (int)m_config.type;
        if (ImGui::Combo("##MapType", &currentType, mapTypeNames, 5)) {
            m_config.type = (LocalMapConfig::MapType)currentType;
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Theme");
        const char* themes[] = {"default", "grass", "desert", "snow", "lava", "swamp"};
        for (const char* theme : themes) {
            bool selected = (m_config.theme == theme);
            if (ModernUI::GlowSelectable(theme, selected)) {
                m_config.theme = theme;
            }
        }

        ModernUI::GradientSeparator(0.3f);

        ImGui::Text("Dimensions");
        ModernUI::CompactStat("Width", std::to_string(m_config.width).c_str());
        ModernUI::CompactStat("Height", std::to_string(m_config.height).c_str());
        ModernUI::CompactStat("Tile Size", std::to_string(m_config.tileSize).c_str());

        ModernUI::GradientSeparator(0.3f);

        ImGui::Checkbox("Inherit from World", &m_config.inheritFromWorld);
        if (m_config.inheritFromWorld) {
            ImGui::Indent();
            ImGui::Text("Lat: %.4f", m_config.sourceLatitude);
            ImGui::Text("Lon: %.4f", m_config.sourceLongitude);
            ImGui::Unindent();
        }

        ModernUI::EndGlassCard();
    }
}

// ============================================================================
// Private Methods - Terrain Operations
// ============================================================================

void LocalMapEditor::FlattenTerrain() {
    spdlog::info("Flattening terrain");
    std::fill(m_heights.begin(), m_heights.end(), 0.0f);
}

void LocalMapEditor::SmoothTerrain() {
    spdlog::info("Smoothing terrain");
    // TODO: Implement terrain smoothing algorithm
}

void LocalMapEditor::NoiseTerrain() {
    spdlog::info("Adding noise to terrain");
    // TODO: Implement noise generation
}

// ============================================================================
// Private Methods - Asset Operations
// ============================================================================

void LocalMapEditor::PlaceTrees(int count) {
    spdlog::info("Placing {} trees", count);
    for (int i = 0; i < count; i++) {
        float x = (rand() % m_config.width) * m_config.tileSize;
        float z = (rand() % m_config.height) * m_config.tileSize;
        float y = GetHeightAt((int)(x / m_config.tileSize), (int)(z / m_config.tileSize));
        PlaceAsset(glm::vec3(x, y, z), "tree");
    }
}

void LocalMapEditor::PlaceRocks(int count) {
    spdlog::info("Placing {} rocks", count);
    for (int i = 0; i < count; i++) {
        float x = (rand() % m_config.width) * m_config.tileSize;
        float z = (rand() % m_config.height) * m_config.tileSize;
        float y = GetHeightAt((int)(x / m_config.tileSize), (int)(z / m_config.tileSize));
        PlaceAsset(glm::vec3(x, y, z), "rock");
    }
}

void LocalMapEditor::PlaceResources(int count) {
    spdlog::info("Placing {} resource nodes", count);
    for (int i = 0; i < count; i++) {
        float x = (rand() % m_config.width) * m_config.tileSize;
        float z = (rand() % m_config.height) * m_config.tileSize;
        float y = GetHeightAt((int)(x / m_config.tileSize), (int)(z / m_config.tileSize));
        PlaceAsset(glm::vec3(x, y, z), "resource");
    }
}

// ============================================================================
// Private Methods - Spawn Points
// ============================================================================

void LocalMapEditor::AddPlayerSpawn(const glm::vec3& position) {
    SpawnPoint spawn;
    spawn.type = SpawnPoint::Type::Player;
    spawn.position = position;
    spawn.faction = "player";
    m_spawnPoints.push_back(spawn);
    spdlog::info("Added player spawn at ({}, {}, {})", position.x, position.y, position.z);
}

void LocalMapEditor::AddEnemySpawn(const glm::vec3& position) {
    SpawnPoint spawn;
    spawn.type = SpawnPoint::Type::Enemy;
    spawn.position = position;
    spawn.faction = "enemy";
    m_spawnPoints.push_back(spawn);
    spdlog::info("Added enemy spawn at ({}, {}, {})", position.x, position.y, position.z);
}

void LocalMapEditor::RemoveSpawn(int index) {
    if (index >= 0 && index < m_spawnPoints.size()) {
        m_spawnPoints.erase(m_spawnPoints.begin() + index);
        spdlog::info("Removed spawn point at index {}", index);
    }
}

// ============================================================================
// Private Methods - Objectives
// ============================================================================

void LocalMapEditor::AddObjective(const std::string& type, const glm::vec3& position) {
    Objective obj;
    obj.type = type;
    obj.position = position;
    obj.description = type + " objective";
    m_objectives.push_back(obj);
    spdlog::info("Added {} objective at ({}, {}, {})", type, position.x, position.y, position.z);
}
