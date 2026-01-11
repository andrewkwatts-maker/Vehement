#include "CollisionEvents.hpp"
#include <algorithm>
#include <cassert>

namespace Nova {

// ============================================================================
// CollisionEventDispatcher
// ============================================================================

CollisionEventDispatcher::CollisionEventDispatcher() = default;

CollisionEventDispatcher::~CollisionEventDispatcher() {
    Clear();
}

// ============================================================================
// Global Listener Registration
// ============================================================================

CollisionListenerId CollisionEventDispatcher::RegisterGlobalListener(
    CollisionEventCallback callback,
    const CollisionEventFilter& filter)
{
    assert(callback && "Callback must be valid");

    CollisionListenerId id = m_nextListenerId++;
    m_globalCallbackListeners.push_back({id, std::move(callback), filter});
    ++m_stats.globalListenerCount;
    return id;
}

CollisionListenerId CollisionEventDispatcher::RegisterGlobalListener(ICollisionListener* listener) {
    assert(listener && "Listener must be valid");

    CollisionListenerId id = m_nextListenerId++;
    m_globalInterfaceListeners.push_back({id, listener});
    ++m_stats.globalListenerCount;
    return id;
}

void CollisionEventDispatcher::UnregisterGlobalListener(CollisionListenerId id) {
    // Check callback listeners
    auto callbackIt = std::find_if(
        m_globalCallbackListeners.begin(),
        m_globalCallbackListeners.end(),
        [id](const GlobalCallbackListener& l) { return l.id == id; }
    );

    if (callbackIt != m_globalCallbackListeners.end()) {
        m_globalCallbackListeners.erase(callbackIt);
        --m_stats.globalListenerCount;
        return;
    }

    // Check interface listeners
    auto interfaceIt = std::find_if(
        m_globalInterfaceListeners.begin(),
        m_globalInterfaceListeners.end(),
        [id](const GlobalInterfaceListener& l) { return l.id == id; }
    );

    if (interfaceIt != m_globalInterfaceListeners.end()) {
        m_globalInterfaceListeners.erase(interfaceIt);
        --m_stats.globalListenerCount;
    }
}

// ============================================================================
// Per-Body Listener Registration
// ============================================================================

CollisionListenerId CollisionEventDispatcher::RegisterBodyListener(
    CollisionBody::BodyId bodyId,
    CollisionEventCallback callback,
    const CollisionEventFilter& filter)
{
    assert(callback && "Callback must be valid");

    CollisionListenerId id = m_nextListenerId++;
    m_bodyCallbackListeners.push_back({id, bodyId, std::move(callback), filter});
    m_bodyListenerMap[bodyId].push_back(id);
    ++m_stats.bodyListenerCount;
    return id;
}

CollisionListenerId CollisionEventDispatcher::RegisterBodyListener(
    CollisionBody::BodyId bodyId,
    ICollisionListener* listener)
{
    assert(listener && "Listener must be valid");

    CollisionListenerId id = m_nextListenerId++;
    m_bodyInterfaceListeners.push_back({id, bodyId, listener});
    m_bodyListenerMap[bodyId].push_back(id);
    ++m_stats.bodyListenerCount;
    return id;
}

void CollisionEventDispatcher::UnregisterBodyListener(CollisionListenerId id) {
    // Check callback listeners
    auto callbackIt = std::find_if(
        m_bodyCallbackListeners.begin(),
        m_bodyCallbackListeners.end(),
        [id](const BodyCallbackListener& l) { return l.id == id; }
    );

    if (callbackIt != m_bodyCallbackListeners.end()) {
        // Remove from body lookup map
        auto& bodyIds = m_bodyListenerMap[callbackIt->bodyId];
        bodyIds.erase(std::remove(bodyIds.begin(), bodyIds.end(), id), bodyIds.end());
        if (bodyIds.empty()) {
            m_bodyListenerMap.erase(callbackIt->bodyId);
        }

        m_bodyCallbackListeners.erase(callbackIt);
        --m_stats.bodyListenerCount;
        return;
    }

    // Check interface listeners
    auto interfaceIt = std::find_if(
        m_bodyInterfaceListeners.begin(),
        m_bodyInterfaceListeners.end(),
        [id](const BodyInterfaceListener& l) { return l.id == id; }
    );

    if (interfaceIt != m_bodyInterfaceListeners.end()) {
        // Remove from body lookup map
        auto& bodyIds = m_bodyListenerMap[interfaceIt->bodyId];
        bodyIds.erase(std::remove(bodyIds.begin(), bodyIds.end(), id), bodyIds.end());
        if (bodyIds.empty()) {
            m_bodyListenerMap.erase(interfaceIt->bodyId);
        }

        m_bodyInterfaceListeners.erase(interfaceIt);
        --m_stats.bodyListenerCount;
    }
}

void CollisionEventDispatcher::RemoveAllBodyListeners(CollisionBody::BodyId bodyId) {
    auto it = m_bodyListenerMap.find(bodyId);
    if (it == m_bodyListenerMap.end()) {
        return;
    }

    // Make a copy since we'll be modifying the collection
    std::vector<CollisionListenerId> idsToRemove = it->second;

    for (CollisionListenerId id : idsToRemove) {
        UnregisterBodyListener(id);
    }
}

// ============================================================================
// Event Dispatch
// ============================================================================

void CollisionEventDispatcher::DispatchEvent(const CollisionEvent& event) {
    ++m_stats.eventsDispatched;

    // Dispatch to global callback listeners
    for (const auto& listener : m_globalCallbackListeners) {
        if (listener.filter.Accepts(event)) {
            listener.callback(event);
        }
    }

    // Dispatch to global interface listeners
    for (const auto& listener : m_globalInterfaceListeners) {
        if (listener.listener->GetFilter().Accepts(event)) {
            DispatchToListener(listener.listener, event);
        }
    }

    // Dispatch to per-body callback listeners
    for (const auto& listener : m_bodyCallbackListeners) {
        if (event.InvolvesId(listener.bodyId) && listener.filter.Accepts(event)) {
            listener.callback(event);
        }
    }

    // Dispatch to per-body interface listeners
    for (const auto& listener : m_bodyInterfaceListeners) {
        if (event.InvolvesId(listener.bodyId)) {
            if (listener.listener->GetFilter().Accepts(event)) {
                DispatchToListener(listener.listener, event);
            }
        }
    }
}

void CollisionEventDispatcher::QueueEvent(const CollisionEvent& event) {
    m_eventQueue.push_back(event);
    ++m_stats.eventsQueued;
}

void CollisionEventDispatcher::FlushEventQueue() {
    // Move queue to local to allow requeuing during dispatch
    std::vector<CollisionEvent> eventsToDispatch = std::move(m_eventQueue);
    m_eventQueue.clear();

    for (const auto& event : eventsToDispatch) {
        DispatchEvent(event);
        ++m_stats.eventsFlushed;
    }
}

void CollisionEventDispatcher::ClearEventQueue() {
    m_eventQueue.clear();
}

// ============================================================================
// Convenience Dispatch Methods
// ============================================================================

void CollisionEventDispatcher::DispatchCollisionEnter(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    const std::vector<CollisionContact>& contacts,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::CollisionEnter, bodyA, bodyB, timestamp);
    event.contacts = contacts;

