#include "SDFModelEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

// Nova engine includes
#include "../../../../engine/sdf/SDFModel.hpp"
#include "../../../../engine/sdf/SDFPrimitive.hpp"
#include "../../../../engine/sdf/SDFAnimation.hpp"
#include "../../../../engine/sdf/SDFSerializer.hpp"
#include "../../../../engine/graphics/Renderer.hpp"
#include "../../../../engine/graphics/Camera.hpp"
#include "../../../../engine/graphics/Mesh.hpp"

namespace Vehement {

SDFModelEditor::SDFModelEditor() = default;
SDFModelEditor::~SDFModelEditor() = default;

bool SDFModelEditor::Initialize(Editor* editor) {
    m_editor = editor;

    // Create default model
    m_model = std::make_unique<Nova::SDFModel>("Untitled");

    // Initialize animation systems
    m_animController = std::make_unique<Nova::SDFAnimationController>();
    m_poseLibrary = std::make_unique<Nova::SDFPoseLibrary>();
    m_currentClip = std::make_shared<Nova::SDFAnimationClip>("Default");
    m_stateMachine = std::make_unique<Nova::SDFAnimationStateMachine>();

    m_animController->SetModel(m_model.get());
    m_animController->SetPoseLibrary(m_poseLibrary);

    m_initialized = true;
    return true;
}

void SDFModelEditor::Shutdown() {
    m_model.reset();
    m_animController.reset();
    m_poseLibrary.reset();
    m_currentClip.reset();
    m_stateMachine.reset();
    m_previewMesh.reset();
    m_initialized = false;
}

void SDFModelEditor::NewModel(const std::string& name) {
    m_model = std::make_unique<Nova::SDFModel>(name);
    m_animController->SetModel(m_model.get());
    m_selectedPrimitive = nullptr;
    m_currentFilePath.clear();
    m_dirty = false;
    m_needsMeshUpdate = true;

    if (OnModelChanged) OnModelChanged();
}

bool SDFModelEditor::LoadModel(const std::string& path) {
    auto model = Nova::SDFSerializer::LoadModel(path);
    if (!model) return false;

    m_model = std::move(model);
    m_animController->SetModel(m_model.get());
    m_selectedPrimitive = nullptr;
    m_currentFilePath = path;
    m_dirty = false;
    m_needsMeshUpdate = true;

    if (OnModelChanged) OnModelChanged();
    return true;
}

bool SDFModelEditor::SaveModel(const std::string& path) {
    if (!m_model) return false;

    if (Nova::SDFSerializer::SaveModel(*m_model, path)) {
        m_currentFilePath = path;
        m_dirty = false;
        return true;
    }
    return false;
}

void SDFModelEditor::SetModel(std::unique_ptr<Nova::SDFModel> model) {
    m_model = std::move(model);
    m_animController->SetModel(m_model.get());
    m_selectedPrimitive = nullptr;
    m_needsMeshUpdate = true;

    if (OnModelChanged) OnModelChanged();
}

void SDFModelEditor::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update animation
    if (m_isPlaying) {
        m_animationTime += deltaTime * m_animationSpeed;

        if (m_currentClip && m_currentClip->GetDuration() > 0) {
            if (m_currentClip->IsLooping()) {
                m_animationTime = std::fmod(m_animationTime, m_currentClip->GetDuration());
            } else {
                m_animationTime = std::min(m_animationTime, m_currentClip->GetDuration());
            }
        }

        // Apply animation to model
        if (m_currentClip && m_model) {
            m_currentClip->ApplyToModel(*m_model, m_animationTime);
            m_needsMeshUpdate = true;
        }

        if (OnAnimationTimeChanged) OnAnimationTimeChanged(m_animationTime);
    }

    // Update mesh preview if needed
    if (m_needsMeshUpdate) {
        UpdateMeshPreview();
    }
}

