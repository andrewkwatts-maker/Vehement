#include "UIComponents.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Nova {

using json = nlohmann::json;

// ============================================================================
// UIStyle Implementation
// ============================================================================

void UIStyle::ToJson(std::string& out) const {
    json j;
    j["backgroundColor"] = {backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a};
    j["textColor"] = {textColor.r, textColor.g, textColor.b, textColor.a};
    j["borderColor"] = {borderColor.r, borderColor.g, borderColor.b, borderColor.a};
    j["accentColor"] = {accentColor.r, accentColor.g, accentColor.b, accentColor.a};
    j["hoverColor"] = {hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a};
    j["activeColor"] = {activeColor.r, activeColor.g, activeColor.b, activeColor.a};
    j["disabledColor"] = {disabledColor.r, disabledColor.g, disabledColor.b, disabledColor.a};
    j["borderWidth"] = borderWidth;
    j["borderRadius"] = borderRadius;
    j["padding"] = padding;
    j["margin"] = margin;
    j["fontSize"] = fontSize;
    j["fontFamily"] = fontFamily;
    out = j.dump(2);
}

UIStyle UIStyle::FromJson(const std::string& jsonStr) {
    UIStyle style;
    try {
        json j = json::parse(jsonStr);
        auto readVec4 = [&](const std::string& key, glm::vec4& out) {
            if (j.contains(key) && j[key].is_array() && j[key].size() >= 4) {
                out = glm::vec4(j[key][0], j[key][1], j[key][2], j[key][3]);
            }
        };
        readVec4("backgroundColor", style.backgroundColor);
        readVec4("textColor", style.textColor);
        readVec4("borderColor", style.borderColor);
        readVec4("accentColor", style.accentColor);
        readVec4("hoverColor", style.hoverColor);
        readVec4("activeColor", style.activeColor);
        readVec4("disabledColor", style.disabledColor);

        if (j.contains("borderWidth")) style.borderWidth = j["borderWidth"];
        if (j.contains("borderRadius")) style.borderRadius = j["borderRadius"];
        if (j.contains("padding")) style.padding = j["padding"];
        if (j.contains("margin")) style.margin = j["margin"];
        if (j.contains("fontSize")) style.fontSize = j["fontSize"];
        if (j.contains("fontFamily")) style.fontFamily = j["fontFamily"];
    } catch (...) {}
    return style;
}

// ============================================================================
// UIComponent Base Implementation
// ============================================================================

UIComponent::UIComponent(const std::string& id) : m_id(id) {}

void UIComponent::ApplyStyle() {
    m_styleColorCount = 0;
    m_styleVarCount = 0;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(m_style.backgroundColor.r, m_style.backgroundColor.g, m_style.backgroundColor.b, m_style.backgroundColor.a));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_style.textColor.r, m_style.textColor.g, m_style.textColor.b, m_style.textColor.a));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(m_style.borderColor.r, m_style.borderColor.g, m_style.borderColor.b, m_style.borderColor.a));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(m_style.hoverColor.r, m_style.hoverColor.g, m_style.hoverColor.b, m_style.hoverColor.a));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(m_style.activeColor.r, m_style.activeColor.g, m_style.activeColor.b, m_style.activeColor.a));
    m_styleColorCount = 5;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, m_style.borderRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(m_style.padding, m_style.padding));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, m_style.borderWidth);
    m_styleVarCount = 3;
}

void UIComponent::PopStyle() {
    ImGui::PopStyleColor(m_styleColorCount);
    ImGui::PopStyleVar(m_styleVarCount);
}

void UIComponent::ToJson(std::string& out) const {
    json j;
    j["type"] = GetTypeName();
    j["id"] = m_id;
    j["visible"] = m_visible;
    j["enabled"] = m_enabled;
    j["tooltip"] = m_tooltip;
    j["size"] = {m_size.x, m_size.y};
    j["position"] = {m_position.x, m_position.y};
    std::string styleJson;
    m_style.ToJson(styleJson);
    j["style"] = json::parse(styleJson);
    out = j.dump(2);
}

void UIComponent::FromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        if (j.contains("id")) m_id = j["id"];
        if (j.contains("visible")) m_visible = j["visible"];
        if (j.contains("enabled")) m_enabled = j["enabled"];
        if (j.contains("tooltip")) m_tooltip = j["tooltip"];
        if (j.contains("size") && j["size"].is_array()) {
            m_size = glm::vec2(j["size"][0], j["size"][1]);
        }
        if (j.contains("position") && j["position"].is_array()) {
            m_position = glm::vec2(j["position"][0], j["position"][1]);
        }
        if (j.contains("style")) {
            m_style = UIStyle::FromJson(j["style"].dump());
        }
    } catch (...) {}
}

