#include "AnimationEventSystem.hpp"
#include <algorithm>
#include <chrono>
#include <regex>

namespace Nova {

// ============================================================================
// AnimationEventData
// ============================================================================

json AnimationEventData::ToJson() const {
    return json{
        {"eventName", eventName},
        {"data", data},
        {"timestamp", timestamp},
        {"source", source},
        {"priority", priority},
        {"consumed", consumed},
        {"eventId", eventId}
    };
}

AnimationEventData AnimationEventData::FromJson(const json& j) {
    AnimationEventData event;
    event.eventName = j.value("eventName", "");
    event.timestamp = j.value("timestamp", 0.0f);
    event.source = j.value("source", "");
    event.priority = j.value("priority", static_cast<int>(EventPriority::Normal));
    event.consumed = j.value("consumed", false);
    event.eventId = j.value("eventId", 0ULL);

    if (j.contains("data")) {
        event.data = j["data"];
    }

    return event;
}

// ============================================================================
// AnimationEventSystem
// ============================================================================

AnimationEventSystem::~AnimationEventSystem() {
    ClearAllHandlers();
    ClearQueue();
}

std::string AnimationEventSystem::RegisterHandler(const std::string& eventName,
                                                   EventHandler handler,
                                                   EventPriority priority) {
    EventHandlerInfo info;
    info.handlerId = "handler_" + std::to_string(m_nextHandlerId++);
    info.eventPattern = eventName;
    info.callback = std::move(handler);
    info.priority = static_cast<int>(priority);
    info.once = false;
    info.async = false;

    m_handlers.push_back(std::move(info));

    // Sort by priority (highest first)
    std::sort(m_handlers.begin(), m_handlers.end(),
              [](const EventHandlerInfo& a, const EventHandlerInfo& b) {
                  return a.priority > b.priority;
              });

    return m_handlers.back().handlerId;
}

std::string AnimationEventSystem::RegisterOnceHandler(const std::string& eventName,
                                                       EventHandler handler,
                                                       EventPriority priority) {
    EventHandlerInfo info;
    info.handlerId = "handler_" + std::to_string(m_nextHandlerId++);
    info.eventPattern = eventName;
    info.callback = std::move(handler);
    info.priority = static_cast<int>(priority);
    info.once = true;
    info.async = false;

    m_handlers.push_back(std::move(info));

    std::sort(m_handlers.begin(), m_handlers.end(),
              [](const EventHandlerInfo& a, const EventHandlerInfo& b) {
                  return a.priority > b.priority;
              });

    return m_handlers.back().handlerId;
}

std::string AnimationEventSystem::RegisterAsyncHandler(const std::string& eventName,
                                                        EventHandler handler,
                                                        EventPriority priority) {
    EventHandlerInfo info;
    info.handlerId = "handler_" + std::to_string(m_nextHandlerId++);
    info.eventPattern = eventName;
    info.callback = std::move(handler);
    info.priority = static_cast<int>(priority);
    info.once = false;
    info.async = true;

    m_handlers.push_back(std::move(info));

    std::sort(m_handlers.begin(), m_handlers.end(),
              [](const EventHandlerInfo& a, const EventHandlerInfo& b) {
                  return a.priority > b.priority;
              });

    return m_handlers.back().handlerId;
}

bool AnimationEventSystem::UnregisterHandler(const std::string& handlerId) {
    auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
                           [&handlerId](const EventHandlerInfo& h) {
                               return h.handlerId == handlerId;
                           });

    if (it != m_handlers.end()) {
        m_handlers.erase(it);
        return true;
    }
    return false;
}

void AnimationEventSystem::UnregisterAllHandlers(const std::string& eventName) {
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
                       [&eventName](const EventHandlerInfo& h) {
                           return h.eventPattern == eventName;
                       }),
        m_handlers.end());
}

void AnimationEventSystem::ClearAllHandlers() {
    m_handlers.clear();
}

