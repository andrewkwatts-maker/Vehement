#include "Inspector.hpp"
#include "Editor.hpp"
#include "ScriptEditor.hpp"
#include "../entities/EntityManager.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>

namespace Vehement {

// Clipboard for transform copy/paste
struct TransformClipboard {
    float position[3] = {0, 0, 0};
    float rotation[3] = {0, 0, 0};
    float scale[3] = {1, 1, 1};
    bool hasData = false;
};
static TransformClipboard s_transformClipboard;

// Clipboard for component copy
struct ComponentClipboard {
    std::string type;
    std::string name;
    bool hasData = false;
};
static ComponentClipboard s_componentClipboard;

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
        // Update entity name in the entity manager
        if (m_editor && m_editor->GetEntityManager()) {
            auto* entity = m_editor->GetEntityManager()->GetEntity(m_selectedEntity);
            if (entity) {
                entity->SetName(m_entityName);
                m_editor->MarkDirty();
            }
        }
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
        Entity* entity = nullptr;
        if (m_editor && m_editor->GetEntityManager()) {
            entity = m_editor->GetEntityManager()->GetEntity(m_selectedEntity);
        }

        // Position
        if (ImGui::DragFloat3("Position", m_position, 0.1f)) {
            // Update entity position
            if (entity) {
                entity->SetPosition(glm::vec3(m_position[0], m_position[1], m_position[2]));
                m_editor->MarkDirty();
            }
        }

        // Rotation
        if (ImGui::DragFloat3("Rotation", m_rotation, 1.0f, -360.0f, 360.0f)) {
            // Update entity rotation (Euler angles to quaternion)
            if (entity) {
                entity->SetRotation(glm::vec3(
                    glm::radians(m_rotation[0]),
                    glm::radians(m_rotation[1]),
                    glm::radians(m_rotation[2])
                ));
                m_editor->MarkDirty();
            }
        }

        // Scale
        if (ImGui::DragFloat3("Scale", m_scale, 0.01f, 0.01f, 100.0f)) {
            // Update entity scale
            if (entity) {
                entity->SetScale(glm::vec3(m_scale[0], m_scale[1], m_scale[2]));
                m_editor->MarkDirty();
            }
        }

