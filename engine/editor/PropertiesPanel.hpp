/**
 * @file PropertiesPanel.hpp
 * @brief Comprehensive Properties Inspector panel for the Nova3D/Vehement editor
 *
 * Provides a unified property editing interface for:
 * - Scene nodes and transforms
 * - SDF primitives and materials
 * - Components (physics, scripts, etc.)
 * - Custom user-defined properties
 *
 * Features:
 * - Type-safe property editors with undo/redo
 * - Multi-object editing with mixed value support
 * - Collapsible property groups
 * - Search/filter functionality
 * - Custom property editor registration
 * - Dynamic property support for scripted components
 */

#pragma once

#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include "../scene/SceneNode.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../reflection/TypeInfo.hpp"
#include "EditorCommand.hpp"
#include "CommandHistory.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <any>
#include <variant>
#include <typeindex>
#include <optional>

namespace Nova {

// Forward declarations
class SceneNode;
class SDFPrimitive;
class Material;
class CommandHistory;

// =============================================================================
// Property Value Types
// =============================================================================

/**
 * @brief Variant type for property values
 */
using PropertyValue = std::variant<
    std::monostate,     // Empty/invalid
    bool,
    int,
    float,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::quat,
    glm::mat3,
    glm::mat4,
    std::vector<EditorWidgets::CurvePoint>,
    std::vector<EditorWidgets::GradientKey>,
    uint64_t            // Object reference ID
>;

/**
 * @brief Mixed value indicator for multi-edit
 */
struct MixedValue {
    static constexpr const char* DISPLAY_TEXT = "---";
};

// =============================================================================
// Property Editor Interface
// =============================================================================

/**
 * @brief Abstract interface for property editors
 *
 * Implement this interface to create custom property editors for specific types.
 * Property editors handle rendering, value getting/setting, and change detection.
 */
class IPropertyEditor {
public:
    virtual ~IPropertyEditor() = default;

    /**
     * @brief Render the property editor UI
     * @param label The property label/name
     * @param data Pointer to the data being edited
     * @return true if value was modified
     */
    virtual bool Render(const char* label, void* data) = 0;

    /**
     * @brief Set the property value
     * @param data Pointer to the target data
     * @param value The new value to set
     */
    virtual void SetValue(void* data, const std::any& value) = 0;

    /**
     * @brief Get the current property value
     * @param data Pointer to the source data
     * @return The current value
     */
    virtual std::any GetValue(void* data) const = 0;

    /**
     * @brief Check if the value has changed since last frame
     * @return true if value changed
     */
    virtual bool HasChanged() const = 0;

    /**
     * @brief Reset change tracking
     */
    virtual void ClearChanged() = 0;

    /**
     * @brief Check if this editor supports multi-edit
     * @return true if multi-edit is supported
     */
    virtual bool SupportsMultiEdit() const { return true; }

    /**
     * @brief Set mixed value state for multi-edit
     * @param isMixed true if multiple different values are selected
     */
    virtual void SetMixedValue(bool isMixed) { m_isMixed = isMixed; }

    /**
     * @brief Check if currently showing mixed value
     */
    [[nodiscard]] bool IsMixed() const { return m_isMixed; }

    /**
     * @brief Get the type this editor handles
     */
    [[nodiscard]] virtual std::type_index GetHandledType() const = 0;