std::vector<std::string> AnimationEventSystem::GetHandlersForEvent(const std::string& eventName) const {
    std::vector<std::string> result;
    for (const auto& handler : m_handlers) {
        if (MatchesPattern(handler.eventPattern, eventName)) {
            result.push_back(handler.handlerId);
        }
    }
    return result;
}

void AnimationEventSystem::DispatchEvent(const std::string& eventName, const json& data) {
    AnimationEventData event;
    event.eventName = eventName;
    event.data = data;
    event.timestamp = GetCurrentTime();
    event.eventId = GenerateEventId();
    event.priority = static_cast<int>(EventPriority::Normal);

    DispatchEvent(event);
}

void AnimationEventSystem::DispatchEvent(const AnimationEventData& event) {
    if (m_batchMode) {
        QueueEvent(event);
        return;
    }

    m_totalEventsDispatched++;
    m_eventCounts[event.eventName]++;

    AnimationEventData mutableEvent = event;
    if (mutableEvent.eventId == 0) {
        mutableEvent.eventId = GenerateEventId();
    }
    if (mutableEvent.timestamp == 0.0f) {
        mutableEvent.timestamp = GetCurrentTime();
    }

    ProcessEvent(mutableEvent);
}

void AnimationEventSystem::QueueEvent(const std::string& eventName, const json& data) {
    AnimationEventData event;
    event.eventName = eventName;
    event.data = data;
    event.timestamp = GetCurrentTime();
    event.eventId = GenerateEventId();
    event.priority = static_cast<int>(EventPriority::Normal);

    QueueEvent(event);
}

void AnimationEventSystem::QueueEvent(const AnimationEventData& event) {
    m_totalEventsQueued++;

    AnimationEventData mutableEvent = event;
    if (mutableEvent.eventId == 0) {
        mutableEvent.eventId = GenerateEventId();
    }

    m_eventQueue.push(mutableEvent);
}

void AnimationEventSystem::QueueDelayedEvent(const std::string& eventName, float delay, const json& data) {
    AnimationEventData event;
    event.eventName = eventName;
    event.data = data;
    event.eventId = GenerateEventId();
    event.priority = static_cast<int>(EventPriority::Normal);

    DelayedEvent delayed;
    delayed.event = event;
    delayed.triggerTime = GetCurrentTime() + delay;

    m_delayedQueue.push(delayed);
}

void AnimationEventSystem::ProcessQueue(size_t maxEvents, float maxTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    size_t processedCount = 0;

    while (!m_eventQueue.empty()) {
        // Check limits
        if (maxEvents > 0 && processedCount >= maxEvents) {
            break;
        }

        if (maxTime > 0.0f) {
            auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
            float elapsedSeconds = std::chrono::duration<float>(elapsed).count();
            if (elapsedSeconds >= maxTime) {
                break;
            }
        }

        AnimationEventData event = m_eventQueue.front();
        m_eventQueue.pop();

        ProcessEvent(event);
        processedCount++;
    }
}

void AnimationEventSystem::ProcessDelayedEvents(float currentTime) {
    while (!m_delayedQueue.empty()) {
        const auto& top = m_delayedQueue.top();
        if (top.triggerTime > currentTime) {
            break;
        }

        AnimationEventData event = top.event;
        event.timestamp = currentTime;
        m_delayedQueue.pop();

        if (m_batchMode) {
            QueueEvent(event);
        } else {
            ProcessEvent(event);
        }
    }
}

void AnimationEventSystem::ClearQueue() {
    std::queue<AnimationEventData> empty;
    std::swap(m_eventQueue, empty);

    std::priority_queue<DelayedEvent, std::vector<DelayedEvent>, std::greater<DelayedEvent>> emptyDelayed;
    std::swap(m_delayedQueue, emptyDelayed);
}

void AnimationEventSystem::BeginBatch() {
    m_batchMode = true;
}

