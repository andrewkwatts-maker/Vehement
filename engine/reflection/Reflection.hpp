#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <typeindex>
#include <any>
#include <optional>
#include <expected>
#include <concepts>
#include <span>
#include <shared_mutex>

namespace Nova {
namespace Reflect {

// Forward declarations
class TypeInfo;
class Property;
class Method;

/**
 * @brief Reflection error types
 */
enum class ReflectionError {
    TypeNotFound,
    PropertyNotFound,
    MethodNotFound,
    TypeMismatch,
    AccessDenied,
    InvocationFailed
};

/**
 * @brief Type trait for reflectable types
 */
template<typename T>
concept Reflectable = requires {
    { T::GetStaticTypeInfo() } -> std::same_as<const TypeInfo&>;
};

/**
 * @brief Concept for types that can be used as property types
 */
template<typename T>
concept PropertyType = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

/**
 * @brief Property accessor types
 */
enum class PropertyAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite
};

/**
 * @brief Property metadata with constexpr defaults
 */
struct PropertyMeta {
    std::string displayName;
    std::string description;
    std::string category;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool hasRange = false;
    bool isColor = false;
    bool isAngle = false;    // For radians/degrees conversion
    bool isHidden = false;
    bool isReadOnly = false;

    // Builder-style setters for fluent configuration
    constexpr PropertyMeta& WithDisplayName(std::string name) { displayName = std::move(name); return *this; }
    constexpr PropertyMeta& WithDescription(std::string desc) { description = std::move(desc); return *this; }
    constexpr PropertyMeta& WithCategory(std::string cat) { category = std::move(cat); return *this; }
    constexpr PropertyMeta& WithRange(float min, float max) { minValue = min; maxValue = max; hasRange = true; return *this; }
    constexpr PropertyMeta& AsColor() { isColor = true; return *this; }
    constexpr PropertyMeta& AsAngle() { isAngle = true; return *this; }
    constexpr PropertyMeta& AsHidden() { isHidden = true; return *this; }
    constexpr PropertyMeta& AsReadOnly() { isReadOnly = true; return *this; }
};

/**
 * @brief Type-erased property accessor with improved error handling
 */
class Property {
public:
    using Getter = std::function<std::any(const void*)>;
    using Setter = std::function<void(void*, const std::any&)>;

