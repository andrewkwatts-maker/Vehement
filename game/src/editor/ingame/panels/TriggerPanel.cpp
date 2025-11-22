#include "TriggerPanel.hpp"
#include <imgui.h>

namespace Vehement {

TriggerPanel::TriggerPanel() = default;
TriggerPanel::~TriggerPanel() = default;

void TriggerPanel::Initialize(TriggerEditor& triggerEditor) {
    m_triggerEditor = &triggerEditor;
}

void TriggerPanel::Shutdown() {
    m_triggerEditor = nullptr;
    m_debugLog.clear();
}

void TriggerPanel::Update(float deltaTime) {
    // Update debug state
}

void TriggerPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Triggers", nullptr, ImGuiWindowFlags_NoCollapse)) {
        // Toolbar
        if (ImGui::Button("+ Trigger")) {
            if (m_triggerEditor) {
                m_triggerEditor->CreateTrigger("New Trigger");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Variables")) {
            // Open variable manager
        }
        ImGui::SameLine();
        if (ImGui::Button("Debug")) {
            m_showDebugger = !m_showDebugger;
        }

        ImGui::Separator();

        // Main content
        if (ImGui::BeginTabBar("TriggerPanelTabs")) {
            if (ImGui::BeginTabItem("List")) {
                RenderTriggerList();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Quick Add")) {
                RenderQuickBuilder();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // Debug panel
        if (m_showDebugger) {
            ImGui::Separator();
            RenderDebugger();
        }
    }
    ImGui::End();
}

void TriggerPanel::RenderTriggerList() {
    if (!m_triggerEditor) {
        ImGui::Text("No trigger editor");
        return;
    }

    ImGui::BeginChild("TriggerListScroll", ImVec2(0, 0), true);

    const auto& triggers = m_triggerEditor->GetTriggers();
    const auto& groups = m_triggerEditor->GetGroups();

    // Render groups
    for (const auto& group : groups) {
        if (group.parentGroupId != 0) continue;  // Skip non-root groups

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (group.expanded) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool open = ImGui::TreeNodeEx(group.name.c_str(), flags);

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Add Trigger")) {
                uint32_t newId = m_triggerEditor->CreateTrigger("New Trigger");
                m_triggerEditor->MoveToGroup(newId, group.id);
            }
            if (ImGui::MenuItem("Rename")) {
                // Open rename dialog
            }
            if (ImGui::MenuItem("Delete Group")) {
                m_triggerEditor->DeleteGroup(group.id);
            }
            ImGui::EndPopup();
        }

        if (open) {
            // Render triggers in this group
            for (uint32_t triggerId : group.triggerIds) {
                for (const auto& trigger : triggers) {
                    if (trigger.id == triggerId) {
                        RenderTriggerItem(trigger);
                        break;
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    // Render ungrouped triggers
    for (const auto& trigger : triggers) {
        if (trigger.parentGroupId == 0) {
            RenderTriggerItem(trigger);
        }
    }

    ImGui::EndChild();
}

void TriggerPanel::RenderTriggerItem(const Trigger& trigger) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    uint32_t selectedId = m_triggerEditor ? m_triggerEditor->GetSelectedTriggerId() : 0;
    if (trigger.id == selectedId) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Icon based on state
    const char* stateIcon = trigger.enabled ? "[+]" : "[-]";
    std::string label = std::string(stateIcon) + " " + trigger.name;

    // Highlight if has issues
    if (trigger.events.empty() || trigger.actions.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
    }

    ImGui::TreeNodeEx(label.c_str(), flags);

    if (trigger.events.empty() || trigger.actions.empty()) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked()) {
        if (m_triggerEditor) {
            m_triggerEditor->SelectTrigger(trigger.id);
        }
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(trigger.enabled ? "Disable" : "Enable")) {
            if (m_triggerEditor) {
                m_triggerEditor->EnableTrigger(trigger.id, !trigger.enabled);
            }
        }
        if (ImGui::MenuItem("Duplicate")) {
            // Duplicate trigger
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            if (m_triggerEditor) {
                m_triggerEditor->DeleteTrigger(trigger.id);
            }
        }
        ImGui::EndPopup();
    }

    // Tooltip with summary
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", trigger.name.c_str());
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "%zu events, %zu conditions, %zu actions",
                           trigger.events.size(), trigger.conditions.size(), trigger.actions.size());
        ImGui::EndTooltip();
    }
}

