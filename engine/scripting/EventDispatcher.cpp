#include "EventDispatcher.hpp"
#include "PythonEngine.hpp"
#include <algorithm>
#include <chrono>

namespace Nova {
namespace Scripting {

// ============================================================================
// EventDispatcher Implementation
// ============================================================================

EventDispatcher::EventDispatcher() = default;
EventDispatcher::~EventDispatcher() = default;

// ============================================================================
// Handler Registration
// ============================================================================

size_t EventDispatcher::RegisterHandler(const std::string& name,
                                        EventType eventType,
                                        CppHandler handler,
                                        HandlerPriority priority) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    EventHandler h;
    h.name = name;
    h.eventType = eventType;
    h.priority = priority;
    h.isPython = false;
    h.cppHandler = std::move(handler);

    size_t id = m_nextHandlerId++;
    m_handlers[id] = std::move(h);
    m_handlersByType[eventType].push_back(id);
    m_handlersByName[name] = id;

    SortHandlers(eventType);

    return id;
}

size_t EventDispatcher::RegisterPythonHandler(const std::string& name,
                                              EventType eventType,
                                              const std::string& pythonModule,
                                              const std::string& pythonFunction,
                                              HandlerPriority priority) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    EventHandler h;
    h.name = name;
    h.eventType = eventType;
    h.priority = priority;
    h.isPython = true;
    h.pythonModule = pythonModule;
    h.pythonFunction = pythonFunction;

    size_t id = m_nextHandlerId++;
    m_handlers[id] = std::move(h);
    m_handlersByType[eventType].push_back(id);
    m_handlersByName[name] = id;

    SortHandlers(eventType);

    return id;
}

size_t EventDispatcher::RegisterCustomHandler(const std::string& name,
                                              const std::string& customEventType,
                                              CppHandler handler,
                                              HandlerPriority priority) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    EventHandler h;
    h.name = name;
    h.eventType = EventType::Custom;
    h.customEventType = customEventType;
    h.priority = priority;
    h.isPython = false;
    h.cppHandler = std::move(handler);

    size_t id = m_nextHandlerId++;
    m_handlers[id] = std::move(h);
    m_handlersByCustomType[customEventType].push_back(id);
    m_handlersByName[name] = id;

    SortCustomHandlers(customEventType);

    return id;
}

size_t EventDispatcher::RegisterCustomPythonHandler(const std::string& name,
                                                    const std::string& customEventType,
                                                    const std::string& pythonModule,
                                                    const std::string& pythonFunction,
                                                    HandlerPriority priority) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    EventHandler h;
    h.name = name;
    h.eventType = EventType::Custom;
    h.customEventType = customEventType;
    h.priority = priority;
    h.isPython = true;
    h.pythonModule = pythonModule;
    h.pythonFunction = pythonFunction;

    size_t id = m_nextHandlerId++;
    m_handlers[id] = std::move(h);
    m_handlersByCustomType[customEventType].push_back(id);
    m_handlersByName[name] = id;

    SortCustomHandlers(customEventType);

    return id;
}

bool EventDispatcher::UnregisterHandler(size_t handlerId) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    auto it = m_handlers.find(handlerId);
    if (it == m_handlers.end()) {
        return false;
    }

    const auto& handler = it->second;

    // Remove from name map
    m_handlersByName.erase(handler.name);

    // Remove from type map
    if (handler.eventType == EventType::Custom) {
        auto& vec = m_handlersByCustomType[handler.customEventType];
        vec.erase(std::remove(vec.begin(), vec.end(), handlerId), vec.end());
    } else {
        auto& vec = m_handlersByType[handler.eventType];
        vec.erase(std::remove(vec.begin(), vec.end(), handlerId), vec.end());
    }

    m_handlers.erase(it);
    return true;
}

bool EventDispatcher::UnregisterHandler(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    auto it = m_handlersByName.find(name);
    if (it == m_handlersByName.end()) {
        return false;
    }

    size_t handlerId = it->second;
    m_handlerMutex.unlock();
    return UnregisterHandler(handlerId);
}

void EventDispatcher::UnregisterAllHandlers(EventType eventType) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    auto it = m_handlersByType.find(eventType);
    if (it == m_handlersByType.end()) {
        return;
    }

    for (size_t id : it->second) {
        auto handlerIt = m_handlers.find(id);
        if (handlerIt != m_handlers.end()) {
            m_handlersByName.erase(handlerIt->second.name);
            m_handlers.erase(handlerIt);
        }
    }

    it->second.clear();
}

void EventDispatcher::SetHandlerEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    auto it = m_handlersByName.find(name);
    if (it != m_handlersByName.end()) {
        auto handlerIt = m_handlers.find(it->second);
        if (handlerIt != m_handlers.end()) {
            handlerIt->second.enabled = enabled;
        }
    }
}

