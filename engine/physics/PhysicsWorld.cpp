#include "PhysicsWorld.hpp"
#include "../graphics/debug/DebugDraw.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace Nova {

namespace {

constexpr float kEpsilon = 1e-6f;

// Closest point on line segment
glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 ab = b - a;
    float t = glm::dot(point - a, ab);
    if (t <= 0.0f) return a;

    float denom = glm::dot(ab, ab);
    if (t >= denom) return b;

    return a + (t / denom) * ab;
}

// Closest points between two line segments
void ClosestPointsOnSegments(
    const glm::vec3& a1, const glm::vec3& a2,
    const glm::vec3& b1, const glm::vec3& b2,
    glm::vec3& closestA, glm::vec3& closestB)
{
    glm::vec3 d1 = a2 - a1;
    glm::vec3 d2 = b2 - b1;
    glm::vec3 r = a1 - b1;

    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s, t;

    if (a <= kEpsilon && e <= kEpsilon) {
        s = t = 0.0f;
    } else if (a <= kEpsilon) {
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(d1, r);
        if (e <= kEpsilon) {
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;

            if (denom != 0.0f) {
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }

            t = (b * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    closestA = a1 + s * d1;
    closestB = b1 + t * d2;
}

} // anonymous namespace

// ============================================================================
// PhysicsWorld
// ============================================================================

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(const PhysicsWorldConfig& config) : m_config(config) {
}

PhysicsWorld::~PhysicsWorld() {
    Clear();
}

void PhysicsWorld::Step(float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_accumulator += deltaTime;

    int steps = 0;
    while (m_accumulator >= m_config.fixedTimestep && steps < m_config.maxSubSteps) {
        FixedStep();
        m_accumulator -= m_config.fixedTimestep;
        ++steps;
    }

    // Clamp accumulator to prevent spiral of death
    if (m_accumulator > m_config.fixedTimestep * m_config.maxSubSteps) {
        m_accumulator = m_config.fixedTimestep;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.stepTime = std::chrono::duration<float>(endTime - startTime).count() * 1000.0f;
}

void PhysicsWorld::FixedStep() {
    const float dt = m_config.fixedTimestep;

    // Store previous contacts for enter/exit detection
    m_previousContacts = m_activeContacts;
    m_activeContacts.clear();

    // Reset statistics
    m_stats.narrowPhaseTests = 0;
    m_stats.contactCount = 0;

    // Integration and collision
    IntegrateForces(dt);
    BroadPhase();
    NarrowPhase();

    // Solve constraints
    for (int i = 0; i < m_config.velocityIterations; ++i) {
        ResolveCollisions(dt);
    }

    IntegrateVelocities(dt);
    UpdateSleepStates(dt);

    // Fire collision callbacks
    for (const auto& pair : m_activeContacts) {
        CollisionBody* bodyA = GetBody(pair.bodyA);
        CollisionBody* bodyB = GetBody(pair.bodyB);
        if (!bodyA || !bodyB) continue;

        ContactInfo contact;
        contact.bodyA = bodyA;
        contact.bodyB = bodyB;

        if (m_previousContacts.find(pair) == m_previousContacts.end()) {
            // New contact - enter
            bodyA->OnCollisionEnter(*bodyB, contact);
            bodyB->OnCollisionEnter(*bodyA, contact);
        } else {
            // Existing contact - stay
            bodyA->OnCollisionStay(*bodyB, contact);
            bodyB->OnCollisionStay(*bodyA, contact);
        }
    }

    // Fire exit callbacks
    for (const auto& pair : m_previousContacts) {
        if (m_activeContacts.find(pair) == m_activeContacts.end()) {
            CollisionBody* bodyA = GetBody(pair.bodyA);
            CollisionBody* bodyB = GetBody(pair.bodyB);
            if (!bodyA || !bodyB) continue;

            ContactInfo contact;
            contact.bodyA = bodyA;
            contact.bodyB = bodyB;

            bodyA->OnCollisionExit(*bodyB, contact);
            bodyB->OnCollisionExit(*bodyA, contact);

            bodyA->RemoveContact(pair.bodyB);
            bodyB->RemoveContact(pair.bodyA);
        }
    }

    // Update stats
    m_stats.bodyCount = m_bodies.size();
    m_stats.activeBodyCount = 0;
    for (const auto& body : m_bodies) {
        if (body && !body->IsSleeping() && body->IsEnabled()) {
            ++m_stats.activeBodyCount;
        }
    }
}

void PhysicsWorld::IntegrateForces(float dt) {
    for (auto& body : m_bodies) {
        if (!body || !body->IsEnabled() || body->IsSleeping()) continue;
        if (body->GetBodyType() != BodyType::Dynamic) continue;

        // Apply gravity
        glm::vec3 gravity = m_config.gravity * body->GetGravityScale();
        body->ApplyForce(gravity * body->GetMass());

        // Integrate forces to velocity
        glm::vec3 linearAccel = body->GetAccumulatedForce() * body->GetInverseMass();
        body->SetLinearVelocity(body->GetLinearVelocity() + linearAccel * dt);

        glm::vec3 angularAccel = body->GetInverseInertiaTensor() * body->GetAccumulatedTorque();
        body->SetAngularVelocity(body->GetAngularVelocity() + angularAccel * dt);

        // Apply damping
        float linearDamping = std::pow(1.0f - body->GetLinearDamping(), dt);
        float angularDamping = std::pow(1.0f - body->GetAngularDamping(), dt);
        body->SetLinearVelocity(body->GetLinearVelocity() * linearDamping);
        body->SetAngularVelocity(body->GetAngularVelocity() * angularDamping);

        body->ClearForces();
    }
}

void PhysicsWorld::BroadPhase() {
    m_broadPhasePairs.clear();
    RebuildSpatialHash();

    std::unordered_set<CollisionPair, CollisionPairHash> testedPairs;

    for (auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;

        AABB aabb = body->GetWorldAABB();
        std::vector<glm::ivec3> cells;
        GetCellsForAABB(aabb, cells);

        for (const auto& cell : cells) {
            size_t hash = HashCell(cell);
            auto it = m_spatialHash.find(hash);
            if (it == m_spatialHash.end()) continue;

            for (auto otherId : it->second.bodies) {
                if (otherId == body->GetId()) continue;

                CollisionPair pair{body->GetId(), otherId};
                if (testedPairs.find(pair) != testedPairs.end()) continue;
                testedPairs.insert(pair);

                CollisionBody* other = GetBody(otherId);
                if (!other || !other->IsEnabled()) continue;

                // Skip if both static
                if (body->IsStatic() && other->IsStatic()) continue;

                // Skip if both sleeping
                if (body->IsSleeping() && other->IsSleeping()) continue;

                // Check layer filtering
                if (!body->ShouldCollideWith(*other)) continue;

                // AABB test
                if (aabb.Intersects(other->GetWorldAABB())) {
                    m_broadPhasePairs.push_back(pair);
                }
            }
        }
    }

    m_stats.broadPhasePairs = m_broadPhasePairs.size();
}

void PhysicsWorld::NarrowPhase() {
    for (const auto& pair : m_broadPhasePairs) {
        CollisionBody* bodyA = GetBody(pair.bodyA);
        CollisionBody* bodyB = GetBody(pair.bodyB);
        if (!bodyA || !bodyB) continue;

        ++m_stats.narrowPhaseTests;

        ContactInfo contact;
        if (TestCollision(*bodyA, *bodyB, contact)) {
            m_activeContacts.insert(pair);
            ++m_stats.contactCount;

            bodyA->AddContact(pair.bodyB);
            bodyB->AddContact(pair.bodyA);
        }
    }
}

void PhysicsWorld::ResolveCollisions(float dt) {
    for (const auto& pair : m_activeContacts) {
        CollisionBody* bodyA = GetBody(pair.bodyA);
        CollisionBody* bodyB = GetBody(pair.bodyB);
        if (!bodyA || !bodyB) continue;

        ContactInfo contact;
        if (TestCollision(*bodyA, *bodyB, contact)) {
            for (const auto& cp : contact.points) {
                ResolveContact(*bodyA, *bodyB, cp, dt);
            }
        }
    }
}

void PhysicsWorld::IntegrateVelocities(float dt) {
    for (auto& body : m_bodies) {
        if (!body || !body->IsEnabled() || body->IsSleeping()) continue;
        if (body->GetBodyType() == BodyType::Static) continue;

        // Integrate position
        body->SetPosition(body->GetPosition() + body->GetLinearVelocity() * dt);

        // Integrate rotation
        glm::vec3 angVel = body->GetAngularVelocity();
        if (glm::length2(angVel) > kEpsilon) {
            glm::quat spin = glm::quat(0.0f, angVel.x, angVel.y, angVel.z);
            glm::quat rot = body->GetRotation();
            rot = rot + (spin * rot) * (dt * 0.5f);
            body->SetRotation(glm::normalize(rot));
        }

        body->MarkBoundsDirty();
    }
}

void PhysicsWorld::UpdateSleepStates(float dt) {
    for (auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if (body->GetBodyType() != BodyType::Dynamic) continue;

        float linearSpeed2 = glm::length2(body->GetLinearVelocity());
        float angularSpeed2 = glm::length2(body->GetAngularVelocity());

        float linearThreshold2 = m_config.linearSleepThreshold * m_config.linearSleepThreshold;
        float angularThreshold2 = m_config.angularSleepThreshold * m_config.angularSleepThreshold;

        if (linearSpeed2 < linearThreshold2 && angularSpeed2 < angularThreshold2) {
            body->m_sleepTimer += dt;
            if (body->m_sleepTimer >= m_config.sleepTimeThreshold) {
                body->SetSleeping(true);
                body->SetLinearVelocity(glm::vec3(0.0f));
                body->SetAngularVelocity(glm::vec3(0.0f));
            }
        } else {
            body->m_sleepTimer = 0.0f;
        }
    }
}

void PhysicsWorld::RebuildSpatialHash() {
    m_spatialHash.clear();

    for (auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;

        AABB aabb = body->GetWorldAABB();
        std::vector<glm::ivec3> cells;
        GetCellsForAABB(aabb, cells);

        for (const auto& cell : cells) {
            size_t hash = HashCell(cell);
            m_spatialHash[hash].bodies.push_back(body->GetId());
        }
    }
}

void PhysicsWorld::GetCellsForAABB(const AABB& aabb, std::vector<glm::ivec3>& cells) const {
    cells.clear();

    glm::ivec3 minCell = GetCellCoord(aabb.min);
    glm::ivec3 maxCell = GetCellCoord(aabb.max);

    for (int x = minCell.x; x <= maxCell.x; ++x) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int z = minCell.z; z <= maxCell.z; ++z) {
                cells.emplace_back(x, y, z);
            }
        }
    }
}