    /**
     * @brief Clone this editor instance
     */
    [[nodiscard]] virtual std::unique_ptr<IPropertyEditor> Clone() const = 0;

protected:
    bool m_isMixed = false;
    bool m_changed = false;
};

// =============================================================================
// Built-in Property Editors
// =============================================================================

/**
 * @brief Float property editor with drag support
 */
class FloatEditor : public IPropertyEditor {
public:
    FloatEditor(float min = -FLT_MAX, float max = FLT_MAX, float step = 0.1f, const char* format = "%.3f");

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(float); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetRange(float min, float max) { m_min = min; m_max = max; }
    void SetStep(float step) { m_step = step; }
    void SetFormat(const char* format) { m_format = format; }

private:
    float m_min;
    float m_max;
    float m_step;
    const char* m_format;
};

/**
 * @brief Integer property editor
 */
class IntEditor : public IPropertyEditor {
public:
    IntEditor(int min = INT_MIN, int max = INT_MAX, const char* format = "%d");

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(int); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetRange(int min, int max) { m_min = min; m_max = max; }

private:
    int m_min;
    int m_max;
    const char* m_format;
};

/**
 * @brief Boolean property editor (checkbox)
 */
class BoolEditor : public IPropertyEditor {
public:
    BoolEditor() = default;

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(bool); }
    std::unique_ptr<IPropertyEditor> Clone() const override;
};

/**
 * @brief String property editor (text field)
 */
class StringEditor : public IPropertyEditor {
public:
    StringEditor(size_t maxLength = 256, bool multiline = false);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(std::string); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetMultiline(bool multiline) { m_multiline = multiline; }
    void SetMaxLength(size_t length) { m_maxLength = length; }

private:
    size_t m_maxLength;
    bool m_multiline;
    char m_buffer[4096] = "";
};

/**
 * @brief Vec2 property editor
 */
class Vec2Editor : public IPropertyEditor {
public:
    Vec2Editor(float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(glm::vec2); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

private:
    float m_min;
    float m_max;
    float m_speed;
};

/**
 * @brief Vec3 property editor
 */
class Vec3Editor : public IPropertyEditor {
public:
    Vec3Editor(float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(glm::vec3); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

private:
    float m_min;
    float m_max;
    float m_speed;
};

/**
 * @brief Vec4 property editor
 */
class Vec4Editor : public IPropertyEditor {
public:
    Vec4Editor(float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(glm::vec4); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

private:
    float m_min;
    float m_max;
    float m_speed;
};

/**
 * @brief Color property editor with color picker
 */
class ColorEditor : public IPropertyEditor {
public:
    ColorEditor(bool showAlpha = false, bool hdr = false);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override;
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetShowAlpha(bool show) { m_showAlpha = show; }
    void SetHDR(bool hdr) { m_hdr = hdr; }

private:
    bool m_showAlpha;
    bool m_hdr;
};

/**
 * @brief Enum property editor (dropdown)
 */
class EnumEditor : public IPropertyEditor {
public:
    EnumEditor(const std::vector<std::string>& names);
    EnumEditor(const char* const* names, int count);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(int); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetOptions(const std::vector<std::string>& names);

private:
    std::vector<std::string> m_names;
    std::vector<const char*> m_namePtrs;  // For C API compatibility
};

/**
 * @brief Object reference property editor (asset picker)
 */
class ObjectReferenceEditor : public IPropertyEditor {
public:
    ObjectReferenceEditor(const std::string& typeName = "", const std::string& filter = "*.*");

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(uint64_t); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetTypeName(const std::string& typeName) { m_typeName = typeName; }
    void SetFilter(const std::string& filter) { m_filter = filter; }

private:
    std::string m_typeName;
    std::string m_filter;
};

/**
 * @brief Transform property editor (position, rotation, scale)
 */
class TransformEditor : public IPropertyEditor {
public:
    TransformEditor();

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(TransformState); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetShowPosition(bool show) { m_showPosition = show; }
    void SetShowRotation(bool show) { m_showRotation = show; }
    void SetShowScale(bool show) { m_showScale = show; }
    void SetUseEuler(bool useEuler) { m_useEuler = useEuler; }

private:
    bool m_showPosition = true;
    bool m_showRotation = true;
    bool m_showScale = true;
    bool m_useEuler = true;  // Use Euler angles for rotation editing
    glm::vec3 m_cachedEuler{0.0f};  // Cached Euler angles to avoid gimbal lock issues
};

/**
 * @brief Curve property editor (animation curves)
 */
class CurveEditor : public IPropertyEditor {
public:
    CurveEditor(float minTime = 0.0f, float maxTime = 1.0f, float minValue = 0.0f, float maxValue = 1.0f);

