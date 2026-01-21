#include "CollisionShape.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// ShapeType helpers
// ============================================================================

std::optional<ShapeType> ShapeTypeFromString(const std::string& str) noexcept {
    if (str == "box")           return ShapeType::Box;
    if (str == "sphere")        return ShapeType::Sphere;
    if (str == "capsule")       return ShapeType::Capsule;
    if (str == "cylinder")      return ShapeType::Cylinder;
    if (str == "convex_hull")   return ShapeType::ConvexHull;
    if (str == "triangle_mesh") return ShapeType::TriangleMesh;
    if (str == "compound")      return ShapeType::Compound;
    return std::nullopt;
}

// ============================================================================
// PhysicsMaterial
// ============================================================================

nlohmann::json PhysicsMaterial::ToJson() const {
    return nlohmann::json{
        {"friction", friction},
        {"restitution", restitution},
        {"density", density}
    };
}

PhysicsMaterial PhysicsMaterial::FromJson(const nlohmann::json& j) {
    PhysicsMaterial mat;
    if (j.contains("friction"))    mat.friction = j["friction"].get<float>();
    if (j.contains("restitution")) mat.restitution = j["restitution"].get<float>();
    if (j.contains("density"))     mat.density = j["density"].get<float>();
    return mat;
}

// ============================================================================
// ShapeTransform
// ============================================================================

glm::mat4 ShapeTransform::ToMatrix() const {
    glm::mat4 mat = glm::mat4_cast(rotation);
    mat[3] = glm::vec4(position, 1.0f);
    return mat;
}

glm::vec3 ShapeTransform::TransformPoint(const glm::vec3& point) const {
    return rotation * point + position;
}

glm::vec3 ShapeTransform::TransformDirection(const glm::vec3& dir) const {
    return rotation * dir;
}

glm::vec3 ShapeTransform::InverseTransformPoint(const glm::vec3& point) const {
    return glm::inverse(rotation) * (point - position);
}

nlohmann::json ShapeTransform::ToJson() const {
    nlohmann::json j;
    if (position != glm::vec3(0.0f)) {
        j["offset"] = {position.x, position.y, position.z};
    }
    if (rotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
        j["rotation"] = {euler.x, euler.y, euler.z};
    }
    return j;
}

ShapeTransform ShapeTransform::FromJson(const nlohmann::json& j) {
    ShapeTransform transform;

    if (j.contains("offset") && j["offset"].is_array()) {
        const auto& arr = j["offset"];
        if (arr.size() >= 3) {
            transform.position = glm::vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()
            );
        }
    }

    if (j.contains("rotation") && j["rotation"].is_array()) {
        const auto& arr = j["rotation"];
        if (arr.size() >= 3) {
            glm::vec3 euler(
                glm::radians(arr[0].get<float>()),
                glm::radians(arr[1].get<float>()),
                glm::radians(arr[2].get<float>())
            );
            transform.rotation = glm::quat(euler);
        }
    }

    return transform;
}

// ============================================================================
// Shape Parameters - Inertia Tensors
// ============================================================================

