#pragma once

#include "../engine/materials/MaterialGraphEditor.hpp"
#include "../engine/materials/AdvancedMaterial.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Engine {

/**
 * @brief Advanced material editor with graph editor, preview, and property inspector
 */
class MaterialEditorAdvanced {
public:
    MaterialEditorAdvanced();
    ~MaterialEditorAdvanced();

    // Main rendering
    void Render();

    // Material management
    void NewMaterial();
    void LoadMaterial(const std::string& filepath);
    void SaveMaterial(const std::string& filepath);

    void SetMaterial(std::shared_ptr<AdvancedMaterial> material);
    std::shared_ptr<AdvancedMaterial> GetMaterial() const;

    // UI panels
    void RenderMainWindow();
    void RenderGraphEditor();
    void RenderPropertyInspector();
    void RenderPreviewPanel();
    void RenderMaterialLibrary();
    void RenderMenuBar();
    void RenderToolbar();
    void RenderStatusBar();

    // Graph editor integration
    MaterialGraphEditor& GetGraphEditor() { return m_graphEditor; }

private:
    std::shared_ptr<AdvancedMaterial> m_currentMaterial;
    MaterialGraphEditor m_graphEditor;
    MaterialGraphPreviewRenderer m_previewRenderer;

    // UI state
    bool m_showGraphEditor = true;
    bool m_showPropertyInspector = true;
    bool m_showPreview = true;
    bool m_showMaterialLibrary = false;

    std::string m_currentFilepath;
    bool m_hasUnsavedChanges = false;

    // Property editor sections
    void RenderBasicProperties();
    void RenderOpticalProperties();
    void RenderEmissionProperties();
    void RenderSubsurfaceProperties();
    void RenderTextureProperties();

    // Material library
    std::vector<std::string> m_materialPresets;
    int m_selectedPreset = -1;

    void LoadMaterialPresets();
    void ApplyPreset(const std::string& presetName);

    // Preview controls
    struct PreviewSettings {
        MaterialGraphPreviewRenderer::PreviewShape shape = MaterialGraphPreviewRenderer::PreviewShape::Sphere;
        float rotation = 0.0f;
        bool autoRotate = true;
        float lightIntensity = 1.0f;
        glm::vec3 lightColor{1.0f};
        glm::vec3 backgroundColor{0.2f, 0.2f, 0.25f};
    } m_previewSettings;

    void RenderPreviewControls();
    void UpdatePreview();
};

} // namespace Engine
