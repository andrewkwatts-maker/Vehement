#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
class ModelImportSettings;
class Mesh;
struct ImportedModel;
struct ImportedMaterial;
}

namespace Vehement {

/**
 * @brief Model import settings panel
 *
 * Features:
 * - 3D preview of imported model
 * - Material assignment
 * - LOD configuration
 * - Animation preview
 * - Statistics display
 */
class ModelImportPanel {
public:
    ModelImportPanel();
    ~ModelImportPanel();

    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    /**
     * @brief Set the model file to configure
     */
    void SetModelPath(const std::string& path);

    /**
     * @brief Get current settings
     */
    Nova::ModelImportSettings* GetSettings() { return m_settings.get(); }

    /**
     * @brief Apply preset
     */
    void ApplyPreset(const std::string& preset);

    // 3D Preview controls
    void RotatePreview(float deltaX, float deltaY);
    void ZoomPreview(float delta);
    void ResetPreview();

    // Callbacks
    using SettingsChangedCallback = std::function<void()>;
    void SetSettingsChangedCallback(SettingsChangedCallback cb) { m_onSettingsChanged = cb; }

private:
    void RenderPreview3D();
    void RenderMeshStatistics();
    void RenderTransformSettings();
    void RenderMeshProcessingSettings();
    void RenderLODSettings();
    void RenderMaterialList();
    void RenderSkeletonInfo();
    void RenderCollisionSettings();

    void UpdatePreview();
    void LoadPreviewModel();

    std::string m_modelPath;
    std::unique_ptr<Nova::ModelImportSettings> m_settings;

    // Preview model data
    std::unique_ptr<Nova::ImportedModel> m_previewModel;

    // 3D preview state
    glm::vec3 m_previewRotation{0.0f};
    float m_previewZoom = 2.0f;
    glm::vec3 m_previewCenter{0.0f};
    bool m_previewDirty = true;

    // LOD preview
    int m_currentLODLevel = 0;
    std::vector<float> m_lodDistances;
    std::vector<int> m_lodTriangleCounts;

    // Material selection
    int m_selectedMaterialIndex = -1;

    // UI state
    bool m_showWireframe = false;
    bool m_showNormals = false;
    bool m_showBones = false;
    bool m_showCollision = false;
    bool m_animatePreview = false;
    float m_animationTime = 0.0f;

    SettingsChangedCallback m_onSettingsChanged;
};

} // namespace Vehement