namespace ShapeParams {

glm::mat3 Box::GetInertiaTensor(float mass) const {
    float x2 = halfExtents.x * halfExtents.x * 4.0f;
    float y2 = halfExtents.y * halfExtents.y * 4.0f;
    float z2 = halfExtents.z * halfExtents.z * 4.0f;
    float factor = mass / 12.0f;

    return glm::mat3(
        factor * (y2 + z2), 0.0f, 0.0f,
        0.0f, factor * (x2 + z2), 0.0f,
        0.0f, 0.0f, factor * (x2 + y2)
    );
}

glm::mat3 Sphere::GetInertiaTensor(float mass) const {
    float I = (2.0f / 5.0f) * mass * radius * radius;
    return glm::mat3(
        I, 0.0f, 0.0f,
        0.0f, I, 0.0f,
        0.0f, 0.0f, I
    );
}

float Capsule::GetVolume() const {
    constexpr float kPi = 3.14159265f;
    // Volume = cylinder + sphere
    float cylinderVol = kPi * radius * radius * height;
    float sphereVol = (4.0f / 3.0f) * kPi * radius * radius * radius;
    return cylinderVol + sphereVol;
}

glm::mat3 Capsule::GetInertiaTensor(float mass) const {
    // Approximation using cylinder + hemisphere contributions
    float r2 = radius * radius;
    float h = height;

    // Cylinder portion mass ratio
    constexpr float kPi = 3.14159265f;
    float cylVol = kPi * r2 * h;
    float sphereVol = (4.0f / 3.0f) * kPi * r2 * radius;
    float totalVol = cylVol + sphereVol;

    float cylMass = mass * cylVol / totalVol;
    float sphereMass = mass * sphereVol / totalVol;

    // Cylinder inertia (along Y axis)
    float Ixx_cyl = cylMass * (3.0f * r2 + h * h) / 12.0f;
    float Iyy_cyl = cylMass * r2 / 2.0f;
    float Izz_cyl = Ixx_cyl;

    // Sphere inertia (two hemispheres at ends)
    float Isphere = (2.0f / 5.0f) * sphereMass * r2;
    // Parallel axis theorem for offset
    float offset = h / 2.0f + (3.0f / 8.0f) * radius;
    float Ixx_sphere = Isphere + sphereMass * offset * offset;

    return glm::mat3(
        Ixx_cyl + Ixx_sphere, 0.0f, 0.0f,
        0.0f, Iyy_cyl + Isphere, 0.0f,
        0.0f, 0.0f, Izz_cyl + Ixx_sphere
    );
}

glm::mat3 Cylinder::GetInertiaTensor(float mass) const {
    float r2 = radius * radius;
    float h2 = height * height;

    float Ixx = mass * (3.0f * r2 + h2) / 12.0f;
    float Iyy = mass * r2 / 2.0f;

    return glm::mat3(
        Ixx, 0.0f, 0.0f,
        0.0f, Iyy, 0.0f,
        0.0f, 0.0f, Ixx
    );
}

float ConvexHull::GetVolume() const {
    if (vertices.size() < 4) return 0.0f;

    // Use signed tetrahedron volumes from centroid
    glm::vec3 centroid = GetCentroid();
    float volume = 0.0f;

    // Simple approximation - actual implementation would need hull faces
    // For now, use bounding box approximation
    glm::vec3 min(FLT_MAX), max(-FLT_MAX);
    for (const auto& v : vertices) {
        min = glm::min(min, v);
        max = glm::max(max, v);
    }
    glm::vec3 size = max - min;
    return size.x * size.y * size.z * 0.6f; // Rough convex hull estimate
}

glm::vec3 ConvexHull::GetCentroid() const {
    if (vertices.empty()) return glm::vec3(0.0f);

    glm::vec3 sum(0.0f);
    for (const auto& v : vertices) {
        sum += v;
    }
    return sum / static_cast<float>(vertices.size());
}

glm::mat3 ConvexHull::GetInertiaTensor(float mass) const {
    // Approximation using bounding box
    if (vertices.empty()) return glm::mat3(1.0f);

    glm::vec3 min(FLT_MAX), max(-FLT_MAX);
    for (const auto& v : vertices) {
        min = glm::min(min, v);
        max = glm::max(max, v);
    }
    glm::vec3 size = max - min;

    float x2 = size.x * size.x;
    float y2 = size.y * size.y;
    float z2 = size.z * size.z;
    float factor = mass / 12.0f;

    return glm::mat3(
        factor * (y2 + z2), 0.0f, 0.0f,
        0.0f, factor * (x2 + z2), 0.0f,
        0.0f, 0.0f, factor * (x2 + y2)
    );
}

} // namespace ShapeParams

// ============================================================================
// AABB
// ============================================================================

// Note: All AABB methods are now defined inline in CollisionShape.hpp
// to avoid duplicate symbol definitions across translation units.

// ============================================================================
// OBB
// ============================================================================

glm::vec3 OBB::GetAxis(int index) const {
    glm::mat3 rot = glm::mat3_cast(orientation);
    return rot[index];
}

