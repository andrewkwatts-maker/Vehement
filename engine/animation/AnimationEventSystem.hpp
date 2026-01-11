#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <memory>
#include <deque>
#include <chrono>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace Nova {

using json = nlohmann::json;

/**
 * @brief Priority levels for event handlers
 */
enum class EventPriority {
    Lowest = 0,
    Low = 25,
    Normal = 50,
    High = 75,
    Highest = 100,
    Critical = 127
};

/**
 * @brief Animation event data
 */
struct AnimationEventData {
    std::string eventName;
    json data;
    float timestamp = 0.0f;
    std::string source;             // Entity or animation that triggered the event
    int priority = static_cast<int>(EventPriority::Normal);
    bool consumed = false;          // Whether event was handled
    uint64_t eventId = 0;           // Unique event ID

    [[nodiscard]] json ToJson() const;
    static AnimationEventData FromJson(const json& j);
};

/**
 * @brief Event handler callback signature
 */
using EventHandler = std::function<void(AnimationEventData&)>;

/**
 * @brief Handler registration info
 */
struct EventHandlerInfo {
    std::string handlerId;
    std::string eventPattern;       // Can be exact name or wildcard pattern
    EventHandler callback;
    int priority = static_cast<int>(EventPriority::Normal);
    bool once = false;              // Remove after first invocation
    bool async = false;             // Process asynchronously
};

/**
 * @brief Event history entry for replay
 */
struct EventHistoryEntry {
    AnimationEventData event;
    std::vector<std::string> handlersInvoked;
    float processingTime = 0.0f;
};

/**
 * @brief Event system for animation events
 *
 * Features:
 * - Register event handlers with priority ordering
 * - Event queuing and batching
 * - Async event processing
 * - Event history for replay/debugging
 * - Wildcard pattern matching
 */
class AnimationEventSystem {
public:
    AnimationEventSystem() = default;
    ~AnimationEventSystem();

    AnimationEventSystem(const AnimationEventSystem&) = delete;
    AnimationEventSystem& operator=(const AnimationEventSystem&) = delete;
    AnimationEventSystem(AnimationEventSystem&&) noexcept = default;
    AnimationEventSystem& operator=(AnimationEventSystem&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Handler Registration
    // -------------------------------------------------------------------------

    /**
     * @brief Register an event handler
     * @param eventName Event name or pattern (supports * wildcard)
     * @param handler Callback function
     * @param priority Handler priority (higher = called first)
     * @return Handler ID for unregistering
     */
    std::string RegisterHandler(const std::string& eventName,
                                 EventHandler handler,
                                 EventPriority priority = EventPriority::Normal);

    /**
     * @brief Register a one-time handler (auto-removed after first call)
     */
    std::string RegisterOnceHandler(const std::string& eventName,
                                     EventHandler handler,
                                     EventPriority priority = EventPriority::Normal);

    /**
     * @brief Register an async handler
     */
    std::string RegisterAsyncHandler(const std::string& eventName,
                                      EventHandler handler,
                                      EventPriority priority = EventPriority::Normal);

    /**
     * @brief Unregister a handler by ID
     */
    bool UnregisterHandler(const std::string& handlerId);

    /**
     * @brief Unregister all handlers for an event
     */
    void UnregisterAllHandlers(const std::string& eventName);

    /**
     * @brief Clear all handlers
     */
    void ClearAllHandlers();

    /**
     * @brief Get number of registered handlers
     */
    [[nodiscard]] size_t GetHandlerCount() const { return m_handlers.size(); }

    /**
     * @brief Get handlers for an event
     */
    [[nodiscard]] std::vector<std::string> GetHandlersForEvent(const std::string& eventName) const;

    // -------------------------------------------------------------------------
    // Event Dispatching
    // -------------------------------------------------------------------------

    /**
     * @brief Dispatch an event immediately
     */
    void DispatchEvent(const std::string& eventName, const json& data = {});

    /**
     * @brief Dispatch an event with full data
     */
    void DispatchEvent(const AnimationEventData& event);

    /**
     * @brief Queue an event for later processing
     */
    void QueueEvent(const std::string& eventName, const json& data = {});

    /**
     * @brief Queue an event with full data
     */
    void QueueEvent(const AnimationEventData& event);

    /**
     * @brief Queue an event with delay
     */
    void QueueDelayedEvent(const std::string& eventName, float delay, const json& data = {});

    /**
     * @brief Process all queued events
     * @param maxEvents Maximum events to process (0 = unlimited)
     * @param maxTime Maximum time in seconds (0 = unlimited)
     */
    void ProcessQueue(size_t maxEvents = 0, float maxTime = 0.0f);

    /**
     * @brief Process delayed events
     * @param currentTime Current game time
     */
    void ProcessDelayedEvents(float currentTime);

    /**
     * @brief Get number of queued events
     */
    [[nodiscard]] size_t GetQueueSize() const { return m_eventQueue.size(); }

    /**
     * @brief Clear event queue
     */
    void ClearQueue();

    // -------------------------------------------------------------------------
    // Batching
    // -------------------------------------------------------------------------

    /**
     * @brief Begin batch mode - events are queued instead of dispatched
     */
    void BeginBatch();

    /**
     * @brief End batch mode and process all batched events
     */
    void EndBatch();

    /**
     * @brief Check if in batch mode
     */
    [[nodiscard]] bool IsBatching() const { return m_batchMode; }

    // -------------------------------------------------------------------------
    // Event History
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable event history recording
     */
    void SetRecordHistory(bool record);
    [[nodiscard]] bool IsRecordingHistory() const { return m_recordHistory; }

    /**
     * @brief Get event history
     */
    [[nodiscard]] const std::deque<EventHistoryEntry>& GetHistory() const { return m_history; }

    /**
     * @brief Clear event history
     */
    void ClearHistory() { m_history.clear(); }

    /**
     * @brief Replay event history
     */
    void ReplayHistory(const std::deque<EventHistoryEntry>& history);

    /**
     * @brief Set maximum history size
     */
    void SetMaxHistorySize(size_t size) { m_maxHistorySize = size; }

    /**
     * @brief Export history to JSON
     */
    [[nodiscard]] json ExportHistory() const;

    /**
     * @brief Import history from JSON
     */
    void ImportHistory(const json& historyData);

    // -------------------------------------------------------------------------
    // Async Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Process async events (call from async context)
     */
    void ProcessAsyncQueue();

    /**
     * @brief Check if there are pending async events
     */
    [[nodiscard]] bool HasPendingAsyncEvents() const;

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    /**
     * @brief Get event statistics
     */
    [[nodiscard]] json GetStatistics() const;

    /**
     * @brief Reset statistics
     */
    void ResetStatistics();

    /**
     * @brief Set current time provider
     */
    void SetTimeProvider(std::function<float()> timeProvider) {
        m_timeProvider = std::move(timeProvider);
    }

private:
    struct DelayedEvent {
        AnimationEventData event;
        float triggerTime;

        bool operator>(const DelayedEvent& other) const {
            return triggerTime > other.triggerTime;
        }
    };

    void ProcessEvent(AnimationEventData& event);
    [[nodiscard]] bool MatchesPattern(const std::string& pattern, const std::string& eventName) const;
    [[nodiscard]] uint64_t GenerateEventId();
    [[nodiscard]] float GetCurrentTime() const;
    void RecordEvent(const AnimationEventData& event, const std::vector<std::string>& handlers, float processingTime);

    // Handlers
    std::vector<EventHandlerInfo> m_handlers;
    std::unordered_map<std::string, std::vector<size_t>> m_handlersByEvent;
    uint64_t m_nextHandlerId = 1;

    // Event queues
    std::queue<AnimationEventData> m_eventQueue;
    std::priority_queue<DelayedEvent, std::vector<DelayedEvent>, std::greater<DelayedEvent>> m_delayedQueue;
    std::queue<AnimationEventData> m_asyncQueue;
    std::mutex m_asyncMutex;

    // Batch mode
    bool m_batchMode = false;

    // History
    bool m_recordHistory = false;
    std::deque<EventHistoryEntry> m_history;
    size_t m_maxHistorySize = 1000;

    // Statistics
    std::atomic<uint64_t> m_totalEventsDispatched{0};
    std::atomic<uint64_t> m_totalEventsQueued{0};
    std::atomic<uint64_t> m_totalEventsProcessed{0};
    std::unordered_map<std::string, uint64_t> m_eventCounts;

    // Time
    std::function<float()> m_timeProvider;
    uint64_t m_nextEventId = 1;

    // Handlers to remove after processing
    std::vector<std::string> m_handlersToRemove;
};

/**
 * @brief Global event bus for cross-system communication
 */
class GlobalAnimationEventBus {
public:
    static GlobalAnimationEventBus& Instance();

