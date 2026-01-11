#pragma once

#include <string>
#include <map>
#include <vector>
#include <any>
#include <typeindex>
#include <functional>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Nova3D {

// Property hierarchy levels
enum class PropertyLevel {
    Global,    // Project-wide defaults
    Asset,     // Asset-level (all instances of an asset)
    Instance   // Per-instance (individual entities)
};

// Convert property level to string
inline const char* PropertyLevelToString(PropertyLevel level) {
    switch (level) {
        case PropertyLevel::Global: return "Global";
        case PropertyLevel::Asset: return "Asset";
        case PropertyLevel::Instance: return "Instance";
        default: return "Unknown";
    }
}

// Property metadata
struct PropertyMetadata {
    std::string name;
    std::string category;           // UI category (e.g., "Optical", "Emission")
    std::string tooltip;            // Help text
    PropertyLevel overrideLevel;    // Which level owns this value
    bool allowOverride = true;      // Can lower levels override?
    std::type_index type;           // Runtime type info

    // UI hints
    float minValue = 0.0f;
    float maxValue = 1.0f;
    bool isColor = false;
    bool isAngle = false;           // Display as degrees
    bool isPercentage = false;

    PropertyMetadata()
        : type(typeid(void)) {}

    PropertyMetadata(const std::string& name, std::type_index type)
        : name(name), type(type), overrideLevel(PropertyLevel::Global) {}
};

// Type-erased property value
class PropertyValue {
public:
    PropertyValue() = default;

    template<typename T>
    PropertyValue(const T& value, const PropertyMetadata& metadata)
        : m_value(value), m_metadata(metadata) {
        m_metadata.type = typeid(T);
    }

    // Get value with type checking
    template<typename T>
    T Get() const {
        if (m_value.type() != typeid(T)) {
            throw std::runtime_error("Type mismatch in PropertyValue::Get");
        }
        return std::any_cast<T>(m_value);
    }

    // Set value
    template<typename T>
    void Set(const T& value, PropertyLevel level) {
        m_value = value;
        m_metadata.overrideLevel = level;
        m_isDirty = true;
    }

    // Get metadata
    const PropertyMetadata& GetMetadata() const { return m_metadata; }
    PropertyMetadata& GetMetadata() { return m_metadata; }

    // Override level
    PropertyLevel GetOverrideLevel() const { return m_metadata.overrideLevel; }
    void SetOverrideLevel(PropertyLevel level) { m_metadata.overrideLevel = level; }

    // Check if overridden at level
    bool IsOverriddenAt(PropertyLevel level) const {
        return m_metadata.overrideLevel >= level;
    }

    // Dirty flag for change tracking
    bool IsDirty() const { return m_isDirty; }
    void ClearDirty() { m_isDirty = false; }

    // Type info
    std::type_index GetType() const { return m_value.type(); }
    bool HasValue() const { return m_value.has_value(); }

    // Serialization
    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& json);

private:
    std::any m_value;
    PropertyMetadata m_metadata;
    bool m_isDirty = false;
};

// Change notification callback
using PropertyChangeCallback = std::function<void(const std::string& propertyName, PropertyLevel level)>;

// Property container with hierarchical inheritance
class PropertyContainer {
public:
    PropertyContainer() = default;
    explicit PropertyContainer(PropertyContainer* parent) : m_parent(parent) {}

    // Register property with default value
    template<typename T>
    void RegisterProperty(const std::string& name, const T& defaultValue,
                         const std::string& category = "",
                         const std::string& tooltip = "") {
        PropertyMetadata metadata;
        metadata.name = name;
        metadata.category = category;
        metadata.tooltip = tooltip;
        metadata.type = typeid(T);
        metadata.overrideLevel = PropertyLevel::Global;

        m_properties[name] = PropertyValue(defaultValue, metadata);
    }

    // Register property with full metadata
    template<typename T>
    void RegisterProperty(const std::string& name, const T& defaultValue,
                         const PropertyMetadata& metadata) {
        PropertyMetadata meta = metadata;
        meta.name = name;
        meta.type = typeid(T);
        m_properties[name] = PropertyValue(defaultValue, meta);
    }

    // Get property value (walks up hierarchy if not overridden)
    template<typename T>
    T GetProperty(const std::string& name, PropertyLevel currentLevel) const {
        auto it = m_properties.find(name);

        // If property exists at this level and is overridden here, return it
        if (it != m_properties.end()) {
            const auto& prop = it->second;
            if (prop.GetOverrideLevel() >= currentLevel) {
                return prop.Get<T>();
            }
        }

        // Otherwise, walk up hierarchy
        if (m_parent && currentLevel > PropertyLevel::Global) {
            PropertyLevel parentLevel = static_cast<PropertyLevel>(
                static_cast<int>(currentLevel) - 1
            );
            return m_parent->GetProperty<T>(name, parentLevel);
        }

        // Fallback: return value if exists, otherwise default
        if (it != m_properties.end()) {
            return it->second.Get<T>();
        }

        return T();
    }

    // Get property value without hierarchy walk (raw value at this level)
    template<typename T>
    std::optional<T> GetPropertyRaw(const std::string& name) const {
        auto it = m_properties.find(name);
        if (it != m_properties.end() && it->second.HasValue()) {
            return it->second.Get<T>();
        }
        return std::nullopt;
    }

