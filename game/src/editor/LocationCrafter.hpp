#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Vehement {

class Editor;

/**
 * @brief Location crafting panel
 *
 * Hand-craft specific locations:
 * - Define named locations
 * - Tile painting tools
 * - Building/entity placement
 * - Preset save/load
 */
class LocationCrafter {
public:
    explicit LocationCrafter(Editor* editor);
    ~LocationCrafter();

    void Render();

    // Location management
    void NewLocation(const std::string& name);
    void SaveLocation();
    void LoadLocation(const std::string& name);
    void DeleteLocation(const std::string& name);

private:
    void RenderLocationList();
    void RenderBrushTools();
    void RenderPlacementTools();
    void RenderPresetManager();

    Editor* m_editor = nullptr;

    // Current location
    std::string m_currentLocation;
    glm::ivec2 m_locationMin{0, 0};
    glm::ivec2 m_locationMax{64, 64};

    // Location list
    std::vector<std::string> m_locations;

    // Brush settings
    int m_brushSize = 3;
    int m_brushShape = 0;  // 0=circle, 1=square, 2=diamond
    int m_brushMode = 0;   // 0=paint, 1=erase, 2=sample
    std::string m_selectedTileType = "grass";
    float m_elevationDelta = 0.5f;

    // Placement mode
    int m_placementMode = 0;  // 0=tile, 1=building, 2=entity
    std::string m_selectedBuilding;
    std::string m_selectedEntity;
    float m_placementRotation = 0.0f;
};

} // namespace Vehement
