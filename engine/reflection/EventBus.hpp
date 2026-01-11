#pragma once

#include "TypeInfo.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <chrono>
#include <any>
#include <optional>
#include <atomic>

namespace Nova {
namespace Reflect {

/**
 * @brief Event priority levels for handler ordering
 */
enum class EventPriority : int {
    Lowest = 0,
    Low = 25,
    Normal = 50,
    High = 75,
    Highest = 100,
    Monitor = 200  // Cannot cancel, just observes
};

/**
 * @brief Base event structure for the event bus
 */
struct BusEvent {
    std::string eventType;
    std::string sourceType;
    void* source = nullptr;
    uint64_t sourceId = 0;
    std::chrono::system_clock::time_point timestamp;
    bool cancelled = false;
    bool propagate = true;

    // Event data
    std::unordered_map<std::string, std::any> data;

    // Constructors
    BusEvent() : timestamp(std::chrono::system_clock::now()) {}
    explicit BusEvent(const std::string& type)
        : eventType(type), timestamp(std::chrono::system_clock::now()) {}
    BusEvent(const std::string& type, const std::string& srcType, uint64_t srcId = 0)
        : eventType(type), sourceType(srcType), sourceId(srcId),
          timestamp(std::chrono::system_clock::now()) {}

    // Data access
    template<typename T>
    void SetData(const std::string& key, const T& value) {
        data[key] = value;
    }

    template<typename T>
    [[nodiscard]] std::optional<T> GetData(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    template<typename T>
    [[nodiscard]] T GetDataOr(const std::string& key, const T& defaultValue) const {
        return GetData<T>(key).value_or(defaultValue);
    }

    [[nodiscard]] bool HasData(const std::string& key) const {
        return data.contains(key);
    }

    // Cancellation
    void Cancel() { cancelled = true; }
    void StopPropagation() { propagate = false; }
    [[nodiscard]] bool IsCancelled() const { return cancelled; }
    [[nodiscard]] bool ShouldPropagate() const { return propagate; }
};

/**
 * @brief Event handler subscription info
 */
struct EventSubscription {
    size_t id = 0;
    std::string name;
    std::string eventType;
    std::string sourceTypeFilter;  // "*" for all
    EventPriority priority = EventPriority::Normal;
    std::function<void(BusEvent&)> handler;
    bool enabled = true;
    std::atomic<uint64_t> callCount{0};
};

/**
 * @brief Event history entry for debugging
 */
struct EventHistoryEntry {
    BusEvent event;
    std::chrono::system_clock::time_point dispatchTime;
    double processingTimeMs = 0.0;
    size_t handlersCalled = 0;
    bool wasCancelled = false;
};

/**
 * @brief Central event routing system with publish/subscribe pattern
 *
 * Features:
 * - Publish/subscribe event pattern
 * - Event filtering by type and source
 * - Priority-based handler ordering
 * - Async event queue option
 * - Event history for debugging
 *
 * Usage:
 * @code
 * auto& bus = EventBus::Instance();
 *
 * // Subscribe to events
 * bus.Subscribe("OnDamage", [](BusEvent& evt) {
 *     float damage = evt.GetData<float>("damage").value_or(0.0f);
 *     // Handle damage
 * });
 *
 * // Publish an event
 * BusEvent evt("OnDamage", "Unit", unitId);
 * evt.SetData("damage", 50.0f);
 * bus.Publish(evt);
 * @endcode
 */
class EventBus {
public:
    /**
     * @brief Get singleton instance
     */
    static EventBus& Instance();

    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // =========================================================================
    // Subscription
    // =========================================================================

    /**
     * @brief Subscribe to an event type
     * @param eventType Event type to subscribe to ("*" for all events)
     * @param handler Handler function
     * @param priority Handler priority
     * @return Subscription ID
     */
    size_t Subscribe(const std::string& eventType,
                     std::function<void(BusEvent&)> handler,
                     EventPriority priority = EventPriority::Normal);

    /**
     * @brief Subscribe with a name for debugging
     */
    size_t Subscribe(const std::string& name,
                     const std::string& eventType,
                     std::function<void(BusEvent&)> handler,
                     EventPriority priority = EventPriority::Normal);

    /**
     * @brief Subscribe with source type filter
     */
    size_t SubscribeFiltered(const std::string& eventType,
                             const std::string& sourceTypeFilter,
                             std::function<void(BusEvent&)> handler,
                             EventPriority priority = EventPriority::Normal);

    /**
     * @brief Unsubscribe by ID
     */
    bool Unsubscribe(size_t subscriptionId);

    /**
     * @brief Unsubscribe by name
     */
    bool Unsubscribe(const std::string& name);

    /**
     * @brief Unsubscribe all handlers for an event type
     */
    void UnsubscribeAll(const std::string& eventType);

    /**
     * @brief Enable/disable a subscription
     */
    void SetEnabled(size_t subscriptionId, bool enabled);

    /**
     * @brief Check if subscription exists
     */
    [[nodiscard]] bool HasSubscription(size_t subscriptionId) const;
    [[nodiscard]] bool HasSubscription(const std::string& name) const;

    /**
     * @brief Get all subscription names for an event type
     */
    [[nodiscard]] std::vector<std::string> GetSubscriptions(const std::string& eventType) const;