    // Set property at specific level
    template<typename T>
    void SetProperty(const std::string& name, const T& value, PropertyLevel level) {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            it->second.Set(value, level);
            NotifyChange(name, level);
        } else {
            // Create new property
            PropertyMetadata metadata;
            metadata.name = name;
            metadata.overrideLevel = level;
            m_properties[name] = PropertyValue(value, metadata);
            NotifyChange(name, level);
        }
    }

    // Check if property is overridden at level
    bool IsPropertyOverridden(const std::string& name, PropertyLevel level) const {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            return it->second.GetOverrideLevel() >= level;
        }
        return false;
    }

    // Reset property to parent level
    template<typename T>
    T ResetToParent(const std::string& name, PropertyLevel currentLevel) {
        if (currentLevel == PropertyLevel::Global) {
            // Can't reset global, return current value
            return GetProperty<T>(name, currentLevel);
        }

        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            // Mark as inherited from parent
            PropertyLevel parentLevel = static_cast<PropertyLevel>(
                static_cast<int>(currentLevel) - 1
            );
            it->second.SetOverrideLevel(parentLevel);
            NotifyChange(name, currentLevel);
        }

        return GetProperty<T>(name, currentLevel);
    }

    // Reset property to default (global level)
    template<typename T>
    T ResetToDefault(const std::string& name) {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            it->second.SetOverrideLevel(PropertyLevel::Global);
            NotifyChange(name, PropertyLevel::Global);
        }
        return GetProperty<T>(name, PropertyLevel::Global);
    }

    // Get property metadata
    const PropertyMetadata* GetMetadata(const std::string& name) const {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            return &it->second.GetMetadata();
        }
        return nullptr;
    }

    PropertyMetadata* GetMetadata(const std::string& name) {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            return &it->second.GetMetadata();
        }
        return nullptr;
    }

    // Get all property names
    std::vector<std::string> GetAllProperties() const {
        std::vector<std::string> names;
        names.reserve(m_properties.size());
        for (const auto& [name, _] : m_properties) {
            names.push_back(name);
        }
        return names;
    }

    // Get properties by category
    std::vector<std::string> GetPropertiesByCategory(const std::string& category) const {
        std::vector<std::string> names;
        for (const auto& [name, prop] : m_properties) {
            if (prop.GetMetadata().category == category) {
                names.push_back(name);
            }
        }
        return names;
    }

    // Get all categories
    std::vector<std::string> GetAllCategories() const {
        std::vector<std::string> categories;
        for (const auto& [_, prop] : m_properties) {
            const auto& cat = prop.GetMetadata().category;
            if (!cat.empty() && std::find(categories.begin(), categories.end(), cat) == categories.end()) {
                categories.push_back(cat);
            }
        }
        return categories;
    }

    // Change notifications
    void AddChangeCallback(PropertyChangeCallback callback) {
        m_changeCallbacks.push_back(callback);
    }

    void ClearChangeCallbacks() {
        m_changeCallbacks.clear();
    }

    // Hierarchy management
    void SetParent(PropertyContainer* parent) { m_parent = parent; }
    PropertyContainer* GetParent() const { return m_parent; }

    // Dirty tracking
    bool HasDirtyProperties() const {
        for (const auto& [_, prop] : m_properties) {
            if (prop.IsDirty()) return true;
        }
        return false;
    }

    void ClearDirtyFlags() {
        for (auto& [_, prop] : m_properties) {
            const_cast<PropertyValue&>(prop).ClearDirty();
        }
    }

    // Serialization
    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& json);

    // SQLite serialization
    void SaveToDatabase(class SQLiteDatabase& db, const std::string& tableName);
    void LoadFromDatabase(class SQLiteDatabase& db, const std::string& tableName);

private:
    void NotifyChange(const std::string& propertyName, PropertyLevel level) {
        for (auto& callback : m_changeCallbacks) {
            callback(propertyName, level);
        }
    }

    std::map<std::string, PropertyValue> m_properties;
    PropertyContainer* m_parent = nullptr;
    std::vector<PropertyChangeCallback> m_changeCallbacks;
};

// Global property system singleton
class PropertySystem {
public:
    static PropertySystem& Instance() {
        static PropertySystem instance;
        return instance;
    }

    PropertySystem(const PropertySystem&) = delete;
    PropertySystem& operator=(const PropertySystem&) = delete;

    // Container management
    PropertyContainer& GetGlobalContainer() { return m_globalContainer; }
    const PropertyContainer& GetGlobalContainer() const { return m_globalContainer; }

    PropertyContainer* CreateAssetContainer() {
        auto container = std::make_unique<PropertyContainer>(&m_globalContainer);
        auto ptr = container.get();
        m_assetContainers.push_back(std::move(container));
        return ptr;
    }

    PropertyContainer* CreateInstanceContainer(PropertyContainer* assetContainer) {
        auto container = std::make_unique<PropertyContainer>(assetContainer);
        auto ptr = container.get();
        m_instanceContainers.push_back(std::move(container));
        return ptr;
    }

    // Bulk operations
    void ResetAllToDefaults() {
        m_globalContainer.ClearDirtyFlags();
        for (auto& container : m_assetContainers) {
            container->ClearDirtyFlags();
        }
        for (auto& container : m_instanceContainers) {
            container->ClearDirtyFlags();
        }
    }

    // Serialization
    void SaveProject(const std::string& filepath);
    void LoadProject(const std::string& filepath);

private:
    PropertySystem() = default;

    PropertyContainer m_globalContainer;
    std::vector<std::unique_ptr<PropertyContainer>> m_assetContainers;
    std::vector<std::unique_ptr<PropertyContainer>> m_instanceContainers;
};

} // namespace Nova3D
