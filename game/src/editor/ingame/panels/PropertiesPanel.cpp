#include "PropertiesPanel.hpp"
#include "../InGameEditor.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Vehement {

PropertiesPanel::PropertiesPanel() = default;
PropertiesPanel::~PropertiesPanel() = default;

void PropertiesPanel::Initialize(InGameEditor& editor) {
    m_editor = &editor;
}

void PropertiesPanel::Shutdown() {
    m_editor = nullptr;
    ClearSelection();
}

void PropertiesPanel::Update(float deltaTime) {
    // Sync with selected object
}

void PropertiesPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (m_selectionId == 0) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No selection");
            ImGui::End();
            return;
        }

        // Selection header
        ImGui::Text("%s", m_selectionName.c_str());
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Type: %s | ID: %u",
                           m_selectionType.c_str(), m_selectionId);
        ImGui::Separator();

        // Filter
        char filterBuffer[64] = "";
        strncpy(filterBuffer, m_filterText.c_str(), sizeof(filterBuffer) - 1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##Filter", "Filter properties...", filterBuffer, sizeof(filterBuffer))) {
            m_filterText = filterBuffer;
        }
        ImGui::Separator();

        // Property sections
        if (ImGui::CollapsingHeader("Transform", m_showTransform ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            m_showTransform = true;
            RenderTransform();
        }

        if (ImGui::CollapsingHeader("Properties", m_showCustomProps ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            m_showCustomProps = true;
            RenderCustomProperties();
        }

        if (ImGui::CollapsingHeader("Behavior", m_showBehavior ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            m_showBehavior = true;
            RenderBehaviorSettings();
        }

        if (ImGui::CollapsingHeader("Visual", m_showVisual ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            m_showVisual = true;
            RenderVisualSettings();
        }
    }
    ImGui::End();
}

void PropertiesPanel::SetProperties(const std::vector<PropertyDef>& properties) {
    m_properties = properties;
}

void PropertiesPanel::ClearProperties() {
    m_properties.clear();
}

void PropertiesPanel::SetSelectionInfo(const std::string& type, const std::string& name, uint32_t id) {
    m_selectionType = type;
    m_selectionName = name;
    m_selectionId = id;
}

void PropertiesPanel::ClearSelection() {
    m_selectionType.clear();
    m_selectionName.clear();
    m_selectionId = 0;
    ClearProperties();
}

void PropertiesPanel::RenderTransform() {
    bool changed = false;

    // Position
    if (RenderVec3Property("Position", m_position)) {
        changed = true;
    }

    // Rotation (in degrees for display)
    glm::vec3 rotDegrees = m_rotation;
    if (RenderVec3Property("Rotation", rotDegrees)) {
        m_rotation = rotDegrees;
        changed = true;
    }

    // Scale
    if (RenderVec3Property("Scale", m_scale)) {
        changed = true;
    }

    // Quick buttons
    ImGui::Separator();
    if (ImGui::Button("Reset Position")) {
        m_position = glm::vec3(0.0f);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Rotation")) {
        m_rotation = glm::vec3(0.0f);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scale")) {
        m_scale = glm::vec3(1.0f);
        changed = true;
    }

    if (changed && OnPropertyChanged) {
        // Notify of transform change
    }
}

void PropertiesPanel::RenderCustomProperties() {
    if (m_properties.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No custom properties");
        return;
    }

    // Group by category
    std::unordered_map<std::string, std::vector<PropertyDef*>> categories;
    for (auto& prop : m_properties) {
        if (!prop.visible) continue;

        // Apply filter
        if (!m_filterText.empty()) {
            std::string nameLower = prop.displayName;
            std::string filterLower = m_filterText;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos) {
                continue;
            }
        }

        categories[prop.category].push_back(&prop);
    }

    // Render each category
    for (auto& [category, props] : categories) {
        if (!category.empty()) {
            ImGui::Text("%s", category.c_str());
            ImGui::Indent();
        }

        for (auto* prop : props) {
            if (RenderProperty(*prop)) {
                if (OnPropertyChanged) {
                    OnPropertyChanged(prop->name, prop->value);
                }
            }
        }

        if (!category.empty()) {
            ImGui::Unindent();
        }
    }
}

void PropertiesPanel::RenderBehaviorSettings() {
    // Common behavior properties
    static bool isEnabled = true;
    static bool isVisible = true;
    static bool isSelectable = true;

    if (ImGui::Checkbox("Enabled", &isEnabled)) {
        if (OnPropertyChanged) OnPropertyChanged("enabled", isEnabled);
    }

    if (ImGui::Checkbox("Visible", &isVisible)) {
        if (OnPropertyChanged) OnPropertyChanged("visible", isVisible);
    }

    if (ImGui::Checkbox("Selectable", &isSelectable)) {
        if (OnPropertyChanged) OnPropertyChanged("selectable", isSelectable);
    }

    // Object-type specific settings
    if (m_selectionType == "unit") {
        ImGui::Separator();
        static int player = 0;
        if (ImGui::SliderInt("Owner Player", &player, 0, 7)) {
            if (OnPropertyChanged) OnPropertyChanged("player", player);
        }

        static int facing = 0;
        if (ImGui::SliderInt("Facing", &facing, 0, 360)) {
            if (OnPropertyChanged) OnPropertyChanged("facing", facing);
        }
    }
    else if (m_selectionType == "building") {
        ImGui::Separator();
        static int player = 0;
        if (ImGui::SliderInt("Owner Player", &player, 0, 7)) {
            if (OnPropertyChanged) OnPropertyChanged("player", player);
        }

        static float constructionProgress = 100.0f;
        if (ImGui::SliderFloat("Construction %", &constructionProgress, 0.0f, 100.0f)) {
            if (OnPropertyChanged) OnPropertyChanged("construction", constructionProgress);
        }
    }
    else if (m_selectionType == "trigger_zone") {
        ImGui::Separator();
        static float radius = 5.0f;
        if (ImGui::SliderFloat("Radius", &radius, 1.0f, 50.0f)) {
            if (OnPropertyChanged) OnPropertyChanged("radius", radius);
        }

        static bool isCircle = true;
        if (ImGui::Checkbox("Circle Shape", &isCircle)) {
            if (OnPropertyChanged) OnPropertyChanged("isCircle", isCircle);
        }
    }
}

void PropertiesPanel::RenderVisualSettings() {
    // Color tint
    static glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    if (RenderColorProperty("Tint", tint)) {
        if (OnPropertyChanged) OnPropertyChanged("tint", tint);
    }

    // Scale override
    static float visualScale = 1.0f;
    if (ImGui::SliderFloat("Visual Scale", &visualScale, 0.1f, 3.0f)) {
        if (OnPropertyChanged) OnPropertyChanged("visualScale", visualScale);
    }

    // Animation
    ImGui::Separator();
    static int animationIndex = 0;
    const char* animations[] = {"Idle", "Walk", "Attack", "Death", "Custom"};
    if (ImGui::Combo("Animation", &animationIndex, animations, 5)) {
        if (OnPropertyChanged) OnPropertyChanged("animation", std::string(animations[animationIndex]));
    }

    // Variation
    static int variation = 0;
    if (ImGui::SliderInt("Variation", &variation, 0, 10)) {
        if (OnPropertyChanged) OnPropertyChanged("variation", variation);
    }
}

bool PropertiesPanel::RenderProperty(PropertyDef& prop) {
    if (prop.readOnly) {
        ImGui::BeginDisabled();
    }

    bool changed = false;

    // Render based on type
    if (std::holds_alternative<bool>(prop.value)) {
        bool val = std::get<bool>(prop.value);
        if (RenderBoolProperty(prop.displayName, val)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<int>(prop.value)) {
        int val = std::get<int>(prop.value);
        int minVal = std::holds_alternative<int>(prop.minValue) ? std::get<int>(prop.minValue) : INT_MIN;
        int maxVal = std::holds_alternative<int>(prop.maxValue) ? std::get<int>(prop.maxValue) : INT_MAX;
        if (RenderIntProperty(prop.displayName, val, minVal, maxVal)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<float>(prop.value)) {
        float val = std::get<float>(prop.value);
        float minVal = std::holds_alternative<float>(prop.minValue) ? std::get<float>(prop.minValue) : -FLT_MAX;
        float maxVal = std::holds_alternative<float>(prop.maxValue) ? std::get<float>(prop.maxValue) : FLT_MAX;
        if (RenderFloatProperty(prop.displayName, val, minVal, maxVal)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<std::string>(prop.value)) {
        std::string val = std::get<std::string>(prop.value);
        if (RenderStringProperty(prop.displayName, val)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<glm::vec2>(prop.value)) {
        glm::vec2 val = std::get<glm::vec2>(prop.value);
        if (RenderVec2Property(prop.displayName, val)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<glm::vec3>(prop.value)) {
        glm::vec3 val = std::get<glm::vec3>(prop.value);
        if (RenderVec3Property(prop.displayName, val)) {
            prop.value = val;
            changed = true;
        }
    }
    else if (std::holds_alternative<glm::vec4>(prop.value)) {
        glm::vec4 val = std::get<glm::vec4>(prop.value);
        if (prop.name.find("color") != std::string::npos ||
            prop.name.find("Color") != std::string::npos) {
            if (RenderColorProperty(prop.displayName, val)) {
                prop.value = val;
                changed = true;
            }
        } else {
            if (RenderVec4Property(prop.displayName, val)) {
                prop.value = val;
                changed = true;
            }
        }
    }

    // Tooltip
    if (!prop.tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", prop.tooltip.c_str());
    }

    if (prop.readOnly) {
        ImGui::EndDisabled();
    }

    return changed;
}

bool PropertiesPanel::RenderBoolProperty(const std::string& name, bool& value) {
    return ImGui::Checkbox(name.c_str(), &value);
}

bool PropertiesPanel::RenderIntProperty(const std::string& name, int& value, int min, int max) {
    if (min != INT_MIN && max != INT_MAX) {
        return ImGui::SliderInt(name.c_str(), &value, min, max);
    }
    return ImGui::InputInt(name.c_str(), &value);
}

bool PropertiesPanel::RenderFloatProperty(const std::string& name, float& value, float min, float max) {
    if (min != -FLT_MAX && max != FLT_MAX) {
        return ImGui::SliderFloat(name.c_str(), &value, min, max);
    }
    return ImGui::InputFloat(name.c_str(), &value, 0.1f, 1.0f, "%.3f");
}

bool PropertiesPanel::RenderStringProperty(const std::string& name, std::string& value) {
    char buffer[256];
    strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer))) {
        value = buffer;
        return true;
    }
    return false;
}

bool PropertiesPanel::RenderVec2Property(const std::string& name, glm::vec2& value) {
    return ImGui::InputFloat2(name.c_str(), &value.x);
}

bool PropertiesPanel::RenderVec3Property(const std::string& name, glm::vec3& value) {
    return ImGui::InputFloat3(name.c_str(), &value.x);
}

bool PropertiesPanel::RenderVec4Property(const std::string& name, glm::vec4& value) {
    return ImGui::InputFloat4(name.c_str(), &value.x);
}

bool PropertiesPanel::RenderColorProperty(const std::string& name, glm::vec4& value) {
    return ImGui::ColorEdit4(name.c_str(), &value.x);
}

} // namespace Vehement