    // =========================================================================
    // Event Publishing
    // =========================================================================

    /**
     * @brief Publish an event immediately (synchronous)
     * @param event Event to publish
     * @return true if event was not cancelled
     */
    bool Publish(BusEvent& event);

    /**
     * @brief Publish an event by type with data
     */
    bool Publish(const std::string& eventType,
                 const std::unordered_map<std::string, std::any>& data = {});

    /**
     * @brief Queue an event for deferred processing
     */
    void QueueEvent(const BusEvent& event);

    /**
     * @brief Queue an event with delay
     * @param event Event to queue
     * @param delaySeconds Delay in seconds
     */
    void QueueDelayed(BusEvent event, float delaySeconds);

    /**
     * @brief Process all queued events
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
    // Async Events
    // =========================================================================

    /**
     * @brief Enable/disable async processing
     */
    void SetAsyncEnabled(bool enabled);

    /**
     * @brief Check if async processing is enabled
     */
    [[nodiscard]] bool IsAsyncEnabled() const { return m_asyncEnabled; }

    // =========================================================================
    // Event History (Debugging)
    // =========================================================================

    /**
     * @brief Enable event history recording
     */
    void SetHistoryEnabled(bool enabled, size_t maxEntries = 1000);

    /**
     * @brief Get event history
     */
    [[nodiscard]] std::vector<EventHistoryEntry> GetHistory() const;

    /**
     * @brief Get history for specific event type
     */
    [[nodiscard]] std::vector<EventHistoryEntry> GetHistory(const std::string& eventType) const;

    /**
     * @brief Clear event history
     */
    void ClearHistory();

    /**
     * @brief Check if history is enabled
     */
    [[nodiscard]] bool IsHistoryEnabled() const { return m_historyEnabled; }

    // =========================================================================
    // Metrics
    // =========================================================================

    struct Metrics {
        std::atomic<uint64_t> totalEventsPublished{0};
        std::atomic<uint64_t> totalEventsCancelled{0};
        std::atomic<uint64_t> totalHandlersCalled{0};
        std::atomic<uint64_t> totalQueuedEvents{0};
        double totalProcessingTimeMs = 0.0;
        std::unordered_map<std::string, uint64_t> eventsPerType;

        void Reset() {
            totalEventsPublished = 0;
            totalEventsCancelled = 0;
            totalHandlersCalled = 0;
            totalQueuedEvents = 0;
            totalProcessingTimeMs = 0.0;
            eventsPerType.clear();
        }
    };

    [[nodiscard]] const Metrics& GetMetrics() const { return m_metrics; }
    void ResetMetrics() { m_metrics.Reset(); }

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Get number of subscriptions
     */
    [[nodiscard]] size_t GetSubscriptionCount() const;

    /**
     * @brief Get all registered event types
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredEventTypes() const;

    /**
     * @brief Remove all subscriptions
     */
    void Clear();

private:
    EventBus() = default;
    ~EventBus() = default;

    void SortSubscriptions(const std::string& eventType);
    void RecordHistory(const BusEvent& event, double processingTimeMs, size_t handlersCalled);

    // Subscriptions
    std::unordered_map<size_t, std::unique_ptr<EventSubscription>> m_subscriptions;
    std::unordered_map<std::string, std::vector<size_t>> m_subscriptionsByType;
    std::unordered_map<std::string, size_t> m_subscriptionsByName;
    std::vector<size_t> m_wildcardSubscriptions;  // Subscriptions for "*" (all events)
    size_t m_nextSubscriptionId = 1;

    // Event queue
    struct QueuedEvent {
        BusEvent event;
        float delay = 0.0f;
    };
    std::queue<BusEvent> m_immediateQueue;
    std::vector<QueuedEvent> m_delayedQueue;

    // History
    bool m_historyEnabled = false;
    size_t m_maxHistoryEntries = 1000;
    std::vector<EventHistoryEntry> m_history;

    // Async
    bool m_asyncEnabled = false;

    // Metrics
    Metrics m_metrics;

    // Thread safety
    mutable std::mutex m_subscriptionMutex;
    mutable std::mutex m_queueMutex;
    mutable std::mutex m_historyMutex;
};

/**
 * @brief RAII subscription guard
 */
class EventSubscriptionGuard {
public:
    EventSubscriptionGuard() = default;
    explicit EventSubscriptionGuard(size_t id) : m_id(id) {}
    ~EventSubscriptionGuard() {
        if (m_id != 0) {
            EventBus::Instance().Unsubscribe(m_id);
        }
    }

    EventSubscriptionGuard(const EventSubscriptionGuard&) = delete;
    EventSubscriptionGuard& operator=(const EventSubscriptionGuard&) = delete;

    EventSubscriptionGuard(EventSubscriptionGuard&& other) noexcept : m_id(other.m_id) {
        other.m_id = 0;
    }

    EventSubscriptionGuard& operator=(EventSubscriptionGuard&& other) noexcept {
        if (this != &other) {
            if (m_id != 0) {
                EventBus::Instance().Unsubscribe(m_id);
            }
            m_id = other.m_id;
            other.m_id = 0;
        }
        return *this;
    }

    void Release() { m_id = 0; }
    [[nodiscard]] size_t GetId() const { return m_id; }

private:
    size_t m_id = 0;
};

} // namespace Reflect
} // namespace Nova