void SDFModelEditor::RenderUI() {
    if (!m_initialized) return;

    // Main menu bar items
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("SDF Editor")) {
            if (ImGui::MenuItem("New Model", "Ctrl+N")) {
                NewModel();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // File dialog would go here
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, !m_currentFilePath.empty())) {
                SaveModel(m_currentFilePath);
            }
            if (ImGui::MenuItem("Save As...")) {
                // File dialog would go here
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export to Entity JSON...")) {
                // Export dialog
            }
            if (ImGui::MenuItem("Export Mesh as OBJ...")) {
                // Export mesh dialog
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Timeline", nullptr, &m_showTimeline);
            ImGui::MenuItem("Pose Library", nullptr, &m_showPoseLibrary);
            ImGui::MenuItem("Mesh Settings", nullptr, &m_showMeshSettings);
            ImGui::MenuItem("Paint Panel", nullptr, &m_showPaintPanel);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Render panels
    RenderToolbarPanel();

    if (m_showHierarchy) RenderHierarchyPanel();
    if (m_showInspector) RenderInspectorPanel();
    if (m_showTimeline) RenderTimelinePanel();
    if (m_showPoseLibrary) RenderPoseLibraryPanel();
    if (m_showMeshSettings) RenderMeshSettingsPanel();
    if (m_showPaintPanel) RenderPaintPanel();
    if (m_showCreateDialog) RenderPrimitiveCreator();
}

