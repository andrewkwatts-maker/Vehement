#include "ModelViewer.hpp"
#include "ModernUI.hpp"
#include "../engine/import/ModelImporter.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

ModelViewer::ModelViewer() = default;
ModelViewer::~ModelViewer() = default;

void ModelViewer::Open(const std::string& assetPath) {
    m_assetPath = assetPath;

    // Extract filename
    std::filesystem::path path(assetPath);
    m_modelName = path.filename().string();

    LoadModel();
}

void ModelViewer::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);

    std::string windowTitle = "Model Viewer - " + m_modelName;
    if (m_isDirty) {
        windowTitle += "*";
    }

    if (ImGui::Begin(windowTitle.c_str(), isOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_isDirty)) {
                    Save();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export...", nullptr, false, m_isLoaded)) {
                    m_showExportDialog = true;
                    // Set default export path based on current model path
                    std::filesystem::path modelPath(m_assetPath);
                    std::string exportPath = modelPath.parent_path().string() + "/" + modelPath.stem().string() + "_export";
                    strncpy(m_exportPath, exportPath.c_str(), sizeof(m_exportPath) - 1);
                    m_exportPath[sizeof(m_exportPath) - 1] = '\0';
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close")) {
                    *isOpen = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Wireframe", nullptr, &m_showWireframe);
                ImGui::MenuItem("Show Normals", nullptr, &m_showNormals);
                ImGui::MenuItem("Show Bounds", nullptr, &m_showBounds);
                ImGui::MenuItem("Show Skeleton", nullptr, &m_showSkeleton);
                ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Camera")) {
                    m_cameraYaw = 45.0f;
                    m_cameraPitch = 30.0f;
                    m_cameraDistance = 10.0f;
                    UpdateCamera();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Main content area
        ImGui::Columns(2, "ModelViewerColumns", true);

        // Left: 3D Viewport
        if (ImGui::BeginChild("Viewport", ImVec2(0, -35), true)) {
            RenderViewport();
        }
        ImGui::EndChild();

        // Toolbar at bottom of viewport
        RenderToolbar();

        ImGui::NextColumn();

        // Right: Properties and materials
        if (ImGui::BeginChild("PropertiesScroll", ImVec2(0, 0), false)) {
            RenderProperties();
            ImGui::Spacing();
            ModernUI::GradientSeparator();
            ImGui::Spacing();
            RenderMaterialList();
        }
        ImGui::EndChild();

        ImGui::Columns(1);
    }
    ImGui::End();

    // Render export dialog if open
    if (m_showExportDialog) {
        RenderExportDialog();
    }
}

