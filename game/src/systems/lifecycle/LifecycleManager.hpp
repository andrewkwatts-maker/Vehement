/**
 * @file LifecycleManager.hpp
 * @brief Central management system for game object lifecycles
 *
 * The LifecycleManager provides comprehensive object lifecycle management
 * including creation, destruction, pooling, event routing, and tick scheduling.
 * It serves as the backbone for all game entities and systems.
 *
 * @section lifecycle_concepts Key Concepts
 *
 * **Handles**: Objects are referenced via LifecycleHandle, a lightweight
 * identifier that includes a generation counter for safe access to
 * potentially destroyed objects.
 *
 * **Deferred Destruction**: By default, objects are destroyed at the end
 * of the frame to prevent invalidating references during iteration.
 *
 * **Object Pools**: For frequently created/destroyed objects, enable
 * pooling to reuse allocations and reduce garbage collection pressure.
 *
 * **Tick Scheduling**: Objects can register for per-frame updates with
 * configurable tick rates and priorities.
 *
 * @section lifecycle_usage Basic Usage
 *
 * @code{.cpp}
 * #include <game/src/systems/lifecycle/LifecycleManager.hpp>
 *
 * // Get the global manager
 * auto& lifecycle = Vehement::Lifecycle::GetGlobalLifecycleManager();
 *
 * // Register types
 * lifecycle.RegisterType<Zombie>("Zombie");
 * lifecycle.RegisterType<Projectile>("Projectile");
 *
 * // Enable pooling for frequently used types
 * lifecycle.EnablePooling<Projectile>(100);  // Pool of 100
 *
 * // Create objects
 * auto zombieHandle = lifecycle.Create<Zombie>({
 *     {"health", 100},
 *     {"speed", 2.5f}
 * });
 *
 * // Or create from config file
 * auto bossHandle = lifecycle.CreateFromFile("config/entities/boss.json");
 *
 * // Access objects
 * if (auto* zombie = lifecycle.GetAs<Zombie>(zombieHandle)) {
 *     zombie->TakeDamage(50);
 * }
 *
 * // Destroy (deferred by default)
 * lifecycle.Destroy(zombieHandle);
 *
 * // Update each frame
 * void GameLoop(float dt) {
 *     lifecycle.Update(dt);
 *     lifecycle.ProcessDeferredActions();
 * }
 * @endcode
 *
 * @section lifecycle_events Event System
 *
 * Objects receive events through the lifecycle event system:
 *
 * @code{.cpp}
 * // Send event to specific object
 * DamageEvent damage{50, DamageType::Fire, attackerId};
 * lifecycle.SendEvent(targetHandle, damage);
 *
 * // Broadcast to all objects
 * WaveStartEvent waveEvent{5};
 * lifecycle.BroadcastEvent(waveEvent);
 *
 * // Queue for later processing
 * lifecycle.QueueEvent(ExplosionEvent{position, radius});
 * @endcode
 *
 * @section lifecycle_hierarchy Parent-Child Relationships
 *
 * Objects can form hierarchies where destroying a parent also
 * destroys all children:
 *
 * @code{.cpp}
 * auto parentHandle = lifecycle.Create<Squad>();
 * auto childHandle = lifecycle.Create<Soldier>();
 *
 * lifecycle.SetParent(childHandle, parentHandle);
 *
 * // Get children
 * auto soldiers = lifecycle.GetChildren(parentHandle);
 *
 * // Destroying parent destroys all children
 * lifecycle.Destroy(parentHandle);
 * @endcode
 *
 * @section lifecycle_pooling Object Pooling
 *
 * For high-frequency object creation (bullets, particles, etc.):
 *
 * @code{.cpp}
 * // Enable pooling during initialization
 * lifecycle.EnablePooling<Bullet>(200);
 * lifecycle.PreWarmPools();  // Pre-allocate objects
 *
 * // Objects from pools are recycled automatically
 * // OnActivate() is called when acquired from pool
 * // OnDeactivate() is called when returned to pool
 * @endcode
 *
 * @see ILifecycle for the object interface
 * @see TickScheduler for update scheduling
 * @see GameEvent for the event system
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include "ILifecycle.hpp"
#include "GameEvent.hpp"
#include "TickScheduler.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <string>
#include <deque>
#include <typeindex>

// Forward declare nlohmann json
namespace nlohmann {
    template<typename, typename, typename...> class basic_json;
    using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, void>;
}

namespace Vehement {
namespace Lifecycle {

// Forward declarations
class ObjectFactory;
class ScriptedLifecycle;

// ============================================================================
// Object Pool
// ============================================================================

/**
 * @brief Pool for efficient object allocation/deallocation
 *
 * Reuses destroyed objects to avoid allocation overhead.
 * Objects are reset via OnDeactivate/OnActivate callbacks.
 */
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialCapacity = 64);
    ~ObjectPool();

    // Disable copy
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief Acquire object from pool
     * @return Pointer to object (may be recycled)
     */
    T* Acquire();

    /**
     * @brief Release object back to pool
     */
    void Release(T* object);

    /**
     * @brief Pre-allocate objects
     */
    void Reserve(size_t count);

    /**
     * @brief Clear all pooled objects
     */
    void Clear();

    /**
     * @brief Get number of objects in pool
     */
    [[nodiscard]] size_t GetPooledCount() const { return m_pool.size(); }

    /**
     * @brief Get total allocated objects
     */
    [[nodiscard]] size_t GetTotalAllocated() const { return m_totalAllocated; }

