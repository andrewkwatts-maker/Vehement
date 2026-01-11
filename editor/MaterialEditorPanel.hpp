#pragma once

#include "../engine/core/PropertySystem.hpp"
#include "PropertyOverrideUI.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Nova3D {

// Forward declarations
class AdvancedMaterial;
class MaterialGraph;
class PreviewRenderer;
class Camera;

// Material preset data
struct MaterialPreset {
    std::string name;
    glm::vec3 albedo;
    float metallic;
    float roughness;
    float ior;
    float subsurfaceScattering;
    glm::vec3 emissiveColor;
    float emissiveIntensity;
    bool enableDispersion;
    float abbeNumber;
    glm::vec3 scatteringCoefficient;
};

// Material editor panel
class MaterialEditorPanel {
public:
    MaterialEditorPanel();
    ~MaterialEditorPanel();

    void Initialize();
    void Shutdown();

    void RenderUI();

    // Material management
    void SetCurrentMaterial(AdvancedMaterial* material);
    AdvancedMaterial* GetCurrentMaterial() const { return m_currentMaterial; }

    void SaveMaterial();
    void SaveMaterialAs();
    void LoadMaterial(const std::string& filepath);
    void ResetMaterial();
    void DuplicateMaterial();

    // Preset management
    void ApplyPreset(const std::string& presetName);
    void SaveAsPreset(const std::string& presetName);

    // Panel state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

    // Property level editing
    void SetEditLevel(PropertyLevel level) { m_editLevel = level; }
    PropertyLevel GetEditLevel() const { return m_editLevel; }

private:
    // Tab rendering
    void RenderBasicPropertiesTab();
    void RenderOpticalPropertiesTab();
    void RenderEmissionPropertiesTab();
    void RenderScatteringPropertiesTab();
    void RenderTexturesTab();
    void RenderMaterialGraphTab();
    void RenderPreviewTab();

    // Basic properties
    void RenderAlbedoProperties();
    void RenderMetallicRoughnessProperties();
    void RenderNormalProperties();

    // Optical properties
    void RenderIORProperties();
    void RenderDispersionProperties();
    void RenderAnisotropyProperties();
    void RenderTransmissionProperties();

    // Emission properties
    void RenderEmissiveColorProperties();
    void RenderBlackbodyProperties();
    void RenderLuminosityProperties();

    // Scattering properties
    void RenderRayleighScatteringProperties();
    void RenderMieScatteringProperties();
    void RenderSubsurfaceScatteringProperties();
    void RenderVolumeScatteringProperties();

    // Texture slots
    void RenderTextureSlot(const char* label, std::string& texturePath, const char* tooltip = nullptr);

    // Material graph
    void RenderNodeEditor();
    void RenderNodeLibrary();
    void RenderNodeProperties();

    // Preview
    void UpdatePreview();
    void RenderPreviewControls();
    void RenderPreviewSphere();

    // Preset management
    void LoadPresets();
    void SavePresets();

    // UI helpers
    void RenderMaterialSelector();
    void RenderPresetDropdown();
    void RenderToolbar();
    void RenderStatusBar();

private:
    bool m_isOpen = true;
    PropertyLevel m_editLevel = PropertyLevel::Asset;

    // Current material
    AdvancedMaterial* m_currentMaterial = nullptr;
    PropertyContainer* m_materialProperties = nullptr;

    // Material library
    std::vector<AdvancedMaterial*> m_materialLibrary;
    std::vector<MaterialPreset> m_presets;

    // Material graph
    std::unique_ptr<MaterialGraph> m_materialGraph;
    bool m_showNodeLibrary = true;
    bool m_showNodeProperties = true;

    // Preview
    std::unique_ptr<PreviewRenderer> m_previewRenderer;
    std::unique_ptr<Camera> m_previewCamera;
    ImTextureID m_previewTexture = nullptr;
    int m_previewSize = 256;
    bool m_autoRotatePreview = true;
    float m_previewRotation = 0.0f;

    // UI state
    int m_currentTab = 0;
    bool m_showOnlyOverridden = false;

    // Temporary values for editing
    struct {
        glm::vec3 albedo = glm::vec3(0.8f);
        float metallic = 0.0f;
        float roughness = 0.5f;

        float ior = 1.5f;
        glm::vec3 iorAnisotropic = glm::vec3(1.5f);
        float abbeNumber = 55.0f;
        bool enableDispersion = false;

        glm::vec3 emissiveColor = glm::vec3(0.0f);
        float emissiveIntensity = 0.0f;
        float emissiveTemperature = 6500.0f;
        float emissiveLuminosity = 0.0f;
        bool useBlackbody = false;

        glm::vec3 rayleighCoefficient = glm::vec3(0.0f);
        glm::vec3 mieCoefficient = glm::vec3(0.0f);
        float mieAnisotropy = 0.76f;
        glm::vec3 subsurfaceColor = glm::vec3(1.0f);
        float subsurfaceRadius = 1.0f;
        float subsurfaceScattering = 0.0f;

        float transmission = 0.0f;
        float thickness = 1.0f;

        std::string albedoMap;
        std::string normalMap;
        std::string metallicMap;
        std::string roughnessMap;
        std::string aoMap;
        std::string emissiveMap;
        std::string heightMap;
        std::string opacityMap;
    } m_tempValues;
};

// Preset definitions
namespace MaterialPresets {
    extern const MaterialPreset Glass;
    extern const MaterialPreset Water;
    extern const MaterialPreset Gold;
    extern const MaterialPreset Silver;
    extern const MaterialPreset Copper;
    extern const MaterialPreset Diamond;
    extern const MaterialPreset Ruby;
    extern const MaterialPreset Emerald;
    extern const MaterialPreset Plastic;
    extern const MaterialPreset Rubber;
    extern const MaterialPreset Wood;
    extern const MaterialPreset Concrete;
    extern const MaterialPreset Skin;
    extern const MaterialPreset Wax;
    extern const MaterialPreset Ice;
}

} // namespace Nova3D