std::array<glm::vec3, 8> OBB::GetCorners() const {
    std::array<glm::vec3, 8> corners;
    glm::vec3 axes[3] = {
        GetAxis(0) * halfExtents.x,
        GetAxis(1) * halfExtents.y,
        GetAxis(2) * halfExtents.z
    };

    corners[0] = center - axes[0] - axes[1] - axes[2];
    corners[1] = center + axes[0] - axes[1] - axes[2];
    corners[2] = center - axes[0] + axes[1] - axes[2];
    corners[3] = center + axes[0] + axes[1] - axes[2];
    corners[4] = center - axes[0] - axes[1] + axes[2];
    corners[5] = center + axes[0] - axes[1] + axes[2];
    corners[6] = center - axes[0] + axes[1] + axes[2];
    corners[7] = center + axes[0] + axes[1] + axes[2];

    return corners;
}

AABB OBB::GetAABB() const {
    auto corners = GetCorners();
    AABB aabb{corners[0], corners[0]};
    for (int i = 1; i < 8; ++i) {
        aabb.Expand(corners[i]);
    }
    return aabb;
}

bool OBB::Contains(const glm::vec3& point) const {
    glm::vec3 local = glm::inverse(orientation) * (point - center);
    return std::abs(local.x) <= halfExtents.x &&
           std::abs(local.y) <= halfExtents.y &&
           std::abs(local.z) <= halfExtents.z;
}

