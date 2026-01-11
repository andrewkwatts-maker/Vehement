#include "RigidBody.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

// Static member initialization
RigidBody::BodyId RigidBody::s_nextId = 1;

namespace {
constexpr float kEpsilon = 1e-6f;
constexpr float kMaxAngularVelocity = 100.0f; // rad/s
} // anonymous namespace

// =============================================================================
// RigidBodyContactManifold Implementation
// =============================================================================

void RigidBodyContactManifold::AddContact(const RigidBodyContact& contact) {
    if (contactCount < MAX_CONTACTS) {
        contacts[contactCount++] = contact;
    } else {
        // Replace the contact with smallest penetration
        size_t minIndex = 0;
        float minPen = contacts[0].penetration;
        for (size_t i = 1; i < contactCount; ++i) {
            if (contacts[i].penetration < minPen) {
                minPen = contacts[i].penetration;
                minIndex = i;
            }
        }
        if (contact.penetration > minPen) {
            contacts[minIndex] = contact;
        }
    }
}

void RigidBodyContactManifold::RemoveContact(size_t index) {
    if (index < contactCount) {
        contacts[index] = contacts[contactCount - 1];
        --contactCount;
    }
}

// =============================================================================
// RigidBody Implementation
// =============================================================================

RigidBody::RigidBody() : m_id(s_nextId++) {
}

RigidBody::RigidBody(RigidBodyType type)
    : m_id(s_nextId++), m_type(type) {
    if (type == RigidBodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        m_inverseLocalInertia = glm::vec3(0.0f);
        m_useGravity = false;
    }
}

void RigidBody::SetType(RigidBodyType type) {
    if (m_type == type) return;

    m_type = type;

    if (type == RigidBodyType::Static) {
        m_inverseMass = 0.0f;
        m_inverseLocalInertia = glm::vec3(0.0f);
        m_linearVelocity = glm::vec3(0.0f);
        m_angularVelocity = glm::vec3(0.0f);
        m_useGravity = false;
        m_activationState = ActivationState::Sleeping;
    } else if (type == RigidBodyType::Kinematic) {
        m_inverseMass = 0.0f;
        m_inverseLocalInertia = glm::vec3(0.0f);
        m_useGravity = false;
        m_activationState = ActivationState::Active;
    } else {
        // Dynamic - restore mass properties
        if (m_mass > kEpsilon) {
            m_inverseMass = 1.0f / m_mass;
            m_inverseLocalInertia = 1.0f / glm::max(m_localInertia, glm::vec3(kEpsilon));
        }
        m_activationState = ActivationState::Active;
    }
}

void RigidBody::SetPosition(const glm::vec3& position) {
    m_position = position;
    m_aabbDirty = true;
    if (m_type == RigidBodyType::Dynamic) {
        Activate();
    }
}

void RigidBody::SetRotation(const glm::quat& rotation) {
    m_rotation = glm::normalize(rotation);
    m_aabbDirty = true;
    if (m_type == RigidBodyType::Dynamic) {
        Activate();
    }
}

glm::mat4 RigidBody::GetTransform() const {
    glm::mat4 transform = glm::mat4_cast(m_rotation);
    transform[3] = glm::vec4(m_position, 1.0f);
    return transform;
}

void RigidBody::SetTransform(const glm::vec3& position, const glm::quat& rotation) {
    m_position = position;
    m_rotation = glm::normalize(rotation);
    m_aabbDirty = true;
    if (m_type == RigidBodyType::Dynamic) {
        Activate();
    }
}

glm::vec3 RigidBody::TransformPoint(const glm::vec3& localPoint) const {
    return m_position + m_rotation * localPoint;
}

glm::vec3 RigidBody::TransformDirection(const glm::vec3& localDir) const {
    return m_rotation * localDir;
}

glm::vec3 RigidBody::InverseTransformPoint(const glm::vec3& worldPoint) const {
    return glm::inverse(m_rotation) * (worldPoint - m_position);
}