void TriggerPanel::RenderQuickBuilder() {
    ImGui::Text("Quick Add Component");
    ImGui::Separator();

    // Mode selection
    const char* modes[] = {"Event", "Condition", "Action"};
    ImGui::Combo("Type", &m_builderMode, modes, 3);

    ImGui::Separator();

    switch (m_builderMode) {
        case 0: RenderEventBuilder(); break;
        case 1: RenderConditionBuilder(); break;
        case 2: RenderActionBuilder(); break;
    }
}

void TriggerPanel::RenderEventBuilder() {
    if (!m_triggerEditor) return;

    ImGui::Text("Add Event to Selected Trigger");

    auto templates = m_triggerEditor->GetEventTemplates();
    if (templates.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No event templates");
        return;
    }

    ImGui::BeginChild("EventTemplates", ImVec2(0, 150), true);
    for (const auto& name : templates) {
        if (ImGui::Selectable(name.c_str(), m_selectedTemplate == name)) {
            m_selectedTemplate = name;
        }
    }
    ImGui::EndChild();

    if (!m_selectedTemplate.empty()) {
        if (ImGui::Button("Add Event")) {
            uint32_t selectedId = m_triggerEditor->GetSelectedTriggerId();
            if (selectedId != 0) {
                TriggerEvent event = m_triggerEditor->CreateEventFromTemplate(m_selectedTemplate);
                m_triggerEditor->AddEvent(selectedId, event);
            }
        }
    }
}

void TriggerPanel::RenderConditionBuilder() {
    if (!m_triggerEditor) return;

    ImGui::Text("Add Condition to Selected Trigger");

    auto templates = m_triggerEditor->GetConditionTemplates();
    if (templates.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No condition templates");
        return;
    }

    ImGui::BeginChild("ConditionTemplates", ImVec2(0, 150), true);
    for (const auto& name : templates) {
        if (ImGui::Selectable(name.c_str(), m_selectedTemplate == name)) {
            m_selectedTemplate = name;
        }
    }
    ImGui::EndChild();

    if (!m_selectedTemplate.empty()) {
        if (ImGui::Button("Add Condition")) {
            uint32_t selectedId = m_triggerEditor->GetSelectedTriggerId();
            if (selectedId != 0) {
                TriggerCondition cond = m_triggerEditor->CreateConditionFromTemplate(m_selectedTemplate);
                m_triggerEditor->AddCondition(selectedId, cond);
            }
        }
    }
}

void TriggerPanel::RenderActionBuilder() {
    if (!m_triggerEditor) return;

    ImGui::Text("Add Action to Selected Trigger");

    auto templates = m_triggerEditor->GetActionTemplates();
    if (templates.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No action templates");
        return;
    }

    ImGui::BeginChild("ActionTemplates", ImVec2(0, 150), true);
    for (const auto& name : templates) {
        if (ImGui::Selectable(name.c_str(), m_selectedTemplate == name)) {
            m_selectedTemplate = name;
        }
    }
    ImGui::EndChild();

    if (!m_selectedTemplate.empty()) {
        if (ImGui::Button("Add Action")) {
            uint32_t selectedId = m_triggerEditor->GetSelectedTriggerId();
            if (selectedId != 0) {
                TriggerAction action = m_triggerEditor->CreateActionFromTemplate(m_selectedTemplate);
                m_triggerEditor->AddAction(selectedId, action);
            }
        }
    }
}

void TriggerPanel::RenderDebugger() {
    ImGui::Text("Trigger Debugger");

    // Controls
    if (ImGui::Button("Step")) {
        // Execute one step
    }
    ImGui::SameLine();
    if (ImGui::Button(m_stepMode ? "Continue" : "Pause")) {
        m_stepMode = !m_stepMode;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        m_debugLog.clear();
    }

    // Log output
    ImGui::BeginChild("DebugLog", ImVec2(0, 100), true);
    for (const auto& line : m_debugLog) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

// Helper to render trigger item - need forward declaration
void TriggerPanel::RenderTriggerItem(const Trigger& trigger);

} // namespace Vehement
