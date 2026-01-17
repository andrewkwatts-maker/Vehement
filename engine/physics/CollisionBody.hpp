#pragma once

#include "CollisionShape.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

namespace Nova {

// Forward declarations
class PhysicsWorld;
struct ContactInfo;

/**
 * @brief Type of physics body determining simulation behavior
 */
enum class BodyType : uint8_t {
    Static,     ///< Never moves, infinite mass (walls, terrain)
    Kinematic,  ///< Moved by user code, not by physics (platforms, animated objects)
    Dynamic     ///< Fully simulated by physics (characters, projectiles)
};

/**
 * @brief Convert body type to string for debugging/serialization
 */
[[nodiscard]] constexpr const char* BodyTypeToString(BodyType type) noexcept {
    switch (type) {
        case BodyType::Static:    return "static";
        case BodyType::Kinematic: return "kinematic";
        case BodyType::Dynamic:   return "dynamic";
        default:                  return "unknown";
    }
}

/**
 * @brief Parse body type from string
 */
[[nodiscard]] std::optional<BodyType> BodyTypeFromString(const std::string& str) noexcept;

/**
 * @brief Collision layer bit flags for filtering
 */
namespace CollisionLayer {
    constexpr uint32_t None       = 0;
    constexpr uint32_t Default    = 1 << 0;
    constexpr uint32_t Terrain    = 1 << 1;
    constexpr uint32_t Unit       = 1 << 2;
    constexpr uint32_t Building   = 1 << 3;
    constexpr uint32_t Projectile = 1 << 4;
    constexpr uint32_t Pickup     = 1 << 5;
    constexpr uint32_t Trigger    = 1 << 6;
    constexpr uint32_t Player     = 1 << 7;
    constexpr uint32_t Enemy      = 1 << 8;
    constexpr uint32_t Vehicle    = 1 << 9;
    constexpr uint32_t Effect     = 1 << 10;
    constexpr uint32_t All        = 0xFFFFFFFF;

    /**
     * @brief Get collision layer from string name
     */
    [[nodiscard]] uint32_t FromString(const std::string& name) noexcept;

    /**
     * @brief Get string name from collision layer
     */
    [[nodiscard]] const char* ToString(uint32_t layer) noexcept;

    /**
     * @brief Parse layer mask from JSON array of layer names
     */
    [[nodiscard]] uint32_t ParseMask(const nlohmann::json& j);
}

/**
 * @brief Contact point information from collision detection
 */
struct ContactPoint {
    glm::vec3 position;         ///< World space contact point
    glm::vec3 normal;           ///< Contact normal (from A to B)
    float penetration = 0.0f;   ///< Penetration depth
    int shapeIndexA = 0;        ///< Shape index on body A
    int shapeIndexB = 0;        ///< Shape index on body B
};

/**
 * @brief Full contact information between two bodies
 */
struct ContactInfo {
    class CollisionBody* bodyA = nullptr;
    class CollisionBody* bodyB = nullptr;
    std::vector<ContactPoint> points;
    glm::vec3 relativeVelocity{0.0f};
    float impulse = 0.0f;

    [[nodiscard]] bool IsValid() const { return bodyA && bodyB && !points.empty(); }
};

/**
 * @brief Collision body - wrapper around collision shapes with physics state
 *
 * Manages one or more collision shapes attached to an entity, handles
 * mass/inertia calculation, collision filtering, and contact queries.
 */
class CollisionBody {
public:
    using BodyId = uint32_t;
    static constexpr BodyId INVALID_ID = 0;

    // Callback types
    using CollisionCallback = std::function<void(CollisionBody& other, const ContactInfo& contact)>;
    using TriggerCallback = std::function<void(CollisionBody& other)>;

    CollisionBody();
    explicit CollisionBody(BodyType type);
    ~CollisionBody() = default;

    // Non-copyable, movable
    CollisionBody(const CollisionBody&) = delete;
    CollisionBody& operator=(const CollisionBody&) = delete;
    CollisionBody(CollisionBody&&) noexcept = default;
    CollisionBody& operator=(CollisionBody&&) noexcept = default;

    // =========================================================================
    // Body Type and State
    // =========================================================================

    [[nodiscard]] BodyId GetId() const noexcept { return m_id; }

    [[nodiscard]] BodyType GetBodyType() const noexcept { return m_bodyType; }
    void SetBodyType(BodyType type);

