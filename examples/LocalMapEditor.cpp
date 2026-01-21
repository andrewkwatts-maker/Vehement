#include "LocalMapEditor.hpp"
#include "ModernUI.hpp"
#include "engine/core/json_wrapper.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <fstream>
#include <cstring>

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

    // Render modal dialogs
    RenderOpenDialog();
    RenderSaveDialog();
    RenderExportDialog();
    RenderMapPropertiesDialog();
    RenderLoadPCGDialog();
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

    try {
        auto jsonOpt = Nova::Json::TryParseFile(path);
        if (!jsonOpt.has_value()) {
            spdlog::error("Failed to parse map file: {}", path);
            return false;
        }

        const auto& json = jsonOpt.value();

        // Load config
        if (json.contains("config")) {
            const auto& configJson = json["config"];
            m_config.width = Nova::Json::Get(configJson, "width", 256);
            m_config.height = Nova::Json::Get(configJson, "height", 256);
            m_config.tileSize = Nova::Json::Get(configJson, "tileSize", 1.0f);
            m_config.type = static_cast<LocalMapConfig::MapType>(
                Nova::Json::Get(configJson, "type", 0));
            m_config.inheritFromWorld = Nova::Json::Get(configJson, "inheritFromWorld", false);
            m_config.sourceLatitude = Nova::Json::Get(configJson, "sourceLatitude", 0.0);
            m_config.sourceLongitude = Nova::Json::Get(configJson, "sourceLongitude", 0.0);
            m_config.pcgGraphPath = Nova::Json::Get<std::string>(configJson, "pcgGraphPath", "");
            m_config.seed = Nova::Json::Get(configJson, "seed", static_cast<uint64_t>(12345));
            m_config.minHeight = Nova::Json::Get(configJson, "minHeight", 0.0f);
            m_config.maxHeight = Nova::Json::Get(configJson, "maxHeight", 50.0f);
            m_config.theme = Nova::Json::Get<std::string>(configJson, "theme", "default");
        }

        // Load terrain heights
        int totalTiles = m_config.width * m_config.height;
        m_heights.resize(totalTiles, 0.0f);
        m_terrainTypes.resize(totalTiles, 0);

        if (json.contains("heights") && json["heights"].is_array()) {
            const auto& heightsArray = json["heights"];
            for (size_t i = 0; i < heightsArray.size() && i < m_heights.size(); ++i) {
                m_heights[i] = heightsArray[i].get<float>();
            }
        }

        if (json.contains("terrainTypes") && json["terrainTypes"].is_array()) {
            const auto& typesArray = json["terrainTypes"];
            for (size_t i = 0; i < typesArray.size() && i < m_terrainTypes.size(); ++i) {
                m_terrainTypes[i] = typesArray[i].get<int>();
            }
        }

        // Load assets
        m_assets.clear();
        if (json.contains("assets") && json["assets"].is_array()) {
            for (const auto& assetJson : json["assets"]) {
                PlacedAsset asset;
                asset.type = Nova::Json::Get<std::string>(assetJson, "type", "");
                if (assetJson.contains("position") && assetJson["position"].is_array()) {
                    asset.position.x = assetJson["position"][0].get<float>();
                    asset.position.y = assetJson["position"][1].get<float>();
                    asset.position.z = assetJson["position"][2].get<float>();
                }
                if (assetJson.contains("rotation") && assetJson["rotation"].is_array()) {
                    asset.rotation.x = assetJson["rotation"][0].get<float>();
                    asset.rotation.y = assetJson["rotation"][1].get<float>();
                    asset.rotation.z = assetJson["rotation"][2].get<float>();
                }
                if (assetJson.contains("scale") && assetJson["scale"].is_array()) {
                    asset.scale.x = assetJson["scale"][0].get<float>();
                    asset.scale.y = assetJson["scale"][1].get<float>();
                    asset.scale.z = assetJson["scale"][2].get<float>();
                } else {
                    asset.scale = glm::vec3(1.0f);
                }
                m_assets.push_back(asset);
            }
        }

        // Load spawn points
        m_spawnPoints.clear();
        if (json.contains("spawnPoints") && json["spawnPoints"].is_array()) {
            for (const auto& spawnJson : json["spawnPoints"]) {
                SpawnPoint spawn;
                spawn.type = static_cast<SpawnPoint::Type>(
                    Nova::Json::Get(spawnJson, "type", 0));
                if (spawnJson.contains("position") && spawnJson["position"].is_array()) {
                    spawn.position.x = spawnJson["position"][0].get<float>();
                    spawn.position.y = spawnJson["position"][1].get<float>();
                    spawn.position.z = spawnJson["position"][2].get<float>();
                }
                spawn.faction = Nova::Json::Get<std::string>(spawnJson, "faction", "");
                m_spawnPoints.push_back(spawn);
            }
        }

        // Load objectives
        m_objectives.clear();
        if (json.contains("objectives") && json["objectives"].is_array()) {
            for (const auto& objJson : json["objectives"]) {
                Objective obj;
                obj.type = Nova::Json::Get<std::string>(objJson, "type", "");
                if (objJson.contains("position") && objJson["position"].is_array()) {
                    obj.position.x = objJson["position"][0].get<float>();
                    obj.position.y = objJson["position"][1].get<float>();
                    obj.position.z = objJson["position"][2].get<float>();
                }
                obj.description = Nova::Json::Get<std::string>(objJson, "description", "");
                m_objectives.push_back(obj);
            }
        }

        // Reset camera to center of map
        m_cameraPos = glm::vec3(m_config.width * 0.5f, 20.0f, m_config.height * 0.5f);
        m_cameraTarget = glm::vec3(m_config.width * 0.5f, 0.0f, m_config.height * 0.5f);

        m_currentMapPath = path;
        spdlog::info("Local map loaded successfully: {} assets, {} spawns, {} objectives",
            m_assets.size(), m_spawnPoints.size(), m_objectives.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception loading map: {}", e.what());
        return false;
    }
}

