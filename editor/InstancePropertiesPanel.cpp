#include "InstancePropertiesPanel.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Nova3D {

InstancePropertiesPanel::InstancePropertiesPanel() {
}

InstancePropertiesPanel::~InstancePropertiesPanel() {
    Shutdown();
}

void InstancePropertiesPanel::Initialize() {
    // Create property container for instances
    m_instanceProperties = PropertySystem::Instance().CreateInstanceContainer(nullptr);
}

void InstancePropertiesPanel::Shutdown() {
}

void InstancePropertiesPanel::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    if (m_selectedInstances.empty()) {
        ImGui::Begin("Instance Properties", &m_isOpen);
        ImGui::Text("No instance selected");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("Instance Properties", &m_isOpen);

    // Instance header
    RenderInstanceHeader();

    ImGui::Separator();

    // Bulk edit info
    if (IsBulkEditing()) {
        RenderBulkEditInfo();
        ImGui::Separator();
    }

    // Inheritance controls
    RenderInheritanceControls();

    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("InstanceTabs")) {
        if (ImGui::BeginTabItem("Transform")) {
            RenderTransformTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material")) {
            RenderMaterialTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering")) {
            RenderRenderingTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("LOD")) {
            RenderLODTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics")) {
            RenderPhysicsTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Action buttons
    RenderActionButtons();

    ImGui::Separator();

    // Status bar
    RenderStatusBar();

    ImGui::End();
}

void InstancePropertiesPanel::RenderInstanceHeader() {
    if (IsBulkEditing()) {
        ImGui::Text("Selected Instances: %zu", m_selectedInstances.size());
    } else {
        ImGui::Text("Instance: Entity #1");
        ImGui::Text("Asset: AssetName");
    }
}

void InstancePropertiesPanel::RenderBulkEditInfo() {
    PropertyOverrideUI::BulkEditContext context;
    context.enabled = true;
    context.selectionCount = static_cast<int>(m_selectedInstances.size());
    context.containers.reserve(m_selectedInstances.size());
    // for (auto* entity : m_selectedInstances) {
    //     context.containers.push_back(entity->GetPropertyContainer());
    // }

    PropertyOverrideUI::BeginBulkEdit(context);
    PropertyOverrideUI::EndBulkEdit();
}

void InstancePropertiesPanel::RenderInheritanceControls() {
    ImGui::Checkbox("Show Only Overridden", &m_showOnlyOverridden);

    ImGui::SameLine();
    if (ImGui::Button("Inherit All from Asset")) {
        ResetToAssetDefaults();
    }
}

void InstancePropertiesPanel::RenderActionButtons() {
    if (ImGui::Button("Reset All")) {
        ResetAllProperties();
    }

    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        CopyProperties();
    }

    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        PasteProperties();
    }

    if (IsBulkEditing()) {
        ImGui::SameLine();
        if (ImGui::Button("Match First Selected")) {
            // Match all selected to first selected
        }
    }
}

void InstancePropertiesPanel::RenderStatusBar() {
    ImGui::Text("Modified: %s",
               m_instanceProperties && m_instanceProperties->HasDirtyProperties() ? "Yes" : "No");
}

void InstancePropertiesPanel::RenderTransformTab() {
    PropertyOverrideUI::BeginCategory("Position");
    RenderPositionControls();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Rotation");
    RenderRotationControls();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Scale");
    RenderScaleControls();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Presets");
    RenderTransformPresets();
    PropertyOverrideUI::EndCategory();
}

void InstancePropertiesPanel::RenderPositionControls() {
    PropertyOverrideUI::RenderVec3(
        "Position",
        m_tempValues.position,
        m_instanceProperties,
        PropertyLevel::Instance,
        [this](glm::vec3 pos) {
            ApplyToAllSelected([pos](Entity* entity) {
                // entity->GetTransform()->SetPosition(pos);
            });
        },
        -1000.0f, 1000.0f,
        "World position"
    );

    // Individual axis controls
    PropertyOverrideUI::RenderFloat(
        "X",
        m_tempValues.position.x,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        -1000.0f, 1000.0f
    );

    PropertyOverrideUI::RenderFloat(
        "Y",
        m_tempValues.position.y,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        -1000.0f, 1000.0f
    );

    PropertyOverrideUI::RenderFloat(
        "Z",
        m_tempValues.position.z,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        -1000.0f, 1000.0f
    );
}

void InstancePropertiesPanel::RenderRotationControls() {
    PropertyOverrideUI::RenderVec3(
        "Rotation (Euler)",
        m_tempValues.rotation,
        m_instanceProperties,
        PropertyLevel::Instance,
        [this](glm::vec3 rot) {
            ApplyToAllSelected([rot](Entity* entity) {
                // glm::quat quat = glm::quat(glm::radians(rot));
                // entity->GetTransform()->SetRotation(quat);
            });
        },
        -180.0f, 180.0f,
        "Rotation in degrees (XYZ)"
    );

    // Individual axis controls
    PropertyOverrideUI::RenderAngle(
        "X (Pitch)",
        m_tempValues.rotation.x,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Rotation around X axis"
    );

    PropertyOverrideUI::RenderAngle(
        "Y (Yaw)",
        m_tempValues.rotation.y,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Rotation around Y axis"
    );

    PropertyOverrideUI::RenderAngle(
        "Z (Roll)",
        m_tempValues.rotation.z,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Rotation around Z axis"
    );
}