void RigidBody::SetLinearVelocity(const glm::vec3& velocity) {
    if (m_type == RigidBodyType::Static) return;
    m_linearVelocity = velocity;
    Activate();
}

void RigidBody::SetAngularVelocity(const glm::vec3& velocity) {
    if (m_type == RigidBodyType::Static) return;
    m_angularVelocity = velocity;
    Activate();
}

glm::vec3 RigidBody::GetPointVelocity(const glm::vec3& worldPoint) const {
    return m_linearVelocity + glm::cross(m_angularVelocity, worldPoint - m_position);
}

glm::vec3 RigidBody::GetLocalPointVelocity(const glm::vec3& localPoint) const {
    return GetPointVelocity(TransformPoint(localPoint));
}

void RigidBody::SetMass(float mass) {
    if (m_type == RigidBodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        return;
    }

    m_mass = std::max(mass, kEpsilon);
    m_inverseMass = 1.0f / m_mass;
}

void RigidBody::SetLocalInertia(const glm::vec3& inertia) {
    if (m_type == RigidBodyType::Static) {
        m_localInertia = glm::vec3(0.0f);
        m_inverseLocalInertia = glm::vec3(0.0f);
        return;
    }

    m_localInertia = glm::max(inertia, glm::vec3(kEpsilon));
    m_inverseLocalInertia = 1.0f / m_localInertia;
}

glm::mat3 RigidBody::GetWorldInverseInertiaTensor() const {
    if (m_type != RigidBodyType::Dynamic) {
        return glm::mat3(0.0f);
    }

    glm::mat3 R = glm::mat3_cast(m_rotation);
    glm::mat3 invInertiaLocal = glm::mat3(
        m_inverseLocalInertia.x, 0.0f, 0.0f,
        0.0f, m_inverseLocalInertia.y, 0.0f,
        0.0f, 0.0f, m_inverseLocalInertia.z
    );
    return R * invInertiaLocal * glm::transpose(R);
}

void RigidBody::ComputeMassProperties(const CollisionShape& shape, float density) {
    float mass = shape.CalculateMass() * density;
    SetMass(mass);

    glm::mat3 inertiaTensor = shape.CalculateInertiaTensor(mass);

    // Extract diagonal (assuming aligned inertia)
    m_localInertia = glm::vec3(inertiaTensor[0][0], inertiaTensor[1][1], inertiaTensor[2][2]);
    m_inverseLocalInertia = 1.0f / glm::max(m_localInertia, glm::vec3(kEpsilon));
}

void RigidBody::SetMassPropertiesBox(float mass, const glm::vec3& halfExtents) {
    SetMass(mass);

    // I = (1/12) * m * (h^2 + d^2) for each axis
    float m12 = mass / 12.0f;
    glm::vec3 size = halfExtents * 2.0f;
    m_localInertia = glm::vec3(
        m12 * (size.y * size.y + size.z * size.z),
        m12 * (size.x * size.x + size.z * size.z),
        m12 * (size.x * size.x + size.y * size.y)
    );
    m_inverseLocalInertia = 1.0f / glm::max(m_localInertia, glm::vec3(kEpsilon));
}

void RigidBody::SetMassPropertiesSphere(float mass, float radius) {
    SetMass(mass);

    // I = (2/5) * m * r^2 for solid sphere
    float I = 0.4f * mass * radius * radius;
    m_localInertia = glm::vec3(I);
    m_inverseLocalInertia = glm::vec3(1.0f / I);
}