        // Quick actions
        if (ImGui::Button("Reset")) {
            m_position[0] = m_position[1] = m_position[2] = 0.0f;
            m_rotation[0] = m_rotation[1] = m_rotation[2] = 0.0f;
            m_scale[0] = m_scale[1] = m_scale[2] = 1.0f;

            // Update entity with reset values
            if (entity) {
                entity->SetPosition(glm::vec3(0, 0, 0));
                entity->SetRotation(glm::vec3(0, 0, 0));
                entity->SetScale(glm::vec3(1, 1, 1));
                m_editor->MarkDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            // Copy transform to clipboard
            memcpy(s_transformClipboard.position, m_position, sizeof(m_position));
            memcpy(s_transformClipboard.rotation, m_rotation, sizeof(m_rotation));
            memcpy(s_transformClipboard.scale, m_scale, sizeof(m_scale));
            s_transformClipboard.hasData = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste")) {
            // Paste transform from clipboard
            if (s_transformClipboard.hasData) {
                memcpy(m_position, s_transformClipboard.position, sizeof(m_position));
                memcpy(m_rotation, s_transformClipboard.rotation, sizeof(m_rotation));
                memcpy(m_scale, s_transformClipboard.scale, sizeof(m_scale));

                // Apply to entity
                if (entity) {
                    entity->SetPosition(glm::vec3(m_position[0], m_position[1], m_position[2]));
                    entity->SetRotation(glm::vec3(
                        glm::radians(m_rotation[0]),
                        glm::radians(m_rotation[1]),
                        glm::radians(m_rotation[2])
                    ));
                    entity->SetScale(glm::vec3(m_scale[0], m_scale[1], m_scale[2]));
                    m_editor->MarkDirty();
                }
            }
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
                // Remove component from entity
                m_components.erase(m_components.begin() + i);
                if (m_editor) m_editor->MarkDirty();
                ImGui::EndPopup();
                ImGui::PopID();
                return;  // Exit early since we modified the vector
            }
            if (ImGui::MenuItem("Reset")) {
                // Reset component to defaults
                comp.enabled = true;
                // In a full implementation, reset component-specific values
                if (m_editor) m_editor->MarkDirty();
            }
            if (ImGui::MenuItem("Copy")) {
                // Copy component to clipboard
                s_componentClipboard.type = comp.type;
                s_componentClipboard.name = comp.name;
                s_componentClipboard.hasData = true;
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
                    // Open script browser popup
                    ImGui::OpenPopup("ScriptBrowserAI");
                }

                // Script browser popup
                if (ImGui::BeginPopup("ScriptBrowserAI")) {
                    ImGui::Text("Select AI Script");
                    ImGui::Separator();

                    // Predefined AI scripts
                    const char* aiScripts[] = {
                        "scripts/ai/idle.py",
                        "scripts/ai/patrol.py",
                        "scripts/ai/guard.py",
                        "scripts/ai/aggressive.py",
                        "scripts/ai/flee.py"
                    };
                    for (const char* script : aiScripts) {
                        if (ImGui::Selectable(script)) {
                            strncpy(scriptPath, script, sizeof(scriptPath) - 1);
                            if (m_editor) m_editor->MarkDirty();
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            else if (comp.type == "Scriptable") {
                static char scriptPath[256] = "";
                ImGui::InputText("Script Path", scriptPath, sizeof(scriptPath));
                ImGui::SameLine();
                if (ImGui::Button("Edit")) {
                    // Open script in the script editor
                    if (strlen(scriptPath) > 0 && m_editor) {
                        if (auto* scriptEditor = m_editor->GetScriptEditor()) {
                            scriptEditor->OpenScript(scriptPath);
                        }
                        m_editor->SetScriptEditorVisible(true);
                    }
                }

                if (ImGui::TreeNode("Script Variables")) {
                    // Show script variables from the Python engine
                    // In a full implementation, query the script context for exposed variables
                    bool hasVars = false;

                    // Example exposed variables (would be queried from script runtime)
                    struct ScriptVar {
                        const char* name;
                        const char* type;
                        const char* value;
                    };
                    ScriptVar exampleVars[] = {
                        {"health", "int", "100"},
                        {"speed", "float", "5.0"},
                        {"target_id", "int", "0"},
                    };

                    if (strlen(scriptPath) > 0) {
                        // Show variables for this script
                        ImGui::Columns(3, "ScriptVarsColumns");
                        ImGui::Text("Name"); ImGui::NextColumn();
                        ImGui::Text("Type"); ImGui::NextColumn();
                        ImGui::Text("Value"); ImGui::NextColumn();
                        ImGui::Separator();

                        for (const auto& var : exampleVars) {
                            ImGui::Text("%s", var.name); ImGui::NextColumn();
                            ImGui::TextDisabled("%s", var.type); ImGui::NextColumn();
                            ImGui::Text("%s", var.value); ImGui::NextColumn();
                            hasVars = true;
                        }
                        ImGui::Columns(1);
                    }

                    if (!hasVars) {
                        ImGui::TextDisabled("No variables exposed");
                    }
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

    // Load entity data from entity manager
    if (m_editor && m_editor->GetEntityManager()) {
        auto* entity = m_editor->GetEntityManager()->GetEntity(entityId);
        if (entity) {
            // Get entity name and type
            m_entityName = entity->GetName();
            if (m_entityName.empty()) {
                m_entityName = "Entity_" + std::to_string(entityId);
            }
            m_entityType = entity->GetTypeName();

            // Get transform
            glm::vec3 pos = entity->GetPosition();
            m_position[0] = pos.x;
            m_position[1] = pos.y;
            m_position[2] = pos.z;

            glm::vec3 rot = entity->GetEulerRotation();
            m_rotation[0] = glm::degrees(rot.x);
            m_rotation[1] = glm::degrees(rot.y);
            m_rotation[2] = glm::degrees(rot.z);

            glm::vec3 scale = entity->GetScale();
            m_scale[0] = scale.x;
            m_scale[1] = scale.y;
            m_scale[2] = scale.z;

            // Load components from entity
            m_components.clear();

            // Check for common components and add them
            if (entity->HasComponent("Health")) {
                m_components.push_back({"Health", "Health", true});
            }
            if (entity->HasComponent("Movement")) {
                m_components.push_back({"Movement", "Movement", true});
            }
            if (entity->HasComponent("Combat")) {
                m_components.push_back({"Combat", "Combat", true});
            }
            if (entity->HasComponent("AI")) {
                m_components.push_back({"AI", "AI", true});
            }
            if (entity->HasComponent("Scriptable")) {
                m_components.push_back({"Scriptable", "Scriptable", true});
            }
            if (entity->HasComponent("Physics")) {
                m_components.push_back({"Physics Body", "Physics", true});
            }

            return;
        }
    }

    // Fallback to defaults if entity not found
    m_entityName = "Entity_" + std::to_string(entityId);
    m_entityType = "unknown";

    m_position[0] = m_position[1] = m_position[2] = 0.0f;
    m_rotation[0] = m_rotation[1] = m_rotation[2] = 0.0f;
    m_scale[0] = m_scale[1] = m_scale[2] = 1.0f;

    // Default components
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