private:
    std::vector<std::unique_ptr<T>> m_pool;
    size_t m_totalAllocated = 0;
};

// ============================================================================
// Deferred Action
// ============================================================================

/**
 * @brief Action to be executed at end of frame
 */
struct DeferredAction {
    enum class Type {
        Destroy,
        Create,
        Event,
        Custom
    };

    Type type;
    LifecycleHandle handle;
    std::function<void()> action;
};

// ============================================================================
// LifecycleManager
// ============================================================================

/**
 * @brief Central manager for all lifecycle objects
 *
 * Responsibilities:
 * - Object creation and destruction
 * - Object pools for efficient allocation
 * - Tick scheduling
 * - Event routing
 * - Deferred destruction
 * - Parent/child relationships
 * - Type registration
 *
 * Usage:
 * 1. Register object types with RegisterType<T>()
 * 2. Create objects with Create<T>() or CreateFromConfig()
 * 3. Call Update() each frame
 * 4. Objects are automatically ticked and receive events
 * 5. Destroy with Destroy() (deferred by default)
 */
class LifecycleManager {
public:
    LifecycleManager();
    ~LifecycleManager();

    // Disable copy
    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;

    // =========================================================================
    // Type Registration
    // =========================================================================

    /**
     * @brief Register a type for creation
     *
     * @tparam T Type to register (must derive from ILifecycle)
     * @param typeName String name for config-based creation
     */
    template<typename T>
    void RegisterType(const std::string& typeName);

    /**
     * @brief Check if type is registered
     */
    [[nodiscard]] bool IsTypeRegistered(const std::string& typeName) const;

    /**
     * @brief Get all registered type names
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;

    // =========================================================================
    // Object Creation
    // =========================================================================

    /**
     * @brief Create object by type
     *
     * @tparam T Type to create
     * @param config JSON configuration
     * @return Handle to created object
     */
    template<typename T>
    LifecycleHandle Create(const nlohmann::json& config = {});

    /**
     * @brief Create object from config (by type name)
     *
     * @param typeName Registered type name
     * @param config JSON configuration
     * @return Handle to created object
     */
    LifecycleHandle CreateFromConfig(const std::string& typeName,
                                     const nlohmann::json& config);

    /**
     * @brief Create object from config file
     *
     * @param configPath Path to JSON config file
     * @return Handle to created object
     */
    LifecycleHandle CreateFromFile(const std::string& configPath);

    /**
     * @brief Clone an existing object
     *
     * @param source Handle to object to clone
     * @return Handle to new cloned object
     */
    LifecycleHandle Clone(LifecycleHandle source);

    // =========================================================================
    // Object Destruction
    // =========================================================================

    /**
     * @brief Destroy object
     *
     * @param handle Object to destroy
     * @param immediate If true, destroy now. If false, defer to end of frame.
     */
    void Destroy(LifecycleHandle handle, bool immediate = false);

    /**
     * @brief Destroy all objects of a type
     *
     * @param typeName Type name to destroy
     * @param immediate If true, destroy now
     */
    void DestroyAllOfType(const std::string& typeName, bool immediate = false);

    /**
     * @brief Destroy all objects
     */
    void DestroyAll();

    /**
     * @brief Check if object is alive
     */
    [[nodiscard]] bool IsAlive(LifecycleHandle handle) const;

    // =========================================================================
    // Object Access
    // =========================================================================

    /**
     * @brief Get object by handle
     */
    [[nodiscard]] ILifecycle* Get(LifecycleHandle handle);
    [[nodiscard]] const ILifecycle* Get(LifecycleHandle handle) const;

    /**
     * @brief Get object as specific type
     */
    template<typename T>
    [[nodiscard]] T* GetAs(LifecycleHandle handle);

    template<typename T>
    [[nodiscard]] const T* GetAs(LifecycleHandle handle) const;

