#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Vehement {

class Editor;

/**
 * @brief Tile inspector panel
 *
 * Shows detailed information about a selected tile:
 * - Tile type, config, position
 * - Real-world geographic data
 * - Entities on tile
 * - Edit tile properties
 */
class TileInspector {
public:
    explicit TileInspector(Editor* editor);
    ~TileInspector();

    void Render();

    void SetSelectedTile(int x, int y, int z);
    void ClearSelection();

private:
    void RenderTileInfo();
    void RenderGeoData();
    void RenderEntities();
    void RenderEditControls();

    Editor* m_editor = nullptr;

    bool m_hasSelection = false;
    glm::ivec3 m_selectedTile{0, 0, 0};
    std::string m_tileType;
    std::string m_tileConfig;
};

} // namespace Vehement
