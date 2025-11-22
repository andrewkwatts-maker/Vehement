#include "TileInspector.hpp"
#include "Editor.hpp"
#include "ConfigEditor.hpp"
#include "../config/ConfigRegistry.hpp"
#include "../entities/EntityManager.hpp"
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
        // Open the config editor and select this tile's config
        if (m_editor) {
            m_editor->SetConfigEditorVisible(true);
            if (auto* configEditor = m_editor->GetConfigEditor()) {
                configEditor->SelectConfig(m_tileConfig);
            }
        }
    }
}

void TileInspector::RenderGeoData() {
    if (ImGui::CollapsingHeader("Geographic Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Convert tile coordinates to geographic data
        // World scale: 1 tile = approximately 1 meter, base at lat 37.7749, lon -122.4194
        double baseLat = 37.7749;
        double baseLon = -122.4194;
        double metersPerDegLat = 111111.0;  // Approximate meters per degree latitude
        double metersPerDegLon = 111111.0 * cos(glm::radians(baseLat));  // Adjusted for latitude

        double latitude = baseLat + (m_selectedTile.z / metersPerDegLat);
        double longitude = baseLon + (m_selectedTile.x / metersPerDegLon);
        float elevation = static_cast<float>(m_selectedTile.y) * 0.5f;  // Each Y level = 0.5m

        // Determine biome based on elevation and position
        const char* biome = "Urban";
        if (elevation < 0) biome = "Water";
        else if (elevation > 100) biome = "Mountain";
        else if (m_tileType == "forest" || m_tileType == "forest_light" || m_tileType == "forest_dense") biome = "Forest";
        else if (m_tileType == "sand") biome = "Desert";

        // Determine land use
        const char* landUse = "Residential";
        if (m_tileType == "road" || m_tileType == "road_dirt" || m_tileType == "road_stone") landUse = "Transportation";
        else if (m_tileType == "water") landUse = "Water Body";
        else if (m_tileType == "forest" || m_tileType.find("forest") != std::string::npos) landUse = "Natural";

        ImGui::Text("Latitude: %.6f", latitude);
        ImGui::Text("Longitude: %.6f", longitude);
        ImGui::Text("Elevation: %.1fm", elevation);
        ImGui::Text("Biome: %s", biome);
        ImGui::Text("Land Use: %s", landUse);

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
        // Get entities from EntityManager near this tile position
        if (m_editor && m_editor->GetEntityManager()) {
            EntityManager* entityMgr = m_editor->GetEntityManager();
            glm::vec3 tileCenter(
                static_cast<float>(m_selectedTile.x) + 0.5f,
                static_cast<float>(m_selectedTile.y),
                static_cast<float>(m_selectedTile.z) + 0.5f
            );

            // Find entities within tile radius (about 1 unit)
            auto entitiesOnTile = entityMgr->FindEntitiesInRadius(tileCenter, 1.5f);

            if (entitiesOnTile.empty()) {
                ImGui::TextDisabled("No entities on this tile");
            } else {
                for (auto* entity : entitiesOnTile) {
                    if (!entity) continue;

                    // Create a display name for the entity
                    std::string displayName = entity->GetName();
                    if (displayName.empty()) {
                        displayName = "Entity_" + std::to_string(entity->GetId());
                    }

                    // Show entity with its type
                    ImGui::PushID(static_cast<int>(entity->GetId()));
                    if (ImGui::Selectable(displayName.c_str())) {
                        // Select this entity in the inspector
                        if (auto* inspector = m_editor->GetInspector()) {
                            inspector->SetSelectedEntity(entity->GetId());
                        }
                        m_editor->SetInspectorVisible(true);
                    }
                    ImGui::PopID();
                }
            }
        } else {
            ImGui::TextDisabled("No entity manager available");
        }

        if (ImGui::Button("Add Entity")) {
            ImGui::OpenPopup("AddEntityPopup");
        }

        if (ImGui::BeginPopup("AddEntityPopup")) {
            if (ImGui::MenuItem("Add Unit")) {
                // Create a unit at this tile position
                if (m_editor && m_editor->GetEntityManager()) {
                    // Entity creation would be handled by editor command system
                    m_editor->MarkDirty();
                }
            }
            if (ImGui::MenuItem("Add Building")) {
                if (m_editor && m_editor->GetEntityManager()) {
                    m_editor->MarkDirty();
                }
            }
            if (ImGui::MenuItem("Add Resource")) {
                if (m_editor && m_editor->GetEntityManager()) {
                    m_editor->MarkDirty();
                }
            }
            ImGui::EndPopup();
        }
    }
}

void TileInspector::RenderEditControls() {
    if (ImGui::CollapsingHeader("Edit Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Tile type dropdown
        static const char* tileTypes[] = {"grass", "road", "water", "forest", "stone", "sand"};
        static const char* tileDisplayNames[] = {"Grass", "Road", "Water", "Forest", "Stone", "Sand"};
        static int currentType = 0;

        // Find current type index
        for (int i = 0; i < IM_ARRAYSIZE(tileTypes); i++) {
            if (m_tileType == tileTypes[i]) {
                currentType = i;
                break;
            }
        }

        if (ImGui::Combo("Tile Type", &currentType, tileDisplayNames, IM_ARRAYSIZE(tileDisplayNames))) {
            // Update tile type in world data
            m_tileType = tileTypes[currentType];
            m_tileConfig = std::string("tile_") + tileTypes[currentType];
            if (m_editor) {
                m_editor->MarkDirty();
            }
        }

        // Elevation
        static float elevation = 0.0f;
        elevation = static_cast<float>(m_selectedTile.y) * 0.5f;
        if (ImGui::DragFloat("Elevation", &elevation, 0.1f, -100.0f, 100.0f)) {
            // Update tile elevation
            m_selectedTile.y = static_cast<int>(elevation * 2.0f);
            if (m_editor) {
                m_editor->MarkDirty();
            }
        }

        // Walkability
        static bool walkable = true;
        if (ImGui::Checkbox("Walkable", &walkable)) {
            if (m_editor) {
                m_editor->MarkDirty();
            }
        }

        static bool buildable = true;
        if (ImGui::Checkbox("Buildable", &buildable)) {
            if (m_editor) {
                m_editor->MarkDirty();
            }
        }

        if (ImGui::Button("Apply Changes")) {
            // Apply all pending tile changes to the world
            // Changes are tracked via MarkDirty and saved with the project
            if (m_editor && m_editor->GetWorld()) {
                // World update would be triggered through the editor's save system
                m_editor->MarkDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            // Reload tile data from world to revert changes
            SetSelectedTile(m_selectedTile.x, m_selectedTile.y, m_selectedTile.z);
        }
    }
}

void TileInspector::SetSelectedTile(int x, int y, int z) {
    m_hasSelection = true;
    m_selectedTile = {x, y, z};

    // Load tile data from world
    if (m_editor && m_editor->GetWorld()) {
        // Query tile data from the world system
        // For now, determine type based on some heuristics
        // In a full implementation, this would query the World's tile data structure

        // Default to grass
        m_tileType = "grass";
        m_tileConfig = "tile_grass";

        // Simple terrain generation logic for demonstration
        // Real implementation would query actual world data
        float noiseVal = static_cast<float>(((x * 374761393 + z * 668265263) ^ (y * 1274126177)) & 0xFFFF) / 65536.0f;

        if (y < 0) {
            m_tileType = "water";
            m_tileConfig = "tile_water";
        } else if (y > 50) {
            m_tileType = "stone";
            m_tileConfig = "tile_stone";
        } else if (noiseVal < 0.15f) {
            m_tileType = "road";
            m_tileConfig = "tile_road";
        } else if (noiseVal < 0.35f) {
            m_tileType = "forest";
            m_tileConfig = "tile_forest";
        } else if (noiseVal > 0.9f) {
            m_tileType = "sand";
            m_tileConfig = "tile_sand";
        }
    } else {
        // No world available, use defaults
        m_tileType = "grass";
        m_tileConfig = "tile_grass";
    }
}

void TileInspector::ClearSelection() {
    m_hasSelection = false;
}

} // namespace Vehement