void RigidBody::SetMassPropertiesCapsule(float mass, float radius, float height) {
    SetMass(mass);

    // Approximate as cylinder + hemispheres
    float cylinderHeight = height - 2.0f * radius;
    if (cylinderHeight < 0.0f) cylinderHeight = 0.0f;

    float totalVolume = glm::pi<float>() * radius * radius * cylinderHeight +
                        (4.0f / 3.0f) * glm::pi<float>() * radius * radius * radius;

    float cylinderVolume = glm::pi<float>() * radius * radius * cylinderHeight;
    float sphereVolume = totalVolume - cylinderVolume;

    float cylinderMass = mass * (cylinderVolume / totalVolume);
    float sphereMass = mass * (sphereVolume / totalVolume);

    // Cylinder inertia (around y-axis)
    float Ixx_cyl = cylinderMass * (3.0f * radius * radius + cylinderHeight * cylinderHeight) / 12.0f;
    float Iyy_cyl = cylinderMass * radius * radius / 2.0f;
    float Izz_cyl = Ixx_cyl;

    // Sphere inertia + parallel axis theorem
    float Isphere = 0.4f * sphereMass * radius * radius;
    float d = cylinderHeight / 2.0f + radius * 0.375f; // Approximate hemisphere center offset
    float Ixx_sphere = Isphere + sphereMass * d * d;

    m_localInertia = glm::vec3(
        Ixx_cyl + Ixx_sphere,
        Iyy_cyl + Isphere,
        Izz_cyl + Ixx_sphere
    );
    m_inverseLocalInertia = 1.0f / glm::max(m_localInertia, glm::vec3(kEpsilon));
}

void RigidBody::SetMassPropertiesCylinder(float mass, float radius, float height) {
    SetMass(mass);

    // Cylinder inertia
    float Ixx = mass * (3.0f * radius * radius + height * height) / 12.0f;
    float Iyy = mass * radius * radius / 2.0f;
    float Izz = Ixx;

    m_localInertia = glm::vec3(Ixx, Iyy, Izz);
    m_inverseLocalInertia = 1.0f / glm::max(m_localInertia, glm::vec3(kEpsilon));
}

void RigidBody::AddForce(const glm::vec3& force, ForceMode mode) {
    if (m_type != RigidBodyType::Dynamic) return;

    switch (mode) {
        case ForceMode::Force:
            m_force += force;
            break;
        case ForceMode::Acceleration:
            m_force += force * m_mass;
            break;
        case ForceMode::Impulse:
            m_linearVelocity += force * m_inverseMass;
            break;
        case ForceMode::VelocityChange:
            m_linearVelocity += force;
            break;
    }

    Activate();
}

void RigidBody::AddForceAtPosition(const glm::vec3& force, const glm::vec3& position, ForceMode mode) {
    if (m_type != RigidBodyType::Dynamic) return;

    glm::vec3 r = position - m_position;

    switch (mode) {
        case ForceMode::Force:
            m_force += force;
            m_torque += glm::cross(r, force);
            break;
        case ForceMode::Acceleration:
            m_force += force * m_mass;
            m_torque += glm::cross(r, force * m_mass);
            break;
        case ForceMode::Impulse:
            m_linearVelocity += force * m_inverseMass;
            m_angularVelocity += GetWorldInverseInertiaTensor() * glm::cross(r, force);
            break;
        case ForceMode::VelocityChange:
            m_linearVelocity += force;
            m_angularVelocity += GetWorldInverseInertiaTensor() * glm::cross(r, force * m_mass);
            break;
    }

    Activate();
}

void RigidBody::AddForceAtLocalPosition(const glm::vec3& force, const glm::vec3& localPosition, ForceMode mode) {
    AddForceAtPosition(force, TransformPoint(localPosition), mode);
}

void RigidBody::AddTorque(const glm::vec3& torque, ForceMode mode) {
    if (m_type != RigidBodyType::Dynamic) return;

    glm::mat3 invI = GetWorldInverseInertiaTensor();

    switch (mode) {
        case ForceMode::Force:
            m_torque += torque;
            break;
        case ForceMode::Acceleration:
            m_torque += glm::inverse(invI) * torque;
            break;
        case ForceMode::Impulse:
            m_angularVelocity += invI * torque;
            break;
        case ForceMode::VelocityChange:
            m_angularVelocity += torque;
            break;
    }

    Activate();
}

void RigidBody::AddRelativeForce(const glm::vec3& localForce, ForceMode mode) {
    AddForce(TransformDirection(localForce), mode);
}

