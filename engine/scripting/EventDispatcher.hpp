#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <any>
#include <chrono>
#include <optional>

namespace Nova {
namespace Scripting {

// Forward declarations
class PythonEngine;

/**
 * @brief Event types that can trigger Python handlers
 */
enum class EventType {
    // Entity events
    EntitySpawn,
    EntityDeath,
    EntityDamaged,
    EntityHealed,
    EntityMoved,

    // Combat events
    AttackStarted,
    AttackEnded,
    ProjectileFired,
    ProjectileHit,

    // World events
    TileEnter,
    TileExit,
    ZoneEnter,
    ZoneExit,

    // Building events
    BuildingPlaced,
    BuildingComplete,
    BuildingDestroyed,
    BuildingUpgraded,
    WorkerAssigned,
    WorkerUnassigned,

    // Resource events
    ResourceGathered,
    ResourceDepleted,
    ResourceStockChanged,
    TradeCompleted,

    // Time events
    DayStarted,
    NightStarted,
    HourPassed,
    MinutePassed,

    // Game events
    GameStarted,
    GamePaused,
    GameResumed,
    GameSaved,
    GameLoaded,
    LevelLoaded,

    // Player events
    PlayerSpawn,
    PlayerDeath,
    PlayerLevelUp,
    ItemPickup,
    ItemUsed,
    QuestStarted,
    QuestCompleted,

    // AI events
    AIStateChanged,
    AITargetChanged,
    AIPathCompleted,

    // Custom events (user-defined)
    Custom,

    COUNT
};

/**
 * @brief Convert event type to string
 */
[[nodiscard]] const char* EventTypeToString(EventType type);

/**
 * @brief Event data container
 */
struct GameEvent {
    EventType type = EventType::Custom;
    std::string customType;  // For EventType::Custom

    // Common event data
    uint32_t entityId = 0;
    uint32_t targetEntityId = 0;
    uint32_t buildingId = 0;

    // Position data
    float x = 0.0f, y = 0.0f, z = 0.0f;
    int tileX = 0, tileY = 0;

    // Value data
    float floatValue = 0.0f;
    int intValue = 0;
    std::string stringValue;

    // Additional custom data
    std::unordered_map<std::string, std::any> customData;

    // Timing
    std::chrono::system_clock::time_point timestamp;
    float delay = 0.0f;  // Delayed execution (seconds)

    // Cancellation
    bool cancelled = false;

    // Constructor helpers
    static GameEvent EntityEvent(EventType type, uint32_t entityId);
    static GameEvent BuildingEvent(EventType type, uint32_t buildingId);
    static GameEvent TileEvent(EventType type, int tileX, int tileY, uint32_t entityId = 0);
    static GameEvent ResourceEvent(EventType type, const std::string& resourceType, int amount);
    static GameEvent CustomEvent(const std::string& customType);

    // Custom data accessors
    template<typename T>
    void SetData(const std::string& key, const T& value) {
        customData[key] = value;
    }

