#include <glm/glm.hpp>
#include <optional>
#include <cmath>

namespace Nova {

namespace Geometry {

// Ray-plane intersection
std::optional<float> RayPlaneIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& planePoint, const glm::vec3& planeNormal) {

    float denom = glm::dot(planeNormal, rayDir);
    if (std::abs(denom) < 1e-6f) {
        return std::nullopt;  // Ray parallel to plane
    }

    float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0) {
        return std::nullopt;  // Intersection behind ray
    }

    return t;
}

// Ray-sphere intersection
std::optional<float> RaySphereIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& sphereCenter, float sphereRadius) {

    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return std::nullopt;
    }

    float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (t < 0) {
        t = (-b + std::sqrt(discriminant)) / (2.0f * a);
    }

    if (t < 0) {
        return std::nullopt;
    }

    return t;
}

// Ray-AABB intersection
std::optional<float> RayAABBIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& boxMin, const glm::vec3& boxMax) {

    glm::vec3 invDir = 1.0f / rayDir;

    float t1 = (boxMin.x - rayOrigin.x) * invDir.x;
    float t2 = (boxMax.x - rayOrigin.x) * invDir.x;
    float t3 = (boxMin.y - rayOrigin.y) * invDir.y;
    float t4 = (boxMax.y - rayOrigin.y) * invDir.y;
    float t5 = (boxMin.z - rayOrigin.z) * invDir.z;
    float t6 = (boxMax.z - rayOrigin.z) * invDir.z;

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0 || tmin > tmax) {
        return std::nullopt;
    }

    return tmin >= 0 ? tmin : tmax;
}

// Point in AABB
bool PointInAABB(const glm::vec3& point, const glm::vec3& boxMin, const glm::vec3& boxMax) {
    return point.x >= boxMin.x && point.x <= boxMax.x &&
           point.y >= boxMin.y && point.y <= boxMax.y &&
           point.z >= boxMin.z && point.z <= boxMax.z;
}

// AABB-AABB intersection
bool AABBIntersection(
    const glm::vec3& aMin, const glm::vec3& aMax,
    const glm::vec3& bMin, const glm::vec3& bMax) {

    return aMin.x <= bMax.x && aMax.x >= bMin.x &&
           aMin.y <= bMax.y && aMax.y >= bMin.y &&
           aMin.z <= bMax.z && aMax.z >= bMin.z;
}

// Sphere-sphere intersection
bool SphereSphereIntersection(
    const glm::vec3& centerA, float radiusA,
    const glm::vec3& centerB, float radiusB) {

    float dist2 = glm::distance2(centerA, centerB);
    float radiusSum = radiusA + radiusB;
    return dist2 <= radiusSum * radiusSum;
}

// Closest point on line segment
glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 ab = b - a;
    float t = glm::dot(point - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

} // namespace Geometry

} // namespace Nova