void RigidBody::AddRelativeTorque(const glm::vec3& localTorque, ForceMode mode) {
    AddTorque(TransformDirection(localTorque), mode);
}

void RigidBody::AddImpulse(const glm::vec3& impulse) {
    if (m_type != RigidBodyType::Dynamic) return;
    m_linearVelocity += impulse * m_inverseMass;
    Activate();
}

void RigidBody::AddImpulseAtPosition(const glm::vec3& impulse, const glm::vec3& position) {
    if (m_type != RigidBodyType::Dynamic) return;

    m_linearVelocity += impulse * m_inverseMass;

    glm::vec3 r = position - m_position;
    m_angularVelocity += GetWorldInverseInertiaTensor() * glm::cross(r, impulse);

    Activate();
}

void RigidBody::ClearForces() {
    m_force = glm::vec3(0.0f);
    m_torque = glm::vec3(0.0f);
}

void RigidBody::SetCollisionShape(std::shared_ptr<CollisionShape> shape) {
    m_collisionShape = std::move(shape);
    m_aabbDirty = true;
    m_sdfCollider.reset(); // Invalidate SDF collider
}

void RigidBody::CreateSDFCollider() {
    if (!m_collisionShape) {
        m_sdfCollider.reset();
        return;
    }

    ShapeType type = m_collisionShape->GetType();

    switch (type) {
        case ShapeType::Sphere: {
            auto* params = m_collisionShape->GetParams<ShapeParams::Sphere>();
            if (params) {
                m_sdfCollider = std::make_unique<SDFSphereCollider>(m_position, params->radius);
            }
            break;
        }
        case ShapeType::Box: {
            auto* params = m_collisionShape->GetParams<ShapeParams::Box>();
            if (params) {
                m_sdfCollider = std::make_unique<SDFBoxCollider>(
                    m_position, params->halfExtents, m_rotation);
            }
            break;
        }
        case ShapeType::Capsule: {
            auto* params = m_collisionShape->GetParams<ShapeParams::Capsule>();
            if (params) {
                glm::vec3 axis = m_rotation * glm::vec3(0.0f, params->height * 0.5f, 0.0f);
                m_sdfCollider = std::make_unique<SDFCapsuleCollider>(
                    m_position - axis, m_position + axis, params->radius);
            }
            break;
        }
        default:
            // For other shapes, use bounding sphere
            float radius = m_collisionShape->GetBoundingRadius();
            m_sdfCollider = std::make_unique<SDFSphereCollider>(m_position, radius);
            break;
    }
}

AABB RigidBody::GetWorldAABB() const {
    if (!m_aabbDirty) {
        return m_worldAABB;
    }

    if (m_collisionShape) {
        m_worldAABB = m_collisionShape->ComputeWorldAABB(GetTransform());
    } else {
        // Default to a unit sphere AABB
        m_worldAABB.min = m_position - glm::vec3(0.5f);
        m_worldAABB.max = m_position + glm::vec3(0.5f);
    }

    m_aabbDirty = false;
    return m_worldAABB;
}

void RigidBody::SetActivationState(ActivationState state) {
    m_activationState = state;

    if (state == ActivationState::Sleeping) {
        m_linearVelocity = glm::vec3(0.0f);
        m_angularVelocity = glm::vec3(0.0f);
        ClearForces();
    }
}

void RigidBody::Activate() {
    if (m_activationState == ActivationState::DisableSimulation) return;
    if (m_type == RigidBodyType::Static) return;

    m_activationState = ActivationState::Active;
    m_sleepTimer = 0.0f;
}

void RigidBody::Sleep() {
    if (m_activationState == ActivationState::DisableSimulation) return;
    if (m_type != RigidBodyType::Dynamic) return;

    SetActivationState(ActivationState::Sleeping);
}

