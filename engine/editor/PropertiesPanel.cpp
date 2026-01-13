/**
 * @file PropertiesPanel.cpp
 * @brief Implementation of the Properties Inspector panel
 */

#include "PropertiesPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include "../scene/SceneNode.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../graphics/Material.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace Nova {

// =============================================================================
// Utility Functions
// =============================================================================

std::string GetPropertyDisplayName(const std::string& propertyName) {
    if (propertyName.empty()) return "";

    std::string result;
    result.reserve(propertyName.size() + 8);

    bool lastWasLower = false;
    bool lastWasDigit = false;

    for (size_t i = 0; i < propertyName.size(); ++i) {
        char c = propertyName[i];

        // Skip common prefixes like m_, s_, g_
        if (i == 0 && (c == 'm' || c == 's' || c == 'g') &&
            propertyName.size() > 2 && propertyName[1] == '_') {
            ++i;  // Skip the underscore too
            continue;
        }

        if (c == '_') {
            result += ' ';
            lastWasLower = false;
            lastWasDigit = false;
        } else if (std::isupper(c)) {
            // Insert space before uppercase if previous was lowercase
            if (lastWasLower || lastWasDigit) {
                result += ' ';
            }
            result += c;
            lastWasLower = false;
            lastWasDigit = false;
        } else if (std::isdigit(c)) {
            // Insert space before digit if previous was letter
            if (lastWasLower && !lastWasDigit) {
                result += ' ';
            }
            result += c;
            lastWasLower = false;
            lastWasDigit = true;
        } else {
            if (result.empty() || result.back() == ' ') {
                result += static_cast<char>(std::toupper(c));
            } else {
                result += c;
            }
            lastWasLower = true;
            lastWasDigit = false;
        }
    }

    // Capitalize first letter
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(result[0]));
    }

    return result;
}

glm::quat EulerToQuat(const glm::vec3& eulerDegrees) {
    glm::vec3 radians = glm::radians(eulerDegrees);
    return glm::quat(radians);
}

glm::vec3 QuatToEuler(const glm::quat& q) {
    return glm::degrees(glm::eulerAngles(q));
}

// =============================================================================
// FloatEditor Implementation
// =============================================================================

FloatEditor::FloatEditor(float min, float max, float step, const char* format)
    : m_min(min), m_max(max), m_step(step), m_format(format) {}

bool FloatEditor::Render(const char* label, void* data) {
    if (!data) return false;

    float* value = static_cast<float*>(data);
    float oldValue = *value;

    if (m_isMixed) {
        // Show mixed value indicator
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s", MixedValue::DISPLAY_TEXT);
        bool changed = EditorWidgets::Property(label, buffer, sizeof(buffer));
        // If user starts typing, treat as non-mixed
        if (changed && std::strlen(buffer) > 0 && buffer[0] != '-') {
            *value = std::strtof(buffer, nullptr);
            m_isMixed = false;
            m_changed = true;
            return true;
        }
        return false;
    }

    m_changed = EditorWidgets::Property(label, *value, m_min, m_max, m_step, m_format);
    return m_changed;
}

void FloatEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<float*>(data) = std::any_cast<float>(value);
    } catch (const std::bad_any_cast&) {
        // Try double conversion
        try {
            *static_cast<float*>(data) = static_cast<float>(std::any_cast<double>(value));
        } catch (...) {}
    }
}

std::any FloatEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<float*>(data);
}

std::unique_ptr<IPropertyEditor> FloatEditor::Clone() const {
    auto clone = std::make_unique<FloatEditor>(m_min, m_max, m_step, m_format);
    return clone;
}

// =============================================================================
// IntEditor Implementation
// =============================================================================

IntEditor::IntEditor(int min, int max, const char* format)
    : m_min(min), m_max(max), m_format(format) {}

bool IntEditor::Render(const char* label, void* data) {
    if (!data) return false;

    int* value = static_cast<int*>(data);

    if (m_isMixed) {
        std::string mixedStr = MixedValue::DISPLAY_TEXT;
        EditorWidgets::Property(label, mixedStr, 64);
        return false;
    }

    m_changed = EditorWidgets::Property(label, *value, m_min, m_max, m_format);
    return m_changed;
}

void IntEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<int*>(data) = std::any_cast<int>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any IntEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<int*>(data);
}

std::unique_ptr<IPropertyEditor> IntEditor::Clone() const {
    return std::make_unique<IntEditor>(m_min, m_max, m_format);
}

// =============================================================================
// BoolEditor Implementation
// =============================================================================

bool BoolEditor::Render(const char* label, void* data) {
    if (!data) return false;

    bool* value = static_cast<bool*>(data);

    if (m_isMixed) {
        // Show indeterminate checkbox state
        // For now, just show as unchecked with special styling
        bool temp = false;
        m_changed = EditorWidgets::Property(label, temp);
        if (m_changed) {
            *value = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::Property(label, *value);
    return m_changed;
}

void BoolEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<bool*>(data) = std::any_cast<bool>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any BoolEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<bool*>(data);
}

std::unique_ptr<IPropertyEditor> BoolEditor::Clone() const {
    return std::make_unique<BoolEditor>();
}

// =============================================================================
// StringEditor Implementation
// =============================================================================

StringEditor::StringEditor(size_t maxLength, bool multiline)
    : m_maxLength(maxLength), m_multiline(multiline) {}

bool StringEditor::Render(const char* label, void* data) {
    if (!data) return false;

    std::string* value = static_cast<std::string*>(data);

    if (m_isMixed) {
        std::string mixedStr = MixedValue::DISPLAY_TEXT;
        m_changed = EditorWidgets::Property(label, mixedStr, m_maxLength);
        if (m_changed && mixedStr != MixedValue::DISPLAY_TEXT) {
            *value = mixedStr;
            m_isMixed = false;
        }
        return m_changed;
    }

    if (m_multiline) {
        m_changed = EditorWidgets::TextAreaInput(label, *value, glm::vec2(0, 100));
    } else {
        m_changed = EditorWidgets::Property(label, *value, m_maxLength);
    }
    return m_changed;
}

void StringEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<std::string*>(data) = std::any_cast<std::string>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any StringEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<std::string*>(data);
}

