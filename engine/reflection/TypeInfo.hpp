#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <any>
#include <optional>

// Forward declaration for JSON support
namespace nlohmann {
    template<typename, typename, typename> class basic_json;
    using json = basic_json<std::map, std::vector, std::string, bool, int64_t, uint64_t, double, std::allocator, struct adl_serializer, std::vector<uint8_t>>;
}
using json = nlohmann::json;

namespace Nova {
namespace Reflect {

// Forward declarations
class TypeInfo;
class TypeRegistry;

/**
 * @brief Property attributes for metadata
 */
enum class PropertyAttribute {
    None = 0,
    Editable = 1 << 0,      // Can be edited in the editor
    Replicated = 1 << 1,    // Replicated across network
    Observable = 1 << 2,    // Triggers change notifications
    Serialized = 1 << 3,    // Saved to disk
    Hidden = 1 << 4,        // Hidden in editor
    ReadOnly = 1 << 5,      // Read-only in editor
    Transient = 1 << 6,     // Not saved
    BlueprintVisible = 1 << 7,
    Category = 1 << 8,
    Range = 1 << 9,
    Clamped = 1 << 10
};

inline PropertyAttribute operator|(PropertyAttribute a, PropertyAttribute b) {
    return static_cast<PropertyAttribute>(static_cast<int>(a) | static_cast<int>(b));
}

inline PropertyAttribute operator&(PropertyAttribute a, PropertyAttribute b) {
    return static_cast<PropertyAttribute>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool HasAttribute(PropertyAttribute flags, PropertyAttribute attr) {
    return (static_cast<int>(flags) & static_cast<int>(attr)) != 0;
}

/**
 * @brief Information about a single property in a reflected type
 */
struct PropertyInfo {
    std::string name;
    std::string typeName;
    std::string displayName;
    std::string description;
    std::string category;
    size_t offset = 0;
    size_t size = 0;
    std::type_index typeIndex;
    PropertyAttribute attributes = PropertyAttribute::None;

    // Attribute list for string-based queries
    std::vector<std::string> attributeStrings;

    // Setter and getter functions using JSON for type-erased value passing
    std::function<void(void*, const json&)> setter;
    std::function<json(const void*)> getter;

    // Type-safe accessors using std::any
    std::function<void(void*, const std::any&)> setterAny;
    std::function<std::any(const void*)> getterAny;

    // Range constraints for numeric properties
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool hasRange = false;

    // Default value
    std::any defaultValue;

    // Constructor
    PropertyInfo() : typeIndex(typeid(void)) {}

    PropertyInfo(std::string n, std::string tn, std::type_index ti)
        : name(std::move(n)), typeName(std::move(tn)), typeIndex(ti) {}

    // Helper to check attributes
    [[nodiscard]] bool HasAttribute(PropertyAttribute attr) const {
        return Nova::Reflect::HasAttribute(attributes, attr);
    }

    [[nodiscard]] bool IsEditable() const { return HasAttribute(PropertyAttribute::Editable); }
    [[nodiscard]] bool IsReplicated() const { return HasAttribute(PropertyAttribute::Replicated); }
    [[nodiscard]] bool IsObservable() const { return HasAttribute(PropertyAttribute::Observable); }
    [[nodiscard]] bool IsHidden() const { return HasAttribute(PropertyAttribute::Hidden); }
    [[nodiscard]] bool IsReadOnly() const { return HasAttribute(PropertyAttribute::ReadOnly); }
    [[nodiscard]] bool IsSerialized() const { return HasAttribute(PropertyAttribute::Serialized); }

    // Builder pattern for configuration
    PropertyInfo& WithDisplayName(const std::string& dn) { displayName = dn; return *this; }
    PropertyInfo& WithDescription(const std::string& desc) { description = desc; return *this; }
    PropertyInfo& WithCategory(const std::string& cat) { category = cat; return *this; }
    PropertyInfo& WithRange(float min, float max) { minValue = min; maxValue = max; hasRange = true; return *this; }
    PropertyInfo& WithAttribute(PropertyAttribute attr) { attributes = attributes | attr; return *this; }
    PropertyInfo& WithAttributeString(const std::string& attr) { attributeStrings.push_back(attr); return *this; }
};

/**
 * @brief Information about an event that a type can emit
 */
struct EventInfo {
    std::string name;
    std::string displayName;
    std::string description;
    std::vector<std::string> parameterTypes;
    std::vector<std::string> parameterNames;

    EventInfo() = default;
    EventInfo(std::string n) : name(std::move(n)) {}

    EventInfo& WithDisplayName(const std::string& dn) { displayName = dn; return *this; }
    EventInfo& WithDescription(const std::string& desc) { description = desc; return *this; }
    EventInfo& WithParameter(const std::string& type, const std::string& paramName) {
        parameterTypes.push_back(type);
        parameterNames.push_back(paramName);
        return *this;
    }
};

/**
 * @brief Information about a method in a reflected type
 */
struct MethodInfo {
    std::string name;
    std::string displayName;
    std::string description;
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::vector<std::string> parameterNames;
    std::function<std::any(void*, const std::vector<std::any>&)> invoker;
    bool isStatic = false;
    bool isConst = false;

    MethodInfo() = default;
    MethodInfo(std::string n) : name(std::move(n)) {}

    MethodInfo& WithDisplayName(const std::string& dn) { displayName = dn; return *this; }
    MethodInfo& WithDescription(const std::string& desc) { description = desc; return *this; }
    MethodInfo& WithReturnType(const std::string& rt) { returnType = rt; return *this; }
    MethodInfo& WithParameter(const std::string& type, const std::string& paramName) {
        parameterTypes.push_back(type);
        parameterNames.push_back(paramName);
        return *this;
    }
    MethodInfo& AsStatic() { isStatic = true; return *this; }
    MethodInfo& AsConst() { isConst = true; return *this; }
};

/**
 * @brief Complete runtime type information for a class
 */
struct TypeInfo {
    std::string name;
    std::string displayName;
    std::string description;
    std::string category;
    size_t size = 0;
    size_t alignment = 0;
    std::type_index typeIndex;

    // Inheritance
    const TypeInfo* baseType = nullptr;
    std::string baseTypeName;

    // Properties
    std::vector<PropertyInfo> properties;
    std::unordered_map<std::string, size_t> propertyIndexByName;

    // Events this type can emit
    std::vector<EventInfo> events;
    std::unordered_map<std::string, size_t> eventIndexByName;

    // Methods
    std::vector<MethodInfo> methods;
    std::unordered_map<std::string, size_t> methodIndexByName;

    // Factory function for creating instances
    std::function<void*()> factory;
    std::function<void(void*)> destructor;
    std::function<void*(const void*)> copyConstructor;

    // Type hash for fast comparison
    size_t typeHash = 0;

    // Flags
    bool isAbstract = false;
    bool isComponent = false;
    bool isEntity = false;
    bool isResource = false;

    // Constructor
    TypeInfo() : typeIndex(typeid(void)) {}
    TypeInfo(std::string n, std::type_index ti, size_t sz)
        : name(std::move(n)), typeIndex(ti), size(sz) {
        typeHash = std::hash<std::string>{}(name);
    }

    // Property access
    [[nodiscard]] const PropertyInfo* FindProperty(const std::string& propName) const {
        auto it = propertyIndexByName.find(propName);
        if (it != propertyIndexByName.end()) {
            return &properties[it->second];
        }
        // Check base type
        if (baseType) {
            return baseType->FindProperty(propName);
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<const PropertyInfo*> GetAllProperties() const {
        std::vector<const PropertyInfo*> result;
        // Include base type properties first
        if (baseType) {
            auto baseProps = baseType->GetAllProperties();
            result.insert(result.end(), baseProps.begin(), baseProps.end());
        }
        for (const auto& prop : properties) {
            result.push_back(&prop);
        }
        return result;
    }

    void AddProperty(PropertyInfo prop) {
        propertyIndexByName[prop.name] = properties.size();
        properties.push_back(std::move(prop));
    }

    // Event access
    [[nodiscard]] const EventInfo* FindEvent(const std::string& eventName) const {
        auto it = eventIndexByName.find(eventName);
        if (it != eventIndexByName.end()) {
            return &events[it->second];
        }
        if (baseType) {
            return baseType->FindEvent(eventName);
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> GetAllEvents() const {
        std::vector<std::string> result;
        if (baseType) {
            auto baseEvents = baseType->GetAllEvents();
            result.insert(result.end(), baseEvents.begin(), baseEvents.end());
        }
        for (const auto& evt : events) {
            result.push_back(evt.name);
        }
        return result;
    }

    void AddEvent(EventInfo evt) {
        eventIndexByName[evt.name] = events.size();
        events.push_back(std::move(evt));
    }

    // Method access
    [[nodiscard]] const MethodInfo* FindMethod(const std::string& methodName) const {
        auto it = methodIndexByName.find(methodName);
        if (it != methodIndexByName.end()) {
            return &methods[it->second];
        }
        if (baseType) {
            return baseType->FindMethod(methodName);
        }
        return nullptr;
    }

    void AddMethod(MethodInfo method) {
        methodIndexByName[method.name] = methods.size();
        methods.push_back(std::move(method));
    }

    // Instance creation
    [[nodiscard]] void* CreateInstance() const {
        return factory ? factory() : nullptr;
    }

    void DestroyInstance(void* instance) const {
        if (destructor && instance) {
            destructor(instance);
        }
    }

    [[nodiscard]] void* CopyInstance(const void* source) const {
        return copyConstructor ? copyConstructor(source) : nullptr;
    }

    // Type checking
    [[nodiscard]] bool IsA(const TypeInfo* other) const {
        if (!other) return false;
        if (typeHash == other->typeHash) return true;
        if (baseType) return baseType->IsA(other);
        return false;
    }

    [[nodiscard]] bool IsA(const std::string& typeName) const {
        if (name == typeName) return true;
        if (baseType) return baseType->IsA(typeName);
        return false;
    }

    // Builder pattern
    TypeInfo& WithDisplayName(const std::string& dn) { displayName = dn; return *this; }
    TypeInfo& WithDescription(const std::string& desc) { description = desc; return *this; }
    TypeInfo& WithCategory(const std::string& cat) { category = cat; return *this; }
    TypeInfo& AsAbstract() { isAbstract = true; return *this; }
    TypeInfo& AsComponent() { isComponent = true; return *this; }
    TypeInfo& AsEntity() { isEntity = true; return *this; }
    TypeInfo& AsResource() { isResource = true; return *this; }
};

/**
 * @brief Property change event data
 */
struct PropertyChangeEvent {
    const TypeInfo* type = nullptr;
    const PropertyInfo* property = nullptr;
    void* instance = nullptr;
    std::any oldValue;
    std::any newValue;
    std::string propertyPath;
};

/**
 * @brief Callback type for property change notifications
 */
using PropertyChangeCallback = std::function<void(const PropertyChangeEvent&)>;

} // namespace Reflect
} // namespace Nova
