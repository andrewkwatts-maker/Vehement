#pragma once

#include "CollisionShape.hpp"
#include "SDFCollision.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

namespace Nova {

// Forward declarations
class RigidBodyWorld;
class SDFCollisionSystem;
class SDFCollisionScene;

/**
 * @brief Rigid body type determining simulation behavior
 */
enum class RigidBodyType : uint8_t {
    Dynamic,    ///< Fully simulated by physics forces
    Kinematic,  ///< User-controlled position, affects dynamic bodies
    Static      ///< Never moves, infinite mass
};

/**
 * @brief Rigid body activation state for sleeping optimization
 */
enum class ActivationState : uint8_t {
    Active,             ///< Fully simulated
    WantsSleep,         ///< Below threshold, counting down to sleep
    Sleeping,           ///< Not simulated until woken
    DisableSimulation   ///< Never simulated
};

/**
 * @brief Material properties for contact resolution
 */
struct RigidBodyMaterial {
    float friction = 0.5f;          ///< Coulomb friction coefficient [0, 1+]
    float restitution = 0.3f;       ///< Bounciness [0, 1]
    float rollingFriction = 0.0f;   ///< Rolling resistance
    float spinningFriction = 0.0f;  ///< Spinning (torsional) friction

    static RigidBodyMaterial Default() { return {}; }
    static RigidBodyMaterial Bouncy() { return {0.3f, 0.9f, 0.0f, 0.0f}; }
    static RigidBodyMaterial Rough() { return {0.9f, 0.1f, 0.1f, 0.1f}; }
    static RigidBodyMaterial Slippery() { return {0.05f, 0.2f, 0.0f, 0.0f}; }
};

/**
 * @brief Force mode for applying forces/impulses
 */
enum class ForceMode : uint8_t {
    Force,          ///< Continuous force (affected by mass, multiplied by dt)
    Acceleration,   ///< Continuous acceleration (ignores mass, multiplied by dt)
    Impulse,        ///< Instant impulse (affected by mass)
    VelocityChange  ///< Instant velocity change (ignores mass)
};

/**
 * @brief Collision filter for layer-based filtering
 */
struct CollisionFilter {
    uint32_t group = 1;         ///< Collision group this body belongs to
    uint32_t mask = 0xFFFFFFFF; ///< Groups this body collides with

    [[nodiscard]] bool CanCollideWith(const CollisionFilter& other) const {
        return (group & other.mask) != 0 && (other.group & mask) != 0;
    }
};

/**
 * @brief Contact point from collision detection
 */
struct RigidBodyContact {
    glm::vec3 point{0.0f};          ///< World-space contact point
    glm::vec3 normal{0.0f, 1.0f, 0.0f}; ///< Contact normal (from A to B)
    float penetration = 0.0f;       ///< Penetration depth
    float normalImpulse = 0.0f;     ///< Accumulated normal impulse (for warmstarting)
    float tangentImpulse1 = 0.0f;   ///< Accumulated tangent impulse
    float tangentImpulse2 = 0.0f;   ///< Accumulated tangent impulse
};

/**
 * @brief Contact manifold between two rigid bodies
 */
struct RigidBodyContactManifold {
    static constexpr size_t MAX_CONTACTS = 4;

    class RigidBody* bodyA = nullptr;
    class RigidBody* bodyB = nullptr;
    RigidBodyContact contacts[MAX_CONTACTS];
    size_t contactCount = 0;

    void AddContact(const RigidBodyContact& contact);
    void RemoveContact(size_t index);
    void Clear() { contactCount = 0; }
};

/**
 * @brief Rigid body component for physics simulation
 *
 * Represents a rigid body with mass, inertia tensor, and velocities.
 * Supports forces, torques, impulses, and constraint solving.
 */
class RigidBody {
public:
    using BodyId = uint32_t;
    static constexpr BodyId INVALID_ID = 0;

    // Collision callback types
    using ContactCallback = std::function<void(RigidBody& other, const RigidBodyContactManifold& manifold)>;