std::unique_ptr<IPropertyEditor> StringEditor::Clone() const {
    return std::make_unique<StringEditor>(m_maxLength, m_multiline);
}

// =============================================================================
// Vec2Editor Implementation
// =============================================================================

Vec2Editor::Vec2Editor(float min, float max, float speed)
    : m_min(min), m_max(max), m_speed(speed) {}

bool Vec2Editor::Render(const char* label, void* data) {
    if (!data) return false;

    glm::vec2* value = static_cast<glm::vec2*>(data);

    if (m_isMixed) {
        glm::vec2 temp(0.0f);
        m_changed = EditorWidgets::Property(label, temp, m_min, m_max, m_speed);
        if (m_changed) {
            *value = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::Property(label, *value, m_min, m_max, m_speed);
    return m_changed;
}

void Vec2Editor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<glm::vec2*>(data) = std::any_cast<glm::vec2>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any Vec2Editor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<glm::vec2*>(data);
}

std::unique_ptr<IPropertyEditor> Vec2Editor::Clone() const {
    return std::make_unique<Vec2Editor>(m_min, m_max, m_speed);
}

// =============================================================================
// Vec3Editor Implementation
// =============================================================================

Vec3Editor::Vec3Editor(float min, float max, float speed)
    : m_min(min), m_max(max), m_speed(speed) {}

bool Vec3Editor::Render(const char* label, void* data) {
    if (!data) return false;

    glm::vec3* value = static_cast<glm::vec3*>(data);

    if (m_isMixed) {
        glm::vec3 temp(0.0f);
        m_changed = EditorWidgets::Property(label, temp, m_min, m_max, m_speed);
        if (m_changed) {
            *value = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::Property(label, *value, m_min, m_max, m_speed);
    return m_changed;
}

void Vec3Editor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<glm::vec3*>(data) = std::any_cast<glm::vec3>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any Vec3Editor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<glm::vec3*>(data);
}

std::unique_ptr<IPropertyEditor> Vec3Editor::Clone() const {
    return std::make_unique<Vec3Editor>(m_min, m_max, m_speed);
}

// =============================================================================
// Vec4Editor Implementation
// =============================================================================

Vec4Editor::Vec4Editor(float min, float max, float speed)
    : m_min(min), m_max(max), m_speed(speed) {}

bool Vec4Editor::Render(const char* label, void* data) {
    if (!data) return false;

    glm::vec4* value = static_cast<glm::vec4*>(data);

    if (m_isMixed) {
        glm::vec4 temp(0.0f);
        m_changed = EditorWidgets::Property(label, temp, m_min, m_max, m_speed);
        if (m_changed) {
            *value = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::Property(label, *value, m_min, m_max, m_speed);
    return m_changed;
}

void Vec4Editor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<glm::vec4*>(data) = std::any_cast<glm::vec4>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any Vec4Editor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<glm::vec4*>(data);
}

std::unique_ptr<IPropertyEditor> Vec4Editor::Clone() const {
    return std::make_unique<Vec4Editor>(m_min, m_max, m_speed);
}

// =============================================================================
// ColorEditor Implementation
// =============================================================================

ColorEditor::ColorEditor(bool showAlpha, bool hdr)
    : m_showAlpha(showAlpha), m_hdr(hdr) {}

bool ColorEditor::Render(const char* label, void* data) {
    if (!data) return false;

    if (m_showAlpha) {
        glm::vec4* color = static_cast<glm::vec4*>(data);

        if (m_isMixed) {
            glm::vec4 temp(0.5f);
            m_changed = EditorWidgets::ColorProperty(label, temp);
            if (m_changed) {
                *color = temp;
                m_isMixed = false;
            }
            return m_changed;
        }

        m_changed = EditorWidgets::ColorProperty(label, *color);
    } else {
        glm::vec3* color = static_cast<glm::vec3*>(data);

        if (m_isMixed) {
            glm::vec3 temp(0.5f);
            m_changed = EditorWidgets::ColorProperty(label, temp);
            if (m_changed) {
                *color = temp;
                m_isMixed = false;
            }
            return m_changed;
        }

        m_changed = EditorWidgets::ColorProperty(label, *color);
    }
    return m_changed;
}

void ColorEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        if (m_showAlpha) {
            *static_cast<glm::vec4*>(data) = std::any_cast<glm::vec4>(value);
        } else {
            *static_cast<glm::vec3*>(data) = std::any_cast<glm::vec3>(value);
        }
    } catch (const std::bad_any_cast&) {}
}

std::any ColorEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    if (m_showAlpha) {
        return *static_cast<glm::vec4*>(data);
    } else {
        return *static_cast<glm::vec3*>(data);
    }
}

std::type_index ColorEditor::GetHandledType() const {
    return m_showAlpha ? typeid(glm::vec4) : typeid(glm::vec3);
}

std::unique_ptr<IPropertyEditor> ColorEditor::Clone() const {
    return std::make_unique<ColorEditor>(m_showAlpha, m_hdr);
}

// =============================================================================
// EnumEditor Implementation
// =============================================================================

EnumEditor::EnumEditor(const std::vector<std::string>& names) {
    SetOptions(names);
}

EnumEditor::EnumEditor(const char* const* names, int count) {
    m_names.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_names.push_back(names[i]);
    }
    m_namePtrs.reserve(count);
    for (const auto& name : m_names) {
        m_namePtrs.push_back(name.c_str());
    }
}