    bool Render(const char* label, void* data) override;
    void SetValue(void* data, const std::any& value) override;
    std::any GetValue(void* data) const override;
    bool HasChanged() const override { return m_changed; }
    void ClearChanged() override { m_changed = false; }
    std::type_index GetHandledType() const override { return typeid(std::vector<EditorWidgets::CurvePoint>); }
    std::unique_ptr<IPropertyEditor> Clone() const override;

    void SetTimeRange(float min, float max) { m_minTime = min; m_maxTime = max; }
    void SetValueRange(float min, float max) { m_minValue = min; m_maxValue = max; }

private:
    float m_minTime;
    float m_maxTime;
    float m_minValue;
    float m_maxValue;
};

// =============================================================================
// Property Group
// =============================================================================

/**
 * @brief Represents a collapsible group of properties
 */
struct PropertyGroup {
    std::string name;
    std::string icon;
    std::string tooltip;
    bool expanded = true;
    bool visible = true;
    int order = 0;  // For sorting groups

    /**
     * @brief Property within a group
     */
    struct Property {
        std::string name;
        std::string displayName;
        std::string tooltip;
        std::type_index type;
        size_t offset = 0;
        std::unique_ptr<IPropertyEditor> editor;
        Reflect::PropertyAttribute attributes = Reflect::PropertyAttribute::None;
        bool visible = true;

        // Getter/setter for dynamic properties
        std::function<std::any(void*)> getter;
        std::function<void(void*, const std::any&)> setter;

        Property() : type(typeid(void)) {}
        Property(const std::string& n, std::type_index t) : name(n), displayName(n), type(t) {}
    };

    std::vector<Property> properties;

    /**
     * @brief Add a property to this group
     */
    void AddProperty(Property prop) {
        properties.push_back(std::move(prop));
    }

    /**
     * @brief Find property by name
     */
    Property* FindProperty(const std::string& propName) {
        for (auto& prop : properties) {
            if (prop.name == propName) return &prop;
        }
        return nullptr;
    }
};

// =============================================================================
// Property Change Command
// =============================================================================

/**
 * @brief Command for property changes with batching support
 */
class PropertyChangeCommand : public ICommand {
public:
    using Getter = std::function<std::any()>;
    using Setter = std::function<void(const std::any&)>;

    /**
     * @brief Create a property change command
     * @param description Human-readable description
     * @param target Pointer to target object (for merging)
     * @param propertyName Property being changed (for merging)
     * @param getter Function to get current value
     * @param setter Function to set value
     * @param newValue The new value
     */
    PropertyChangeCommand(
        const std::string& description,
        void* target,
        const std::string& propertyName,
        Getter getter,
        Setter setter,
        const std::any& newValue
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;
    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    std::string m_description;
    void* m_target;
    std::string m_propertyName;
    Getter m_getter;
    Setter m_setter;
    std::any m_oldValue;
    std::any m_newValue;
};

// =============================================================================
// Properties Panel
// =============================================================================

/**
 * @brief Comprehensive Properties Inspector panel
 *
 * Features:
 * - Single and multi-object editing
 * - Automatic property discovery via reflection
 * - Custom property editor registration
 * - Component sections with collapsible headers
 * - Search/filter functionality
 * - Undo/redo integration
 *
 * Usage:
 * @code
 * PropertiesPanel panel;
 * panel.Initialize(config);
 * panel.SetCommandHistory(&history);
 *
 * // Single selection
 * panel.SetTarget(selectedNode);
 *
 * // Multi-selection
 * panel.SetTargets(selectedNodes);
 *
 * // Register custom editor
 * panel.RegisterPropertyEditor<MyCustomType>(std::make_unique<MyCustomEditor>());
 * @endcode
 */
class PropertiesPanel : public EditorPanel {
public:
    PropertiesPanel();
    ~PropertiesPanel() override;