glm::ivec3 PhysicsWorld::GetCellCoord(const glm::vec3& pos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(pos.x / m_config.cellSize)),
        static_cast<int>(std::floor(pos.y / m_config.cellSize)),
        static_cast<int>(std::floor(pos.z / m_config.cellSize))
    );
}

size_t PhysicsWorld::HashCell(const glm::ivec3& cell) const {
    // Simple hash combining
    size_t h = 0;
    h ^= std::hash<int>{}(cell.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(cell.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(cell.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

bool PhysicsWorld::TestCollision(CollisionBody& bodyA, CollisionBody& bodyB, ContactInfo& contact) const {
    contact.bodyA = &bodyA;
    contact.bodyB = &bodyB;
    contact.points.clear();

    glm::mat4 transformA = bodyA.GetTransformMatrix();
    glm::mat4 transformB = bodyB.GetTransformMatrix();

    for (size_t i = 0; i < bodyA.GetShapeCount(); ++i) {
        const CollisionShape& shapeA = bodyA.GetShape(i);

        for (size_t j = 0; j < bodyB.GetShapeCount(); ++j) {
            const CollisionShape& shapeB = bodyB.GetShape(j);

            // Skip trigger-trigger collisions for physics
            if (shapeA.IsTrigger() && shapeB.IsTrigger()) continue;

            std::vector<ContactPoint> shapeContacts;
            if (TestShapeCollision(shapeA, transformA, shapeB, transformB, shapeContacts)) {
                for (auto& cp : shapeContacts) {
                    cp.shapeIndexA = static_cast<int>(i);
                    cp.shapeIndexB = static_cast<int>(j);
                    contact.points.push_back(cp);
                }
            }
        }
    }

    return !contact.points.empty();
}

bool PhysicsWorld::TestShapeCollision(
    const CollisionShape& shapeA, const glm::mat4& transformA,
    const CollisionShape& shapeB, const glm::mat4& transformB,
    std::vector<ContactPoint>& contacts) const
{
    ShapeType typeA = shapeA.GetType();
    ShapeType typeB = shapeB.GetType();

    // Get world-space representations
    OBB obbA = shapeA.ComputeWorldOBB(transformA);
    OBB obbB = shapeB.ComputeWorldOBB(transformB);

    // Sphere-Sphere
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Sphere) {
        auto* paramsA = shapeA.GetParams<ShapeParams::Sphere>();
        auto* paramsB = shapeB.GetParams<ShapeParams::Sphere>();
        if (!paramsA || !paramsB) return false;

        ContactPoint cp;
        if (TestSphereSphere(obbA.center, paramsA->radius, obbB.center, paramsB->radius, cp)) {
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Sphere-Box
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Box) {
        auto* params = shapeA.GetParams<ShapeParams::Sphere>();
        if (!params) return false;

        ContactPoint cp;
        if (TestSphereBox(obbA.center, params->radius, obbB, cp)) {
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Box-Sphere (swap)
    if (typeA == ShapeType::Box && typeB == ShapeType::Sphere) {
        auto* params = shapeB.GetParams<ShapeParams::Sphere>();
        if (!params) return false;

        ContactPoint cp;
        if (TestSphereBox(obbB.center, params->radius, obbA, cp)) {
            cp.normal = -cp.normal;  // Flip normal
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Box-Box
    if (typeA == ShapeType::Box && typeB == ShapeType::Box) {
        return TestBoxBox(obbA, obbB, contacts);
    }

    // Capsule-Capsule
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Capsule) {
        auto* paramsA = shapeA.GetParams<ShapeParams::Capsule>();
        auto* paramsB = shapeB.GetParams<ShapeParams::Capsule>();
        if (!paramsA || !paramsB) return false;

        // Get capsule endpoints in world space
        glm::vec3 axisA = glm::vec3(0, paramsA->height * 0.5f, 0);
        glm::vec3 startA = obbA.center - obbA.orientation * axisA;
        glm::vec3 endA = obbA.center + obbA.orientation * axisA;

        glm::vec3 axisB = glm::vec3(0, paramsB->height * 0.5f, 0);
        glm::vec3 startB = obbB.center - obbB.orientation * axisB;
        glm::vec3 endB = obbB.center + obbB.orientation * axisB;

        ContactPoint cp;
        if (TestCapsuleCapsule(startA, endA, paramsA->radius, startB, endB, paramsB->radius, cp)) {
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Sphere-Capsule
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Capsule) {
        auto* sphereParams = shapeA.GetParams<ShapeParams::Sphere>();
        auto* capsuleParams = shapeB.GetParams<ShapeParams::Capsule>();
        if (!sphereParams || !capsuleParams) return false;

        glm::vec3 axis = glm::vec3(0, capsuleParams->height * 0.5f, 0);
        glm::vec3 start = obbB.center - obbB.orientation * axis;
        glm::vec3 end = obbB.center + obbB.orientation * axis;

        ContactPoint cp;
        if (TestSphereCapsule(obbA.center, sphereParams->radius, start, end, capsuleParams->radius, cp)) {
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Capsule-Sphere (swap)
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Sphere) {
        auto* capsuleParams = shapeA.GetParams<ShapeParams::Capsule>();
        auto* sphereParams = shapeB.GetParams<ShapeParams::Sphere>();
        if (!capsuleParams || !sphereParams) return false;

        glm::vec3 axis = glm::vec3(0, capsuleParams->height * 0.5f, 0);
        glm::vec3 start = obbA.center - obbA.orientation * axis;
        glm::vec3 end = obbA.center + obbA.orientation * axis;

        ContactPoint cp;
        if (TestSphereCapsule(obbB.center, sphereParams->radius, start, end, capsuleParams->radius, cp)) {
            cp.normal = -cp.normal;
            contacts.push_back(cp);
            return true;
        }
        return false;
    }

    // Fallback: Use OBB test for other shape combinations
    if (obbA.Intersects(obbB)) {
        // Generate approximate contact point at closest points
        ContactPoint cp;
        cp.position = (obbA.center + obbB.center) * 0.5f;
        cp.normal = glm::normalize(obbB.center - obbA.center);
        if (glm::length2(cp.normal) < kEpsilon) {
            cp.normal = glm::vec3(0, 1, 0);
        }
        cp.penetration = 0.01f;  // Approximate
        contacts.push_back(cp);
        return true;
    }

    return false;
}

bool PhysicsWorld::TestSphereSphere(
    const glm::vec3& centerA, float radiusA,
    const glm::vec3& centerB, float radiusB,
    ContactPoint& contact) const
{
    glm::vec3 delta = centerB - centerA;
    float dist2 = glm::dot(delta, delta);
    float radiusSum = radiusA + radiusB;

    if (dist2 > radiusSum * radiusSum) {
        return false;
    }

    float dist = std::sqrt(dist2);
    if (dist < kEpsilon) {
        contact.normal = glm::vec3(0, 1, 0);
        contact.penetration = radiusSum;
    } else {
        contact.normal = delta / dist;
        contact.penetration = radiusSum - dist;
    }

    contact.position = centerA + contact.normal * radiusA;
    return true;
}

bool PhysicsWorld::TestSphereBox(
    const glm::vec3& sphereCenter, float sphereRadius,
    const OBB& box,
    ContactPoint& contact) const
{
    glm::vec3 closest = box.ClosestPoint(sphereCenter);
    glm::vec3 delta = sphereCenter - closest;
    float dist2 = glm::dot(delta, delta);

    if (dist2 > sphereRadius * sphereRadius) {
        return false;
    }

    float dist = std::sqrt(dist2);
    if (dist < kEpsilon) {
        // Sphere center inside box
        contact.normal = glm::vec3(0, 1, 0);
        contact.penetration = sphereRadius;
    } else {
        contact.normal = delta / dist;
        contact.penetration = sphereRadius - dist;
    }

    contact.position = closest;
    return true;
}

bool PhysicsWorld::TestBoxBox(
    const OBB& boxA, const OBB& boxB,
    std::vector<ContactPoint>& contacts) const
{
    if (!boxA.Intersects(boxB)) {
        return false;
    }

    // Generate contact point at overlap center
    ContactPoint cp;
    cp.position = (boxA.center + boxB.center) * 0.5f;
    cp.normal = glm::normalize(boxB.center - boxA.center);
    if (glm::length2(cp.normal) < kEpsilon) {
        cp.normal = glm::vec3(0, 1, 0);
    }

    // Estimate penetration from overlap
    AABB aabbA = boxA.GetAABB();
    AABB aabbB = boxB.GetAABB();
    glm::vec3 overlap(
        std::min(aabbA.max.x, aabbB.max.x) - std::max(aabbA.min.x, aabbB.min.x),
        std::min(aabbA.max.y, aabbB.max.y) - std::max(aabbA.min.y, aabbB.min.y),
        std::min(aabbA.max.z, aabbB.max.z) - std::max(aabbA.min.z, aabbB.min.z)
    );
    cp.penetration = std::min({overlap.x, overlap.y, overlap.z});

    contacts.push_back(cp);
    return true;
}

bool PhysicsWorld::TestCapsuleCapsule(
    const glm::vec3& startA, const glm::vec3& endA, float radiusA,
    const glm::vec3& startB, const glm::vec3& endB, float radiusB,
    ContactPoint& contact) const
{
    glm::vec3 closestA, closestB;
    ClosestPointsOnSegments(startA, endA, startB, endB, closestA, closestB);

    glm::vec3 delta = closestB - closestA;
    float dist2 = glm::dot(delta, delta);
    float radiusSum = radiusA + radiusB;

    if (dist2 > radiusSum * radiusSum) {
        return false;
    }

    float dist = std::sqrt(dist2);
    if (dist < kEpsilon) {
        contact.normal = glm::vec3(0, 1, 0);
        contact.penetration = radiusSum;
    } else {
        contact.normal = delta / dist;
        contact.penetration = radiusSum - dist;
    }

    contact.position = closestA + contact.normal * radiusA;
    return true;
}

bool PhysicsWorld::TestSphereCapsule(
    const glm::vec3& sphereCenter, float sphereRadius,
    const glm::vec3& capsuleStart, const glm::vec3& capsuleEnd, float capsuleRadius,
    ContactPoint& contact) const
{
    glm::vec3 closest = ClosestPointOnSegment(sphereCenter, capsuleStart, capsuleEnd);
    glm::vec3 delta = sphereCenter - closest;
    float dist2 = glm::dot(delta, delta);
    float radiusSum = sphereRadius + capsuleRadius;

    if (dist2 > radiusSum * radiusSum) {
        return false;
    }

    float dist = std::sqrt(dist2);
    if (dist < kEpsilon) {
        contact.normal = glm::vec3(0, 1, 0);
        contact.penetration = radiusSum;
    } else {
        contact.normal = delta / dist;
        contact.penetration = radiusSum - dist;
    }

    contact.position = closest + contact.normal * capsuleRadius;
    return true;
}

void PhysicsWorld::ResolveContact(CollisionBody& bodyA, CollisionBody& bodyB,
                                   const ContactPoint& contact, float dt)
{
    // Skip if both are non-dynamic
    if (bodyA.GetBodyType() != BodyType::Dynamic &&
        bodyB.GetBodyType() != BodyType::Dynamic) {
        return;
    }

    // Get inverse masses
    float invMassA = bodyA.GetInverseMass();
    float invMassB = bodyB.GetInverseMass();
    float invMassSum = invMassA + invMassB;

    if (invMassSum < kEpsilon) return;

    // Calculate relative velocity at contact point
    glm::vec3 rA = contact.position - bodyA.GetPosition();
    glm::vec3 rB = contact.position - bodyB.GetPosition();

    glm::vec3 velA = bodyA.GetLinearVelocity() + glm::cross(bodyA.GetAngularVelocity(), rA);
    glm::vec3 velB = bodyB.GetLinearVelocity() + glm::cross(bodyB.GetAngularVelocity(), rB);
    glm::vec3 relVel = velB - velA;

    float velAlongNormal = glm::dot(relVel, contact.normal);

    // Don't resolve if separating
    if (velAlongNormal > 0) return;

    // Get combined restitution
    float restitution = 0.0f;
    if (bodyA.GetShapeCount() > 0 && bodyB.GetShapeCount() > 0) {
        restitution = std::min(
            bodyA.GetShape(0).GetMaterial().restitution,
            bodyB.GetShape(0).GetMaterial().restitution
        );
    }

    // Calculate impulse scalar
    float j = -(1.0f + restitution) * velAlongNormal;

    // Include rotational effect
    glm::vec3 crossA = glm::cross(rA, contact.normal);
    glm::vec3 crossB = glm::cross(rB, contact.normal);

    float angularEffectA = glm::dot(crossA, bodyA.GetInverseInertiaTensor() * crossA);
    float angularEffectB = glm::dot(crossB, bodyB.GetInverseInertiaTensor() * crossB);

    j /= invMassSum + angularEffectA + angularEffectB;

    // Apply impulse
    glm::vec3 impulse = j * contact.normal;

    if (bodyA.GetBodyType() == BodyType::Dynamic) {
        bodyA.ApplyImpulseAtPoint(-impulse, contact.position);
    }
    if (bodyB.GetBodyType() == BodyType::Dynamic) {
        bodyB.ApplyImpulseAtPoint(impulse, contact.position);
    }

    // Position correction (Baumgarte stabilization)
    float slop = m_config.allowedPenetration;
    float correction = std::max(contact.penetration - slop, 0.0f) * m_config.baumgarte;
    glm::vec3 correctionVec = correction / invMassSum * contact.normal;

    if (bodyA.GetBodyType() == BodyType::Dynamic) {
        bodyA.SetPosition(bodyA.GetPosition() - correctionVec * invMassA);
    }
    if (bodyB.GetBodyType() == BodyType::Dynamic) {
        bodyB.SetPosition(bodyB.GetPosition() + correctionVec * invMassB);
    }

    // Friction impulse
    glm::vec3 tangent = relVel - velAlongNormal * contact.normal;
    if (glm::length2(tangent) > kEpsilon) {
        tangent = glm::normalize(tangent);

        float friction = 0.5f;
        if (bodyA.GetShapeCount() > 0 && bodyB.GetShapeCount() > 0) {
            friction = std::sqrt(
                bodyA.GetShape(0).GetMaterial().friction *
                bodyB.GetShape(0).GetMaterial().friction
            );
        }

        float jt = -glm::dot(relVel, tangent);
        jt /= invMassSum;

        // Coulomb friction
        glm::vec3 frictionImpulse;
        if (std::abs(jt) < j * friction) {
            frictionImpulse = jt * tangent;
        } else {
            frictionImpulse = -j * friction * tangent;
        }

        if (bodyA.GetBodyType() == BodyType::Dynamic) {
            bodyA.ApplyImpulse(-frictionImpulse * invMassA);
        }
        if (bodyB.GetBodyType() == BodyType::Dynamic) {
            bodyB.ApplyImpulse(frictionImpulse * invMassB);
        }
    }
}

CollisionBody* PhysicsWorld::AddBody(std::unique_ptr<CollisionBody> body) {
    if (!body) return nullptr;

    CollisionBody* ptr = body.get();
    m_bodyMap[ptr->GetId()] = ptr;
    m_bodies.push_back(std::move(body));
    return ptr;
}

CollisionBody* PhysicsWorld::CreateBody(BodyType type) {
    auto body = std::make_unique<CollisionBody>(type);
    return AddBody(std::move(body));
}

void PhysicsWorld::RemoveBody(CollisionBody* body) {
    if (!body) return;
    RemoveBody(body->GetId());
}

void PhysicsWorld::RemoveBody(CollisionBody::BodyId id) {
    m_bodyMap.erase(id);
    m_bodies.erase(
        std::remove_if(m_bodies.begin(), m_bodies.end(),
            [id](const std::unique_ptr<CollisionBody>& b) {
                return b && b->GetId() == id;
            }),
        m_bodies.end()
    );
}

CollisionBody* PhysicsWorld::GetBody(CollisionBody::BodyId id) {
    auto it = m_bodyMap.find(id);
    return (it != m_bodyMap.end()) ? it->second : nullptr;
}

const CollisionBody* PhysicsWorld::GetBody(CollisionBody::BodyId id) const {
    auto it = m_bodyMap.find(id);
    return (it != m_bodyMap.end()) ? it->second : nullptr;
}

void PhysicsWorld::Clear() {
    m_bodies.clear();
    m_bodyMap.clear();
    m_spatialHash.clear();
    m_activeContacts.clear();
    m_previousContacts.clear();
    m_broadPhasePairs.clear();
}

std::optional<RaycastHit> PhysicsWorld::Raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    uint32_t layerMask) const
{
    auto hits = RaycastAll(origin, direction, maxDistance, layerMask);
    if (hits.empty()) return std::nullopt;

    std::sort(hits.begin(), hits.end());
    return hits.front();
}

std::vector<RaycastHit> PhysicsWorld::RaycastAll(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    uint32_t layerMask) const
{
    std::vector<RaycastHit> hits;
    glm::vec3 dir = glm::normalize(direction);

    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        RaycastHit hit;
        if (RaycastBody(origin, dir, maxDistance, *body, hit)) {
            hits.push_back(hit);
        }
    }

    return hits;
}

bool PhysicsWorld::RaycastAny(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    uint32_t layerMask) const
{
    glm::vec3 dir = glm::normalize(direction);

    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        RaycastHit hit;
        if (RaycastBody(origin, dir, maxDistance, *body, hit)) {
            return true;
        }
    }

    return false;
}

bool PhysicsWorld::RaycastBody(
    const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
    const CollisionBody& body, RaycastHit& hit) const
{
    // Quick AABB test first
    AABB aabb = body.GetWorldAABB();
    glm::vec3 invDir = 1.0f / direction;

    float t1 = (aabb.min.x - origin.x) * invDir.x;
    float t2 = (aabb.max.x - origin.x) * invDir.x;
    float t3 = (aabb.min.y - origin.y) * invDir.y;
    float t4 = (aabb.max.y - origin.y) * invDir.y;
    float t5 = (aabb.min.z - origin.z) * invDir.z;
    float t6 = (aabb.max.z - origin.z) * invDir.z;

    float tmin = std::max({std::min(t1, t2), std::min(t3, t4), std::min(t5, t6)});
    float tmax = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    if (tmax < 0 || tmin > tmax || tmin > maxDistance) {
        return false;
    }

    // Test individual shapes
    glm::mat4 transform = body.GetTransformMatrix();
    float closestDist = maxDistance;
    bool found = false;

    for (size_t i = 0; i < body.GetShapeCount(); ++i) {
        const CollisionShape& shape = body.GetShape(i);
        if (shape.IsTrigger()) continue;

        float dist;
        glm::vec3 normal;
        if (RaycastShape(origin, direction, closestDist, shape, transform, dist, normal)) {
            closestDist = dist;
            hit.body = const_cast<CollisionBody*>(&body);
            hit.shapeIndex = static_cast<int>(i);
            hit.distance = dist;
            hit.point = origin + direction * dist;
            hit.normal = normal;
            found = true;
        }
    }

    return found;
}

bool PhysicsWorld::RaycastShape(
    const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
    const CollisionShape& shape, const glm::mat4& transform,
    float& hitDistance, glm::vec3& hitNormal) const
{
    OBB obb = shape.ComputeWorldOBB(transform);

    // Transform ray to OBB local space
    glm::mat3 rot = glm::mat3_cast(obb.orientation);
    glm::mat3 invRot = glm::transpose(rot);
    glm::vec3 localOrigin = invRot * (origin - obb.center);
    glm::vec3 localDir = invRot * direction;

    // Ray-AABB in local space
    glm::vec3 invDir = 1.0f / localDir;

    float t1 = (-obb.halfExtents.x - localOrigin.x) * invDir.x;
    float t2 = (obb.halfExtents.x - localOrigin.x) * invDir.x;
    float t3 = (-obb.halfExtents.y - localOrigin.y) * invDir.y;
    float t4 = (obb.halfExtents.y - localOrigin.y) * invDir.y;
    float t5 = (-obb.halfExtents.z - localOrigin.z) * invDir.z;
    float t6 = (obb.halfExtents.z - localOrigin.z) * invDir.z;

    float tmin = std::max({std::min(t1, t2), std::min(t3, t4), std::min(t5, t6)});
    float tmax = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    if (tmax < 0 || tmin > tmax || tmin > maxDistance) {
        return false;
    }

    hitDistance = tmin >= 0 ? tmin : tmax;

    // Calculate normal
    glm::vec3 hitPoint = localOrigin + localDir * hitDistance;
    glm::vec3 localNormal(0);

    float minDiff = FLT_MAX;
    for (int i = 0; i < 3; ++i) {
        float diff = std::abs(std::abs(hitPoint[i]) - obb.halfExtents[i]);
        if (diff < minDiff) {
            minDiff = diff;
            localNormal = glm::vec3(0);
            localNormal[i] = (hitPoint[i] > 0) ? 1.0f : -1.0f;
        }
    }

    hitNormal = rot * localNormal;
    return true;
}

std::optional<ShapeCastHit> PhysicsWorld::SphereCast(
    const glm::vec3& origin,
    float radius,
    const glm::vec3& direction,
    float maxDistance,
    uint32_t layerMask) const
{
    // Simple implementation: test at discrete intervals
    glm::vec3 dir = glm::normalize(direction);
    float stepSize = radius * 0.5f;
    int steps = static_cast<int>(maxDistance / stepSize);

    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        glm::vec3 pos = origin + dir * (t * maxDistance);

        auto overlaps = OverlapSphere(pos, radius, layerMask);
        if (!overlaps.empty()) {
            ShapeCastHit hit;
            hit.body = overlaps[0].body;
            hit.shapeIndex = overlaps[0].shapeIndex;
            hit.point = pos;
            hit.fraction = t;
            hit.normal = glm::normalize(pos - hit.body->GetPosition());
            return hit;
        }
    }

    return std::nullopt;
}

std::optional<ShapeCastHit> PhysicsWorld::BoxCast(
    const glm::vec3& origin,
    const glm::vec3& halfExtents,
    const glm::quat& orientation,
    const glm::vec3& direction,
    float maxDistance,
    uint32_t layerMask) const
{
    glm::vec3 dir = glm::normalize(direction);
    float stepSize = std::min({halfExtents.x, halfExtents.y, halfExtents.z}) * 0.5f;
    int steps = static_cast<int>(maxDistance / stepSize);

    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        glm::vec3 pos = origin + dir * (t * maxDistance);

        auto overlaps = OverlapBox(pos, halfExtents, orientation, layerMask);
        if (!overlaps.empty()) {
            ShapeCastHit hit;
            hit.body = overlaps[0].body;
            hit.shapeIndex = overlaps[0].shapeIndex;
            hit.point = pos;
            hit.fraction = t;
            hit.normal = glm::normalize(pos - hit.body->GetPosition());
            return hit;
        }
    }

    return std::nullopt;
}

std::vector<OverlapResult> PhysicsWorld::OverlapSphere(
    const glm::vec3& center,
    float radius,
    uint32_t layerMask) const
{
    std::vector<OverlapResult> results;

    AABB queryAABB = AABB::FromCenterExtents(center, glm::vec3(radius));

    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        if (!queryAABB.Intersects(body->GetWorldAABB())) continue;

        // Test shapes
        glm::mat4 transform = body->GetTransformMatrix();
        for (size_t i = 0; i < body->GetShapeCount(); ++i) {
            const CollisionShape& shape = body->GetShape(i);
            OBB obb = shape.ComputeWorldOBB(transform);

            glm::vec3 closest = obb.ClosestPoint(center);
            if (glm::distance2(closest, center) <= radius * radius) {
                results.push_back({body.get(), static_cast<int>(i)});
                break;
            }
        }
    }

    return results;
}

std::vector<OverlapResult> PhysicsWorld::OverlapBox(
    const glm::vec3& center,
    const glm::vec3& halfExtents,
    const glm::quat& orientation,
    uint32_t layerMask) const
{
    std::vector<OverlapResult> results;

    OBB queryOBB{center, halfExtents, orientation};
    AABB queryAABB = queryOBB.GetAABB();

    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        if (!queryAABB.Intersects(body->GetWorldAABB())) continue;

        glm::mat4 transform = body->GetTransformMatrix();
        for (size_t i = 0; i < body->GetShapeCount(); ++i) {
            const CollisionShape& shape = body->GetShape(i);
            OBB obb = shape.ComputeWorldOBB(transform);

            if (queryOBB.Intersects(obb)) {
                results.push_back({body.get(), static_cast<int>(i)});
                break;
            }
        }
    }

    return results;
}

std::vector<OverlapResult> PhysicsWorld::OverlapAABB(
    const AABB& aabb,
    uint32_t layerMask) const
{
    std::vector<OverlapResult> results;

    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        if (aabb.Intersects(body->GetWorldAABB())) {
            results.push_back({body.get(), -1});
        }
    }

    return results;
}

CollisionBody* PhysicsWorld::PointQuery(
    const glm::vec3& point,
    uint32_t layerMask) const
{
    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;
        if ((body->GetCollisionLayer() & layerMask) == 0) continue;

        if (!body->GetWorldAABB().Contains(point)) continue;

        glm::mat4 transform = body->GetTransformMatrix();
        for (size_t i = 0; i < body->GetShapeCount(); ++i) {
            const CollisionShape& shape = body->GetShape(i);
            OBB obb = shape.ComputeWorldOBB(transform);

            if (obb.Contains(point)) {
                return body.get();
            }
        }
    }

    return nullptr;
}

