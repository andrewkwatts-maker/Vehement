#include "OBB.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Nova {

std::array<glm::vec3, 8> OBB::GetCorners() const noexcept {
    UpdateAxes();

    glm::vec3 x = axes[0] * halfExtents.x;
    glm::vec3 y = axes[1] * halfExtents.y;
    glm::vec3 z = axes[2] * halfExtents.z;

    return {{
        center - x - y - z,
        center + x - y - z,
        center - x + y - z,
        center + x + y - z,
        center - x - y + z,
        center + x - y + z,
        center - x + y + z,
        center + x + y + z
    }};
}

AABB OBB::GetBoundingAABB() const noexcept {
    UpdateAxes();

    // Project each axis extent onto world axes
    glm::vec3 worldExtent(0.0f);

    for (int i = 0; i < 3; ++i) {
        worldExtent.x += std::abs(axes[i].x) * halfExtents[i];
        worldExtent.y += std::abs(axes[i].y) * halfExtents[i];
        worldExtent.z += std::abs(axes[i].z) * halfExtents[i];
    }

    return AABB(center - worldExtent, center + worldExtent);
}

bool OBB::Contains(const glm::vec3& point) const noexcept {
    glm::vec3 local = WorldToLocal(point);

    return std::abs(local.x) <= halfExtents.x &&
           std::abs(local.y) <= halfExtents.y &&
           std::abs(local.z) <= halfExtents.z;
}

glm::vec3 OBB::ClosestPoint(const glm::vec3& point) const noexcept {
    UpdateAxes();

    glm::vec3 d = point - center;
    glm::vec3 result = center;

    // Project point onto each axis and clamp
    for (int i = 0; i < 3; ++i) {
        float dist = glm::dot(d, axes[i]);
        dist = glm::clamp(dist, -halfExtents[i], halfExtents[i]);
        result += dist * axes[i];
    }

    return result;
}

float OBB::DistanceSquared(const glm::vec3& point) const noexcept {
    glm::vec3 closest = ClosestPoint(point);
    glm::vec3 diff = point - closest;
    return glm::dot(diff, diff);
}

float OBB::Distance(const glm::vec3& point) const noexcept {
    return std::sqrt(DistanceSquared(point));
}

glm::vec3 OBB::WorldToLocal(const glm::vec3& worldPoint) const noexcept {
    UpdateAxes();

    glm::vec3 d = worldPoint - center;
    return glm::vec3(
        glm::dot(d, axes[0]),
        glm::dot(d, axes[1]),
        glm::dot(d, axes[2])
    );
}

glm::vec3 OBB::LocalToWorld(const glm::vec3& localPoint) const noexcept {
    UpdateAxes();
    return center + axes[0] * localPoint.x + axes[1] * localPoint.y + axes[2] * localPoint.z;
}

bool OBB::Intersects(const OBB& other) const noexcept {
    UpdateAxes();
    other.UpdateAxes();

    constexpr float epsilon = 1e-6f;

    // Build rotation matrix expressing other in this OBB's coordinate frame
    float R[3][3], AbsR[3][3];

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = glm::dot(axes[i], other.axes[j]);
            AbsR[i][j] = std::abs(R[i][j]) + epsilon;
        }
    }

    // Translation in this OBB's coordinate frame
    glm::vec3 t = other.center - center;
    t = glm::vec3(glm::dot(t, axes[0]), glm::dot(t, axes[1]), glm::dot(t, axes[2]));

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
        float sep = std::abs(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]);
        if (sep > ra + rb) return false;
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

    return true;
}

bool OBB::Intersects(const AABB& aabb) const noexcept {
    // Create OBB from AABB and use OBB-OBB test
    OBB aabbOBB = OBB::FromAABB(aabb);
    return Intersects(aabbOBB);
}

bool OBB::IntersectsSphere(const glm::vec3& sphereCenter, float radius) const noexcept {
    float dist2 = DistanceSquared(sphereCenter);
    return dist2 <= radius * radius;
}

float OBB::RayIntersect(const Ray& ray) const noexcept {
    float t;
    glm::vec3 normal;
    if (RayIntersect(ray, t, normal)) {
        return t;
    }
    return -1.0f;
}

