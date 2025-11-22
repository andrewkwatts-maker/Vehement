#pragma once

#include "ILifecycle.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <queue>
#include <variant>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Event Types
// ============================================================================

/**
 * @brief All game event types
 *
 * Events are categorized by their purpose:
 * - Lifecycle events: Spawned, Destroyed, etc.
 * - Combat events: Damaged, Healed, Killed, etc.
 * - Building events: Built, Upgraded, etc.
 * - AI events: TargetAcquired, PathBlocked, etc.
 * - Game events: WaveStarted, VictoryAchieved, etc.
 */
enum class EventType : uint32_t {
    // =========================================================================
    // Lifecycle Events (0x0000 - 0x00FF)
    // =========================================================================
    None                = 0x0000,

    // Basic lifecycle
    Spawned             = 0x0001,   // Entity spawned into world
    Destroyed           = 0x0002,   // Entity destroyed
    Activated           = 0x0003,   // Entity activated (from pool)
    Deactivated         = 0x0004,   // Entity deactivated (to pool)
    Paused              = 0x0005,   // Entity paused
    Resumed             = 0x0006,   // Entity resumed

    // State changes
    StateChanged        = 0x0010,   // Generic state change
    EnabledChanged      = 0x0011,   // Enabled/disabled changed
    VisibilityChanged   = 0x0012,   // Visibility changed

    // =========================================================================
    // Combat Events (0x0100 - 0x01FF)
    // =========================================================================
    CombatStart         = 0x0100,

    // Damage and healing
    Damaged             = 0x0101,   // Entity took damage
    Healed              = 0x0102,   // Entity was healed
    Killed              = 0x0103,   // Entity was killed
    Revived             = 0x0104,   // Entity was revived

    // Attacks
    AttackStarted       = 0x0110,   // Attack animation started
    AttackLanded        = 0x0111,   // Attack hit target
    AttackMissed        = 0x0112,   // Attack missed
    AttackBlocked       = 0x0113,   // Attack was blocked/parried

    // Status effects
    StatusApplied       = 0x0120,   // Status effect applied
    StatusRemoved       = 0x0121,   // Status effect removed
    StatusTick          = 0x0122,   // Periodic status effect tick

    // Critical events
    CriticalHit         = 0x0130,   // Critical hit landed
    DodgedAttack        = 0x0131,   // Successfully dodged
    ShieldBroken        = 0x0132,   // Shield depleted

    // =========================================================================
    // Building Events (0x0200 - 0x02FF)
    // =========================================================================
    BuildingStart       = 0x0200,

    // Construction
    Built               = 0x0201,   // Building construction completed
    ConstructionStarted = 0x0202,   // Building construction began
    ConstructionProgress= 0x0203,   // Construction progress updated
    Demolished          = 0x0204,   // Building demolished

    // Upgrades
    Upgraded            = 0x0210,   // Building/unit upgraded
    UpgradeStarted      = 0x0211,   // Upgrade process started
    UpgradeProgress     = 0x0212,   // Upgrade progress updated
    UpgradeCancelled    = 0x0213,   // Upgrade was cancelled

    // Production
    ProductionStarted   = 0x0220,   // Started producing unit/resource
    ProductionComplete  = 0x0221,   // Production finished
    ProductionCancelled = 0x0222,   // Production cancelled
    ProductionQueued    = 0x0223,   // Added to production queue

    // Building state
    GarrisonEntered     = 0x0230,   // Unit entered garrison
    GarrisonExited      = 0x0231,   // Unit left garrison
    GateOpened          = 0x0232,   // Gate opened
    GateClosed          = 0x0233,   // Gate closed

    // =========================================================================
    // Unit/Movement Events (0x0300 - 0x03FF)
    // =========================================================================
    UnitStart           = 0x0300,

    // Movement
    MovementStarted     = 0x0301,   // Unit started moving
    MovementStopped     = 0x0302,   // Unit stopped moving
    DestinationReached  = 0x0303,   // Unit reached destination
    PathBlocked         = 0x0304,   // Movement path is blocked

