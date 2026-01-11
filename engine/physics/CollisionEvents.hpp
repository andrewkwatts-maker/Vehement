#pragma once

#include "CollisionBody.hpp"
#include "CollisionShape.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>

namespace Nova {

// Forward declarations
class PhysicsWorld;
class CollisionEventDispatcher;

/**
 * @brief Types of collision events
 */
enum class CollisionEventType : uint8_t {
    CollisionEnter,  ///< First frame of collision contact
    CollisionStay,   ///< Ongoing collision contact
    CollisionExit,   ///< Last frame of collision contact
    TriggerEnter,    ///< First frame of trigger overlap
    TriggerStay,     ///< Ongoing trigger overlap
    TriggerExit      ///< Last frame of trigger overlap
};

/**
 * @brief Convert collision event type to string for debugging
 */
[[nodiscard]] constexpr const char* CollisionEventTypeToString(CollisionEventType type) noexcept {
    switch (type) {
        case CollisionEventType::CollisionEnter: return "CollisionEnter";
        case CollisionEventType::CollisionStay:  return "CollisionStay";
        case CollisionEventType::CollisionExit:  return "CollisionExit";
        case CollisionEventType::TriggerEnter:   return "TriggerEnter";
        case CollisionEventType::TriggerStay:    return "TriggerStay";
        case CollisionEventType::TriggerExit:    return "TriggerExit";
        default:                                  return "Unknown";
    }
}

/**
 * @brief Detailed contact point information
 */
struct CollisionContact {
    glm::vec3 point{0.0f};           ///< World space contact point
    glm::vec3 normal{0.0f};          ///< Contact normal (from body A to body B)
    float penetrationDepth = 0.0f;   ///< Penetration depth at this contact
    float impulse = 0.0f;            ///< Applied impulse magnitude at this contact
    int shapeIndexA = -1;            ///< Shape index on body A
    int shapeIndexB = -1;            ///< Shape index on body B
};

/**
 * @brief Complete collision event data
 *
 * Contains all information about a collision between two bodies,
 * including contact points, relative velocities, and applied impulses.
 */
struct CollisionEvent {
    CollisionEventType type = CollisionEventType::CollisionEnter;

    // Bodies involved
    CollisionBody* bodyA = nullptr;
    CollisionBody* bodyB = nullptr;

    // Contact information
    std::vector<CollisionContact> contacts;

    // Physics data
    glm::vec3 relativeVelocity{0.0f};  ///< Velocity of B relative to A at contact
    float totalImpulse = 0.0f;          ///< Total impulse applied this frame
    float separationSpeed = 0.0f;       ///< Speed at which bodies are separating

    // Filtering information
    uint32_t layerA = 0;                ///< Collision layer of body A
    uint32_t layerB = 0;                ///< Collision layer of body B
    std::string tagA;                   ///< Tag/identifier of body A
    std::string tagB;                   ///< Tag/identifier of body B

    // Timestamp
    float timestamp = 0.0f;             ///< Physics time when event occurred

    /**
     * @brief Check if this collision involves a specific body
     */
    [[nodiscard]] bool Involves(const CollisionBody* body) const {
        return bodyA == body || bodyB == body;
    }

    /**
     * @brief Check if this collision involves a body with a specific ID
     */
    [[nodiscard]] bool InvolvesId(CollisionBody::BodyId id) const {
        return (bodyA && bodyA->GetId() == id) || (bodyB && bodyB->GetId() == id);
    }

    /**
     * @brief Get the other body in the collision given one body
     */
    [[nodiscard]] CollisionBody* GetOtherBody(const CollisionBody* body) const {
        if (bodyA == body) return bodyB;
        if (bodyB == body) return bodyA;
        return nullptr;
    }

    /**
     * @brief Check if either body has a specific layer
     */
    [[nodiscard]] bool HasLayer(uint32_t layer) const {
        return (layerA & layer) != 0 || (layerB & layer) != 0;
    }

    /**
     * @brief Check if either body has a specific tag
     */
    [[nodiscard]] bool HasTag(const std::string& tag) const {
        return tagA == tag || tagB == tag;
    }