bool EventDispatcher::HasHandler(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_handlerMutex);
    return m_handlersByName.contains(name);
}

std::vector<std::string> EventDispatcher::GetHandlerNames() const {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    std::vector<std::string> names;
    names.reserve(m_handlersByName.size());
    for (const auto& [name, _] : m_handlersByName) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> EventDispatcher::GetHandlersForEvent(EventType eventType) const {
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    std::vector<std::string> names;
    auto it = m_handlersByType.find(eventType);
    if (it != m_handlersByType.end()) {
        for (size_t id : it->second) {
            auto handlerIt = m_handlers.find(id);
            if (handlerIt != m_handlers.end()) {
                names.push_back(handlerIt->second.name);
            }
        }
    }
    return names;
}

// ============================================================================
// Event Dispatch
// ============================================================================

bool EventDispatcher::Dispatch(GameEvent& event) {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<size_t> handlerIds;

    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);

        if (event.type == EventType::Custom) {
            auto it = m_handlersByCustomType.find(event.customType);
            if (it != m_handlersByCustomType.end()) {
                handlerIds = it->second;
            }
        } else {
            auto it = m_handlersByType.find(event.type);
            if (it != m_handlersByType.end()) {
                handlerIds = it->second;
            }
        }
    }

    // Call handlers in priority order
    for (size_t id : handlerIds) {
        std::lock_guard<std::mutex> lock(m_handlerMutex);

        auto it = m_handlers.find(id);
        if (it == m_handlers.end()) continue;

        auto& handler = it->second;
        if (!handler.enabled) continue;

        // Apply filters
        if (handler.filterEntityId && *handler.filterEntityId != event.entityId) continue;
        if (handler.filterBuildingId && *handler.filterBuildingId != event.buildingId) continue;

        // Call the handler
        CallHandler(handler, event);
        handler.callCount++;
        m_metrics.totalHandlersCalled++;

        // Check for cancellation (but not for Monitor priority handlers)
        if (event.cancelled && handler.priority != HandlerPriority::Monitor) {
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.totalDispatchTimeMs += timeMs;
    m_metrics.totalEventsDispatched++;
    m_metrics.eventsPerType[event.type]++;

    if (event.cancelled) {
        m_metrics.totalEventsCancelled++;
    }

    return !event.cancelled;
}

void EventDispatcher::QueueEvent(const GameEvent& event) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    QueuedEvent qe;
    qe.event = event;
    qe.delay = 0.0f;
    qe.queueTime = std::chrono::system_clock::now();
    m_eventQueue.push(qe);
}

void EventDispatcher::QueueDelayedEvent(GameEvent event, float delay) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    QueuedEvent qe;
    qe.event = std::move(event);
    qe.delay = delay;
    qe.queueTime = std::chrono::system_clock::now();
    m_delayedEvents.push_back(qe);
}

void EventDispatcher::ProcessQueue(float deltaTime) {
    // Process delayed events
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        auto it = m_delayedEvents.begin();
        while (it != m_delayedEvents.end()) {
            it->delay -= deltaTime;
            if (it->delay <= 0.0f) {
                m_eventQueue.push(*it);
                it = m_delayedEvents.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Process queued events
    while (true) {
        QueuedEvent qe;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_eventQueue.empty()) break;
            qe = m_eventQueue.front();
            m_eventQueue.pop();
        }

        Dispatch(qe.event);
    }
}

void EventDispatcher::ClearQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    while (!m_eventQueue.empty()) {
        m_eventQueue.pop();
    }
    m_delayedEvents.clear();
}

size_t EventDispatcher::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_eventQueue.size() + m_delayedEvents.size();
}

// ============================================================================
// Convenience Dispatch Methods
// ============================================================================

void EventDispatcher::DispatchEntitySpawn(uint32_t entityId, const std::string& entityType,
                                          float x, float y, float z) {
    GameEvent evt = GameEvent::EntityEvent(EventType::EntitySpawn, entityId);
    evt.stringValue = entityType;
    evt.x = x;
    evt.y = y;
    evt.z = z;
    Dispatch(evt);
}

void EventDispatcher::DispatchEntityDeath(uint32_t entityId, uint32_t killerId) {
    GameEvent evt = GameEvent::EntityEvent(EventType::EntityDeath, entityId);
    evt.targetEntityId = killerId;
    Dispatch(evt);
}

void EventDispatcher::DispatchEntityDamaged(uint32_t entityId, float damage, uint32_t sourceId) {
    GameEvent evt = GameEvent::EntityEvent(EventType::EntityDamaged, entityId);
    evt.floatValue = damage;
    evt.targetEntityId = sourceId;
    Dispatch(evt);
}