bool OBB::Intersects(const OBB& other) const {
    // SAT (Separating Axis Theorem) test
    constexpr float kEpsilon = 1e-6f;

    glm::mat3 R, AbsR;
    glm::vec3 aAxes[3] = {GetAxis(0), GetAxis(1), GetAxis(2)};
    glm::vec3 bAxes[3] = {other.GetAxis(0), other.GetAxis(1), other.GetAxis(2)};

    // Compute rotation matrix expressing B in A's coordinate frame
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = glm::dot(aAxes[i], bAxes[j]);
            AbsR[i][j] = std::abs(R[i][j]) + kEpsilon;
        }
    }

    // Compute translation vector
    glm::vec3 t = other.center - center;
    // Bring translation into A's coordinate frame
    t = glm::vec3(glm::dot(t, aAxes[0]), glm::dot(t, aAxes[1]), glm::dot(t, aAxes[2]));

    float ra, rb;

    // Test axes L = A0, A1, A2
    for (int i = 0; i < 3; ++i) {
        ra = halfExtents[i];
        rb = other.halfExtents[0] * AbsR[i][0] +
             other.halfExtents[1] * AbsR[i][1] +
             other.halfExtents[2] * AbsR[i][2];
        if (std::abs(t[i]) > ra + rb) return false;
    }

    // Test axes L = B0, B1, B2
    for (int i = 0; i < 3; ++i) {
        ra = halfExtents[0] * AbsR[0][i] +
             halfExtents[1] * AbsR[1][i] +
             halfExtents[2] * AbsR[2][i];
        rb = other.halfExtents[i];
        float proj = std::abs(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]);
        if (proj > ra + rb) return false;
    }

    // Test axis L = A0 x B0
    ra = halfExtents[1] * AbsR[2][0] + halfExtents[2] * AbsR[1][0];
    rb = other.halfExtents[1] * AbsR[0][2] + other.halfExtents[2] * AbsR[0][1];
    if (std::abs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;

    // Test axis L = A0 x B1
    ra = halfExtents[1] * AbsR[2][1] + halfExtents[2] * AbsR[1][1];
    rb = other.halfExtents[0] * AbsR[0][2] + other.halfExtents[2] * AbsR[0][0];
    if (std::abs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;

    // Test axis L = A0 x B2
    ra = halfExtents[1] * AbsR[2][2] + halfExtents[2] * AbsR[1][2];
    rb = other.halfExtents[0] * AbsR[0][1] + other.halfExtents[1] * AbsR[0][0];
    if (std::abs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;

    // Test axis L = A1 x B0
    ra = halfExtents[0] * AbsR[2][0] + halfExtents[2] * AbsR[0][0];
    rb = other.halfExtents[1] * AbsR[1][2] + other.halfExtents[2] * AbsR[1][1];
    if (std::abs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;

    // Test axis L = A1 x B1
    ra = halfExtents[0] * AbsR[2][1] + halfExtents[2] * AbsR[0][1];
    rb = other.halfExtents[0] * AbsR[1][2] + other.halfExtents[2] * AbsR[1][0];
    if (std::abs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;

    // Test axis L = A1 x B2
    ra = halfExtents[0] * AbsR[2][2] + halfExtents[2] * AbsR[0][2];
    rb = other.halfExtents[0] * AbsR[1][1] + other.halfExtents[1] * AbsR[1][0];
    if (std::abs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;

    // Test axis L = A2 x B0
    ra = halfExtents[0] * AbsR[1][0] + halfExtents[1] * AbsR[0][0];
    rb = other.halfExtents[1] * AbsR[2][2] + other.halfExtents[2] * AbsR[2][1];
    if (std::abs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;

    // Test axis L = A2 x B1
    ra = halfExtents[0] * AbsR[1][1] + halfExtents[1] * AbsR[0][1];
    rb = other.halfExtents[0] * AbsR[2][2] + other.halfExtents[2] * AbsR[2][0];
    if (std::abs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;

    // Test axis L = A2 x B2
    ra = halfExtents[0] * AbsR[1][2] + halfExtents[1] * AbsR[0][2];
    rb = other.halfExtents[0] * AbsR[2][1] + other.halfExtents[1] * AbsR[2][0];
    if (std::abs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;

    // No separating axis found, OBBs must be intersecting
    return true;
}

glm::vec3 OBB::ClosestPoint(const glm::vec3& point) const {
    glm::vec3 d = point - center;
    glm::vec3 result = center;

    for (int i = 0; i < 3; ++i) {
        glm::vec3 axis = GetAxis(i);
        float dist = glm::dot(d, axis);
        dist = glm::clamp(dist, -halfExtents[i], halfExtents[i]);
        result += dist * axis;
    }

    return result;
}

// ============================================================================
// CollisionShape
// ============================================================================

CollisionShape::CollisionShape(ShapeType type) : m_type(type) {
    InitDefaultParams();
}

CollisionShape::CollisionShape(ShapeType type, const ShapeParamsVariant& params)
    : m_type(type), m_params(params) {
}

void CollisionShape::InitDefaultParams() {
    switch (m_type) {
        case ShapeType::Box:
            m_params = ShapeParams::Box{};
            break;
        case ShapeType::Sphere:
            m_params = ShapeParams::Sphere{};
            break;
        case ShapeType::Capsule:
            m_params = ShapeParams::Capsule{};
            break;
        case ShapeType::Cylinder:
            m_params = ShapeParams::Cylinder{};
            break;
        case ShapeType::ConvexHull:
            m_params = ShapeParams::ConvexHull{};
            break;
        case ShapeType::TriangleMesh:
            m_params = ShapeParams::TriangleMesh{};
            break;
        case ShapeType::Compound:
            m_params = ShapeParams::Compound{};
            break;
    }
}

void CollisionShape::SetType(ShapeType type) {
    if (m_type != type) {
        m_type = type;
        InitDefaultParams();
        m_boundsDirty = true;
    }
}

void CollisionShape::SetLocalTransform(const ShapeTransform& transform) {
    m_localTransform = transform;
    m_boundsDirty = true;
}

void CollisionShape::SetLocalPosition(const glm::vec3& pos) {
    m_localTransform.position = pos;
    m_boundsDirty = true;
}

void CollisionShape::SetLocalRotation(const glm::quat& rot) {
    m_localTransform.rotation = rot;
    m_boundsDirty = true;
}

AABB CollisionShape::ComputeLocalAABB() const {
    AABB aabb;

    std::visit([&](const auto& params) {
        using T = std::decay_t<decltype(params)>;

        if constexpr (std::is_same_v<T, ShapeParams::Box>) {
            glm::vec3 he = params.halfExtents;
            aabb = AABB{-he, he};
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Sphere>) {
            glm::vec3 r(params.radius);
            aabb = AABB{-r, r};
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Capsule>) {
            float h = params.height / 2.0f + params.radius;
            glm::vec3 ext(params.radius, h, params.radius);
            aabb = AABB{-ext, ext};
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Cylinder>) {
            float h = params.height / 2.0f;
            glm::vec3 ext(params.radius, h, params.radius);
            aabb = AABB{-ext, ext};
        }
        else if constexpr (std::is_same_v<T, ShapeParams::ConvexHull>) {
            if (!params.vertices.empty()) {
                aabb.min = aabb.max = params.vertices[0];
                for (const auto& v : params.vertices) {
                    aabb.Expand(v);
                }
            }
        }
        else if constexpr (std::is_same_v<T, ShapeParams::TriangleMesh>) {
            if (!params.vertices.empty()) {
                aabb.min = aabb.max = params.vertices[0];
                for (const auto& v : params.vertices) {
                    aabb.Expand(v);
                }
            }
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Compound>) {
            bool first = true;
            for (const auto& child : params.children) {
                if (child) {
                    AABB childAABB = child->ComputeLocalAABB();
                    if (first) {
                        aabb = childAABB;
                        first = false;
                    } else {
                        aabb.Expand(childAABB);
                    }
                }
            }
        }
    }, m_params);

    // Apply local transform
    if (m_localTransform.position != glm::vec3(0.0f) ||
        m_localTransform.rotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
        // Transform all 8 corners and recompute AABB
        OBB obb;
        obb.center = aabb.GetCenter();
        obb.halfExtents = aabb.GetExtents();
        obb.center = m_localTransform.TransformPoint(obb.center);
        obb.orientation = m_localTransform.rotation;
        aabb = obb.GetAABB();
    }

    return aabb;
}

AABB CollisionShape::ComputeWorldAABB(const glm::mat4& worldTransform) const {
    OBB localOBB = ComputeLocalOBB();

    // Transform OBB to world space
    OBB worldOBB;
    worldOBB.center = glm::vec3(worldTransform * glm::vec4(localOBB.center, 1.0f));
    worldOBB.halfExtents = localOBB.halfExtents;
    worldOBB.orientation = glm::quat_cast(glm::mat3(worldTransform)) * localOBB.orientation;

    return worldOBB.GetAABB();
}

OBB CollisionShape::ComputeLocalOBB() const {
    OBB obb;

    std::visit([&](const auto& params) {
        using T = std::decay_t<decltype(params)>;

        if constexpr (std::is_same_v<T, ShapeParams::Box>) {
            obb.halfExtents = params.halfExtents;
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Sphere>) {
            obb.halfExtents = glm::vec3(params.radius);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Capsule>) {
            float h = params.height / 2.0f + params.radius;
            obb.halfExtents = glm::vec3(params.radius, h, params.radius);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Cylinder>) {
            float h = params.height / 2.0f;
            obb.halfExtents = glm::vec3(params.radius, h, params.radius);
        }
        else {
            AABB aabb = ComputeLocalAABB();
            obb.center = aabb.GetCenter();
            obb.halfExtents = aabb.GetExtents();
        }
    }, m_params);

    obb.center = m_localTransform.TransformPoint(obb.center);
    obb.orientation = m_localTransform.rotation;

    return obb;
}

OBB CollisionShape::ComputeWorldOBB(const glm::mat4& worldTransform) const {
    OBB localOBB = ComputeLocalOBB();

    OBB worldOBB;
    worldOBB.center = glm::vec3(worldTransform * glm::vec4(localOBB.center, 1.0f));
    worldOBB.halfExtents = localOBB.halfExtents;

    glm::mat3 worldRot = glm::mat3(worldTransform);
    // Check for uniform scale
    glm::vec3 scale(
        glm::length(worldRot[0]),
        glm::length(worldRot[1]),
        glm::length(worldRot[2])
    );
    worldOBB.halfExtents *= scale;
    worldRot[0] /= scale.x;
    worldRot[1] /= scale.y;
    worldRot[2] /= scale.z;

    worldOBB.orientation = glm::quat_cast(worldRot) * localOBB.orientation;

    return worldOBB;
}

float CollisionShape::GetVolume() const {
    return std::visit([](const auto& params) -> float {
        using T = std::decay_t<decltype(params)>;

        if constexpr (std::is_same_v<T, ShapeParams::Box>) {
            return params.GetVolume();
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Sphere>) {
            return params.GetVolume();
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Capsule>) {
            return params.GetVolume();
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Cylinder>) {
            return params.GetVolume();
        }
        else if constexpr (std::is_same_v<T, ShapeParams::ConvexHull>) {
            return params.GetVolume();
        }
        else if constexpr (std::is_same_v<T, ShapeParams::TriangleMesh>) {
            return 0.0f; // Mesh volume not computed
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Compound>) {
            float total = 0.0f;
            for (const auto& child : params.children) {
                if (child) total += child->GetVolume();
            }
            return total;
        }
        return 0.0f;
    }, m_params);
}

float CollisionShape::CalculateMass() const {
    return GetVolume() * m_material.density;
}

glm::mat3 CollisionShape::CalculateInertiaTensor(float mass) const {
    return std::visit([mass](const auto& params) -> glm::mat3 {
        using T = std::decay_t<decltype(params)>;

        if constexpr (std::is_same_v<T, ShapeParams::Box>) {
            return params.GetInertiaTensor(mass);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Sphere>) {
            return params.GetInertiaTensor(mass);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Capsule>) {
            return params.GetInertiaTensor(mass);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Cylinder>) {
            return params.GetInertiaTensor(mass);
        }
        else if constexpr (std::is_same_v<T, ShapeParams::ConvexHull>) {
            return params.GetInertiaTensor(mass);
        }
        else {
            // Default to unit inertia
            return glm::mat3(mass);
        }
    }, m_params);
}

nlohmann::json CollisionShape::ToJson() const {
    nlohmann::json j;

    j["type"] = ShapeTypeToString(m_type);

    std::visit([&j](const auto& params) {
        using T = std::decay_t<decltype(params)>;

        if constexpr (std::is_same_v<T, ShapeParams::Box>) {
            j["half_extents"] = {params.halfExtents.x, params.halfExtents.y, params.halfExtents.z};
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Sphere>) {
            j["radius"] = params.radius;
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Capsule>) {
            j["radius"] = params.radius;
            j["height"] = params.height;
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Cylinder>) {
            j["radius"] = params.radius;
            j["height"] = params.height;
        }
        else if constexpr (std::is_same_v<T, ShapeParams::ConvexHull>) {
            nlohmann::json verts = nlohmann::json::array();
            for (const auto& v : params.vertices) {
                verts.push_back({v.x, v.y, v.z});
            }
            j["vertices"] = verts;
        }
        else if constexpr (std::is_same_v<T, ShapeParams::TriangleMesh>) {
            if (!params.meshFilePath.empty()) {
                j["mesh_file"] = params.meshFilePath;
            }
        }
        else if constexpr (std::is_same_v<T, ShapeParams::Compound>) {
            nlohmann::json children = nlohmann::json::array();
            for (const auto& child : params.children) {
                if (child) {
                    children.push_back(child->ToJson());
                }
            }
            j["children"] = children;
        }
    }, m_params);

    nlohmann::json transformJson = m_localTransform.ToJson();
    if (!transformJson.empty()) {
        j.merge_patch(transformJson);
    }

    if (m_material.friction != 0.5f || m_material.restitution != 0.3f || m_material.density != 1.0f) {
        j["material"] = m_material.ToJson();
    }

    if (m_isTrigger) {
        j["is_trigger"] = true;
        if (!m_triggerEvent.empty()) {
            j["trigger_event"] = m_triggerEvent;
        }
    }

    return j;
}

std::optional<CollisionShape> CollisionShape::FromJson(const nlohmann::json& j) {
    if (!j.contains("type")) {
        return std::nullopt;
    }

    auto typeOpt = ShapeTypeFromString(j["type"].get<std::string>());
    if (!typeOpt) {
        return std::nullopt;
    }

    CollisionShape shape(*typeOpt);

    // Parse shape-specific parameters
    switch (*typeOpt) {
        case ShapeType::Box: {
            ShapeParams::Box params;
            if (j.contains("half_extents") && j["half_extents"].is_array() && j["half_extents"].size() >= 3) {
                params.halfExtents = glm::vec3(
                    j["half_extents"][0].get<float>(),
                    j["half_extents"][1].get<float>(),
                    j["half_extents"][2].get<float>()
                );
            }
            shape.SetParams(params);
            break;
        }
        case ShapeType::Sphere: {
            ShapeParams::Sphere params;
            if (j.contains("radius")) params.radius = j["radius"].get<float>();
            shape.SetParams(params);
            break;
        }
        case ShapeType::Capsule: {
            ShapeParams::Capsule params;
            if (j.contains("radius")) params.radius = j["radius"].get<float>();
            if (j.contains("height")) params.height = j["height"].get<float>();
            shape.SetParams(params);
            break;
        }
        case ShapeType::Cylinder: {
            ShapeParams::Cylinder params;
            if (j.contains("radius")) params.radius = j["radius"].get<float>();
            if (j.contains("height")) params.height = j["height"].get<float>();
            shape.SetParams(params);
            break;
        }
        case ShapeType::ConvexHull: {
            ShapeParams::ConvexHull params;
            if (j.contains("vertices") && j["vertices"].is_array()) {
                for (const auto& v : j["vertices"]) {
                    if (v.is_array() && v.size() >= 3) {
                        params.vertices.emplace_back(
                            v[0].get<float>(),
                            v[1].get<float>(),
                            v[2].get<float>()
                        );
                    }
                }
            }
            shape.SetParams(params);
            break;
        }
        case ShapeType::TriangleMesh: {
            ShapeParams::TriangleMesh params;
            if (j.contains("mesh_file")) {
                params.meshFilePath = j["mesh_file"].get<std::string>();
            }
            shape.SetParams(params);
            break;
        }
        case ShapeType::Compound: {
            ShapeParams::Compound params;
            if (j.contains("children") && j["children"].is_array()) {
                for (const auto& childJson : j["children"]) {
                    auto childResult = CollisionShape::FromJson(childJson);
                    if (childResult) {
                        params.children.push_back(std::make_shared<CollisionShape>(std::move(*childResult)));
                    }
                }
            }
            shape.SetParams(params);
            break;
        }
    }

    // Parse transform
    shape.SetLocalTransform(ShapeTransform::FromJson(j));

    // Parse material
    if (j.contains("material")) {
        shape.SetMaterial(PhysicsMaterial::FromJson(j["material"]));
    }

    // Parse trigger settings
    if (j.contains("is_trigger")) {
        shape.SetTrigger(j["is_trigger"].get<bool>());
    }
    if (j.contains("trigger_event")) {
        shape.SetTriggerEvent(j["trigger_event"].get<std::string>());
    }

    return shape;
}

CollisionShape CollisionShape::CreateDefault(ShapeType type) {
    return CollisionShape(type);
}

CollisionShape CollisionShape::CreateBox(const glm::vec3& halfExtents) {
    CollisionShape shape(ShapeType::Box);
    ShapeParams::Box params;
    params.halfExtents = halfExtents;
    shape.SetParams(params);
    return shape;
}

CollisionShape CollisionShape::CreateSphere(float radius) {
    CollisionShape shape(ShapeType::Sphere);
    ShapeParams::Sphere params;
    params.radius = radius;
    shape.SetParams(params);
    return shape;
}

CollisionShape CollisionShape::CreateCapsule(float radius, float height) {
    CollisionShape shape(ShapeType::Capsule);
    ShapeParams::Capsule params;
    params.radius = radius;
    params.height = height;
    shape.SetParams(params);
    return shape;
}

CollisionShape CollisionShape::CreateCylinder(float radius, float height) {
    CollisionShape shape(ShapeType::Cylinder);
    ShapeParams::Cylinder params;
    params.radius = radius;
    params.height = height;
    shape.SetParams(params);
    return shape;
}

} // namespace Nova
