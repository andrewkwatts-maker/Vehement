#include <glm/glm.hpp>
#include <optional>
#include <cmath>
#include <algorithm>

namespace Nova {

namespace Geometry {

// Epsilon for floating point comparisons
constexpr float kEpsilon = 1e-6f;

/**
 * @brief Calculate ray-plane intersection
 * @param rayOrigin Starting point of the ray
 * @param rayDir Direction of the ray (should be normalized for accurate t)
 * @param planePoint Any point on the plane
 * @param planeNormal Normal vector of the plane (should be normalized)
 * @return Distance along ray to intersection, or nullopt if no intersection
 */
[[nodiscard]] std::optional<float> RayPlaneIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& planePoint, const glm::vec3& planeNormal) noexcept {

    const float denom = glm::dot(planeNormal, rayDir);
    if (std::abs(denom) < kEpsilon) {
        return std::nullopt;  // Ray parallel to plane
    }

    const float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0.0f) {
        return std::nullopt;  // Intersection behind ray
    }

    return t;
}

/**
 * @brief Calculate ray-sphere intersection
 * @param rayOrigin Starting point of the ray
 * @param rayDir Direction of the ray (should be normalized)
 * @param sphereCenter Center of the sphere
 * @param sphereRadius Radius of the sphere
 * @return Distance to nearest intersection, or nullopt if no intersection
 */
[[nodiscard]] std::optional<float> RaySphereIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& sphereCenter, float sphereRadius) noexcept {

    const glm::vec3 oc = rayOrigin - sphereCenter;
    const float a = glm::dot(rayDir, rayDir);
    const float halfB = glm::dot(oc, rayDir);
    const float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    const float discriminant = halfB * halfB - a * c;

    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    const float sqrtD = std::sqrt(discriminant);
    float t = (-halfB - sqrtD) / a;

    if (t < 0.0f) {
        t = (-halfB + sqrtD) / a;
        if (t < 0.0f) {
            return std::nullopt;
        }
    }

    return t;
}

/**
 * @brief Calculate ray-AABB intersection using slab method
 * @param rayOrigin Starting point of the ray
 * @param rayDir Direction of the ray
 * @param boxMin Minimum corner of AABB
 * @param boxMax Maximum corner of AABB
 * @return Distance to nearest intersection, or nullopt if no intersection
 */
[[nodiscard]] std::optional<float> RayAABBIntersection(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& boxMin, const glm::vec3& boxMax) noexcept {

    const glm::vec3 invDir = 1.0f / rayDir;

    const float t1 = (boxMin.x - rayOrigin.x) * invDir.x;
    const float t2 = (boxMax.x - rayOrigin.x) * invDir.x;
    const float t3 = (boxMin.y - rayOrigin.y) * invDir.y;
    const float t4 = (boxMax.y - rayOrigin.y) * invDir.y;
    const float t5 = (boxMin.z - rayOrigin.z) * invDir.z;
    const float t6 = (boxMax.z - rayOrigin.z) * invDir.z;

    const float tmin = std::max({std::min(t1, t2), std::min(t3, t4), std::min(t5, t6)});
    const float tmax = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    if (tmax < 0.0f || tmin > tmax) {
        return std::nullopt;
    }

    return tmin >= 0.0f ? tmin : tmax;
}

/**
 * @brief Test if a point is inside an AABB
 */
[[nodiscard]] bool PointInAABB(const glm::vec3& point, const glm::vec3& boxMin, const glm::vec3& boxMax) noexcept {
    return point.x >= boxMin.x && point.x <= boxMax.x &&
           point.y >= boxMin.y && point.y <= boxMax.y &&
           point.z >= boxMin.z && point.z <= boxMax.z;
}

/**
 * @brief Test if two AABBs intersect
 */
[[nodiscard]] bool AABBIntersection(
    const glm::vec3& aMin, const glm::vec3& aMax,
    const glm::vec3& bMin, const glm::vec3& bMax) noexcept {

    return aMin.x <= bMax.x && aMax.x >= bMin.x &&
           aMin.y <= bMax.y && aMax.y >= bMin.y &&
           aMin.z <= bMax.z && aMax.z >= bMin.z;
}

/**
 * @brief Test if two spheres intersect
 */
[[nodiscard]] bool SphereSphereIntersection(
    const glm::vec3& centerA, float radiusA,
    const glm::vec3& centerB, float radiusB) noexcept {

    const float dist2 = glm::distance2(centerA, centerB);
    const float radiusSum = radiusA + radiusB;
    return dist2 <= radiusSum * radiusSum;
}

/**
 * @brief Find the closest point on a line segment to a given point
 * @param point The query point
 * @param a Start of line segment
 * @param b End of line segment
 * @return The closest point on segment [a, b] to point
 */
[[nodiscard]] glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept {
    const glm::vec3 ab = b - a;
    const float abLen2 = glm::dot(ab, ab);

    // Handle degenerate case where a == b
    if (abLen2 < kEpsilon) {
        return a;
    }

    const float t = glm::clamp(glm::dot(point - a, ab) / abLen2, 0.0f, 1.0f);
    return a + t * ab;
}

/**
 * @brief Calculate distance from a point to a line segment
 */
[[nodiscard]] float DistanceToSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept {
    return glm::distance(point, ClosestPointOnSegment(point, a, b));
}

/**
 * @brief Calculate the squared distance from a point to a line segment
 */
[[nodiscard]] float DistanceToSegmentSquared(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept {
    return glm::distance2(point, ClosestPointOnSegment(point, a, b));
}

/**
 * @brief Test if a point is inside a sphere
 */
[[nodiscard]] bool PointInSphere(const glm::vec3& point, const glm::vec3& center, float radius) noexcept {
    return glm::distance2(point, center) <= radius * radius;
}

/**
 * @brief Calculate the barycentric coordinates of a point in a triangle
 * @param p The point to test
 * @param a, b, c Triangle vertices
 * @return Barycentric coordinates (u, v, w) where p = u*a + v*b + w*c
 */
[[nodiscard]] glm::vec3 Barycentric(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept {
    const glm::vec3 v0 = b - a;
    const glm::vec3 v1 = c - a;
    const glm::vec3 v2 = p - a;

    const float d00 = glm::dot(v0, v0);
    const float d01 = glm::dot(v0, v1);
    const float d11 = glm::dot(v1, v1);
    const float d20 = glm::dot(v2, v0);
    const float d21 = glm::dot(v2, v1);

    const float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < kEpsilon) {
        return glm::vec3(1.0f, 0.0f, 0.0f);  // Degenerate triangle
    }

    const float v = (d11 * d20 - d01 * d21) / denom;
    const float w = (d00 * d21 - d01 * d20) / denom;
    const float u = 1.0f - v - w;

    return glm::vec3(u, v, w);
}

} // namespace Geometry

} // namespace Nova