bool RigidBody::CanSleep(float linearThreshold, float angularThreshold) const {
    if (m_type != RigidBodyType::Dynamic) return false;
    if (m_activationState == ActivationState::DisableSimulation) return false;

    float linearSpeed2 = glm::dot(m_linearVelocity, m_linearVelocity);
    float angularSpeed2 = glm::dot(m_angularVelocity, m_angularVelocity);

    return linearSpeed2 < linearThreshold * linearThreshold &&
           angularSpeed2 < angularThreshold * angularThreshold;
}

void RigidBody::IntegrateForces(float dt, const glm::vec3& gravity) {
    if (m_type != RigidBodyType::Dynamic) return;
    if (!IsActive()) return;

    // Apply gravity
    if (m_useGravity) {
        m_force += gravity * m_mass * m_gravityScale;
    }

    // Integrate linear velocity: v += F * invM * dt
    m_linearVelocity += m_force * m_inverseMass * dt;

    // Integrate angular velocity: w += invI * T * dt
    glm::mat3 invI = GetWorldInverseInertiaTensor();
    m_angularVelocity += invI * m_torque * dt;

    // Apply damping
    float linearDampFactor = std::pow(1.0f - m_linearDamping, dt);
    float angularDampFactor = std::pow(1.0f - m_angularDamping, dt);

    m_linearVelocity *= linearDampFactor;
    m_angularVelocity *= angularDampFactor;

    // Clamp angular velocity
    float angularSpeed = glm::length(m_angularVelocity);
    if (angularSpeed > kMaxAngularVelocity) {
        m_angularVelocity *= kMaxAngularVelocity / angularSpeed;
    }

    ClearForces();
}

void RigidBody::IntegrateVelocities(float dt) {
    if (m_type == RigidBodyType::Static) return;
    if (!IsActive()) return;

    // Integrate position: x += v * dt
    m_position += m_linearVelocity * dt;

    // Integrate rotation using quaternion derivative
    // dq/dt = 0.5 * w * q (where w is quaternion (0, wx, wy, wz))
    if (glm::length2(m_angularVelocity) > kEpsilon) {
        glm::quat angularVelQuat(0.0f, m_angularVelocity.x, m_angularVelocity.y, m_angularVelocity.z);
        glm::quat spin = angularVelQuat * m_rotation * 0.5f;
        m_rotation = glm::normalize(m_rotation + spin * dt);
    }

    m_aabbDirty = true;

    // Update SDF collider position if exists
    if (m_sdfCollider) {
        // This is a simplified update - full update would recreate collider
        // For now, we'll mark it as needing update
    }
}

void RigidBody::ApplyConstraints() {
    // Apply linear motion constraints
    m_linearVelocity *= m_linearFactor;

    // Apply angular motion constraints (in local space)
    if (m_angularFactor != glm::vec3(1.0f)) {
        glm::vec3 localAngular = glm::inverse(m_rotation) * m_angularVelocity;
        localAngular *= m_angularFactor;
        m_angularVelocity = m_rotation * localAngular;
    }
}

void RigidBody::OnContactBegin(RigidBody& other, const RigidBodyContactManifold& manifold) {
    if (m_onContactBegin) {
        m_onContactBegin(other, manifold);
    }
}

void RigidBody::OnContactEnd(RigidBody& other, const RigidBodyContactManifold& manifold) {
    if (m_onContactEnd) {
        m_onContactEnd(other, manifold);
    }
}

// =============================================================================
// PointToPointConstraint Implementation
// =============================================================================

PointToPointConstraint::PointToPointConstraint(RigidBody* bodyA, RigidBody* bodyB,
                                                const glm::vec3& pivotA, const glm::vec3& pivotB)
    : m_bodyA(bodyA), m_bodyB(bodyB), m_pivotA(pivotA), m_pivotB(pivotB) {
}