    /**
     * @brief Get the average contact point
     */
    [[nodiscard]] glm::vec3 GetAverageContactPoint() const {
        if (contacts.empty()) {
            return (bodyA && bodyB) ?
                (bodyA->GetPosition() + bodyB->GetPosition()) * 0.5f :
                glm::vec3(0.0f);
        }

        glm::vec3 sum{0.0f};
        for (const auto& c : contacts) {
            sum += c.point;
        }
        return sum / static_cast<float>(contacts.size());
    }

    /**
     * @brief Get the average contact normal
     */
    [[nodiscard]] glm::vec3 GetAverageNormal() const {
        if (contacts.empty()) return glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 sum{0.0f};
        for (const auto& c : contacts) {
            sum += c.normal;
        }
        float len = glm::length(sum);
        return len > 0.0001f ? sum / len : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    /**
     * @brief Check if this is a collision event (not trigger)
     */
    [[nodiscard]] bool IsCollision() const {
        return type == CollisionEventType::CollisionEnter ||
               type == CollisionEventType::CollisionStay ||
               type == CollisionEventType::CollisionExit;
    }

    /**
     * @brief Check if this is a trigger event
     */
    [[nodiscard]] bool IsTrigger() const {
        return type == CollisionEventType::TriggerEnter ||
               type == CollisionEventType::TriggerStay ||
               type == CollisionEventType::TriggerExit;
    }

    /**
     * @brief Check if this is an enter event (collision or trigger)
     */
    [[nodiscard]] bool IsEnter() const {
        return type == CollisionEventType::CollisionEnter ||
               type == CollisionEventType::TriggerEnter;
    }

    /**
     * @brief Check if this is an exit event (collision or trigger)
     */
    [[nodiscard]] bool IsExit() const {
        return type == CollisionEventType::CollisionExit ||
               type == CollisionEventType::TriggerExit;
    }
};

/**
 * @brief Filter configuration for collision event listeners
 */
struct CollisionEventFilter {
    // Event type filtering
    bool collisionEnter = true;
    bool collisionStay = false;   // Default off - can be very frequent
    bool collisionExit = true;
    bool triggerEnter = true;
    bool triggerStay = false;     // Default off - can be very frequent
    bool triggerExit = true;

    // Layer filtering
    uint32_t layerMask = CollisionLayer::All;

    // Tag filtering
    std::vector<std::string> tags;  // Empty = accept all tags

    // Body filtering
    std::unordered_set<CollisionBody::BodyId> specificBodies;  // Empty = accept all

    /**
     * @brief Check if an event passes this filter
     */
    [[nodiscard]] bool Accepts(const CollisionEvent& event) const {
        // Check event type
        switch (event.type) {
            case CollisionEventType::CollisionEnter: if (!collisionEnter) return false; break;
            case CollisionEventType::CollisionStay:  if (!collisionStay) return false; break;
            case CollisionEventType::CollisionExit:  if (!collisionExit) return false; break;
            case CollisionEventType::TriggerEnter:   if (!triggerEnter) return false; break;
            case CollisionEventType::TriggerStay:    if (!triggerStay) return false; break;
            case CollisionEventType::TriggerExit:    if (!triggerExit) return false; break;
        }

        // Check layers
        if (layerMask != CollisionLayer::All) {
            if ((event.layerA & layerMask) == 0 && (event.layerB & layerMask) == 0) {
                return false;
            }
        }

        // Check tags
        if (!tags.empty()) {
            bool hasTag = false;
            for (const auto& tag : tags) {
                if (event.tagA == tag || event.tagB == tag) {
                    hasTag = true;
                    break;
                }
            }
            if (!hasTag) return false;
        }

        // Check specific bodies
        if (!specificBodies.empty()) {
            bool hasBody = false;
            if (event.bodyA && specificBodies.count(event.bodyA->GetId()) > 0) hasBody = true;
            if (event.bodyB && specificBodies.count(event.bodyB->GetId()) > 0) hasBody = true;
            if (!hasBody) return false;
        }

        return true;
    }

    /**
     * @brief Create filter that accepts all events
     */
    [[nodiscard]] static CollisionEventFilter All() {
        CollisionEventFilter filter;
        filter.collisionStay = true;
        filter.triggerStay = true;
        return filter;
    }

