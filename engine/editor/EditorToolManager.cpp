/**
 * @file EditorToolManager.cpp
 * @brief Implementation of tool tracking and management
 */

#include "EditorToolManager.hpp"
#include "TransformGizmo.hpp"
#include "../scene/SceneNode.hpp"
#include "../ui/EditorTheme.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace Nova {

// =============================================================================
// Utility Functions
// =============================================================================

const char* GetTransformToolName(TransformTool tool) {
    switch (tool) {
        case TransformTool::Select:    return "Select";
        case TransformTool::Translate: return "Translate";
        case TransformTool::Rotate:    return "Rotate";
        case TransformTool::Scale:     return "Scale";
        default:                       return "Unknown";
    }
}

const char* GetTransformToolIcon(TransformTool tool) {
    switch (tool) {
        case TransformTool::Select:    return "Q";
        case TransformTool::Translate: return "W";
        case TransformTool::Rotate:    return "E";
        case TransformTool::Scale:     return "R";
        default:                       return "?";
    }
}

const char* GetTransformSpaceName(TransformSpace space) {
    switch (space) {
        case TransformSpace::World: return "World";
        case TransformSpace::Local: return "Local";
        default:                    return "Unknown";
    }
}

// =============================================================================
// EditorToolManager - Static Methods
// =============================================================================

const char* EditorToolManager::GetToolName(TransformTool tool) {
    return GetTransformToolName(tool);
}

const char* EditorToolManager::GetToolIcon(TransformTool tool) {
    return GetTransformToolIcon(tool);
}

const char* EditorToolManager::GetSpaceName(TransformSpace space) {
    return GetTransformSpaceName(space);
}

// =============================================================================
// Initialization
// =============================================================================

bool EditorToolManager::Initialize() {
    if (m_initialized) {
        spdlog::warn("EditorToolManager already initialized");
        return true;
    }

    // Create transform gizmo
    m_gizmo = std::make_unique<TransformGizmo>();
    if (!m_gizmo->Initialize()) {
        spdlog::warn("Failed to initialize transform gizmo");
        // Continue anyway - gizmo is optional
    }

    // Apply default settings
    m_activeTool = m_settings.defaultTool;
    m_activeSpace = m_settings.defaultSpace;

    SyncGizmoSettings();

    m_initialized = true;
    spdlog::debug("EditorToolManager initialized");
    return true;
}

void EditorToolManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_gizmo) {
        m_gizmo->Shutdown();
        m_gizmo.reset();
    }

    m_onToolChanged = nullptr;
    m_onSpaceChanged = nullptr;
    m_onSnappingChanged = nullptr;

    m_initialized = false;
    spdlog::debug("EditorToolManager shutdown");
}

// =============================================================================
// Tool Selection
// =============================================================================

void EditorToolManager::SetTool(TransformTool tool) {
    if (tool == m_activeTool) {
        return;
    }

    TransformTool previous = m_activeTool;
    m_activeTool = tool;

    // Update gizmo mode
    if (m_gizmo) {
        switch (tool) {
            case TransformTool::Select:
                m_gizmo->SetVisible(false);
                break;
            case TransformTool::Translate:
                m_gizmo->SetMode(GizmoMode::Translate);
                m_gizmo->SetVisible(m_gizmoVisible);
                break;
            case TransformTool::Rotate:
                m_gizmo->SetMode(GizmoMode::Rotate);
                m_gizmo->SetVisible(m_gizmoVisible);
                break;
            case TransformTool::Scale:
                m_gizmo->SetMode(GizmoMode::Scale);
                m_gizmo->SetVisible(m_gizmoVisible);
                break;
        }
    }

    NotifyToolChanged(previous);
}

void EditorToolManager::NextTool() {
    int current = static_cast<int>(m_activeTool);
    int next = (current + 1) % 4;
    SetTool(static_cast<TransformTool>(next));
}

void EditorToolManager::PreviousTool() {
    int current = static_cast<int>(m_activeTool);
    int prev = (current - 1 + 4) % 4;
    SetTool(static_cast<TransformTool>(prev));
}

// =============================================================================
// Transform Space
// =============================================================================