// ============================================================================
// Basic Component Implementations
// ============================================================================

UILabel::UILabel(const std::string& text, const std::string& id)
    : UIComponent(id), m_text(text) {}

void UILabel::Render() {
    if (!m_visible) return;
    ApplyStyle();
    ImGui::TextUnformatted(m_text.c_str());
    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UIButton::UIButton(const std::string& label, const std::string& id)
    : UIComponent(id), m_label(label) {}

void UIButton::Render() {
    if (!m_visible) return;
    ApplyStyle();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(m_style.accentColor.r, m_style.accentColor.g, m_style.accentColor.b, m_style.accentColor.a));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(m_style.hoverColor.r, m_style.hoverColor.g, m_style.hoverColor.b, m_style.hoverColor.a));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(m_style.activeColor.r, m_style.activeColor.g, m_style.activeColor.b, m_style.activeColor.a));

    bool disabled = !m_enabled;
    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(m_style.disabledColor.r, m_style.disabledColor.g, m_style.disabledColor.b, m_style.disabledColor.a));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(m_style.disabledColor.r, m_style.disabledColor.g, m_style.disabledColor.b, m_style.disabledColor.a));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(m_style.disabledColor.r, m_style.disabledColor.g, m_style.disabledColor.b, m_style.disabledColor.a));
    }

    std::string displayText = m_icon.empty() ? m_label : (m_icon + " " + m_label);
    ImVec2 btnSize = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(0, 0);

    if (ImGui::Button(displayText.c_str(), btnSize) && m_enabled) {
        TriggerClick();
    }

    if (disabled) {
        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleColor(3);

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UICheckbox::UICheckbox(const std::string& label, bool* value, const std::string& id)
    : UIComponent(id), m_label(label), m_value(value) {}

void UICheckbox::SetValue(bool value) {
    if (m_value) *m_value = value;
    else m_internalValue = value;
}

void UICheckbox::Render() {
    if (!m_visible) return;
    ApplyStyle();

    bool& val = m_value ? *m_value : m_internalValue;
    bool disabled = !m_enabled;

    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(m_style.disabledColor.r, m_style.disabledColor.g, m_style.disabledColor.b, m_style.disabledColor.a));
        ImGui::BeginDisabled();
    }

    std::string labelId = m_label + "##" + m_id;
    if (ImGui::Checkbox(labelId.c_str(), &val)) {
        TriggerChange();
    }

    if (disabled) {
        ImGui::EndDisabled();
        ImGui::PopStyleColor();
    }

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UITextInput::UITextInput(const std::string& label, const std::string& id)
    : UIComponent(id), m_label(label) {}

void UITextInput::SetText(const std::string& text) {
    m_text = text;
    strncpy(m_buffer, text.c_str(), sizeof(m_buffer) - 1);
    m_buffer[sizeof(m_buffer) - 1] = '\0';
}

