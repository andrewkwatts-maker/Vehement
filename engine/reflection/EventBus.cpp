#include "EventBus.hpp"
#include <algorithm>
#include <chrono>

namespace Nova {
namespace Reflect {

// ============================================================================
// EventBus Singleton
// ============================================================================

EventBus& EventBus::Instance() {
    static EventBus instance;
    return instance;
}

// ============================================================================
// Subscription
// ============================================================================

size_t EventBus::Subscribe(const std::string& eventType,
                           std::function<void(BusEvent&)> handler,
                           EventPriority priority) {
    return Subscribe("", eventType, std::move(handler), priority);
}

size_t EventBus::Subscribe(const std::string& name,
                           const std::string& eventType,
                           std::function<void(BusEvent&)> handler,
                           EventPriority priority) {
    std::lock_guard lock(m_subscriptionMutex);

    auto sub = std::make_unique<EventSubscription>();
    sub->id = m_nextSubscriptionId++;
    sub->name = name;
    sub->eventType = eventType;
    sub->priority = priority;
    sub->handler = std::move(handler);
    sub->enabled = true;

    size_t id = sub->id;

    // Store subscription
    m_subscriptions[id] = std::move(sub);

    // Index by type
    if (eventType == "*") {
        m_wildcardSubscriptions.push_back(id);
    } else {
        m_subscriptionsByType[eventType].push_back(id);
        SortSubscriptions(eventType);
    }

    // Index by name if provided
    if (!name.empty()) {
        m_subscriptionsByName[name] = id;
    }

    return id;
}

size_t EventBus::SubscribeFiltered(const std::string& eventType,
                                   const std::string& sourceTypeFilter,
                                   std::function<void(BusEvent&)> handler,
                                   EventPriority priority) {
    std::lock_guard lock(m_subscriptionMutex);

    auto sub = std::make_unique<EventSubscription>();
    sub->id = m_nextSubscriptionId++;
    sub->eventType = eventType;
    sub->sourceTypeFilter = sourceTypeFilter;
    sub->priority = priority;
    sub->handler = std::move(handler);
    sub->enabled = true;

    size_t id = sub->id;
    m_subscriptions[id] = std::move(sub);

    if (eventType == "*") {
        m_wildcardSubscriptions.push_back(id);
    } else {
        m_subscriptionsByType[eventType].push_back(id);
        SortSubscriptions(eventType);
    }

    return id;
}

bool EventBus::Unsubscribe(size_t subscriptionId) {
    std::lock_guard lock(m_subscriptionMutex);

    auto it = m_subscriptions.find(subscriptionId);
    if (it == m_subscriptions.end()) {
        return false;
    }

    const auto& sub = it->second;

    // Remove from name index
    if (!sub->name.empty()) {
        m_subscriptionsByName.erase(sub->name);
    }

    // Remove from type index
    if (sub->eventType == "*") {
        auto& vec = m_wildcardSubscriptions;
        vec.erase(std::remove(vec.begin(), vec.end(), subscriptionId), vec.end());
    } else {
        auto typeIt = m_subscriptionsByType.find(sub->eventType);
        if (typeIt != m_subscriptionsByType.end()) {
            auto& vec = typeIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), subscriptionId), vec.end());
        }
    }

    m_subscriptions.erase(it);
    return true;
}

bool EventBus::Unsubscribe(const std::string& name) {
    std::lock_guard lock(m_subscriptionMutex);

    auto it = m_subscriptionsByName.find(name);
    if (it == m_subscriptionsByName.end()) {
        return false;
    }

    size_t id = it->second;
    m_subscriptionMutex.unlock();
    return Unsubscribe(id);
}

void EventBus::UnsubscribeAll(const std::string& eventType) {
    std::lock_guard lock(m_subscriptionMutex);

    auto it = m_subscriptionsByType.find(eventType);
    if (it == m_subscriptionsByType.end()) {
        return;
    }

    for (size_t id : it->second) {
        auto subIt = m_subscriptions.find(id);
        if (subIt != m_subscriptions.end()) {
            if (!subIt->second->name.empty()) {
                m_subscriptionsByName.erase(subIt->second->name);
            }
            m_subscriptions.erase(subIt);
        }
    }

    it->second.clear();
}

