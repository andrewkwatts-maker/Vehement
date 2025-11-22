#include "CollisionBody.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Nova {

// Static member initialization
CollisionBody::BodyId CollisionBody::s_nextId = 1;

// ============================================================================
// BodyType helpers
// ============================================================================

std::optional<BodyType> BodyTypeFromString(const std::string& str) noexcept {
    if (str == "static")    return BodyType::Static;
    if (str == "kinematic") return BodyType::Kinematic;
    if (str == "dynamic")   return BodyType::Dynamic;
    return std::nullopt;
}

// ============================================================================
// CollisionLayer helpers
// ============================================================================

namespace CollisionLayer {

uint32_t FromString(const std::string& name) noexcept {
    if (name == "none")       return None;
    if (name == "default")    return Default;
    if (name == "terrain")    return Terrain;
    if (name == "unit")       return Unit;
    if (name == "building")   return Building;
    if (name == "projectile") return Projectile;
    if (name == "pickup")     return Pickup;
    if (name == "trigger")    return Trigger;
    if (name == "player")     return Player;
    if (name == "enemy")      return Enemy;
    if (name == "vehicle")    return Vehicle;
    if (name == "effect")     return Effect;
    if (name == "all")        return All;
    return Default;
}

const char* ToString(uint32_t layer) noexcept {
    switch (layer) {
        case None:       return "none";
        case Default:    return "default";
        case Terrain:    return "terrain";
        case Unit:       return "unit";
        case Building:   return "building";
        case Projectile: return "projectile";
        case Pickup:     return "pickup";
        case Trigger:    return "trigger";
        case Player:     return "player";
        case Enemy:      return "enemy";
        case Vehicle:    return "vehicle";
        case Effect:     return "effect";
        case All:        return "all";
        default:         return "custom";
    }
}

uint32_t ParseMask(const nlohmann::json& j) {
    if (!j.is_array()) {
        if (j.is_string()) {
            return FromString(j.get<std::string>());
        }
        return All;
    }

    uint32_t mask = None;
    for (const auto& item : j) {
        if (item.is_string()) {
            mask |= FromString(item.get<std::string>());
        }
    }
    return mask;
}

} // namespace CollisionLayer

// ============================================================================
// CollisionBody
// ============================================================================

CollisionBody::CollisionBody() : m_id(s_nextId++) {
}

CollisionBody::CollisionBody(BodyType type) : m_id(s_nextId++), m_bodyType(type) {
    if (type == BodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        m_inverseInertiaTensor = glm::mat3(0.0f);
    }
}

void CollisionBody::SetBodyType(BodyType type) {
    if (m_bodyType == type) return;

    m_bodyType = type;

    if (type == BodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        m_inverseInertiaTensor = glm::mat3(0.0f);
        m_linearVelocity = glm::vec3(0.0f);
        m_angularVelocity = glm::vec3(0.0f);
    } else {
        RecalculateMassProperties();
    }
}

void CollisionBody::SetPosition(const glm::vec3& pos) {
    m_position = pos;
    m_boundsDirty = true;
    WakeUp();
}

void CollisionBody::SetRotation(const glm::quat& rot) {
    m_rotation = glm::normalize(rot);
    m_boundsDirty = true;
    WakeUp();
}

glm::mat4 CollisionBody::GetTransformMatrix() const {
    glm::mat4 mat = glm::mat4_cast(m_rotation);
    mat[3] = glm::vec4(m_position, 1.0f);
    return mat;
}

void CollisionBody::SetLinearVelocity(const glm::vec3& vel) {
    if (m_bodyType == BodyType::Static) return;
    m_linearVelocity = vel;
    WakeUp();
}

void CollisionBody::SetAngularVelocity(const glm::vec3& vel) {
    if (m_bodyType == BodyType::Static) return;
    m_angularVelocity = vel;
    WakeUp();
}

void CollisionBody::ApplyForce(const glm::vec3& force) {
    if (m_bodyType != BodyType::Dynamic) return;
    m_accumulatedForce += force;
    WakeUp();
}

void CollisionBody::ApplyForceAtPoint(const glm::vec3& force, const glm::vec3& point) {
    if (m_bodyType != BodyType::Dynamic) return;
    m_accumulatedForce += force;
    m_accumulatedTorque += glm::cross(point - m_position, force);
    WakeUp();
}

void CollisionBody::ApplyTorque(const glm::vec3& torque) {
    if (m_bodyType != BodyType::Dynamic) return;
    m_accumulatedTorque += torque;
    WakeUp();
}

void CollisionBody::ApplyImpulse(const glm::vec3& impulse) {
    if (m_bodyType != BodyType::Dynamic) return;
    m_linearVelocity += impulse * m_inverseMass;
    WakeUp();
}

