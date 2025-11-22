#pragma once

#include "ILifecycle.hpp"
#include "GameEvent.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <memory>
#include <functional>
#include <string>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Component ID
// ============================================================================

/**
 * @brief Unique component type identifier
 */
using ComponentTypeId = uint32_t;

/**
 * @brief Generate component type ID from type
 */
template<typename T>
ComponentTypeId GetComponentTypeId() {
    static ComponentTypeId id = []() {
        static ComponentTypeId counter = 0;
        return ++counter;
    }();
    return id;
}

// ============================================================================
// IComponent Interface
// ============================================================================

/**
 * @brief Base interface for ECS components
 *
 * Components are data containers that can be attached to entities.
 * They follow the lifecycle pattern for initialization and cleanup.
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Called when component is attached to an entity
     */
    virtual void OnAttach(LifecycleHandle owner) {
        m_owner = owner;
        m_initialized = false;
    }

    /**
     * @brief Called to initialize the component (may be deferred)
     */
    virtual void OnInitialize() {
        m_initialized = true;
    }

    /**
     * @brief Called each tick if component is tickable
     */
    virtual void OnTick(float deltaTime) {
        (void)deltaTime;
    }

    /**
     * @brief Called when an event is received
     */
    virtual bool OnEvent(const GameEvent& event) {
        (void)event;
        return false;
    }

    /**
     * @brief Called when component is detached from entity
     */
    virtual void OnDetach() {
        m_owner = LifecycleHandle::Invalid;
    }

    // =========================================================================
    // State
    // =========================================================================

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] LifecycleHandle GetOwner() const { return m_owner; }

    // =========================================================================
    // Type Info
    // =========================================================================

    [[nodiscard]] virtual ComponentTypeId GetTypeId() const = 0;
    [[nodiscard]] virtual const char* GetTypeName() const = 0;

protected:
    LifecycleHandle m_owner = LifecycleHandle::Invalid;
    bool m_initialized = false;
    bool m_enabled = true;
};

// ============================================================================
// ComponentBase Template
// ============================================================================

/**
 * @brief CRTP base for components with automatic type ID
 */
template<typename Derived>
class ComponentBase : public IComponent {
public:
    [[nodiscard]] ComponentTypeId GetTypeId() const override {
        return GetComponentTypeId<Derived>();
    }

    [[nodiscard]] const char* GetTypeName() const override {
        return typeid(Derived).name();
    }

    static ComponentTypeId StaticTypeId() {
        return GetComponentTypeId<Derived>();
    }
};

// ============================================================================
// Component Dependencies
// ============================================================================

/**
 * @brief Describes dependencies between components
 */
struct ComponentDependency {
    ComponentTypeId typeId;
    bool required = true;      // Hard dependency vs soft
    bool initBefore = true;    // Must init before this component
};

/**
 * @brief Interface for declaring component dependencies
 */
class IComponentDependencies {
public:
    virtual ~IComponentDependencies() = default;
    virtual std::vector<ComponentDependency> GetDependencies() const { return {}; }
};

// ============================================================================
// Component Container
// ============================================================================

/**
 * @brief Container for components attached to an entity
 *
 * Features:
 * - Fast component lookup by type
 * - Dependency resolution
 * - Lazy initialization
 * - Tick management
 */
class ComponentContainer {
public:
    ComponentContainer();
    explicit ComponentContainer(LifecycleHandle owner);
    ~ComponentContainer();

    // Disable copy, allow move
    ComponentContainer(const ComponentContainer&) = delete;
    ComponentContainer& operator=(const ComponentContainer&) = delete;
    ComponentContainer(ComponentContainer&&) noexcept = default;
    ComponentContainer& operator=(ComponentContainer&&) noexcept = default;

    // =========================================================================
    // Component Management
    // =========================================================================

    /**
     * @brief Add a component
     */
    template<typename T, typename... Args>
    T* Add(Args&&... args);

    /**
     * @brief Add a component by type ID
     */
    bool Add(ComponentTypeId typeId, std::unique_ptr<IComponent> component);

    /**
     * @brief Remove a component
     */
    template<typename T>
    bool Remove();

    /**
     * @brief Remove component by type ID
     */
    bool Remove(ComponentTypeId typeId);

    /**
     * @brief Get a component
     */
    template<typename T>
    T* Get();

    template<typename T>
    const T* Get() const;

    /**
     * @brief Get component by type ID
     */
    IComponent* Get(ComponentTypeId typeId);
    const IComponent* Get(ComponentTypeId typeId) const;

    /**
     * @brief Check if component exists
     */
    template<typename T>
    bool Has() const;

    bool Has(ComponentTypeId typeId) const;

    /**
     * @brief Get all components
     */
    std::vector<IComponent*> GetAll();
    std::vector<const IComponent*> GetAll() const;

    /**
     * @brief Get component count
     */
    [[nodiscard]] size_t Count() const { return m_components.size(); }

    /**
     * @brief Clear all components
     */
    void Clear();

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize all components (respects dependencies)
     */
    void InitializeAll();

    /**
     * @brief Tick all tickable components
     */
    void TickAll(float deltaTime);

    /**
     * @brief Send event to all components
     */
    bool SendEvent(const GameEvent& event);

    /**
     * @brief Set owner handle
     */
    void SetOwner(LifecycleHandle owner);

    // =========================================================================
    // Dependency Management
    // =========================================================================

    /**
     * @brief Resolve and validate dependencies
     */
    bool ResolveDependencies();

    /**
     * @brief Get missing dependencies
     */
    std::vector<ComponentTypeId> GetMissingDependencies() const;

private:
    LifecycleHandle m_owner = LifecycleHandle::Invalid;
    std::unordered_map<ComponentTypeId, std::unique_ptr<IComponent>> m_components;
    std::vector<ComponentTypeId> m_initOrder; // Topologically sorted
    bool m_dependenciesResolved = false;