void EditorToolManager::SetSpace(TransformSpace space) {
    if (space == m_activeSpace) {
        return;
    }

    TransformSpace previous = m_activeSpace;
    m_activeSpace = space;

    // Update gizmo space
    if (m_gizmo) {
        m_gizmo->SetSpace(space == TransformSpace::World ? GizmoSpace::World : GizmoSpace::Local);
    }

    NotifySpaceChanged(previous);
}

void EditorToolManager::ToggleSpace() {
    SetSpace(m_activeSpace == TransformSpace::World ? TransformSpace::Local : TransformSpace::World);
}

// =============================================================================
// Snapping
// =============================================================================

void EditorToolManager::SetSnappingEnabled(bool enabled) {
    if (enabled == m_settings.snapping.enabled) {
        return;
    }

    m_settings.snapping.enabled = enabled;
    SyncGizmoSettings();

    if (m_onSnappingChanged) {
        m_onSnappingChanged(enabled);
    }
}

void EditorToolManager::ToggleSnapping() {
    SetSnappingEnabled(!m_settings.snapping.enabled);
}

// =============================================================================
// Gizmo Integration
// =============================================================================

bool EditorToolManager::IsGizmoActive() const {
    return m_gizmo && m_gizmo->IsActive();
}

void EditorToolManager::CancelGizmoManipulation() {
    if (m_gizmo && m_gizmo->IsActive()) {
        m_gizmo->CancelManipulation();
    }
}

void EditorToolManager::UpdateGizmo(const std::vector<SceneNode*>& selectedNodes) {
    if (!m_gizmo) {
        return;
    }

    if (selectedNodes.empty() || m_activeTool == TransformTool::Select) {
        m_gizmo->SetVisible(false);
        return;
    }

    // Calculate pivot position
    glm::vec3 pivotPosition{0.0f};
    glm::quat pivotRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    if (m_settings.usePivotCenter) {
        // Use center of selection
        for (auto* node : selectedNodes) {
            pivotPosition += node->GetWorldPosition();
        }
        pivotPosition /= static_cast<float>(selectedNodes.size());

        // Use primary selection's rotation
        if (!selectedNodes.empty()) {
            pivotRotation = selectedNodes.back()->GetWorldRotation();
        }
    } else {
        // Use primary selection's transform
        if (!selectedNodes.empty()) {
            auto* primary = selectedNodes.back();
            pivotPosition = primary->GetWorldPosition();
            pivotRotation = primary->GetWorldRotation();
        }
    }

    m_gizmo->SetTransform(pivotPosition, pivotRotation);
    m_gizmo->SetVisible(m_gizmoVisible && m_activeTool != TransformTool::Select);
}

void EditorToolManager::SetGizmoVisible(bool visible) {
    m_gizmoVisible = visible;
    if (m_gizmo && m_activeTool != TransformTool::Select) {
        m_gizmo->SetVisible(visible);
    }
}

// =============================================================================
// Settings
// =============================================================================

bool EditorToolManager::LoadSettings(const std::string& path) {
    // TODO: Implement settings loading from JSON/INI file
    spdlog::debug("Loading tool settings from: {}", path);
    return true;
}

bool EditorToolManager::SaveSettings(const std::string& path) {
    // TODO: Implement settings saving to JSON/INI file
    spdlog::debug("Saving tool settings to: {}", path);
    return true;
}

void EditorToolManager::ResetSettings() {
    m_settings = ToolSettings{};
    m_activeTool = m_settings.defaultTool;
    m_activeSpace = m_settings.defaultSpace;
    SyncGizmoSettings();
}

// =============================================================================
// Callbacks
// =============================================================================

void EditorToolManager::SetOnToolChanged(std::function<void(const ToolChangedEvent&)> callback) {
    m_onToolChanged = std::move(callback);
}

void EditorToolManager::SetOnSpaceChanged(std::function<void(const SpaceChangedEvent&)> callback) {
    m_onSpaceChanged = std::move(callback);
}

void EditorToolManager::SetOnSnappingChanged(std::function<void(bool)> callback) {
    m_onSnappingChanged = std::move(callback);
}

// =============================================================================
// Update
// =============================================================================

void EditorToolManager::Update(float deltaTime, const std::vector<SceneNode*>& selectedNodes) {
    (void)deltaTime;  // Currently unused

    UpdateGizmo(selectedNodes);
}

void EditorToolManager::RenderToolbar() {
    RenderTransformToolButtons();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    RenderSnappingControls();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    RenderSpaceToggle();
}

