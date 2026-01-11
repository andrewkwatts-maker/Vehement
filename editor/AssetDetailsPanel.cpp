#include "AssetDetailsPanel.hpp"

namespace Nova3D {

AssetDetailsPanel::AssetDetailsPanel() {
}

AssetDetailsPanel::~AssetDetailsPanel() {
    Shutdown();
}

void AssetDetailsPanel::Initialize() {
    // Create property container for asset
    m_assetProperties = PropertySystem::Instance().CreateAssetContainer();
}

void AssetDetailsPanel::Shutdown() {
}

void AssetDetailsPanel::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    if (!m_selectedAsset) {
        ImGui::Begin("Asset Details", &m_isOpen);
        ImGui::Text("No asset selected");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Asset Details", &m_isOpen);

    // Asset header
    RenderAssetHeader();

    ImGui::Separator();

    // Edit mode controls
    RenderEditModeControls();

    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("AssetTabs")) {
        if (ImGui::BeginTabItem("Material")) {
            RenderMaterialTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering")) {
            RenderRenderingTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("LOD")) {
            RenderLODTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("SDF")) {
            RenderSDFTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Metadata")) {
            RenderMetadataTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Import/Export")) {
            RenderImportTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Action buttons
    RenderActionButtons();

    ImGui::Separator();

    // Status bar
    RenderStatusBar();

    ImGui::End();
}

void AssetDetailsPanel::RenderAssetHeader() {
    ImGui::Text("Asset: %s", m_tempValues.assetName.c_str());
    ImGui::Text("Type: %s", AssetTypeToString(m_assetType));
    ImGui::Text("Path: %s", m_tempValues.assetPath.c_str());
}

void AssetDetailsPanel::RenderEditModeControls() {
    ImGui::Checkbox("Edit at Asset Level", &m_editAtAssetLevel);
    ImGui::SameLine();
    PropertyOverrideUI::HelpMarker("When enabled, changes will affect all instances of this asset");

    ImGui::SameLine();
    ImGui::Checkbox("Show Only Overridden", &m_showOnlyOverridden);
}

void AssetDetailsPanel::RenderActionButtons() {
    if (ImGui::Button("Reset All to Default")) {
        ResetAllPropertiesToDefault();
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply to All Instances")) {
        ApplyToAllInstances();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reimport")) {
        ReimportAsset();
    }

    ImGui::SameLine();
    if (ImGui::Button("Export")) {
        ExportAsset();
    }
}

void AssetDetailsPanel::RenderStatusBar() {
    ImGui::Text("Modified: %s",
               m_assetProperties && m_assetProperties->HasDirtyProperties() ? "Yes" : "No");
}

void AssetDetailsPanel::RenderMaterialTab() {
    PropertyOverrideUI::BeginCategory("Material Assignment");
    RenderMaterialAssignment();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Material Slots");
    RenderMaterialSlots();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Material Overrides");
    RenderMaterialOverrides();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderMaterialAssignment() {
    ImGui::Text("Default Material");

    if (ImGui::BeginCombo("Material", "None")) {
        if (ImGui::Selectable("Material 1")) {
            // Assign material
        }
        if (ImGui::Selectable("Material 2")) {
            // Assign material
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Create New Material")) {
        // Create new material
    }
}

void AssetDetailsPanel::RenderMaterialSlots() {
    ImGui::Text("Material Slots: %zu", m_tempValues.materialSlots.size());

    for (size_t i = 0; i < m_tempValues.materialSlots.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        char label[64];
        snprintf(label, sizeof(label), "Slot %zu", i);

        if (ImGui::BeginCombo(label, "Material")) {
            if (ImGui::Selectable("Material 1")) {
                // Assign to slot
            }
            if (ImGui::Selectable("Material 2")) {
                // Assign to slot
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            // Clear slot
        }

        ImGui::PopID();
    }

    if (ImGui::Button("Add Slot")) {
        // Add new slot
    }
}

void AssetDetailsPanel::RenderMaterialOverrides() {
    ImGui::Text("Override material properties at asset level");

    PropertyOverrideUI::RenderBool(
        "Override Albedo",
        m_editAtAssetLevel,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Override base color"
    );

    PropertyOverrideUI::RenderBool(
        "Override Metallic",
        m_editAtAssetLevel,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Override metallic value"
    );

    PropertyOverrideUI::RenderBool(
        "Override Roughness",
        m_editAtAssetLevel,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Override roughness value"
    );
}

void AssetDetailsPanel::RenderRenderingTab() {
    PropertyOverrideUI::BeginCategory("Rendering Flags");
    RenderRenderingFlags();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Visibility");
    RenderVisibilitySettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Culling");
    RenderCullingSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Layer");
    RenderLayerSettings();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderRenderingFlags() {
    PropertyOverrideUI::RenderBool(
        "Cast Shadows",
        m_tempValues.castShadows,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Cast shadows onto other objects"
    );

    PropertyOverrideUI::RenderBool(
        "Receive Shadows",
        m_tempValues.receiveShadows,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Receive shadows from other objects"
    );

    PropertyOverrideUI::RenderBool(
        "Receive GI",
        m_tempValues.receiveGI,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Receive global illumination"
    );

    PropertyOverrideUI::RenderBool(
        "Contribute GI",
        m_tempValues.contributeGI,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Contribute to global illumination"
    );

    PropertyOverrideUI::RenderBool(
        "Motion Vectors",
        m_tempValues.motionVectors,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Generate motion vectors for motion blur"
    );

    PropertyOverrideUI::RenderBool(
        "Dynamic Occlusion",
        m_tempValues.dynamicOcclusion,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Use dynamic occlusion culling"
    );
}

void AssetDetailsPanel::RenderVisibilitySettings() {
    PropertyOverrideUI::RenderFloat(
        "Visibility Range",
        m_tempValues.visibilityRange,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        0.0f, 1000.0f,
        "Maximum distance at which object is visible"
    );

    PropertyOverrideUI::RenderFloat(
        "Fade Start",
        m_tempValues.fadeStart,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        0.0f, 1000.0f,
        "Distance at which fade-out begins"
    );

    PropertyOverrideUI::RenderFloat(
        "Fade End",
        m_tempValues.fadeEnd,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        0.0f, 1000.0f,
        "Distance at which object is fully faded"
    );
}

void AssetDetailsPanel::RenderCullingSettings() {
    PropertyOverrideUI::RenderBool(
        "Frustum Culling",
        m_tempValues.frustumCulling,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Cull when outside camera frustum"
    );

    PropertyOverrideUI::RenderBool(
        "Occlusion Culling",
        m_tempValues.occlusionCulling,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Cull when occluded by other objects"
    );

    PropertyOverrideUI::RenderFloat(
        "Cull Margin",
        m_tempValues.cullMargin,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        0.0f, 10.0f,
        "Extra margin for culling bounds"
    );
}

void AssetDetailsPanel::RenderLayerSettings() {
    PropertyOverrideUI::RenderInt(
        "Render Layer",
        m_tempValues.renderLayer,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        0, 31,
        "Rendering layer (0-31)"
    );

    ImGui::Text("Visibility Mask: 0x%08X", m_tempValues.visibilityMask);
}

void AssetDetailsPanel::RenderLODTab() {
    PropertyOverrideUI::BeginCategory("LOD Configuration");
    RenderLODConfiguration();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("LOD Levels");
    for (int i = 0; i < m_tempValues.lod.levelCount; ++i) {
        char label[64];
        snprintf(label, sizeof(label), "LOD %d", i);
        if (ImGui::TreeNode(label)) {
            RenderLODLevelSettings(i);
            ImGui::TreePop();
        }
    }
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Transition Settings");
    RenderLODTransitionSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Preview");
    RenderLODPreview();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderLODConfiguration() {
    PropertyOverrideUI::RenderInt(
        "LOD Levels",
        m_tempValues.lod.levelCount,
        m_assetProperties,
        PropertyLevel::Asset,
        [this](int count) {
            m_tempValues.lod.distances.resize(count, 100.0f);
            m_tempValues.lod.screenPercentages.resize(count, 0.5f);
        },
        1, 8,
        "Number of LOD levels"
    );
}

void AssetDetailsPanel::RenderLODLevelSettings(int level) {
    if (level >= m_tempValues.lod.levelCount) return;

    char distLabel[64];
    snprintf(distLabel, sizeof(distLabel), "Distance##%d", level);

    if (level < static_cast<int>(m_tempValues.lod.distances.size())) {
        PropertyOverrideUI::RenderFloat(
            distLabel,
            m_tempValues.lod.distances[level],
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            0.0f, 1000.0f,
            "Distance threshold for this LOD level"
        );
    }

    char screenLabel[64];
    snprintf(screenLabel, sizeof(screenLabel), "Screen Percentage##%d", level);

    if (level < static_cast<int>(m_tempValues.lod.screenPercentages.size())) {
        PropertyOverrideUI::RenderPercentage(
            screenLabel,
            m_tempValues.lod.screenPercentages[level],
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            "Screen coverage percentage for this LOD"
        );
    }
}

void AssetDetailsPanel::RenderLODTransitionSettings() {
    PropertyOverrideUI::RenderBool(
        "Fade Transition",
        m_tempValues.lod.fadeTransition,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Smooth fade between LOD levels"
    );

    if (m_tempValues.lod.fadeTransition) {
        PropertyOverrideUI::RenderFloat(
            "Transition Duration",
            m_tempValues.lod.transitionDuration,
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            0.0f, 2.0f,
            "Duration of LOD transition in seconds"
        );
    }
}

void AssetDetailsPanel::RenderLODPreview() {
    ImGui::Text("LOD Preview");

    ImGui::SliderInt("Preview LOD Level", &m_selectedLODLevel, 0, m_tempValues.lod.levelCount - 1);

    // Placeholder for LOD visualization
    ImVec2 size(256, 256);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           IM_COL32(40, 40, 40, 255));
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(size);

    ImGui::Text("LOD %d", m_selectedLODLevel);
    if (m_selectedLODLevel < static_cast<int>(m_tempValues.lod.distances.size())) {
        ImGui::Text("Distance: %.1f", m_tempValues.lod.distances[m_selectedLODLevel]);
    }
}

void AssetDetailsPanel::RenderSDFTab() {
    PropertyOverrideUI::BeginCategory("SDF Conversion");
    RenderSDFConversionSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Generation");
    RenderSDFGenerationControls();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Preview");
    RenderSDFPreview();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderSDFConversionSettings() {
    PropertyOverrideUI::RenderBool(
        "Enable SDF",
        m_tempValues.sdf.enabled,
        m_assetProperties,
        PropertyLevel::Asset,
        nullptr,
        "Enable signed distance field representation"
    );

    if (m_tempValues.sdf.enabled) {
        PropertyOverrideUI::RenderInt(
            "SDF Resolution",
            m_tempValues.sdf.resolution,
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            16, 256,
            "Resolution of SDF volume"
        );

        PropertyOverrideUI::RenderFloat(
            "Padding",
            m_tempValues.sdf.padding,
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            0.0f, 1.0f,
            "Padding around mesh bounds"
        );

        PropertyOverrideUI::RenderBool(
            "Generate on Import",
            m_tempValues.sdf.generateOnImport,
            m_assetProperties,
            PropertyLevel::Asset,
            nullptr,
            "Automatically generate SDF when importing"
        );

        ImGui::Text("SDF File: %s",
                   m_tempValues.sdf.sdfFilePath.empty() ? "None" : m_tempValues.sdf.sdfFilePath.c_str());
    }
}

void AssetDetailsPanel::RenderSDFGenerationControls() {
    if (ImGui::Button("Generate SDF Now")) {
        // Generate SDF
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear SDF")) {
        m_tempValues.sdf.sdfFilePath.clear();
    }

    if (ImGui::Button("Load SDF from File")) {
        // Open file dialog
    }

    ImGui::SameLine();
    if (ImGui::Button("Export SDF")) {
        // Save SDF to file
    }
}

void AssetDetailsPanel::RenderSDFPreview() {
    ImGui::Text("SDF Preview");

    if (m_tempValues.sdf.sdfFilePath.empty()) {
        ImGui::Text("No SDF generated");
        return;
    }

    // Placeholder for SDF visualization
    ImVec2 size(256, 256);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           IM_COL32(40, 40, 40, 255));
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(size);
}

void AssetDetailsPanel::RenderMetadataTab() {
    PropertyOverrideUI::BeginCategory("Asset Information");
    RenderAssetInfo();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Import Settings");
    RenderImportSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Dependencies");
    RenderDependencies();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderAssetInfo() {
    ImGui::Text("Name: %s", m_tempValues.assetName.c_str());
    ImGui::Text("Type: %s", AssetTypeToString(m_assetType));
    ImGui::Text("Path: %s", m_tempValues.assetPath.c_str());
    ImGui::Text("File Size: %zu bytes", m_tempValues.fileSize);
    ImGui::Text("Imported: %s", m_tempValues.importDate.c_str());
    ImGui::Text("Modified: %s", m_tempValues.modifiedDate.c_str());
}

void AssetDetailsPanel::RenderImportSettings() {
    ImGui::Text("Import settings would appear here");
    ImGui::Text("(Scale, rotation, material import, etc.)");
}

void AssetDetailsPanel::RenderDependencies() {
    ImGui::Text("Dependencies (%zu):", m_tempValues.dependencies.size());

    for (const auto& dep : m_tempValues.dependencies) {
        ImGui::BulletText("%s", dep.c_str());
    }

    if (m_tempValues.dependencies.empty()) {
        ImGui::Text("No dependencies");
    }
}

void AssetDetailsPanel::RenderImportTab() {
    PropertyOverrideUI::BeginCategory("Import Options");
    RenderImportOptions();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Export Options");
    RenderExportOptions();
    PropertyOverrideUI::EndCategory();
}

void AssetDetailsPanel::RenderImportOptions() {
    if (ImGui::Button("Reimport with Current Settings")) {
        ReimportAsset();
    }

    if (ImGui::Button("Reimport with New Settings")) {
        // Open import dialog
    }

    ImGui::Separator();

    ImGui::Text("Import Options:");
    ImGui::BulletText("Scale: 1.0");
    ImGui::BulletText("Generate Normals: Yes");
    ImGui::BulletText("Generate Tangents: Yes");
    ImGui::BulletText("Optimize Mesh: Yes");
}

void AssetDetailsPanel::RenderExportOptions() {
    if (ImGui::Button("Export as FBX")) {
        ExportAsset();
    }

    ImGui::SameLine();
    if (ImGui::Button("Export as OBJ")) {
        // Export as OBJ
    }

    ImGui::SameLine();
    if (ImGui::Button("Export as glTF")) {
        // Export as glTF
    }
}

void AssetDetailsPanel::SetSelectedAsset(Asset* asset) {
    m_selectedAsset = asset;
    RefreshAssetProperties();
}

void AssetDetailsPanel::ResetAllPropertiesToDefault() {
    if (!m_assetProperties) return;

    auto properties = m_assetProperties->GetAllProperties();
    for (const auto& prop : properties) {
        // Reset each property to default
    }
}

void AssetDetailsPanel::ApplyToAllInstances() {
    // Apply current asset properties to all instances
}

void AssetDetailsPanel::ReimportAsset() {
    // Reimport the asset with current settings
}

void AssetDetailsPanel::ExportAsset() {
    // Export asset to file
}

const char* AssetDetailsPanel::AssetTypeToString(AssetType type) const {
    switch (type) {
        case AssetType::Mesh: return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Texture: return "Texture";
        case AssetType::Audio: return "Audio";
        case AssetType::Animation: return "Animation";
        case AssetType::Prefab: return "Prefab";
        case AssetType::Script: return "Script";
        default: return "Unknown";
    }
}

void AssetDetailsPanel::RefreshAssetProperties() {
    if (!m_selectedAsset) return;

    // Load asset data into temp values
    m_tempValues.assetName = "AssetName";
    m_tempValues.assetPath = "Assets/...";
    m_assetType = AssetType::Mesh;
}

} // namespace Nova3D