void EnumEditor::SetOptions(const std::vector<std::string>& names) {
    m_names = names;
    m_namePtrs.clear();
    m_namePtrs.reserve(m_names.size());
    for (const auto& name : m_names) {
        m_namePtrs.push_back(name.c_str());
    }
}

bool EnumEditor::Render(const char* label, void* data) {
    if (!data || m_namePtrs.empty()) return false;

    int* value = static_cast<int*>(data);

    if (m_isMixed) {
        int temp = -1;  // Invalid index to show "---"
        m_changed = EditorWidgets::EnumProperty(label, temp, m_namePtrs.data(), static_cast<int>(m_namePtrs.size()));
        if (m_changed && temp >= 0) {
            *value = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::EnumProperty(label, *value, m_namePtrs.data(), static_cast<int>(m_namePtrs.size()));
    return m_changed;
}

void EnumEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<int*>(data) = std::any_cast<int>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any EnumEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<int*>(data);
}

std::unique_ptr<IPropertyEditor> EnumEditor::Clone() const {
    return std::make_unique<EnumEditor>(m_names);
}

// =============================================================================
// ObjectReferenceEditor Implementation
// =============================================================================

ObjectReferenceEditor::ObjectReferenceEditor(const std::string& typeName, const std::string& filter)
    : m_typeName(typeName), m_filter(filter) {}

bool ObjectReferenceEditor::Render(const char* label, void* data) {
    if (!data) return false;

    uint64_t* objectId = static_cast<uint64_t*>(data);

    if (m_isMixed) {
        uint64_t temp = 0;
        m_changed = EditorWidgets::ObjectProperty(label, temp, m_typeName.empty() ? nullptr : m_typeName.c_str());
        if (m_changed) {
            *objectId = temp;
            m_isMixed = false;
        }
        return m_changed;
    }

    m_changed = EditorWidgets::ObjectProperty(label, *objectId, m_typeName.empty() ? nullptr : m_typeName.c_str());
    return m_changed;
}

void ObjectReferenceEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<uint64_t*>(data) = std::any_cast<uint64_t>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any ObjectReferenceEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<uint64_t*>(data);
}

std::unique_ptr<IPropertyEditor> ObjectReferenceEditor::Clone() const {
    return std::make_unique<ObjectReferenceEditor>(m_typeName, m_filter);
}

// =============================================================================
// TransformEditor Implementation
// =============================================================================

TransformEditor::TransformEditor() = default;

bool TransformEditor::Render(const char* label, void* data) {
    if (!data) return false;

    TransformState* transform = static_cast<TransformState*>(data);
    m_changed = false;

    if (EditorWidgets::BeginPropertyPanel(label)) {
        // Position
        if (m_showPosition) {
            if (EditorWidgets::Property("Position", transform->position)) {
                m_changed = true;
            }
        }

        // Rotation (Euler or Quaternion)
        if (m_showRotation) {
            if (m_useEuler) {
                // Convert quaternion to Euler for editing
                glm::vec3 euler = QuatToEuler(transform->rotation);

                // Use cached Euler to avoid gimbal lock issues during editing
                if (!m_changed) {
                    m_cachedEuler = euler;
                }

                if (EditorWidgets::Property("Rotation", m_cachedEuler, -360.0f, 360.0f, 0.5f)) {
                    transform->rotation = EulerToQuat(m_cachedEuler);
                    m_changed = true;
                }
            } else {
                // Edit quaternion directly (advanced mode)
                glm::vec4 quatVec(transform->rotation.x, transform->rotation.y,
                                  transform->rotation.z, transform->rotation.w);
                if (EditorWidgets::Property("Rotation", quatVec, -1.0f, 1.0f, 0.01f)) {
                    transform->rotation = glm::normalize(glm::quat(quatVec.w, quatVec.x, quatVec.y, quatVec.z));
                    m_changed = true;
                }
            }
        }

        // Scale
        if (m_showScale) {
            if (EditorWidgets::Property("Scale", transform->scale, 0.001f, 1000.0f, 0.01f)) {
                m_changed = true;
            }
        }

        EditorWidgets::EndPropertyPanel();
    }

    return m_changed;
}

void TransformEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<TransformState*>(data) = std::any_cast<TransformState>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any TransformEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<TransformState*>(data);
}

std::unique_ptr<IPropertyEditor> TransformEditor::Clone() const {
    auto clone = std::make_unique<TransformEditor>();
    clone->m_showPosition = m_showPosition;
    clone->m_showRotation = m_showRotation;
    clone->m_showScale = m_showScale;
    clone->m_useEuler = m_useEuler;
    return clone;
}

// =============================================================================
// CurveEditor Implementation
// =============================================================================

CurveEditor::CurveEditor(float minTime, float maxTime, float minValue, float maxValue)
    : m_minTime(minTime), m_maxTime(maxTime), m_minValue(minValue), m_maxValue(maxValue) {}

bool CurveEditor::Render(const char* label, void* data) {
    if (!data) return false;

    auto* curve = static_cast<std::vector<EditorWidgets::CurvePoint>*>(data);

    if (m_isMixed) {
        // Show "---" for mixed curves
        EditorWidgets::SubHeader(MixedValue::DISPLAY_TEXT);
        return false;
    }

    m_changed = EditorWidgets::CurveProperty(label, *curve, m_minTime, m_maxTime, m_minValue, m_maxValue);
    return m_changed;
}

void CurveEditor::SetValue(void* data, const std::any& value) {
    if (!data) return;
    try {
        *static_cast<std::vector<EditorWidgets::CurvePoint>*>(data) =
            std::any_cast<std::vector<EditorWidgets::CurvePoint>>(value);
    } catch (const std::bad_any_cast&) {}
}

