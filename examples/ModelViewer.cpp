#include "ModelViewer.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

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
                if (ImGui::MenuItem("Export...")) {
                    // TODO: Export dialog
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

    // TODO: Actual model loading using engine's model system
    // For now, simulate loading

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("ModelViewer: File does not exist: '{}'", m_assetPath);
            return;
        }

        // Simulate model properties
        m_vertexCount = 8426;
        m_triangleCount = 5248;
        m_materialCount = 3;
        m_boneCount = 0;
        m_lodCount = 1;

        // Simulate materials
        m_materials.clear();
        m_materials.push_back({"Material_Body", "body_diffuse.png", "body_normal.png", glm::vec3(0.8f, 0.8f, 0.8f)});
        m_materials.push_back({"Material_Metal", "metal_diffuse.png", "", glm::vec3(0.7f, 0.7f, 0.8f)});
        m_materials.push_back({"Material_Glass", "", "", glm::vec3(0.9f, 0.95f, 1.0f)});

        m_isLoaded = true;
        UpdateCamera();

        spdlog::info("ModelViewer: Model loaded successfully");

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

    spdlog::info("ModelViewer: Saving model '{}'", m_assetPath);

    // TODO: Actual save implementation (if model was modified)

    m_isDirty = false;
}

std::string ModelViewer::GetAssetPath() const {
    return m_assetPath;
}