void AnimationEventSystem::EndBatch() {
    m_batchMode = false;
    ProcessQueue();
}

void AnimationEventSystem::SetRecordHistory(bool record) {
    m_recordHistory = record;
}

void AnimationEventSystem::ReplayHistory(const std::deque<EventHistoryEntry>& history) {
    for (const auto& entry : history) {
        AnimationEventData event = entry.event;
        event.timestamp = GetCurrentTime();
        ProcessEvent(event);
    }
}

json AnimationEventSystem::ExportHistory() const {
    json result = json::array();
    for (const auto& entry : m_history) {
        result.push_back({
            {"event", entry.event.ToJson()},
            {"handlers", entry.handlersInvoked},
            {"processingTime", entry.processingTime}
        });
    }
    return result;
}

void AnimationEventSystem::ImportHistory(const json& historyData) {
    m_history.clear();

    if (!historyData.is_array()) {
        return;
    }

    for (const auto& item : historyData) {
        EventHistoryEntry entry;
        if (item.contains("event")) {
            entry.event = AnimationEventData::FromJson(item["event"]);
        }
        if (item.contains("handlers") && item["handlers"].is_array()) {
            entry.handlersInvoked = item["handlers"].get<std::vector<std::string>>();
        }
        entry.processingTime = item.value("processingTime", 0.0f);
        m_history.push_back(entry);
    }
}

void AnimationEventSystem::ProcessAsyncQueue() {
    std::queue<AnimationEventData> localQueue;

    {
        std::lock_guard<std::mutex> lock(m_asyncMutex);
        std::swap(localQueue, m_asyncQueue);
    }

    while (!localQueue.empty()) {
        AnimationEventData event = localQueue.front();
        localQueue.pop();
        ProcessEvent(event);
    }
}

bool AnimationEventSystem::HasPendingAsyncEvents() const {
    return !m_asyncQueue.empty();
}

json AnimationEventSystem::GetStatistics() const {
    json stats;
    stats["totalEventsDispatched"] = m_totalEventsDispatched.load();
    stats["totalEventsQueued"] = m_totalEventsQueued.load();
    stats["totalEventsProcessed"] = m_totalEventsProcessed.load();
    stats["currentQueueSize"] = m_eventQueue.size();
    stats["currentDelayedQueueSize"] = m_delayedQueue.size();
    stats["handlerCount"] = m_handlers.size();
    stats["historySize"] = m_history.size();

    json eventCountsJson = json::object();
    for (const auto& [name, count] : m_eventCounts) {
        eventCountsJson[name] = count;
    }
    stats["eventCounts"] = eventCountsJson;

    return stats;
}

void AnimationEventSystem::ResetStatistics() {
    m_totalEventsDispatched = 0;
    m_totalEventsQueued = 0;
    m_totalEventsProcessed = 0;
    m_eventCounts.clear();
}

void AnimationEventSystem::ProcessEvent(AnimationEventData& event) {
    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<std::string> invokedHandlers;

    m_handlersToRemove.clear();

    for (auto& handler : m_handlers) {
        if (event.consumed) {
            break;
        }

        if (!MatchesPattern(handler.eventPattern, event.eventName)) {
            continue;
        }

        if (handler.async) {
            // Queue for async processing
            std::lock_guard<std::mutex> lock(m_asyncMutex);
            m_asyncQueue.push(event);
            continue;
        }

        // Invoke handler
        if (handler.callback) {
            handler.callback(event);
            invokedHandlers.push_back(handler.handlerId);
        }

        // Mark one-time handlers for removal
        if (handler.once) {
            m_handlersToRemove.push_back(handler.handlerId);
        }
    }

    // Remove one-time handlers
    for (const auto& id : m_handlersToRemove) {
        UnregisterHandler(id);
    }

    m_totalEventsProcessed++;

    // Record history
    if (m_recordHistory) {
        auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
        float processingTime = std::chrono::duration<float, std::milli>(elapsed).count();
        RecordEvent(event, invokedHandlers, processingTime);
    }
}