    // Non-copyable
    PropertiesPanel(const PropertiesPanel&) = delete;
    PropertiesPanel& operator=(const PropertiesPanel&) = delete;

    // =========================================================================
    // Target Management
    // =========================================================================

    /**
     * @brief Set single target object
     * @param node The scene node to inspect
     */
    void SetTarget(SceneNode* node);

    /**
     * @brief Set multiple target objects for multi-editing
     * @param nodes Vector of scene nodes to inspect
     */
    void SetTargets(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Clear current target(s)
     */
    void ClearTarget();

    /**
     * @brief Get current target (primary if multi-select)
     */
    [[nodiscard]] SceneNode* GetTarget() const;

    /**
     * @brief Get all current targets
     */
    [[nodiscard]] const std::vector<SceneNode*>& GetTargets() const { return m_targets; }

    /**
     * @brief Check if editing multiple objects
     */
    [[nodiscard]] bool IsMultiEdit() const { return m_targets.size() > 1; }

    /**
     * @brief Refresh/rebuild the property list
     */
    void Refresh();

    // =========================================================================
    // Property Editor Registration
    // =========================================================================

    /**
     * @brief Register a custom property editor for a type
     * @tparam T The type to register editor for
     * @param editor The editor instance
     */
    template<typename T>
    void RegisterPropertyEditor(std::unique_ptr<IPropertyEditor> editor) {
        RegisterPropertyEditorByType(typeid(T), std::move(editor));
    }

    /**
     * @brief Register property editor by type_index
     */
    void RegisterPropertyEditorByType(std::type_index type, std::unique_ptr<IPropertyEditor> editor);

    /**
     * @brief Unregister a custom property editor
     */
    template<typename T>
    void UnregisterPropertyEditor() {
        UnregisterPropertyEditorByType(typeid(T));
    }

    /**
     * @brief Unregister by type_index
     */
    void UnregisterPropertyEditorByType(std::type_index type);

    /**
     * @brief Get registered editor for type
     */
    [[nodiscard]] IPropertyEditor* GetPropertyEditor(std::type_index type) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set command history for undo/redo
     */
    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }

    /**
     * @brief Enable/disable property filtering
     */
    void SetFilterEnabled(bool enabled) { m_filterEnabled = enabled; }

