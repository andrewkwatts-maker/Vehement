#include "Inspector.hpp"
#include "Editor.hpp"
#include <imgui.h>

namespace Vehement {

Inspector::Inspector(Editor* editor) : m_editor(editor) {}
Inspector::~Inspector() = default;

void Inspector::Render() {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }

    if (m_selectedEntity == 0) {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    RenderEntityInspector();

    ImGui::End();
}

void Inspector::RenderEntityInspector() {
    // Entity header
    ImGui::Text("Entity: %s", m_entityName.c_str());
    ImGui::TextDisabled("ID: %llu | Type: %s", (unsigned long long)m_selectedEntity, m_entityType.c_str());
    ImGui::Separator();

    // Name edit
    static char nameBuffer[256];
    strcpy(nameBuffer, m_entityName.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        m_entityName = nameBuffer;
        // TODO: Update entity name
    }

    // Active/enabled toggle
    static bool active = true;
    ImGui::Checkbox("Active", &active);

    ImGui::Separator();

    // Transform
    RenderTransformComponent();

    ImGui::Separator();

    // Components
    RenderComponents();

    ImGui::Separator();

    // Add component
    RenderAddComponent();
}

void Inspector::RenderTransformComponent() {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Position
        if (ImGui::DragFloat3("Position", m_position, 0.1f)) {
            // TODO: Update position
        }

        // Rotation
        if (ImGui::DragFloat3("Rotation", m_rotation, 1.0f, -360.0f, 360.0f)) {
            // TODO: Update rotation
        }

        // Scale
        if (ImGui::DragFloat3("Scale", m_scale, 0.01f, 0.01f, 100.0f)) {
            // TODO: Update scale
        }

        // Quick actions
        if (ImGui::Button("Reset")) {
            m_position[0] = m_position[1] = m_position[2] = 0.0f;
            m_rotation[0] = m_rotation[1] = m_rotation[2] = 0.0f;
            m_scale[0] = m_scale[1] = m_scale[2] = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            // TODO: Copy transform to clipboard
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste")) {
            // TODO: Paste transform from clipboard
        }
    }
}

void Inspector::RenderComponents() {
    for (size_t i = 0; i < m_components.size(); i++) {
        auto& comp = m_components[i];
        ImGui::PushID((int)i);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        bool open = ImGui::CollapsingHeader(comp.name.c_str(), flags);

        // Context menu for component
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove Component")) {
                // TODO: Remove component
            }
            if (ImGui::MenuItem("Reset")) {
                // TODO: Reset component to defaults
            }
            if (ImGui::MenuItem("Copy")) {
                // TODO: Copy component
            }
            ImGui::EndPopup();
        }

        if (open) {
            // Render component-specific properties
            if (comp.type == "Health") {
                static float maxHealth = 100.0f;
                static float currentHealth = 100.0f;
                ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 10000.0f);
                ImGui::DragFloat("Current Health", &currentHealth, 1.0f, 0.0f, maxHealth);

                // Health bar
                float ratio = currentHealth / maxHealth;
                ImGui::ProgressBar(ratio, ImVec2(-1, 0));
            }
            else if (comp.type == "Movement") {
                static float speed = 5.0f;
                static float turnRate = 180.0f;
                ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat("Turn Rate", &turnRate, 1.0f, 0.0f, 720.0f);
            }
            else if (comp.type == "Combat") {
                static float damage = 10.0f;
                static float attackSpeed = 1.0f;
                static float range = 1.5f;
                ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("Attack Speed", &attackSpeed, 0.1f, 0.1f, 10.0f);
                ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100.0f);
            }
            else if (comp.type == "AI") {
                static const char* behaviors[] = {"Idle", "Patrol", "Guard", "Aggressive"};
                static int behavior = 0;
                ImGui::Combo("Behavior", &behavior, behaviors, IM_ARRAYSIZE(behaviors));

                static float aggroRange = 15.0f;
                ImGui::DragFloat("Aggro Range", &aggroRange, 0.5f, 0.0f, 100.0f);

                static char scriptPath[256] = "";
                ImGui::InputText("Script", scriptPath, sizeof(scriptPath));
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    // TODO: Open script browser
                }
            }
            else if (comp.type == "Scriptable") {
                static char scriptPath[256] = "";
                ImGui::InputText("Script Path", scriptPath, sizeof(scriptPath));
                ImGui::SameLine();
                if (ImGui::Button("Edit")) {
                    // TODO: Open script in editor
                }

                if (ImGui::TreeNode("Script Variables")) {
                    // TODO: Show script variables
                    ImGui::TextDisabled("No variables exposed");
                    ImGui::TreePop();
                }
            }
            else {
                ImGui::TextDisabled("Component properties not implemented");
            }
        }

        ImGui::PopID();
    }
}

void Inspector::RenderAddComponent() {
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::Text("Components");
        ImGui::Separator();

        if (ImGui::MenuItem("Health")) {
            m_components.push_back({"Health", "Health", true});
        }
        if (ImGui::MenuItem("Movement")) {
            m_components.push_back({"Movement", "Movement", true});
        }
        if (ImGui::MenuItem("Combat")) {
            m_components.push_back({"Combat", "Combat", true});
        }
        if (ImGui::MenuItem("AI")) {
            m_components.push_back({"AI", "AI", true});
        }
        if (ImGui::MenuItem("Scriptable")) {
            m_components.push_back({"Scriptable", "Scriptable", true});
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Physics Body")) {
            m_components.push_back({"Physics Body", "Physics", true});
        }
        if (ImGui::MenuItem("Collision Shape")) {
            m_components.push_back({"Collision Shape", "Collision", true});
        }
        if (ImGui::MenuItem("Audio Source")) {
            m_components.push_back({"Audio Source", "Audio", true});
        }

        ImGui::EndPopup();
    }
}

void Inspector::SetSelectedEntity(uint64_t entityId) {
    m_selectedEntity = entityId;
    // TODO: Load entity data
    m_entityName = "Entity_" + std::to_string(entityId);
    m_entityType = "unit";

    // Load components
    m_components.clear();
    m_components.push_back({"Health", "Health", true});
    m_components.push_back({"Movement", "Movement", true});
    m_components.push_back({"Combat", "Combat", true});
}

void Inspector::ClearSelection() {
    m_selectedEntity = 0;
    m_components.clear();
}

} // namespace Vehement
