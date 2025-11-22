#pragma once

#include "../MapEditor.hpp"
#include <string>
#include <vector>

namespace Vehement {

class MapEditor;

/**
 * @brief Terrain editing panel
 *
 * Provides terrain tools:
 * - Brush sizes
 * - Height tools (raise, lower, smooth, plateau)
 * - Texture painting
 * - Water level
 * - Cliff tools
 */
class TerrainPanel {
public:
    TerrainPanel();
    ~TerrainPanel();

    void Initialize(MapEditor& mapEditor);
    void Shutdown();

    void Update(float deltaTime);
    void Render();

private:
    void RenderBrushSettings();
    void RenderHeightTools();
    void RenderTextureTools();
    void RenderWaterTools();
    void RenderCliffTools();

    MapEditor* m_mapEditor = nullptr;

    // Brush preview
    float m_brushSize = 4.0f;
    float m_brushStrength = 0.5f;
    float m_brushFalloff = 0.3f;
    int m_brushShape = 0; // 0=circle, 1=square, 2=diamond

    // Height tool state
    int m_heightTool = 0; // 0=raise, 1=lower, 2=smooth, 3=plateau, 4=flatten
    float m_plateauHeight = 0.0f;
    bool m_smoothBrush = true;

    // Texture state
    int m_selectedTexture = 0;
    std::vector<std::string> m_textureNames;

    // Water state
    float m_waterLevel = 0.0f;
    bool m_waterEnabled = false;
    bool m_showWaterPreview = true;

    // Cliff state
    float m_cliffHeight = 2.0f;
    int m_cliffStyle = 0;
};

} // namespace Vehement