bool AnimationEventSystem::MatchesPattern(const std::string& pattern, const std::string& eventName) const {
    if (pattern == eventName) {
        return true;
    }

    // Check for wildcard pattern
    if (pattern.find('*') != std::string::npos) {
        // Convert glob pattern to regex
        std::string regexPattern;
        for (char c : pattern) {
            switch (c) {
                case '*': regexPattern += ".*"; break;
                case '?': regexPattern += "."; break;
                case '.': regexPattern += "\\."; break;
                default: regexPattern += c; break;
            }
        }

        try {
            std::regex re(regexPattern);
            return std::regex_match(eventName, re);
        } catch (...) {
            return false;
        }
    }

    return false;
}

uint64_t AnimationEventSystem::GenerateEventId() {
    return m_nextEventId++;
}

float AnimationEventSystem::GetCurrentTime() const {
    if (m_timeProvider) {
        return m_timeProvider();
    }

    auto now = std::chrono::high_resolution_clock::now();
    auto epoch = std::chrono::high_resolution_clock::time_point();
    return std::chrono::duration<float>(now - epoch).count();
}

void AnimationEventSystem::RecordEvent(const AnimationEventData& event,
                                        const std::vector<std::string>& handlers,
                                        float processingTime) {
    EventHistoryEntry entry;
    entry.event = event;
    entry.handlersInvoked = handlers;
    entry.processingTime = processingTime;

    m_history.push_back(entry);

    // Trim history if too large
    while (m_history.size() > m_maxHistorySize) {
        m_history.pop_front();
    }
}

// ============================================================================
// GlobalAnimationEventBus
// ============================================================================

GlobalAnimationEventBus& GlobalAnimationEventBus::Instance() {
    static GlobalAnimationEventBus instance;
    return instance;
}

void GlobalAnimationEventBus::Dispatch(const std::string& eventName, const json& data) {
    m_eventSystem.DispatchEvent(eventName, data);
}

std::string GlobalAnimationEventBus::Subscribe(const std::string& eventName,
                                                EventHandler handler,
                                                EventPriority priority) {
    return m_eventSystem.RegisterHandler(eventName, std::move(handler), priority);
}

void GlobalAnimationEventBus::Unsubscribe(const std::string& handlerId) {
    m_eventSystem.UnregisterHandler(handlerId);
}

// ============================================================================
// EventFilter
// ============================================================================

void EventFilter::Allow(const std::string& eventName) {
    if (std::find(m_whitelist.begin(), m_whitelist.end(), eventName) == m_whitelist.end()) {
        m_whitelist.push_back(eventName);
    }
    // Remove from blacklist if present
    m_blacklist.erase(std::remove(m_blacklist.begin(), m_blacklist.end(), eventName), m_blacklist.end());
}

void EventFilter::Block(const std::string& eventName) {
    if (std::find(m_blacklist.begin(), m_blacklist.end(), eventName) == m_blacklist.end()) {
        m_blacklist.push_back(eventName);
    }
    // Remove from whitelist if present
    m_whitelist.erase(std::remove(m_whitelist.begin(), m_whitelist.end(), eventName), m_whitelist.end());
}

bool EventFilter::ShouldPass(const std::string& eventName) const {
    // Check blacklist first
    if (std::find(m_blacklist.begin(), m_blacklist.end(), eventName) != m_blacklist.end()) {
        return false;
    }

    // In whitelist mode, only allow whitelisted events
    if (m_whitelistMode) {
        return std::find(m_whitelist.begin(), m_whitelist.end(), eventName) != m_whitelist.end();
    }

    return true;
}

void EventFilter::Clear() {
    m_whitelist.clear();
    m_blacklist.clear();
}

} // namespace Nova
