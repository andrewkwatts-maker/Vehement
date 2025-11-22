#include "TileInspector.hpp"
#include "Editor.hpp"
#include <imgui.h>

namespace Vehement {

TileInspector::TileInspector(Editor* editor) : m_editor(editor) {}
TileInspector::~TileInspector() = default;

void TileInspector::Render() {
    if (!ImGui::Begin("Tile Inspector")) {
        ImGui::End();
        return;
    }

    if (!m_hasSelection) {
        ImGui::TextDisabled("No tile selected");
        ImGui::TextDisabled("Click on a tile in World View to inspect it");
        ImGui::End();
        return;
    }

    RenderTileInfo();
    ImGui::Separator();
    RenderGeoData();
    ImGui::Separator();
    RenderEntities();
    ImGui::Separator();
    RenderEditControls();

    ImGui::End();
}

void TileInspector::RenderTileInfo() {
    ImGui::Text("Tile Information");

    ImGui::Text("Position: %d, %d, %d", m_selectedTile.x, m_selectedTile.y, m_selectedTile.z);
    ImGui::Text("Type: %s", m_tileType.c_str());
    ImGui::Text("Config: %s", m_tileConfig.c_str());

    if (ImGui::Button("Open Config")) {
        // TODO: Open config editor for this tile
    }
}

void TileInspector::RenderGeoData() {
    if (ImGui::CollapsingHeader("Geographic Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        // TODO: Get real data from GeoDataProvider
        ImGui::Text("Latitude: 37.7749");
        ImGui::Text("Longitude: -122.4194");
        ImGui::Text("Elevation: 15.3m");
        ImGui::Text("Biome: Urban");
        ImGui::Text("Land Use: Residential");

        if (ImGui::TreeNode("Road Data")) {
            ImGui::Text("Nearest Road: Main Street");
            ImGui::Text("Road Type: Secondary");
            ImGui::Text("Distance: 5m");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Building Data")) {
            ImGui::Text("Building Type: House");
            ImGui::Text("Building Height: 8m");
            ImGui::Text("Building Area: 120 sqm");
            ImGui::TreePop();
        }
    }
}

void TileInspector::RenderEntities() {
    if (ImGui::CollapsingHeader("Entities On Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
        // TODO: Get actual entities from EntityManager
        if (ImGui::Selectable("Worker #1234")) {
            // Select entity
        }
        if (ImGui::Selectable("Tree #5678")) {
            // Select entity
        }

        if (ImGui::Button("Add Entity")) {
            ImGui::OpenPopup("AddEntityPopup");
        }

        if (ImGui::BeginPopup("AddEntityPopup")) {
            if (ImGui::MenuItem("Add Unit")) {}
            if (ImGui::MenuItem("Add Building")) {}
            if (ImGui::MenuItem("Add Resource")) {}
            ImGui::EndPopup();
        }
    }
}

void TileInspector::RenderEditControls() {
    if (ImGui::CollapsingHeader("Edit Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Tile type dropdown
        static const char* tileTypes[] = {"Grass", "Road", "Water", "Forest", "Stone", "Sand"};
        static int currentType = 0;
        if (ImGui::Combo("Tile Type", &currentType, tileTypes, IM_ARRAYSIZE(tileTypes))) {
            // TODO: Change tile type
        }

        // Elevation
        static float elevation = 0.0f;
        if (ImGui::DragFloat("Elevation", &elevation, 0.1f, -100.0f, 100.0f)) {
            // TODO: Update elevation
        }

        // Walkability
        static bool walkable = true;
        ImGui::Checkbox("Walkable", &walkable);

        static bool buildable = true;
        ImGui::Checkbox("Buildable", &buildable);

        if (ImGui::Button("Apply Changes")) {
            // TODO: Apply tile changes
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            // TODO: Revert changes
        }
    }
}

void TileInspector::SetSelectedTile(int x, int y, int z) {
    m_hasSelection = true;
    m_selectedTile = {x, y, z};
    // TODO: Load tile data
    m_tileType = "grass";
    m_tileConfig = "tile_grass";
}

void TileInspector::ClearSelection() {
    m_hasSelection = false;
}

} // namespace Vehement