bool OBB::RayIntersect(const Ray& ray, float& outT, glm::vec3& outNormal) const noexcept {
    UpdateAxes();

    // Transform ray to OBB local space
    glm::vec3 localOrigin = WorldToLocal(ray.origin);
    glm::vec3 localDir = glm::vec3(
        glm::dot(ray.direction, axes[0]),
        glm::dot(ray.direction, axes[1]),
        glm::dot(ray.direction, axes[2])
    );

    // Now test against axis-aligned box in local space
    float tMin = -std::numeric_limits<float>::max();
    float tMax = std::numeric_limits<float>::max();
    int normalAxis = -1;
    float normalSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(localDir[i]) < 1e-6f) {
            // Ray is parallel to this slab
            if (localOrigin[i] < -halfExtents[i] || localOrigin[i] > halfExtents[i]) {
                return false;
            }
        } else {
            float invD = 1.0f / localDir[i];
            float t1 = (-halfExtents[i] - localOrigin[i]) * invD;
            float t2 = (halfExtents[i] - localOrigin[i]) * invD;

            float tNear = t1;
            float tFar = t2;
            float sign = -1.0f;

            if (t1 > t2) {
                tNear = t2;
                tFar = t1;
                sign = 1.0f;
            }

            if (tNear > tMin) {
                tMin = tNear;
                normalAxis = i;
                normalSign = sign;
            }

            tMax = std::min(tMax, tFar);

            if (tMin > tMax) return false;
        }
    }

    if (tMax < 0.0f) return false;

    outT = tMin >= 0.0f ? tMin : tMax;

    // Calculate normal in world space
    if (normalAxis >= 0) {
        outNormal = axes[normalAxis] * normalSign;
    } else {
        outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    return true;
}

bool OBB::GetPenetration(const OBB& other, float& depth, glm::vec3& normal) const noexcept {
    UpdateAxes();
    other.UpdateAxes();

    constexpr float epsilon = 1e-6f;

    float minPenetration = std::numeric_limits<float>::max();
    glm::vec3 minAxis(0.0f);

    // Test 15 axes
    glm::vec3 testAxes[15];
    int numAxes = 0;

    // 6 face axes
    for (int i = 0; i < 3; ++i) {
        testAxes[numAxes++] = axes[i];
        testAxes[numAxes++] = other.axes[i];
    }

    // 9 edge-edge axes
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            glm::vec3 cross = glm::cross(axes[i], other.axes[j]);
            float len = glm::length(cross);
            if (len > epsilon) {
                testAxes[numAxes++] = cross / len;
            }
        }
    }

    for (int i = 0; i < numAxes; ++i) {
        float minA, maxA, minB, maxB;
        SAT::ProjectOBB(*this, testAxes[i], minA, maxA);
        SAT::ProjectOBB(other, testAxes[i], minB, maxB);

        float overlap = SAT::GetOverlap(minA, maxA, minB, maxB);
        if (overlap < 0.0f) {
            return false; // Separating axis found
        }

        if (overlap < minPenetration) {
            minPenetration = overlap;
            minAxis = testAxes[i];

            // Ensure normal points from this to other
            glm::vec3 d = other.center - center;
            if (glm::dot(d, minAxis) < 0.0f) {
                minAxis = -minAxis;
            }
        }
    }

    depth = minPenetration;
    normal = minAxis;
    return true;
}

glm::vec3 OBB::GetSupport(const glm::vec3& direction) const noexcept {
    UpdateAxes();

    glm::vec3 result = center;

    for (int i = 0; i < 3; ++i) {
        float sign = glm::dot(direction, axes[i]) >= 0.0f ? 1.0f : -1.0f;
        result += axes[i] * (halfExtents[i] * sign);
    }

    return result;
}

OBB OBB::Transform(const glm::vec3& translation, const glm::quat& rotation) const noexcept {
    OBB result;
    result.center = rotation * center + translation;
    result.halfExtents = halfExtents;
    result.orientation = rotation * orientation;
    result.axesDirty = true;
    return result;
}

OBB OBB::Transform(const glm::mat4& matrix) const noexcept {
    glm::vec3 newCenter = glm::vec3(matrix * glm::vec4(center, 1.0f));
    glm::mat3 rotation = glm::mat3(matrix);
    glm::quat newOrientation = glm::quat_cast(rotation) * orientation;

    // Scale the extents
    glm::vec3 scale(
        glm::length(rotation[0]),
        glm::length(rotation[1]),
        glm::length(rotation[2])
    );

    return OBB(newCenter, halfExtents * scale, newOrientation);
}

// SAT Helper Functions
namespace SAT {

void ProjectOBB(const OBB& obb, const glm::vec3& axis, float& outMin, float& outMax) noexcept {
    const glm::vec3* axes = obb.GetAxes();

    float projection = glm::dot(obb.center, axis);
    float radius = 0.0f;

    for (int i = 0; i < 3; ++i) {
        radius += std::abs(glm::dot(axes[i] * obb.halfExtents[i], axis));
    }

    outMin = projection - radius;
    outMax = projection + radius;
}

void ProjectAABB(const AABB& aabb, const glm::vec3& axis, float& outMin, float& outMax) noexcept {
    glm::vec3 center = aabb.GetCenter();
    glm::vec3 extents = aabb.GetExtents();

    float projection = glm::dot(center, axis);
    float radius = std::abs(axis.x) * extents.x +
                   std::abs(axis.y) * extents.y +
                   std::abs(axis.z) * extents.z;

    outMin = projection - radius;
    outMax = projection + radius;
}

bool Overlaps(float minA, float maxA, float minB, float maxB) noexcept {
    return minA <= maxB && maxA >= minB;
}

float GetOverlap(float minA, float maxA, float minB, float maxB) noexcept {
    return std::min(maxA, maxB) - std::max(minA, minB);
}

} // namespace SAT

} // namespace Nova