    template<typename T>
    std::optional<T> GetData(const std::string& key) const {
        auto it = customData.find(key);
        if (it != customData.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief Event handler priority levels
 */
enum class HandlerPriority {
    Lowest = 0,
    Low = 25,
    Normal = 50,
    High = 75,
    Highest = 100,
    Monitor = 200  // Cannot cancel, just observes
};

/**
 * @brief Event handler registration info
 */
struct EventHandler {
    std::string name;
    EventType eventType;
    std::string customEventType;  // For EventType::Custom
    HandlerPriority priority = HandlerPriority::Normal;

    // Handler function - can be C++ or Python
    bool isPython = false;
    std::string pythonModule;
    std::string pythonFunction;
    std::function<void(GameEvent&)> cppHandler;

    // Filter options
    std::optional<uint32_t> filterEntityId;
    std::optional<uint32_t> filterBuildingId;
    std::optional<std::string> filterEntityType;

    // State
    bool enabled = true;
    size_t callCount = 0;
};

/**
 * @brief Event dispatcher that triggers Python and C++ handlers
 *
 * Features:
 * - Register event handlers (C++ or Python)
 * - Event types: EntitySpawn, EntityDeath, TileEnter, BuildingComplete, etc.
 * - Priority ordering of handlers
 * - Event cancellation support
 * - Async event queue for deferred execution
 *
 * Usage:
 * @code
 * EventDispatcher dispatcher;
 *
 * // Register C++ handler
 * dispatcher.RegisterHandler("on_death_log", EventType::EntityDeath,
 *     [](GameEvent& evt) {
 *         std::cout << "Entity died: " << evt.entityId << std::endl;
 *     });
 *
 * // Register Python handler
 * dispatcher.RegisterPythonHandler("zombie_death_bonus", EventType::EntityDeath,
 *     "events.on_death", "handle_zombie_death", HandlerPriority::Normal);
 *
 * // Dispatch event
 * GameEvent evt = GameEvent::EntityEvent(EventType::EntityDeath, zombieId);
 * dispatcher.Dispatch(evt);
 * @endcode
 */
class EventDispatcher {
public:
    using CppHandler = std::function<void(GameEvent&)>;

    EventDispatcher();
    ~EventDispatcher();

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // =========================================================================
    // Handler Registration
    // =========================================================================

    /**
     * @brief Register a C++ event handler
     * @param name Unique handler name
     * @param eventType Event type to handle
     * @param handler Handler function
     * @param priority Handler priority
     * @return Handler ID for later removal
     */
    size_t RegisterHandler(const std::string& name,
                           EventType eventType,
                           CppHandler handler,
                           HandlerPriority priority = HandlerPriority::Normal);

    /**
     * @brief Register a Python event handler
     * @param name Unique handler name
     * @param eventType Event type to handle
     * @param pythonModule Python module containing the handler
     * @param pythonFunction Python function name
     * @param priority Handler priority
     * @return Handler ID
     */
    size_t RegisterPythonHandler(const std::string& name,
                                 EventType eventType,
                                 const std::string& pythonModule,
                                 const std::string& pythonFunction,
                                 HandlerPriority priority = HandlerPriority::Normal);

    /**
     * @brief Register a handler for custom event type
     */
    size_t RegisterCustomHandler(const std::string& name,
                                 const std::string& customEventType,
                                 CppHandler handler,
                                 HandlerPriority priority = HandlerPriority::Normal);

    /**
     * @brief Register Python handler for custom event type
     */
    size_t RegisterCustomPythonHandler(const std::string& name,
                                       const std::string& customEventType,
                                       const std::string& pythonModule,
                                       const std::string& pythonFunction,
                                       HandlerPriority priority = HandlerPriority::Normal);

    /**
     * @brief Unregister a handler by ID
     */
    bool UnregisterHandler(size_t handlerId);

    /**
     * @brief Unregister a handler by name
     */
    bool UnregisterHandler(const std::string& name);

    /**
     * @brief Unregister all handlers for an event type
     */
    void UnregisterAllHandlers(EventType eventType);

    /**
     * @brief Enable/disable a handler
     */
    void SetHandlerEnabled(const std::string& name, bool enabled);

    /**
     * @brief Check if handler exists
     */
    [[nodiscard]] bool HasHandler(const std::string& name) const;

    /**
     * @brief Get list of registered handler names
     */
    [[nodiscard]] std::vector<std::string> GetHandlerNames() const;

    /**
     * @brief Get handlers for a specific event type
     */
    [[nodiscard]] std::vector<std::string> GetHandlersForEvent(EventType eventType) const;

    // =========================================================================
    // Event Dispatch
    // =========================================================================

    /**
     * @brief Dispatch an event immediately
     * @param event Event to dispatch
     * @return true if event was not cancelled
     */
    bool Dispatch(GameEvent& event);

    /**
     * @brief Queue an event for deferred dispatch
     * @param event Event to queue
     */
    void QueueEvent(const GameEvent& event);

    /**
     * @brief Queue an event with delay
     * @param event Event to queue
     * @param delay Delay in seconds
     */
    void QueueDelayedEvent(GameEvent event, float delay);

    /**
     * @brief Process queued events
     * @param deltaTime Time since last update
     */
    void ProcessQueue(float deltaTime);

    /**
     * @brief Clear all queued events
     */
    void ClearQueue();

    /**
     * @brief Get number of queued events
     */
    [[nodiscard]] size_t GetQueueSize() const;

    // =========================================================================
    // Convenience Dispatch Methods
    // =========================================================================

    void DispatchEntitySpawn(uint32_t entityId, const std::string& entityType,
                             float x, float y, float z);
    void DispatchEntityDeath(uint32_t entityId, uint32_t killerId = 0);
    void DispatchEntityDamaged(uint32_t entityId, float damage, uint32_t sourceId = 0);
    void DispatchTileEnter(int tileX, int tileY, uint32_t entityId);
    void DispatchTileExit(int tileX, int tileY, uint32_t entityId);
    void DispatchBuildingComplete(uint32_t buildingId, const std::string& buildingType);
    void DispatchBuildingDestroyed(uint32_t buildingId);
    void DispatchResourceGathered(const std::string& resourceType, int amount, uint32_t gathererId);
    void DispatchDayStarted(int dayNumber);
    void DispatchNightStarted(int dayNumber);
    void DispatchCustomEvent(const std::string& eventType,
                             const std::unordered_map<std::string, std::any>& data = {});

    // =========================================================================
    // Python Engine Integration
    // =========================================================================

    /**
     * @brief Set the Python engine for calling Python handlers
     */
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }

    // =========================================================================
    // Metrics
    // =========================================================================

    struct DispatcherMetrics {
        size_t totalEventsDispatched = 0;
        size_t totalEventsCancelled = 0;
        size_t totalHandlersCalled = 0;
        std::unordered_map<EventType, size_t> eventsPerType;
        double totalDispatchTimeMs = 0.0;

        void Reset() {
            totalEventsDispatched = 0;
            totalEventsCancelled = 0;
            totalHandlersCalled = 0;
            eventsPerType.clear();
            totalDispatchTimeMs = 0.0;
        }
    };

    [[nodiscard]] const DispatcherMetrics& GetMetrics() const { return m_metrics; }
    void ResetMetrics() { m_metrics.Reset(); }

private:
    // Handler storage
    std::unordered_map<size_t, EventHandler> m_handlers;
    std::unordered_map<EventType, std::vector<size_t>> m_handlersByType;
    std::unordered_map<std::string, std::vector<size_t>> m_handlersByCustomType;
    std::unordered_map<std::string, size_t> m_handlersByName;
    size_t m_nextHandlerId = 1;

    // Event queue
    struct QueuedEvent {
        GameEvent event;
        float delay;
        std::chrono::system_clock::time_point queueTime;
    };
    std::queue<QueuedEvent> m_eventQueue;
    std::vector<QueuedEvent> m_delayedEvents;

    // Thread safety
    mutable std::mutex m_handlerMutex;
    mutable std::mutex m_queueMutex;

    // Python integration
    PythonEngine* m_pythonEngine = nullptr;

    // Metrics
    DispatcherMetrics m_metrics;

    // Helpers
    void SortHandlers(EventType eventType);
    void SortCustomHandlers(const std::string& customType);
    void CallHandler(EventHandler& handler, GameEvent& event);
    void CallPythonHandler(const EventHandler& handler, const GameEvent& event);
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline const char* EventTypeToString(EventType type) {
    switch (type) {
        case EventType::EntitySpawn: return "EntitySpawn";
        case EventType::EntityDeath: return "EntityDeath";
        case EventType::EntityDamaged: return "EntityDamaged";
        case EventType::EntityHealed: return "EntityHealed";
        case EventType::EntityMoved: return "EntityMoved";
        case EventType::AttackStarted: return "AttackStarted";
        case EventType::AttackEnded: return "AttackEnded";
        case EventType::ProjectileFired: return "ProjectileFired";
        case EventType::ProjectileHit: return "ProjectileHit";
        case EventType::TileEnter: return "TileEnter";
        case EventType::TileExit: return "TileExit";
        case EventType::ZoneEnter: return "ZoneEnter";
        case EventType::ZoneExit: return "ZoneExit";
        case EventType::BuildingPlaced: return "BuildingPlaced";
        case EventType::BuildingComplete: return "BuildingComplete";
        case EventType::BuildingDestroyed: return "BuildingDestroyed";
        case EventType::BuildingUpgraded: return "BuildingUpgraded";
        case EventType::WorkerAssigned: return "WorkerAssigned";
        case EventType::WorkerUnassigned: return "WorkerUnassigned";
        case EventType::ResourceGathered: return "ResourceGathered";
        case EventType::ResourceDepleted: return "ResourceDepleted";
        case EventType::ResourceStockChanged: return "ResourceStockChanged";
        case EventType::TradeCompleted: return "TradeCompleted";
        case EventType::DayStarted: return "DayStarted";
        case EventType::NightStarted: return "NightStarted";
        case EventType::HourPassed: return "HourPassed";
        case EventType::MinutePassed: return "MinutePassed";
        case EventType::GameStarted: return "GameStarted";
        case EventType::GamePaused: return "GamePaused";
        case EventType::GameResumed: return "GameResumed";
        case EventType::GameSaved: return "GameSaved";
        case EventType::GameLoaded: return "GameLoaded";
        case EventType::LevelLoaded: return "LevelLoaded";
        case EventType::PlayerSpawn: return "PlayerSpawn";
        case EventType::PlayerDeath: return "PlayerDeath";
        case EventType::PlayerLevelUp: return "PlayerLevelUp";
        case EventType::ItemPickup: return "ItemPickup";
        case EventType::ItemUsed: return "ItemUsed";
        case EventType::QuestStarted: return "QuestStarted";
        case EventType::QuestCompleted: return "QuestCompleted";
        case EventType::AIStateChanged: return "AIStateChanged";
        case EventType::AITargetChanged: return "AITargetChanged";
        case EventType::AIPathCompleted: return "AIPathCompleted";
        case EventType::Custom: return "Custom";
        default: return "Unknown";
    }
}

inline GameEvent GameEvent::EntityEvent(EventType type, uint32_t entityId) {
    GameEvent evt;
    evt.type = type;
    evt.entityId = entityId;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

inline GameEvent GameEvent::BuildingEvent(EventType type, uint32_t buildingId) {
    GameEvent evt;
    evt.type = type;
    evt.buildingId = buildingId;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

inline GameEvent GameEvent::TileEvent(EventType type, int tileX, int tileY, uint32_t entityId) {
    GameEvent evt;
    evt.type = type;
    evt.tileX = tileX;
    evt.tileY = tileY;
    evt.entityId = entityId;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

inline GameEvent GameEvent::ResourceEvent(EventType type, const std::string& resourceType, int amount) {
    GameEvent evt;
    evt.type = type;
    evt.stringValue = resourceType;
    evt.intValue = amount;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

inline GameEvent GameEvent::CustomEvent(const std::string& customType) {
    GameEvent evt;
    evt.type = EventType::Custom;
    evt.customType = customType;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

} // namespace Scripting
} // namespace Nova
