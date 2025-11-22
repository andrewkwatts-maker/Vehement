#pragma once

#include "CollisionShape.hpp"
#include "CollisionBody.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <string>
#include <optional>

namespace Nova {

// Forward declarations
class PhysicsWorld;

/**
 * @brief Event data passed to trigger callbacks
 */
struct TriggerEvent {
    class TriggerVolume* trigger = nullptr;
    CollisionBody* otherBody = nullptr;
    std::string eventName;
    glm::vec3 contactPoint{0.0f};

    // User data for event context
    void* userData = nullptr;
};

/**
 * @brief Trigger callback types
 */
using TriggerEnterCallback = std::function<void(const TriggerEvent&)>;
using TriggerStayCallback = std::function<void(const TriggerEvent&, float deltaTime)>;
using TriggerExitCallback = std::function<void(const TriggerEvent&)>;

/**
 * @brief Python event binding for triggers
 *
 * Allows triggers to fire events that can be handled by Python scripts.
 */
struct PythonEventBinding {
    std::string moduleName;
    std::string functionName;
    std::string eventName;

    [[nodiscard]] bool IsValid() const {
        return !moduleName.empty() && !functionName.empty();
    }
};

/**
 * @brief Trigger volume - non-physical collision detection
 *
 * Trigger volumes detect when other collision bodies enter, stay in, or exit
 * the volume without causing physical collision response. Useful for:
 * - Area effects (damage zones, heal zones)
 * - Gameplay triggers (checkpoints, spawn points)
 * - Detection zones (enemy awareness, stealth)
 * - Scripted events
 */
class TriggerVolume {
public:
    using TriggerId = uint32_t;
    static constexpr TriggerId INVALID_ID = 0;

    TriggerVolume();
    explicit TriggerVolume(const CollisionShape& shape);
    ~TriggerVolume() = default;

    // Copy and move
    TriggerVolume(const TriggerVolume&) = default;
    TriggerVolume& operator=(const TriggerVolume&) = default;
    TriggerVolume(TriggerVolume&&) noexcept = default;
    TriggerVolume& operator=(TriggerVolume&&) noexcept = default;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] TriggerId GetId() const noexcept { return m_id; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetEventName() const noexcept { return m_eventName; }
    void SetEventName(const std::string& eventName) { m_eventName = eventName; }

    // =========================================================================
    // State
    // =========================================================================

    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    [[nodiscard]] bool IsOneShot() const noexcept { return m_oneShot; }
    void SetOneShot(bool oneShot) { m_oneShot = oneShot; }

    [[nodiscard]] bool HasTriggered() const noexcept { return m_hasTriggered; }
    void Reset() { m_hasTriggered = false; m_overlappingBodies.clear(); }

    // =========================================================================
    // Transform
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; m_boundsDirty = true; }

    [[nodiscard]] const glm::quat& GetRotation() const noexcept { return m_rotation; }
    void SetRotation(const glm::quat& rot) { m_rotation = rot; m_boundsDirty = true; }

    [[nodiscard]] const glm::vec3& GetScale() const noexcept { return m_scale; }
    void SetScale(const glm::vec3& scale) { m_scale = scale; m_boundsDirty = true; }

    [[nodiscard]] glm::mat4 GetTransformMatrix() const;

    // =========================================================================
    // Shape
    // =========================================================================

    [[nodiscard]] const CollisionShape& GetShape() const noexcept { return m_shape; }
    void SetShape(const CollisionShape& shape) { m_shape = shape; m_boundsDirty = true; }

    [[nodiscard]] AABB GetWorldAABB() const;
    [[nodiscard]] OBB GetWorldOBB() const;

    // =========================================================================
    // Collision Filtering
    // =========================================================================

    [[nodiscard]] uint32_t GetCollisionMask() const noexcept { return m_collisionMask; }
    void SetCollisionMask(uint32_t mask) { m_collisionMask = mask; }

    /**
     * @brief Check if a body passes the filter
     */
    [[nodiscard]] bool ShouldTriggerFor(const CollisionBody& body) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnEnter(TriggerEnterCallback callback) { m_onEnter = std::move(callback); }
    void SetOnStay(TriggerStayCallback callback) { m_onStay = std::move(callback); }
    void SetOnExit(TriggerExitCallback callback) { m_onExit = std::move(callback); }

    // =========================================================================
    // Python Event System Integration
    // =========================================================================