    /**
     * @brief Get all objects of a type
     */
    template<typename T>
    [[nodiscard]] std::vector<T*> GetAllOfType();

    /**
     * @brief Get all objects matching predicate
     */
    [[nodiscard]] std::vector<ILifecycle*> GetAll(
        std::function<bool(const ILifecycle*)> predicate = nullptr);

    /**
     * @brief Get total object count
     */
    [[nodiscard]] size_t GetObjectCount() const;

    /**
     * @brief Get active object count
     */
    [[nodiscard]] size_t GetActiveObjectCount() const;

    // =========================================================================
    // Parent/Child Relationships
    // =========================================================================

    /**
     * @brief Set parent of an object
     */
    void SetParent(LifecycleHandle child, LifecycleHandle parent);

    /**
     * @brief Get parent of an object
     */
    [[nodiscard]] LifecycleHandle GetParent(LifecycleHandle handle) const;

    /**
     * @brief Get children of an object
     */
    [[nodiscard]] std::vector<LifecycleHandle> GetChildren(LifecycleHandle handle) const;

    /**
     * @brief Detach from parent
     */
    void DetachFromParent(LifecycleHandle handle);

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Main update - call once per frame
     *
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Process deferred actions (destruction, creation)
     */
    void ProcessDeferredActions();

    // =========================================================================
    // Events
    // =========================================================================

    /**
     * @brief Send event to specific object
     */
    bool SendEvent(LifecycleHandle target, GameEvent& event);

    /**
     * @brief Broadcast event to all objects
     */
    void BroadcastEvent(GameEvent& event);

    /**
     * @brief Queue event for processing
     */
    void QueueEvent(GameEvent event);

    /**
     * @brief Get event dispatcher
     */
    [[nodiscard]] EventDispatcher& GetEventDispatcher() { return m_eventDispatcher; }

    // =========================================================================
    // Tick Scheduler
    // =========================================================================

    /**
     * @brief Get tick scheduler
     */
    [[nodiscard]] TickScheduler& GetTickScheduler() { return m_tickScheduler; }

    // =========================================================================
    // Object Pools
    // =========================================================================

    /**
     * @brief Enable pooling for a type
     */
    template<typename T>
    void EnablePooling(size_t initialSize = 64);

    /**
     * @brief Pre-warm pools
     */
    void PreWarmPools();

    /**
     * @brief Clear all pools
     */
    void ClearPools();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set default tick config for new objects
     */
    void SetDefaultTickConfig(const TickConfig& config);

