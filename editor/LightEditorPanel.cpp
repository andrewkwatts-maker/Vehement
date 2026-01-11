#include "LightEditorPanel.hpp"
#include <cmath>

namespace Nova3D {

// Light preset definitions
namespace LightPresets {
    const LightPreset Daylight = {
        "Daylight",
        LightType::Directional,
        6500.0f,
        100000.0f,
        glm::vec3(1.0f, 1.0f, 1.0f),
        0.0f,
        0.0f
    };

    const LightPreset Tungsten = {
        "Tungsten",
        LightType::Point,
        2700.0f,
        800.0f,
        glm::vec3(1.0f, 0.8f, 0.6f),
        5.0f,
        0.0f
    };

    const LightPreset Fluorescent = {
        "Fluorescent",
        LightType::Tube,
        4000.0f,
        2000.0f,
        glm::vec3(0.9f, 1.0f, 1.0f),
        10.0f,
        0.0f
    };

    const LightPreset LED = {
        "LED",
        LightType::Point,
        5500.0f,
        1200.0f,
        glm::vec3(1.0f, 1.0f, 1.0f),
        5.0f,
        0.0f
    };

    const LightPreset Candle = {
        "Candle",
        LightType::Point,
        1850.0f,
        12.0f,
        glm::vec3(1.0f, 0.6f, 0.2f),
        2.0f,
        0.0f
    };

    const LightPreset Fire = {
        "Fire",
        LightType::Point,
        2000.0f,
        500.0f,
        glm::vec3(1.0f, 0.5f, 0.1f),
        3.0f,
        0.0f
    };

    const LightPreset Moonlight = {
        "Moonlight",
        LightType::Directional,
        4100.0f,
        0.25f,
        glm::vec3(0.7f, 0.8f, 1.0f),
        0.0f,
        0.0f
    };

    const LightPreset Streetlight = {
        "Streetlight",
        LightType::Point,
        2200.0f,
        5000.0f,
        glm::vec3(1.0f, 0.7f, 0.4f),
        15.0f,
        0.0f
    };
}

LightEditorPanel::LightEditorPanel() {
    LoadPresets();
}

LightEditorPanel::~LightEditorPanel() {
    Shutdown();
}

void LightEditorPanel::Initialize() {
    // Create property container for current light
    m_lightProperties = PropertySystem::Instance().CreateAssetContainer();
}

void LightEditorPanel::Shutdown() {
    m_iesProfiles.clear();
}

void LightEditorPanel::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Light Editor", &m_isOpen);

    // Toolbar
    RenderToolbar();

    ImGui::Separator();