    [[nodiscard]] bool IsStatic() const noexcept { return m_bodyType == BodyType::Static; }
    [[nodiscard]] bool IsKinematic() const noexcept { return m_bodyType == BodyType::Kinematic; }
    [[nodiscard]] bool IsDynamic() const noexcept { return m_bodyType == BodyType::Dynamic; }

    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    [[nodiscard]] bool IsSleeping() const noexcept { return m_sleeping; }
    void SetSleeping(bool sleeping) { m_sleeping = sleeping; }
    void WakeUp() { m_sleeping = false; m_sleepTimer = 0.0f; }

    // =========================================================================
    // Transform
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }
    void SetPosition(const glm::vec3& pos);

    [[nodiscard]] const glm::quat& GetRotation() const noexcept { return m_rotation; }
    void SetRotation(const glm::quat& rot);

    [[nodiscard]] glm::mat4 GetTransformMatrix() const;

    // =========================================================================
    // Velocity (for dynamic bodies)
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetLinearVelocity() const noexcept { return m_linearVelocity; }
    void SetLinearVelocity(const glm::vec3& vel);

    [[nodiscard]] const glm::vec3& GetAngularVelocity() const noexcept { return m_angularVelocity; }
    void SetAngularVelocity(const glm::vec3& vel);

    /**
     * @brief Apply force at center of mass (accumulates for this frame)
     */
    void ApplyForce(const glm::vec3& force);

    /**
     * @brief Apply force at world position (creates torque)
     */
    void ApplyForceAtPoint(const glm::vec3& force, const glm::vec3& point);

    /**
     * @brief Apply torque (accumulates for this frame)
     */
    void ApplyTorque(const glm::vec3& torque);

    /**
     * @brief Apply instant impulse at center of mass
     */
    void ApplyImpulse(const glm::vec3& impulse);

    /**
     * @brief Apply instant impulse at world position
     */
    void ApplyImpulseAtPoint(const glm::vec3& impulse, const glm::vec3& point);

    /**
     * @brief Clear accumulated forces and torques
     */
    void ClearForces();

    [[nodiscard]] const glm::vec3& GetAccumulatedForce() const noexcept { return m_accumulatedForce; }
    [[nodiscard]] const glm::vec3& GetAccumulatedTorque() const noexcept { return m_accumulatedTorque; }

    // =========================================================================
    // Mass Properties
    // =========================================================================

    [[nodiscard]] float GetMass() const noexcept { return m_mass; }
    void SetMass(float mass);

    [[nodiscard]] float GetInverseMass() const noexcept { return m_inverseMass; }

    [[nodiscard]] const glm::mat3& GetInertiaTensor() const noexcept { return m_inertiaTensor; }
    [[nodiscard]] const glm::mat3& GetInverseInertiaTensor() const noexcept { return m_inverseInertiaTensor; }

    /**
     * @brief Recalculate mass and inertia from attached shapes
     */
    void RecalculateMassProperties();

    // =========================================================================
    // Damping
    // =========================================================================

    [[nodiscard]] float GetLinearDamping() const noexcept { return m_linearDamping; }
    void SetLinearDamping(float damping) { m_linearDamping = glm::clamp(damping, 0.0f, 1.0f); }

    [[nodiscard]] float GetAngularDamping() const noexcept { return m_angularDamping; }
    void SetAngularDamping(float damping) { m_angularDamping = glm::clamp(damping, 0.0f, 1.0f); }

    // =========================================================================
    // Gravity
    // =========================================================================

    [[nodiscard]] float GetGravityScale() const noexcept { return m_gravityScale; }
    void SetGravityScale(float scale) { m_gravityScale = scale; }

    // =========================================================================
    // Collision Shapes
    // =========================================================================

    /**
     * @brief Add a collision shape to this body
     * @return Index of the added shape
     */
    size_t AddShape(const CollisionShape& shape);
    size_t AddShape(CollisionShape&& shape);

    /**
     * @brief Remove shape at index
     */
    void RemoveShape(size_t index);

    /**
     * @brief Clear all shapes
     */
    void ClearShapes();

    [[nodiscard]] size_t GetShapeCount() const noexcept { return m_shapes.size(); }
    [[nodiscard]] CollisionShape& GetShape(size_t index) { return m_shapes.at(index); }
    [[nodiscard]] const CollisionShape& GetShape(size_t index) const { return m_shapes.at(index); }
    [[nodiscard]] const std::vector<CollisionShape>& GetShapes() const noexcept { return m_shapes; }

    // =========================================================================
    // Collision Filtering
    // =========================================================================

    [[nodiscard]] uint32_t GetCollisionLayer() const noexcept { return m_collisionLayer; }
    void SetCollisionLayer(uint32_t layer) { m_collisionLayer = layer; }