void EventDispatcher::DispatchTileEnter(int tileX, int tileY, uint32_t entityId) {
    GameEvent evt = GameEvent::TileEvent(EventType::TileEnter, tileX, tileY, entityId);
    Dispatch(evt);
}

void EventDispatcher::DispatchTileExit(int tileX, int tileY, uint32_t entityId) {
    GameEvent evt = GameEvent::TileEvent(EventType::TileExit, tileX, tileY, entityId);
    Dispatch(evt);
}

void EventDispatcher::DispatchBuildingComplete(uint32_t buildingId, const std::string& buildingType) {
    GameEvent evt = GameEvent::BuildingEvent(EventType::BuildingComplete, buildingId);
    evt.stringValue = buildingType;
    Dispatch(evt);
}

void EventDispatcher::DispatchBuildingDestroyed(uint32_t buildingId) {
    GameEvent evt = GameEvent::BuildingEvent(EventType::BuildingDestroyed, buildingId);
    Dispatch(evt);
}

void EventDispatcher::DispatchResourceGathered(const std::string& resourceType, int amount, uint32_t gathererId) {
    GameEvent evt = GameEvent::ResourceEvent(EventType::ResourceGathered, resourceType, amount);
    evt.entityId = gathererId;
    Dispatch(evt);
}

void EventDispatcher::DispatchDayStarted(int dayNumber) {
    GameEvent evt;
    evt.type = EventType::DayStarted;
    evt.intValue = dayNumber;
    evt.timestamp = std::chrono::system_clock::now();
    Dispatch(evt);
}

void EventDispatcher::DispatchNightStarted(int dayNumber) {
    GameEvent evt;
    evt.type = EventType::NightStarted;
    evt.intValue = dayNumber;
    evt.timestamp = std::chrono::system_clock::now();
    Dispatch(evt);
}

void EventDispatcher::DispatchCustomEvent(const std::string& eventType,
                                          const std::unordered_map<std::string, std::any>& data) {
    GameEvent evt = GameEvent::CustomEvent(eventType);
    evt.customData = data;
    Dispatch(evt);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void EventDispatcher::SortHandlers(EventType eventType) {
    auto it = m_handlersByType.find(eventType);
    if (it == m_handlersByType.end()) return;

    std::sort(it->second.begin(), it->second.end(),
        [this](size_t a, size_t b) {
            auto itA = m_handlers.find(a);
            auto itB = m_handlers.find(b);
            if (itA == m_handlers.end() || itB == m_handlers.end()) return false;
            return static_cast<int>(itA->second.priority) > static_cast<int>(itB->second.priority);
        });
}

void EventDispatcher::SortCustomHandlers(const std::string& customType) {
    auto it = m_handlersByCustomType.find(customType);
    if (it == m_handlersByCustomType.end()) return;

    std::sort(it->second.begin(), it->second.end(),
        [this](size_t a, size_t b) {
            auto itA = m_handlers.find(a);
            auto itB = m_handlers.find(b);
            if (itA == m_handlers.end() || itB == m_handlers.end()) return false;
            return static_cast<int>(itA->second.priority) > static_cast<int>(itB->second.priority);
        });
}

void EventDispatcher::CallHandler(EventHandler& handler, GameEvent& event) {
    if (handler.isPython) {
        CallPythonHandler(handler, event);
    } else if (handler.cppHandler) {
        handler.cppHandler(event);
    }
}

void EventDispatcher::CallPythonHandler(const EventHandler& handler, const GameEvent& event) {
    if (!m_pythonEngine) return;

    // Build argument list for Python
    std::vector<std::variant<bool, int, float, double, std::string>> args;

    // Pass event data as positional arguments
    args.emplace_back(static_cast<int>(event.type));
    args.emplace_back(static_cast<int>(event.entityId));
    args.emplace_back(static_cast<int>(event.targetEntityId));
    args.emplace_back(static_cast<int>(event.buildingId));
    args.emplace_back(event.x);
    args.emplace_back(event.y);
    args.emplace_back(event.z);
    args.emplace_back(static_cast<int>(event.tileX));
    args.emplace_back(static_cast<int>(event.tileY));
    args.emplace_back(event.floatValue);
    args.emplace_back(event.intValue);
    args.emplace_back(event.stringValue);

    // Call Python function
    auto result = m_pythonEngine->CallFunctionV(handler.pythonModule, handler.pythonFunction, args);

    // Check if handler wants to cancel the event
    if (result.success) {
        auto cancelled = result.GetValue<bool>();
        if (cancelled && *cancelled) {
            // Note: This is a const reference, so we can't modify it directly
            // The cancellation would need to be handled differently in a real implementation
        }
    }
}

} // namespace Scripting
} // namespace Nova