void PointToPointConstraint::PrepareForSolve(float dt) {
    if (!m_bodyA) return;

    // Calculate world-space attachment points
    glm::vec3 worldPivotA = m_bodyA->TransformPoint(m_pivotA);
    glm::vec3 worldPivotB = m_bodyB ? m_bodyB->TransformPoint(m_pivotB) : m_pivotB;

    // Radius vectors from center of mass to attachment points
    m_rA = worldPivotA - m_bodyA->GetPosition();
    m_rB = m_bodyB ? (worldPivotB - m_bodyB->GetPosition()) : glm::vec3(0.0f);

    // Calculate effective mass matrix
    glm::mat3 K = glm::mat3(0.0f);

    // Contribution from body A
    if (m_bodyA->IsDynamic()) {
        glm::mat3 skewA = glm::mat3(
            0.0f, -m_rA.z, m_rA.y,
            m_rA.z, 0.0f, -m_rA.x,
            -m_rA.y, m_rA.x, 0.0f
        );
        K += glm::mat3(m_bodyA->GetInverseMass()) - skewA * m_bodyA->GetWorldInverseInertiaTensor() * skewA;
    }

    // Contribution from body B
    if (m_bodyB && m_bodyB->IsDynamic()) {
        glm::mat3 skewB = glm::mat3(
            0.0f, -m_rB.z, m_rB.y,
            m_rB.z, 0.0f, -m_rB.x,
            -m_rB.y, m_rB.x, 0.0f
        );
        K += glm::mat3(m_bodyB->GetInverseMass()) - skewB * m_bodyB->GetWorldInverseInertiaTensor() * skewB;
    }

    m_effectiveMass = glm::inverse(K);

    // Calculate position error for bias
    glm::vec3 error = worldPivotB - worldPivotA;
    float baumgarte = 0.2f; // Position correction factor
    m_bias = baumgarte / dt * error;

    // Clear accumulated impulse for this step
    m_accumulatedImpulse = glm::vec3(0.0f);
}

void PointToPointConstraint::SolveVelocity() {
    if (!m_bodyA) return;

    // Calculate relative velocity at the constraint points
    glm::vec3 velA = m_bodyA->GetLinearVelocity() + glm::cross(m_bodyA->GetAngularVelocity(), m_rA);
    glm::vec3 velB = m_bodyB ?
        (m_bodyB->GetLinearVelocity() + glm::cross(m_bodyB->GetAngularVelocity(), m_rB)) :
        glm::vec3(0.0f);

    glm::vec3 relVel = velB - velA;

    // Calculate impulse
    glm::vec3 impulse = m_effectiveMass * (-relVel + m_bias);

    m_accumulatedImpulse += impulse;

    // Apply impulse
    if (m_bodyA->IsDynamic()) {
        m_bodyA->AddImpulse(-impulse);
        m_bodyA->SetAngularVelocity(
            m_bodyA->GetAngularVelocity() -
            m_bodyA->GetWorldInverseInertiaTensor() * glm::cross(m_rA, impulse)
        );
    }

    if (m_bodyB && m_bodyB->IsDynamic()) {
        m_bodyB->AddImpulse(impulse);
        m_bodyB->SetAngularVelocity(
            m_bodyB->GetAngularVelocity() +
            m_bodyB->GetWorldInverseInertiaTensor() * glm::cross(m_rB, impulse)
        );
    }
}

void PointToPointConstraint::SolvePosition() {
    if (!m_bodyA) return;

    // Calculate world-space attachment points
    glm::vec3 worldPivotA = m_bodyA->TransformPoint(m_pivotA);
    glm::vec3 worldPivotB = m_bodyB ? m_bodyB->TransformPoint(m_pivotB) : m_pivotB;

    glm::vec3 error = worldPivotB - worldPivotA;
    float errorMagnitude = glm::length(error);

    if (errorMagnitude < kEpsilon) return;

    // Apply position correction
    float baumgarte = 0.2f;
    glm::vec3 correction = m_effectiveMass * error * baumgarte;

    if (m_bodyA->IsDynamic()) {
        m_bodyA->SetPosition(m_bodyA->GetPosition() - correction * m_bodyA->GetInverseMass());
    }

    if (m_bodyB && m_bodyB->IsDynamic()) {
        m_bodyB->SetPosition(m_bodyB->GetPosition() + correction * m_bodyB->GetInverseMass());
    }
}