bool LocalMapEditor::SaveMap(const std::string& path) {
    spdlog::info("Saving local map to: {}", path);

    try {
        Nova::Json::JsonValue json = Nova::Json::Object();

        // Save config
        Nova::Json::JsonValue configJson = Nova::Json::Object();
        configJson["width"] = m_config.width;
        configJson["height"] = m_config.height;
        configJson["tileSize"] = m_config.tileSize;
        configJson["type"] = static_cast<int>(m_config.type);
        configJson["inheritFromWorld"] = m_config.inheritFromWorld;
        configJson["sourceLatitude"] = m_config.sourceLatitude;
        configJson["sourceLongitude"] = m_config.sourceLongitude;
        configJson["pcgGraphPath"] = m_config.pcgGraphPath;
        configJson["seed"] = m_config.seed;
        configJson["minHeight"] = m_config.minHeight;
        configJson["maxHeight"] = m_config.maxHeight;
        configJson["theme"] = m_config.theme;
        json["config"] = configJson;

        // Save terrain heights
        Nova::Json::JsonValue heightsArray = Nova::Json::Array();
        for (float h : m_heights) {
            heightsArray.push_back(h);
        }
        json["heights"] = heightsArray;

        // Save terrain types
        Nova::Json::JsonValue typesArray = Nova::Json::Array();
        for (int t : m_terrainTypes) {
            typesArray.push_back(t);
        }
        json["terrainTypes"] = typesArray;

        // Save assets
        Nova::Json::JsonValue assetsArray = Nova::Json::Array();
        for (const auto& asset : m_assets) {
            Nova::Json::JsonValue assetJson = Nova::Json::Object();
            assetJson["type"] = asset.type;
            assetJson["position"] = Nova::Json::Array({asset.position.x, asset.position.y, asset.position.z});
            assetJson["rotation"] = Nova::Json::Array({asset.rotation.x, asset.rotation.y, asset.rotation.z});
            assetJson["scale"] = Nova::Json::Array({asset.scale.x, asset.scale.y, asset.scale.z});
            assetsArray.push_back(assetJson);
        }
        json["assets"] = assetsArray;

        // Save spawn points
        Nova::Json::JsonValue spawnsArray = Nova::Json::Array();
        for (const auto& spawn : m_spawnPoints) {
            Nova::Json::JsonValue spawnJson = Nova::Json::Object();
            spawnJson["type"] = static_cast<int>(spawn.type);
            spawnJson["position"] = Nova::Json::Array({spawn.position.x, spawn.position.y, spawn.position.z});
            spawnJson["faction"] = spawn.faction;
            spawnsArray.push_back(spawnJson);
        }
        json["spawnPoints"] = spawnsArray;

        // Save objectives
        Nova::Json::JsonValue objectivesArray = Nova::Json::Array();
        for (const auto& obj : m_objectives) {
            Nova::Json::JsonValue objJson = Nova::Json::Object();
            objJson["type"] = obj.type;
            objJson["position"] = Nova::Json::Array({obj.position.x, obj.position.y, obj.position.z});
            objJson["description"] = obj.description;
            objectivesArray.push_back(objJson);
        }
        json["objectives"] = objectivesArray;

        // Write to file
        Nova::Json::WriteFile(path, json, 2);

        m_currentMapPath = path;
        spdlog::info("Local map saved successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception saving map: {}", e.what());
        return false;
    }
}