    RigidBody();
    explicit RigidBody(RigidBodyType type);
    ~RigidBody() = default;

    // Non-copyable, movable
    RigidBody(const RigidBody&) = delete;
    RigidBody& operator=(const RigidBody&) = delete;
    RigidBody(RigidBody&&) noexcept = default;
    RigidBody& operator=(RigidBody&&) noexcept = default;

    // =========================================================================
    // Identity and Type
    // =========================================================================

    [[nodiscard]] BodyId GetId() const noexcept { return m_id; }

    [[nodiscard]] RigidBodyType GetType() const noexcept { return m_type; }
    void SetType(RigidBodyType type);

    [[nodiscard]] bool IsDynamic() const noexcept { return m_type == RigidBodyType::Dynamic; }
    [[nodiscard]] bool IsKinematic() const noexcept { return m_type == RigidBodyType::Kinematic; }
    [[nodiscard]] bool IsStatic() const noexcept { return m_type == RigidBodyType::Static; }

    // =========================================================================
    // Transform
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }
    void SetPosition(const glm::vec3& position);

    [[nodiscard]] const glm::quat& GetRotation() const noexcept { return m_rotation; }
    void SetRotation(const glm::quat& rotation);

    /**
     * @brief Get the full 4x4 transformation matrix
     */
    [[nodiscard]] glm::mat4 GetTransform() const;

    /**
     * @brief Set position and rotation simultaneously
     */
    void SetTransform(const glm::vec3& position, const glm::quat& rotation);

    /**
     * @brief Transform a point from local space to world space
     */
    [[nodiscard]] glm::vec3 TransformPoint(const glm::vec3& localPoint) const;

    /**
     * @brief Transform a direction from local space to world space
     */
    [[nodiscard]] glm::vec3 TransformDirection(const glm::vec3& localDir) const;

    /**
     * @brief Transform a point from world space to local space
     */
    [[nodiscard]] glm::vec3 InverseTransformPoint(const glm::vec3& worldPoint) const;

    // =========================================================================
    // Velocity
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetLinearVelocity() const noexcept { return m_linearVelocity; }
    void SetLinearVelocity(const glm::vec3& velocity);

    [[nodiscard]] const glm::vec3& GetAngularVelocity() const noexcept { return m_angularVelocity; }
    void SetAngularVelocity(const glm::vec3& velocity);

    /**
     * @brief Get velocity at a world-space point
     */
    [[nodiscard]] glm::vec3 GetPointVelocity(const glm::vec3& worldPoint) const;

    /**
     * @brief Get velocity at a local-space point
     */
    [[nodiscard]] glm::vec3 GetLocalPointVelocity(const glm::vec3& localPoint) const;

    // =========================================================================
    // Mass Properties
    // =========================================================================

    [[nodiscard]] float GetMass() const noexcept { return m_mass; }
    void SetMass(float mass);

    [[nodiscard]] float GetInverseMass() const noexcept { return m_inverseMass; }

    /**
     * @brief Get the local-space inertia tensor
     */
    [[nodiscard]] const glm::vec3& GetLocalInertia() const noexcept { return m_localInertia; }

    /**
     * @brief Set the local-space inertia tensor (diagonal)
     */
    void SetLocalInertia(const glm::vec3& inertia);

    /**
     * @brief Get the world-space inverse inertia tensor
     */
    [[nodiscard]] glm::mat3 GetWorldInverseInertiaTensor() const;

    /**
     * @brief Compute mass properties from a collision shape
     */
    void ComputeMassProperties(const CollisionShape& shape, float density = 1.0f);

    /**
     * @brief Set mass properties for common shapes
     */
    void SetMassPropertiesBox(float mass, const glm::vec3& halfExtents);
    void SetMassPropertiesSphere(float mass, float radius);
    void SetMassPropertiesCapsule(float mass, float radius, float height);
    void SetMassPropertiesCylinder(float mass, float radius, float height);

    // =========================================================================
    // Forces and Impulses
    // =========================================================================