void UITextInput::Render() {
    if (!m_visible) return;
    ApplyStyle();

    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    ImGuiInputTextFlags flags = 0;
    if (m_password) flags |= ImGuiInputTextFlags_Password;

    bool changed = false;
    if (m_multiline) {
        ImVec2 size = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(-FLT_MIN, 100);
        changed = ImGui::InputTextMultiline(labelId.c_str(), m_buffer, m_maxLength, size, flags);
    } else {
        if (m_size.x > 0) ImGui::SetNextItemWidth(m_size.x);
        changed = ImGui::InputText(labelId.c_str(), m_buffer, m_maxLength, flags);
    }

    if (changed) {
        m_text = m_buffer;
        TriggerChange();
    }

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UISlider::UISlider(const std::string& label, float* value, const std::string& id)
    : UIComponent(id), m_label(label), m_value(value) {}

void UISlider::SetValue(float value) {
    if (m_value) *m_value = std::clamp(value, m_min, m_max);
    else m_internalValue = std::clamp(value, m_min, m_max);
}

void UISlider::Render() {
    if (!m_visible) return;
    ApplyStyle();

    float& val = m_value ? *m_value : m_internalValue;
    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    if (m_size.x > 0) ImGui::SetNextItemWidth(m_size.x);
    if (ImGui::SliderFloat(labelId.c_str(), &val, m_min, m_max, m_format.c_str())) {
        TriggerChange();
    }

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UISliderInt::UISliderInt(const std::string& label, int* value, const std::string& id)
    : UIComponent(id), m_label(label), m_value(value) {}

void UISliderInt::SetValue(int value) {
    if (m_value) *m_value = std::clamp(value, m_min, m_max);
    else m_internalValue = std::clamp(value, m_min, m_max);
}

void UISliderInt::Render() {
    if (!m_visible) return;
    ApplyStyle();

    int& val = m_value ? *m_value : m_internalValue;
    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    if (m_size.x > 0) ImGui::SetNextItemWidth(m_size.x);
    if (ImGui::SliderInt(labelId.c_str(), &val, m_min, m_max)) {
        TriggerChange();
    }

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UIColorPicker::UIColorPicker(const std::string& label, glm::vec4* value, const std::string& id)
    : UIComponent(id), m_label(label), m_value(value) {}

void UIColorPicker::SetValue(const glm::vec4& value) {
    if (m_value) *m_value = value;
    else m_internalValue = value;
}

void UIColorPicker::Render() {
    if (!m_visible) return;
    ApplyStyle();

    glm::vec4& val = m_value ? *m_value : m_internalValue;
    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float;
    bool changed = false;
    if (m_hasAlpha) {
        changed = ImGui::ColorEdit4(labelId.c_str(), &val.r, flags);
    } else {
        changed = ImGui::ColorEdit3(labelId.c_str(), &val.r, flags);
    }

    if (changed) TriggerChange();

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UIDropdown::UIDropdown(const std::string& label, const std::string& id)
    : UIComponent(id), m_label(label) {}

void UIDropdown::SetSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(m_options.size())) {
        m_selectedIndex = index;
    }
}

const std::string& UIDropdown::GetSelectedOption() const {
    static std::string empty;
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_options.size())) {
        return m_options[m_selectedIndex];
    }
    return empty;
}

void UIDropdown::Render() {
    if (!m_visible) return;
    ApplyStyle();

    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    if (m_size.x > 0) ImGui::SetNextItemWidth(m_size.x);

    const char* preview = m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_options.size())
        ? m_options[m_selectedIndex].c_str() : "";

    if (ImGui::BeginCombo(labelId.c_str(), preview)) {
        for (int i = 0; i < static_cast<int>(m_options.size()); ++i) {
            bool isSelected = (m_selectedIndex == i);
            if (ImGui::Selectable(m_options[i].c_str(), isSelected)) {
                m_selectedIndex = i;
                TriggerChange();
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

UIVector3Input::UIVector3Input(const std::string& label, glm::vec3* value, const std::string& id)
    : UIComponent(id), m_label(label), m_value(value) {}

void UIVector3Input::SetValue(const glm::vec3& value) {
    if (m_value) *m_value = value;
    else m_internalValue = value;
}

void UIVector3Input::Render() {
    if (!m_visible) return;
    ApplyStyle();

    glm::vec3& val = m_value ? *m_value : m_internalValue;
    std::string labelId = m_label + "##" + m_id;
    bool disabled = !m_enabled;

    if (disabled) ImGui::BeginDisabled();

    if (m_size.x > 0) ImGui::SetNextItemWidth(m_size.x);
    if (ImGui::DragFloat3(labelId.c_str(), &val.x, m_speed, m_min, m_max)) {
        TriggerChange();
    }

    if (disabled) ImGui::EndDisabled();

    if (!m_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_tooltip.c_str());
    }
    PopStyle();
}

// ============================================================================
// Container Implementations
// ============================================================================

UIContainer::UIContainer(const std::string& id) : UIComponent(id) {}

void UIContainer::AddChild(UIComponent::Ptr child) {
    m_children.push_back(child);
}

void UIContainer::RemoveChild(const std::string& id) {
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [&id](const UIComponent::Ptr& c) { return c->GetId() == id; }),
        m_children.end());
}

void UIContainer::ClearChildren() {
    m_children.clear();
}

UIComponent::Ptr UIContainer::GetChild(const std::string& id) {
    for (auto& child : m_children) {
        if (child->GetId() == id) return child;
    }
    return nullptr;
}

void UIContainer::Render() {
    if (!m_visible) return;
    for (auto& child : m_children) {
        child->Render();
    }
}