void InstancePropertiesPanel::RenderScaleControls() {
    ImGui::Checkbox("Link Scale", &m_linkScale);
    PropertyOverrideUI::HelpMarker("When enabled, scaling one axis scales all axes uniformly");

    if (m_linkScale) {
        float uniformScale = m_tempValues.scale.x;
        if (PropertyOverrideUI::RenderFloat(
            "Uniform Scale",
            uniformScale,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.01f, 100.0f
        )) {
            m_tempValues.scale = glm::vec3(uniformScale);
        }
    } else {
        PropertyOverrideUI::RenderVec3(
            "Scale",
            m_tempValues.scale,
            m_instanceProperties,
            PropertyLevel::Instance,
            [this](glm::vec3 scale) {
                ApplyToAllSelected([scale](Entity* entity) {
                    // entity->GetTransform()->SetScale(scale);
                });
            },
            0.01f, 100.0f,
            "Local scale"
        );

        // Individual axis controls
        PropertyOverrideUI::RenderFloat(
            "X",
            m_tempValues.scale.x,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.01f, 100.0f
        );

        PropertyOverrideUI::RenderFloat(
            "Y",
            m_tempValues.scale.y,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.01f, 100.0f
        );

        PropertyOverrideUI::RenderFloat(
            "Z",
            m_tempValues.scale.z,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.01f, 100.0f
        );
    }
}

void InstancePropertiesPanel::RenderTransformPresets() {
    if (ImGui::Button("Reset Position")) {
        m_tempValues.position = glm::vec3(0.0f);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Rotation")) {
        m_tempValues.rotation = glm::vec3(0.0f);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Scale")) {
        m_tempValues.scale = glm::vec3(1.0f);
    }

    if (ImGui::Button("Reset All Transform")) {
        m_tempValues.position = glm::vec3(0.0f);
        m_tempValues.rotation = glm::vec3(0.0f);
        m_tempValues.scale = glm::vec3(1.0f);
    }
}

void InstancePropertiesPanel::RenderMaterialTab() {
    PropertyOverrideUI::BeginCategory("Material Override");
    RenderMaterialOverrides();
    PropertyOverrideUI::EndCategory();
}