std::any CurveEditor::GetValue(void* data) const {
    if (!data) return std::any{};
    return *static_cast<std::vector<EditorWidgets::CurvePoint>*>(data);
}

std::unique_ptr<IPropertyEditor> CurveEditor::Clone() const {
    return std::make_unique<CurveEditor>(m_minTime, m_maxTime, m_minValue, m_maxValue);
}

// =============================================================================
// PropertyChangeCommand Implementation
// =============================================================================

PropertyChangeCommand::PropertyChangeCommand(
    const std::string& description,
    void* target,
    const std::string& propertyName,
    Getter getter,
    Setter setter,
    const std::any& newValue
)
    : m_description(description)
    , m_target(target)
    , m_propertyName(propertyName)
    , m_getter(std::move(getter))
    , m_setter(std::move(setter))
    , m_newValue(newValue)
{
    m_oldValue = m_getter();
}

bool PropertyChangeCommand::Execute() {
    m_setter(m_newValue);
    return true;
}

bool PropertyChangeCommand::Undo() {
    m_setter(m_oldValue);
    return true;
}

std::string PropertyChangeCommand::GetName() const {
    return m_description;
}

CommandTypeId PropertyChangeCommand::GetTypeId() const {
    return GetCommandTypeId<PropertyChangeCommand>();
}

bool PropertyChangeCommand::CanMergeWith(const ICommand& other) const {
    const auto* otherCmd = dynamic_cast<const PropertyChangeCommand*>(&other);
    if (!otherCmd) return false;

    // Merge if same target and property within time window
    return m_target == otherCmd->m_target &&
           m_propertyName == otherCmd->m_propertyName &&
           IsWithinMergeWindow(500);
}

bool PropertyChangeCommand::MergeWith(const ICommand& other) {
    const auto* otherCmd = dynamic_cast<const PropertyChangeCommand*>(&other);
    if (!otherCmd) return false;

    // Keep old value from first command, take new value from merged command
    m_newValue = otherCmd->m_newValue;
    return true;
}

// =============================================================================
// PropertyEditorFactory Implementation
// =============================================================================

PropertyEditorFactory& PropertyEditorFactory::Instance() {
    static PropertyEditorFactory instance;
    return instance;
}

PropertyEditorFactory::PropertyEditorFactory() {
    // Register default factories
    RegisterByType(typeid(float), []() { return std::make_unique<FloatEditor>(); });
    RegisterByType(typeid(int), []() { return std::make_unique<IntEditor>(); });
    RegisterByType(typeid(bool), []() { return std::make_unique<BoolEditor>(); });
    RegisterByType(typeid(std::string), []() { return std::make_unique<StringEditor>(); });
    RegisterByType(typeid(glm::vec2), []() { return std::make_unique<Vec2Editor>(); });
    RegisterByType(typeid(glm::vec3), []() { return std::make_unique<Vec3Editor>(); });
    RegisterByType(typeid(glm::vec4), []() { return std::make_unique<Vec4Editor>(); });
    RegisterByType(typeid(TransformState), []() { return std::make_unique<TransformEditor>(); });
    RegisterByType(typeid(std::vector<EditorWidgets::CurvePoint>), []() { return std::make_unique<CurveEditor>(); });
}

void PropertyEditorFactory::RegisterByType(std::type_index type, std::function<std::unique_ptr<IPropertyEditor>()> factory) {
    m_factories[type] = std::move(factory);
}

std::unique_ptr<IPropertyEditor> PropertyEditorFactory::Create(std::type_index type) const {
    auto it = m_factories.find(type);
    if (it != m_factories.end()) {
        return it->second();
    }
    return nullptr;
}

bool PropertyEditorFactory::HasFactory(std::type_index type) const {
    return m_factories.find(type) != m_factories.end();
}

// =============================================================================
// PropertiesPanel Implementation
// =============================================================================

PropertiesPanel::PropertiesPanel() = default;

PropertiesPanel::~PropertiesPanel() = default;

void PropertiesPanel::OnInitialize() {
    RegisterDefaultEditors();
}

void PropertiesPanel::OnShutdown() {
    m_customEditors.clear();
    m_targets.clear();
    m_propertyGroups.clear();
}

void PropertiesPanel::RegisterDefaultEditors() {
    // Use the factory's default editors
    auto& factory = PropertyEditorFactory::Instance();

    // Additional specialized editors not in factory
    m_customEditors[typeid(glm::vec3)] = std::make_unique<Vec3Editor>();
    m_customEditors[typeid(glm::vec4)] = std::make_unique<Vec4Editor>();
}

void PropertiesPanel::SetTarget(SceneNode* node) {
    m_targets.clear();
    if (node) {
        m_targets.push_back(node);
    }
    m_needsRebuild = true;
    m_cachedValues.clear();
}

void PropertiesPanel::SetTargets(const std::vector<SceneNode*>& nodes) {
    m_targets = nodes;
    m_needsRebuild = true;
    m_cachedValues.clear();
}

void PropertiesPanel::ClearTarget() {
    m_targets.clear();
    m_needsRebuild = true;
    m_cachedValues.clear();
    m_propertyGroups.clear();
}

SceneNode* PropertiesPanel::GetTarget() const {
    return m_targets.empty() ? nullptr : m_targets.front();
}

void PropertiesPanel::Refresh() {
    m_needsRebuild = true;
    m_cachedValues.clear();
}

void PropertiesPanel::RegisterPropertyEditorByType(std::type_index type, std::unique_ptr<IPropertyEditor> editor) {
    m_customEditors[type] = std::move(editor);
}

void PropertiesPanel::UnregisterPropertyEditorByType(std::type_index type) {
    m_customEditors.erase(type);
}

IPropertyEditor* PropertiesPanel::GetPropertyEditor(std::type_index type) const {
    auto it = m_customEditors.find(type);
    if (it != m_customEditors.end()) {
        return it->second.get();
    }
    return nullptr;
}