void UIContainer::Update(float dt) {
    for (auto& child : m_children) {
        child->Update(dt);
    }
}

UIPanel::UIPanel(const std::string& title, const std::string& id)
    : UIContainer(id), m_title(title) {}

void UIPanel::Render() {
    if (!m_visible) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (!m_closable) flags |= ImGuiWindowFlags_NoCollapse;

    bool open = true;
    bool* pOpen = m_closable ? &open : nullptr;

    if (m_collapsible) {
        if (ImGui::CollapsingHeader(m_title.c_str(), pOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
            m_collapsed = false;
            UIContainer::Render();
        } else {
            m_collapsed = true;
        }
    } else {
        if (ImGui::Begin(m_title.c_str(), pOpen, flags)) {
            UIContainer::Render();
        }
        ImGui::End();
    }

    if (pOpen && !*pOpen) {
        m_visible = false;
        if (m_onClose) m_onClose(this);
    }
}

UIHorizontalLayout::UIHorizontalLayout(const std::string& id) : UIContainer(id) {}

void UIHorizontalLayout::Render() {
    if (!m_visible) return;
    bool first = true;
    for (auto& child : m_children) {
        if (!first) {
            ImGui::SameLine(0, m_spacing);
        }
        first = false;
        child->Render();
    }
}

UIVerticalLayout::UIVerticalLayout(const std::string& id) : UIContainer(id) {}

void UIVerticalLayout::Render() {
    if (!m_visible) return;
    for (size_t i = 0; i < m_children.size(); ++i) {
        m_children[i]->Render();
        if (i < m_children.size() - 1) {
            ImGui::Dummy(ImVec2(0, m_spacing));
        }
    }
}

UITabContainer::UITabContainer(const std::string& id) : UIComponent(id) {}

void UITabContainer::AddTab(const std::string& name, UIComponent::Ptr content) {
    m_tabs.emplace_back(name, content);
    if (m_activeTab.empty()) m_activeTab = name;
}

void UITabContainer::RemoveTab(const std::string& name) {
    m_tabs.erase(
        std::remove_if(m_tabs.begin(), m_tabs.end(),
            [&name](const auto& tab) { return tab.first == name; }),
        m_tabs.end());
    if (m_activeTab == name && !m_tabs.empty()) {
        m_activeTab = m_tabs[0].first;
    }
}

void UITabContainer::SetActiveTab(const std::string& name) {
    for (const auto& tab : m_tabs) {
        if (tab.first == name) {
            m_activeTab = name;
            break;
        }
    }
}

void UITabContainer::Render() {
    if (!m_visible) return;

    if (ImGui::BeginTabBar(m_id.c_str())) {
        for (auto& [name, content] : m_tabs) {
            if (ImGui::BeginTabItem(name.c_str())) {
                m_activeTab = name;
                if (content) content->Render();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

UIScrollView::UIScrollView(const std::string& id) : UIContainer(id) {}

void UIScrollView::Render() {
    if (!m_visible) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (!m_scrollX) flags |= ImGuiWindowFlags_NoScrollbar;

    ImVec2 size = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(0, 0);
    if (ImGui::BeginChild(m_id.c_str(), size, ImGuiChildFlags_Borders, flags)) {
        UIContainer::Render();
    }
    ImGui::EndChild();
}

UIGridLayout::UIGridLayout(int columns, const std::string& id)
    : UIContainer(id), m_columns(columns) {}

void UIGridLayout::Render() {
    if (!m_visible) return;

    int col = 0;
    for (auto& child : m_children) {
        if (col > 0) {
            ImGui::SameLine(0, m_spacing.x);
        }
        child->Render();
        col++;
        if (col >= m_columns) {
            col = 0;
        }
    }
}

// ============================================================================
// Specialized Component Implementations
// ============================================================================

UIPropertyGrid::UIPropertyGrid(const std::string& id) : UIComponent(id) {}

void UIPropertyGrid::AddProperty(const std::string& name, PropertyValue value) {
    m_properties.push_back({name, value, ""});
}

void UIPropertyGrid::SetProperty(const std::string& name, PropertyValue value) {
    for (auto& prop : m_properties) {
        if (prop.name == name) {
            prop.value = value;
            return;
        }
    }
    AddProperty(name, value);
}

UIPropertyGrid::PropertyValue UIPropertyGrid::GetProperty(const std::string& name) const {
    for (const auto& prop : m_properties) {
        if (prop.name == name) return prop.value;
    }
    return false;
}

void UIPropertyGrid::RemoveProperty(const std::string& name) {
    m_properties.erase(
        std::remove_if(m_properties.begin(), m_properties.end(),
            [&name](const Property& p) { return p.name == name; }),
        m_properties.end());
}

void UIPropertyGrid::ClearProperties() {
    m_properties.clear();
}

void UIPropertyGrid::Render() {
    if (!m_visible) return;

    if (ImGui::BeginTable(m_id.c_str(), 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        for (auto& prop : m_properties) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", prop.name.c_str());

            ImGui::TableNextColumn();
            std::string id = "##" + prop.name;

            bool changed = false;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, bool>) {
                    bool val = arg;
                    if (ImGui::Checkbox(id.c_str(), &val)) {
                        prop.value = val;
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, int>) {
                    int val = arg;
                    if (ImGui::DragInt(id.c_str(), &val)) {
                        prop.value = val;
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, float>) {
                    float val = arg;
                    if (ImGui::DragFloat(id.c_str(), &val, 0.1f)) {
                        prop.value = val;
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    char buf[256];
                    strncpy(buf, arg.c_str(), sizeof(buf) - 1);
                    if (ImGui::InputText(id.c_str(), buf, sizeof(buf))) {
                        prop.value = std::string(buf);
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    glm::vec2 val = arg;
                    if (ImGui::DragFloat2(id.c_str(), &val.x, 0.1f)) {
                        prop.value = val;
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    glm::vec3 val = arg;
                    if (ImGui::DragFloat3(id.c_str(), &val.x, 0.1f)) {
                        prop.value = val;
                        changed = true;
                    }
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    glm::vec4 val = arg;
                    if (ImGui::ColorEdit4(id.c_str(), &val.r)) {
                        prop.value = val;
                        changed = true;
                    }
                }
            }, prop.value);

            if (changed && m_onPropertyChanged) {
                m_onPropertyChanged(prop.name, prop.value);
            }
        }

        ImGui::EndTable();
    }
}

UITreeView::UITreeView(const std::string& id) : UIComponent(id) {
    m_root = std::make_shared<TreeNode>();
    m_root->id = "root";
    m_root->label = "Root";
}

std::shared_ptr<UITreeView::TreeNode> UITreeView::AddNode(const std::string& label, const std::string& parentId) {
    auto parent = parentId.empty() ? m_root : FindNode(parentId);
    if (!parent) parent = m_root;

    auto node = std::make_shared<TreeNode>();
    node->id = label + "_" + std::to_string(parent->children.size());
    node->label = label;
    parent->children.push_back(node);
    return node;
}

void UITreeView::RemoveNode(const std::string& id) {
    std::function<bool(std::shared_ptr<TreeNode>)> removeFromParent;
    removeFromParent = [&](std::shared_ptr<TreeNode> parent) -> bool {
        auto it = std::find_if(parent->children.begin(), parent->children.end(),
            [&id](const auto& n) { return n->id == id; });
        if (it != parent->children.end()) {
            parent->children.erase(it);
            return true;
        }
        for (auto& child : parent->children) {
            if (removeFromParent(child)) return true;
        }
        return false;
    };
    removeFromParent(m_root);
}

void UITreeView::ClearNodes() {
    m_root->children.clear();
    m_selectedNode = nullptr;
}

void UITreeView::SetSelectedNode(const std::string& id) {
    if (m_selectedNode) m_selectedNode->selected = false;
    m_selectedNode = FindNode(id);
    if (m_selectedNode) m_selectedNode->selected = true;
}

std::shared_ptr<UITreeView::TreeNode> UITreeView::FindNode(const std::string& id, std::shared_ptr<TreeNode> root) {
    if (!root) root = m_root;
    if (root->id == id) return root;
    for (auto& child : root->children) {
        auto found = FindNode(id, child);
        if (found) return found;
    }
    return nullptr;
}

void UITreeView::RenderNode(std::shared_ptr<TreeNode> node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (node->selected) flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(node->id.c_str(), flags, "%s", node->label.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (m_selectedNode) m_selectedNode->selected = false;
        node->selected = true;
        m_selectedNode = node;
        if (m_onSelectionChanged) m_onSelectionChanged(node);
    }

    if (open) {
        for (auto& child : node->children) {
            RenderNode(child);
        }
        ImGui::TreePop();
    }
}

void UITreeView::Render() {
    if (!m_visible) return;
    for (auto& child : m_root->children) {
        RenderNode(child);
    }
}

UIListView::UIListView(const std::string& id) : UIComponent(id) {}

void UIListView::AddItem(const ListItem& item) {
    m_items.push_back(item);
}

void UIListView::RemoveItem(const std::string& id) {
    m_items.erase(
        std::remove_if(m_items.begin(), m_items.end(),
            [&id](const ListItem& item) { return item.id == id; }),
        m_items.end());
}

void UIListView::ClearItems() {
    m_items.clear();
    m_selectedIndex = -1;
    m_selectedIndices.clear();
}

void UIListView::SetSelectedIndex(int index) {
    if (index >= -1 && index < static_cast<int>(m_items.size())) {
        m_selectedIndex = index;
    }
}

const UIListView::ListItem* UIListView::GetSelectedItem() const {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_items.size())) {
        return &m_items[m_selectedIndex];
    }
    return nullptr;
}

void UIListView::Render() {
    if (!m_visible) return;

    ImVec2 size = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(-FLT_MIN, 200);
    if (ImGui::BeginListBox(m_id.c_str(), size)) {
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            const auto& item = m_items[i];
            bool isSelected = (m_selectedIndex == i);

            std::string displayText = item.icon.empty() ? item.label : (item.icon + " " + item.label);
            if (ImGui::Selectable(displayText.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selectedIndex = i;
                if (m_onSelectionChanged) m_onSelectionChanged(&m_items[i]);

                if (ImGui::IsMouseDoubleClicked(0) && m_onItemDoubleClicked) {
                    m_onItemDoubleClicked(&m_items[i]);
                }
            }

            if (!item.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", item.description.c_str());
            }
        }
        ImGui::EndListBox();
    }
}

UIImage::UIImage(const std::string& id) : UIComponent(id) {}

void UIImage::Render() {
    if (!m_visible || m_textureId == 0) return;

    ImVec2 size = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(100, 100);
    ImGui::Image(static_cast<ImTextureID>(m_textureId), size,
                 ImVec2(m_uv0.x, m_uv0.y), ImVec2(m_uv1.x, m_uv1.y),
                 ImVec4(m_tint.r, m_tint.g, m_tint.b, m_tint.a),
                 ImVec4(0, 0, 0, 0));
}

UIProgressBar::UIProgressBar(const std::string& id) : UIComponent(id) {}

void UIProgressBar::Render() {
    if (!m_visible) return;

    ImVec2 size = m_size.x > 0 ? ImVec2(m_size.x, m_size.y) : ImVec2(-FLT_MIN, 0);

    std::string overlay;
    if (m_showPercentage) {
        overlay = std::to_string(static_cast<int>(m_progress * 100)) + "%";
    }
    if (!m_label.empty()) {
        overlay = m_label + (m_showPercentage ? " - " + overlay : "");
    }

    ImGui::ProgressBar(m_progress, size, overlay.empty() ? nullptr : overlay.c_str());
}

// ============================================================================
// Factory Implementation
// ============================================================================

UIComponentFactory& UIComponentFactory::Instance() {
    static UIComponentFactory instance;
    return instance;
}

UIComponentFactory::UIComponentFactory() {
    RegisterBuiltinTypes();
}

void UIComponentFactory::RegisterBuiltinTypes() {
    RegisterType("Label", []() { return std::make_shared<UILabel>(); });
    RegisterType("Button", []() { return std::make_shared<UIButton>(); });
    RegisterType("Checkbox", []() { return std::make_shared<UICheckbox>(); });
    RegisterType("TextInput", []() { return std::make_shared<UITextInput>(); });
    RegisterType("Slider", []() { return std::make_shared<UISlider>(); });
    RegisterType("SliderInt", []() { return std::make_shared<UISliderInt>(); });
    RegisterType("ColorPicker", []() { return std::make_shared<UIColorPicker>(); });
    RegisterType("Dropdown", []() { return std::make_shared<UIDropdown>(); });
    RegisterType("Vector3Input", []() { return std::make_shared<UIVector3Input>(); });
    RegisterType("Container", []() { return std::make_shared<UIContainer>(); });
    RegisterType("Panel", []() { return std::make_shared<UIPanel>(); });
    RegisterType("HorizontalLayout", []() { return std::make_shared<UIHorizontalLayout>(); });
    RegisterType("VerticalLayout", []() { return std::make_shared<UIVerticalLayout>(); });
    RegisterType("TabContainer", []() { return std::make_shared<UITabContainer>(); });
    RegisterType("ScrollView", []() { return std::make_shared<UIScrollView>(); });
    RegisterType("GridLayout", []() { return std::make_shared<UIGridLayout>(); });
    RegisterType("PropertyGrid", []() { return std::make_shared<UIPropertyGrid>(); });
    RegisterType("TreeView", []() { return std::make_shared<UITreeView>(); });
    RegisterType("ListView", []() { return std::make_shared<UIListView>(); });
    RegisterType("Image", []() { return std::make_shared<UIImage>(); });
    RegisterType("ProgressBar", []() { return std::make_shared<UIProgressBar>(); });
}

void UIComponentFactory::RegisterType(const std::string& typeName, CreatorFunc creator) {
    m_creators[typeName] = creator;
}

UIComponent::Ptr UIComponentFactory::Create(const std::string& typeName) const {
    auto it = m_creators.find(typeName);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

UIComponent::Ptr UIComponentFactory::CreateFromJson(const std::string& jsonStr) const {
    try {
        json j = json::parse(jsonStr);
        if (!j.contains("type")) return nullptr;

        auto component = Create(j["type"]);
        if (component) {
            component->FromJson(jsonStr);
        }
        return component;
    } catch (...) {
        return nullptr;
    }
}

UIComponent::Ptr UIComponentFactory::CreateFromTemplate(const std::string& templatePath) const {
    std::ifstream file(templatePath);
    if (!file.is_open()) return nullptr;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return CreateFromJson(content);
}

std::vector<std::string> UIComponentFactory::GetRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& [name, _] : m_creators) {
        types.push_back(name);
    }
    return types;
}

// ============================================================================
// Theme Manager Implementation
// ============================================================================

UIThemeManager& UIThemeManager::Instance() {
    static UIThemeManager instance;
    return instance;
}

UIThemeManager::UIThemeManager() {
    LoadDefaultThemes();
    SetActiveTheme("Dark");
}

void UIThemeManager::LoadDefaultThemes() {
    UIStyle dark;
    dark.backgroundColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
    dark.textColor = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    dark.accentColor = glm::vec4(0.4f, 0.6f, 1.0f, 1.0f);
    m_themes["Dark"] = dark;

    UIStyle light;
    light.backgroundColor = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    light.textColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    light.accentColor = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f);
    light.hoverColor = glm::vec4(0.85f, 0.85f, 0.9f, 1.0f);
    light.activeColor = glm::vec4(0.8f, 0.8f, 0.85f, 1.0f);
    m_themes["Light"] = light;

    UIStyle highContrast;
    highContrast.backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    highContrast.textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    highContrast.borderColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    highContrast.accentColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    highContrast.borderWidth = 2.0f;
    m_themes["HighContrast"] = highContrast;
}

void UIThemeManager::LoadTheme(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    try {
        json j = json::parse(content);
        for (auto& [name, themeJson] : j.items()) {
            m_themes[name] = UIStyle::FromJson(themeJson.dump());
        }
    } catch (...) {}
}

void UIThemeManager::SaveTheme(const std::string& path) const {
    json j;
    for (const auto& [name, style] : m_themes) {
        std::string styleJson;
        style.ToJson(styleJson);
        j[name] = json::parse(styleJson);
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

void UIThemeManager::SetTheme(const std::string& name, const UIStyle& style) {
    m_themes[name] = style;
}

const UIStyle& UIThemeManager::GetTheme(const std::string& name) const {
    auto it = m_themes.find(name);
    if (it != m_themes.end()) return it->second;
    return m_activeTheme;
}

void UIThemeManager::SetActiveTheme(const std::string& name) {
    auto it = m_themes.find(name);
    if (it != m_themes.end()) {
        m_activeTheme = it->second;
        m_activeThemeName = name;
    }
}

std::vector<std::string> UIThemeManager::GetThemeNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_themes) {
        names.push_back(name);
    }
    return names;
}

void UIThemeManager::ApplyDarkTheme() {
    SetActiveTheme("Dark");
}

void UIThemeManager::ApplyLightTheme() {
    SetActiveTheme("Light");
}

void UIThemeManager::ApplyHighContrastTheme() {
    SetActiveTheme("HighContrast");
}

} // namespace Nova