    // Calculate total impulse
    float totalImpulse = 0.0f;
    for (const auto& contact : contacts) {
        totalImpulse += contact.impulse;
    }
    event.totalImpulse = totalImpulse;

    // Calculate relative velocity
    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();
    }

    DispatchEvent(event);
}

void CollisionEventDispatcher::DispatchCollisionStay(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    const std::vector<CollisionContact>& contacts,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::CollisionStay, bodyA, bodyB, timestamp);
    event.contacts = contacts;

    float totalImpulse = 0.0f;
    for (const auto& contact : contacts) {
        totalImpulse += contact.impulse;
    }
    event.totalImpulse = totalImpulse;

    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();
    }

    DispatchEvent(event);
}

void CollisionEventDispatcher::DispatchCollisionExit(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::CollisionExit, bodyA, bodyB, timestamp);

    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();

        // Calculate separation speed
        glm::vec3 separationDir = glm::normalize(bodyB->GetPosition() - bodyA->GetPosition());
        if (glm::length(separationDir) > 0.001f) {
            event.separationSpeed = glm::dot(event.relativeVelocity, separationDir);
        }
    }

    DispatchEvent(event);
}

void CollisionEventDispatcher::DispatchTriggerEnter(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    const glm::vec3& overlapPoint,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::TriggerEnter, bodyA, bodyB, timestamp);

    // Add a single contact at the overlap point
    if (overlapPoint != glm::vec3(0.0f)) {
        CollisionContact contact;
        contact.point = overlapPoint;
        if (bodyA && bodyB) {
            contact.normal = glm::normalize(bodyB->GetPosition() - bodyA->GetPosition());
        }
        event.contacts.push_back(contact);
    }

    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();
    }

    DispatchEvent(event);
}