    /**
     * @brief Set filter text
     */
    void SetFilter(const std::string& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const std::string& GetFilter() const { return m_filterText; }

    /**
     * @brief Clear filter
     */
    void ClearFilter();

    /**
     * @brief Show/hide read-only properties
     */
    void SetShowReadOnly(bool show) { m_showReadOnly = show; }

    /**
     * @brief Show/hide hidden properties (debug mode)
     */
    void SetShowHidden(bool show) { m_showHidden = show; }

    /**
     * @brief Enable/disable property change batching for drag operations
     */
    void SetBatchingEnabled(bool enabled) { m_batchingEnabled = enabled; }

    /**
     * @brief Set batching time window in milliseconds
     */
    void SetBatchingWindow(uint32_t ms) { m_batchingWindowMs = ms; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Called when a property value changes
     */
    std::function<void(const std::string& propertyName, const std::any& oldValue, const std::any& newValue)> OnPropertyChanged;

    /**
     * @brief Called when refresh is needed (external change)
     */
    std::function<void()> OnRefreshRequested;

protected:
    // =========================================================================
    // EditorPanel Overrides
    // =========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnSearchChanged(const std::string& filter) override;
    void OnInitialize() override;
    void OnShutdown() override;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Property group building
    void RebuildPropertyGroups();
    void BuildTransformGroup();
    void BuildSDFPrimitiveGroup();
    void BuildMaterialGroup();
    void BuildPhysicsGroup();
    void BuildScriptGroup();
    void BuildCustomComponentGroups();
    void BuildReflectedProperties(const Reflect::TypeInfo* typeInfo, void* instance, const std::string& category);

    // Rendering
    void RenderPropertyGroups();
    void RenderPropertyGroup(PropertyGroup& group);
    void RenderProperty(PropertyGroup::Property& prop, void* data);
    void RenderMixedValueIndicator();
    void RenderNoSelection();
    void RenderMultiEditHeader();

    // Filtering
    void ApplyFilter();
    bool MatchesFilter(const std::string& name) const;
    void HighlightFilterMatch(const std::string& text);

    // Value handling
    std::any GetPropertyValue(const PropertyGroup::Property& prop, void* data) const;
    void SetPropertyValue(PropertyGroup::Property& prop, void* data, const std::any& value);
    bool AreValuesEqual(const std::any& a, const std::any& b) const;
    bool CheckMixedValues(const PropertyGroup::Property& prop) const;

    // Undo/redo
    void RecordPropertyChange(const std::string& propName, void* target, const std::any& oldValue, const std::any& newValue);
    void BeginPropertyEdit(const std::string& propName);
    void EndPropertyEdit();

    // Editor creation
    std::unique_ptr<IPropertyEditor> CreateDefaultEditor(std::type_index type) const;
    void RegisterDefaultEditors();

    // Utility
    void* GetDataPointer(SceneNode* node, size_t offset) const;
    SDFPrimitive* GetSDFPrimitive(SceneNode* node) const;
    Material* GetMaterial(SceneNode* node) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Targets
    std::vector<SceneNode*> m_targets;
    bool m_needsRebuild = true;

    // Property groups
    std::vector<PropertyGroup> m_propertyGroups;

    // Custom editors by type
    std::unordered_map<std::type_index, std::unique_ptr<IPropertyEditor>> m_customEditors;

    // Filter
    std::string m_filterText;
    char m_filterBuffer[256] = "";
    bool m_filterEnabled = true;
    bool m_hasActiveFilter = false;

    // Display options
    bool m_showReadOnly = true;
    bool m_showHidden = false;

    // Undo/redo
    CommandHistory* m_commandHistory = nullptr;
    bool m_batchingEnabled = true;
    uint32_t m_batchingWindowMs = 500;
    std::string m_currentEditProperty;
    bool m_isEditing = false;
    std::chrono::steady_clock::time_point m_editStartTime;

    // UI State
    float m_labelWidth = 120.0f;
    bool m_lockIcon = false;

    // Cached data for multi-edit
    struct CachedPropertyValue {
        std::any value;
        bool isMixed = false;
    };
    std::unordered_map<std::string, CachedPropertyValue> m_cachedValues;
};

// =============================================================================
// Property Editor Factory
// =============================================================================

/**
 * @brief Factory for creating property editors
 */
class PropertyEditorFactory {
public:
    static PropertyEditorFactory& Instance();

    /**
     * @brief Register a factory function for a type
     */
    template<typename T>
    void Register(std::function<std::unique_ptr<IPropertyEditor>()> factory) {
        RegisterByType(typeid(T), std::move(factory));
    }

    void RegisterByType(std::type_index type, std::function<std::unique_ptr<IPropertyEditor>()> factory);

    /**
     * @brief Create an editor for a type
     */
    [[nodiscard]] std::unique_ptr<IPropertyEditor> Create(std::type_index type) const;

    template<typename T>
    [[nodiscard]] std::unique_ptr<IPropertyEditor> Create() const {
        return Create(typeid(T));
    }

    /**
     * @brief Check if type has registered factory
     */
    [[nodiscard]] bool HasFactory(std::type_index type) const;

private:
    PropertyEditorFactory();
    std::unordered_map<std::type_index, std::function<std::unique_ptr<IPropertyEditor>()>> m_factories;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get display name from property name (e.g., "myProperty" -> "My Property")
 */
std::string GetPropertyDisplayName(const std::string& propertyName);

/**
 * @brief Convert Euler angles to quaternion
 */
glm::quat EulerToQuat(const glm::vec3& eulerDegrees);

/**
 * @brief Convert quaternion to Euler angles
 */
glm::vec3 QuatToEuler(const glm::quat& q);

} // namespace Nova
