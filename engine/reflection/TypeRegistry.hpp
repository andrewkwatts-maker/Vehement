#pragma once

#include "TypeInfo.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Nova {
namespace Reflect {

/**
 * @brief Global registry for runtime type information
 *
 * Singleton registry that maintains information about all reflected types in the engine.
 * Thread-safe for concurrent read access with exclusive write access.
 *
 * Usage:
 * @code
 * // Register a type
 * auto& registry = TypeRegistry::Instance();
 * registry.RegisterType<MyClass>("MyClass");
 *
 * // Query type info
 * const TypeInfo* info = registry.GetType("MyClass");
 * @endcode
 */
class TypeRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static TypeRegistry& Instance();

    // Non-copyable, non-movable
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;

    // =========================================================================
    // Type Registration
    // =========================================================================

    /**
     * @brief Register a type with the registry
     * @tparam T Type to register
     * @param name Type name
     * @return Reference to the created TypeInfo
     */
    template<typename T>
    TypeInfo& RegisterType(const std::string& name);

    /**
     * @brief Register a type derived from a base type
     * @tparam T Derived type
     * @tparam Base Base type
     * @param name Type name
     * @return Reference to the created TypeInfo
     */
    template<typename T, typename Base>
    TypeInfo& RegisterDerivedType(const std::string& name);

    /**
     * @brief Check if a type is registered
     * @param name Type name
     * @return true if type is registered
     */
    [[nodiscard]] bool IsRegistered(const std::string& name) const;

    /**
     * @brief Check if a type is registered by type_index
     */
    template<typename T>
    [[nodiscard]] bool IsRegistered() const;

    // =========================================================================
    // Type Queries
    // =========================================================================

    /**
     * @brief Get type info by name
     * @param name Type name
     * @return Pointer to TypeInfo or nullptr if not found
     */
    [[nodiscard]] const TypeInfo* GetType(const std::string& name) const;

    /**
     * @brief Get type info by type_index
     */
    template<typename T>
    [[nodiscard]] const TypeInfo* GetType() const;

    /**
     * @brief Get type info by hash
     */
    [[nodiscard]] const TypeInfo* GetTypeByHash(size_t hash) const;

    /**
     * @brief Get mutable type info for registration
     */
    [[nodiscard]] TypeInfo* GetMutableType(const std::string& name);

    /**
     * @brief Get all registered types
     */
    [[nodiscard]] std::vector<const TypeInfo*> GetAllTypes() const;

    /**
     * @brief Get types by category
     */
    [[nodiscard]] std::vector<const TypeInfo*> GetTypesByCategory(const std::string& category) const;

    /**
     * @brief Get types derived from a base type
     */
    [[nodiscard]] std::vector<const TypeInfo*> GetDerivedTypes(const std::string& baseTypeName) const;

    /**
     * @brief Get all component types
     */
    [[nodiscard]] std::vector<const TypeInfo*> GetComponentTypes() const;

    /**
     * @brief Get all entity types
     */
    [[nodiscard]] std::vector<const TypeInfo*> GetEntityTypes() const;

    /**
     * @brief Get number of registered types
     */
    [[nodiscard]] size_t GetTypeCount() const;

    // =========================================================================
    // Type Iteration
    // =========================================================================

    /**
     * @brief Iterate over all registered types
     * @param callback Function to call for each type
     */
    void ForEachType(const std::function<void(const TypeInfo&)>& callback) const;

    /**
     * @brief Iterate over types matching a predicate
     */
    void ForEachTypeWhere(const std::function<bool(const TypeInfo&)>& predicate,
                          const std::function<void(const TypeInfo&)>& callback) const;

    // =========================================================================
    // Instance Creation
    // =========================================================================

    /**
     * @brief Create an instance of a type by name
     * @param name Type name
     * @return Pointer to new instance or nullptr
     */
    [[nodiscard]] void* CreateInstance(const std::string& name) const;

    /**
     * @brief Create a typed instance
     */
    template<typename T>
    [[nodiscard]] std::unique_ptr<T> Create(const std::string& name) const;