void CollisionEventDispatcher::DispatchTriggerStay(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::TriggerStay, bodyA, bodyB, timestamp);

    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();
    }

    DispatchEvent(event);
}

void CollisionEventDispatcher::DispatchTriggerExit(
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    float timestamp)
{
    CollisionEvent event = BuildEvent(CollisionEventType::TriggerExit, bodyA, bodyB, timestamp);

    if (bodyA && bodyB) {
        event.relativeVelocity = bodyB->GetLinearVelocity() - bodyA->GetLinearVelocity();
    }

    DispatchEvent(event);
}

// ============================================================================
// Statistics
// ============================================================================

void CollisionEventDispatcher::ResetStats() {
    m_stats.eventsDispatched = 0;
    m_stats.eventsQueued = 0;
    m_stats.eventsFlushed = 0;
    // Keep listener counts as-is
}

// ============================================================================
// Cleanup
// ============================================================================

void CollisionEventDispatcher::Clear() {
    m_globalCallbackListeners.clear();
    m_globalInterfaceListeners.clear();
    m_bodyCallbackListeners.clear();
    m_bodyInterfaceListeners.clear();
    m_bodyListenerMap.clear();
    m_eventQueue.clear();
    m_stats = Stats{};
}

// ============================================================================
// Private Helpers
// ============================================================================

void CollisionEventDispatcher::DispatchToListener(ICollisionListener* listener, const CollisionEvent& event) {
    switch (event.type) {
        case CollisionEventType::CollisionEnter:
            listener->OnCollisionEnter(event);
            break;
        case CollisionEventType::CollisionStay:
            listener->OnCollisionStay(event);
            break;
        case CollisionEventType::CollisionExit:
            listener->OnCollisionExit(event);
            break;
        case CollisionEventType::TriggerEnter:
            listener->OnTriggerEnter(event);
            break;
        case CollisionEventType::TriggerStay:
            listener->OnTriggerStay(event);
            break;
        case CollisionEventType::TriggerExit:
            listener->OnTriggerExit(event);
            break;
    }
}

CollisionEvent CollisionEventDispatcher::BuildEvent(
    CollisionEventType type,
    CollisionBody* bodyA,
    CollisionBody* bodyB,
    float timestamp)
{
    CollisionEvent event;
    event.type = type;
    event.bodyA = bodyA;
    event.bodyB = bodyB;
    event.timestamp = timestamp;

    if (bodyA) {
        event.layerA = bodyA->GetCollisionLayer();
        // Tag could be set via user data or a tag component
        // For now, leaving it empty
    }

    if (bodyB) {
        event.layerB = bodyB->GetCollisionLayer();
    }

    return event;
}

} // namespace Nova