    /**
     * @brief Create filter for enter/exit events only
     */
    [[nodiscard]] static CollisionEventFilter EnterExitOnly() {
        return CollisionEventFilter{};  // Default is enter/exit only
    }

    /**
     * @brief Create filter for specific layers
     */
    [[nodiscard]] static CollisionEventFilter ForLayers(uint32_t mask) {
        CollisionEventFilter filter;
        filter.layerMask = mask;
        return filter;
    }

    /**
     * @brief Create filter for specific body
     */
    [[nodiscard]] static CollisionEventFilter ForBody(CollisionBody::BodyId bodyId) {
        CollisionEventFilter filter;
        filter.specificBodies.insert(bodyId);
        return filter;
    }
};

/**
 * @brief Collision event callback function type
 */
using CollisionEventCallback = std::function<void(const CollisionEvent&)>;

/**
 * @brief Interface for collision event listeners
 *
 * Inherit from this class to receive collision events through
 * virtual method dispatch. Alternative to callback-based approach.
 */
class ICollisionListener {
public:
    virtual ~ICollisionListener() = default;

    /**
     * @brief Called when a collision first occurs
     * @param event Collision event data
     */
    virtual void OnCollisionEnter(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Called every frame while collision persists
     * @param event Collision event data
     */
    virtual void OnCollisionStay(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Called when a collision ends
     * @param event Collision event data
     */
    virtual void OnCollisionExit(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Called when entering a trigger volume
     * @param event Trigger event data
     */
    virtual void OnTriggerEnter(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Called every frame while inside trigger volume
     * @param event Trigger event data
     */
    virtual void OnTriggerStay(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Called when exiting a trigger volume
     * @param event Trigger event data
     */
    virtual void OnTriggerExit(const CollisionEvent& event) { (void)event; }

    /**
     * @brief Get the filter for this listener
     * @return Event filter configuration
     */
    [[nodiscard]] virtual CollisionEventFilter GetFilter() const {
        return CollisionEventFilter{};
    }
};

/**
 * @brief Handle for registered collision event listeners
 */
using CollisionListenerId = uint64_t;

/**
 * @brief Collision event dispatcher
 *
 * Central hub for collision event distribution. Supports both callback-based
 * and interface-based listener registration with filtering support.
 *
 * Features:
 * - Global listeners receive all events (filtered)
 * - Per-body listeners receive events for specific bodies
 * - Callback and interface-based listening
 * - Event queuing with deferred dispatch
 * - Thread-safe event queuing (dispatch on main thread)
 */
class CollisionEventDispatcher {
public:
    CollisionEventDispatcher();
    ~CollisionEventDispatcher();

    // Non-copyable
    CollisionEventDispatcher(const CollisionEventDispatcher&) = delete;
    CollisionEventDispatcher& operator=(const CollisionEventDispatcher&) = delete;

    // =========================================================================
    // Global Listener Registration
    // =========================================================================

    /**
     * @brief Register a global callback listener
     * @param callback Function to call for matching events
     * @param filter Event filter configuration
     * @return Listener ID for unregistration
     */
    [[nodiscard]] CollisionListenerId RegisterGlobalListener(
        CollisionEventCallback callback,
        const CollisionEventFilter& filter = CollisionEventFilter{});

    /**
     * @brief Register a global interface listener
     * @param listener Pointer to listener object (must remain valid)
     * @return Listener ID for unregistration
     */
    [[nodiscard]] CollisionListenerId RegisterGlobalListener(ICollisionListener* listener);

    /**
     * @brief Unregister a global listener by ID
     */
    void UnregisterGlobalListener(CollisionListenerId id);

    // =========================================================================
    // Per-Body Listener Registration
    // =========================================================================

    /**
     * @brief Register a callback listener for a specific body
     * @param bodyId Body to listen for
     * @param callback Function to call for events involving this body
     * @param filter Additional event filtering
     * @return Listener ID for unregistration
     */
    [[nodiscard]] CollisionListenerId RegisterBodyListener(
        CollisionBody::BodyId bodyId,
        CollisionEventCallback callback,
        const CollisionEventFilter& filter = CollisionEventFilter{});

    /**
     * @brief Register an interface listener for a specific body
     * @param bodyId Body to listen for
     * @param listener Pointer to listener object
     * @return Listener ID for unregistration
     */
    [[nodiscard]] CollisionListenerId RegisterBodyListener(
        CollisionBody::BodyId bodyId,
        ICollisionListener* listener);

    /**
     * @brief Unregister a per-body listener
     */
    void UnregisterBodyListener(CollisionListenerId id);

    /**
     * @brief Remove all listeners for a specific body
     */
    void RemoveAllBodyListeners(CollisionBody::BodyId bodyId);

    // =========================================================================
    // Event Dispatch
    // =========================================================================

    /**
     * @brief Immediately dispatch an event to all matching listeners
     * @param event Event to dispatch
     */
    void DispatchEvent(const CollisionEvent& event);

    /**
     * @brief Queue an event for later dispatch
     * @param event Event to queue
     */
    void QueueEvent(const CollisionEvent& event);

    /**
     * @brief Dispatch all queued events
     * @note Call from main thread only
     */
    void FlushEventQueue();

    /**
     * @brief Clear event queue without dispatching
     */
    void ClearEventQueue();

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Dispatch a collision enter event
     */
    void DispatchCollisionEnter(CollisionBody* bodyA, CollisionBody* bodyB,
                                 const std::vector<CollisionContact>& contacts,
                                 float timestamp = 0.0f);

    /**
     * @brief Dispatch a collision stay event
     */
    void DispatchCollisionStay(CollisionBody* bodyA, CollisionBody* bodyB,
                                const std::vector<CollisionContact>& contacts,
                                float timestamp = 0.0f);

    /**
     * @brief Dispatch a collision exit event
     */
    void DispatchCollisionExit(CollisionBody* bodyA, CollisionBody* bodyB,
                                float timestamp = 0.0f);

    /**
     * @brief Dispatch a trigger enter event
     */
    void DispatchTriggerEnter(CollisionBody* bodyA, CollisionBody* bodyB,
                               const glm::vec3& overlapPoint = glm::vec3(0.0f),
                               float timestamp = 0.0f);

    /**
     * @brief Dispatch a trigger stay event
     */
    void DispatchTriggerStay(CollisionBody* bodyA, CollisionBody* bodyB,
                              float timestamp = 0.0f);

    /**
     * @brief Dispatch a trigger exit event
     */
    void DispatchTriggerExit(CollisionBody* bodyA, CollisionBody* bodyB,
                              float timestamp = 0.0f);

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        size_t globalListenerCount = 0;
        size_t bodyListenerCount = 0;
        size_t eventsDispatched = 0;
        size_t eventsQueued = 0;
        size_t eventsFlushed = 0;
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }
    void ResetStats();

    // =========================================================================
    // Cleanup
    // =========================================================================

    /**
     * @brief Remove all listeners
     */
    void Clear();

private:
    struct GlobalCallbackListener {
        CollisionListenerId id;
        CollisionEventCallback callback;
        CollisionEventFilter filter;
    };

    struct GlobalInterfaceListener {
        CollisionListenerId id;
        ICollisionListener* listener;
    };

    struct BodyCallbackListener {
        CollisionListenerId id;
        CollisionBody::BodyId bodyId;
        CollisionEventCallback callback;
        CollisionEventFilter filter;
    };

    struct BodyInterfaceListener {
        CollisionListenerId id;
        CollisionBody::BodyId bodyId;
        ICollisionListener* listener;
    };

    void DispatchToListener(ICollisionListener* listener, const CollisionEvent& event);
    CollisionEvent BuildEvent(CollisionEventType type,
                              CollisionBody* bodyA,
                              CollisionBody* bodyB,
                              float timestamp);

    // Listener storage
    std::vector<GlobalCallbackListener> m_globalCallbackListeners;
    std::vector<GlobalInterfaceListener> m_globalInterfaceListeners;
    std::vector<BodyCallbackListener> m_bodyCallbackListeners;
    std::vector<BodyInterfaceListener> m_bodyInterfaceListeners;

    // Body lookup for fast per-body dispatch
    std::unordered_map<CollisionBody::BodyId, std::vector<CollisionListenerId>> m_bodyListenerMap;

    // Event queue
    std::vector<CollisionEvent> m_eventQueue;

    // ID generation
    CollisionListenerId m_nextListenerId = 1;

    // Statistics
    Stats m_stats;
};

/**
 * @brief RAII helper for auto-unregistering collision listeners
 */
class ScopedCollisionListener {
public:
    ScopedCollisionListener() = default;

    ScopedCollisionListener(CollisionEventDispatcher& dispatcher, CollisionListenerId id, bool isBodyListener = false)
        : m_dispatcher(&dispatcher), m_id(id), m_isBodyListener(isBodyListener) {}

    ~ScopedCollisionListener() {
        Unregister();
    }

    // Move only
    ScopedCollisionListener(const ScopedCollisionListener&) = delete;
    ScopedCollisionListener& operator=(const ScopedCollisionListener&) = delete;

    ScopedCollisionListener(ScopedCollisionListener&& other) noexcept
        : m_dispatcher(other.m_dispatcher)
        , m_id(other.m_id)
        , m_isBodyListener(other.m_isBodyListener) {
        other.m_dispatcher = nullptr;
        other.m_id = 0;
    }

    ScopedCollisionListener& operator=(ScopedCollisionListener&& other) noexcept {
        if (this != &other) {
            Unregister();
            m_dispatcher = other.m_dispatcher;
            m_id = other.m_id;
            m_isBodyListener = other.m_isBodyListener;
            other.m_dispatcher = nullptr;
            other.m_id = 0;
        }
        return *this;
    }

    void Unregister() {
        if (m_dispatcher && m_id != 0) {
            if (m_isBodyListener) {
                m_dispatcher->UnregisterBodyListener(m_id);
            } else {
                m_dispatcher->UnregisterGlobalListener(m_id);
            }
            m_dispatcher = nullptr;
            m_id = 0;
        }
    }

    [[nodiscard]] CollisionListenerId GetId() const { return m_id; }
    [[nodiscard]] bool IsValid() const { return m_dispatcher != nullptr && m_id != 0; }

private:
    CollisionEventDispatcher* m_dispatcher = nullptr;
    CollisionListenerId m_id = 0;
    bool m_isBodyListener = false;
};

/**
 * @brief Helper class for building collision event filters
 */
class CollisionEventFilterBuilder {
public:
    CollisionEventFilterBuilder() = default;

    CollisionEventFilterBuilder& AcceptCollisionEnter(bool accept = true) {
        m_filter.collisionEnter = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptCollisionStay(bool accept = true) {
        m_filter.collisionStay = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptCollisionExit(bool accept = true) {
        m_filter.collisionExit = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptTriggerEnter(bool accept = true) {
        m_filter.triggerEnter = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptTriggerStay(bool accept = true) {
        m_filter.triggerStay = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptTriggerExit(bool accept = true) {
        m_filter.triggerExit = accept;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptAllCollisions() {
        m_filter.collisionEnter = true;
        m_filter.collisionStay = true;
        m_filter.collisionExit = true;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptAllTriggers() {
        m_filter.triggerEnter = true;
        m_filter.triggerStay = true;
        m_filter.triggerExit = true;
        return *this;
    }

    CollisionEventFilterBuilder& AcceptAll() {
        return AcceptAllCollisions().AcceptAllTriggers();
    }

    CollisionEventFilterBuilder& WithLayerMask(uint32_t mask) {
        m_filter.layerMask = mask;
        return *this;
    }

    CollisionEventFilterBuilder& WithLayers(std::initializer_list<uint32_t> layers) {
        m_filter.layerMask = 0;
        for (uint32_t layer : layers) {
            m_filter.layerMask |= layer;
        }
        return *this;
    }

    CollisionEventFilterBuilder& WithTag(const std::string& tag) {
        m_filter.tags.push_back(tag);
        return *this;
    }

    CollisionEventFilterBuilder& WithTags(std::initializer_list<std::string> tags) {
        m_filter.tags.insert(m_filter.tags.end(), tags.begin(), tags.end());
        return *this;
    }

    CollisionEventFilterBuilder& ForBody(CollisionBody::BodyId bodyId) {
        m_filter.specificBodies.insert(bodyId);
        return *this;
    }

    [[nodiscard]] CollisionEventFilter Build() const { return m_filter; }
    [[nodiscard]] operator CollisionEventFilter() const { return m_filter; }

private:
    CollisionEventFilter m_filter;
};

} // namespace Nova