void PropertiesPanel::SetFilter(const std::string& filter) {
    m_filterText = filter;
    std::strncpy(m_filterBuffer, filter.c_str(), sizeof(m_filterBuffer) - 1);
    m_filterBuffer[sizeof(m_filterBuffer) - 1] = '\0';
    m_hasActiveFilter = !filter.empty();
    ApplyFilter();
}

void PropertiesPanel::ClearFilter() {
    m_filterText.clear();
    m_filterBuffer[0] = '\0';
    m_hasActiveFilter = false;
    ApplyFilter();
}

void PropertiesPanel::OnSearchChanged(const std::string& filter) {
    SetFilter(filter);
}

void PropertiesPanel::OnRender() {
    // Rebuild property groups if needed
    if (m_needsRebuild) {
        RebuildPropertyGroups();
        m_needsRebuild = false;
    }

    // Handle no selection
    if (m_targets.empty()) {
        RenderNoSelection();
        return;
    }

    // Multi-edit header
    if (IsMultiEdit()) {
        RenderMultiEditHeader();
    }

    // Render property groups
    RenderPropertyGroups();
}

void PropertiesPanel::OnRenderToolbar() {
    EditorWidgets::BeginToolbar("PropertiesToolbar");

    // Lock/unlock editing
    if (EditorWidgets::ToolbarButton(m_lockIcon ? "lock" : "unlock", "Lock inspector")) {
        m_lockIcon = !m_lockIcon;
    }

    EditorWidgets::ToolbarSeparator();

    // Refresh button
    if (EditorWidgets::ToolbarButton("refresh", "Refresh properties")) {
        Refresh();
    }

    // Show/hide advanced
    if (EditorWidgets::ToolbarButton("settings", "Show advanced properties")) {
        m_showHidden = !m_showHidden;
        Refresh();
    }

    EditorWidgets::EndToolbar();
}

void PropertiesPanel::RenderNoSelection() {
    EditorWidgets::CenterNextItem(200.0f);
    EditorWidgets::SubHeader("No object selected");
}

void PropertiesPanel::RenderMultiEditHeader() {
    std::stringstream ss;
    ss << m_targets.size() << " objects selected";
    EditorWidgets::SubHeader(ss.str().c_str());
    EditorWidgets::Separator();
}

void PropertiesPanel::RebuildPropertyGroups() {
    m_propertyGroups.clear();

    if (m_targets.empty()) return;

    // Always show transform section
    BuildTransformGroup();

    // Check for SDF primitive
    if (GetSDFPrimitive(m_targets.front())) {
        BuildSDFPrimitiveGroup();
    }

    // Check for material
    if (GetMaterial(m_targets.front())) {
        BuildMaterialGroup();
    }

    // Add other component sections
    BuildPhysicsGroup();
    BuildScriptGroup();
    BuildCustomComponentGroups();

    // Apply current filter
    if (m_hasActiveFilter) {
        ApplyFilter();
    }
}

void PropertiesPanel::BuildTransformGroup() {
    PropertyGroup group;
    group.name = "Transform";
    group.icon = "transform";
    group.order = 0;
    group.expanded = true;

    // Position property
    PropertyGroup::Property posProp;
    posProp.name = "position";
    posProp.displayName = "Position";
    posProp.type = typeid(glm::vec3);
    posProp.editor = std::make_unique<Vec3Editor>(-FLT_MAX, FLT_MAX, 0.1f);
    posProp.getter = [](void* node) -> std::any {
        return static_cast<SceneNode*>(node)->GetPosition();
    };
    posProp.setter = [](void* node, const std::any& value) {
        static_cast<SceneNode*>(node)->SetPosition(std::any_cast<glm::vec3>(value));
    };
    group.AddProperty(std::move(posProp));

    // Rotation property (Euler angles)
    PropertyGroup::Property rotProp;
    rotProp.name = "rotation";
    rotProp.displayName = "Rotation";
    rotProp.type = typeid(glm::vec3);
    rotProp.editor = std::make_unique<Vec3Editor>(-360.0f, 360.0f, 0.5f);
    rotProp.getter = [](void* node) -> std::any {
        glm::quat q = static_cast<SceneNode*>(node)->GetRotation();
        return QuatToEuler(q);
    };
    rotProp.setter = [](void* node, const std::any& value) {
        glm::vec3 euler = std::any_cast<glm::vec3>(value);
        static_cast<SceneNode*>(node)->SetRotation(EulerToQuat(euler));
    };
    group.AddProperty(std::move(rotProp));

    // Scale property
    PropertyGroup::Property scaleProp;
    scaleProp.name = "scale";
    scaleProp.displayName = "Scale";
    scaleProp.type = typeid(glm::vec3);
    scaleProp.editor = std::make_unique<Vec3Editor>(0.001f, 1000.0f, 0.01f);
    scaleProp.getter = [](void* node) -> std::any {
        return static_cast<SceneNode*>(node)->GetScale();
    };
    scaleProp.setter = [](void* node, const std::any& value) {
        static_cast<SceneNode*>(node)->SetScale(std::any_cast<glm::vec3>(value));
    };
    group.AddProperty(std::move(scaleProp));

    m_propertyGroups.push_back(std::move(group));
}