    void SetPythonBinding(const PythonEventBinding& binding) { m_pythonBinding = binding; }
    [[nodiscard]] const std::optional<PythonEventBinding>& GetPythonBinding() const {
        return m_pythonBinding;
    }
    void ClearPythonBinding() { m_pythonBinding.reset(); }

    // =========================================================================
    // Overlap State
    // =========================================================================

    /**
     * @brief Get bodies currently overlapping this trigger
     */
    [[nodiscard]] const std::unordered_set<CollisionBody::BodyId>& GetOverlappingBodies() const {
        return m_overlappingBodies;
    }

    /**
     * @brief Check if a specific body is overlapping
     */
    [[nodiscard]] bool IsOverlapping(CollisionBody::BodyId bodyId) const {
        return m_overlappingBodies.count(bodyId) > 0;
    }

    /**
     * @brief Get count of overlapping bodies
     */
    [[nodiscard]] size_t GetOverlapCount() const { return m_overlappingBodies.size(); }

    // =========================================================================
    // User Data
    // =========================================================================

    void SetUserData(void* data) { m_userData = data; }
    [[nodiscard]] void* GetUserData() const noexcept { return m_userData; }

    template<typename T>
    [[nodiscard]] T* GetUserDataAs() const { return static_cast<T*>(m_userData); }

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    static std::expected<TriggerVolume, std::string> FromJson(const nlohmann::json& j);

private:
    friend class TriggerSystem;

    void OnEnter(CollisionBody* body);
    void OnStay(CollisionBody* body, float deltaTime);
    void OnExit(CollisionBody* body);

    // Identity
    TriggerId m_id;
    std::string m_name;
    std::string m_eventName;

    // State
    bool m_enabled = true;
    bool m_oneShot = false;
    bool m_hasTriggered = false;

    // Transform
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f};

    // Shape
    CollisionShape m_shape;
    mutable bool m_boundsDirty = true;
    mutable AABB m_cachedAABB;

    // Filtering
    uint32_t m_collisionMask = CollisionLayer::All;

    // Callbacks
    TriggerEnterCallback m_onEnter;
    TriggerStayCallback m_onStay;
    TriggerExitCallback m_onExit;

    // Python integration
    std::optional<PythonEventBinding> m_pythonBinding;

    // Overlap tracking
    std::unordered_set<CollisionBody::BodyId> m_overlappingBodies;

    // User data
    void* m_userData = nullptr;

    // ID counter
    static TriggerId s_nextId;
};

/**
 * @brief Trigger system - manages all trigger volumes
 *
 * Provides centralized management of trigger volumes with efficient
 * overlap detection and event dispatching.
 */
class TriggerSystem {
public:
    TriggerSystem();
    explicit TriggerSystem(PhysicsWorld* world);
    ~TriggerSystem() = default;