void SDFModelEditor::RenderToolbarPanel() {
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 50), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("SDF Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        // Tool mode buttons
        bool selectMode = m_toolMode == SDFToolMode::Select;
        bool createMode = m_toolMode == SDFToolMode::Create;
        bool paintMode = m_toolMode == SDFToolMode::Paint;

        if (ImGui::Selectable("Select", selectMode, 0, ImVec2(60, 30))) {
            m_toolMode = SDFToolMode::Select;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Create", createMode, 0, ImVec2(60, 30))) {
            m_toolMode = SDFToolMode::Create;
            m_showCreateDialog = true;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Paint", paintMode, 0, ImVec2(60, 30))) {
            m_toolMode = SDFToolMode::Paint;
            m_showPaintPanel = true;
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Gizmo mode buttons
        bool translateMode = m_gizmoMode == SDFGizmoMode::Translate;
        bool rotateMode = m_gizmoMode == SDFGizmoMode::Rotate;
        bool scaleMode = m_gizmoMode == SDFGizmoMode::Scale;

        if (ImGui::Selectable("Move", translateMode, 0, ImVec2(50, 30))) {
            m_gizmoMode = SDFGizmoMode::Translate;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Rotate", rotateMode, 0, ImVec2(50, 30))) {
            m_gizmoMode = SDFGizmoMode::Rotate;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Scale", scaleMode, 0, ImVec2(50, 30))) {
            m_gizmoMode = SDFGizmoMode::Scale;
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderHierarchyPanel() {
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("SDF Hierarchy", &m_showHierarchy)) {
        if (ImGui::Button("Add Primitive")) {
            m_showCreateDialog = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete") && m_selectedPrimitive) {
            DeleteSelected();
        }

        ImGui::Separator();

        if (m_model && m_model->GetRoot()) {
            RenderPrimitiveNode(m_model->GetRoot());
        } else {
            ImGui::TextDisabled("No primitives");
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderPrimitiveNode(Nova::SDFPrimitive* primitive) {
    if (!primitive) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (primitive == m_selectedPrimitive) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (primitive->GetChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    std::string label = primitive->GetName();
    if (label.empty()) {
        label = "Unnamed_" + std::to_string(primitive->GetId());
    }

    // Add type indicator
    label += " [" + std::string(Nova::SDFSerializer::PrimitiveTypeToString(primitive->GetType())) + "]";

    bool isOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(primitive->GetId())),
                                    flags, "%s", label.c_str());

    // Selection
    if (ImGui::IsItemClicked()) {
        SelectPrimitive(primitive);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Child")) {
            AddPrimitive(Nova::SDFPrimitiveType::Sphere, primitive);
        }
        if (ImGui::MenuItem("Duplicate")) {
            if (m_selectedPrimitive == primitive) {
                DuplicateSelected();
            }
        }
        if (ImGui::MenuItem("Delete")) {
            if (m_selectedPrimitive == primitive) {
                DeleteSelected();
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Set as Root")) {
            // Reparent logic
        }
        ImGui::EndPopup();
    }

    // Drag and drop for reordering
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("SDF_PRIMITIVE", &primitive, sizeof(Nova::SDFPrimitive*));
        ImGui::Text("Move: %s", primitive->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SDF_PRIMITIVE")) {
            Nova::SDFPrimitive* droppedPrim = *static_cast<Nova::SDFPrimitive**>(payload->Data);
            // Reparent logic would go here
            (void)droppedPrim;
            MarkDirty();
        }
        ImGui::EndDragDropTarget();
    }

    if (isOpen) {
        for (auto& child : primitive->GetChildren()) {
            RenderPrimitiveNode(child.get());
        }
        ImGui::TreePop();
    }
}

void SDFModelEditor::RenderInspectorPanel() {
    ImGui::SetNextWindowPos(ImVec2(800, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("SDF Inspector", &m_showInspector)) {
        if (!m_selectedPrimitive) {
            ImGui::TextDisabled("No primitive selected");
            ImGui::End();
            return;
        }

        // Name
        char nameBuffer[256];
        strncpy(nameBuffer, m_selectedPrimitive->GetName().c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            m_selectedPrimitive->SetName(nameBuffer);
            MarkDirty();
        }

        // Type
        const char* typeNames[] = {"Sphere", "Box", "Cylinder", "Capsule", "Cone", "Torus",
                                   "Plane", "RoundedBox", "Ellipsoid", "Pyramid", "Prism", "Custom"};
        int currentType = static_cast<int>(m_selectedPrimitive->GetType());
        if (ImGui::Combo("Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
            m_selectedPrimitive->SetType(static_cast<Nova::SDFPrimitiveType>(currentType));
            MarkDirty();
            m_needsMeshUpdate = true;
        }

        // CSG Operation
        const char* csgNames[] = {"Union", "Subtraction", "Intersection",
                                  "Smooth Union", "Smooth Subtraction", "Smooth Intersection"};
        int currentCSG = static_cast<int>(m_selectedPrimitive->GetCSGOperation());
        if (ImGui::Combo("CSG Operation", &currentCSG, csgNames, IM_ARRAYSIZE(csgNames))) {
            m_selectedPrimitive->SetCSGOperation(static_cast<Nova::CSGOperation>(currentCSG));
            MarkDirty();
            m_needsMeshUpdate = true;
        }

        ImGui::Separator();

        // Transform
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            Nova::SDFTransform transform = m_selectedPrimitive->GetLocalTransform();
            bool changed = false;

            if (ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.01f)) {
                changed = true;
            }

            glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(transform.rotation));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerDeg), 1.0f)) {
                transform.rotation = glm::quat(glm::radians(eulerDeg));
                changed = true;
            }

            if (ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.01f, 0.01f, 100.0f)) {
                changed = true;
            }

            if (changed) {
                m_selectedPrimitive->SetLocalTransform(transform);
                MarkDirty();
                m_needsMeshUpdate = true;
            }
        }

        // Parameters
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            Nova::SDFParameters params = m_selectedPrimitive->GetParameters();
            bool changed = false;

            Nova::SDFPrimitiveType type = m_selectedPrimitive->GetType();

            // Show relevant parameters based on type
            if (type == Nova::SDFPrimitiveType::Sphere) {
                changed |= ImGui::DragFloat("Radius", &params.radius, 0.01f, 0.01f, 10.0f);
            }
            else if (type == Nova::SDFPrimitiveType::Box || type == Nova::SDFPrimitiveType::RoundedBox) {
                changed |= ImGui::DragFloat3("Dimensions", glm::value_ptr(params.dimensions), 0.01f, 0.01f, 10.0f);
                if (type == Nova::SDFPrimitiveType::RoundedBox) {
                    changed |= ImGui::DragFloat("Corner Radius", &params.cornerRadius, 0.01f, 0.0f, 1.0f);
                }
            }
            else if (type == Nova::SDFPrimitiveType::Cylinder || type == Nova::SDFPrimitiveType::Capsule) {
                changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 10.0f);
                changed |= ImGui::DragFloat("Radius", &params.bottomRadius, 0.01f, 0.01f, 10.0f);
            }
            else if (type == Nova::SDFPrimitiveType::Cone) {
                changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 10.0f);
                changed |= ImGui::DragFloat("Base Radius", &params.bottomRadius, 0.01f, 0.01f, 10.0f);
            }
            else if (type == Nova::SDFPrimitiveType::Torus) {
                changed |= ImGui::DragFloat("Major Radius", &params.majorRadius, 0.01f, 0.01f, 10.0f);
                changed |= ImGui::DragFloat("Minor Radius", &params.minorRadius, 0.01f, 0.01f, 10.0f);
            }
            else if (type == Nova::SDFPrimitiveType::Ellipsoid) {
                changed |= ImGui::DragFloat3("Radii", glm::value_ptr(params.radii), 0.01f, 0.01f, 10.0f);
            }
            else if (type == Nova::SDFPrimitiveType::Prism) {
                changed |= ImGui::DragInt("Sides", &params.sides, 1, 3, 12);
                changed |= ImGui::DragFloat("Radius", &params.bottomRadius, 0.01f, 0.01f, 10.0f);
                changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 10.0f);
            }

            // Smoothness for smooth CSG operations
            Nova::CSGOperation csg = m_selectedPrimitive->GetCSGOperation();
            if (csg == Nova::CSGOperation::SmoothUnion ||
                csg == Nova::CSGOperation::SmoothSubtraction ||
                csg == Nova::CSGOperation::SmoothIntersection) {
                changed |= ImGui::DragFloat("Smoothness", &params.smoothness, 0.01f, 0.0f, 1.0f);
            }

            if (changed) {
                m_selectedPrimitive->SetParameters(params);
                MarkDirty();
                m_needsMeshUpdate = true;
            }
        }

        // Material
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            Nova::SDFMaterial material = m_selectedPrimitive->GetMaterial();
            bool changed = false;

            changed |= ImGui::ColorEdit4("Base Color", glm::value_ptr(material.baseColor));
            changed |= ImGui::DragFloat("Metallic", &material.metallic, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Roughness", &material.roughness, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Emissive", &material.emissive, 0.01f, 0.0f, 10.0f);

            if (material.emissive > 0) {
                changed |= ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.emissiveColor));
            }

            if (changed) {
                m_selectedPrimitive->SetMaterial(material);
                MarkDirty();
            }
        }

        // Visibility
        ImGui::Separator();
        bool visible = m_selectedPrimitive->IsVisible();
        if (ImGui::Checkbox("Visible", &visible)) {
            m_selectedPrimitive->SetVisible(visible);
            MarkDirty();
            m_needsMeshUpdate = true;
        }

        bool locked = m_selectedPrimitive->IsLocked();
        if (ImGui::Checkbox("Locked", &locked)) {
            m_selectedPrimitive->SetLocked(locked);
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderTimelinePanel() {
    ImGui::SetNextWindowPos(ImVec2(270, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Animation Timeline", &m_showTimeline)) {
        // Playback controls
        if (ImGui::Button(m_isPlaying ? "||" : ">")) {
            if (m_isPlaying) PauseAnimation();
            else PlayAnimation();
        }
        ImGui::SameLine();
        if (ImGui::Button("[]")) {
            StopAnimation();
        }
        ImGui::SameLine();
        if (ImGui::Button(m_isRecording ? "Stop Rec" : "Record")) {
            if (m_isRecording) StopRecording();
            else StartRecording();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Speed", &m_animationSpeed, 0.1f, 0.1f, 5.0f);

        // Time slider
        float duration = m_currentClip ? m_currentClip->GetDuration() : 1.0f;
        if (ImGui::SliderFloat("Time", &m_animationTime, 0.0f, duration, "%.2f s")) {
            if (m_currentClip && m_model) {
                m_currentClip->ApplyToModel(*m_model, m_animationTime);
                m_needsMeshUpdate = true;
            }
            if (OnAnimationTimeChanged) OnAnimationTimeChanged(m_animationTime);
        }

        ImGui::SameLine();
        if (ImGui::Button("+Key")) {
            AddKeyframe();
        }

        // Duration setting
        if (m_currentClip) {
            float clipDuration = m_currentClip->GetDuration();
            if (ImGui::DragFloat("Duration", &clipDuration, 0.1f, 0.1f, 60.0f)) {
                m_currentClip->SetDuration(clipDuration);
            }
        }

        ImGui::Separator();

        // Keyframe display (simplified)
        if (m_currentClip) {
            ImGui::Text("Keyframes: %zu", m_currentClip->GetKeyframeCount());

            const auto& keyframes = m_currentClip->GetKeyframes();
            for (size_t i = 0; i < keyframes.size(); ++i) {
                ImGui::SameLine();
                char label[32];
                snprintf(label, sizeof(label), "%.1fs", keyframes[i].time);
                if (ImGui::SmallButton(label)) {
                    m_animationTime = keyframes[i].time;
                    if (m_model) {
                        m_currentClip->ApplyToModel(*m_model, m_animationTime);
                        m_needsMeshUpdate = true;
                    }
                }
            }
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderPoseLibraryPanel() {
    ImGui::SetNextWindowPos(ImVec2(870, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Pose Library", &m_showPoseLibrary)) {
        static char poseName[64] = "NewPose";
        ImGui::InputText("Name", poseName, sizeof(poseName));
        ImGui::SameLine();
        if (ImGui::Button("Save Pose")) {
            SaveCurrentPose(poseName);
        }

        ImGui::Separator();

        // List poses
        if (m_poseLibrary) {
            const auto& poses = m_poseLibrary->GetAllPoses();
            for (const auto& pose : poses) {
                if (ImGui::Selectable(pose.name.c_str())) {
                    ApplyPose(pose.name);
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Apply")) {
                        ApplyPose(pose.name);
                    }
                    if (ImGui::MenuItem("Delete")) {
                        m_poseLibrary->DeletePose(pose.name);
                    }
                    if (ImGui::MenuItem("Add to Animation")) {
                        if (m_currentClip) {
                            Nova::SDFPose p;
                            p.transforms = m_poseLibrary->BlendPoses(pose.name, pose.name, 0.0f);
                            m_currentClip->AddKeyframeFromPose(m_animationTime, p);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderPrimitiveCreator() {
    ImGui::SetNextWindowPos(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Create Primitive", &m_showCreateDialog)) {
        ImGui::InputText("Name", m_createName, sizeof(m_createName));

        const char* typeNames[] = {"Sphere", "Box", "Cylinder", "Capsule", "Cone", "Torus",
                                   "Plane", "RoundedBox", "Ellipsoid", "Pyramid", "Prism"};
        int typeIndex = static_cast<int>(m_createType);
        if (ImGui::Combo("Type", &typeIndex, typeNames, IM_ARRAYSIZE(typeNames))) {
            m_createType = static_cast<Nova::SDFPrimitiveType>(typeIndex);
        }

        ImGui::Separator();

        if (ImGui::Button("Create")) {
            Nova::SDFPrimitive* parent = m_selectedPrimitive;
            auto* prim = AddPrimitive(m_createType, parent);
            if (prim) {
                prim->SetName(m_createName);
                SelectPrimitive(prim);
            }
            m_showCreateDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_showCreateDialog = false;
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderMeshSettingsPanel() {
    ImGui::SetNextWindowPos(ImVec2(550, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Mesh Settings", &m_showMeshSettings)) {
        if (m_model) {
            Nova::SDFMeshSettings settings = m_model->GetMeshSettings();
            bool changed = false;

            changed |= ImGui::SliderInt("Resolution", &settings.resolution, 16, 128);
            changed |= ImGui::DragFloat("Bounds Padding", &settings.boundsPadding, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::Checkbox("Smooth Normals", &settings.smoothNormals);
            changed |= ImGui::Checkbox("Generate UVs", &settings.generateUVs);

            if (changed) {
                m_model->SetMeshSettings(settings);
                m_needsMeshUpdate = true;
            }

            ImGui::Separator();

            if (ImGui::Button("Regenerate Mesh")) {
                m_needsMeshUpdate = true;
            }

            // Mesh stats
            if (m_previewMesh) {
                ImGui::Text("Vertices: %zu", m_previewMesh->GetVertexCount());
                ImGui::Text("Triangles: %zu", m_previewMesh->GetIndexCount() / 3);
            }
        }
    }
    ImGui::End();
}

void SDFModelEditor::RenderPaintPanel() {
    ImGui::SetNextWindowPos(ImVec2(550, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Paint Settings", &m_showPaintPanel)) {
        ImGui::ColorEdit4("Color", glm::value_ptr(m_brushSettings.color));
        ImGui::DragFloat("Radius", &m_brushSettings.radius, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Hardness", &m_brushSettings.hardness, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Opacity", &m_brushSettings.opacity, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();

        // Paint layers
        if (m_model) {
            const auto& layers = m_model->GetPaintLayers();
            ImGui::Text("Layers:");
            for (const auto& layer : layers) {
                bool selected = (layer.name == m_brushSettings.currentLayer);
                if (ImGui::Selectable(layer.name.c_str(), selected)) {
                    m_brushSettings.currentLayer = layer.name;
                }
            }

            if (ImGui::Button("Add Layer")) {
                static int layerCount = 0;
                std::string layerName = "Layer_" + std::to_string(++layerCount);
                m_model->AddPaintLayer(layerName);
                m_brushSettings.currentLayer = layerName;
            }
        }
    }
    ImGui::End();
}

void SDFModelEditor::Render3D(Nova::Renderer& renderer, Nova::Camera& camera) {
    if (!m_initialized || !m_model) return;

    // Render preview mesh
    if (m_previewMesh) {
        // renderer.DrawMesh(*m_previewMesh, material, transform);
    }

    // Render gizmo for selected primitive
    if (m_selectedPrimitive && m_gizmoMode != SDFGizmoMode::None) {
        RenderGizmo(renderer, camera);
    }
}

void SDFModelEditor::RenderGizmo(Nova::Renderer& renderer, Nova::Camera& camera) {
    (void)renderer;
    (void)camera;
    // Gizmo rendering implementation would use line primitives
    // This is a placeholder - actual implementation would draw axis handles
}

void SDFModelEditor::ProcessInput() {
    if (!m_initialized) return;

    HandleKeyboardInput();
}

void SDFModelEditor::HandleKeyboardInput() {
    // Keyboard shortcuts
    // These would integrate with ImGui key state checking
}

void SDFModelEditor::SelectPrimitive(Nova::SDFPrimitive* primitive) {
    m_selectedPrimitive = primitive;
    if (OnPrimitiveSelected) OnPrimitiveSelected(primitive);
}

void SDFModelEditor::ClearSelection() {
    m_selectedPrimitive = nullptr;
    if (OnPrimitiveSelected) OnPrimitiveSelected(nullptr);
}

Nova::SDFPrimitive* SDFModelEditor::AddPrimitive(Nova::SDFPrimitiveType type, Nova::SDFPrimitive* parent) {
    if (!m_model) return nullptr;

    std::string name = "Primitive_" + std::to_string(m_model->GetPrimitiveCount() + 1);
    Nova::SDFPrimitive* prim = m_model->CreatePrimitive(name, type, parent);

    MarkDirty();
    m_needsMeshUpdate = true;

    return prim;
}

Nova::SDFPrimitive* SDFModelEditor::DuplicateSelected() {
    if (!m_selectedPrimitive || !m_model) return nullptr;

    auto clone = m_selectedPrimitive->Clone();
    clone->SetName(clone->GetName() + "_copy");

    Nova::SDFPrimitive* parent = m_selectedPrimitive->GetParent();
    Nova::SDFPrimitive* result = nullptr;

    if (parent) {
        result = parent->AddChild(std::move(clone));
    } else {
        // Add as sibling to root
        result = m_model->GetRoot()->AddChild(std::move(clone));
    }

    MarkDirty();
    m_needsMeshUpdate = true;

    return result;
}

void SDFModelEditor::DeleteSelected() {
    if (!m_selectedPrimitive || !m_model) return;

    m_model->DeletePrimitive(m_selectedPrimitive);
    m_selectedPrimitive = nullptr;

    MarkDirty();
    m_needsMeshUpdate = true;

    if (OnPrimitiveSelected) OnPrimitiveSelected(nullptr);
}

void SDFModelEditor::SaveCurrentPose(const std::string& name, const std::string& category) {
    if (!m_model || !m_poseLibrary) return;

    m_poseLibrary->SavePoseFromModel(name, *m_model, category);

    if (OnPoseSaved) OnPoseSaved(name);
}

void SDFModelEditor::ApplyPose(const std::string& name) {
    if (!m_model || !m_poseLibrary) return;

    const Nova::SDFPose* pose = m_poseLibrary->GetPose(name);
    if (pose) {
        m_model->ApplyPose(pose->transforms);
        m_needsMeshUpdate = true;
    }
}

void SDFModelEditor::StartRecording() {
    m_isRecording = true;
}

void SDFModelEditor::StopRecording() {
    m_isRecording = false;
}

void SDFModelEditor::AddKeyframe() {
    if (!m_currentClip || !m_model) return;

    Nova::SDFPose pose;
    pose.transforms = m_model->GetCurrentPose();
    m_currentClip->AddKeyframeFromPose(m_animationTime, pose);

    MarkDirty();
}

void SDFModelEditor::SetAnimationTime(float time) {
    m_animationTime = time;
    if (m_currentClip && m_model) {
        m_currentClip->ApplyToModel(*m_model, time);
        m_needsMeshUpdate = true;
    }
}

void SDFModelEditor::PlayAnimation() {
    m_isPlaying = true;
}

void SDFModelEditor::PauseAnimation() {
    m_isPlaying = false;
}

void SDFModelEditor::StopAnimation() {
    m_isPlaying = false;
    m_animationTime = 0.0f;
    if (m_currentClip && m_model) {
        m_currentClip->ApplyToModel(*m_model, 0.0f);
        m_needsMeshUpdate = true;
    }
}

void SDFModelEditor::UpdateMeshPreview() {
    if (!m_model) return;

    // Use lower resolution for preview
    Nova::SDFMeshSettings previewSettings = m_model->GetMeshSettings();
    previewSettings.resolution = std::min(previewSettings.resolution, m_meshResolution);

    m_previewMesh = m_model->GenerateMesh(previewSettings);
    m_needsMeshUpdate = false;
}

bool SDFModelEditor::ExportToEntityJson(const std::string& jsonPath) {
    if (!m_model) return false;

    std::vector<const Nova::SDFAnimationClip*> clips;
    if (m_currentClip) {
        clips.push_back(m_currentClip.get());
    }

    return Nova::SDFSerializer::UpdateEntityJson(
        jsonPath,
        *m_model,
        m_poseLibrary.get(),
        clips,
        m_stateMachine.get()
    );
}

bool SDFModelEditor::ImportFromEntityJson(const std::string& jsonPath) {
    auto data = Nova::SDFSerializer::LoadEntitySDF(jsonPath);

    if (data.model) {
        SetModel(std::move(data.model));
    }

    if (data.poseLibrary) {
        m_poseLibrary = std::move(data.poseLibrary);
    }

    if (!data.animations.empty()) {
        m_currentClip = std::move(data.animations[0]);
    }

    if (data.stateMachine) {
        m_stateMachine = std::move(data.stateMachine);
    }

    return true;
}

} // namespace Vehement