void PropertiesPanel::BuildSDFPrimitiveGroup() {
    PropertyGroup group;
    group.name = "SDF Primitive";
    group.icon = "sdf";
    group.order = 1;
    group.expanded = true;

    // Primitive type enum
    static const char* primitiveTypeNames[] = {
        "Sphere", "Box", "Cylinder", "Capsule", "Cone", "Torus",
        "Plane", "Rounded Box", "Ellipsoid", "Pyramid", "Prism", "Custom"
    };

    PropertyGroup::Property typeProp;
    typeProp.name = "primitiveType";
    typeProp.displayName = "Type";
    typeProp.type = typeid(int);
    typeProp.editor = std::make_unique<EnumEditor>(primitiveTypeNames, 12);
    typeProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return static_cast<int>(sdf->GetType());
        }
        return 0;
    };
    typeProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->SetType(static_cast<SDFPrimitiveType>(std::any_cast<int>(value)));
        }
    };
    group.AddProperty(std::move(typeProp));

    // CSG Operation enum
    static const char* csgOpNames[] = {
        "Union", "Subtraction", "Intersection",
        "Smooth Union", "Smooth Subtraction", "Smooth Intersection"
    };

    PropertyGroup::Property csgProp;
    csgProp.name = "csgOperation";
    csgProp.displayName = "CSG Operation";
    csgProp.type = typeid(int);
    csgProp.editor = std::make_unique<EnumEditor>(csgOpNames, 6);
    csgProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return static_cast<int>(sdf->GetCSGOperation());
        }
        return 0;
    };
    csgProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->SetCSGOperation(static_cast<CSGOperation>(std::any_cast<int>(value)));
        }
    };
    group.AddProperty(std::move(csgProp));

    // Smoothness (for smooth CSG operations)
    PropertyGroup::Property smoothProp;
    smoothProp.name = "smoothness";
    smoothProp.displayName = "Smoothness";
    smoothProp.type = typeid(float);
    smoothProp.editor = std::make_unique<FloatEditor>(0.0f, 1.0f, 0.01f);
    smoothProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetParameters().smoothness;
        }
        return 0.1f;
    };
    smoothProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetParameters().smoothness = std::any_cast<float>(value);
        }
    };
    group.AddProperty(std::move(smoothProp));

    // Visibility toggle
    PropertyGroup::Property visProp;
    visProp.name = "visible";
    visProp.displayName = "Visible";
    visProp.type = typeid(bool);
    visProp.editor = std::make_unique<BoolEditor>();
    visProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->IsVisible();
        }
        return true;
    };
    visProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->SetVisible(std::any_cast<bool>(value));
        }
    };
    group.AddProperty(std::move(visProp));

    m_propertyGroups.push_back(std::move(group));
}

void PropertiesPanel::BuildMaterialGroup() {
    PropertyGroup group;
    group.name = "Material";
    group.icon = "material";
    group.order = 2;
    group.expanded = true;

    // Base color
    PropertyGroup::Property colorProp;
    colorProp.name = "baseColor";
    colorProp.displayName = "Base Color";
    colorProp.type = typeid(glm::vec4);
    colorProp.editor = std::make_unique<ColorEditor>(true);
    colorProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetMaterial().baseColor;
        }
        return glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    };
    colorProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetMaterial().baseColor = std::any_cast<glm::vec4>(value);
        }
    };
    group.AddProperty(std::move(colorProp));

    // Metallic
    PropertyGroup::Property metallicProp;
    metallicProp.name = "metallic";
    metallicProp.displayName = "Metallic";
    metallicProp.type = typeid(float);
    metallicProp.editor = std::make_unique<FloatEditor>(0.0f, 1.0f, 0.01f);
    metallicProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetMaterial().metallic;
        }
        return 0.0f;
    };
    metallicProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetMaterial().metallic = std::any_cast<float>(value);
        }
    };
    group.AddProperty(std::move(metallicProp));

    // Roughness
    PropertyGroup::Property roughProp;
    roughProp.name = "roughness";
    roughProp.displayName = "Roughness";
    roughProp.type = typeid(float);
    roughProp.editor = std::make_unique<FloatEditor>(0.0f, 1.0f, 0.01f);
    roughProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetMaterial().roughness;
        }
        return 0.5f;
    };
    roughProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetMaterial().roughness = std::any_cast<float>(value);
        }
    };
    group.AddProperty(std::move(roughProp));

    // Emissive
    PropertyGroup::Property emissiveProp;
    emissiveProp.name = "emissive";
    emissiveProp.displayName = "Emissive";
    emissiveProp.type = typeid(float);
    emissiveProp.editor = std::make_unique<FloatEditor>(0.0f, 100.0f, 0.1f);
    emissiveProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetMaterial().emissive;
        }
        return 0.0f;
    };
    emissiveProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetMaterial().emissive = std::any_cast<float>(value);
        }
    };
    group.AddProperty(std::move(emissiveProp));

    // Emissive Color
    PropertyGroup::Property emissiveColorProp;
    emissiveColorProp.name = "emissiveColor";
    emissiveColorProp.displayName = "Emissive Color";
    emissiveColorProp.type = typeid(glm::vec3);
    emissiveColorProp.editor = std::make_unique<ColorEditor>(false, true);  // HDR
    emissiveColorProp.getter = [this](void* node) -> std::any {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            return sdf->GetMaterial().emissiveColor;
        }
        return glm::vec3(0.0f);
    };
    emissiveColorProp.setter = [this](void* node, const std::any& value) {
        if (auto* sdf = GetSDFPrimitive(static_cast<SceneNode*>(node))) {
            sdf->GetMaterial().emissiveColor = std::any_cast<glm::vec3>(value);
        }
    };
    group.AddProperty(std::move(emissiveColorProp));

    m_propertyGroups.push_back(std::move(group));
}

void PropertiesPanel::BuildPhysicsGroup() {
    // Placeholder for physics properties
    // This would be populated if the node has a physics component
}

void PropertiesPanel::BuildScriptGroup() {
    // Placeholder for script properties
    // This would be populated if the node has script components
}

void PropertiesPanel::BuildCustomComponentGroups() {
    // Placeholder for custom component properties
    // These would be built from reflection data
}

