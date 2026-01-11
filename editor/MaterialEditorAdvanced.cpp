#include "MaterialEditorAdvanced.hpp"
#include <imgui.h>
#include <imnodes.h>

namespace Engine {

MaterialEditorAdvanced::MaterialEditorAdvanced() {
    m_currentMaterial = std::make_shared<AdvancedMaterial>();
    LoadMaterialPresets();
    m_previewRenderer.Initialize();
}

MaterialEditorAdvanced::~MaterialEditorAdvanced() {
}

void MaterialEditorAdvanced::Render() {
    RenderMainWindow();
}

void MaterialEditorAdvanced::RenderMainWindow() {
    ImGui::Begin("Advanced Material Editor", nullptr, ImGuiWindowFlags_MenuBar);

    RenderMenuBar();

    // Main layout with splitter
    ImGui::BeginChild("LeftPanel", ImVec2(250, 0), true);
    RenderMaterialLibrary();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CenterPanel", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    // Tab bar for different views
    if (ImGui::BeginTabBar("EditorTabs")) {
        if (ImGui::BeginTabItem("Graph")) {
            RenderGraphEditor();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Properties")) {
            RenderPropertyInspector();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Preview")) {
            RenderPreviewPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    RenderStatusBar();

    ImGui::End();
}

void MaterialEditorAdvanced::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Material", "Ctrl+N")) {
                NewMaterial();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // Open file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_currentFilepath.empty()) {
                    SaveMaterial(m_currentFilepath);
                }
            }
            if (ImGui::MenuItem("Save As...")) {
                // Save file dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Exit editor
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_graphEditor.CanUndo())) {
                m_graphEditor.Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_graphEditor.CanRedo())) {
                m_graphEditor.Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                m_graphEditor.CopySelectedNodes();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                m_graphEditor.PasteNodes();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Graph Editor", nullptr, &m_showGraphEditor);
            ImGui::MenuItem("Properties", nullptr, &m_showPropertyInspector);
            ImGui::MenuItem("Preview", nullptr, &m_showPreview);
            ImGui::MenuItem("Material Library", nullptr, &m_showMaterialLibrary);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Compile Shader")) {
                m_graphEditor.CompileGraph();
            }
            if (ImGui::MenuItem("Validate Graph")) {
                // Validate graph
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void MaterialEditorAdvanced::RenderGraphEditor() {
    m_graphEditor.RenderNodeEditor();
}

void MaterialEditorAdvanced::RenderPropertyInspector() {
    ImGui::BeginChild("Properties", ImVec2(0, 0), false);

    if (ImGui::CollapsingHeader("Basic Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderBasicProperties();
    }

    if (ImGui::CollapsingHeader("Optical Properties")) {
        RenderOpticalProperties();
    }

    if (ImGui::CollapsingHeader("Emission")) {
        RenderEmissionProperties();
    }

    if (ImGui::CollapsingHeader("Subsurface Scattering")) {
        RenderSubsurfaceProperties();
    }

    if (ImGui::CollapsingHeader("Textures")) {
        RenderTextureProperties();
    }

    ImGui::EndChild();
}

void MaterialEditorAdvanced::RenderBasicProperties() {
    if (!m_currentMaterial) return;

    ImGui::ColorEdit3("Albedo", &m_currentMaterial->albedo[0]);
    ImGui::SliderFloat("Metallic", &m_currentMaterial->metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &m_currentMaterial->roughness, 0.01f, 1.0f);
    ImGui::SliderFloat("Specular", &m_currentMaterial->specular, 0.0f, 1.0f);

    m_hasUnsavedChanges = true;
}

void MaterialEditorAdvanced::RenderOpticalProperties() {
    if (!m_currentMaterial) return;

    ImGui::SliderFloat("IOR", &m_currentMaterial->ior, 1.0f, 3.0f);
    ImGui::SliderFloat("Transmission", &m_currentMaterial->transmission, 0.0f, 1.0f);

    ImGui::Checkbox("Enable Dispersion", &m_currentMaterial->dispersion.enableDispersion);
    if (m_currentMaterial->dispersion.enableDispersion) {
        ImGui::SliderFloat("Abbe Number", &m_currentMaterial->dispersion.abbeNumber, 10.0f, 100.0f);
    }

    m_hasUnsavedChanges = true;
}

void MaterialEditorAdvanced::RenderEmissionProperties() {
    if (!m_currentMaterial) return;

    ImGui::Checkbox("Enable Emission", &m_currentMaterial->emission.enabled);

    if (m_currentMaterial->emission.enabled) {
        ImGui::Checkbox("Use Blackbody", &m_currentMaterial->emission.useBlackbody);

        if (m_currentMaterial->emission.useBlackbody) {
            ImGui::SliderFloat("Temperature (K)", &m_currentMaterial->emission.temperature, 1000.0f, 10000.0f);
            ImGui::SliderFloat("Luminosity (cd/m²)", &m_currentMaterial->emission.luminosity, 0.0f, 10000.0f);
        } else {
            ImGui::ColorEdit3("Emission Color", &m_currentMaterial->emission.emissionColor[0]);
            ImGui::SliderFloat("Emission Strength", &m_currentMaterial->emission.emissionStrength, 0.0f, 100.0f);
        }
    }

    m_hasUnsavedChanges = true;
}

void MaterialEditorAdvanced::RenderSubsurfaceProperties() {
    if (!m_currentMaterial) return;

    ImGui::Checkbox("Enable SSS", &m_currentMaterial->subsurface.enabled);

    if (m_currentMaterial->subsurface.enabled) {
        ImGui::SliderFloat("Radius (mm)", &m_currentMaterial->subsurface.radius, 0.0f, 10.0f);
        ImGui::ColorEdit3("SSS Color", &m_currentMaterial->subsurface.color[0]);
        ImGui::SliderFloat("Density", &m_currentMaterial->subsurface.scatteringDensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Anisotropy", &m_currentMaterial->subsurface.scatteringAnisotropy, -1.0f, 1.0f);
    }

    m_hasUnsavedChanges = true;
}

void MaterialEditorAdvanced::RenderTextureProperties() {
    if (!m_currentMaterial) return;

    ImGui::Text("Albedo Map: %s", m_currentMaterial->albedoMap ? "Loaded" : "None");
    if (ImGui::Button("Load Albedo Map...")) {
        // Open file dialog
    }

    ImGui::Text("Normal Map: %s", m_currentMaterial->normalMap ? "Loaded" : "None");
    if (ImGui::Button("Load Normal Map...")) {
        // Open file dialog
    }

    ImGui::Text("Roughness Map: %s", m_currentMaterial->roughnessMap ? "Loaded" : "None");
    if (ImGui::Button("Load Roughness Map...")) {
        // Open file dialog
    }
}

void MaterialEditorAdvanced::RenderPreviewPanel() {
    RenderPreviewControls();

    ImGui::Separator();

    // Render preview image
    m_previewRenderer.Render(m_graphEditor.GetGraph());
    unsigned int previewTex = m_previewRenderer.GetPreviewTexture();

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    float size = glm::min(availSize.x, availSize.y);
    ImGui::Image((void*)(intptr_t)previewTex, ImVec2(size, size));
}

void MaterialEditorAdvanced::RenderPreviewControls() {
    const char* shapes[] = { "Sphere", "Cube", "Plane", "Cylinder", "Torus" };
    int currentShape = static_cast<int>(m_previewSettings.shape);
    if (ImGui::Combo("Shape", &currentShape, shapes, IM_ARRAYSIZE(shapes))) {
        m_previewSettings.shape = static_cast<MaterialGraphPreviewRenderer::PreviewShape>(currentShape);
    }

    ImGui::Checkbox("Auto Rotate", &m_previewSettings.autoRotate);
    if (!m_previewSettings.autoRotate) {
        ImGui::SliderFloat("Rotation", &m_previewSettings.rotation, 0.0f, 360.0f);
    }

    ImGui::SliderFloat("Light Intensity", &m_previewSettings.lightIntensity, 0.0f, 10.0f);
    ImGui::ColorEdit3("Light Color", &m_previewSettings.lightColor[0]);
    ImGui::ColorEdit3("Background", &m_previewSettings.backgroundColor[0]);

    // Apply settings to preview renderer
    m_previewRenderer.previewShape = m_previewSettings.shape;
    m_previewRenderer.autoRotate = m_previewSettings.autoRotate;
    m_previewRenderer.rotation = m_previewSettings.rotation;
    m_previewRenderer.lightIntensity = m_previewSettings.lightIntensity;
    m_previewRenderer.lightColor = m_previewSettings.lightColor;
}

void MaterialEditorAdvanced::RenderMaterialLibrary() {
    ImGui::Text("Material Presets");
    ImGui::Separator();

    for (size_t i = 0; i < m_materialPresets.size(); ++i) {
        bool selected = (m_selectedPreset == static_cast<int>(i));
        if (ImGui::Selectable(m_materialPresets[i].c_str(), selected)) {
            m_selectedPreset = static_cast<int>(i);
            ApplyPreset(m_materialPresets[i]);
        }
    }
}

void MaterialEditorAdvanced::RenderStatusBar() {
    ImGui::Separator();

    ImGui::Text("Material: %s", m_currentMaterial ? m_currentMaterial->name.c_str() : "None");
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    if (m_hasUnsavedChanges) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "* Unsaved Changes");
    } else {
        ImGui::Text("Saved");
    }
}

void MaterialEditorAdvanced::NewMaterial() {
    m_currentMaterial = std::make_shared<AdvancedMaterial>();
    m_currentMaterial->name = "New Material";
    m_currentFilepath.clear();
    m_hasUnsavedChanges = false;
    m_graphEditor.NewGraph();
}

void MaterialEditorAdvanced::LoadMaterial(const std::string& filepath) {
    m_currentMaterial = std::make_shared<AdvancedMaterial>();
    m_currentMaterial->Load(filepath);
    m_currentFilepath = filepath;
    m_hasUnsavedChanges = false;

    if (m_currentMaterial->materialGraph) {
        m_graphEditor.SetGraph(m_currentMaterial->materialGraph);
    }
}

void MaterialEditorAdvanced::SaveMaterial(const std::string& filepath) {
    if (m_currentMaterial) {
        m_currentMaterial->materialGraph = m_graphEditor.GetGraph();
        m_currentMaterial->Save(filepath);
        m_currentFilepath = filepath;
        m_hasUnsavedChanges = false;
    }
}

void MaterialEditorAdvanced::SetMaterial(std::shared_ptr<AdvancedMaterial> material) {
    m_currentMaterial = material;
    if (material && material->materialGraph) {
        m_graphEditor.SetGraph(material->materialGraph);
    }
}

std::shared_ptr<AdvancedMaterial> MaterialEditorAdvanced::GetMaterial() const {
    return m_currentMaterial;
}

void MaterialEditorAdvanced::LoadMaterialPresets() {
    m_materialPresets = {
        "Glass",
        "Water",
        "Gold",
        "Copper",
        "Diamond",
        "Plastic",
        "Skin",
        "Marble",
        "Wax",
        "Neon",
        "Velvet",
        "Car Paint"
    };
}

void MaterialEditorAdvanced::ApplyPreset(const std::string& presetName) {
    if (presetName == "Glass") {
        *m_currentMaterial = AdvancedMaterial::CreateGlass();
    } else if (presetName == "Water") {
        *m_currentMaterial = AdvancedMaterial::CreateWater();
    } else if (presetName == "Gold") {
        *m_currentMaterial = AdvancedMaterial::CreateGold();
    } else if (presetName == "Diamond") {
        *m_currentMaterial = AdvancedMaterial::CreateDiamond();
    } else if (presetName == "Skin") {
        *m_currentMaterial = AdvancedMaterial::CreateSkin();
    } else if (presetName == "Marble") {
        *m_currentMaterial = AdvancedMaterial::CreateMarble();
    } else if (presetName == "Velvet") {
        *m_currentMaterial = AdvancedMaterial::CreateVelvet(glm::vec3(0.7f, 0.2f, 0.2f));
    }

    m_hasUnsavedChanges = true;
    UpdatePreview();
}

void MaterialEditorAdvanced::UpdatePreview() {
    m_previewRenderer.Render(m_graphEditor.GetGraph());
}

// MaterialGraphPreviewRenderer implementation
MaterialGraphPreviewRenderer::MaterialGraphPreviewRenderer() {
}

MaterialGraphPreviewRenderer::~MaterialGraphPreviewRenderer() {
    if (m_fbo) {
        // Cleanup OpenGL resources
    }
}

void MaterialGraphPreviewRenderer::Initialize() {
    // Create framebuffer, texture, and depth buffer for preview rendering
    // OpenGL initialization code would go here
}

void MaterialGraphPreviewRenderer::Render(std::shared_ptr<MaterialGraph> graph) {
    // Render preview scene with current material
    // This would involve actual 3D rendering
}

void MaterialGraphPreviewRenderer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    // Resize framebuffer
}

} // namespace Engine
