#include "TerrainPanel.hpp"
#include <imgui.h>

namespace Vehement {

TerrainPanel::TerrainPanel() = default;
TerrainPanel::~TerrainPanel() = default;

void TerrainPanel::Initialize(MapEditor& mapEditor) {
    m_mapEditor = &mapEditor;

    // Initialize texture list
    m_textureNames = {"Grass", "Dirt", "Sand", "Rock", "Snow"};
}

void TerrainPanel::Shutdown() {
    m_mapEditor = nullptr;
}

void TerrainPanel::Update(float deltaTime) {
    // Update any animated previews
}

void TerrainPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(280, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Terrain", nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::CollapsingHeader("Brush Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderBrushSettings();
        }

        if (ImGui::CollapsingHeader("Height Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderHeightTools();
        }

        if (ImGui::CollapsingHeader("Texture Painting")) {
            RenderTextureTools();
        }

        if (ImGui::CollapsingHeader("Water")) {
            RenderWaterTools();
        }

        if (ImGui::CollapsingHeader("Cliffs")) {
            RenderCliffTools();
        }
    }
    ImGui::End();
}

void TerrainPanel::RenderBrushSettings() {
    // Brush size
    if (ImGui::SliderFloat("Size", &m_brushSize, 1.0f, 32.0f, "%.0f")) {
        if (m_mapEditor) {
            m_mapEditor->SetBrushSize(m_brushSize);
        }
    }

    // Brush strength
    if (ImGui::SliderFloat("Strength", &m_brushStrength, 0.0f, 1.0f, "%.2f")) {
        if (m_mapEditor) {
            m_mapEditor->SetBrushStrength(m_brushStrength);
        }
    }

    // Falloff
    if (ImGui::SliderFloat("Falloff", &m_brushFalloff, 0.0f, 1.0f, "%.2f")) {
        if (m_mapEditor) {
            TerrainBrush brush = m_mapEditor->GetBrush();
            brush.falloff = m_brushFalloff;
            m_mapEditor->SetBrush(brush);
        }
    }

    // Brush shape
    const char* shapes[] = {"Circle", "Square", "Diamond"};
    if (ImGui::Combo("Shape", &m_brushShape, shapes, 3)) {
        if (m_mapEditor) {
            m_mapEditor->SetBrushShape(static_cast<BrushShape>(m_brushShape));
        }
    }

    // Smooth option
    if (ImGui::Checkbox("Smooth Edges", &m_smoothBrush)) {
        if (m_mapEditor) {
            TerrainBrush brush = m_mapEditor->GetBrush();
            brush.smooth = m_smoothBrush;
            m_mapEditor->SetBrush(brush);
        }
    }

    // Keyboard shortcuts info
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ] to change size");
}

void TerrainPanel::RenderHeightTools() {
    ImGui::Text("Mode:");

    // Tool selection buttons
    ImGui::BeginGroup();
    bool raise = (m_heightTool == 0);
    bool lower = (m_heightTool == 1);
    bool smooth = (m_heightTool == 2);
    bool plateau = (m_heightTool == 3);
    bool flatten = (m_heightTool == 4);

    if (ImGui::RadioButton("Raise", raise)) {
        m_heightTool = 0;
        if (m_mapEditor) m_mapEditor->SetHeightMode(HeightMode::Raise);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Lower", lower)) {
        m_heightTool = 1;
        if (m_mapEditor) m_mapEditor->SetHeightMode(HeightMode::Lower);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Smooth", smooth)) {
        m_heightTool = 2;
        if (m_mapEditor) m_mapEditor->SetHeightMode(HeightMode::Smooth);
    }

    if (ImGui::RadioButton("Plateau", plateau)) {
        m_heightTool = 3;
        if (m_mapEditor) m_mapEditor->SetHeightMode(HeightMode::Plateau);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Flatten", flatten)) {
        m_heightTool = 4;
        if (m_mapEditor) m_mapEditor->SetHeightMode(HeightMode::Flatten);
    }
    ImGui::EndGroup();

    // Plateau/flatten height
    if (m_heightTool == 3 || m_heightTool == 4) {
        ImGui::SliderFloat("Target Height", &m_plateauHeight, -10.0f, 10.0f);
        if (ImGui::Button("Sample Height")) {
            // Sample height from cursor position
        }
    }

    // Quick presets
    ImGui::Separator();
    ImGui::Text("Quick Presets:");
    if (ImGui::Button("Hill")) {
        m_heightTool = 0;
        m_brushSize = 8.0f;
        m_brushStrength = 0.3f;
        m_brushFalloff = 0.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Mountain")) {
        m_heightTool = 0;
        m_brushSize = 16.0f;
        m_brushStrength = 0.5f;
        m_brushFalloff = 0.3f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Valley")) {
        m_heightTool = 1;
        m_brushSize = 12.0f;
        m_brushStrength = 0.4f;
        m_brushFalloff = 0.4f;
    }
}

