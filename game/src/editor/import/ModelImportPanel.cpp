#include "ModelImportPanel.hpp"
#include <engine/import/ImportSettings.hpp>
#include <engine/import/ModelImporter.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace Vehement {

ModelImportPanel::ModelImportPanel()
    : m_settings(std::make_unique<Nova::ModelImportSettings>()) {
}

ModelImportPanel::~ModelImportPanel() = default;

void ModelImportPanel::Initialize() {
    // Setup default LOD distances
    m_lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
}

void ModelImportPanel::Shutdown() {
    m_previewModel.reset();
}

void ModelImportPanel::Update(float deltaTime) {
    if (m_previewDirty) {
        UpdatePreview();
        m_previewDirty = false;
    }

    if (m_animatePreview) {
        m_animationTime += deltaTime;
        m_previewRotation.y += deltaTime * 30.0f;  // Slow rotation
    }
}

void ModelImportPanel::Render() {
    RenderPreview3D();
    RenderMeshStatistics();
    RenderTransformSettings();
    RenderMeshProcessingSettings();
    RenderLODSettings();
    RenderMaterialList();

    if (m_previewModel && m_previewModel->hasSkeleton) {
        RenderSkeletonInfo();
    }

    RenderCollisionSettings();
}

void ModelImportPanel::SetModelPath(const std::string& path) {
    m_modelPath = path;
    m_settings->assetPath = path;
    m_previewDirty = true;
    m_selectedMaterialIndex = -1;

    LoadPreviewModel();
}

void ModelImportPanel::ApplyPreset(const std::string& preset) {
    if (preset == "Mobile") {
        m_settings->ApplyPreset(Nova::ImportPreset::Mobile);
    } else if (preset == "Desktop") {
        m_settings->ApplyPreset(Nova::ImportPreset::Desktop);
    } else if (preset == "HighQuality") {
        m_settings->ApplyPreset(Nova::ImportPreset::HighQuality);
    }

    m_previewDirty = true;
    if (m_onSettingsChanged) m_onSettingsChanged();
}

void ModelImportPanel::RotatePreview(float deltaX, float deltaY) {
    m_previewRotation.y += deltaX * 0.5f;
    m_previewRotation.x += deltaY * 0.5f;
    m_previewRotation.x = glm::clamp(m_previewRotation.x, -89.0f, 89.0f);
}

void ModelImportPanel::ZoomPreview(float delta) {
    m_previewZoom += delta * 0.1f;
    m_previewZoom = glm::clamp(m_previewZoom, 0.5f, 10.0f);
}

void ModelImportPanel::ResetPreview() {
    m_previewRotation = glm::vec3(0.0f);
    m_previewZoom = 2.0f;
}

void ModelImportPanel::RenderPreview3D() {
    /*
    ImGui::Text("3D Preview");
    ImGui::Separator();

    // Preview viewport
    ImVec2 previewSize(256, 256);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Would render 3D preview here using offscreen framebuffer
    ImGui::Dummy(previewSize);

    // Handle mouse interaction
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseDragging(0)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(0);
            RotatePreview(delta.x, delta.y);
            ImGui::ResetMouseDragDelta(0);
        }

        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ZoomPreview(wheel);
        }
    }

    // View controls
    ImGui::Checkbox("Wireframe", &m_showWireframe);
    ImGui::SameLine();
    ImGui::Checkbox("Normals", &m_showNormals);
    ImGui::SameLine();
    ImGui::Checkbox("Animate", &m_animatePreview);

    if (m_previewModel && m_previewModel->hasSkeleton) {
        ImGui::SameLine();
        ImGui::Checkbox("Bones", &m_showBones);
    }

    if (m_settings->generateCollision) {
        ImGui::SameLine();
        ImGui::Checkbox("Collision", &m_showCollision);
    }

    if (ImGui::Button("Reset View")) {
        ResetPreview();
    }

    // LOD slider
    if (m_settings->generateLODs && !m_lodTriangleCounts.empty()) {
        int maxLOD = static_cast<int>(m_lodTriangleCounts.size()) - 1;
        ImGui::SliderInt("LOD Level", &m_currentLODLevel, 0, maxLOD);
        ImGui::Text("Triangles: %d", m_lodTriangleCounts[m_currentLODLevel]);
    }
    */
}