    void TopologicalSort();
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T, typename... Args>
T* ComponentContainer::Add(Args&&... args) {
    static_assert(std::is_base_of_v<IComponent, T>,
                  "Type must derive from IComponent");

    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = component.get();

    component->OnAttach(m_owner);
    m_components[GetComponentTypeId<T>()] = std::move(component);
    m_dependenciesResolved = false;

    return ptr;
}

template<typename T>
bool ComponentContainer::Remove() {
    return Remove(GetComponentTypeId<T>());
}

template<typename T>
T* ComponentContainer::Get() {
    auto it = m_components.find(GetComponentTypeId<T>());
    return it != m_components.end() ? static_cast<T*>(it->second.get()) : nullptr;
}

template<typename T>
const T* ComponentContainer::Get() const {
    auto it = m_components.find(GetComponentTypeId<T>());
    return it != m_components.end() ? static_cast<const T*>(it->second.get()) : nullptr;
}

template<typename T>
bool ComponentContainer::Has() const {
    return Has(GetComponentTypeId<T>());
}

// ============================================================================
// Common Components
// ============================================================================

/**
 * @brief Transform component - position, rotation, scale
 */
class TransformComponent : public ComponentBase<TransformComponent> {
public:
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};    // Euler angles
    glm::vec3 scale{1.0f};

    void OnInitialize() override;
    void OnTick(float deltaTime) override;
    [[nodiscard]] const char* GetTypeName() const override { return "Transform"; }

    // Transform helpers
    [[nodiscard]] glm::vec3 GetForward() const;
    [[nodiscard]] glm::vec3 GetRight() const;
    [[nodiscard]] glm::vec3 GetUp() const;
    [[nodiscard]] glm::mat4 GetMatrix() const;
};

/**
 * @brief Health component - health, damage, death
 */
class HealthComponent : public ComponentBase<HealthComponent>,
                        public IComponentDependencies {
public:
    float health = 100.0f;
    float maxHealth = 100.0f;
    float armor = 0.0f;
    bool invulnerable = false;

    void OnInitialize() override;
    bool OnEvent(const GameEvent& event) override;
    [[nodiscard]] const char* GetTypeName() const override { return "Health"; }

    // Health operations
    float TakeDamage(float amount, LifecycleHandle source = LifecycleHandle::Invalid);
    void Heal(float amount);
    void SetHealth(float value);
    [[nodiscard]] bool IsAlive() const { return health > 0.0f; }
    [[nodiscard]] float GetHealthPercent() const;

    std::vector<ComponentDependency> GetDependencies() const override {
        // Health requires Transform (for death position)
        return {{ GetComponentTypeId<TransformComponent>(), false, true }};
    }
};

/**
 * @brief Movement component - velocity, speed
 */
class MovementComponent : public ComponentBase<MovementComponent>,
                          public IComponentDependencies {
public:
    glm::vec3 velocity{0.0f};
    float maxSpeed = 10.0f;
    float acceleration = 50.0f;
    float friction = 5.0f;

    void OnInitialize() override;
    void OnTick(float deltaTime) override;
    [[nodiscard]] const char* GetTypeName() const override { return "Movement"; }

    // Movement operations
    void SetTargetVelocity(const glm::vec3& target);
    void ApplyForce(const glm::vec3& force);
    void Stop();
    [[nodiscard]] float GetSpeed() const;

    std::vector<ComponentDependency> GetDependencies() const override {
        return {{ GetComponentTypeId<TransformComponent>(), true, true }};
    }

private:
    TransformComponent* m_transform = nullptr;
    glm::vec3 m_targetVelocity{0.0f};
};

// ============================================================================
// Component Registry
// ============================================================================

/**
 * @brief Registry for component types
 */
class ComponentRegistry {
public:
    using CreatorFunc = std::function<std::unique_ptr<IComponent>()>;

    static ComponentRegistry& Instance();

    template<typename T>
    void Register(const std::string& name);

    void Register(const std::string& name, ComponentTypeId typeId, CreatorFunc creator);

    std::unique_ptr<IComponent> Create(const std::string& name);
    std::unique_ptr<IComponent> Create(ComponentTypeId typeId);

    [[nodiscard]] bool IsRegistered(const std::string& name) const;
    [[nodiscard]] ComponentTypeId GetTypeId(const std::string& name) const;
    [[nodiscard]] std::string GetTypeName(ComponentTypeId typeId) const;
    [[nodiscard]] std::vector<std::string> GetRegisteredNames() const;

private:
    ComponentRegistry() = default;

    struct TypeInfo {
        ComponentTypeId typeId;
        CreatorFunc creator;
    };

    std::unordered_map<std::string, TypeInfo> m_nameToInfo;
    std::unordered_map<ComponentTypeId, std::string> m_idToName;
};

template<typename T>
void ComponentRegistry::Register(const std::string& name) {
    static_assert(std::is_base_of_v<IComponent, T>,
                  "Type must derive from IComponent");

    Register(name, GetComponentTypeId<T>(), []() -> std::unique_ptr<IComponent> {
        return std::make_unique<T>();
    });
}

// ============================================================================
// Registration Macro
// ============================================================================

#define REGISTER_COMPONENT(Name, ClassName) \
    namespace { \
        struct ClassName##_ComponentRegistrar { \
            ClassName##_ComponentRegistrar() { \
                ComponentRegistry::Instance().Register<ClassName>(Name); \
            } \
        }; \
        static ClassName##_ComponentRegistrar g_##ClassName##_componentRegistrar; \
    }

} // namespace Lifecycle
} // namespace Vehement