    /**
     * @brief Add force at center of mass
     * @param force Force vector in world space
     * @param mode How to apply the force
     */
    void AddForce(const glm::vec3& force, ForceMode mode = ForceMode::Force);

    /**
     * @brief Add force at a world-space position
     * @param force Force vector in world space
     * @param position World-space position to apply force
     * @param mode How to apply the force
     */
    void AddForceAtPosition(const glm::vec3& force, const glm::vec3& position,
                            ForceMode mode = ForceMode::Force);

    /**
     * @brief Add force at a local-space position
     */
    void AddForceAtLocalPosition(const glm::vec3& force, const glm::vec3& localPosition,
                                  ForceMode mode = ForceMode::Force);

    /**
     * @brief Add torque around center of mass
     * @param torque Torque vector in world space
     * @param mode How to apply the torque
     */
    void AddTorque(const glm::vec3& torque, ForceMode mode = ForceMode::Force);

    /**
     * @brief Add relative force (in local space)
     */
    void AddRelativeForce(const glm::vec3& localForce, ForceMode mode = ForceMode::Force);

    /**
     * @brief Add relative torque (in local space)
     */
    void AddRelativeTorque(const glm::vec3& localTorque, ForceMode mode = ForceMode::Force);

    /**
     * @brief Apply impulse at center of mass
     */
    void AddImpulse(const glm::vec3& impulse);

    /**
     * @brief Apply impulse at world position
     */
    void AddImpulseAtPosition(const glm::vec3& impulse, const glm::vec3& position);

    /**
     * @brief Clear accumulated forces and torques
     */
    void ClearForces();

    [[nodiscard]] const glm::vec3& GetForce() const noexcept { return m_force; }
    [[nodiscard]] const glm::vec3& GetTorque() const noexcept { return m_torque; }

    // =========================================================================
    // Damping and Gravity
    // =========================================================================

    [[nodiscard]] float GetLinearDamping() const noexcept { return m_linearDamping; }
    void SetLinearDamping(float damping) { m_linearDamping = glm::clamp(damping, 0.0f, 1.0f); }

    [[nodiscard]] float GetAngularDamping() const noexcept { return m_angularDamping; }
    void SetAngularDamping(float damping) { m_angularDamping = glm::clamp(damping, 0.0f, 1.0f); }

    [[nodiscard]] float GetGravityScale() const noexcept { return m_gravityScale; }
    void SetGravityScale(float scale) { m_gravityScale = scale; }

    [[nodiscard]] bool UsesGravity() const noexcept { return m_useGravity; }
    void SetUseGravity(bool use) { m_useGravity = use; }

    // =========================================================================
    // Material
    // =========================================================================

    [[nodiscard]] const RigidBodyMaterial& GetMaterial() const noexcept { return m_material; }
    void SetMaterial(const RigidBodyMaterial& material) { m_material = material; }

    // =========================================================================
    // Collision
    // =========================================================================

    [[nodiscard]] const CollisionFilter& GetCollisionFilter() const noexcept { return m_collisionFilter; }
    void SetCollisionFilter(const CollisionFilter& filter) { m_collisionFilter = filter; }

    /**
     * @brief Attach a collision shape
     */
    void SetCollisionShape(std::shared_ptr<CollisionShape> shape);
    [[nodiscard]] CollisionShape* GetCollisionShape() const { return m_collisionShape.get(); }

    /**
     * @brief Create and attach an SDF collider for this body
     */
    void CreateSDFCollider();
    [[nodiscard]] ISDFCollider* GetSDFCollider() const { return m_sdfCollider.get(); }

    /**
     * @brief Get world-space AABB
     */
    [[nodiscard]] AABB GetWorldAABB() const;

    // =========================================================================
    // Activation State (Sleeping)
    // =========================================================================

    [[nodiscard]] ActivationState GetActivationState() const noexcept { return m_activationState; }
    void SetActivationState(ActivationState state);

    [[nodiscard]] bool IsActive() const noexcept {
        return m_activationState == ActivationState::Active ||
               m_activationState == ActivationState::WantsSleep;
    }
    [[nodiscard]] bool IsSleeping() const noexcept { return m_activationState == ActivationState::Sleeping; }