void TerrainPanel::RenderTextureTools() {
    ImGui::Text("Texture Layer:");

    // Texture selection grid
    int columns = 3;
    for (size_t i = 0; i < m_textureNames.size(); ++i) {
        bool isSelected = (m_selectedTexture == static_cast<int>(i));

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(m_textureNames[i].c_str(), isSelected,
                              ImGuiSelectableFlags_None, ImVec2(70, 70))) {
            m_selectedTexture = static_cast<int>(i);
            if (m_mapEditor) {
                m_mapEditor->SetCurrentTextureLayer(m_selectedTexture);
            }
        }
        ImGui::PopID();

        if ((i + 1) % columns != 0 && i < m_textureNames.size() - 1) {
            ImGui::SameLine();
        }
    }

    ImGui::Separator();

    // Texture settings
    if (ImGui::Button("Add Texture Layer")) {
        // Open texture browser
    }

    if (m_mapEditor) {
        const auto& layers = m_mapEditor->GetTextureLayers();
        if (!layers.empty() && m_selectedTexture < static_cast<int>(layers.size())) {
            ImGui::Text("Current: %s", layers[m_selectedTexture].textureId.c_str());
        }
    }
}

void TerrainPanel::RenderWaterTools() {
    if (ImGui::Checkbox("Enable Water", &m_waterEnabled)) {
        if (m_mapEditor) {
            m_mapEditor->SetWaterEnabled(m_waterEnabled);
        }
    }

    if (m_waterEnabled) {
        if (ImGui::SliderFloat("Water Level", &m_waterLevel, -5.0f, 10.0f)) {
            if (m_mapEditor) {
                m_mapEditor->SetWaterLevel(m_waterLevel);
            }
        }

        ImGui::Checkbox("Show Preview", &m_showWaterPreview);

        ImGui::Separator();
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("Sea Level")) {
            m_waterLevel = 0.0f;
            if (m_mapEditor) m_mapEditor->SetWaterLevel(0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Lake")) {
            m_waterLevel = -1.0f;
            if (m_mapEditor) m_mapEditor->SetWaterLevel(-1.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("River")) {
            m_waterLevel = -0.5f;
            if (m_mapEditor) m_mapEditor->SetWaterLevel(-0.5f);
        }

        // Water properties
        ImGui::Separator();
        ImGui::Text("Water Properties:");
        static float waterDepth = 2.0f;
        ImGui::SliderFloat("Deep Water Depth", &waterDepth, 0.5f, 5.0f);
        static float waveHeight = 0.1f;
        ImGui::SliderFloat("Wave Height", &waveHeight, 0.0f, 0.5f);
    }
}

void TerrainPanel::RenderCliffTools() {
    ImGui::SliderFloat("Cliff Height", &m_cliffHeight, 0.5f, 5.0f);

    const char* styles[] = {"Natural", "Rocky", "Smooth"};
    ImGui::Combo("Cliff Style", &m_cliffStyle, styles, 3);

    ImGui::Separator();

    if (ImGui::Button("Auto-Generate Cliffs")) {
        // Generate cliffs based on height differences
    }

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "Tip: Paint cliffs along steep slopes");
}

} // namespace Vehement