    // =========================================================================
    // Property Change Notifications
    // =========================================================================

    /**
     * @brief Register a global property change listener
     * @param callback Callback to invoke on property changes
     * @return Listener ID for unregistration
     */
    size_t RegisterPropertyChangeListener(PropertyChangeCallback callback);

    /**
     * @brief Unregister a property change listener
     */
    void UnregisterPropertyChangeListener(size_t listenerId);

    /**
     * @brief Notify listeners of a property change
     */
    void NotifyPropertyChange(const PropertyChangeEvent& event);

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;

    // Type storage
    std::unordered_map<std::string, std::unique_ptr<TypeInfo>> m_types;
    std::unordered_map<std::type_index, TypeInfo*> m_typesByIndex;
    std::unordered_map<size_t, TypeInfo*> m_typesByHash;

    // Property change listeners
    std::unordered_map<size_t, PropertyChangeCallback> m_propertyChangeListeners;
    size_t m_nextListenerId = 1;

    // Thread safety
    mutable std::shared_mutex m_mutex;
    mutable std::mutex m_listenerMutex;
};

// ============================================================================
// Reflection Macros
// ============================================================================

/**
 * @brief Register a type with the reflection system
 * Usage: REFLECT_TYPE(MyClass)
 */
#define REFLECT_TYPE(TypeName) \
    namespace { \
        static const bool _reflected_##TypeName = []() { \
            Nova::Reflect::TypeRegistry::Instance().RegisterType<TypeName>(#TypeName); \
            return true; \
        }(); \
    }

/**
 * @brief Register a derived type
 * Usage: REFLECT_DERIVED_TYPE(DerivedClass, BaseClass)
 */
#define REFLECT_DERIVED_TYPE(TypeName, BaseType) \
    namespace { \
        static const bool _reflected_##TypeName = []() { \
            Nova::Reflect::TypeRegistry::Instance().RegisterDerivedType<TypeName, BaseType>(#TypeName); \
            return true; \
        }(); \
    }

/**
 * @brief Begin property registration for a type
 * Usage: REFLECT_TYPE_BEGIN(MyClass)
 */