void LocalMapEditor::GenerateTerrain() {
    spdlog::info("Generating terrain using PCG graph");

    if (!m_pcgGraph) {
        spdlog::warn("No PCG graph loaded, using default noise generation");
        NoiseTerrain();
        return;
    }

    // Ensure terrain data is properly sized
    int totalTiles = m_config.width * m_config.height;
    m_heights.resize(totalTiles, 0.0f);
    m_terrainTypes.resize(totalTiles, 0);

    // Create PCG context
    PCG::PCGContext context;
    context.seed = m_config.seed;
    context.biome = m_config.theme;
    context.latitude = m_config.sourceLatitude;
    context.longitude = m_config.sourceLongitude;

    // Generate heights for each tile using PCG graph
    for (int y = 0; y < m_config.height; ++y) {
        for (int x = 0; x < m_config.width; ++x) {
            int index = y * m_config.width + x;

            // Set position in context
            context.position = glm::vec3(
                x * m_config.tileSize,
                0.0f,
                y * m_config.tileSize
            );

            // Execute PCG graph
            m_pcgGraph->Execute(context);

            // Get height output from graph (assumes output node with ID 0)
            // For now, use a simple fallback if no nodes configured
            auto& nodes = m_pcgGraph->GetNodes();
            if (!nodes.empty()) {
                // Find first node and get its float output
                for (const auto& [id, node] : nodes) {
                    float height = node->GetFloatOutput(0);
                    m_heights[index] = height * (m_config.maxHeight - m_config.minHeight) + m_config.minHeight;
                    break;
                }
            } else {
                // Default: use simple noise if no nodes
                float nx = static_cast<float>(x) / m_config.width;
                float ny = static_cast<float>(y) / m_config.height;
                m_heights[index] = std::sin(nx * 3.14159f * 2.0f) * std::cos(ny * 3.14159f * 2.0f) * 5.0f;
            }
        }
    }

    spdlog::info("Terrain generation complete");
}

