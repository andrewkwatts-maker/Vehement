#pragma once

#include "../engine/core/PropertySystem.hpp"
#include "PropertyOverrideUI.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Nova3D {

// Forward declarations
class Entity;
class Transform;
class Material;

// Instance properties panel
class InstancePropertiesPanel {
public:
    InstancePropertiesPanel();
    ~InstancePropertiesPanel();

    void Initialize();
    void Shutdown();

    void RenderUI();

    // Instance management
    void SetSelectedInstance(Entity* entity);
    void SetSelectedInstances(const std::vector<Entity*>& entities);
    Entity* GetSelectedInstance() const;
    const std::vector<Entity*>& GetSelectedInstances() const { return m_selectedInstances; }

    // Panel state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

    // Bulk editing
    bool IsBulkEditing() const { return m_selectedInstances.size() > 1; }

private:
    // Tab rendering
    void RenderTransformTab();
    void RenderMaterialTab();
    void RenderRenderingTab();
    void RenderLODTab();
    void RenderPhysicsTab();

    // Transform
    void RenderPositionControls();
    void RenderRotationControls();
    void RenderScaleControls();
    void RenderTransformPresets();

    // Material
    void RenderMaterialOverrides();
    void RenderInheritanceControls();

    // Rendering
    void RenderRenderingOverrides();
    void RenderInstanceFlags();

    // LOD
    void RenderLODOverrides();
    void RenderLODDistanceOverrides();

    // Physics
    void RenderPhysicsSettings();

    // UI helpers
    void RenderInstanceHeader();
    void RenderInheritanceControls();
    void RenderActionButtons();
    void RenderBulkEditInfo();
    void RenderStatusBar();

    // Actions
    void ResetToAssetDefaults();
    void ResetAllProperties();
    void ApplyTransform();
    void CopyProperties();
    void PasteProperties();

    // Bulk edit helpers
    void ApplyToAllSelected(const std::function<void(Entity*)>& func);

private:
    bool m_isOpen = true;

    // Selected instances
    std::vector<Entity*> m_selectedInstances;
    PropertyContainer* m_instanceProperties = nullptr;

    // UI state
    bool m_showOnlyOverridden = false;
    bool m_linkScale = true;

    // Clipboard
    struct PropertyClipboard {
        bool hasData = false;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    } m_clipboard;

    // Temporary values for editing
    struct {
        // Transform
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);  // Euler angles in degrees
        glm::vec3 scale = glm::vec3(1.0f);

        // Material overrides
        bool overrideMaterial = false;
        Material* materialOverride = nullptr;

        // Rendering overrides
        bool overrideCastShadows = false;
        bool castShadows = true;

        bool overrideReceiveShadows = false;
        bool receiveShadows = true;

        bool overrideVisibility = false;
        bool isVisible = true;

        // LOD overrides
        bool overrideLODDistances = false;
        std::vector<float> lodDistances;
        float lodBias = 0.0f;

        // Physics
        bool physicsEnabled = false;
        float mass = 1.0f;
        bool isKinematic = false;
        bool isTrigger = false;
    } m_tempValues;
};

} // namespace Nova3D
