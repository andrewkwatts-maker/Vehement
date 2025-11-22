#include "LocationCrafter.hpp"
#include "Editor.hpp"
#include <imgui.h>

namespace Vehement {

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
        // TODO: Raise elevation in brush area
    }
    ImGui::SameLine();
    if (ImGui::Button("Lower")) {
        // TODO: Lower elevation
    }
    ImGui::SameLine();
    if (ImGui::Button("Smooth")) {
        // TODO: Smooth elevation
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten")) {
        // TODO: Flatten elevation
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
        ImGui::Combo("Road Type", &roadType, roadTypes, IM_ARRAYSIZE(roadTypes));

        static float roadWidth = 2.0f;
        ImGui::DragFloat("Width", &roadWidth, 0.1f, 1.0f, 5.0f);

        ImGui::TextDisabled("Click to place road points");
        if (ImGui::Button("Finish Road")) {
            // TODO: Complete road
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            // TODO: Cancel road
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
        if (ImGui::Button("Save")) {
            // TODO: Save preset
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply Preset")) {
        // TODO: Apply selected preset
    }
}

void LocationCrafter::NewLocation(const std::string& name) {
    m_locations.push_back(name);
    m_currentLocation = name;
}

void LocationCrafter::SaveLocation() {
    // TODO: Save location to file
}

void LocationCrafter::LoadLocation(const std::string& name) {
    m_currentLocation = name;
    // TODO: Load location data
}

void LocationCrafter::DeleteLocation(const std::string& name) {
    auto it = std::find(m_locations.begin(), m_locations.end(), name);
    if (it != m_locations.end()) {
        m_locations.erase(it);
    }
    if (m_currentLocation == name) {
        m_currentLocation.clear();
    }
}

} // namespace Vehement