// =============================================================================
// Utility
// =============================================================================

float EditorToolManager::SnapValue(float value, float snap) const {
    if (!m_settings.snapping.enabled || snap <= 0.0f) {
        return value;
    }
    return std::round(value / snap) * snap;
}

glm::vec3 EditorToolManager::SnapPosition(const glm::vec3& position) const {
    if (!m_settings.snapping.enabled) {
        return position;
    }

    float snap = m_settings.snapping.translateSnap;
    return glm::vec3(
        SnapValue(position.x, snap),
        SnapValue(position.y, snap),
        SnapValue(position.z, snap)
    );
}

glm::vec3 EditorToolManager::SnapRotation(const glm::vec3& rotation) const {
    if (!m_settings.snapping.enabled) {
        return rotation;
    }

    float snap = m_settings.snapping.rotateSnap;
    return glm::vec3(
        SnapValue(rotation.x, snap),
        SnapValue(rotation.y, snap),
        SnapValue(rotation.z, snap)
    );
}

glm::vec3 EditorToolManager::SnapScale(const glm::vec3& scale) const {
    if (!m_settings.snapping.enabled) {
        return scale;
    }

    float snap = m_settings.snapping.scaleSnap;
    return glm::vec3(
        SnapValue(scale.x, snap),
        SnapValue(scale.y, snap),
        SnapValue(scale.z, snap)
    );
}

// =============================================================================
// Internal Helpers
// =============================================================================

void EditorToolManager::SyncGizmoSettings() {
    if (!m_gizmo) {
        return;
    }

    GizmoSnapping snapping;
    snapping.enabled = m_settings.snapping.enabled;
    snapping.translateSnap = m_settings.snapping.translateSnap;
    snapping.rotateSnap = m_settings.snapping.rotateSnap;
    snapping.scaleSnap = m_settings.snapping.scaleSnap;
    m_gizmo->SetSnapping(snapping);
}

void EditorToolManager::NotifyToolChanged(TransformTool previous) {
    if (m_onToolChanged) {
        ToolChangedEvent event;
        event.previousTool = previous;
        event.newTool = m_activeTool;
        m_onToolChanged(event);
    }
}

void EditorToolManager::NotifySpaceChanged(TransformSpace previous) {
    if (m_onSpaceChanged) {
        SpaceChangedEvent event;
        event.previousSpace = previous;
        event.newSpace = m_activeSpace;
        m_onSpaceChanged(event);
    }
}

void EditorToolManager::RenderTransformToolButtons() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    // Select tool
    bool isSelect = (m_activeTool == TransformTool::Select);
    if (isSelect) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("Q", ImVec2(buttonSize, buttonSize))) {
        SetTool(TransformTool::Select);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select (Q)");
    }
    if (isSelect) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Translate tool
    bool isTranslate = (m_activeTool == TransformTool::Translate);
    if (isTranslate) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("W", ImVec2(buttonSize, buttonSize))) {
        SetTool(TransformTool::Translate);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Translate (W)");
    }
    if (isTranslate) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Rotate tool
    bool isRotate = (m_activeTool == TransformTool::Rotate);
    if (isRotate) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("E", ImVec2(buttonSize, buttonSize))) {
        SetTool(TransformTool::Rotate);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rotate (E)");
    }
    if (isRotate) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Scale tool
    bool isScale = (m_activeTool == TransformTool::Scale);
    if (isScale) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("R", ImVec2(buttonSize, buttonSize))) {
        SetTool(TransformTool::Scale);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Scale (R)");
    }
    if (isScale) {
        ImGui::PopStyleColor();
    }
}

void EditorToolManager::RenderSnappingControls() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    // Snap toggle button
    if (m_settings.snapping.enabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("Snap", ImVec2(buttonSize * 1.5f, buttonSize))) {
        ToggleSnapping();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle Snapping");
    }
    if (m_settings.snapping.enabled) {
        ImGui::PopStyleColor();
    }
}

void EditorToolManager::RenderSpaceToggle() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    const char* spaceLabel = (m_activeSpace == TransformSpace::World) ? "World" : "Local";
    if (ImGui::Button(spaceLabel, ImVec2(buttonSize * 2.0f, buttonSize))) {
        ToggleSpace();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle Transform Space (X)");
    }
}

} // namespace Nova