void CollisionBody::ApplyImpulseAtPoint(const glm::vec3& impulse, const glm::vec3& point) {
    if (m_bodyType != BodyType::Dynamic) return;

    m_linearVelocity += impulse * m_inverseMass;

    glm::vec3 r = point - m_position;
    glm::vec3 angularImpulse = glm::cross(r, impulse);
    m_angularVelocity += m_inverseInertiaTensor * angularImpulse;

    WakeUp();
}

void CollisionBody::ClearForces() {
    m_accumulatedForce = glm::vec3(0.0f);
    m_accumulatedTorque = glm::vec3(0.0f);
}

void CollisionBody::SetMass(float mass) {
    if (m_bodyType == BodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        return;
    }

    m_mass = std::max(mass, 0.001f);
    m_inverseMass = 1.0f / m_mass;
    RecalculateMassProperties();
}

void CollisionBody::RecalculateMassProperties() {
    if (m_bodyType == BodyType::Static) {
        m_mass = 0.0f;
        m_inverseMass = 0.0f;
        m_inertiaTensor = glm::mat3(0.0f);
        m_inverseInertiaTensor = glm::mat3(0.0f);
        return;
    }

    if (m_shapes.empty()) {
        m_inertiaTensor = glm::mat3(m_mass);
        m_inverseInertiaTensor = glm::mat3(1.0f / m_mass);
        return;
    }

    // Calculate combined mass and inertia from shapes
    float totalMass = 0.0f;
    for (const auto& shape : m_shapes) {
        if (!shape.IsTrigger()) {
            totalMass += shape.CalculateMass();
        }
    }

    if (totalMass <= 0.0f) {
        totalMass = m_mass;
    }

    // Use provided mass if set, otherwise use calculated
    if (m_mass > 0.001f) {
        float scale = m_mass / totalMass;
        totalMass = m_mass;
        // Scale would be applied to inertia calculation if needed
    } else {
        m_mass = totalMass;
    }

    m_inverseMass = 1.0f / m_mass;

    // Calculate combined inertia tensor
    m_inertiaTensor = glm::mat3(0.0f);
    for (const auto& shape : m_shapes) {
        if (!shape.IsTrigger()) {
            float shapeMass = shape.CalculateMass();
            glm::mat3 shapeInertia = shape.CalculateInertiaTensor(shapeMass);

            // Apply parallel axis theorem for offset shapes
            const auto& transform = shape.GetLocalTransform();
            if (transform.position != glm::vec3(0.0f)) {
                glm::vec3 r = transform.position;
                float r2 = glm::dot(r, r);
                glm::mat3 offset = shapeMass * (r2 * glm::mat3(1.0f) -
                    glm::outerProduct(r, r));
                shapeInertia += offset;
            }

            // Rotate inertia tensor if shape has local rotation
            if (transform.rotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
                glm::mat3 R = glm::mat3_cast(transform.rotation);
                shapeInertia = R * shapeInertia * glm::transpose(R);
            }

            m_inertiaTensor += shapeInertia;
        }
    }

    // Ensure positive definite
    for (int i = 0; i < 3; ++i) {
        m_inertiaTensor[i][i] = std::max(m_inertiaTensor[i][i], 0.001f);
    }

    m_inverseInertiaTensor = glm::inverse(m_inertiaTensor);
}

size_t CollisionBody::AddShape(const CollisionShape& shape) {
    m_shapes.push_back(shape);
    m_boundsDirty = true;
    RecalculateMassProperties();
    return m_shapes.size() - 1;
}

size_t CollisionBody::AddShape(CollisionShape&& shape) {
    m_shapes.push_back(std::move(shape));
    m_boundsDirty = true;
    RecalculateMassProperties();
    return m_shapes.size() - 1;
}

void CollisionBody::RemoveShape(size_t index) {
    if (index < m_shapes.size()) {
        m_shapes.erase(m_shapes.begin() + static_cast<ptrdiff_t>(index));
        m_boundsDirty = true;
        RecalculateMassProperties();
    }
}

void CollisionBody::ClearShapes() {
    m_shapes.clear();
    m_boundsDirty = true;
    RecalculateMassProperties();
}

bool CollisionBody::ShouldCollideWith(const CollisionBody& other) const {
    // Check if layers match masks
    return (m_collisionLayer & other.m_collisionMask) != 0 &&
           (other.m_collisionLayer & m_collisionMask) != 0;
}

AABB CollisionBody::GetWorldAABB() const {
    if (m_boundsDirty) {
        UpdateWorldAABB();
    }
    return m_worldAABB;
}

void CollisionBody::UpdateWorldAABB() const {
    if (m_shapes.empty()) {
        m_worldAABB = AABB{m_position, m_position};
        m_boundsDirty = false;
        return;
    }

    glm::mat4 transform = GetTransformMatrix();
    bool first = true;

    for (const auto& shape : m_shapes) {
        AABB shapeAABB = shape.ComputeWorldAABB(transform);
        if (first) {
            m_worldAABB = shapeAABB;
            first = false;
        } else {
            m_worldAABB.Expand(shapeAABB);
        }
    }

    m_boundsDirty = false;
}