void InstancePropertiesPanel::RenderMaterialOverrides() {
    PropertyOverrideUI::RenderBool(
        "Override Material",
        m_tempValues.overrideMaterial,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Override asset's default material"
    );

    if (m_tempValues.overrideMaterial) {
        ImGui::Text("Material Override:");

        if (ImGui::BeginCombo("Material", m_tempValues.materialOverride ? "Custom Material" : "None")) {
            if (ImGui::Selectable("Material 1")) {
                // Set material override
            }
            if (ImGui::Selectable("Material 2")) {
                // Set material override
            }
            if (ImGui::Selectable("None")) {
                m_tempValues.materialOverride = nullptr;
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Inherit from Asset")) {
            m_tempValues.overrideMaterial = false;
            m_tempValues.materialOverride = nullptr;
        }
    }
}

void InstancePropertiesPanel::RenderRenderingTab() {
    PropertyOverrideUI::BeginCategory("Rendering Overrides");
    RenderRenderingOverrides();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Instance Flags");
    RenderInstanceFlags();
    PropertyOverrideUI::EndCategory();
}

void InstancePropertiesPanel::RenderRenderingOverrides() {
    // Shadow overrides
    PropertyOverrideUI::RenderBool(
        "Override Cast Shadows",
        m_tempValues.overrideCastShadows,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Override asset's shadow casting setting"
    );

    if (m_tempValues.overrideCastShadows) {
        ImGui::Indent();
        PropertyOverrideUI::RenderBool(
            "Cast Shadows",
            m_tempValues.castShadows,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            "Cast shadows"
        );
        ImGui::Unindent();
    }

    PropertyOverrideUI::RenderBool(
        "Override Receive Shadows",
        m_tempValues.overrideReceiveShadows,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Override asset's shadow receiving setting"
    );

    if (m_tempValues.overrideReceiveShadows) {
        ImGui::Indent();
        PropertyOverrideUI::RenderBool(
            "Receive Shadows",
            m_tempValues.receiveShadows,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            "Receive shadows"
        );
        ImGui::Unindent();
    }

    // Visibility override
    PropertyOverrideUI::RenderBool(
        "Override Visibility",
        m_tempValues.overrideVisibility,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Override asset's visibility"
    );

    if (m_tempValues.overrideVisibility) {
        ImGui::Indent();
        PropertyOverrideUI::RenderBool(
            "Is Visible",
            m_tempValues.isVisible,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            "Visible in scene"
        );
        ImGui::Unindent();
    }
}

void InstancePropertiesPanel::RenderInstanceFlags() {
    ImGui::Text("Instance-specific rendering flags");

    PropertyOverrideUI::RenderBool(
        "Static",
        m_tempValues.physicsEnabled,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Mark as static for optimization"
    );
}

void InstancePropertiesPanel::RenderLODTab() {
    PropertyOverrideUI::BeginCategory("LOD Overrides");
    RenderLODOverrides();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("LOD Distance Overrides");
    RenderLODDistanceOverrides();
    PropertyOverrideUI::EndCategory();
}

void InstancePropertiesPanel::RenderLODOverrides() {
    PropertyOverrideUI::RenderBool(
        "Override LOD Distances",
        m_tempValues.overrideLODDistances,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Override asset's LOD distances"
    );

    if (m_tempValues.overrideLODDistances) {
        PropertyOverrideUI::RenderFloat(
            "LOD Bias",
            m_tempValues.lodBias,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            -2.0f, 2.0f,
            "Bias LOD selection (negative = higher quality, positive = lower quality)"
        );
    }
}

void InstancePropertiesPanel::RenderLODDistanceOverrides() {
    if (!m_tempValues.overrideLODDistances) {
        ImGui::TextDisabled("Enable LOD override to customize distances");
        return;
    }

    for (size_t i = 0; i < m_tempValues.lodDistances.size(); ++i) {
        char label[64];
        snprintf(label, sizeof(label), "LOD %zu Distance", i);

        PropertyOverrideUI::RenderFloat(
            label,
            m_tempValues.lodDistances[i],
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.0f, 1000.0f
        );
    }
}

void InstancePropertiesPanel::RenderPhysicsTab() {
    PropertyOverrideUI::BeginCategory("Physics Settings");
    RenderPhysicsSettings();
    PropertyOverrideUI::EndCategory();
}

void InstancePropertiesPanel::RenderPhysicsSettings() {
    PropertyOverrideUI::RenderBool(
        "Physics Enabled",
        m_tempValues.physicsEnabled,
        m_instanceProperties,
        PropertyLevel::Instance,
        nullptr,
        "Enable physics simulation"
    );

    if (m_tempValues.physicsEnabled) {
        PropertyOverrideUI::RenderFloat(
            "Mass",
            m_tempValues.mass,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            0.1f, 1000.0f,
            "Mass in kilograms"
        );

        PropertyOverrideUI::RenderBool(
            "Is Kinematic",
            m_tempValues.isKinematic,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            "Kinematic objects are not affected by forces"
        );

        PropertyOverrideUI::RenderBool(
            "Is Trigger",
            m_tempValues.isTrigger,
            m_instanceProperties,
            PropertyLevel::Instance,
            nullptr,
            "Trigger volumes don't collide but generate events"
        );
    }
}

void InstancePropertiesPanel::SetSelectedInstance(Entity* entity) {
    m_selectedInstances.clear();
    if (entity) {
        m_selectedInstances.push_back(entity);
    }
}

void InstancePropertiesPanel::SetSelectedInstances(const std::vector<Entity*>& entities) {
    m_selectedInstances = entities;
}

Entity* InstancePropertiesPanel::GetSelectedInstance() const {
    return m_selectedInstances.empty() ? nullptr : m_selectedInstances[0];
}

void InstancePropertiesPanel::ResetToAssetDefaults() {
    if (!m_instanceProperties) return;

    // Reset all properties to asset defaults
    auto properties = m_instanceProperties->GetAllProperties();
    for (const auto& prop : properties) {
        // Reset to asset level
    }
}

void InstancePropertiesPanel::ResetAllProperties() {
    m_tempValues.position = glm::vec3(0.0f);
    m_tempValues.rotation = glm::vec3(0.0f);
    m_tempValues.scale = glm::vec3(1.0f);

    m_tempValues.overrideMaterial = false;
    m_tempValues.overrideCastShadows = false;
    m_tempValues.overrideReceiveShadows = false;
    m_tempValues.overrideVisibility = false;
    m_tempValues.overrideLODDistances = false;
}

void InstancePropertiesPanel::ApplyTransform() {
    // Apply transform to selected instances
}

void InstancePropertiesPanel::CopyProperties() {
    if (m_selectedInstances.empty()) return;

    m_clipboard.hasData = true;
    m_clipboard.position = m_tempValues.position;
    m_clipboard.scale = m_tempValues.scale;
    // Convert euler to quat
    m_clipboard.rotation = glm::quat(glm::radians(m_tempValues.rotation));
}

void InstancePropertiesPanel::PasteProperties() {
    if (!m_clipboard.hasData) return;

    m_tempValues.position = m_clipboard.position;
    m_tempValues.scale = m_clipboard.scale;
    // Convert quat to euler
    glm::vec3 euler = glm::degrees(glm::eulerAngles(m_clipboard.rotation));
    m_tempValues.rotation = euler;

    ApplyTransform();
}

void InstancePropertiesPanel::ApplyToAllSelected(const std::function<void(Entity*)>& func) {
    for (auto* entity : m_selectedInstances) {
        if (entity) {
            func(entity);
        }
    }
}

} // namespace Nova3D