    /**
     * @brief Enable/disable deferred destruction by default
     */
    void SetDeferredDestructionDefault(bool deferred) {
        m_defaultDeferredDestruction = deferred;
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Get statistics
     */
    struct Stats {
        size_t totalObjects = 0;
        size_t activeObjects = 0;
        size_t pooledObjects = 0;
        size_t pendingDestructions = 0;
        size_t eventsProcessed = 0;
        double lastUpdateDuration = 0.0;
    };
    [[nodiscard]] Stats GetStats() const;

private:
    // Object storage
    struct ObjectEntry {
        std::unique_ptr<ILifecycle> object;
        std::string typeName;
        uint32_t generation = 0;
        TickHandle tickHandle;
        LifecycleHandle parentHandle;
        std::vector<LifecycleHandle> children;
        bool pendingDestruction = false;
    };

    std::vector<ObjectEntry> m_objects;
    std::vector<uint32_t> m_freeSlots;
    uint32_t m_nextGeneration = 1;

    // Type registry
    using CreateFunc = std::function<std::unique_ptr<ILifecycle>()>;
    std::unordered_map<std::string, CreateFunc> m_typeRegistry;
    std::unordered_map<std::type_index, std::string> m_typeToName;

    // Object pools (type-erased)
    struct PoolBase {
        virtual ~PoolBase() = default;
        virtual void* Acquire() = 0;
        virtual void Release(void* ptr) = 0;
        virtual void Clear() = 0;
        virtual size_t GetPooledCount() const = 0;
    };

    template<typename T>
    struct TypedPool : public PoolBase {
        ObjectPool<T> pool;

        explicit TypedPool(size_t initialSize) : pool(initialSize) {}
        void* Acquire() override { return pool.Acquire(); }
        void Release(void* ptr) override { pool.Release(static_cast<T*>(ptr)); }
        void Clear() override { pool.Clear(); }
        size_t GetPooledCount() const override { return pool.GetPooledCount(); }
    };

    std::unordered_map<std::type_index, std::unique_ptr<PoolBase>> m_pools;

    // Deferred actions
    std::deque<DeferredAction> m_deferredActions;

    // Subsystems
    EventDispatcher m_eventDispatcher;
    TickScheduler m_tickScheduler;

    // Configuration
    TickConfig m_defaultTickConfig;
    bool m_defaultDeferredDestruction = true;

    // Stats
    mutable Stats m_stats;

    // Helpers
    ObjectEntry* GetEntry(LifecycleHandle handle);
    const ObjectEntry* GetEntry(LifecycleHandle handle) const;
    LifecycleHandle AllocateHandle();
    void DestroyImmediate(LifecycleHandle handle);
    void PropagateEventToChildren(LifecycleHandle parent, GameEvent& event);
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
ObjectPool<T>::ObjectPool(size_t initialCapacity) {
    m_pool.reserve(initialCapacity);
}

template<typename T>
ObjectPool<T>::~ObjectPool() = default;

template<typename T>
T* ObjectPool<T>::Acquire() {
    if (!m_pool.empty()) {
        auto obj = std::move(m_pool.back());
        m_pool.pop_back();
        return obj.release();
    }

    m_totalAllocated++;
    return new T();
}

template<typename T>
void ObjectPool<T>::Release(T* object) {
    if (object) {
        m_pool.push_back(std::unique_ptr<T>(object));
    }
}

template<typename T>
void ObjectPool<T>::Reserve(size_t count) {
    while (m_pool.size() < count) {
        m_pool.push_back(std::make_unique<T>());
        m_totalAllocated++;
    }
}

template<typename T>
void ObjectPool<T>::Clear() {
    m_pool.clear();
}

template<typename T>
void LifecycleManager::RegisterType(const std::string& typeName) {
    static_assert(std::is_base_of_v<ILifecycle, T>,
                  "Type must derive from ILifecycle");

    m_typeRegistry[typeName] = []() -> std::unique_ptr<ILifecycle> {
        return std::make_unique<T>();
    };
    m_typeToName[std::type_index(typeid(T))] = typeName;
}

template<typename T>
LifecycleHandle LifecycleManager::Create(const nlohmann::json& config) {
    static_assert(std::is_base_of_v<ILifecycle, T>,
                  "Type must derive from ILifecycle");

    // Check for pool
    auto poolIt = m_pools.find(std::type_index(typeid(T)));

    std::unique_ptr<ILifecycle> object;
    if (poolIt != m_pools.end()) {
        object.reset(static_cast<ILifecycle*>(poolIt->second->Acquire()));
        object->OnActivate();
    } else {
        object = std::make_unique<T>();
    }

    // Allocate handle
    LifecycleHandle handle = AllocateHandle();

    // Setup entry
    ObjectEntry& entry = m_objects[handle.index];
    entry.object = std::move(object);
    entry.generation = handle.generation;

    auto nameIt = m_typeToName.find(std::type_index(typeid(T)));
    entry.typeName = nameIt != m_typeToName.end() ? nameIt->second : typeid(T).name();

    // Set handle on object
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetHandle(handle);
        base->SetLifecycleState(LifecycleState::Creating);
    }

    // Register for ticking
    entry.tickHandle = m_tickScheduler.Register(entry.object.get(), m_defaultTickConfig);

    // Call OnCreate
    entry.object->OnCreate(config);

    // Mark as active
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetLifecycleState(LifecycleState::Active);
    }

    // Fire spawned event
    GameEvent spawnEvent(EventType::Spawned, handle);
    m_eventDispatcher.Dispatch(spawnEvent);

    return handle;
}

template<typename T>
T* LifecycleManager::GetAs(LifecycleHandle handle) {
    ILifecycle* obj = Get(handle);
    return obj ? dynamic_cast<T*>(obj) : nullptr;
}

template<typename T>
const T* LifecycleManager::GetAs(LifecycleHandle handle) const {
    const ILifecycle* obj = Get(handle);
    return obj ? dynamic_cast<const T*>(obj) : nullptr;
}

template<typename T>
std::vector<T*> LifecycleManager::GetAllOfType() {
    std::vector<T*> result;
    for (auto& entry : m_objects) {
        if (entry.object && !entry.pendingDestruction) {
            if (T* typed = dynamic_cast<T*>(entry.object.get())) {
                result.push_back(typed);
            }
        }
    }
    return result;
}

template<typename T>
void LifecycleManager::EnablePooling(size_t initialSize) {
    static_assert(std::is_base_of_v<ILifecycle, T>,
                  "Type must derive from ILifecycle");

    auto pool = std::make_unique<TypedPool<T>>(initialSize);
    m_pools[std::type_index(typeid(T))] = std::move(pool);
}

// ============================================================================
// Global Manager Access
// ============================================================================

/**
 * @brief Get global lifecycle manager instance
 */
LifecycleManager& GetGlobalLifecycleManager();

} // namespace Lifecycle
} // namespace Vehement