void LocalMapEditor::PaintTerrain(int x, int y, const std::string& terrainType) {
    // Map terrain type string to index
    int typeIndex = 0;
    if (terrainType == "grass") typeIndex = 0;
    else if (terrainType == "dirt") typeIndex = 1;
    else if (terrainType == "stone") typeIndex = 2;
    else if (terrainType == "sand") typeIndex = 3;
    else if (terrainType == "water") typeIndex = 4;

    // Apply brush to area around center point
    int halfBrush = m_brushSize / 2;
    for (int dy = -halfBrush; dy <= halfBrush; ++dy) {
        for (int dx = -halfBrush; dx <= halfBrush; ++dx) {
            int px = x + dx;
            int py = y + dy;

            // Check bounds
            if (px < 0 || px >= m_config.width || py < 0 || py >= m_config.height) {
                continue;
            }

            // Calculate distance from brush center for circular brush
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist <= static_cast<float>(halfBrush)) {
                int index = py * m_config.width + px;
                if (index >= 0 && index < static_cast<int>(m_terrainTypes.size())) {
                    m_terrainTypes[index] = typeIndex;
                }
            }
        }
    }
}

void LocalMapEditor::SculptTerrain(int x, int y, float strength, bool raise) {
    // Direction multiplier: positive for raise, negative for lower
    float direction = raise ? 1.0f : -1.0f;
    float adjustedStrength = strength * m_brushStrength * direction;

    // Apply brush to area around center point with falloff
    int halfBrush = m_brushSize / 2;
    for (int dy = -halfBrush; dy <= halfBrush; ++dy) {
        for (int dx = -halfBrush; dx <= halfBrush; ++dx) {
            int px = x + dx;
            int py = y + dy;

            // Check bounds
            if (px < 0 || px >= m_config.width || py < 0 || py >= m_config.height) {
                continue;
            }

            // Calculate distance from brush center
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            float maxDist = static_cast<float>(halfBrush);

            // Only affect points within brush radius
            if (dist <= maxDist) {
                // Calculate falloff (1.0 at center, 0.0 at edge)
                float falloff = 1.0f - (dist / (maxDist + 0.001f));
                falloff = falloff * falloff; // Quadratic falloff for smoother edges

                int index = py * m_config.width + px;
                if (index >= 0 && index < static_cast<int>(m_heights.size())) {
                    m_heights[index] += adjustedStrength * falloff;
                    // Clamp to height range
                    m_heights[index] = std::clamp(m_heights[index], m_config.minHeight, m_config.maxHeight);
                }
            }
        }
    }
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
                m_showOpenDialog = true;
                std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            }
            if (ImGui::MenuItem("Save Map", "Ctrl+S", false, !m_currentMapPath.empty())) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                }
            }
            if (ImGui::MenuItem("Save Map As...", "Ctrl+Shift+S")) {
                m_showSaveDialog = true;
                std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Export...", "")) {
                m_showExportDialog = true;
                std::memset(m_exportPathBuffer, 0, sizeof(m_exportPathBuffer));
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
                m_showMapPropertiesDialog = true;
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
        m_showOpenDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
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
            m_showLoadPCGDialog = true;
            std::memset(m_pcgGraphPathBuffer, 0, sizeof(m_pcgGraphPathBuffer));
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

    if (m_heights.empty()) {
        return;
    }

    // Create a copy of heights for sampling
    std::vector<float> originalHeights = m_heights;

    // Apply box blur filter (average of 3x3 neighborhood)
    for (int y = 0; y < m_config.height; ++y) {
        for (int x = 0; x < m_config.width; ++x) {
            float sum = 0.0f;
            int count = 0;

            // Sample 3x3 neighborhood
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;

                    // Check bounds
                    if (nx >= 0 && nx < m_config.width && ny >= 0 && ny < m_config.height) {
                        int neighborIndex = ny * m_config.width + nx;
                        sum += originalHeights[neighborIndex];
                        count++;
                    }
                }
            }

            // Set smoothed height
            int index = y * m_config.width + x;
            if (count > 0) {
                m_heights[index] = sum / static_cast<float>(count);
            }
        }
    }

    spdlog::info("Terrain smoothing complete");
}

