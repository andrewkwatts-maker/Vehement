#include "MaterialEditorPanel.hpp"
#include <fstream>

namespace Nova3D {

// Material preset definitions
namespace MaterialPresets {
    const MaterialPreset Glass = {
        "Glass",
        glm::vec3(1.0f, 1.0f, 1.0f),  // albedo
        0.0f,                           // metallic
        0.1f,                           // roughness
        1.5f,                           // ior
        0.0f,                           // subsurface
        glm::vec3(0.0f),               // emissive
        0.0f,                           // intensity
        true,                           // dispersion
        55.0f                           // abbe
    };

    const MaterialPreset Water = {
        "Water",
        glm::vec3(0.8f, 0.9f, 1.0f),
        0.0f,
        0.05f,
        1.33f,
        0.2f,
        glm::vec3(0.0f),
        0.0f,
        false,
        55.0f
    };

    const MaterialPreset Gold = {
        "Gold",
        glm::vec3(1.0f, 0.85f, 0.57f),
        1.0f,
        0.3f,
        0.47f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Silver = {
        "Silver",
        glm::vec3(0.97f, 0.96f, 0.91f),
        1.0f,
        0.25f,
        0.18f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Copper = {
        "Copper",
        glm::vec3(0.95f, 0.64f, 0.54f),
        1.0f,
        0.4f,
        1.1f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Diamond = {
        "Diamond",
        glm::vec3(1.0f, 1.0f, 1.0f),
        0.0f,
        0.0f,
        2.42f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        true,
        55.3f
    };

    const MaterialPreset Ruby = {
        "Ruby",
        glm::vec3(1.0f, 0.1f, 0.1f),
        0.0f,
        0.1f,
        1.77f,
        0.1f,
        glm::vec3(0.0f),
        0.0f,
        true,
        42.0f
    };

    const MaterialPreset Emerald = {
        "Emerald",
        glm::vec3(0.1f, 1.0f, 0.1f),
        0.0f,
        0.1f,
        1.57f,
        0.1f,
        glm::vec3(0.0f),
        0.0f,
        true,
        42.0f
    };

    const MaterialPreset Plastic = {
        "Plastic",
        glm::vec3(0.8f, 0.1f, 0.1f),
        0.0f,
        0.5f,
        1.46f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Rubber = {
        "Rubber",
        glm::vec3(0.2f, 0.2f, 0.2f),
        0.0f,
        0.9f,
        1.52f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Wood = {
        "Wood",
        glm::vec3(0.6f, 0.4f, 0.2f),
        0.0f,
        0.8f,
        1.54f,
        0.5f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Concrete = {
        "Concrete",
        glm::vec3(0.5f, 0.5f, 0.5f),
        0.0f,
        0.95f,
        1.55f,
        0.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Skin = {
        "Skin",
        glm::vec3(0.98f, 0.8f, 0.7f),
        0.0f,
        0.5f,
        1.4f,
        1.0f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Wax = {
        "Wax",
        glm::vec3(0.95f, 0.9f, 0.8f),
        0.0f,
        0.6f,
        1.44f,
        0.8f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };

    const MaterialPreset Ice = {
        "Ice",
        glm::vec3(0.9f, 0.95f, 1.0f),
        0.0f,
        0.05f,
        1.31f,
        0.1f,
        glm::vec3(0.0f),
        0.0f,
        false,
        0.0f
    };
}

MaterialEditorPanel::MaterialEditorPanel() {
    LoadPresets();
}

MaterialEditorPanel::~MaterialEditorPanel() {
    Shutdown();
}

void MaterialEditorPanel::Initialize() {
    // Initialize preview renderer
    // m_previewRenderer = std::make_unique<PreviewRenderer>();
    // m_previewCamera = std::make_unique<Camera>();

    // Initialize material graph
    // m_materialGraph = std::make_unique<MaterialGraph>();

    // Create property container for current material
    m_materialProperties = PropertySystem::Instance().CreateAssetContainer();
}

void MaterialEditorPanel::Shutdown() {
    m_previewRenderer.reset();
    m_previewCamera.reset();
    m_materialGraph.reset();
}

void MaterialEditorPanel::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Material Editor", &m_isOpen);

    // Toolbar
    RenderToolbar();

    ImGui::Separator();

    // Material selector
    RenderMaterialSelector();

    // Preset dropdown
    RenderPresetDropdown();

    ImGui::Separator();

    // Edit level selector
    const char* levels[] = { "Global", "Asset", "Instance" };
    int currentLevel = static_cast<int>(m_editLevel);
    if (ImGui::Combo("Edit Level", &currentLevel, levels, 3)) {
        m_editLevel = static_cast<PropertyLevel>(currentLevel);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Show Only Overridden", &m_showOnlyOverridden);

    ImGui::Separator();

    // Tabs for organization
    if (ImGui::BeginTabBar("MaterialTabs")) {
        if (ImGui::BeginTabItem("Basic")) {
            RenderBasicPropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Optical")) {
            RenderOpticalPropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Emission")) {
            RenderEmissionPropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scattering")) {
            RenderScatteringPropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures")) {
            RenderTexturesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Graph")) {
            RenderMaterialGraphTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Preview")) {
            RenderPreviewTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Status bar
    RenderStatusBar();

    ImGui::End();
}

void MaterialEditorPanel::RenderToolbar() {
    if (ImGui::Button("Save")) {
        SaveMaterial();
    }
    ImGui::SameLine();

    if (ImGui::Button("Save As")) {
        SaveMaterialAs();
    }
    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        // TODO: Open file dialog
    }
    ImGui::SameLine();

    if (ImGui::Button("Reset")) {
        ResetMaterial();
    }
    ImGui::SameLine();

    if (ImGui::Button("Duplicate")) {
        DuplicateMaterial();
    }
}

void MaterialEditorPanel::RenderMaterialSelector() {
    if (!m_currentMaterial && !m_materialLibrary.empty()) {
        m_currentMaterial = m_materialLibrary[0];
    }

    const char* currentName = m_currentMaterial ? "Current Material" : "No Material";

    if (ImGui::BeginCombo("Material", currentName)) {
        for (auto* mat : m_materialLibrary) {
            bool isSelected = (mat == m_currentMaterial);
            if (ImGui::Selectable("Material", isSelected)) {
                SetCurrentMaterial(mat);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void MaterialEditorPanel::RenderPresetDropdown() {
    if (ImGui::BeginCombo("Preset", "Custom")) {
        if (ImGui::Selectable("Glass")) ApplyPreset("Glass");
        if (ImGui::Selectable("Water")) ApplyPreset("Water");
        if (ImGui::Selectable("Gold")) ApplyPreset("Gold");
        if (ImGui::Selectable("Silver")) ApplyPreset("Silver");
        if (ImGui::Selectable("Copper")) ApplyPreset("Copper");
        if (ImGui::Selectable("Diamond")) ApplyPreset("Diamond");
        if (ImGui::Selectable("Ruby")) ApplyPreset("Ruby");
        if (ImGui::Selectable("Emerald")) ApplyPreset("Emerald");
        if (ImGui::Selectable("Plastic")) ApplyPreset("Plastic");
        if (ImGui::Selectable("Rubber")) ApplyPreset("Rubber");
        if (ImGui::Selectable("Wood")) ApplyPreset("Wood");
        if (ImGui::Selectable("Concrete")) ApplyPreset("Concrete");
        if (ImGui::Selectable("Skin")) ApplyPreset("Skin");
        if (ImGui::Selectable("Wax")) ApplyPreset("Wax");
        if (ImGui::Selectable("Ice")) ApplyPreset("Ice");
        ImGui::EndCombo();
    }
}

void MaterialEditorPanel::RenderBasicPropertiesTab() {
    PropertyOverrideUI::BeginCategory("Albedo");
    RenderAlbedoProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Metallic/Roughness");
    RenderMetallicRoughnessProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Normal");
    RenderNormalProperties();
    PropertyOverrideUI::EndCategory();
}

void MaterialEditorPanel::RenderAlbedoProperties() {
    PropertyOverrideUI::RenderColor3(
        "Albedo Color",
        m_tempValues.albedo,
        m_materialProperties,
        m_editLevel,
        nullptr,
        "Base color of the material"
    );
}

void MaterialEditorPanel::RenderMetallicRoughnessProperties() {
    PropertyOverrideUI::RenderFloat(
        "Metallic",
        m_tempValues.metallic,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "0 = Dielectric, 1 = Metallic"
    );

    PropertyOverrideUI::RenderFloat(
        "Roughness",
        m_tempValues.roughness,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "0 = Smooth, 1 = Rough"
    );
}

void MaterialEditorPanel::RenderNormalProperties() {
    ImGui::Text("Normal mapping controls will appear here");
}

void MaterialEditorPanel::RenderOpticalPropertiesTab() {
    PropertyOverrideUI::BeginCategory("Index of Refraction");
    RenderIORProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Dispersion");
    RenderDispersionProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Anisotropy");
    RenderAnisotropyProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Transmission");
    RenderTransmissionProperties();
    PropertyOverrideUI::EndCategory();
}

void MaterialEditorPanel::RenderIORProperties() {
    PropertyOverrideUI::RenderFloat(
        "IOR",
        m_tempValues.ior,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.1f, 5.0f,
        "Index of refraction (1.0 = air, 1.33 = water, 1.5 = glass, 2.42 = diamond)"
    );
}

void MaterialEditorPanel::RenderDispersionProperties() {
    PropertyOverrideUI::RenderBool(
        "Enable Dispersion",
        m_tempValues.enableDispersion,
        m_materialProperties,
        m_editLevel,
        nullptr,
        "Enable chromatic dispersion (rainbow effect)"
    );

    if (m_tempValues.enableDispersion) {
        PropertyOverrideUI::RenderFloat(
            "Abbe Number",
            m_tempValues.abbeNumber,
            m_materialProperties,
            m_editLevel,
            nullptr,
            10.0f, 100.0f,
            "Dispersion coefficient (lower = more rainbow effect)"
        );
    }
}

void MaterialEditorPanel::RenderAnisotropyProperties() {
    PropertyOverrideUI::RenderVec3(
        "Anisotropic IOR",
        m_tempValues.iorAnisotropic,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.1f, 5.0f,
        "Different IOR per axis for anisotropic materials"
    );
}

void MaterialEditorPanel::RenderTransmissionProperties() {
    PropertyOverrideUI::RenderFloat(
        "Transmission",
        m_tempValues.transmission,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "How much light passes through (0 = opaque, 1 = transparent)"
    );

    PropertyOverrideUI::RenderFloat(
        "Thickness",
        m_tempValues.thickness,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.01f, 100.0f,
        "Physical thickness of the material"
    );
}

void MaterialEditorPanel::RenderEmissionPropertiesTab() {
    PropertyOverrideUI::BeginCategory("Emissive Color");
    RenderEmissiveColorProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Blackbody Emission");
    RenderBlackbodyProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Luminosity");
    RenderLuminosityProperties();
    PropertyOverrideUI::EndCategory();
}

void MaterialEditorPanel::RenderEmissiveColorProperties() {
    PropertyOverrideUI::RenderColor3(
        "Emissive Color",
        m_tempValues.emissiveColor,
        m_materialProperties,
        m_editLevel,
        nullptr,
        "Color of emitted light"
    );

    PropertyOverrideUI::RenderFloat(
        "Emissive Intensity",
        m_tempValues.emissiveIntensity,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 100.0f,
        "Intensity of emitted light"
    );
}

void MaterialEditorPanel::RenderBlackbodyProperties() {
    PropertyOverrideUI::RenderBool(
        "Use Blackbody",
        m_tempValues.useBlackbody,
        m_materialProperties,
        m_editLevel,
        nullptr,
        "Use blackbody radiation for emission"
    );

    if (m_tempValues.useBlackbody) {
        PropertyOverrideUI::RenderFloat(
            "Temperature (K)",
            m_tempValues.emissiveTemperature,
            m_materialProperties,
            m_editLevel,
            nullptr,
            1000.0f, 10000.0f,
            "Blackbody temperature in Kelvin (6500K = daylight)"
        );
    }
}

void MaterialEditorPanel::RenderLuminosityProperties() {
    PropertyOverrideUI::RenderFloat(
        "Luminosity",
        m_tempValues.emissiveLuminosity,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 10000.0f,
        "Luminosity in lumens"
    );
}

void MaterialEditorPanel::RenderScatteringPropertiesTab() {
    PropertyOverrideUI::BeginCategory("Rayleigh Scattering");
    RenderRayleighScatteringProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Mie Scattering");
    RenderMieScatteringProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Subsurface Scattering");
    RenderSubsurfaceScatteringProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Volume Scattering");
    RenderVolumeScatteringProperties();
    PropertyOverrideUI::EndCategory();
}

void MaterialEditorPanel::RenderRayleighScatteringProperties() {
    PropertyOverrideUI::RenderVec3(
        "Rayleigh Coefficient",
        m_tempValues.rayleighCoefficient,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "Rayleigh scattering coefficient (atmospheric scattering)"
    );
}

void MaterialEditorPanel::RenderMieScatteringProperties() {
    PropertyOverrideUI::RenderVec3(
        "Mie Coefficient",
        m_tempValues.mieCoefficient,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "Mie scattering coefficient (fog, haze)"
    );

    PropertyOverrideUI::RenderFloat(
        "Mie Anisotropy",
        m_tempValues.mieAnisotropy,
        m_materialProperties,
        m_editLevel,
        nullptr,
        -1.0f, 1.0f,
        "Anisotropy of Mie scattering (-1 = backward, 0 = isotropic, 1 = forward)"
    );
}

void MaterialEditorPanel::RenderSubsurfaceScatteringProperties() {
    PropertyOverrideUI::RenderFloat(
        "Subsurface Scattering",
        m_tempValues.subsurfaceScattering,
        m_materialProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "Amount of subsurface scattering (skin, wax, marble)"
    );

    if (m_tempValues.subsurfaceScattering > 0.0f) {
        PropertyOverrideUI::RenderColor3(
            "Subsurface Color",
            m_tempValues.subsurfaceColor,
            m_materialProperties,
            m_editLevel,
            nullptr,
            "Color of subsurface scattering"
        );

        PropertyOverrideUI::RenderFloat(
            "Subsurface Radius",
            m_tempValues.subsurfaceRadius,
            m_materialProperties,
            m_editLevel,
            nullptr,
            0.01f, 10.0f,
            "Scattering radius"
        );
    }
}

void MaterialEditorPanel::RenderVolumeScatteringProperties() {
    ImGui::Text("Volume scattering controls will appear here");
}

void MaterialEditorPanel::RenderTexturesTab() {
    PropertyOverrideUI::BeginCategory("Color Maps");

    RenderTextureSlot("Albedo Map", m_tempValues.albedoMap, "Base color texture");
    RenderTextureSlot("Emissive Map", m_tempValues.emissiveMap, "Emissive color texture");

    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Surface Maps");

    RenderTextureSlot("Normal Map", m_tempValues.normalMap, "Normal map (tangent space)");
    RenderTextureSlot("Height Map", m_tempValues.heightMap, "Height map for parallax");
    RenderTextureSlot("Metallic Map", m_tempValues.metallicMap, "Metallic mask");
    RenderTextureSlot("Roughness Map", m_tempValues.roughnessMap, "Roughness map");
    RenderTextureSlot("AO Map", m_tempValues.aoMap, "Ambient occlusion");
    RenderTextureSlot("Opacity Map", m_tempValues.opacityMap, "Opacity/alpha mask");

    PropertyOverrideUI::EndCategory();
}

void MaterialEditorPanel::RenderTextureSlot(const char* label, std::string& texturePath, const char* tooltip) {
    PropertyOverrideUI::RenderTextureSlot(
        label,
        texturePath,
        m_materialProperties,
        m_editLevel,
        nullptr,
        tooltip
    );
}

void MaterialEditorPanel::RenderMaterialGraphTab() {
    ImGui::BeginChild("GraphEditor", ImVec2(0, -30), true);

    ImGui::Text("Material Graph Editor");
    ImGui::Separator();

    // Split into three sections
    if (m_showNodeLibrary) {
        ImGui::BeginChild("NodeLibrary", ImVec2(200, 0), true);
        RenderNodeLibrary();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginChild("NodeEditor", ImVec2(m_showNodeProperties ? -200 : 0, 0), true);
    RenderNodeEditor();
    ImGui::EndChild();

    if (m_showNodeProperties) {
        ImGui::SameLine();
        ImGui::BeginChild("NodeProperties", ImVec2(200, 0), true);
        RenderNodeProperties();
        ImGui::EndChild();
    }

    ImGui::EndChild();

    // Controls
    ImGui::Checkbox("Show Node Library", &m_showNodeLibrary);
    ImGui::SameLine();
    ImGui::Checkbox("Show Node Properties", &m_showNodeProperties);
}

void MaterialEditorPanel::RenderNodeLibrary() {
    ImGui::Text("Node Library");
    ImGui::Separator();

    if (ImGui::TreeNode("Inputs")) {
        ImGui::Selectable("Texture Sample");
        ImGui::Selectable("Vertex Color");
        ImGui::Selectable("UV Coordinates");
        ImGui::Selectable("World Position");
        ImGui::Selectable("World Normal");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Math")) {
        ImGui::Selectable("Add");
        ImGui::Selectable("Multiply");
        ImGui::Selectable("Lerp");
        ImGui::Selectable("Dot Product");
        ImGui::Selectable("Cross Product");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Outputs")) {
        ImGui::Selectable("Base Color");
        ImGui::Selectable("Metallic");
        ImGui::Selectable("Roughness");
        ImGui::Selectable("Normal");
        ImGui::Selectable("Emissive");
        ImGui::TreePop();
    }
}

void MaterialEditorPanel::RenderNodeEditor() {
    ImGui::Text("Node Editor Canvas");
    ImGui::Text("Drag nodes from library to add them");
    ImGui::Text("Right-click canvas for context menu");
}

void MaterialEditorPanel::RenderNodeProperties() {
    ImGui::Text("Node Properties");
    ImGui::Separator();
    ImGui::Text("Select a node to edit");
}

void MaterialEditorPanel::RenderPreviewTab() {
    ImGui::BeginChild("Preview", ImVec2(0, 0), false);

    // Preview controls
    RenderPreviewControls();

    ImGui::Separator();

    // Preview sphere
    RenderPreviewSphere();

    ImGui::EndChild();
}

void MaterialEditorPanel::RenderPreviewControls() {
    ImGui::Text("Preview Controls");

    ImGui::SliderInt("Preview Size", &m_previewSize, 128, 512);

    ImGui::Checkbox("Auto Rotate", &m_autoRotatePreview);

    if (!m_autoRotatePreview) {
        ImGui::SliderFloat("Rotation", &m_previewRotation, 0.0f, 360.0f);
    }

    if (ImGui::Button("Update Preview")) {
        UpdatePreview();
    }
}

void MaterialEditorPanel::RenderPreviewSphere() {
    // Placeholder for preview rendering
    ImVec2 size(static_cast<float>(m_previewSize), static_cast<float>(m_previewSize));

    ImGui::Text("Material Preview");

    // Draw a dummy rectangle as placeholder
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(50, 50, 50, 255));
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(size);
}

void MaterialEditorPanel::RenderStatusBar() {
    ImGui::Text("Material: %s | Edit Level: %s | Modified: %s",
               m_currentMaterial ? "Loaded" : "None",
               PropertyLevelToString(m_editLevel),
               m_materialProperties && m_materialProperties->HasDirtyProperties() ? "Yes" : "No");
}

void MaterialEditorPanel::SetCurrentMaterial(AdvancedMaterial* material) {
    m_currentMaterial = material;
    // Load material properties into temp values
    // TODO: Sync with actual material
}

void MaterialEditorPanel::SaveMaterial() {
    if (!m_currentMaterial) {
        return;
    }

    // TODO: Save material to file
    if (m_materialProperties) {
        m_materialProperties->ClearDirtyFlags();
    }
}

void MaterialEditorPanel::SaveMaterialAs() {
    // TODO: Open save dialog and save to new file
}

void MaterialEditorPanel::LoadMaterial(const std::string& filepath) {
    // TODO: Load material from file
}

void MaterialEditorPanel::ResetMaterial() {
    if (!m_materialProperties) {
        return;
    }

    // Reset all properties to defaults
    auto properties = m_materialProperties->GetAllProperties();
    for (const auto& prop : properties) {
        // Reset based on type
        // This is a simplified version
    }
}

void MaterialEditorPanel::DuplicateMaterial() {
    // TODO: Create a copy of current material
}

void MaterialEditorPanel::ApplyPreset(const std::string& presetName) {
    MaterialPreset preset;

    if (presetName == "Glass") preset = MaterialPresets::Glass;
    else if (presetName == "Water") preset = MaterialPresets::Water;
    else if (presetName == "Gold") preset = MaterialPresets::Gold;
    else if (presetName == "Silver") preset = MaterialPresets::Silver;
    else if (presetName == "Copper") preset = MaterialPresets::Copper;
    else if (presetName == "Diamond") preset = MaterialPresets::Diamond;
    else if (presetName == "Ruby") preset = MaterialPresets::Ruby;
    else if (presetName == "Emerald") preset = MaterialPresets::Emerald;
    else if (presetName == "Plastic") preset = MaterialPresets::Plastic;
    else if (presetName == "Rubber") preset = MaterialPresets::Rubber;
    else if (presetName == "Wood") preset = MaterialPresets::Wood;
    else if (presetName == "Concrete") preset = MaterialPresets::Concrete;
    else if (presetName == "Skin") preset = MaterialPresets::Skin;
    else if (presetName == "Wax") preset = MaterialPresets::Wax;
    else if (presetName == "Ice") preset = MaterialPresets::Ice;
    else return;

    // Apply preset values
    m_tempValues.albedo = preset.albedo;
    m_tempValues.metallic = preset.metallic;
    m_tempValues.roughness = preset.roughness;
    m_tempValues.ior = preset.ior;
    m_tempValues.subsurfaceScattering = preset.subsurfaceScattering;
    m_tempValues.emissiveColor = preset.emissiveColor;
    m_tempValues.emissiveIntensity = preset.emissiveIntensity;
    m_tempValues.enableDispersion = preset.enableDispersion;
    m_tempValues.abbeNumber = preset.abbeNumber;

    UpdatePreview();
}

void MaterialEditorPanel::SaveAsPreset(const std::string& presetName) {
    // TODO: Save current values as a custom preset
}

void MaterialEditorPanel::LoadPresets() {
    // Load presets from file or use built-in presets
    m_presets.push_back(MaterialPresets::Glass);
    m_presets.push_back(MaterialPresets::Water);
    m_presets.push_back(MaterialPresets::Gold);
    m_presets.push_back(MaterialPresets::Silver);
    m_presets.push_back(MaterialPresets::Copper);
    m_presets.push_back(MaterialPresets::Diamond);
    m_presets.push_back(MaterialPresets::Ruby);
    m_presets.push_back(MaterialPresets::Emerald);
    m_presets.push_back(MaterialPresets::Plastic);
    m_presets.push_back(MaterialPresets::Rubber);
    m_presets.push_back(MaterialPresets::Wood);
    m_presets.push_back(MaterialPresets::Concrete);
    m_presets.push_back(MaterialPresets::Skin);
    m_presets.push_back(MaterialPresets::Wax);
    m_presets.push_back(MaterialPresets::Ice);
}

void MaterialEditorPanel::SavePresets() {
    // TODO: Save presets to file
}

void MaterialEditorPanel::UpdatePreview() {
    if (m_autoRotatePreview) {
        m_previewRotation += 1.0f;
        if (m_previewRotation >= 360.0f) {
            m_previewRotation = 0.0f;
        }
    }

    // TODO: Render preview with current material
}

} // namespace Nova3D