bool CollisionBody::IsInContactWith(BodyId otherId) const {
    return std::find(m_contactBodies.begin(), m_contactBodies.end(), otherId) != m_contactBodies.end();
}

void CollisionBody::OnCollisionEnter(CollisionBody& other, const ContactInfo& contact) {
    if (m_onCollisionEnter) {
        m_onCollisionEnter(other, contact);
    }
}

void CollisionBody::OnCollisionStay(CollisionBody& other, const ContactInfo& contact) {
    if (m_onCollisionStay) {
        m_onCollisionStay(other, contact);
    }
}

void CollisionBody::OnCollisionExit(CollisionBody& other, const ContactInfo& contact) {
    if (m_onCollisionExit) {
        m_onCollisionExit(other, contact);
    }
}

void CollisionBody::OnTriggerEnter(CollisionBody& other) {
    if (m_onTriggerEnter) {
        m_onTriggerEnter(other);
    }
}

void CollisionBody::OnTriggerStay(CollisionBody& other) {
    if (m_onTriggerStay) {
        m_onTriggerStay(other);
    }
}

void CollisionBody::OnTriggerExit(CollisionBody& other) {
    if (m_onTriggerExit) {
        m_onTriggerExit(other);
    }
}

void CollisionBody::AddContact(BodyId otherId) {
    if (!IsInContactWith(otherId)) {
        m_contactBodies.push_back(otherId);
    }
}

void CollisionBody::RemoveContact(BodyId otherId) {
    auto it = std::find(m_contactBodies.begin(), m_contactBodies.end(), otherId);
    if (it != m_contactBodies.end()) {
        m_contactBodies.erase(it);
    }
}

void CollisionBody::ClearContacts() {
    m_contactBodies.clear();
}

nlohmann::json CollisionBody::ToJson() const {
    nlohmann::json j;

    j["body_type"] = BodyTypeToString(m_bodyType);

    if (m_mass != 1.0f && m_bodyType != BodyType::Static) {
        j["mass"] = m_mass;
    }

    if (m_collisionLayer != CollisionLayer::Default) {
        j["layer"] = CollisionLayer::ToString(m_collisionLayer);
    }

    if (m_collisionMask != CollisionLayer::All) {
        // Convert mask to array of layer names
        nlohmann::json maskArray = nlohmann::json::array();
        uint32_t mask = m_collisionMask;
        for (uint32_t i = 0; i < 32 && mask != 0; ++i) {
            uint32_t layer = 1u << i;
            if (mask & layer) {
                maskArray.push_back(CollisionLayer::ToString(layer));
                mask &= ~layer;
            }
        }
        j["mask"] = maskArray;
    }

    if (m_linearDamping != 0.01f) {
        j["linear_damping"] = m_linearDamping;
    }

    if (m_angularDamping != 0.05f) {
        j["angular_damping"] = m_angularDamping;
    }

    if (m_gravityScale != 1.0f) {
        j["gravity_scale"] = m_gravityScale;
    }

    // Serialize shapes
    if (!m_shapes.empty()) {
        nlohmann::json shapesArray = nlohmann::json::array();
        for (const auto& shape : m_shapes) {
            shapesArray.push_back(shape.ToJson());
        }
        j["shapes"] = shapesArray;
    }

    return j;
}

std::expected<CollisionBody, std::string> CollisionBody::FromJson(const nlohmann::json& j) {
    BodyType bodyType = BodyType::Static;
    if (j.contains("body_type")) {
        auto typeOpt = BodyTypeFromString(j["body_type"].get<std::string>());
        if (typeOpt) {
            bodyType = *typeOpt;
        }
    }

    CollisionBody body(bodyType);

    if (j.contains("mass")) {
        body.SetMass(j["mass"].get<float>());
    }

    if (j.contains("layer")) {
        if (j["layer"].is_string()) {
            body.SetCollisionLayer(CollisionLayer::FromString(j["layer"].get<std::string>()));
        }
    }

    if (j.contains("mask")) {
        body.SetCollisionMask(CollisionLayer::ParseMask(j["mask"]));
    }

    if (j.contains("linear_damping")) {
        body.SetLinearDamping(j["linear_damping"].get<float>());
    }

    if (j.contains("angular_damping")) {
        body.SetAngularDamping(j["angular_damping"].get<float>());
    }

    if (j.contains("gravity_scale")) {
        body.SetGravityScale(j["gravity_scale"].get<float>());
    }

    // Parse shapes
    if (j.contains("shapes") && j["shapes"].is_array()) {
        for (const auto& shapeJson : j["shapes"]) {
            auto shapeResult = CollisionShape::FromJson(shapeJson);
            if (shapeResult) {
                body.AddShape(std::move(*shapeResult));
            }
        }
    }

    return body;
}

} // namespace Nova
