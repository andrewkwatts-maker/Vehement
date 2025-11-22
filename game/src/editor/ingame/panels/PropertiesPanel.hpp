#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace Vehement {

class InGameEditor;

/**
 * @brief Property value types
 */
using PropertyValue = std::variant<
    bool,
    int,
    float,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4
>;

/**
 * @brief Property definition
 */
struct PropertyDef {
    std::string name;
    std::string displayName;
    std::string category;
    std::string tooltip;
    PropertyValue value;
    PropertyValue minValue;
    PropertyValue maxValue;
    bool readOnly = false;
    bool visible = true;
};

/**
 * @brief Properties Panel - Display and edit selected object properties
 *
 * Features:
 * - Transform editing (position, rotation, scale)
 * - Custom properties
 * - Behavior settings
 * - Visual options
 */
class PropertiesPanel {
public:
    PropertiesPanel();
    ~PropertiesPanel();

    void Initialize(InGameEditor& editor);
    void Shutdown();

    void Update(float deltaTime);
    void Render();

    // Property editing
    void SetProperties(const std::vector<PropertyDef>& properties);
    void ClearProperties();

    // Selection info
    void SetSelectionInfo(const std::string& type, const std::string& name, uint32_t id);
    void ClearSelection();

    // Callbacks
    std::function<void(const std::string&, const PropertyValue&)> OnPropertyChanged;

private:
    void RenderTransform();
    void RenderCustomProperties();
    void RenderBehaviorSettings();
    void RenderVisualSettings();

    bool RenderProperty(PropertyDef& prop);
    bool RenderBoolProperty(const std::string& name, bool& value);
    bool RenderIntProperty(const std::string& name, int& value, int min, int max);
    bool RenderFloatProperty(const std::string& name, float& value, float min, float max);
    bool RenderStringProperty(const std::string& name, std::string& value);
    bool RenderVec2Property(const std::string& name, glm::vec2& value);
    bool RenderVec3Property(const std::string& name, glm::vec3& value);
    bool RenderVec4Property(const std::string& name, glm::vec4& value);
    bool RenderColorProperty(const std::string& name, glm::vec4& value);

    InGameEditor* m_editor = nullptr;

    // Selection state
    std::string m_selectionType;
    std::string m_selectionName;
    uint32_t m_selectionId = 0;

    // Transform (special handling)
    glm::vec3 m_position = glm::vec3(0.0f);
    glm::vec3 m_rotation = glm::vec3(0.0f);
    glm::vec3 m_scale = glm::vec3(1.0f);

    // Custom properties
    std::vector<PropertyDef> m_properties;

    // UI state
    bool m_showTransform = true;
    bool m_showCustomProps = true;
    bool m_showBehavior = true;
    bool m_showVisual = true;
    std::string m_filterText;
};

} // namespace Vehement