void ModelImportPanel::RenderMeshStatistics() {
    /*
    ImGui::Text("Mesh Statistics");
    ImGui::Separator();

    if (m_previewModel) {
        ImGui::Text("Meshes: %zu", m_previewModel->meshes.size());
        ImGui::Text("Total Vertices: %u", m_previewModel->totalVertices);
        ImGui::Text("Total Triangles: %u", m_previewModel->totalTriangles);
        ImGui::Text("Materials: %u", m_previewModel->totalMaterials);
        ImGui::Text("Bones: %u", m_previewModel->totalBones);

        // Bounds
        glm::vec3 size = m_previewModel->boundsMax - m_previewModel->boundsMin;
        ImGui::Text("Bounds: %.2f x %.2f x %.2f", size.x, size.y, size.z);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
    }
    */
}

void ModelImportPanel::RenderTransformSettings() {
    /*
    ImGui::Text("Transform Settings");
    ImGui::Separator();

    // Scale
    if (ImGui::DragFloat("Scale Factor", &m_settings->scaleFactor, 0.01f, 0.001f, 100.0f, "%.3f")) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Units
    const char* unitNames[] = {"Meters", "Centimeters", "Millimeters", "Inches", "Feet"};

    int sourceUnit = static_cast<int>(m_settings->sourceUnits);
    if (ImGui::Combo("Source Units", &sourceUnit, unitNames, IM_ARRAYSIZE(unitNames))) {
        m_settings->sourceUnits = static_cast<Nova::ModelUnits>(sourceUnit);
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    int targetUnit = static_cast<int>(m_settings->targetUnits);
    if (ImGui::Combo("Target Units", &targetUnit, unitNames, IM_ARRAYSIZE(unitNames))) {
        m_settings->targetUnits = static_cast<Nova::ModelUnits>(targetUnit);
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Coordinate system
    if (ImGui::Checkbox("Swap Y/Z (Z-up to Y-up)", &m_settings->swapYZ)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Flip Winding Order", &m_settings->flipWindingOrder)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }
    */
}

void ModelImportPanel::RenderMeshProcessingSettings() {
    /*
    ImGui::Text("Mesh Processing");
    ImGui::Separator();

    if (ImGui::Checkbox("Optimize Mesh", &m_settings->optimizeMesh)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Generate Normals", &m_settings->generateNormals)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Generate Tangents", &m_settings->generateTangents)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Merge Vertices", &m_settings->mergeVertices)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->mergeVertices) {
        ImGui::DragFloat("Merge Threshold", &m_settings->mergeThreshold,
                         0.00001f, 0.00001f, 0.01f, "%.5f");
    }

    if (ImGui::Checkbox("Remove Redundant Materials", &m_settings->removeRedundantMaterials)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }
    */
}

void ModelImportPanel::RenderLODSettings() {
    /*
    ImGui::Text("LOD Generation");
    ImGui::Separator();

    if (ImGui::Checkbox("Generate LODs", &m_settings->generateLODs)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->generateLODs) {
        // LOD levels table
        ImGui::Text("LOD Levels:");

        ImGui::Columns(3, "LODColumns");
        ImGui::Text("Level"); ImGui::NextColumn();
        ImGui::Text("Distance"); ImGui::NextColumn();
        ImGui::Text("Reduction"); ImGui::NextColumn();
        ImGui::Separator();

        for (size_t i = 0; i < m_settings->lodDistances.size(); ++i) {
            ImGui::Text("%zu", i + 1);
            ImGui::NextColumn();

            float dist = m_settings->lodDistances[i];
            if (ImGui::DragFloat(("##dist" + std::to_string(i)).c_str(), &dist, 1.0f, 1.0f, 1000.0f)) {
                m_settings->lodDistances[i] = dist;
                m_previewDirty = true;
            }
            ImGui::NextColumn();

            float reduction = m_settings->lodReductions[i] * 100.0f;
            if (ImGui::DragFloat(("##red" + std::to_string(i)).c_str(), &reduction, 1.0f, 1.0f, 100.0f, "%.0f%%")) {
                m_settings->lodReductions[i] = reduction / 100.0f;
                m_previewDirty = true;
            }
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        // Add/remove LOD level buttons
        if (ImGui::Button("Add LOD Level") && m_settings->lodDistances.size() < 8) {
            float lastDist = m_settings->lodDistances.back();
            float lastRed = m_settings->lodReductions.back();
            m_settings->lodDistances.push_back(lastDist * 2.0f);
            m_settings->lodReductions.push_back(lastRed * 0.5f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove LOD Level") && m_settings->lodDistances.size() > 1) {
            m_settings->lodDistances.pop_back();
            m_settings->lodReductions.pop_back();
        }
    }
    */
}

