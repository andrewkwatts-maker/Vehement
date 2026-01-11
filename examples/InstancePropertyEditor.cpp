#include "InstancePropertyEditor.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

InstancePropertyEditor::InstancePropertyEditor()
    : m_lastChangeTime(std::chrono::steady_clock::now()) {
}

void InstancePropertyEditor::Initialize(Nova::InstanceManager* instanceManager) {
    m_instanceManager = instanceManager;
    spdlog::info("InstancePropertyEditor initialized");
}

void InstancePropertyEditor::RenderPanel(const std::string& selectedInstanceId,
                                         glm::vec3& position, glm::vec3& rotation, glm::vec3& scale) {
    if (selectedInstanceId.empty() || !m_instanceManager) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
        ImGui::Separator();
        ImGui::TextWrapped("Select an object in the viewport to view and edit its properties.");
        return;
    }

    m_currentInstanceId = selectedInstanceId;

    Nova::InstanceData* instance = m_instanceManager->GetInstance(selectedInstanceId);
    if (!instance) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Instance not found!");
        ImGui::Text("Instance ID: %s", selectedInstanceId.c_str());
        return;
    }

    // Render header
    RenderHeader(*instance);

    ImGui::Separator();

    // Search/filter bar
    PropertyEditor::RenderFilterBar(m_propertyFilter);

    ImGui::Separator();

    // Transform properties (always editable)
    if (m_showTransform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (PropertyEditor::RenderTransformProperties(position, rotation, scale)) {
            // Update instance transform
            instance->position = position;
            instance->rotation = glm::quat(glm::radians(rotation));
            instance->scale = scale;
            MarkDirty();
        }
    }

    // Load archetype config
    nlohmann::json archetypeConfig = m_instanceManager->LoadArchetype(instance->archetypeId);

    // Archetype properties (read-only)
    if (m_showArchetypeProperties && !archetypeConfig.empty()) {
        RenderArchetypeProperties(archetypeConfig, *instance);
    }

    // Instance overrides (editable)
    if (m_showInstanceOverrides) {
        RenderInstanceOverrides(*instance);
    }

    // Custom data
    if (m_showCustomData) {
        RenderCustomData(*instance);
    }

    // Show dirty indicator at bottom
    if (instance->isDirty) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "* Unsaved changes");

        if (m_autoSaveEnabled) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - m_lastChangeTime).count();
            float remaining = m_autoSaveDelay - elapsed;

            if (remaining > 0) {
                ImGui::SameLine();
                ImGui::Text("(Auto-save in %.1fs)", remaining);
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Save Now")) {
            if (m_instanceManager->SaveInstanceToMap(m_currentMapName, *instance)) {
                spdlog::info("Manually saved instance: {}", instance->instanceId);
            }
        }
    }
}

void InstancePropertyEditor::RenderHeader(const Nova::InstanceData& instance) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
    ImGui::Text("Instance Properties");
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Instance name
    ImGui::Text("Name: %s", instance.name.empty() ? "<unnamed>" : instance.name.c_str());

    // Archetype reference
    ImGui::Text("Archetype: %s", instance.archetypeId.c_str());

    // Instance ID (collapsible)
    if (ImGui::TreeNode("Instance ID")) {
        ImGui::TextWrapped("%s", instance.instanceId.c_str());
        ImGui::TreePop();
    }

    // Action buttons
    ImGui::Spacing();
    if (ImGui::Button("Copy Instance ID")) {
        ImGui::SetClipboardText(instance.instanceId.c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button("View Archetype")) {
        // TODO: Open archetype file or show archetype details
        spdlog::info("View archetype: {}", instance.archetypeId);
    }
}

void InstancePropertyEditor::RenderArchetypeProperties(const nlohmann::json& archetypeConfig,
                                                       const Nova::InstanceData& instance) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 0.8f));

    if (ImGui::CollapsingHeader("Archetype Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Base properties from archetype (read-only)");

        ImGui::Separator();

        // Render archetype properties as read-only tree
        nlohmann::json mutableConfig = archetypeConfig;  // Copy for rendering
        PropertyEditor::RenderPropertyTree("Base Config", mutableConfig, true, &instance.overrides);
    }

    ImGui::PopStyleColor();
}