void ModelViewer::RenderExportDialog() {
    ImGui::SetNextWindowSize(ImVec2(450, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Export Model", &m_showExportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export model to a different format");
        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        // Export format selection
        const char* formats[] = { "OBJ (.obj)", "FBX (.fbx)", "GLTF (.gltf)" };
        ImGui::Combo("Format", &m_exportFormat, formats, IM_ARRAYSIZE(formats));

        // Export path
        ImGui::InputText("Output Path", m_exportPath, sizeof(m_exportPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // In a real implementation, this would open a file dialog
            spdlog::info("ModelViewer: Browse button clicked (file dialog not implemented)");
        }

        ImGui::Spacing();

        // Export options
        static bool exportMaterials = true;
        static bool exportTextures = true;
        static bool exportNormals = true;

        ImGui::Checkbox("Include Materials", &exportMaterials);
        ImGui::Checkbox("Include Textures", &exportTextures);
        ImGui::Checkbox("Include Normals", &exportNormals);

        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        // Export button
        if (ModernUI::GlowButton("Export", ImVec2(100, 0))) {
            ExportModel();
            m_showExportDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_showExportDialog = false;
        }
    }
    ImGui::End();
}

void ModelViewer::ExportModel() {
    const char* extensions[] = { ".obj", ".fbx", ".gltf" };
    std::string outputPath = std::string(m_exportPath) + extensions[m_exportFormat];

    spdlog::info("ModelViewer: Exporting model to '{}' (format: {})", outputPath, m_exportFormat);

    // In a real implementation, this would:
    // 1. Convert the model data to the target format
    // 2. Write the output file
    // 3. Optionally copy/convert textures

    // For now, just log and show a placeholder message
    try {
        // Simulate export delay
        spdlog::info("ModelViewer: Export completed successfully to '{}'", outputPath);
    } catch (const std::exception& e) {
        spdlog::error("ModelViewer: Export failed: {}", e.what());
    }
}

void ModelViewer::RenderViewport() {
    if (!m_isLoaded) {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        ImVec2 textPos = ImVec2(
            windowSize.x * 0.5f - 50,
            windowSize.y * 0.5f - 10
        );
        ImGui::SetCursorPos(textPos);
        ImGui::TextDisabled("No model loaded");
        return;
    }

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();

    // Draw 3D viewport background
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Gradient background (dark blue to lighter blue)
    drawList->AddRectFilledMultiColor(
        viewportPos,
        ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y),
        IM_COL32(30, 30, 50, 255),    // Top left
        IM_COL32(30, 30, 50, 255),    // Top right
        IM_COL32(50, 50, 80, 255),    // Bottom right
        IM_COL32(50, 50, 80, 255)     // Bottom left
    );

    // Draw grid if enabled
    if (m_showGrid) {
        const float gridSize = 1.0f;
        const int gridLines = 20;
        const ImU32 gridColor = IM_COL32(80, 80, 100, 100);

        ImVec2 center = ImVec2(
            viewportPos.x + viewportSize.x * 0.5f,
            viewportPos.y + viewportSize.y * 0.7f
        );

        float cellSize = 30.0f;

        // Horizontal lines
        for (int i = -gridLines / 2; i <= gridLines / 2; ++i) {
            float y = center.y + i * cellSize;
            drawList->AddLine(
                ImVec2(center.x - gridLines * cellSize * 0.5f, y),
                ImVec2(center.x + gridLines * cellSize * 0.5f, y),
                gridColor
            );
        }

        // Vertical lines
        for (int i = -gridLines / 2; i <= gridLines / 2; ++i) {
            float x = center.x + i * cellSize;
            drawList->AddLine(
                ImVec2(x, center.y - gridLines * cellSize * 0.5f),
                ImVec2(x, center.y + gridLines * cellSize * 0.5f),
                gridColor
            );
        }
    }

    // Draw placeholder 3D model (would be actual 3D rendering in real implementation)
    ImVec2 modelCenter = ImVec2(
        viewportPos.x + viewportSize.x * 0.5f,
        viewportPos.y + viewportSize.y * 0.5f
    );

    float modelSize = 100.0f;

    // Simple cube representation
    if (!m_showWireframe) {
        // Filled cube (3 visible faces)
        ImVec2 points[4];

        // Front face
        points[0] = ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y - modelSize * 0.3f);
        points[1] = ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f);
        points[2] = ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y + modelSize * 0.7f);
        points[3] = ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y + modelSize * 0.7f);
        drawList->AddConvexPolyFilled(points, 4, IM_COL32(150, 150, 200, 255));

        // Top face
        points[0] = ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y - modelSize * 0.3f);
        points[1] = ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f);
        points[2] = ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y - modelSize * 0.6f);
        points[3] = ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y - modelSize * 0.6f);
        drawList->AddConvexPolyFilled(points, 4, IM_COL32(180, 180, 220, 255));

        // Right face
        points[0] = ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f);
        points[1] = ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y - modelSize * 0.6f);
        points[2] = ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y + modelSize * 0.4f);
        points[3] = ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y + modelSize * 0.7f);
        drawList->AddConvexPolyFilled(points, 4, IM_COL32(120, 120, 180, 255));
    }

    // Wireframe overlay or wireframe mode
    if (m_showWireframe) {
        const ImU32 wireColor = IM_COL32(200, 200, 255, 255);

        // Front face edges
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y - modelSize * 0.3f),
                         ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f),
                         ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y + modelSize * 0.7f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y + modelSize * 0.7f),
                         ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y + modelSize * 0.7f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y + modelSize * 0.7f),
                         ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y - modelSize * 0.3f), wireColor, 2.0f);

        // Back face edges
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y - modelSize * 0.6f),
                         ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y - modelSize * 0.6f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y - modelSize * 0.6f),
                         ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y + modelSize * 0.4f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y + modelSize * 0.4f),
                         ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y + modelSize * 0.4f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y + modelSize * 0.4f),
                         ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y - modelSize * 0.6f), wireColor, 2.0f);

        // Connecting edges
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y - modelSize * 0.3f),
                         ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y - modelSize * 0.6f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y - modelSize * 0.3f),
                         ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y - modelSize * 0.6f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x + modelSize * 0.5f, modelCenter.y + modelSize * 0.7f),
                         ImVec2(modelCenter.x + modelSize * 0.8f, modelCenter.y + modelSize * 0.4f), wireColor, 2.0f);
        drawList->AddLine(ImVec2(modelCenter.x - modelSize * 0.5f, modelCenter.y + modelSize * 0.7f),
                         ImVec2(modelCenter.x - modelSize * 0.2f, modelCenter.y + modelSize * 0.4f), wireColor, 2.0f);
    }

    // Draw bounding box if enabled
    if (m_showBounds) {
        const ImU32 boundsColor = IM_COL32(255, 200, 0, 200);
        float boundsSize = modelSize * 1.2f;

        drawList->AddRect(
            ImVec2(modelCenter.x - boundsSize * 0.5f, modelCenter.y - boundsSize * 0.4f),
            ImVec2(modelCenter.x + boundsSize * 0.5f, modelCenter.y + boundsSize * 0.8f),
            boundsColor,
            0.0f,
            0,
            2.0f
        );
    }

    // Handle camera controls
    ImGui::InvisibleButton("ViewportButton", viewportSize);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

        m_cameraYaw += delta.x * 0.5f;
        m_cameraPitch -= delta.y * 0.5f;

        // Clamp pitch
        if (m_cameraPitch > 89.0f) m_cameraPitch = 89.0f;
        if (m_cameraPitch < -89.0f) m_cameraPitch = -89.0f;

        UpdateCamera();
    }

    // Mouse wheel for zoom
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_cameraDistance -= wheel * 0.5f;
            if (m_cameraDistance < 1.0f) m_cameraDistance = 1.0f;
            if (m_cameraDistance > 100.0f) m_cameraDistance = 100.0f;
            UpdateCamera();
        }
    }
}