    /**
     * @brief Wake up the body
     */
    void Activate();

    /**
     * @brief Put the body to sleep
     */
    void Sleep();

    /**
     * @brief Check if body can go to sleep based on velocities
     */
    [[nodiscard]] bool CanSleep(float linearThreshold, float angularThreshold) const;

    [[nodiscard]] float GetSleepTimer() const noexcept { return m_sleepTimer; }
    void ResetSleepTimer() { m_sleepTimer = 0.0f; }
    void UpdateSleepTimer(float dt) { m_sleepTimer += dt; }

    // =========================================================================
    // Constraints
    // =========================================================================

    /**
     * @brief Lock linear motion on specified axes (local space)
     */
    void SetLinearFactor(const glm::vec3& factor) { m_linearFactor = factor; }
    [[nodiscard]] const glm::vec3& GetLinearFactor() const noexcept { return m_linearFactor; }

    /**
     * @brief Lock angular motion on specified axes (local space)
     */
    void SetAngularFactor(const glm::vec3& factor) { m_angularFactor = factor; }
    [[nodiscard]] const glm::vec3& GetAngularFactor() const noexcept { return m_angularFactor; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnContactBegin(ContactCallback callback) { m_onContactBegin = std::move(callback); }
    void SetOnContactEnd(ContactCallback callback) { m_onContactEnd = std::move(callback); }

    // =========================================================================
    // User Data
    // =========================================================================

    void SetUserData(void* data) { m_userData = data; }
    [[nodiscard]] void* GetUserData() const noexcept { return m_userData; }

    template<typename T>
    [[nodiscard]] T* GetUserDataAs() const { return static_cast<T*>(m_userData); }

    // =========================================================================
    // Integration (called by RigidBodyWorld)
    // =========================================================================

    /**
     * @brief Integrate forces to velocities (semi-implicit Euler)
     */
    void IntegrateForces(float dt, const glm::vec3& gravity);

    /**
     * @brief Integrate velocities to positions
     */
    void IntegrateVelocities(float dt);

    /**
     * @brief Apply velocity constraints (linear/angular factors)
     */
    void ApplyConstraints();

private:
    friend class RigidBodyWorld;

    void SetId(BodyId id) { m_id = id; }
    void OnContactBegin(RigidBody& other, const RigidBodyContactManifold& manifold);
    void OnContactEnd(RigidBody& other, const RigidBodyContactManifold& manifold);

    // Identity
    BodyId m_id = INVALID_ID;
    RigidBodyType m_type = RigidBodyType::Dynamic;

    // Transform
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Velocity
    glm::vec3 m_linearVelocity{0.0f};
    glm::vec3 m_angularVelocity{0.0f};

    // Accumulated forces (cleared each step)
    glm::vec3 m_force{0.0f};
    glm::vec3 m_torque{0.0f};

    // Mass properties
    float m_mass = 1.0f;
    float m_inverseMass = 1.0f;
    glm::vec3 m_localInertia{1.0f}; // Diagonal inertia tensor
    glm::vec3 m_inverseLocalInertia{1.0f};

    // Damping
    float m_linearDamping = 0.01f;
    float m_angularDamping = 0.05f;

    // Gravity
    float m_gravityScale = 1.0f;
    bool m_useGravity = true;

    // Material
    RigidBodyMaterial m_material;

    // Collision
    CollisionFilter m_collisionFilter;
    std::shared_ptr<CollisionShape> m_collisionShape;
    std::unique_ptr<ISDFCollider> m_sdfCollider;

    // Activation
    ActivationState m_activationState = ActivationState::Active;
    float m_sleepTimer = 0.0f;

    // Motion constraints
    glm::vec3 m_linearFactor{1.0f};
    glm::vec3 m_angularFactor{1.0f};

    // Callbacks
    ContactCallback m_onContactBegin;
    ContactCallback m_onContactEnd;

    // User data
    void* m_userData = nullptr;

    // Cached world AABB
    mutable bool m_aabbDirty = true;
    mutable AABB m_worldAABB;

    // ID counter
    static BodyId s_nextId;
};

/**
 * @brief Constraint base class for connecting rigid bodies
 */
class IConstraint {
public:
    virtual ~IConstraint() = default;