    // AI/Targeting
    TargetAcquired      = 0x0310,   // New target acquired
    TargetLost          = 0x0311,   // Target lost (died/out of range)
    TargetChanged       = 0x0312,   // Switched to different target

    // Orders
    OrderReceived       = 0x0320,   // New order received
    OrderCompleted      = 0x0321,   // Order finished
    OrderCancelled      = 0x0322,   // Order was cancelled

    // Groups
    GroupJoined         = 0x0330,   // Joined a control group
    GroupLeft           = 0x0331,   // Left a control group
    FormationChanged    = 0x0332,   // Formation updated

    // =========================================================================
    // Projectile Events (0x0400 - 0x04FF)
    // =========================================================================
    ProjectileStart     = 0x0400,

    Launched            = 0x0401,   // Projectile launched
    ProjectileHit       = 0x0402,   // Projectile hit something
    ProjectileExpired   = 0x0403,   // Projectile timed out
    Exploded            = 0x0404,   // Projectile exploded (area effect)
    Bounced             = 0x0405,   // Projectile bounced

    // =========================================================================
    // Effect Events (0x0500 - 0x05FF)
    // =========================================================================
    EffectStart         = 0x0500,

    EffectStarted       = 0x0501,   // Visual effect started
    EffectEnded         = 0x0502,   // Visual effect ended
    EffectLooped        = 0x0503,   // Effect looped

    // Spells/abilities
    AbilityCast         = 0x0510,   // Ability was cast
    AbilityHit          = 0x0511,   // Ability hit target
    AbilityCancelled    = 0x0512,   // Ability cancelled
    CooldownStarted     = 0x0513,   // Cooldown began
    CooldownReady       = 0x0514,   // Ability ready again

    // =========================================================================
    // Resource Events (0x0600 - 0x06FF)
    // =========================================================================
    ResourceStart       = 0x0600,

    ResourceGained      = 0x0601,   // Gained resources
    ResourceSpent       = 0x0602,   // Spent resources
    ResourceDepleted    = 0x0603,   // Resource node depleted
    ResourceDiscovered  = 0x0604,   // New resource found

    // =========================================================================
    // Game Events (0x0700 - 0x07FF)
    // =========================================================================
    GameStart           = 0x0700,

    WaveStarted         = 0x0701,   // Enemy wave started
    WaveCompleted       = 0x0702,   // Enemy wave completed
    BossSpawned         = 0x0703,   // Boss enemy spawned
    ObjectiveUpdated    = 0x0704,   // Objective status changed
    ObjectiveCompleted  = 0x0705,   // Objective completed

    GamePaused          = 0x0710,   // Game paused
    GameResumed         = 0x0711,   // Game resumed
    GameOver            = 0x0712,   // Game ended
    Victory             = 0x0713,   // Player won
    Defeat              = 0x0714,   // Player lost

    // =========================================================================
    // Custom Events (0x1000+)
    // =========================================================================
    CustomEventBase     = 0x1000,   // Base for custom events

    MaxEventType        = 0xFFFF
};

/**
 * @brief Get string name for event type
 */
const char* EventTypeToString(EventType type);

/**
 * @brief Check if event type is in a category
 */
inline bool IsLifecycleEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0000 &&
           static_cast<uint32_t>(type) < 0x0100;
}

inline bool IsCombatEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0100 &&
           static_cast<uint32_t>(type) < 0x0200;
}

inline bool IsBuildingEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0200 &&
           static_cast<uint32_t>(type) < 0x0300;
}

inline bool IsUnitEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0300 &&
           static_cast<uint32_t>(type) < 0x0400;
}

inline bool IsProjectileEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0400 &&
           static_cast<uint32_t>(type) < 0x0500;
}

inline bool IsEffectEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0500 &&
           static_cast<uint32_t>(type) < 0x0600;
}

