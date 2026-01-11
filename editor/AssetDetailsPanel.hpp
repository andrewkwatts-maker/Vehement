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
class Asset;
class Material;
class LODGroup;

// Asset type enumeration
enum class AssetType {
    Mesh,
    Material,
    Texture,
    Audio,
    Animation,
    Prefab,
    Script,
    Unknown
};

// LOD settings
struct LODSettings {
    int levelCount = 4;
    std::vector<float> distances = { 10.0f, 25.0f, 50.0f, 100.0f };
    std::vector<float> screenPercentages = { 1.0f, 0.5f, 0.25f, 0.1f };
    float transitionDuration = 0.5f;
    bool fadeTransition = true;
};

// SDF conversion settings
struct SDFSettings {
    bool enabled = false;
    int resolution = 64;
    float padding = 0.1f;
    bool generateOnImport = false;
    std::string sdfFilePath;
};

// Asset details panel
class AssetDetailsPanel {
public:
    AssetDetailsPanel();
    ~AssetDetailsPanel();

    void Initialize();
    void Shutdown();

    void RenderUI();

    // Asset management
    void SetSelectedAsset(Asset* asset);
    Asset* GetSelectedAsset() const { return m_selectedAsset; }

    // Panel state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

    // Edit level (always Asset level for this panel)
    PropertyLevel GetEditLevel() const { return PropertyLevel::Asset; }

private:
    // Tab rendering
    void RenderMaterialTab();
    void RenderRenderingTab();
    void RenderLODTab();
    void RenderSDFTab();
    void RenderMetadataTab();
    void RenderImportTab();

    // Material tab
    void RenderMaterialAssignment();
    void RenderMaterialOverrides();
    void RenderMaterialSlots();

    // Rendering tab
    void RenderRenderingFlags();
    void RenderVisibilitySettings();
    void RenderCullingSettings();
    void RenderLayerSettings();

    // LOD tab
    void RenderLODConfiguration();
    void RenderLODLevelSettings(int level);
    void RenderLODTransitionSettings();
    void RenderLODPreview();

    // SDF tab
    void RenderSDFConversionSettings();
    void RenderSDFGenerationControls();
    void RenderSDFPreview();

    // Metadata tab
    void RenderAssetInfo();
    void RenderImportSettings();
    void RenderDependencies();

    // Import tab
    void RenderImportOptions();
    void RenderExportOptions();

    // UI helpers
    void RenderAssetHeader();
    void RenderEditModeControls();
    void RenderActionButtons();
    void RenderStatusBar();

    // Actions
    void ResetAllPropertiesToDefault();
    void ApplyToAllInstances();
    void ReimportAsset();
    void ExportAsset();

    // Helper functions
    const char* AssetTypeToString(AssetType type) const;
    void RefreshAssetProperties();

private:
    bool m_isOpen = true;
    bool m_editAtAssetLevel = true;

    // Current asset
    Asset* m_selectedAsset = nullptr;
    PropertyContainer* m_assetProperties = nullptr;
    AssetType m_assetType = AssetType::Unknown;

    // UI state
    bool m_showOnlyOverridden = false;
    int m_selectedLODLevel = 0;

    // Temporary values for editing
    struct {
        // Material
        std::vector<Material*> materialSlots;
        int selectedMaterialSlot = 0;

        // Rendering flags
        bool castShadows = true;
        bool receiveShadows = true;
        bool receiveGI = true;
        bool contributeGI = true;
        bool motionVectors = true;
        bool dynamicOcclusion = true;

        // Visibility
        float visibilityRange = 100.0f;
        float fadeStart = 90.0f;
        float fadeEnd = 100.0f;
        int renderLayer = 0;
        uint32_t visibilityMask = 0xFFFFFFFF;

        // Culling
        bool frustumCulling = true;
        bool occlusionCulling = true;
        float cullMargin = 0.0f;

        // LOD
        LODSettings lod;

        // SDF
        SDFSettings sdf;

        // Metadata
        std::string assetName;
        std::string assetPath;
        std::string importDate;
        std::string modifiedDate;
        size_t fileSize = 0;
        std::vector<std::string> dependencies;
    } m_tempValues;
};

} // namespace Nova3D