    // Light selector
    RenderLightSelector();

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
    if (ImGui::BeginTabBar("LightTabs")) {
        if (ImGui::BeginTabItem("Basic")) {
            RenderBasicPropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physical Units")) {
            RenderPhysicalUnitsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Function")) {
            RenderFunctionTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Texture")) {
            RenderTextureTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("IES")) {
            RenderIESTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Shadows")) {
            RenderShadowsTab();
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

void LightEditorPanel::RenderToolbar() {
    if (ImGui::Button("Save")) {
        SaveLight();
    }
    ImGui::SameLine();

    if (ImGui::Button("Save As")) {
        SaveLightAs();
    }
    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        // TODO: Open file dialog
    }
    ImGui::SameLine();

    if (ImGui::Button("Reset")) {
        ResetLight();
    }
    ImGui::SameLine();

    if (ImGui::Button("Duplicate")) {
        DuplicateLight();
    }
}

void LightEditorPanel::RenderLightSelector() {
    const char* currentName = m_currentLight ? "Current Light" : "No Light";

    if (ImGui::BeginCombo("Light", currentName)) {
        for (auto* light : m_lightLibrary) {
            bool isSelected = (light == m_currentLight);
            if (ImGui::Selectable("Light", isSelected)) {
                SetCurrentLight(light);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void LightEditorPanel::RenderPresetDropdown() {
    if (ImGui::BeginCombo("Preset", "Custom")) {
        if (ImGui::Selectable("Daylight")) ApplyPreset("Daylight");
        if (ImGui::Selectable("Tungsten")) ApplyPreset("Tungsten");
        if (ImGui::Selectable("Fluorescent")) ApplyPreset("Fluorescent");
        if (ImGui::Selectable("LED")) ApplyPreset("LED");
        if (ImGui::Selectable("Candle")) ApplyPreset("Candle");
        if (ImGui::Selectable("Fire")) ApplyPreset("Fire");
        if (ImGui::Selectable("Moonlight")) ApplyPreset("Moonlight");
        if (ImGui::Selectable("Streetlight")) ApplyPreset("Streetlight");
        ImGui::EndCombo();
    }
}

void LightEditorPanel::RenderBasicPropertiesTab() {
    PropertyOverrideUI::BeginCategory("Light Type");
    RenderLightTypeSelector();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Color");
    RenderColorProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Intensity");
    RenderIntensityProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Range");
    RenderRangeProperties();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Shape");
    RenderShapeProperties();
    PropertyOverrideUI::EndCategory();
}

void LightEditorPanel::RenderLightTypeSelector() {
    const char* types[] = {
        "Directional", "Point", "Spot", "Area", "Tube", "Emissive", "IES", "Volumetric"
    };

    int currentType = static_cast<int>(m_tempValues.type);
    if (ImGui::Combo("Type", &currentType, types, 8)) {
        m_tempValues.type = static_cast<LightType>(currentType);
    }
}

void LightEditorPanel::RenderColorProperties() {
    PropertyOverrideUI::RenderBool(
        "Use Temperature",
        m_tempValues.useTemperature,
        m_lightProperties,
        m_editLevel,
        nullptr,
        "Use blackbody temperature for color"
    );

    if (m_tempValues.useTemperature) {
        PropertyOverrideUI::RenderFloat(
            "Temperature (K)",
            m_tempValues.temperature,
            m_lightProperties,
            m_editLevel,
            [this](float temp) {
                m_tempValues.color = TemperatureToColor(temp);
            },
            1000.0f, 10000.0f,
            "Color temperature in Kelvin"
        );

        // Show resulting color
        ImGui::Text("Resulting Color:");
        ImGui::SameLine();
        glm::vec3 tempColor = TemperatureToColor(m_tempValues.temperature);
        ImGui::ColorButton("##TempColor", ImVec4(tempColor.r, tempColor.g, tempColor.b, 1.0f));
    } else {
        PropertyOverrideUI::RenderColor3(
            "Color",
            m_tempValues.color,
            m_lightProperties,
            m_editLevel,
            nullptr,
            "Light color"
        );
    }
}

void LightEditorPanel::RenderIntensityProperties() {
    PropertyOverrideUI::RenderFloat(
        "Intensity",
        m_tempValues.intensity,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 100000.0f,
        "Light intensity"
    );
}

void LightEditorPanel::RenderRangeProperties() {
    if (m_tempValues.type != LightType::Directional) {
        PropertyOverrideUI::RenderFloat(
            "Radius",
            m_tempValues.radius,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.1f, 100.0f,
            "Light attenuation radius"
        );
    }
}

void LightEditorPanel::RenderShapeProperties() {
    switch (m_tempValues.type) {
        case LightType::Spot:
            PropertyOverrideUI::RenderFloat(
                "Inner Cone Angle",
                m_tempValues.coneAngleInner,
                m_lightProperties,
                m_editLevel,
                nullptr,
                0.0f, 90.0f,
                "Inner cone angle in degrees"
            );

            PropertyOverrideUI::RenderFloat(
                "Outer Cone Angle",
                m_tempValues.coneAngleOuter,
                m_lightProperties,
                m_editLevel,
                nullptr,
                0.0f, 90.0f,
                "Outer cone angle in degrees"
            );
            break;

        case LightType::Area:
            PropertyOverrideUI::RenderVec2(
                "Area Size",
                m_tempValues.areaSize,
                m_lightProperties,
                m_editLevel,
                nullptr,
                0.1f, 100.0f,
                "Size of area light"
            );
            break;

        case LightType::Tube:
            PropertyOverrideUI::RenderFloat(
                "Length",
                m_tempValues.length,
                m_lightProperties,
                m_editLevel,
                nullptr,
                0.1f, 100.0f,
                "Length of tube light"
            );
            break;

        default:
            break;
    }
}

void LightEditorPanel::RenderPhysicalUnitsTab() {
    PropertyOverrideUI::BeginCategory("Unit Selection");
    RenderUnitSelector();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Unit Controls");

    switch (m_tempValues.unit) {
        case LightUnit::Candela:
            RenderCandelaControls();
            break;
        case LightUnit::Lumen:
            RenderLumenControls();
            break;
        case LightUnit::Lux:
            RenderLuxControls();
            break;
        case LightUnit::Nits:
            RenderNitsControls();
            break;
    }

    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Temperature");
    RenderTemperatureControls();
    PropertyOverrideUI::EndCategory();
}

void LightEditorPanel::RenderUnitSelector() {
    const char* units[] = { "Candela (cd)", "Lumen (lm)", "Lux (lx)", "Nits (cd/m²)" };

    int currentUnit = static_cast<int>(m_tempValues.unit);
    if (ImGui::Combo("Unit", &currentUnit, units, 4)) {
        m_tempValues.unit = static_cast<LightUnit>(currentUnit);
    }

    // Show unit description
    ImGui::TextWrapped("Unit: %s", LightUnitToString(m_tempValues.unit));
}

void LightEditorPanel::RenderCandelaControls() {
    PropertyOverrideUI::RenderFloat(
        "Candela",
        m_tempValues.candela,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 10000.0f,
        "Luminous intensity in candela (cd)"
    );

    ImGui::TextWrapped("Candela measures the luminous intensity in a given direction.");
}

void LightEditorPanel::RenderLumenControls() {
    PropertyOverrideUI::RenderFloat(
        "Lumen",
        m_tempValues.lumen,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 100000.0f,
        "Luminous flux in lumens (lm)"
    );

    ImGui::TextWrapped("Lumen measures the total amount of light emitted by the source.");
}

void LightEditorPanel::RenderLuxControls() {
    PropertyOverrideUI::RenderFloat(
        "Lux",
        m_tempValues.lux,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 100000.0f,
        "Illuminance in lux (lx)"
    );

    ImGui::TextWrapped("Lux measures the amount of light falling on a surface.");
}

void LightEditorPanel::RenderNitsControls() {
    PropertyOverrideUI::RenderFloat(
        "Nits",
        m_tempValues.nits,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 10000.0f,
        "Luminance in nits (cd/m²)"
    );

    ImGui::TextWrapped("Nits measure the brightness of an emitting surface.");
}

void LightEditorPanel::RenderTemperatureControls() {
    PropertyOverrideUI::RenderFloat(
        "Temperature (K)",
        m_tempValues.temperature,
        m_lightProperties,
        m_editLevel,
        nullptr,
        1000.0f, 10000.0f,
        "Color temperature in Kelvin"
    );

    // Common temperature reference
    ImGui::Separator();
    ImGui::Text("Common Temperatures:");
    ImGui::BulletText("Candle: 1850K");
    ImGui::BulletText("Tungsten: 2700K");
    ImGui::BulletText("Halogen: 3200K");
    ImGui::BulletText("Fluorescent: 4000K");
    ImGui::BulletText("LED: 5500K");
    ImGui::BulletText("Daylight: 6500K");
    ImGui::BulletText("Sky: 10000K");
}

void LightEditorPanel::RenderFunctionTab() {
    PropertyOverrideUI::BeginCategory("Function Type");
    RenderFunctionTypeSelector();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Parameters");
    RenderFunctionParameters();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Preview");
    RenderFunctionPreview();
    PropertyOverrideUI::EndCategory();
}

void LightEditorPanel::RenderFunctionTypeSelector() {
    const char* types[] = {
        "Constant", "Sine", "Pulse", "Flicker", "Strobe",
        "Breath", "Fire", "Lightning", "Custom", "Texture"
    };

    int currentType = static_cast<int>(m_tempValues.functionType);
    if (ImGui::Combo("Type", &currentType, types, 10)) {
        m_tempValues.functionType = static_cast<LightFunctionType>(currentType);
    }
}

void LightEditorPanel::RenderFunctionParameters() {
    if (m_tempValues.functionType == LightFunctionType::Constant) {
        ImGui::Text("No parameters for constant function");
        return;
    }

    if (m_tempValues.functionType == LightFunctionType::Texture) {
        PropertyOverrideUI::RenderTextureSlot(
            "Function Texture",
            m_tempValues.functionTexture,
            m_lightProperties,
            m_editLevel,
            nullptr,
            "Texture to use for light animation"
        );
    } else {
        PropertyOverrideUI::RenderFloat(
            "Frequency",
            m_tempValues.functionFrequency,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.1f, 10.0f,
            "Animation frequency"
        );

        PropertyOverrideUI::RenderFloat(
            "Amplitude",
            m_tempValues.functionAmplitude,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.0f, 2.0f,
            "Animation amplitude"
        );

        PropertyOverrideUI::RenderFloat(
            "Phase",
            m_tempValues.functionPhase,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.0f, 360.0f,
            "Animation phase offset"
        );

        PropertyOverrideUI::RenderFloat(
            "Offset",
            m_tempValues.functionOffset,
            m_lightProperties,
            m_editLevel,
            nullptr,
            -1.0f, 1.0f,
            "Vertical offset"
        );
    }
}

void LightEditorPanel::RenderFunctionPreview() {
    ImGui::Text("Function Preview");

    // Draw a simple graph
    ImVec2 graphSize(400, 100);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + graphSize.x, pos.y + graphSize.y),
                           IM_COL32(30, 30, 30, 255));

    // Grid lines
    for (int i = 0; i <= 4; ++i) {
        float y = pos.y + (graphSize.y / 4.0f) * i;
        drawList->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + graphSize.x, y),
                         IM_COL32(60, 60, 60, 255));
    }

    // Draw function curve (simplified)
    const int steps = 100;
    for (int i = 0; i < steps - 1; ++i) {
        float t1 = static_cast<float>(i) / steps;
        float t2 = static_cast<float>(i + 1) / steps;

        float v1 = 0.5f; // Placeholder value
        float v2 = 0.5f; // Placeholder value

        ImVec2 p1(pos.x + t1 * graphSize.x, pos.y + (1.0f - v1) * graphSize.y);
        ImVec2 p2(pos.x + t2 * graphSize.x, pos.y + (1.0f - v2) * graphSize.y);

        drawList->AddLine(p1, p2, IM_COL32(255, 255, 0, 255), 2.0f);
    }

    ImGui::Dummy(graphSize);
}

void LightEditorPanel::RenderTextureTab() {
    PropertyOverrideUI::BeginCategory("Texture Mapping");
    RenderTextureMappingSelector();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Texture");

    PropertyOverrideUI::RenderTextureSlot(
        "Light Texture",
        m_tempValues.lightTexture,
        m_lightProperties,
        m_editLevel,
        nullptr,
        "Texture to project from light"
    );

    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Mapping Controls");

    switch (m_tempValues.textureMapping) {
        case LightTextureMapping::UV:
            RenderUVMappingControls();
            break;
        case LightTextureMapping::Spherical:
        case LightTextureMapping::Cylindrical:
        case LightTextureMapping::LatLong:
            RenderSphericalMappingControls();
            break;
        case LightTextureMapping::Gobo:
            RenderGoboControls();
            break;
    }

    PropertyOverrideUI::EndCategory();
}

void LightEditorPanel::RenderTextureMappingSelector() {
    const char* modes[] = { "UV", "Spherical", "Cylindrical", "Lat-Long", "Gobo" };

    int currentMode = static_cast<int>(m_tempValues.textureMapping);
    if (ImGui::Combo("Mapping Mode", &currentMode, modes, 5)) {
        m_tempValues.textureMapping = static_cast<LightTextureMapping>(currentMode);
    }
}

void LightEditorPanel::RenderUVMappingControls() {
    PropertyOverrideUI::RenderVec2(
        "Texture Scale",
        m_tempValues.textureScale,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.1f, 10.0f,
        "Texture tiling scale"
    );

    PropertyOverrideUI::RenderVec2(
        "Texture Offset",
        m_tempValues.textureOffset,
        m_lightProperties,
        m_editLevel,
        nullptr,
        -1.0f, 1.0f,
        "Texture offset"
    );

    PropertyOverrideUI::RenderAngle(
        "Texture Rotation",
        m_tempValues.textureRotation,
        m_lightProperties,
        m_editLevel,
        nullptr,
        "Texture rotation"
    );
}

void LightEditorPanel::RenderSphericalMappingControls() {
    ImGui::Text("Spherical mapping uses automatic coordinates");
}

void LightEditorPanel::RenderGoboControls() {
    ImGui::Text("Gobo projection for spotlights and area lights");

    PropertyOverrideUI::RenderAngle(
        "Gobo Rotation",
        m_tempValues.textureRotation,
        m_lightProperties,
        m_editLevel,
        nullptr,
        "Gobo rotation angle"
    );
}

void LightEditorPanel::RenderIESTab() {
    PropertyOverrideUI::BeginCategory("IES Profile");
    RenderIESProfileSelector();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Preview");
    RenderIESPreview();
    PropertyOverrideUI::EndCategory();
}

void LightEditorPanel::RenderIESProfileSelector() {
    ImGui::Text("IES Profile Selector");

    if (ImGui::Button("Load IES File...")) {
        // TODO: Open file dialog
    }

    if (!m_iesProfiles.empty()) {
        if (ImGui::BeginCombo("Profile", "Select Profile")) {
            for (size_t i = 0; i < m_iesProfiles.size(); ++i) {
                bool isSelected = (static_cast<int>(i) == m_currentIESProfile);
                if (ImGui::Selectable("Profile", isSelected)) {
                    m_currentIESProfile = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::Text("No IES profiles loaded");
    }
}

void LightEditorPanel::RenderIESPreview() {
    ImGui::Text("IES Profile Preview");
    ImGui::Text("(Polar plot of light distribution)");

    // Placeholder for IES visualization
    ImVec2 size(256, 256);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           IM_COL32(30, 30, 30, 255));
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(size);
}

void LightEditorPanel::RenderShadowsTab() {
    PropertyOverrideUI::BeginCategory("Shadow Settings");
    RenderShadowSettings();
    PropertyOverrideUI::EndCategory();

    if (m_tempValues.type == LightType::Directional) {
        PropertyOverrideUI::BeginCategory("Cascade Settings");
        RenderCascadeSettings();
        PropertyOverrideUI::EndCategory();
    }
}

void LightEditorPanel::RenderShadowSettings() {
    PropertyOverrideUI::RenderBool(
        "Cast Shadows",
        m_tempValues.castShadows,
        m_lightProperties,
        m_editLevel,
        nullptr,
        "Enable shadow casting"
    );

    if (m_tempValues.castShadows) {
        const char* sizes[] = { "512", "1024", "2048", "4096" };
        int sizeIndex = 1; // Default to 1024
        if (m_tempValues.shadowMapSize == 512) sizeIndex = 0;
        else if (m_tempValues.shadowMapSize == 1024) sizeIndex = 1;
        else if (m_tempValues.shadowMapSize == 2048) sizeIndex = 2;
        else if (m_tempValues.shadowMapSize == 4096) sizeIndex = 3;

        PropertyOverrideUI::RenderCombo(
            "Shadow Map Size",
            sizeIndex,
            sizes,
            4,
            m_lightProperties,
            m_editLevel,
            [this](int index) {
                int sizes[] = { 512, 1024, 2048, 4096 };
                m_tempValues.shadowMapSize = sizes[index];
            },
            "Resolution of shadow map"
        );

        PropertyOverrideUI::RenderFloat(
            "Shadow Bias",
            m_tempValues.shadowBias,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.0f, 0.01f,
            "Depth bias to reduce shadow acne",
            "%.5f"
        );

        PropertyOverrideUI::RenderFloat(
            "Normal Bias",
            m_tempValues.shadowNormalBias,
            m_lightProperties,
            m_editLevel,
            nullptr,
            0.0f, 0.1f,
            "Normal-based bias",
            "%.4f"
        );
    }
}

void LightEditorPanel::RenderCascadeSettings() {
    PropertyOverrideUI::RenderInt(
        "Cascade Count",
        m_tempValues.cascadeCount,
        m_lightProperties,
        m_editLevel,
        nullptr,
        1, 8,
        "Number of cascade splits"
    );

    PropertyOverrideUI::RenderFloat(
        "Split Lambda",
        m_tempValues.cascadeSplitLambda,
        m_lightProperties,
        m_editLevel,
        nullptr,
        0.0f, 1.0f,
        "Balance between logarithmic and uniform splits"
    );
}

void LightEditorPanel::RenderPreviewTab() {
    ImGui::BeginChild("Preview", ImVec2(0, 0), false);

    RenderPreviewControls();

    ImGui::Separator();

    RenderLightPreview();

    ImGui::EndChild();
}

void LightEditorPanel::RenderPreviewControls() {
    ImGui::Text("Preview Controls");

    ImGui::SliderInt("Preview Size", &m_previewSize, 128, 512);

    if (ImGui::Button("Update Preview")) {
        UpdatePreview();
    }
}

void LightEditorPanel::RenderLightPreview() {
    ImVec2 size(static_cast<float>(m_previewSize), static_cast<float>(m_previewSize));

    ImGui::Text("Light Preview");

    // Placeholder
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           IM_COL32(20, 20, 20, 255));
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(size);
}

void LightEditorPanel::RenderStatusBar() {
    ImGui::Text("Light: %s | Type: %s | Edit Level: %s",
               m_currentLight ? "Loaded" : "None",
               LightTypeToString(m_tempValues.type),
               PropertyLevelToString(m_editLevel));
}

void LightEditorPanel::SetCurrentLight(Light* light) {
    m_currentLight = light;
    // Load light properties
}

void LightEditorPanel::SaveLight() {
    if (!m_currentLight) return;

    if (m_lightProperties) {
        m_lightProperties->ClearDirtyFlags();
    }
}

void LightEditorPanel::SaveLightAs() {
    // TODO: Save dialog
}

void LightEditorPanel::LoadLight(const std::string& filepath) {
    // TODO: Load from file
}

void LightEditorPanel::ResetLight() {
    // TODO: Reset properties
}

void LightEditorPanel::DuplicateLight() {
    // TODO: Duplicate light
}

void LightEditorPanel::ApplyPreset(const std::string& presetName) {
    LightPreset preset;

    if (presetName == "Daylight") preset = LightPresets::Daylight;
    else if (presetName == "Tungsten") preset = LightPresets::Tungsten;
    else if (presetName == "Fluorescent") preset = LightPresets::Fluorescent;
    else if (presetName == "LED") preset = LightPresets::LED;
    else if (presetName == "Candle") preset = LightPresets::Candle;
    else if (presetName == "Fire") preset = LightPresets::Fire;
    else if (presetName == "Moonlight") preset = LightPresets::Moonlight;
    else if (presetName == "Streetlight") preset = LightPresets::Streetlight;
    else return;

    m_tempValues.type = preset.type;
    m_tempValues.temperature = preset.temperature;
    m_tempValues.intensity = preset.intensity;
    m_tempValues.color = preset.color;
    m_tempValues.radius = preset.radius;
}

void LightEditorPanel::SaveAsPreset(const std::string& presetName) {
    // TODO: Save as preset
}

void LightEditorPanel::LoadPresets() {
    m_presets.push_back(LightPresets::Daylight);
    m_presets.push_back(LightPresets::Tungsten);
    m_presets.push_back(LightPresets::Fluorescent);
    m_presets.push_back(LightPresets::LED);
    m_presets.push_back(LightPresets::Candle);
    m_presets.push_back(LightPresets::Fire);
    m_presets.push_back(LightPresets::Moonlight);
    m_presets.push_back(LightPresets::Streetlight);
}

void LightEditorPanel::SavePresets() {
    // TODO: Save to file
}

void LightEditorPanel::UpdatePreview() {
    // TODO: Render preview
}

const char* LightEditorPanel::LightTypeToString(LightType type) const {
    switch (type) {
        case LightType::Directional: return "Directional";
        case LightType::Point: return "Point";
        case LightType::Spot: return "Spot";
        case LightType::Area: return "Area";
        case LightType::Tube: return "Tube";
        case LightType::Emissive: return "Emissive";
        case LightType::IES: return "IES";
        case LightType::Volumetric: return "Volumetric";
        default: return "Unknown";
    }
}

const char* LightEditorPanel::LightUnitToString(LightUnit unit) const {
    switch (unit) {
        case LightUnit::Candela: return "Candela (cd)";
        case LightUnit::Lumen: return "Lumen (lm)";
        case LightUnit::Lux: return "Lux (lx)";
        case LightUnit::Nits: return "Nits (cd/m²)";
        default: return "Unknown";
    }
}

const char* LightEditorPanel::LightFunctionTypeToString(LightFunctionType type) const {
    switch (type) {
        case LightFunctionType::Constant: return "Constant";
        case LightFunctionType::Sine: return "Sine";
        case LightFunctionType::Pulse: return "Pulse";
        case LightFunctionType::Flicker: return "Flicker";
        case LightFunctionType::Strobe: return "Strobe";
        case LightFunctionType::Breath: return "Breath";
        case LightFunctionType::Fire: return "Fire";
        case LightFunctionType::Lightning: return "Lightning";
        case LightFunctionType::Custom: return "Custom";
        case LightFunctionType::Texture: return "Texture";
        default: return "Unknown";
    }
}

glm::vec3 LightEditorPanel::TemperatureToColor(float kelvin) const {
    // Simplified blackbody approximation
    float temp = kelvin / 100.0f;

    float red, green, blue;

    // Red
    if (temp <= 66.0f) {
        red = 255.0f;
    } else {
        red = temp - 60.0f;
        red = 329.698727446f * std::pow(red, -0.1332047592f);
        red = glm::clamp(red, 0.0f, 255.0f);
    }

    // Green
    if (temp <= 66.0f) {
        green = temp;
        green = 99.4708025861f * std::log(green) - 161.1195681661f;
    } else {
        green = temp - 60.0f;
        green = 288.1221695283f * std::pow(green, -0.0755148492f);
    }
    green = glm::clamp(green, 0.0f, 255.0f);

    // Blue
    if (temp >= 66.0f) {
        blue = 255.0f;
    } else if (temp <= 19.0f) {
        blue = 0.0f;
    } else {
        blue = temp - 10.0f;
        blue = 138.5177312231f * std::log(blue) - 305.0447927307f;
        blue = glm::clamp(blue, 0.0f, 255.0f);
    }

    return glm::vec3(red / 255.0f, green / 255.0f, blue / 255.0f);
}

} // namespace Nova3D