void PropertiesPanel::BuildReflectedProperties(const Reflect::TypeInfo* typeInfo, void* instance, const std::string& category) {
    if (!typeInfo || !instance) return;

    PropertyGroup group;
    group.name = !category.empty() ? category : typeInfo->displayName;
    if (group.name.empty()) group.name = typeInfo->name;
    group.expanded = true;

    for (const auto* propInfo : typeInfo->GetAllProperties()) {
        if (!propInfo) continue;

        // Skip hidden properties unless showing advanced
        if (propInfo->IsHidden() && !m_showHidden) continue;

        PropertyGroup::Property prop;
        prop.name = propInfo->name;
        prop.displayName = propInfo->displayName.empty() ? GetPropertyDisplayName(propInfo->name) : propInfo->displayName;
        prop.tooltip = propInfo->description;
        prop.type = propInfo->typeIndex;
        prop.attributes = propInfo->attributes;

        // Create editor based on type
        prop.editor = CreateDefaultEditor(propInfo->typeIndex);
        if (!prop.editor) continue;

        // Set range if specified
        if (propInfo->hasRange) {
            if (auto* floatEditor = dynamic_cast<FloatEditor*>(prop.editor.get())) {
                floatEditor->SetRange(propInfo->minValue, propInfo->maxValue);
            } else if (auto* intEditor = dynamic_cast<IntEditor*>(prop.editor.get())) {
                intEditor->SetRange(static_cast<int>(propInfo->minValue), static_cast<int>(propInfo->maxValue));
            }
        }

        // Use reflection getter/setter
        if (propInfo->getterAny && propInfo->setterAny) {
            prop.getter = [propInfo](void* obj) -> std::any {
                return propInfo->getterAny(obj);
            };
            prop.setter = [propInfo](void* obj, const std::any& value) {
                propInfo->setterAny(obj, value);
            };
        }

        group.AddProperty(std::move(prop));
    }

    if (!group.properties.empty()) {
        m_propertyGroups.push_back(std::move(group));
    }
}

void PropertiesPanel::RenderPropertyGroups() {
    // Sort groups by order
    std::sort(m_propertyGroups.begin(), m_propertyGroups.end(),
              [](const PropertyGroup& a, const PropertyGroup& b) {
                  return a.order < b.order;
              });

    for (auto& group : m_propertyGroups) {
        if (!group.visible) continue;

        // Check if any properties match filter
        bool hasVisibleProperties = false;
        for (const auto& prop : group.properties) {
            if (prop.visible) {
                hasVisibleProperties = true;
                break;
            }
        }

        if (!hasVisibleProperties && m_hasActiveFilter) continue;

        RenderPropertyGroup(group);
    }
}

void PropertiesPanel::RenderPropertyGroup(PropertyGroup& group) {
    bool open = EditorWidgets::CollapsingHeader(group.name.c_str(), &group.expanded, group.expanded);

    if (open && group.expanded) {
        EditorWidgets::BeginSection();

        for (auto& prop : group.properties) {
            if (!prop.visible) continue;

            // Get target data pointer
            void* data = m_targets.empty() ? nullptr : m_targets.front();
            if (!data) continue;

            // Check for mixed values in multi-edit
            bool isMixed = IsMultiEdit() && CheckMixedValues(prop);
            if (prop.editor) {
                prop.editor->SetMixedValue(isMixed);
            }

            RenderProperty(prop, data);
        }

        EditorWidgets::EndSection();
    }
}