// =============================================================================
// HingeConstraint Implementation
// =============================================================================

HingeConstraint::HingeConstraint(RigidBody* bodyA, RigidBody* bodyB,
                                  const glm::vec3& pivotA, const glm::vec3& pivotB,
                                  const glm::vec3& axisA, const glm::vec3& axisB)
    : m_bodyA(bodyA), m_bodyB(bodyB)
    , m_pivotA(pivotA), m_pivotB(pivotB)
    , m_axisA(glm::normalize(axisA)), m_axisB(glm::normalize(axisB)) {
}

void HingeConstraint::PrepareForSolve(float /*dt*/) {
    // Hinge constraint preparation
    // Full implementation would set up the constraint matrices
}

void HingeConstraint::SolveVelocity() {
    if (!m_bodyA) return;

    // Point-to-point constraint for the pivot
    // Plus angular constraints to align axes

    // Simplified: only enforce point constraint
    glm::vec3 worldPivotA = m_bodyA->TransformPoint(m_pivotA);
    glm::vec3 worldPivotB = m_bodyB ? m_bodyB->TransformPoint(m_pivotB) : m_pivotB;

    glm::vec3 rA = worldPivotA - m_bodyA->GetPosition();
    glm::vec3 rB = m_bodyB ? (worldPivotB - m_bodyB->GetPosition()) : glm::vec3(0.0f);

    glm::vec3 velA = m_bodyA->GetLinearVelocity() + glm::cross(m_bodyA->GetAngularVelocity(), rA);
    glm::vec3 velB = m_bodyB ?
        (m_bodyB->GetLinearVelocity() + glm::cross(m_bodyB->GetAngularVelocity(), rB)) :
        glm::vec3(0.0f);

    glm::vec3 relVel = velB - velA;

    // Calculate effective mass (simplified scalar)
    float invMassSum = m_bodyA->GetInverseMass();
    if (m_bodyB) invMassSum += m_bodyB->GetInverseMass();

    if (invMassSum < kEpsilon) return;

    glm::vec3 impulse = relVel / invMassSum;

    if (m_bodyA->IsDynamic()) {
        m_bodyA->AddImpulse(-impulse);
    }
    if (m_bodyB && m_bodyB->IsDynamic()) {
        m_bodyB->AddImpulse(impulse);
    }

    // Motor
    if (m_useMotor && m_motorMaxTorque > 0.0f) {
        glm::vec3 worldAxisA = m_bodyA->TransformDirection(m_axisA);
        glm::vec3 angVelA = m_bodyA->GetAngularVelocity();
        glm::vec3 angVelB = m_bodyB ? m_bodyB->GetAngularVelocity() : glm::vec3(0.0f);

        float relAngVel = glm::dot(angVelB - angVelA, worldAxisA);
        float motorError = m_motorTargetVelocity - relAngVel;

        float motorImpulse = glm::clamp(motorError * 0.1f, -m_motorMaxTorque, m_motorMaxTorque);
        glm::vec3 torqueImpulse = worldAxisA * motorImpulse;

        if (m_bodyA->IsDynamic()) {
            m_bodyA->SetAngularVelocity(
                m_bodyA->GetAngularVelocity() -
                m_bodyA->GetWorldInverseInertiaTensor() * torqueImpulse
            );
        }
        if (m_bodyB && m_bodyB->IsDynamic()) {
            m_bodyB->SetAngularVelocity(
                m_bodyB->GetAngularVelocity() +
                m_bodyB->GetWorldInverseInertiaTensor() * torqueImpulse
            );
        }
    }
}

void HingeConstraint::SolvePosition() {
    // Similar to point-to-point position solve
}

void HingeConstraint::SetLimits(float lower, float upper) {
    m_lowerLimit = lower;
    m_upperLimit = upper;
}

void HingeConstraint::SetMotor(float targetVelocity, float maxTorque) {
    m_motorTargetVelocity = targetVelocity;
    m_motorMaxTorque = maxTorque;
}

} // namespace Nova