    Property(std::string_view name, std::type_index type,
             Getter getter, Setter setter = nullptr,
             PropertyMeta meta = {})
        : m_name(name)
        , m_type(type)
        , m_getter(std::move(getter))
        , m_setter(std::move(setter))
        , m_meta(std::move(meta))
        , m_access(setter ? PropertyAccess::ReadWrite : PropertyAccess::ReadOnly)
    {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    [[nodiscard]] std::type_index GetType() const noexcept { return m_type; }
    [[nodiscard]] PropertyAccess GetAccess() const noexcept { return m_access; }
    [[nodiscard]] const PropertyMeta& GetMeta() const noexcept { return m_meta; }
    [[nodiscard]] bool IsReadOnly() const noexcept { return m_access == PropertyAccess::ReadOnly || m_meta.isReadOnly; }
    [[nodiscard]] bool IsWritable() const noexcept { return m_setter != nullptr && !m_meta.isReadOnly; }

    /**
     * @brief Get property value with type checking
     * @return Expected value or error
     */
    template<PropertyType T>
    [[nodiscard]] std::expected<T, ReflectionError> Get(const void* instance) const {
        if (std::type_index(typeid(T)) != m_type) {
            return std::unexpected(ReflectionError::TypeMismatch);
        }
        try {
            return std::any_cast<T>(m_getter(instance));
        } catch (...) {
            return std::unexpected(ReflectionError::TypeMismatch);
        }
    }

    /**
     * @brief Get property value, returning optional (legacy compatibility)
     */
    template<PropertyType T>
    [[nodiscard]] std::optional<T> GetOptional(const void* instance) const {
        if (std::type_index(typeid(T)) != m_type) {
            return std::nullopt;
        }
        try {
            return std::any_cast<T>(m_getter(instance));
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Set property value with type checking
     * @return Expected success or error
     */
    template<PropertyType T>
    std::expected<void, ReflectionError> Set(void* instance, const T& value) {
        if (!m_setter) {
            return std::unexpected(ReflectionError::AccessDenied);
        }
        if (std::type_index(typeid(T)) != m_type) {
            return std::unexpected(ReflectionError::TypeMismatch);
        }
        try {
            m_setter(instance, std::any(value));
            return {};
        } catch (...) {
            return std::unexpected(ReflectionError::InvocationFailed);
        }
    }

    /**
     * @brief Set property value, returning bool (legacy compatibility)
     */
    template<PropertyType T>
    bool TrySet(void* instance, const T& value) {
        return Set(instance, value).has_value();
    }

    [[nodiscard]] std::any GetAny(const void* instance) const {
        return m_getter(instance);
    }

    std::expected<void, ReflectionError> SetAny(void* instance, const std::any& value) {
        if (!m_setter) {
            return std::unexpected(ReflectionError::AccessDenied);
        }
        try {
            m_setter(instance, value);
            return {};
        } catch (...) {
            return std::unexpected(ReflectionError::InvocationFailed);
        }
    }

private:
    std::string m_name;
    std::type_index m_type;
    Getter m_getter;
    Setter m_setter;
    PropertyMeta m_meta;
    PropertyAccess m_access;
};

/**
 * @brief Type-erased method wrapper with improved error handling
 */
class Method {
public:
    using Invoker = std::function<std::any(void*, const std::vector<std::any>&)>;

    Method(std::string_view name, Invoker invoker,
           std::vector<std::type_index> paramTypes,
           std::type_index returnType)
        : m_name(name)
        , m_invoker(std::move(invoker))
        , m_paramTypes(std::move(paramTypes))
        , m_returnType(returnType)
    {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    [[nodiscard]] std::span<const std::type_index> GetParamTypes() const noexcept { return m_paramTypes; }
    [[nodiscard]] std::type_index GetReturnType() const noexcept { return m_returnType; }
    [[nodiscard]] size_t GetParamCount() const noexcept { return m_paramTypes.size(); }

    /**
     * @brief Invoke method with argument validation
     */
    [[nodiscard]] std::expected<std::any, ReflectionError> Invoke(void* instance, const std::vector<std::any>& args = {}) const {
        if (args.size() != m_paramTypes.size()) {
            return std::unexpected(ReflectionError::InvocationFailed);
        }
        try {
            return m_invoker(instance, args);
        } catch (...) {
            return std::unexpected(ReflectionError::InvocationFailed);
        }
    }

    /**
     * @brief Invoke method without validation (legacy compatibility)
     */
    std::any InvokeUnchecked(void* instance, const std::vector<std::any>& args = {}) const {
        return m_invoker(instance, args);
    }

private:
    std::string m_name;
    Invoker m_invoker;
    std::vector<std::type_index> m_paramTypes;
    std::type_index m_returnType;
};

/**
 * @brief Runtime type information for a class with improved API
 */
class TypeInfo {
public:
    TypeInfo(std::string_view name, std::type_index type, size_t size)
        : m_name(name)
        , m_type(type)
        , m_size(size)
    {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    [[nodiscard]] std::type_index GetType() const noexcept { return m_type; }
    [[nodiscard]] size_t GetSize() const noexcept { return m_size; }
    [[nodiscard]] size_t GetPropertyCount() const noexcept { return m_properties.size(); }
    [[nodiscard]] size_t GetMethodCount() const noexcept { return m_methods.size(); }
    [[nodiscard]] bool HasBase() const noexcept { return m_baseType != nullptr; }

    // Property access
    void AddProperty(Property prop) {
        m_properties.try_emplace(prop.GetName(), std::move(prop));
    }

    [[nodiscard]] std::expected<const Property*, ReflectionError> GetProperty(std::string_view name) const {
        auto it = m_properties.find(std::string(name));
        if (it != m_properties.end()) {
            return &it->second;
        }
        // Check base class
        if (m_baseType) {
            return m_baseType->GetProperty(name);
        }
        return std::unexpected(ReflectionError::PropertyNotFound);
    }

    [[nodiscard]] const Property* FindProperty(std::string_view name) const noexcept {
        auto it = m_properties.find(std::string(name));
        if (it != m_properties.end()) {
            return &it->second;
        }
        return m_baseType ? m_baseType->FindProperty(name) : nullptr;
    }

    [[nodiscard]] std::vector<const Property*> GetProperties() const {
        std::vector<const Property*> props;
        props.reserve(m_properties.size());
        for (const auto& [name, prop] : m_properties) {
            props.push_back(&prop);
        }
        return props;
    }

    [[nodiscard]] std::vector<const Property*> GetAllProperties() const {
        std::vector<const Property*> props;
        if (m_baseType) {
            props = m_baseType->GetAllProperties();
        }
        props.reserve(props.size() + m_properties.size());
        for (const auto& [name, prop] : m_properties) {
            props.push_back(&prop);
        }
        return props;
    }

    // Method access
    void AddMethod(Method method) {
        m_methods.try_emplace(method.GetName(), std::move(method));
    }

    [[nodiscard]] std::expected<const Method*, ReflectionError> GetMethod(std::string_view name) const {
        auto it = m_methods.find(std::string(name));
        if (it != m_methods.end()) {
            return &it->second;
        }
        if (m_baseType) {
            return m_baseType->GetMethod(name);
        }
        return std::unexpected(ReflectionError::MethodNotFound);
    }

    [[nodiscard]] const Method* FindMethod(std::string_view name) const noexcept {
        auto it = m_methods.find(std::string(name));
        if (it != m_methods.end()) {
            return &it->second;
        }
        return m_baseType ? m_baseType->FindMethod(name) : nullptr;
    }

    [[nodiscard]] std::vector<const Method*> GetMethods() const {
        std::vector<const Method*> methods;
        methods.reserve(m_methods.size());
        for (const auto& [name, method] : m_methods) {
            methods.push_back(&method);
        }
        return methods;
    }

    // Base class
    void SetBaseType(const TypeInfo* base) noexcept { m_baseType = base; }
    [[nodiscard]] const TypeInfo* GetBaseType() const noexcept { return m_baseType; }

    /**
     * @brief Check if this type derives from another type
     */
    [[nodiscard]] bool DerivedFrom(const TypeInfo* other) const noexcept {
        if (!other) return false;
        if (m_type == other->m_type) return true;
        return m_baseType ? m_baseType->DerivedFrom(other) : false;
    }

    // Factory function
    using Factory = std::function<void*()>;
    void SetFactory(Factory factory) { m_factory = std::move(factory); }
    [[nodiscard]] bool HasFactory() const noexcept { return m_factory != nullptr; }

    [[nodiscard]] void* CreateInstance() const {
        return m_factory ? m_factory() : nullptr;
    }

    template<typename T>
    [[nodiscard]] std::unique_ptr<T> Create() const {
        return std::unique_ptr<T>(static_cast<T*>(CreateInstance()));
    }

private:
    std::string m_name;
    std::type_index m_type;
    size_t m_size;
    const TypeInfo* m_baseType = nullptr;
    Factory m_factory;
    std::unordered_map<std::string, Property> m_properties;
    std::unordered_map<std::string, Method> m_methods;
};

/**
 * @brief Global type registry with thread-safe operations
 */
class TypeRegistry {
public:
    static TypeRegistry& Instance() {
        static TypeRegistry instance;
        return instance;
    }

    // Delete copy/move
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;

    template<typename T>
    TypeInfo& RegisterType(std::string_view name) {
        auto type = std::type_index(typeid(T));
        std::unique_lock lock(m_mutex);

        auto [it, inserted] = m_types.try_emplace(
            type,
            std::make_unique<TypeInfo>(name, type, sizeof(T))
        );

        // Set up factory for default constructible types
        if constexpr (std::is_default_constructible_v<T>) {
            it->second->SetFactory([]() -> void* { return new T(); });
        }

        m_typesByName[std::string(name)] = it->second.get();
        return *it->second;
    }

    template<typename T>
    [[nodiscard]] const TypeInfo* GetType() const {
        std::shared_lock lock(m_mutex);
        auto it = m_types.find(std::type_index(typeid(T)));
        return it != m_types.end() ? it->second.get() : nullptr;
    }

    [[nodiscard]] std::expected<const TypeInfo*, ReflectionError> GetType(std::string_view name) const {
        std::shared_lock lock(m_mutex);
        auto it = m_typesByName.find(std::string(name));
        if (it != m_typesByName.end()) {
            return it->second;
        }
        return std::unexpected(ReflectionError::TypeNotFound);
    }

    [[nodiscard]] const TypeInfo* FindType(std::string_view name) const noexcept {
        std::shared_lock lock(m_mutex);
        auto it = m_typesByName.find(std::string(name));
        return it != m_typesByName.end() ? it->second : nullptr;
    }

    [[nodiscard]] const TypeInfo* FindType(std::type_index type) const noexcept {
        std::shared_lock lock(m_mutex);
        auto it = m_types.find(type);
        return it != m_types.end() ? it->second.get() : nullptr;
    }

    [[nodiscard]] std::vector<const TypeInfo*> GetAllTypes() const {
        std::shared_lock lock(m_mutex);
        std::vector<const TypeInfo*> types;
        types.reserve(m_types.size());
        for (const auto& [_, info] : m_types) {
            types.push_back(info.get());
        }
        return types;
    }

    [[nodiscard]] size_t GetTypeCount() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_types.size();
    }

    [[nodiscard]] bool IsRegistered(std::type_index type) const noexcept {
        std::shared_lock lock(m_mutex);
        return m_types.contains(type);
    }

    template<typename T>
    [[nodiscard]] bool IsRegistered() const noexcept {
        return IsRegistered(std::type_index(typeid(T)));
    }

private:
    TypeRegistry() = default;
    std::unordered_map<std::type_index, std::unique_ptr<TypeInfo>> m_types;
    std::unordered_map<std::string, TypeInfo*> m_typesByName;
    mutable std::shared_mutex m_mutex;
};

/**
 * @brief Helper class for building type reflection info
 */
template<typename T>
class TypeBuilder {
public:
    explicit TypeBuilder(TypeInfo& info) : m_info(info) {}

    template<typename MemberT>
    TypeBuilder& Property(std::string_view name,
                          MemberT T::* member,
                          PropertyMeta meta = {}) {
        m_info.AddProperty(Reflect::Property(
            name,
            std::type_index(typeid(MemberT)),
            [member](const void* instance) -> std::any {
                return static_cast<const T*>(instance)->*member;
            },
            [member](void* instance, const std::any& value) {
                static_cast<T*>(instance)->*member = std::any_cast<MemberT>(value);
            },
            std::move(meta)
        ));
        return *this;
    }

    template<typename MemberT>
    TypeBuilder& ReadOnlyProperty(std::string_view name,
                                   MemberT T::* member,
                                   PropertyMeta meta = {}) {
        meta.isReadOnly = true;
        m_info.AddProperty(Reflect::Property(
            name,
            std::type_index(typeid(MemberT)),
            [member](const void* instance) -> std::any {
                return static_cast<const T*>(instance)->*member;
            },
            nullptr,
            std::move(meta)
        ));
        return *this;
    }

    template<typename GetterT, typename SetterT>
    TypeBuilder& Property(std::string_view name,
                          GetterT getter, SetterT setter,
                          PropertyMeta meta = {}) {
        using ReturnT = std::invoke_result_t<GetterT, const T*>;
        m_info.AddProperty(Reflect::Property(
            name,
            std::type_index(typeid(std::remove_cvref_t<ReturnT>)),
            [getter](const void* instance) -> std::any {
                return getter(static_cast<const T*>(instance));
            },
            [setter](void* instance, const std::any& value) {
                setter(static_cast<T*>(instance),
                       std::any_cast<std::remove_cvref_t<ReturnT>>(value));
            },
            std::move(meta)
        ));
        return *this;
    }

    template<typename BaseT>
    TypeBuilder& Base() {
        auto baseInfo = TypeRegistry::Instance().GetType<BaseT>();
        m_info.SetBaseType(baseInfo);
        return *this;
    }

private:
    TypeInfo& m_info;
};

/**
 * @brief Helper macro for declaring reflection in a class
 */
#define NOVA_REFLECT_TYPE(TypeName) \
    static const Nova::Reflect::TypeInfo& GetStaticTypeInfo() { \
        static const Nova::Reflect::TypeInfo& info = \
            Nova::Reflect::TypeRegistry::Instance().GetType<TypeName>() \
            ? *Nova::Reflect::TypeRegistry::Instance().GetType<TypeName>() \
            : Nova::Reflect::TypeRegistry::Instance().RegisterType<TypeName>(#TypeName); \
        return info; \
    } \
    virtual const Nova::Reflect::TypeInfo& GetTypeInfo() const { \
        return GetStaticTypeInfo(); \
    }

/**
 * @brief Helper function to start building type reflection
 */
template<typename T>
TypeBuilder<T> BuildType(std::string_view name) {
    auto& info = TypeRegistry::Instance().RegisterType<T>(name);
    return TypeBuilder<T>(info);
}

} // namespace Reflect
} // namespace Nova