void PropertiesPanel::RenderProperty(PropertyGroup::Property& prop, void* data) {
    if (!prop.editor) return;

    // Get current value
    std::any currentValue;
    if (prop.getter) {
        currentValue = prop.getter(data);
    }

    // Create temp storage for editing
    // This is a simplified approach - in production, use proper type handling
    bool changed = false;

    // Use the appropriate editor based on type
    if (prop.type == typeid(float)) {
        float value = 0.0f;
        try { value = std::any_cast<float>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            // Apply to all targets in multi-edit
            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(int)) {
        int value = 0;
        try { value = std::any_cast<int>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(bool)) {
        bool value = false;
        try { value = std::any_cast<bool>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(std::string)) {
        std::string value;
        try { value = std::any_cast<std::string>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(glm::vec2)) {
        glm::vec2 value(0.0f);
        try { value = std::any_cast<glm::vec2>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(glm::vec3)) {
        glm::vec3 value(0.0f);
        try { value = std::any_cast<glm::vec3>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    } else if (prop.type == typeid(glm::vec4)) {
        glm::vec4 value(0.0f);
        try { value = std::any_cast<glm::vec4>(currentValue); } catch (...) {}
        changed = prop.editor->Render(prop.displayName.c_str(), &value);
        if (changed) {
            BeginPropertyEdit(prop.name);
            RecordPropertyChange(prop.name, data, currentValue, value);
            if (prop.setter) prop.setter(data, value);

            if (IsMultiEdit()) {
                for (size_t i = 1; i < m_targets.size(); ++i) {
                    prop.setter(m_targets[i], value);
                }
            }
        }
    }

    // Clear editor change state after handling
    if (prop.editor) {
        prop.editor->ClearChanged();
    }

    // End property edit if not actively editing
    if (!changed && m_isEditing && m_currentEditProperty == prop.name) {
        EndPropertyEdit();
    }

    // Notify callback
    if (changed && OnPropertyChanged) {
        OnPropertyChanged(prop.name, currentValue, prop.getter ? prop.getter(data) : std::any{});
    }
}

void PropertiesPanel::ApplyFilter() {
    if (m_filterText.empty()) {
        // Show all properties
        for (auto& group : m_propertyGroups) {
            group.visible = true;
            for (auto& prop : group.properties) {
                prop.visible = true;
            }
        }
        return;
    }

    // Convert filter to lowercase for case-insensitive matching
    std::string lowerFilter = m_filterText;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    for (auto& group : m_propertyGroups) {
        bool groupHasMatch = false;

        // Check group name
        std::string lowerGroupName = group.name;
        std::transform(lowerGroupName.begin(), lowerGroupName.end(), lowerGroupName.begin(), ::tolower);
        if (lowerGroupName.find(lowerFilter) != std::string::npos) {
            groupHasMatch = true;
            for (auto& prop : group.properties) {
                prop.visible = true;
            }
        } else {
            // Check individual properties
            for (auto& prop : group.properties) {
                std::string lowerPropName = prop.displayName;
                std::transform(lowerPropName.begin(), lowerPropName.end(), lowerPropName.begin(), ::tolower);

                prop.visible = (lowerPropName.find(lowerFilter) != std::string::npos);
                if (prop.visible) groupHasMatch = true;
            }
        }

        group.visible = groupHasMatch;
    }
}

bool PropertiesPanel::MatchesFilter(const std::string& name) const {
    if (m_filterText.empty()) return true;

    std::string lowerName = name;
    std::string lowerFilter = m_filterText;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    return lowerName.find(lowerFilter) != std::string::npos;
}

std::any PropertiesPanel::GetPropertyValue(const PropertyGroup::Property& prop, void* data) const {
    if (prop.getter && data) {
        return prop.getter(data);
    }
    return std::any{};
}

void PropertiesPanel::SetPropertyValue(PropertyGroup::Property& prop, void* data, const std::any& value) {
    if (prop.setter && data) {
        prop.setter(data, value);
    }
}

bool PropertiesPanel::AreValuesEqual(const std::any& a, const std::any& b) const {
    if (a.type() != b.type()) return false;

    try {
        if (a.type() == typeid(float)) {
            return std::abs(std::any_cast<float>(a) - std::any_cast<float>(b)) < 0.0001f;
        } else if (a.type() == typeid(int)) {
            return std::any_cast<int>(a) == std::any_cast<int>(b);
        } else if (a.type() == typeid(bool)) {
            return std::any_cast<bool>(a) == std::any_cast<bool>(b);
        } else if (a.type() == typeid(std::string)) {
            return std::any_cast<std::string>(a) == std::any_cast<std::string>(b);
        } else if (a.type() == typeid(glm::vec2)) {
            glm::vec2 va = std::any_cast<glm::vec2>(a);
            glm::vec2 vb = std::any_cast<glm::vec2>(b);
            return glm::length(va - vb) < 0.0001f;
        } else if (a.type() == typeid(glm::vec3)) {
            glm::vec3 va = std::any_cast<glm::vec3>(a);
            glm::vec3 vb = std::any_cast<glm::vec3>(b);
            return glm::length(va - vb) < 0.0001f;
        } else if (a.type() == typeid(glm::vec4)) {
            glm::vec4 va = std::any_cast<glm::vec4>(a);
            glm::vec4 vb = std::any_cast<glm::vec4>(b);
            return glm::length(va - vb) < 0.0001f;
        }
    } catch (const std::bad_any_cast&) {
        return false;
    }

    return false;
}

bool PropertiesPanel::CheckMixedValues(const PropertyGroup::Property& prop) const {
    if (m_targets.size() <= 1) return false;
    if (!prop.getter) return false;

    std::any firstValue = prop.getter(m_targets.front());

    for (size_t i = 1; i < m_targets.size(); ++i) {
        std::any otherValue = prop.getter(m_targets[i]);
        if (!AreValuesEqual(firstValue, otherValue)) {
            return true;
        }
    }

    return false;
}

void PropertiesPanel::RecordPropertyChange(const std::string& propName, void* target,
                                            const std::any& oldValue, const std::any& newValue) {
    if (!m_commandHistory) return;

    // Find the property to get getter/setter
    PropertyGroup::Property* foundProp = nullptr;
    for (auto& group : m_propertyGroups) {
        for (auto& prop : group.properties) {
            if (prop.name == propName) {
                foundProp = &prop;
                break;
            }
        }
        if (foundProp) break;
    }

    if (!foundProp || !foundProp->getter || !foundProp->setter) return;

    // Create getter/setter closures that capture the target
    auto getter = [target, foundProp]() -> std::any {
        return foundProp->getter(target);
    };

    auto setter = [target, foundProp](const std::any& value) {
        foundProp->setter(target, value);
    };

    std::string description = "Set " + GetPropertyDisplayName(propName);
    auto cmd = std::make_unique<PropertyChangeCommand>(
        description, target, propName, getter, setter, newValue
    );

    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void PropertiesPanel::BeginPropertyEdit(const std::string& propName) {
    if (m_isEditing && m_currentEditProperty == propName) {
        // Already editing this property
        return;
    }

    m_currentEditProperty = propName;
    m_isEditing = true;
    m_editStartTime = std::chrono::steady_clock::now();
}

void PropertiesPanel::EndPropertyEdit() {
    m_isEditing = false;
    m_currentEditProperty.clear();
}

std::unique_ptr<IPropertyEditor> PropertiesPanel::CreateDefaultEditor(std::type_index type) const {
    return PropertyEditorFactory::Instance().Create(type);
}

void* PropertiesPanel::GetDataPointer(SceneNode* node, size_t offset) const {
    if (!node) return nullptr;
    return reinterpret_cast<char*>(node) + offset;
}

SDFPrimitive* PropertiesPanel::GetSDFPrimitive(SceneNode* node) const {
    // This would need to be implemented based on how SDFPrimitives are attached to SceneNodes
    // For now, return nullptr as placeholder
    (void)node;
    return nullptr;
}

Material* PropertiesPanel::GetMaterial(SceneNode* node) const {
    // Get material from scene node if it has one
    if (!node) return nullptr;
    auto materialPtr = node->GetMaterial();
    return materialPtr.get();
}

} // namespace Nova