    // Non-copyable
    TriggerSystem(const TriggerSystem&) = delete;
    TriggerSystem& operator=(const TriggerSystem&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /**
     * @brief Set the physics world to query bodies from
     */
    void SetPhysicsWorld(PhysicsWorld* world) { m_physicsWorld = world; }

    // =========================================================================
    // Trigger Management
    // =========================================================================

    /**
     * @brief Add a trigger volume
     * @return Pointer to the added trigger (owned by system)
     */
    TriggerVolume* AddTrigger(std::unique_ptr<TriggerVolume> trigger);

    /**
     * @brief Create and add a trigger with shape
     */
    TriggerVolume* CreateTrigger(const CollisionShape& shape);

    /**
     * @brief Create box trigger
     */
    TriggerVolume* CreateBoxTrigger(const glm::vec3& position, const glm::vec3& halfExtents);

    /**
     * @brief Create sphere trigger
     */
    TriggerVolume* CreateSphereTrigger(const glm::vec3& position, float radius);

    /**
     * @brief Remove a trigger
     */
    void RemoveTrigger(TriggerVolume* trigger);
    void RemoveTrigger(TriggerVolume::TriggerId id);

    /**
     * @brief Get trigger by ID
     */
    [[nodiscard]] TriggerVolume* GetTrigger(TriggerVolume::TriggerId id);
    [[nodiscard]] const TriggerVolume* GetTrigger(TriggerVolume::TriggerId id) const;

    /**
     * @brief Get trigger by name
     */
    [[nodiscard]] TriggerVolume* GetTriggerByName(const std::string& name);

    /**
     * @brief Get all triggers
     */
    [[nodiscard]] const std::vector<std::unique_ptr<TriggerVolume>>& GetTriggers() const {
        return m_triggers;
    }

    /**
     * @brief Get trigger count
     */
    [[nodiscard]] size_t GetTriggerCount() const { return m_triggers.size(); }

    /**
     * @brief Clear all triggers
     */
    void Clear();

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update all triggers and fire events
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Force update a specific trigger
     */
    void UpdateTrigger(TriggerVolume& trigger, float deltaTime);

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Find all triggers containing a point
     */
    [[nodiscard]] std::vector<TriggerVolume*> QueryPoint(const glm::vec3& point) const;

    /**
     * @brief Find all triggers overlapping a sphere
     */
    [[nodiscard]] std::vector<TriggerVolume*> QuerySphere(const glm::vec3& center, float radius) const;

    /**
     * @brief Find all triggers overlapping an AABB
     */
    [[nodiscard]] std::vector<TriggerVolume*> QueryAABB(const AABB& aabb) const;

    /**
     * @brief Find all triggers a body overlaps
     */
    [[nodiscard]] std::vector<TriggerVolume*> QueryBody(const CollisionBody& body) const;

    // =========================================================================
    // Python Event System
    // =========================================================================

    /**
     * @brief Event handler type for Python events
     */
    using PythonEventHandler = std::function<void(const std::string& module,
                                                   const std::string& function,
                                                   const TriggerEvent& event)>;

    /**
     * @brief Set handler for Python events
     */
    void SetPythonEventHandler(PythonEventHandler handler) {
        m_pythonEventHandler = std::move(handler);
    }

    // =========================================================================
    // Global Event Callbacks
    // =========================================================================

    /**
     * @brief Set global callback for any trigger enter
     */
    void SetGlobalOnEnter(TriggerEnterCallback callback) {
        m_globalOnEnter = std::move(callback);
    }

    /**
     * @brief Set global callback for any trigger exit
     */
    void SetGlobalOnExit(TriggerExitCallback callback) {
        m_globalOnExit = std::move(callback);
    }

private:
    /**
     * @brief Check if a body overlaps a trigger
     */
    bool TestOverlap(const TriggerVolume& trigger, const CollisionBody& body) const;

    /**
     * @brief Fire Python event
     */
    void FirePythonEvent(const TriggerVolume& trigger, const TriggerEvent& event);

    PhysicsWorld* m_physicsWorld = nullptr;
    std::vector<std::unique_ptr<TriggerVolume>> m_triggers;
    std::unordered_map<TriggerVolume::TriggerId, TriggerVolume*> m_triggerMap;

    // Global callbacks
    TriggerEnterCallback m_globalOnEnter;
    TriggerExitCallback m_globalOnExit;

    // Python event handler
    PythonEventHandler m_pythonEventHandler;
};

/**
 * @brief Convenience functions for common trigger patterns
 */
namespace TriggerHelpers {

/**
 * @brief Create a checkpoint trigger
 */
std::unique_ptr<TriggerVolume> CreateCheckpoint(
    const glm::vec3& position,
    const glm::vec3& size,
    const std::string& checkpointId,
    TriggerEnterCallback onReach = nullptr);

/**
 * @brief Create a damage zone trigger
 */
std::unique_ptr<TriggerVolume> CreateDamageZone(
    const glm::vec3& position,
    float radius,
    float damagePerSecond,
    uint32_t affectedLayers = CollisionLayer::Unit | CollisionLayer::Player);

/**
 * @brief Create an area effect trigger (heal, buff, etc.)
 */
std::unique_ptr<TriggerVolume> CreateAreaEffect(
    const glm::vec3& position,
    const glm::vec3& halfExtents,
    const std::string& effectId,
    TriggerStayCallback onStay);

/**
 * @brief Create a spawn zone trigger
 */
std::unique_ptr<TriggerVolume> CreateSpawnZone(
    const glm::vec3& position,
    const glm::vec3& halfExtents,
    const std::string& spawnEventName);

/**
 * @brief Create an awareness/detection trigger
 */
std::unique_ptr<TriggerVolume> CreateDetectionZone(
    const glm::vec3& position,
    float radius,
    uint32_t detectLayers,
    TriggerEnterCallback onDetect);

} // namespace TriggerHelpers

} // namespace Nova