void LocalMapEditor::NoiseTerrain() {
    spdlog::info("Adding noise to terrain");

    // Ensure terrain data is properly sized
    int totalTiles = m_config.width * m_config.height;
    m_heights.resize(totalTiles, 0.0f);

    // Initialize random generator with seed
    std::mt19937_64 rng(m_config.seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // Generate multi-octave noise
    const int octaves = 4;
    const float persistence = 0.5f;
    const float baseFrequency = 0.02f;

    for (int y = 0; y < m_config.height; ++y) {
        for (int x = 0; x < m_config.width; ++x) {
            int index = y * m_config.width + x;
            float height = 0.0f;
            float amplitude = 1.0f;
            float frequency = baseFrequency;
            float maxAmplitude = 0.0f;

            // Sum multiple octaves of sine-based noise
            for (int oct = 0; oct < octaves; ++oct) {
                // Use sine waves with different frequencies for pseudo-noise
                float nx = static_cast<float>(x) * frequency;
                float ny = static_cast<float>(y) * frequency;

                // Combine multiple sine waves for more organic look
                float noise = std::sin(nx + m_config.seed * 0.1f) *
                             std::cos(ny + m_config.seed * 0.2f) *
                             std::sin((nx + ny) * 0.7f + m_config.seed * 0.3f);

                height += noise * amplitude;
                maxAmplitude += amplitude;

                amplitude *= persistence;
                frequency *= 2.0f;
            }

            // Normalize to 0-1 range
            height = (height / maxAmplitude + 1.0f) * 0.5f;

            // Scale to height range and add to existing height
            float noiseHeight = height * (m_config.maxHeight - m_config.minHeight) * m_brushStrength;
            m_heights[index] += noiseHeight;

            // Clamp to valid range
            m_heights[index] = std::clamp(m_heights[index], m_config.minHeight, m_config.maxHeight);
        }
    }

    spdlog::info("Noise generation complete");
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

// ============================================================================
// Dialog Implementations
// ============================================================================

void LocalMapEditor::RenderOpenDialog() {
    if (!m_showOpenDialog) return;

    ImGui::OpenPopup("Open Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Open Map", &m_showOpenDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter map file path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (std::strlen(m_filePathBuffer) > 0) {
                if (LoadMap(m_filePathBuffer)) {
                    spdlog::info("Map loaded successfully from: {}", m_filePathBuffer);
                } else {
                    spdlog::error("Failed to load map from: {}", m_filePathBuffer);
                }
                m_showOpenDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showOpenDialog = false;
        }

        ImGui::EndPopup();
    }
}

void LocalMapEditor::RenderSaveDialog() {
    if (!m_showSaveDialog) return;

    ImGui::OpenPopup("Save Map As");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Save Map As", &m_showSaveDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter save file path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SavePath", m_filePathBuffer, sizeof(m_filePathBuffer));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (std::strlen(m_filePathBuffer) > 0) {
                if (SaveMap(m_filePathBuffer)) {
                    spdlog::info("Map saved successfully to: {}", m_filePathBuffer);
                } else {
                    spdlog::error("Failed to save map to: {}", m_filePathBuffer);
                }
                m_showSaveDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSaveDialog = false;
        }

        ImGui::EndPopup();
    }
}

void LocalMapEditor::RenderExportDialog() {
    if (!m_showExportDialog) return;

    ImGui::OpenPopup("Export Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Export Map", &m_showExportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export Options");
        ModernUI::GradientSeparator(0.3f);

        static int exportFormat = 0;
        ImGui::Text("Format:");
        ImGui::RadioButton("JSON", &exportFormat, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Binary", &exportFormat, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Heightmap PNG", &exportFormat, 2);

        ImGui::Spacing();

        ImGui::Text("Export path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##ExportPath", m_exportPathBuffer, sizeof(m_exportPathBuffer));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0))) {
            if (std::strlen(m_exportPathBuffer) > 0) {
                // For now, use JSON export (same as SaveMap)
                if (exportFormat == 0) {
                    SaveMap(m_exportPathBuffer);
                } else {
                    spdlog::warn("Binary and PNG export not yet implemented, using JSON");
                    SaveMap(m_exportPathBuffer);
                }
                m_showExportDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showExportDialog = false;
        }

        ImGui::EndPopup();
    }
}