void ModelImportPanel::RenderMaterialList() {
    /*
    if (!m_previewModel || m_previewModel->materials.empty()) return;

    ImGui::Text("Materials (%zu)", m_previewModel->materials.size());
    ImGui::Separator();

    for (size_t i = 0; i < m_previewModel->materials.size(); ++i) {
        const auto& mat = m_previewModel->materials[i];

        bool selected = (static_cast<int>(i) == m_selectedMaterialIndex);

        if (ImGui::Selectable(mat.name.c_str(), selected)) {
            m_selectedMaterialIndex = static_cast<int>(i);
        }

        // Color swatch
        ImGui::SameLine(200);
        ImVec4 color(mat.diffuseColor.r, mat.diffuseColor.g, mat.diffuseColor.b, 1.0f);
        ImGui::ColorButton(("##matcolor" + std::to_string(i)).c_str(), color,
                           ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

        // Texture count
        ImGui::SameLine();
        ImGui::Text("(%zu tex)", mat.textures.size());
    }

    // Material details
    if (m_selectedMaterialIndex >= 0 && m_selectedMaterialIndex < static_cast<int>(m_previewModel->materials.size())) {
        const auto& mat = m_previewModel->materials[m_selectedMaterialIndex];

        ImGui::Separator();
        ImGui::Text("Material: %s", mat.name.c_str());

        // Properties
        ImGui::Text("Metallic: %.2f", mat.metallic);
        ImGui::Text("Roughness: %.2f", mat.roughness);

        // Textures
        if (!mat.textures.empty()) {
            ImGui::Text("Textures:");
            for (const auto& tex : mat.textures) {
                ImGui::BulletText("%s: %s", tex.type.c_str(), tex.path.c_str());
            }
        }
    }
    */
}

void ModelImportPanel::RenderSkeletonInfo() {
    /*
    ImGui::Text("Skeleton");
    ImGui::Separator();

    if (m_previewModel) {
        ImGui::Text("Bones: %zu", m_previewModel->bones.size());

        if (ImGui::Checkbox("Import Skeleton", &m_settings->importSkeleton)) {
            if (m_onSettingsChanged) m_onSettingsChanged();
        }

        if (ImGui::Checkbox("Import Skin Weights", &m_settings->importSkinWeights)) {
            if (m_onSettingsChanged) m_onSettingsChanged();
        }

        ImGui::SliderInt("Max Bones Per Vertex", &m_settings->maxBonesPerVertex, 1, 8);

        // Bone list (collapsible)
        if (ImGui::TreeNode("Bone Hierarchy")) {
            for (const auto& bone : m_previewModel->bones) {
                ImGui::BulletText("%s (parent: %d)", bone.name.c_str(), bone.parentIndex);
            }
            ImGui::TreePop();
        }
    }
    */
}

void ModelImportPanel::RenderCollisionSettings() {
    /*
    ImGui::Text("Collision");
    ImGui::Separator();

    if (ImGui::Checkbox("Generate Collision", &m_settings->generateCollision)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->generateCollision) {
        if (ImGui::Checkbox("Convex Decomposition", &m_settings->convexDecomposition)) {
            m_previewDirty = true;
        }

        if (m_settings->convexDecomposition) {
            ImGui::SliderInt("Max Convex Hulls", &m_settings->maxConvexHulls, 1, 32);
            ImGui::SliderInt("Max Vertices Per Hull", &m_settings->maxVerticesPerHull, 8, 256);
        }

        if (ImGui::Checkbox("Simplified Collision", &m_settings->generateSimplifiedCollision)) {
            m_previewDirty = true;
        }

        if (m_settings->generateSimplifiedCollision) {
            ImGui::SliderFloat("Simplification", &m_settings->collisionSimplification, 0.1f, 1.0f);
        }
    }
    */
}

void ModelImportPanel::UpdatePreview() {
    if (m_modelPath.empty()) return;

    // Would regenerate 3D preview here
}

void ModelImportPanel::LoadPreviewModel() {
    if (m_modelPath.empty()) return;

    Nova::ModelImporter importer;
    Nova::ModelImportSettings previewSettings = *m_settings;
    previewSettings.generateLODs = false;  // Don't generate LODs for preview
    previewSettings.generateCollision = false;

    auto result = importer.Import(m_modelPath, previewSettings);

    if (result.success) {
        m_previewModel = std::make_unique<Nova::ImportedModel>(std::move(result));

        // Calculate preview center
        m_previewCenter = (m_previewModel->boundsMin + m_previewModel->boundsMax) * 0.5f;

        // Calculate initial zoom
        glm::vec3 size = m_previewModel->boundsMax - m_previewModel->boundsMin;
        float maxDim = std::max({size.x, size.y, size.z});
        m_previewZoom = maxDim * 1.5f;
    }
}

} // namespace Vehement