void InstancePropertyEditor::RenderInstanceOverrides(Nova::InstanceData& instance) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.3f, 0.8f));

    if (ImGui::CollapsingHeader("Instance Overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(ImVec4(0.9f, 1.0f, 0.9f, 1.0f),
                           "Properties overridden for this instance");

        if (instance.overrides.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No overrides");
            ImGui::TextWrapped("Click 'Override' next to archetype properties to customize them.");
        } else {
            ImGui::Separator();

            // Render overrides as editable tree
            if (PropertyEditor::RenderPropertyTree("Overrides", instance.overrides, false, nullptr,
                                                   [this](const std::string& key) { this->MarkDirty(); })) {
                MarkDirty();
            }

            ImGui::Separator();

            // Clear all overrides button
            if (ImGui::Button("Clear All Overrides")) {
                instance.ClearOverrides();
                MarkDirty();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset all properties to archetype defaults");
            }
        }
    }

    ImGui::PopStyleColor();
}

void InstancePropertyEditor::RenderCustomData(Nova::InstanceData& instance) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.3f, 0.5f, 0.8f));

    if (ImGui::CollapsingHeader("Custom Data")) {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 1.0f, 1.0f),
                           "Instance-specific data (not in archetype)");

        if (instance.customData.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No custom data");
        } else {
            ImGui::Separator();

            // Render custom data as editable tree
            if (PropertyEditor::RenderPropertyTree("Custom", instance.customData, false, nullptr,
                                                   [this](const std::string& key) { this->MarkDirty(); })) {
                MarkDirty();
            }

            ImGui::Separator();

            // Clear custom data button
            if (ImGui::Button("Clear Custom Data")) {
                instance.customData = nlohmann::json::object();
                MarkDirty();
            }
        }

        ImGui::Separator();

        // Add new custom property UI
        static char keyBuffer[128] = "";
        static char valueBuffer[256] = "";

        ImGui::Text("Add Custom Property:");
        ImGui::InputText("Key", keyBuffer, sizeof(keyBuffer));
        ImGui::InputText("Value", valueBuffer, sizeof(valueBuffer));

        if (ImGui::Button("Add String Property")) {
            if (strlen(keyBuffer) > 0) {
                instance.customData[keyBuffer] = std::string(valueBuffer);
                MarkDirty();
                keyBuffer[0] = '\0';
                valueBuffer[0] = '\0';
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Add Boolean")) {
            if (strlen(keyBuffer) > 0) {
                instance.customData[keyBuffer] = true;
                MarkDirty();
                keyBuffer[0] = '\0';
            }
        }
    }

    ImGui::PopStyleColor();
}

void InstancePropertyEditor::MarkDirty() {
    if (!m_currentInstanceId.empty() && m_instanceManager) {
        m_instanceManager->MarkDirty(m_currentInstanceId);
    }

    m_hasPendingChanges = true;
    m_lastChangeTime = std::chrono::steady_clock::now();
}

bool InstancePropertyEditor::ShouldAutoSave() const {
    if (!m_autoSaveEnabled || !m_hasPendingChanges) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_lastChangeTime).count();

    return elapsed >= m_autoSaveDelay;
}

void InstancePropertyEditor::Update(float deltaTime) {
    if (ShouldAutoSave() && m_instanceManager) {
        int savedCount = m_instanceManager->SaveDirtyInstances(m_currentMapName);
        if (savedCount > 0) {
            spdlog::info("Auto-saved {} instances", savedCount);
            m_hasPendingChanges = false;
        }
    }
}

void InstancePropertyEditor::SaveAll(const std::string& mapName) {
    m_currentMapName = mapName;

    if (m_instanceManager) {
        int savedCount = m_instanceManager->SaveDirtyInstances(mapName);
        if (savedCount > 0) {
            spdlog::info("Saved {} instances to map: {}", savedCount, mapName);
        }
        m_hasPendingChanges = false;
    }
}

bool InstancePropertyEditor::HasUnsavedChanges() const {
    return m_hasPendingChanges && m_instanceManager &&
           !m_instanceManager->GetDirtyInstances().empty();
}

int InstancePropertyEditor::GetDirtyCount() const {
    if (m_instanceManager) {
        return static_cast<int>(m_instanceManager->GetDirtyInstances().size());
    }
    return 0;
}