void PhysicsWorld::DebugRender() {
    if (!m_debugDrawEnabled || !m_debugDraw) return;

    // Draw body shapes
    for (const auto& body : m_bodies) {
        if (!body || !body->IsEnabled()) continue;

        glm::vec4 color(0.0f, 1.0f, 0.0f, 1.0f);  // Green for active
        if (body->IsSleeping()) {
            color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray for sleeping
        } else if (body->IsStatic()) {
            color = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f);  // Blue for static
        } else if (body->IsKinematic()) {
            color = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange for kinematic
        }

        AABB aabb = body->GetWorldAABB();
        m_debugDraw->AddAABB(aabb.min, aabb.max, color);

        // Draw shapes
        glm::mat4 transform = body->GetTransformMatrix();
        for (const auto& shape : body->GetShapes()) {
            glm::vec4 shapeColor = shape.IsTrigger() ?
                glm::vec4(1.0f, 1.0f, 0.0f, 0.5f) : color;

            OBB obb = shape.ComputeWorldOBB(transform);
            m_debugDraw->AddBox(
                glm::translate(glm::mat4(1.0f), obb.center) * glm::mat4_cast(obb.orientation),
                obb.halfExtents,
                shapeColor
            );
        }
    }

    // Draw contact points
    for (const auto& pair : m_activeContacts) {
        CollisionBody* bodyA = const_cast<PhysicsWorld*>(this)->GetBody(pair.bodyA);
        CollisionBody* bodyB = const_cast<PhysicsWorld*>(this)->GetBody(pair.bodyB);
        if (!bodyA || !bodyB) continue;

        glm::vec3 contactPoint = (bodyA->GetPosition() + bodyB->GetPosition()) * 0.5f;
        m_debugDraw->AddPoint(contactPoint, 0.1f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}

} // namespace Nova