#define REFLECT_TYPE_BEGIN(TypeName) \
    namespace { \
        static const bool _props_##TypeName = []() { \
            auto* typeInfo = Nova::Reflect::TypeRegistry::Instance().GetMutableType(#TypeName); \
            if (!typeInfo) return false; \
            using ReflectedType = TypeName;

/**
 * @brief Register a property
 * Usage: REFLECT_PROPERTY(propertyName, attributes)
 */
#define REFLECT_PROPERTY(PropName, ...) \
    { \
        Nova::Reflect::PropertyInfo prop(#PropName, typeid(decltype(ReflectedType::PropName)).name(), \
            std::type_index(typeid(decltype(ReflectedType::PropName)))); \
        prop.offset = offsetof(ReflectedType, PropName); \
        prop.size = sizeof(decltype(ReflectedType::PropName)); \
        prop.setterAny = [](void* instance, const std::any& value) { \
            static_cast<ReflectedType*>(instance)->PropName = \
                std::any_cast<std::decay_t<decltype(ReflectedType::PropName)>>(value); \
        }; \
        prop.getterAny = [](const void* instance) -> std::any { \
            return static_cast<const ReflectedType*>(instance)->PropName; \
        }; \
        __VA_ARGS__ \
        typeInfo->AddProperty(std::move(prop)); \
    }

/**
 * @brief Register a property with getter/setter functions
 */
#define REFLECT_PROPERTY_ACCESSORS(PropName, GetterFunc, SetterFunc, ...) \
    { \
        Nova::Reflect::PropertyInfo prop(#PropName, "", std::type_index(typeid(void))); \
        prop.setterAny = [](void* instance, const std::any& value) { \
            static_cast<ReflectedType*>(instance)->SetterFunc(std::any_cast<decltype(std::declval<ReflectedType>().GetterFunc())>(value)); \
        }; \
        prop.getterAny = [](const void* instance) -> std::any { \
            return static_cast<const ReflectedType*>(instance)->GetterFunc(); \
        }; \
        __VA_ARGS__ \
        typeInfo->AddProperty(std::move(prop)); \
    }

/**
 * @brief Register an event the type can emit
 * Usage: REFLECT_EVENT(OnDamage)
 */
#define REFLECT_EVENT(EventName, ...) \
    { \
        Nova::Reflect::EventInfo evt(#EventName); \
        __VA_ARGS__ \
        typeInfo->AddEvent(std::move(evt)); \
    }

/**
 * @brief End property registration
 */
#define REFLECT_TYPE_END() \
            return true; \
        }(); \
    }

/**
 * @brief Add an attribute to property (use inside REFLECT_PROPERTY)
 */
#define REFLECT_ATTR(AttrValue) prop.WithAttributeString(#AttrValue);
#define REFLECT_ATTR_EDITABLE prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::Editable;
#define REFLECT_ATTR_OBSERVABLE prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::Observable;
#define REFLECT_ATTR_REPLICATED prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::Replicated;
#define REFLECT_ATTR_SERIALIZED prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::Serialized;
#define REFLECT_ATTR_HIDDEN prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::Hidden;
#define REFLECT_ATTR_READONLY prop.attributes = prop.attributes | Nova::Reflect::PropertyAttribute::ReadOnly;
#define REFLECT_ATTR_RANGE(Min, Max) prop.WithRange(Min, Max);
#define REFLECT_ATTR_CATEGORY(Cat) prop.WithCategory(Cat);
#define REFLECT_ATTR_DISPLAY(Name) prop.WithDisplayName(Name);
#define REFLECT_ATTR_DESC(Desc) prop.WithDescription(Desc);

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
TypeInfo& TypeRegistry::RegisterType(const std::string& name) {
    std::unique_lock lock(m_mutex);

    // Check if already registered
    auto it = m_types.find(name);
    if (it != m_types.end()) {
        return *it->second;
    }

    // Create new TypeInfo
    auto info = std::make_unique<TypeInfo>(name, std::type_index(typeid(T)), sizeof(T));
    info->alignment = alignof(T);
    info->typeHash = std::hash<std::string>{}(name);

    // Setup factory for default constructible types
    if constexpr (std::is_default_constructible_v<T>) {
        info->factory = []() -> void* { return new T(); };
    }

    // Setup destructor
    info->destructor = [](void* ptr) { delete static_cast<T*>(ptr); };

    // Setup copy constructor for copyable types
    if constexpr (std::is_copy_constructible_v<T>) {
        info->copyConstructor = [](const void* src) -> void* {
            return new T(*static_cast<const T*>(src));
        };
    }

    TypeInfo& result = *info;
    m_types[name] = std::move(info);
    m_typesByIndex[std::type_index(typeid(T))] = &result;
    m_typesByHash[result.typeHash] = &result;

    return result;
}

template<typename T, typename Base>
TypeInfo& TypeRegistry::RegisterDerivedType(const std::string& name) {
    auto& info = RegisterType<T>(name);

    // Link to base type
    info.baseTypeName = typeid(Base).name();
    auto baseIt = m_typesByIndex.find(std::type_index(typeid(Base)));
    if (baseIt != m_typesByIndex.end()) {
        info.baseType = baseIt->second;
    }

    return info;
}

template<typename T>
bool TypeRegistry::IsRegistered() const {
    std::shared_lock lock(m_mutex);
    return m_typesByIndex.contains(std::type_index(typeid(T)));
}

template<typename T>
const TypeInfo* TypeRegistry::GetType() const {
    std::shared_lock lock(m_mutex);
    auto it = m_typesByIndex.find(std::type_index(typeid(T)));
    return it != m_typesByIndex.end() ? it->second : nullptr;
}

template<typename T>
std::unique_ptr<T> TypeRegistry::Create(const std::string& name) const {
    void* ptr = CreateInstance(name);
    return std::unique_ptr<T>(static_cast<T*>(ptr));
}

} // namespace Reflect
} // namespace Nova