void EventBus::SetEnabled(size_t subscriptionId, bool enabled) {
    std::lock_guard lock(m_subscriptionMutex);

    auto it = m_subscriptions.find(subscriptionId);
    if (it != m_subscriptions.end()) {
        it->second->enabled = enabled;
    }
}

bool EventBus::HasSubscription(size_t subscriptionId) const {
    std::lock_guard lock(m_subscriptionMutex);
    return m_subscriptions.contains(subscriptionId);
}

bool EventBus::HasSubscription(const std::string& name) const {
    std::lock_guard lock(m_subscriptionMutex);
    return m_subscriptionsByName.contains(name);
}

std::vector<std::string> EventBus::GetSubscriptions(const std::string& eventType) const {
    std::lock_guard lock(m_subscriptionMutex);

    std::vector<std::string> names;
    auto it = m_subscriptionsByType.find(eventType);
    if (it != m_subscriptionsByType.end()) {
        for (size_t id : it->second) {
            auto subIt = m_subscriptions.find(id);
            if (subIt != m_subscriptions.end() && !subIt->second->name.empty()) {
                names.push_back(subIt->second->name);
            }
        }
    }
    return names;
}

// ============================================================================
// Event Publishing
// ============================================================================

bool EventBus::Publish(BusEvent& event) {
    auto startTime = std::chrono::high_resolution_clock::now();
    size_t handlersCalled = 0;

    // Collect handlers to call
    std::vector<EventSubscription*> handlers;

    {
        std::lock_guard lock(m_subscriptionMutex);

        // Add type-specific handlers
        auto it = m_subscriptionsByType.find(event.eventType);
        if (it != m_subscriptionsByType.end()) {
            for (size_t id : it->second) {
                auto subIt = m_subscriptions.find(id);
                if (subIt != m_subscriptions.end()) {
                    handlers.push_back(subIt->second.get());
                }
            }
        }

        // Add wildcard handlers
        for (size_t id : m_wildcardSubscriptions) {
            auto subIt = m_subscriptions.find(id);
            if (subIt != m_subscriptions.end()) {
                handlers.push_back(subIt->second.get());
            }
        }
    }

    // Sort by priority
    std::sort(handlers.begin(), handlers.end(),
        [](const EventSubscription* a, const EventSubscription* b) {
            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
        });

    // Call handlers
    for (auto* sub : handlers) {
        if (!sub->enabled) continue;

        // Apply source type filter
        if (!sub->sourceTypeFilter.empty() && sub->sourceTypeFilter != "*" &&
            sub->sourceTypeFilter != event.sourceType) {
            continue;
        }

        // Call handler
        if (sub->handler) {
            sub->handler(event);
            sub->callCount++;
            handlersCalled++;
            m_metrics.totalHandlersCalled++;
        }

        // Check for cancellation (skip Monitor priority)
        if ((event.cancelled || !event.propagate) &&
            sub->priority != EventPriority::Monitor) {
            break;
        }
    }

    // Update metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    double processingTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.totalProcessingTimeMs += processingTimeMs;
    m_metrics.totalEventsPublished++;
    m_metrics.eventsPerType[event.eventType]++;

    if (event.cancelled) {
        m_metrics.totalEventsCancelled++;
    }

    // Record history
    if (m_historyEnabled) {
        RecordHistory(event, processingTimeMs, handlersCalled);
    }

    return !event.cancelled;
}

bool EventBus::Publish(const std::string& eventType,
                       const std::unordered_map<std::string, std::any>& data) {
    BusEvent event(eventType);
    event.data = data;
    return Publish(event);
}

void EventBus::QueueEvent(const BusEvent& event) {
    std::lock_guard lock(m_queueMutex);
    m_immediateQueue.push(event);
    m_metrics.totalQueuedEvents++;
}

void EventBus::QueueDelayed(BusEvent event, float delaySeconds) {
    std::lock_guard lock(m_queueMutex);
    m_delayedQueue.push_back({std::move(event), delaySeconds});
    m_metrics.totalQueuedEvents++;
}