    /**
     * @brief Prepare constraint for solving (pre-step)
     */
    virtual void PrepareForSolve(float dt) = 0;

    /**
     * @brief Solve velocity constraint
     */
    virtual void SolveVelocity() = 0;

    /**
     * @brief Solve position constraint (optional)
     */
    virtual void SolvePosition() {}

    /**
     * @brief Get bodies involved in constraint
     */
    [[nodiscard]] virtual RigidBody* GetBodyA() const = 0;
    [[nodiscard]] virtual RigidBody* GetBodyB() const = 0;

    /**
     * @brief Check if constraint is still valid
     */
    [[nodiscard]] virtual bool IsValid() const = 0;

    /**
     * @brief Enable/disable constraint
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

protected:
    bool m_enabled = true;
};

/**
 * @brief Point-to-point constraint (ball joint)
 */
class PointToPointConstraint : public IConstraint {
public:
    PointToPointConstraint(RigidBody* bodyA, RigidBody* bodyB,
                           const glm::vec3& pivotA, const glm::vec3& pivotB);

    void PrepareForSolve(float dt) override;
    void SolveVelocity() override;
    void SolvePosition() override;

    [[nodiscard]] RigidBody* GetBodyA() const override { return m_bodyA; }
    [[nodiscard]] RigidBody* GetBodyB() const override { return m_bodyB; }
    [[nodiscard]] bool IsValid() const override { return m_bodyA != nullptr; }

    void SetPivotA(const glm::vec3& pivot) { m_pivotA = pivot; }
    void SetPivotB(const glm::vec3& pivot) { m_pivotB = pivot; }

private:
    RigidBody* m_bodyA = nullptr;
    RigidBody* m_bodyB = nullptr;
    glm::vec3 m_pivotA{0.0f};
    glm::vec3 m_pivotB{0.0f};

    // Solver data
    glm::vec3 m_rA{0.0f};
    glm::vec3 m_rB{0.0f};
    glm::mat3 m_effectiveMass{1.0f};
    glm::vec3 m_bias{0.0f};
    glm::vec3 m_accumulatedImpulse{0.0f};
};

/**
 * @brief Hinge constraint (revolute joint)
 */
class HingeConstraint : public IConstraint {
public:
    HingeConstraint(RigidBody* bodyA, RigidBody* bodyB,
                    const glm::vec3& pivotA, const glm::vec3& pivotB,
                    const glm::vec3& axisA, const glm::vec3& axisB);

    void PrepareForSolve(float dt) override;
    void SolveVelocity() override;
    void SolvePosition() override;

    [[nodiscard]] RigidBody* GetBodyA() const override { return m_bodyA; }
    [[nodiscard]] RigidBody* GetBodyB() const override { return m_bodyB; }
    [[nodiscard]] bool IsValid() const override { return m_bodyA != nullptr; }

    void SetLimits(float lower, float upper);
    void EnableLimits(bool enable) { m_useLimits = enable; }

    void SetMotor(float targetVelocity, float maxTorque);
    void EnableMotor(bool enable) { m_useMotor = enable; }

private:
    RigidBody* m_bodyA = nullptr;
    RigidBody* m_bodyB = nullptr;
    glm::vec3 m_pivotA{0.0f};
    glm::vec3 m_pivotB{0.0f};
    glm::vec3 m_axisA{0.0f, 1.0f, 0.0f};
    glm::vec3 m_axisB{0.0f, 1.0f, 0.0f};

    bool m_useLimits = false;
    float m_lowerLimit = 0.0f;
    float m_upperLimit = 0.0f;

    bool m_useMotor = false;
    float m_motorTargetVelocity = 0.0f;
    float m_motorMaxTorque = 0.0f;
};

} // namespace Nova