    [[nodiscard]] uint32_t GetCollisionMask() const noexcept { return m_collisionMask; }
    void SetCollisionMask(uint32_t mask) { m_collisionMask = mask; }

    /**
     * @brief Check if this body should collide with another based on layers
     */
    [[nodiscard]] bool ShouldCollideWith(const CollisionBody& other) const;

    // =========================================================================
    // Bounds
    // =========================================================================

    /**
     * @brief Get combined AABB of all shapes in world space
     */
    [[nodiscard]] AABB GetWorldAABB() const;

    /**
     * @brief Mark bounds as dirty (will recompute on next query)
     */
    void MarkBoundsDirty() { m_boundsDirty = true; }

    // =========================================================================
    // Collision Callbacks
    // =========================================================================

    void SetOnCollisionEnter(CollisionCallback callback) { m_onCollisionEnter = std::move(callback); }
    void SetOnCollisionStay(CollisionCallback callback) { m_onCollisionStay = std::move(callback); }
    void SetOnCollisionExit(CollisionCallback callback) { m_onCollisionExit = std::move(callback); }

    void SetOnTriggerEnter(TriggerCallback callback) { m_onTriggerEnter = std::move(callback); }
    void SetOnTriggerStay(TriggerCallback callback) { m_onTriggerStay = std::move(callback); }
    void SetOnTriggerExit(TriggerCallback callback) { m_onTriggerExit = std::move(callback); }

    // =========================================================================
    // Contact Queries
    // =========================================================================

    /**
     * @brief Get bodies currently in contact with this one
     */
    [[nodiscard]] const std::vector<BodyId>& GetContactBodies() const noexcept { return m_contactBodies; }

    /**
     * @brief Check if currently in contact with specific body
     */
    [[nodiscard]] bool IsInContactWith(BodyId otherId) const;

    /**
     * @brief Get number of current contacts
     */
    [[nodiscard]] size_t GetContactCount() const noexcept { return m_contactBodies.size(); }

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
    static std::optional<CollisionBody> FromJson(const nlohmann::json& j);

private:
    friend class PhysicsWorld;

    void SetId(BodyId id) { m_id = id; }
    void UpdateWorldAABB() const;

    // Callback invocation (called by PhysicsWorld)
    void OnCollisionEnter(CollisionBody& other, const ContactInfo& contact);
    void OnCollisionStay(CollisionBody& other, const ContactInfo& contact);
    void OnCollisionExit(CollisionBody& other, const ContactInfo& contact);
    void OnTriggerEnter(CollisionBody& other);
    void OnTriggerStay(CollisionBody& other);
    void OnTriggerExit(CollisionBody& other);

    // Add/remove contact tracking
    void AddContact(BodyId otherId);
    void RemoveContact(BodyId otherId);
    void ClearContacts();

    // Identity
    BodyId m_id = INVALID_ID;
    BodyType m_bodyType = BodyType::Static;

    // State
    bool m_enabled = true;
    bool m_sleeping = false;
    float m_sleepTimer = 0.0f;

    // Transform
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Velocity
    glm::vec3 m_linearVelocity{0.0f};
    glm::vec3 m_angularVelocity{0.0f};

    // Accumulated forces (cleared each step)
    glm::vec3 m_accumulatedForce{0.0f};
    glm::vec3 m_accumulatedTorque{0.0f};

    // Mass properties
    float m_mass = 1.0f;
    float m_inverseMass = 1.0f;
    glm::mat3 m_inertiaTensor{1.0f};
    glm::mat3 m_inverseInertiaTensor{1.0f};

    // Damping
    float m_linearDamping = 0.01f;
    float m_angularDamping = 0.05f;
    float m_gravityScale = 1.0f;

    // Collision shapes
    std::vector<CollisionShape> m_shapes;

    // Collision filtering
    uint32_t m_collisionLayer = CollisionLayer::Default;
    uint32_t m_collisionMask = CollisionLayer::All;

    // Cached world bounds
    mutable bool m_boundsDirty = true;
    mutable AABB m_worldAABB;

    // Callbacks
    CollisionCallback m_onCollisionEnter;
    CollisionCallback m_onCollisionStay;
    CollisionCallback m_onCollisionExit;
    TriggerCallback m_onTriggerEnter;
    TriggerCallback m_onTriggerStay;
    TriggerCallback m_onTriggerExit;

    // Contact tracking
    std::vector<BodyId> m_contactBodies;

    // User data
    void* m_userData = nullptr;

    // ID counter
    static BodyId s_nextId;
};

} // namespace Nova