void LocalMapEditor::RenderMapPropertiesDialog() {
    if (!m_showMapPropertiesDialog) return;

    ImGui::OpenPopup("Map Properties");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Map Properties", &m_showMapPropertiesDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Map Configuration");
        ModernUI::GradientSeparator(0.3f);

        // Dimensions
        ImGui::Text("Dimensions");
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("Width", &m_config.width);
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("Height", &m_config.height);
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Tile Size", &m_config.tileSize, 0.1f, 1.0f);

        ModernUI::GradientSeparator(0.3f);

        // Map type
        ImGui::Text("Map Type");
        const char* mapTypeNames[] = {"Arena", "Dungeon", "Housing", "Scenario", "Tutorial"};
        int currentType = static_cast<int>(m_config.type);
        if (ImGui::Combo("##MapType", &currentType, mapTypeNames, 5)) {
            m_config.type = static_cast<LocalMapConfig::MapType>(currentType);
        }

        ModernUI::GradientSeparator(0.3f);

        // Height range
        ImGui::Text("Height Range");
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Min Height", &m_config.minHeight);
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Max Height", &m_config.maxHeight);

        ModernUI::GradientSeparator(0.3f);

        // Theme
        ImGui::Text("Theme");
        static char themeBuffer[64] = "";
        if (themeBuffer[0] == '\0') {
            std::strncpy(themeBuffer, m_config.theme.c_str(), sizeof(themeBuffer) - 1);
        }
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##Theme", themeBuffer, sizeof(themeBuffer))) {
            m_config.theme = themeBuffer;
        }

        ModernUI::GradientSeparator(0.3f);

        // World inheritance
        ImGui::Checkbox("Inherit from World", &m_config.inheritFromWorld);
        if (m_config.inheritFromWorld) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(150);
            double lat = m_config.sourceLatitude;
            if (ImGui::InputDouble("Latitude", &lat)) {
                m_config.sourceLatitude = lat;
            }
            ImGui::SetNextItemWidth(150);
            double lon = m_config.sourceLongitude;
            if (ImGui::InputDouble("Longitude", &lon)) {
                m_config.sourceLongitude = lon;
            }
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            // Resize terrain data if dimensions changed
            int totalTiles = m_config.width * m_config.height;
            if (m_heights.size() != static_cast<size_t>(totalTiles)) {
                m_heights.resize(totalTiles, 0.0f);
                m_terrainTypes.resize(totalTiles, 0);
                spdlog::info("Terrain resized to {}x{}", m_config.width, m_config.height);
            }
            m_showMapPropertiesDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showMapPropertiesDialog = false;
        }

        ImGui::EndPopup();
    }
}

void LocalMapEditor::RenderLoadPCGDialog() {
    if (!m_showLoadPCGDialog) return;

    ImGui::OpenPopup("Load PCG Graph");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Load PCG Graph", &m_showLoadPCGDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter PCG graph file path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##PCGPath", m_pcgGraphPathBuffer, sizeof(m_pcgGraphPathBuffer));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (std::strlen(m_pcgGraphPathBuffer) > 0) {
                m_config.pcgGraphPath = m_pcgGraphPathBuffer;
                if (m_pcgGraph) {
                    if (m_pcgGraph->LoadFromFile(m_pcgGraphPathBuffer)) {
                        spdlog::info("PCG graph loaded from: {}", m_pcgGraphPathBuffer);
                    } else {
                        spdlog::error("Failed to load PCG graph from: {}", m_pcgGraphPathBuffer);
                    }
                }
                m_showLoadPCGDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showLoadPCGDialog = false;
        }

        ImGui::EndPopup();
    }
}