    /**
     * @brief Get the global event system
     */
    [[nodiscard]] AnimationEventSystem& GetEventSystem() { return m_eventSystem; }

    /**
     * @brief Dispatch event globally
     */
    void Dispatch(const std::string& eventName, const json& data = {});

    /**
     * @brief Subscribe to global events
     */
    std::string Subscribe(const std::string& eventName, EventHandler handler,
                           EventPriority priority = EventPriority::Normal);

    /**
     * @brief Unsubscribe from global events
     */
    void Unsubscribe(const std::string& handlerId);

private:
    GlobalAnimationEventBus() = default;
    AnimationEventSystem m_eventSystem;
};

/**
 * @brief Scoped event batch helper
 */
class ScopedEventBatch {
public:
    explicit ScopedEventBatch(AnimationEventSystem& system) : m_system(system) {
        m_system.BeginBatch();
    }

    ~ScopedEventBatch() {
        m_system.EndBatch();
    }

    ScopedEventBatch(const ScopedEventBatch&) = delete;
    ScopedEventBatch& operator=(const ScopedEventBatch&) = delete;

private:
    AnimationEventSystem& m_system;
};

/**
 * @brief Event filter for selective event handling
 */
class EventFilter {
public:
    /**
     * @brief Add event to whitelist
     */
    void Allow(const std::string& eventName);

    /**
     * @brief Add event to blacklist
     */
    void Block(const std::string& eventName);

    /**
     * @brief Check if event should pass
     */
    [[nodiscard]] bool ShouldPass(const std::string& eventName) const;

    /**
     * @brief Clear all filters
     */
    void Clear();

    /**
     * @brief Set to whitelist mode (only allow specified events)
     */
    void SetWhitelistMode(bool whitelist) { m_whitelistMode = whitelist; }

private:
    std::vector<std::string> m_whitelist;
    std::vector<std::string> m_blacklist;
    bool m_whitelistMode = false;
};

} // namespace Nova