void EventBus::ProcessQueue(float deltaTime) {
    // Process delayed events
    {
        std::lock_guard lock(m_queueMutex);
        auto it = m_delayedQueue.begin();
        while (it != m_delayedQueue.end()) {
            it->delay -= deltaTime;
            if (it->delay <= 0.0f) {
                m_immediateQueue.push(std::move(it->event));
                it = m_delayedQueue.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Process immediate queue
    while (true) {
        BusEvent event;

        {
            std::lock_guard lock(m_queueMutex);
            if (m_immediateQueue.empty()) break;
            event = std::move(m_immediateQueue.front());
            m_immediateQueue.pop();
        }

        Publish(event);
    }
}

void EventBus::ClearQueue() {
    std::lock_guard lock(m_queueMutex);
    while (!m_immediateQueue.empty()) {
        m_immediateQueue.pop();
    }
    m_delayedQueue.clear();
}

size_t EventBus::GetQueueSize() const {
    std::lock_guard lock(m_queueMutex);
    return m_immediateQueue.size() + m_delayedQueue.size();
}

// ============================================================================
// Async Events
// ============================================================================

void EventBus::SetAsyncEnabled(bool enabled) {
    m_asyncEnabled = enabled;
}

// ============================================================================
// Event History
// ============================================================================

void EventBus::SetHistoryEnabled(bool enabled, size_t maxEntries) {
    std::lock_guard lock(m_historyMutex);
    m_historyEnabled = enabled;
    m_maxHistoryEntries = maxEntries;
    if (!enabled) {
        m_history.clear();
    }
}

std::vector<EventHistoryEntry> EventBus::GetHistory() const {
    std::lock_guard lock(m_historyMutex);
    return m_history;
}

std::vector<EventHistoryEntry> EventBus::GetHistory(const std::string& eventType) const {
    std::lock_guard lock(m_historyMutex);

    std::vector<EventHistoryEntry> filtered;
    for (const auto& entry : m_history) {
        if (entry.event.eventType == eventType) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

void EventBus::ClearHistory() {
    std::lock_guard lock(m_historyMutex);
    m_history.clear();
}

void EventBus::RecordHistory(const BusEvent& event, double processingTimeMs, size_t handlersCalled) {
    std::lock_guard lock(m_historyMutex);

    EventHistoryEntry entry;
    entry.event = event;
    entry.dispatchTime = std::chrono::system_clock::now();
    entry.processingTimeMs = processingTimeMs;
    entry.handlersCalled = handlersCalled;
    entry.wasCancelled = event.cancelled;

    m_history.push_back(std::move(entry));

    // Trim history if needed
    while (m_history.size() > m_maxHistoryEntries) {
        m_history.erase(m_history.begin());
    }
}

// ============================================================================
// Utilities
// ============================================================================

size_t EventBus::GetSubscriptionCount() const {
    std::lock_guard lock(m_subscriptionMutex);
    return m_subscriptions.size();
}

std::vector<std::string> EventBus::GetRegisteredEventTypes() const {
    std::lock_guard lock(m_subscriptionMutex);

    std::vector<std::string> types;
    types.reserve(m_subscriptionsByType.size());
    for (const auto& [type, _] : m_subscriptionsByType) {
        types.push_back(type);
    }
    return types;
}

void EventBus::Clear() {
    std::lock_guard lock(m_subscriptionMutex);
    m_subscriptions.clear();
    m_subscriptionsByType.clear();
    m_subscriptionsByName.clear();
    m_wildcardSubscriptions.clear();
}

void EventBus::SortSubscriptions(const std::string& eventType) {
    auto it = m_subscriptionsByType.find(eventType);
    if (it == m_subscriptionsByType.end()) return;

    std::sort(it->second.begin(), it->second.end(),
        [this](size_t a, size_t b) {
            auto itA = m_subscriptions.find(a);
            auto itB = m_subscriptions.find(b);
            if (itA == m_subscriptions.end() || itB == m_subscriptions.end()) {
                return false;
            }
            return static_cast<int>(itA->second->priority) >
                   static_cast<int>(itB->second->priority);
        });
}

} // namespace Reflect
} // namespace Nova
