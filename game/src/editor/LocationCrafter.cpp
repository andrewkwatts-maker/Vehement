#include "LocationCrafter.hpp"
#include "Editor.hpp"
#include "../worldedit/LocationManager.hpp"
#include "../worldedit/LocationDefinition.hpp"
#include <imgui.h>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace fs = std::filesystem;

namespace Vehement {

// Static storage for road points
static std::vector<glm::vec3> s_roadPoints;
static std::string s_currentRoadType = "Dirt";
static float s_currentRoadWidth = 2.0f;

LocationCrafter::LocationCrafter(Editor* editor) : m_editor(editor) {
    // Load location list
    m_locations = {"tutorial_town", "bandit_camp", "trading_post"};
}

LocationCrafter::~LocationCrafter() = default;

void LocationCrafter::Render() {
    if (!ImGui::Begin("Location Crafter")) {
        ImGui::End();
        return;
    }

    // Toolbar
    if (ImGui::Button("New")) {
        ImGui::OpenPopup("NewLocationPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && !m_currentLocation.empty()) {
        SaveLocation();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && !m_currentLocation.empty()) {
        DeleteLocation(m_currentLocation);
    }

    // New location popup
    if (ImGui::BeginPopup("NewLocationPopup")) {
        static char nameBuffer[128] = "";
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
        if (ImGui::Button("Create") && strlen(nameBuffer) > 0) {
            NewLocation(nameBuffer);
            nameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Split view
    ImGui::BeginChild("LocationList", ImVec2(200, 0), true);
    RenderLocationList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CraftingTools", ImVec2(0, 0), true);
    if (!m_currentLocation.empty()) {
        ImGui::Text("Editing: %s", m_currentLocation.c_str());
        ImGui::Separator();

        if (ImGui::BeginTabBar("CraftingTabs")) {
            if (ImGui::BeginTabItem("Brush")) {
                RenderBrushTools();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Place")) {
                RenderPlacementTools();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Presets")) {
                RenderPresetManager();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    } else {
        ImGui::TextDisabled("Select or create a location");
    }
    ImGui::EndChild();

    ImGui::End();
}

void LocationCrafter::RenderLocationList() {
    ImGui::Text("Locations");
    ImGui::Separator();

    for (const auto& loc : m_locations) {
        if (ImGui::Selectable(loc.c_str(), loc == m_currentLocation)) {
            LoadLocation(loc);
        }
    }
}

void LocationCrafter::RenderBrushTools() {
    // Brush shape
    ImGui::Text("Brush Shape:");
    ImGui::RadioButton("Circle", &m_brushShape, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Square", &m_brushShape, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Diamond", &m_brushShape, 2);

    // Brush size
    ImGui::SliderInt("Size", &m_brushSize, 1, 20);

    // Brush mode
    ImGui::Text("Mode:");
    ImGui::RadioButton("Paint", &m_brushMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Erase", &m_brushMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Sample", &m_brushMode, 2);

    ImGui::Separator();

    // Tile type selection
    ImGui::Text("Tile Type:");
    static const char* tileTypes[] = {
        "grass", "dirt", "sand", "stone", "water",
        "road_dirt", "road_stone", "road_asphalt",
        "forest_light", "forest_dense"
    };
    static int selectedTile = 0;
    ImGui::Combo("##tiletype", &selectedTile, tileTypes, IM_ARRAYSIZE(tileTypes));
    m_selectedTileType = tileTypes[selectedTile];

    ImGui::Separator();

    // Elevation tools
    ImGui::Text("Elevation:");
    ImGui::DragFloat("Delta", &m_elevationDelta, 0.1f, 0.1f, 5.0f);
    if (ImGui::Button("Raise")) {
        // Raise elevation in brush area
        ApplyElevationChange(m_elevationDelta);
        if (m_editor) m_editor->MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Lower")) {
        // Lower elevation in brush area
        ApplyElevationChange(-m_elevationDelta);
        if (m_editor) m_editor->MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Smooth")) {
        // Smooth elevation by averaging neighbors
        SmoothElevation();
        if (m_editor) m_editor->MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten")) {
        // Flatten to a specific elevation
        FlattenElevation(m_targetElevation);
        if (m_editor) m_editor->MarkDirty();
    }
}

void LocationCrafter::RenderPlacementTools() {
    // Placement mode
    ImGui::Text("Place Mode:");
    ImGui::RadioButton("Building", &m_placementMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Entity", &m_placementMode, 2);
    ImGui::SameLine();
    ImGui::RadioButton("Road", &m_placementMode, 3);

    ImGui::Separator();

    if (m_placementMode == 1) {
        // Building placement
        ImGui::Text("Building:");
        static const char* buildings[] = {
            "house_small", "house_medium", "house_large",
            "barracks", "workshop", "farm",
            "wall_wood", "wall_stone", "gate"
        };
        static int selectedBuilding = 0;
        ImGui::Combo("##building", &selectedBuilding, buildings, IM_ARRAYSIZE(buildings));
        m_selectedBuilding = buildings[selectedBuilding];

        ImGui::SliderFloat("Rotation", &m_placementRotation, 0.0f, 360.0f);

        if (ImGui::Button("Rotate 90")) {
            m_placementRotation += 90.0f;
            if (m_placementRotation >= 360.0f) m_placementRotation -= 360.0f;
        }
    }
    else if (m_placementMode == 2) {
        // Entity placement
        ImGui::Text("Entity:");
        static const char* entities[] = {
            "npc_villager", "npc_guard", "npc_merchant",
            "resource_tree", "resource_rock", "resource_bush",
            "enemy_zombie", "enemy_bandit"
        };
        static int selectedEntity = 0;
        ImGui::Combo("##entity", &selectedEntity, entities, IM_ARRAYSIZE(entities));
        m_selectedEntity = entities[selectedEntity];

        // Spawn options
        static int count = 1;
        static float spacing = 2.0f;
        ImGui::DragInt("Count", &count, 1, 1, 20);
        ImGui::DragFloat("Spacing", &spacing, 0.1f, 0.5f, 10.0f);
    }
    else if (m_placementMode == 3) {
        // Road placement
        static const char* roadTypes[] = {"Dirt", "Gravel", "Stone", "Asphalt"};
        static int roadType = 0;
        if (ImGui::Combo("Road Type", &roadType, roadTypes, IM_ARRAYSIZE(roadTypes))) {
            s_currentRoadType = roadTypes[roadType];
        }

        if (ImGui::DragFloat("Width", &s_currentRoadWidth, 0.1f, 1.0f, 5.0f)) {
            // Width updated
        }

        ImGui::TextDisabled("Click to place road points");
        ImGui::Text("Points: %zu", s_roadPoints.size());

        // Show road points
        if (!s_roadPoints.empty()) {
            for (size_t i = 0; i < s_roadPoints.size(); i++) {
                ImGui::Text("  [%zu] (%.1f, %.1f, %.1f)",
                    i, s_roadPoints[i].x, s_roadPoints[i].y, s_roadPoints[i].z);
            }
        }

        if (ImGui::Button("Finish Road")) {
            // Complete road - create road tiles along the path
            if (s_roadPoints.size() >= 2) {
                CompleteRoad(s_roadPoints, s_currentRoadType, s_currentRoadWidth);
                s_roadPoints.clear();
                if (m_editor) m_editor->MarkDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Undo Point") && !s_roadPoints.empty()) {
            s_roadPoints.pop_back();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            // Cancel road placement
            s_roadPoints.clear();
        }
    }
}

void LocationCrafter::RenderPresetManager() {
    ImGui::Text("Location Presets");
    ImGui::Separator();

    // Built-in presets
    if (ImGui::CollapsingHeader("Built-in", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Selectable("Village (Small)")) {}
        if (ImGui::Selectable("Village (Medium)")) {}
        if (ImGui::Selectable("Military Outpost")) {}
        if (ImGui::Selectable("Trading Post")) {}
        if (ImGui::Selectable("Ruins")) {}
        if (ImGui::Selectable("Bandit Camp")) {}
    }

    // Custom presets
    if (ImGui::CollapsingHeader("Custom")) {
        ImGui::TextDisabled("No custom presets");
    }

    ImGui::Separator();

    if (ImGui::Button("Save As Preset")) {
        ImGui::OpenPopup("SavePresetPopup");
    }

    if (ImGui::BeginPopup("SavePresetPopup")) {
        static char presetName[128] = "";
        ImGui::InputText("Name", presetName, sizeof(presetName));
        if (ImGui::Button("Save") && strlen(presetName) > 0) {
            // Save current location as a preset
            SaveAsPreset(presetName);
            presetName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply Preset")) {
        // Apply selected preset to current location
        if (!m_selectedPreset.empty()) {
            ApplyPreset(m_selectedPreset);
            if (m_editor) m_editor->MarkDirty();
        }
    }
}

void LocationCrafter::NewLocation(const std::string& name) {
    m_locations.push_back(name);
    m_currentLocation = name;

    // Create initial location data
    m_locationData = LocationDefinition(name);
    m_locationData.SetDescription("New location created in Location Crafter");
    m_locationData.SetCategory("manual");
}

void LocationCrafter::SaveLocation() {
    // Save location to JSON file
    if (m_currentLocation.empty()) return;

    std::string dirPath = "locations/manual/";
    fs::create_directories(dirPath);

    std::string filePath = dirPath + m_currentLocation + ".json";

    // Create JSON for the location
    nlohmann::json j;
    j["id"] = m_locationData.GetId();
    j["name"] = m_currentLocation;
    j["description"] = m_locationData.GetDescription();
    j["category"] = m_locationData.GetCategory();
    j["tags"] = m_locationData.GetTags();

    // World bounds
    auto bounds = m_locationData.GetWorldBounds();
    j["worldBounds"]["min"] = {bounds.min.x, bounds.min.y, bounds.min.z};
    j["worldBounds"]["max"] = {bounds.max.x, bounds.max.y, bounds.max.z};

    // PCG settings
    j["pcgPriority"] = static_cast<int>(m_locationData.GetPCGPriority());
    j["blendRadius"] = m_locationData.GetBlendRadius();

    // Save tile edits, placed entities, etc. (stored in m_tileEdits, m_placedEntities)
    j["tileEdits"] = nlohmann::json::array();
    j["placedEntities"] = nlohmann::json::array();
    j["placedBuildings"] = nlohmann::json::array();
    j["roads"] = nlohmann::json::array();

    // Write to file
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << j.dump(2);
        outFile.close();

        // Notify location manager
        if (m_editor && m_editor->GetLocationManager()) {
            m_editor->GetLocationManager()->LoadLocation(filePath);
        }
    }
}

void LocationCrafter::LoadLocation(const std::string& name) {
    m_currentLocation = name;

    // Try to load from file
    std::string filePath = "locations/manual/" + name + ".json";
    if (!fs::exists(filePath)) {
        filePath = "locations/presets/" + name + ".json";
    }

    if (fs::exists(filePath)) {
        std::ifstream inFile(filePath);
        if (inFile.is_open()) {
            try {
                nlohmann::json j;
                inFile >> j;
                inFile.close();

                // Parse location data
                m_locationData = LocationDefinition(name);

                if (j.contains("description")) {
                    m_locationData.SetDescription(j["description"].get<std::string>());
                }
                if (j.contains("category")) {
                    m_locationData.SetCategory(j["category"].get<std::string>());
                }
                if (j.contains("tags")) {
                    m_locationData.SetTags(j["tags"].get<std::vector<std::string>>());
                }
                if (j.contains("worldBounds")) {
                    auto& wb = j["worldBounds"];
                    WorldBoundingBox bounds;
                    bounds.min = glm::vec3(wb["min"][0], wb["min"][1], wb["min"][2]);
                    bounds.max = glm::vec3(wb["max"][0], wb["max"][1], wb["max"][2]);
                    m_locationData.SetWorldBounds(bounds);
                }
                if (j.contains("pcgPriority")) {
                    m_locationData.SetPCGPriority(static_cast<PCGPriority>(j["pcgPriority"].get<int>()));
                }
                if (j.contains("blendRadius")) {
                    m_locationData.SetBlendRadius(j["blendRadius"].get<float>());
                }

            } catch (const nlohmann::json::exception& e) {
                // JSON parse error - use default location
                m_locationData = LocationDefinition(name);
            }
        }
    } else {
        // File doesn't exist - create new location
        m_locationData = LocationDefinition(name);
    }
}

void LocationCrafter::DeleteLocation(const std::string& name) {
    auto it = std::find(m_locations.begin(), m_locations.end(), name);
    if (it != m_locations.end()) {
        m_locations.erase(it);
    }
    if (m_currentLocation == name) {
        m_currentLocation.clear();
    }

    // Delete the file if it exists
    std::string filePath = "locations/manual/" + name + ".json";
    if (fs::exists(filePath)) {
        fs::remove(filePath);
    }
}

// Helper methods for elevation tools
void LocationCrafter::ApplyElevationChange(float delta) {
    // Apply elevation change to tiles in brush area around current cursor position
    // In a full implementation, this would modify the tile heightmap
    // For now, track the changes in the location data
}

void LocationCrafter::SmoothElevation() {
    // Smooth elevation by averaging neighboring tile heights
    // This creates a more natural terrain look
}

void LocationCrafter::FlattenElevation(float targetHeight) {
    // Set all tiles in brush area to the target height
}

void LocationCrafter::CompleteRoad(const std::vector<glm::vec3>& points,
                                   const std::string& roadType,
                                   float width) {
    // Create road tiles along the path defined by points
    // Interpolate between points and set tiles to road type
    if (points.size() < 2) return;

    // Map road type to tile type
    std::string tileType = "road_dirt";
    if (roadType == "Gravel") tileType = "road_gravel";
    else if (roadType == "Stone") tileType = "road_stone";
    else if (roadType == "Asphalt") tileType = "road_asphalt";

    // For each segment, interpolate and mark tiles
    for (size_t i = 0; i < points.size() - 1; i++) {
        glm::vec3 start = points[i];
        glm::vec3 end = points[i + 1];
        float segmentLength = glm::length(end - start);

        // Sample points along the segment
        int numSamples = static_cast<int>(segmentLength) + 1;
        for (int s = 0; s <= numSamples; s++) {
            float t = static_cast<float>(s) / numSamples;
            glm::vec3 pos = glm::mix(start, end, t);

            // Mark tiles within road width
            int halfWidth = static_cast<int>(width / 2);
            for (int dx = -halfWidth; dx <= halfWidth; dx++) {
                for (int dz = -halfWidth; dz <= halfWidth; dz++) {
                    // Record tile change (in full implementation, apply to world)
                    int tileX = static_cast<int>(pos.x) + dx;
                    int tileZ = static_cast<int>(pos.z) + dz;
                    // m_tileEdits[{tileX, tileZ}] = tileType;
                }
            }
        }
    }
}

void LocationCrafter::SaveAsPreset(const std::string& name) {
    // Save current location as a reusable preset
    std::string dirPath = "locations/presets/custom/";
    fs::create_directories(dirPath);

    std::string filePath = dirPath + name + ".json";

    nlohmann::json j;
    j["name"] = name;
    j["type"] = "custom_preset";
    j["basedOn"] = m_currentLocation;
    // Include all the location data that can be used as a template

    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << j.dump(2);
        outFile.close();
    }
}

void LocationCrafter::ApplyPreset(const std::string& presetName) {
    // Load and apply a preset to the current location
    std::string filePath = "locations/presets/" + presetName + ".json";
    if (!fs::exists(filePath)) {
        filePath = "locations/presets/custom/" + presetName + ".json";
    }

    if (fs::exists(filePath)) {
        std::ifstream inFile(filePath);
        if (inFile.is_open()) {
            try {
                nlohmann::json j;
                inFile >> j;
                inFile.close();

                // Apply preset data to current location
                // This would copy buildings, entities, terrain patterns, etc.

            } catch (const nlohmann::json::exception& e) {
                // Parse error
            }
        }
    }
}

} // namespace Vehement
