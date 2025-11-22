#pragma once

#include <string>
#include <vector>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Procedural Content Generation panel
 *
 * Controls for procedural generation:
 * - Script selection per stage
 * - Parameter editing
 * - Preview generation
 * - Real-world data overlay
 */
class PCGPanel {
public:
    explicit PCGPanel(Editor* editor);
    ~PCGPanel();

    void Update(float deltaTime);
    void Render();

    // Generation
    void GeneratePreview();
    void GenerateFull();
    void CancelGeneration();

    // Progress
    float GetProgress() const { return m_progress; }
    bool IsGenerating() const { return m_isGenerating; }

    std::function<void()> OnGenerationComplete;

private:
    void RenderStageConfig();
    void RenderParameters();
    void RenderPreview();
    void RenderRealWorldOverlay();

    Editor* m_editor = nullptr;

    // Scripts per stage
    std::string m_terrainScript = "terrain_default";
    std::string m_roadScript = "road_network";
    std::string m_buildingScript = "urban_generator";
    std::string m_foliageScript = "wilderness_generator";
    std::string m_entityScript = "";

    // Generation state
    bool m_isGenerating = false;
    float m_progress = 0.0f;
    std::string m_currentStage;

    // Settings
    int m_seed = 12345;
    int m_previewWidth = 64;
    int m_previewHeight = 64;
    bool m_useRealWorldData = true;

    // Preview data
    std::vector<uint8_t> m_previewTexture;
    bool m_previewDirty = true;
};

} // namespace Vehement