inline bool IsResourceEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0600 &&
           static_cast<uint32_t>(type) < 0x0700;
}

inline bool IsGameEvent(EventType type) {
    return static_cast<uint32_t>(type) >= 0x0700 &&
           static_cast<uint32_t>(type) < 0x0800;
}

inline bool IsCustomEvent(EventType type) {
    return static_cast<uint32_t>(type) >= static_cast<uint32_t>(EventType::CustomEventBase);
}

// ============================================================================
// Event Propagation
// ============================================================================

/**
 * @brief Event propagation direction
 */
enum class EventPropagation : uint8_t {
    None        = 0,        // No propagation
    BubbleUp    = 1 << 0,   // Propagate to parent
    CaptureDown = 1 << 1,   // Propagate to children
    Broadcast   = 1 << 2,   // Send to all subscribers
    Immediate   = 1 << 3,   // Process immediately (not queued)

    Default = BubbleUp
};

inline EventPropagation operator|(EventPropagation a, EventPropagation b) {
    return static_cast<EventPropagation>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline EventPropagation operator&(EventPropagation a, EventPropagation b) {
    return static_cast<EventPropagation>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool HasPropagation(EventPropagation flags, EventPropagation flag) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

// ============================================================================
// Event Data Types
// ============================================================================

/**
 * @brief Common event data for damage/healing events
 */
struct DamageEventData {
    float amount = 0.0f;
    float actualDamage = 0.0f;     // After armor/resistance
    float overkill = 0.0f;         // Damage beyond death
    LifecycleHandle sourceHandle;
    LifecycleHandle targetHandle;
    std::string damageType;        // "physical", "fire", "poison", etc.
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
    bool isCritical = false;
    bool isDot = false;            // Damage over time
};

/**
 * @brief Common event data for position/movement events
 */
struct PositionEventData {
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    float rotation = 0.0f;
};

/**
 * @brief Common event data for building events
 */
struct BuildingEventData {
    float progress = 0.0f;         // 0-100%
    int level = 1;
    int previousLevel = 0;
    std::string buildingType;
    LifecycleHandle builderHandle;
};

/**
 * @brief Common event data for resource events
 */
struct ResourceEventData {
    std::string resourceType;
    float amount = 0.0f;
    float previousAmount = 0.0f;
    glm::vec3 position{0.0f};
};

/**
 * @brief Event data variant for type-safe access
 */
using EventDataVariant = std::variant<
    std::monostate,            // No data
    DamageEventData,
    PositionEventData,
    BuildingEventData,
    ResourceEventData,
    std::string,               // Simple string data
    float,                     // Simple numeric data
    glm::vec3                  // Simple position data
>;

// ============================================================================
// GameEvent Structure
// ============================================================================

/**
 * @brief Core game event structure
 *
 * Events are lightweight and can be pooled. Complex data is stored
 * in the optional data variant or as JSON for scripting.
 */
struct GameEvent {
    // Event identification
    EventType type = EventType::None;
    uint32_t customTypeId = 0;     // For custom events

    // Source and target
    LifecycleHandle source;        // Who triggered the event
    LifecycleHandle target;        // Primary target (if any)

    // Propagation control
    EventPropagation propagation = EventPropagation::Default;
    bool handled = false;          // Set true to stop propagation
    bool cancelled = false;        // Set true to cancel the event

    // Timing
    double timestamp = 0.0;        // When event was created
    float delay = 0.0f;           // Delay before processing

    // Priority (higher = processed first)
    int32_t priority = 0;

    // Event data (type-safe)
    EventDataVariant data;

    // JSON data for scripting flexibility
    std::shared_ptr<std::string> jsonData;

    // Constructors
    GameEvent() = default;

    explicit GameEvent(EventType eventType)
        : type(eventType) {}

    GameEvent(EventType eventType, LifecycleHandle sourceHandle)
        : type(eventType), source(sourceHandle) {}

    GameEvent(EventType eventType, LifecycleHandle sourceHandle, LifecycleHandle targetHandle)
        : type(eventType), source(sourceHandle), target(targetHandle) {}

    // =========================================================================
    // Data Access Helpers
    // =========================================================================

    /**
     * @brief Get typed data if available
     */
    template<typename T>
    const T* GetData() const {
        return std::get_if<T>(&data);
    }

    template<typename T>
    T* GetMutableData() {
        return std::get_if<T>(&data);
    }

    /**
     * @brief Set typed data
     */
    template<typename T>
    void SetData(T&& value) {
        data = std::forward<T>(value);
    }

    /**
     * @brief Check if event has data of type
     */
    template<typename T>
    bool HasData() const {
        return std::holds_alternative<T>(data);
    }

    /**
     * @brief Set JSON data string
     */
    void SetJsonData(const std::string& json) {
        jsonData = std::make_shared<std::string>(json);
    }

    /**
     * @brief Get JSON data string
     */
    const std::string& GetJsonData() const {
        static const std::string empty;
        return jsonData ? *jsonData : empty;
    }

    /**
     * @brief Mark event as handled
     */
    void Handle() { handled = true; }

    /**
     * @brief Cancel the event
     */
    void Cancel() { cancelled = true; handled = true; }

    /**
     * @brief Check if event should continue propagating
     */
    [[nodiscard]] bool ShouldPropagate() const {
        return !handled && !cancelled;
    }
};

// ============================================================================
// Event Subscription
// ============================================================================

/**
 * @brief Subscription handle for unsubscribing
 */
struct EventSubscription {
    uint64_t id = 0;
    EventType type = EventType::None;

    [[nodiscard]] bool IsValid() const { return id != 0; }

    bool operator==(const EventSubscription& other) const {
        return id == other.id;
    }
};

/**
 * @brief Event callback function type
 */
using EventCallback = std::function<bool(const GameEvent&)>;

/**
 * @brief Event filter predicate
 */
using EventFilter = std::function<bool(const GameEvent&)>;

// ============================================================================
// Event Dispatcher
// ============================================================================

/**
 * @brief Central event dispatch and subscription system
 *
 * Features:
 * - Type-based subscription
 * - Priority ordering
 * - Event filtering
 * - Deferred event queue
 * - Event pooling
 */
class EventDispatcher {
public:
    EventDispatcher();
    ~EventDispatcher();

    // Disable copy
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // =========================================================================
    // Subscription
    // =========================================================================

    /**
     * @brief Subscribe to an event type
     *
     * @param type Event type to subscribe to
     * @param callback Function to call when event fires
     * @param priority Higher priority = called first
     * @return Subscription handle for unsubscribing
     */
    EventSubscription Subscribe(EventType type, EventCallback callback, int priority = 0);

    /**
     * @brief Subscribe to an event type with filter
     *
     * @param type Event type to subscribe to
     * @param callback Function to call when event fires
     * @param filter Only call if filter returns true
     * @param priority Higher priority = called first
     * @return Subscription handle for unsubscribing
     */
    EventSubscription Subscribe(EventType type, EventCallback callback,
                               EventFilter filter, int priority = 0);

    /**
     * @brief Subscribe to multiple event types
     */
    std::vector<EventSubscription> Subscribe(const std::vector<EventType>& types,
                                             EventCallback callback, int priority = 0);

    /**
     * @brief Subscribe to all events in a category
     */
    EventSubscription SubscribeToCategory(EventCallback callback,
                                         bool(*categoryCheck)(EventType),
                                         int priority = 0);

    /**
     * @brief Unsubscribe from events
     */
    void Unsubscribe(const EventSubscription& subscription);

    /**
     * @brief Unsubscribe all handlers for an event type
     */
    void UnsubscribeAll(EventType type);

    /**
     * @brief Clear all subscriptions
     */
    void ClearAllSubscriptions();

    // =========================================================================
    // Event Dispatch
    // =========================================================================

    /**
     * @brief Dispatch event immediately
     *
     * @param event The event to dispatch
     * @return true if event was handled
     */
    bool Dispatch(GameEvent& event);

    /**
     * @brief Queue event for later processing
     *
     * @param event The event to queue
     */
    void QueueEvent(GameEvent event);

    /**
     * @brief Queue event with delay
     *
     * @param event The event to queue
     * @param delay Delay in seconds
     */
    void QueueDelayedEvent(GameEvent event, float delay);

    /**
     * @brief Process all queued events
     *
     * @param currentTime Current game time
     */
    void ProcessQueuedEvents(double currentTime);

    /**
     * @brief Clear event queue
     */
    void ClearEventQueue();

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t GetSubscriberCount(EventType type) const;
    [[nodiscard]] size_t GetQueuedEventCount() const;
    [[nodiscard]] size_t GetTotalSubscriptionCount() const;

private:
    struct SubscriberInfo {
        uint64_t id;
        EventCallback callback;
        EventFilter filter;
        int priority;
    };

    struct CategorySubscriber {
        uint64_t id;
        EventCallback callback;
        bool(*categoryCheck)(EventType);
        int priority;
    };

    // Subscribers per event type
    std::unordered_map<EventType, std::vector<SubscriberInfo>> m_subscribers;

    // Category subscribers (checked for all events)
    std::vector<CategorySubscriber> m_categorySubscribers;

    // Event queue (sorted by timestamp + delay)
    struct QueuedEvent {
        GameEvent event;
        double processTime;

        bool operator>(const QueuedEvent& other) const {
            return processTime > other.processTime;
        }
    };
    std::priority_queue<QueuedEvent, std::vector<QueuedEvent>, std::greater<>> m_eventQueue;

    // ID generation
    uint64_t m_nextSubscriptionId = 1;

    // Helper to sort subscribers by priority
    void SortSubscribers(EventType type);
};

// ============================================================================
// Global Event Functions
// ============================================================================

/**
 * @brief Get global event dispatcher instance
 */
EventDispatcher& GetGlobalEventDispatcher();

/**
 * @brief Convenience function to dispatch event globally
 */
inline bool DispatchEvent(GameEvent& event) {
    return GetGlobalEventDispatcher().Dispatch(event);
}

/**
 * @brief Convenience function to queue event globally
 */
inline void QueueEvent(GameEvent event) {
    GetGlobalEventDispatcher().QueueEvent(std::move(event));
}

// ============================================================================
// Event Builder (Fluent API)
// ============================================================================

/**
 * @brief Fluent builder for creating events
 */
class EventBuilder {
public:
    explicit EventBuilder(EventType type) : m_event(type) {}

    EventBuilder& Source(LifecycleHandle handle) {
        m_event.source = handle;
        return *this;
    }

    EventBuilder& Target(LifecycleHandle handle) {
        m_event.target = handle;
        return *this;
    }

    EventBuilder& Priority(int32_t priority) {
        m_event.priority = priority;
        return *this;
    }

    EventBuilder& Propagation(EventPropagation prop) {
        m_event.propagation = prop;
        return *this;
    }

    EventBuilder& Delay(float seconds) {
        m_event.delay = seconds;
        return *this;
    }

    template<typename T>
    EventBuilder& Data(T&& data) {
        m_event.SetData(std::forward<T>(data));
        return *this;
    }

    EventBuilder& Json(const std::string& json) {
        m_event.SetJsonData(json);
        return *this;
    }

    GameEvent Build() { return std::move(m_event); }

    bool Dispatch() {
        return GetGlobalEventDispatcher().Dispatch(m_event);
    }

    void Queue() {
        GetGlobalEventDispatcher().QueueEvent(std::move(m_event));
    }

private:
    GameEvent m_event;
};

/**
 * @brief Create event builder
 */
inline EventBuilder MakeEvent(EventType type) {
    return EventBuilder(type);
}

} // namespace Lifecycle
} // namespace Vehement