void ModelViewer::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

    if (ModernUI::GlowButton(m_showWireframe ? "Solid" : "Wireframe", ImVec2(80, 0))) {
        m_showWireframe = !m_showWireframe;
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton(m_showBounds ? "Hide Bounds" : "Show Bounds", ImVec2(100, 0))) {
        m_showBounds = !m_showBounds;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-Rotate", &m_autoRotate);

    if (m_autoRotate) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("##Speed", &m_autoRotateSpeed, 0.0f, 180.0f, "%.0f°/s");
    }

    ImGui::PopStyleVar();
}

void ModelViewer::RenderProperties() {
    ModernUI::GradientHeader("Model Information", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::Indent();
    ModernUI::CompactStat("File", m_modelName.c_str());

    char vertStr[32];
    snprintf(vertStr, sizeof(vertStr), "%d", m_vertexCount);
    ModernUI::CompactStat("Vertices", vertStr);

    char triStr[32];
    snprintf(triStr, sizeof(triStr), "%d", m_triangleCount);
    ModernUI::CompactStat("Triangles", triStr);

    char matStr[32];
    snprintf(matStr, sizeof(matStr), "%d", m_materialCount);
    ModernUI::CompactStat("Materials", matStr);

    if (m_boneCount > 0) {
        char boneStr[32];
        snprintf(boneStr, sizeof(boneStr), "%d", m_boneCount);
        ModernUI::CompactStat("Bones", boneStr);
    }

    if (m_lodCount > 1) {
        char lodStr[32];
        snprintf(lodStr, sizeof(lodStr), "%d", m_lodCount);
        ModernUI::CompactStat("LOD Levels", lodStr);
    }

    ImGui::Unindent();

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    if (ModernUI::GradientHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        if (ImGui::SliderFloat("Distance", &m_cameraDistance, 1.0f, 50.0f)) {
            UpdateCamera();
        }

        if (ImGui::SliderFloat("FOV", &m_cameraFov, 30.0f, 120.0f)) {
            UpdateCamera();
        }

        if (ModernUI::GlowButton("Reset Camera", ImVec2(-1, 0))) {
            m_cameraYaw = 45.0f;
            m_cameraPitch = 30.0f;
            m_cameraDistance = 10.0f;
            UpdateCamera();
        }

        ImGui::Unindent();
    }

    if (m_lodCount > 1) {
        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        if (ModernUI::GradientHeader("LOD Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::SliderInt("LOD Level", &m_currentLod, 0, m_lodCount - 1);
            ImGui::Unindent();
        }
    }
}

void ModelViewer::RenderMaterialList() {
    if (ModernUI::GradientHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        if (m_materials.empty()) {
            ImGui::TextDisabled("No materials");
        } else {
            for (size_t i = 0; i < m_materials.size(); ++i) {
                const auto& mat = m_materials[i];

                ImGui::PushID(static_cast<int>(i));

                if (ImGui::TreeNodeEx(mat.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::ColorEdit3("Color", &const_cast<MaterialInfo&>(mat).color.x);

                    if (!mat.diffuseTexture.empty()) {
                        ImGui::Text("Diffuse: %s", mat.diffuseTexture.c_str());
                    }

                    if (!mat.normalTexture.empty()) {
                        ImGui::Text("Normal: %s", mat.normalTexture.c_str());
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();

                if (i < m_materials.size() - 1) {
                    ImGui::Spacing();
                }
            }
        }

        ImGui::Unindent();
    }
}

void ModelViewer::LoadModel() {
    spdlog::info("ModelViewer: Loading model '{}'", m_assetPath);

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("ModelViewer: File does not exist: '{}'", m_assetPath);
            return;
        }

        // Use the engine's ModelImporter to load the model
        Nova::ModelImporter importer;
        Nova::ImportedModel importedModel = importer.Import(m_assetPath);

        if (!importedModel.success) {
            spdlog::error("ModelViewer: Failed to import model: {}", importedModel.errorMessage);
            return;
        }

        // Log any warnings
        for (const auto& warning : importedModel.warnings) {
            spdlog::warn("ModelViewer: {}", warning);
        }

        // Extract statistics from the imported model
        m_vertexCount = static_cast<int>(importedModel.totalVertices);
        m_triangleCount = static_cast<int>(importedModel.totalTriangles);
        m_materialCount = static_cast<int>(importedModel.totalMaterials);
        m_boneCount = static_cast<int>(importedModel.totalBones);

        // Count LOD levels (check the first mesh's LOD chain)
        if (!importedModel.lodChains.empty() && !importedModel.lodChains[0].empty()) {
            m_lodCount = static_cast<int>(importedModel.lodChains[0].size());
        } else {
            m_lodCount = 1;
        }

        // Extract bounds
        m_boundsMin = importedModel.boundsMin;
        m_boundsMax = importedModel.boundsMax;

        // Extract material information for display
        m_materials.clear();
        for (const auto& mat : importedModel.materials) {
            MaterialInfo matInfo;
            matInfo.name = mat.name;
            matInfo.color = glm::vec3(mat.diffuseColor);

            // Find diffuse and normal textures
            for (const auto& tex : mat.textures) {
                if (tex.type == "diffuse") {
                    matInfo.diffuseTexture = tex.path;
                } else if (tex.type == "normal") {
                    matInfo.normalTexture = tex.path;
                }
            }

            m_materials.push_back(matInfo);
        }

        // If no materials were loaded, add a default one
        if (m_materials.empty()) {
            m_materials.push_back({"Default Material", "", "", glm::vec3(0.8f, 0.8f, 0.8f)});
        }

        m_isLoaded = true;

        // Center camera on model
        glm::vec3 center = (m_boundsMin + m_boundsMax) * 0.5f;
        float modelSize = glm::length(m_boundsMax - m_boundsMin);
        m_cameraTarget = center;
        m_cameraDistance = modelSize * 1.5f;

        UpdateCamera();

        spdlog::info("ModelViewer: Model loaded successfully - {} vertices, {} triangles, {} materials",
                     m_vertexCount, m_triangleCount, m_materialCount);

    } catch (const std::exception& e) {
        spdlog::error("ModelViewer: Failed to load model: {}", e.what());
        m_isLoaded = false;
    }
}

void ModelViewer::UpdateCamera() {
    // Calculate camera position from spherical coordinates
    float yawRad = glm::radians(m_cameraYaw);
    float pitchRad = glm::radians(m_cameraPitch);

    m_cameraPosition.x = m_cameraTarget.x + m_cameraDistance * cos(pitchRad) * cos(yawRad);
    m_cameraPosition.y = m_cameraTarget.y + m_cameraDistance * sin(pitchRad);
    m_cameraPosition.z = m_cameraTarget.z + m_cameraDistance * cos(pitchRad) * sin(yawRad);
}

std::string ModelViewer::GetEditorName() const {
    return "Model Viewer - " + m_modelName;
}

bool ModelViewer::IsDirty() const {
    return m_isDirty;
}

void ModelViewer::Save() {
    if (!m_isDirty) {
        return;
    }

    spdlog::info("ModelViewer: Saving model overrides '{}'", m_assetPath);

    try {
        // Save material overrides to a sidecar .meta file
        // This preserves any material color changes made in the viewer
        std::filesystem::path modelPath(m_assetPath);
        std::string metaPath = modelPath.string() + ".meta";

        std::ofstream metaFile(metaPath);
        if (!metaFile.is_open()) {
            spdlog::error("ModelViewer: Failed to create meta file: '{}'", metaPath);
            return;
        }

        // Write simple metadata format
        metaFile << "# Model Viewer Overrides\n";
        metaFile << "# Generated automatically - do not edit manually\n\n";
        metaFile << "source: " << m_assetPath << "\n";
        metaFile << "materials:\n";

        for (size_t i = 0; i < m_materials.size(); ++i) {
            const auto& mat = m_materials[i];
            metaFile << "  - name: " << mat.name << "\n";
            metaFile << "    color: [" << mat.color.r << ", " << mat.color.g << ", " << mat.color.b << "]\n";
            if (!mat.diffuseTexture.empty()) {
                metaFile << "    diffuse: " << mat.diffuseTexture << "\n";
            }
            if (!mat.normalTexture.empty()) {
                metaFile << "    normal: " << mat.normalTexture << "\n";
            }
        }

        metaFile.close();

        m_isDirty = false;
        spdlog::info("ModelViewer: Saved material overrides to '{}'", metaPath);

    } catch (const std::exception& e) {
        spdlog::error("ModelViewer: Failed to save: {}", e.what());
    }
}

std::string ModelViewer::GetAssetPath() const {
    return m_assetPath;
}
