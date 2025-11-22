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
#include <concepts>

namespace Nova {
namespace Reflect {

// Forward declarations
class TypeInfo;
class Property;
class Method;

/**
 * @brief Type trait for reflectable types
 */
template<typename T>
concept Reflectable = requires {
    { T::GetStaticTypeInfo() } -> std::same_as<const TypeInfo&>;
};

/**
 * @brief Property accessor types
 */
enum class PropertyAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite
};

/**
 * @brief Property metadata
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
};

/**
 * @brief Type-erased property accessor
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

    const std::string& GetName() const { return m_name; }
    std::type_index GetType() const { return m_type; }
    PropertyAccess GetAccess() const { return m_access; }
    const PropertyMeta& GetMeta() const { return m_meta; }

    template<typename T>
    std::optional<T> Get(const void* instance) const {
        if (std::type_index(typeid(T)) != m_type) {
            return std::nullopt;
        }
        try {
            return std::any_cast<T>(m_getter(instance));
        } catch (...) {
            return std::nullopt;
        }
    }

    template<typename T>
    bool Set(void* instance, const T& value) {
        if (!m_setter || std::type_index(typeid(T)) != m_type) {
            return false;
        }
        try {
            m_setter(instance, std::any(value));
            return true;
        } catch (...) {
            return false;
        }
    }

    std::any GetAny(const void* instance) const {
        return m_getter(instance);
    }

    bool SetAny(void* instance, const std::any& value) {
        if (!m_setter) return false;
        try {
            m_setter(instance, value);
            return true;
        } catch (...) {
            return false;
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
 * @brief Type-erased method wrapper
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

    const std::string& GetName() const { return m_name; }
    const std::vector<std::type_index>& GetParamTypes() const { return m_paramTypes; }
    std::type_index GetReturnType() const { return m_returnType; }

    std::any Invoke(void* instance, const std::vector<std::any>& args = {}) const {
        return m_invoker(instance, args);
    }

private:
    std::string m_name;
    Invoker m_invoker;
    std::vector<std::type_index> m_paramTypes;
    std::type_index m_returnType;
};

/**
 * @brief Runtime type information for a class
 */
class TypeInfo {
public:
    TypeInfo(std::string_view name, std::type_index type, size_t size)
        : m_name(name)
        , m_type(type)
        , m_size(size)
    {}

    const std::string& GetName() const { return m_name; }
    std::type_index GetType() const { return m_type; }
    size_t GetSize() const { return m_size; }

    // Property access
    void AddProperty(Property prop) {
        m_properties[prop.GetName()] = std::move(prop);
    }

    const Property* GetProperty(const std::string& name) const {
        auto it = m_properties.find(name);
        return it != m_properties.end() ? &it->second : nullptr;
    }

    std::vector<const Property*> GetProperties() const {
        std::vector<const Property*> props;
        for (const auto& [name, prop] : m_properties) {
            props.push_back(&prop);
        }
        return props;
    }

    // Method access
    void AddMethod(Method method) {
        m_methods[method.GetName()] = std::move(method);
    }

    const Method* GetMethod(const std::string& name) const {
        auto it = m_methods.find(name);
        return it != m_methods.end() ? &it->second : nullptr;
    }

    std::vector<const Method*> GetMethods() const {
        std::vector<const Method*> methods;
        for (const auto& [name, method] : m_methods) {
            methods.push_back(&method);
        }
        return methods;
    }

    // Base class
    void SetBaseType(const TypeInfo* base) { m_baseType = base; }
    const TypeInfo* GetBaseType() const { return m_baseType; }

    // Factory function
    using Factory = std::function<void*()>;
    void SetFactory(Factory factory) { m_factory = std::move(factory); }

    void* CreateInstance() const {
        return m_factory ? m_factory() : nullptr;
    }

    template<typename T>
    std::unique_ptr<T> Create() const {
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
 * @brief Global type registry
 */
class TypeRegistry {
public:
    static TypeRegistry& Instance() {
        static TypeRegistry instance;
        return instance;
    }

    template<typename T>
    TypeInfo& RegisterType(std::string_view name) {
        auto type = std::type_index(typeid(T));
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
    const TypeInfo* GetType() const {
        auto it = m_types.find(std::type_index(typeid(T)));
        return it != m_types.end() ? it->second.get() : nullptr;
    }

    const TypeInfo* GetType(const std::string& name) const {
        auto it = m_typesByName.find(name);
        return it != m_typesByName.end() ? it->second : nullptr;
    }

    const TypeInfo* GetType(std::type_index type) const {
        auto it = m_types.find(type);
        return it != m_types.end() ? it->second.get() : nullptr;
    }

    std::vector<const TypeInfo*> GetAllTypes() const {
        std::vector<const TypeInfo*> types;
        for (const auto& [_, info] : m_types) {
            types.push_back(info.get());
        }
        return types;
    }

private:
    TypeRegistry() = default;
    std::unordered_map<std::type_index, std::unique_ptr<TypeInfo>> m_types;
    std::unordered_map<std::string, TypeInfo*> m_typesByName;
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
