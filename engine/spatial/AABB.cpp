#include "AABB.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

bool AABB::IntersectsSphere(const glm::vec3& center, float radius) const noexcept {
    // Find closest point on AABB to sphere center
    glm::vec3 closest = ClosestPoint(center);

    // Calculate squared distance from closest point to sphere center
    glm::vec3 diff = closest - center;
    float dist2 = glm::dot(diff, diff);

    return dist2 <= radius * radius;
}

bool AABB::IntersectsRay(
    const glm::vec3& origin,
    const glm::vec3& invDir,
    float& tMin,
    float& tMax) const noexcept {

    // Slab method for ray-AABB intersection
    glm::vec3 t0 = (min - origin) * invDir;
    glm::vec3 t1 = (max - origin) * invDir;

    glm::vec3 tSmall = glm::min(t0, t1);
    glm::vec3 tBig = glm::max(t0, t1);

    tMin = std::max({tSmall.x, tSmall.y, tSmall.z});
    tMax = std::min({tBig.x, tBig.y, tBig.z});

    return tMax >= tMin && tMax >= 0.0f;
}

float AABB::RayIntersect(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance) const noexcept {

    glm::vec3 invDir = glm::vec3(
        1.0f / direction.x,
        1.0f / direction.y,
        1.0f / direction.z
    );

    float tMin, tMax;
    if (!IntersectsRay(origin, invDir, tMin, tMax)) {
        return -1.0f;
    }

    float t = tMin >= 0.0f ? tMin : tMax;
    if (t < 0.0f || t > maxDistance) {
        return -1.0f;
    }

    return t;
}

#ifdef __SSE__
bool AABB::IntersectsRaySIMD(
    __m128 origin,
    __m128 invDir,
    __m128 dirSign,
    float& outTMin,
    float& outTMax) const noexcept {

    // Load AABB bounds
    __m128 boxMin = _mm_set_ps(0.0f, min.z, min.y, min.x);
    __m128 boxMax = _mm_set_ps(0.0f, max.z, max.y, max.x);

    // Reorder based on ray direction signs for optimal slab intersection
    __m128 tNear = _mm_mul_ps(_mm_sub_ps(boxMin, origin), invDir);
    __m128 tFar = _mm_mul_ps(_mm_sub_ps(boxMax, origin), invDir);

    // Swap based on direction
    __m128 tMin = _mm_min_ps(tNear, tFar);
    __m128 tMax = _mm_max_ps(tNear, tFar);

    // Horizontal max for tMin
    __m128 tMin_yzx = _mm_shuffle_ps(tMin, tMin, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tMin_zxy = _mm_shuffle_ps(tMin, tMin, _MM_SHUFFLE(3, 1, 0, 2));
    tMin = _mm_max_ps(tMin, _mm_max_ps(tMin_yzx, tMin_zxy));

    // Horizontal min for tMax
    __m128 tMax_yzx = _mm_shuffle_ps(tMax, tMax, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tMax_zxy = _mm_shuffle_ps(tMax, tMax, _MM_SHUFFLE(3, 1, 0, 2));
    tMax = _mm_min_ps(tMax, _mm_min_ps(tMax_yzx, tMax_zxy));

    // Extract results
    _mm_store_ss(&outTMin, tMin);
    _mm_store_ss(&outTMax, tMax);

    return outTMax >= outTMin && outTMax >= 0.0f;
}

uint32_t AABB::IntersectsRay4(
    const AABB* boxes,
    const glm::vec3& origin,
    const glm::vec3& invDir,
    float maxDist) noexcept {

    // Load ray data
    __m128 rayOriginX = _mm_set1_ps(origin.x);
    __m128 rayOriginY = _mm_set1_ps(origin.y);
    __m128 rayOriginZ = _mm_set1_ps(origin.z);

    __m128 rayInvDirX = _mm_set1_ps(invDir.x);
    __m128 rayInvDirY = _mm_set1_ps(invDir.y);
    __m128 rayInvDirZ = _mm_set1_ps(invDir.z);

    // Load 4 AABBs (SoA format would be better but we work with AoS here)
    __m128 minX = _mm_set_ps(boxes[3].min.x, boxes[2].min.x, boxes[1].min.x, boxes[0].min.x);
    __m128 minY = _mm_set_ps(boxes[3].min.y, boxes[2].min.y, boxes[1].min.y, boxes[0].min.y);
    __m128 minZ = _mm_set_ps(boxes[3].min.z, boxes[2].min.z, boxes[1].min.z, boxes[0].min.z);

    __m128 maxX = _mm_set_ps(boxes[3].max.x, boxes[2].max.x, boxes[1].max.x, boxes[0].max.x);
    __m128 maxY = _mm_set_ps(boxes[3].max.y, boxes[2].max.y, boxes[1].max.y, boxes[0].max.y);
    __m128 maxZ = _mm_set_ps(boxes[3].max.z, boxes[2].max.z, boxes[1].max.z, boxes[0].max.z);

    // Compute slab intersections
    __m128 t1x = _mm_mul_ps(_mm_sub_ps(minX, rayOriginX), rayInvDirX);
    __m128 t2x = _mm_mul_ps(_mm_sub_ps(maxX, rayOriginX), rayInvDirX);
    __m128 t1y = _mm_mul_ps(_mm_sub_ps(minY, rayOriginY), rayInvDirY);
    __m128 t2y = _mm_mul_ps(_mm_sub_ps(maxY, rayOriginY), rayInvDirY);
    __m128 t1z = _mm_mul_ps(_mm_sub_ps(minZ, rayOriginZ), rayInvDirZ);
    __m128 t2z = _mm_mul_ps(_mm_sub_ps(maxZ, rayOriginZ), rayInvDirZ);

    __m128 tMinX = _mm_min_ps(t1x, t2x);
    __m128 tMaxX = _mm_max_ps(t1x, t2x);
    __m128 tMinY = _mm_min_ps(t1y, t2y);
    __m128 tMaxY = _mm_max_ps(t1y, t2y);
    __m128 tMinZ = _mm_min_ps(t1z, t2z);
    __m128 tMaxZ = _mm_max_ps(t1z, t2z);

    __m128 tNear = _mm_max_ps(_mm_max_ps(tMinX, tMinY), tMinZ);
    __m128 tFar = _mm_min_ps(_mm_min_ps(tMaxX, tMaxY), tMaxZ);

    // Check valid intersections: tFar >= tNear && tFar >= 0 && tNear <= maxDist
    __m128 zero = _mm_setzero_ps();
    __m128 maxD = _mm_set1_ps(maxDist);

    __m128 validMask = _mm_and_ps(
        _mm_cmpge_ps(tFar, tNear),
        _mm_and_ps(
            _mm_cmpge_ps(tFar, zero),
            _mm_cmple_ps(tNear, maxD)
        )
    );

    return static_cast<uint32_t>(_mm_movemask_ps(validMask));
}
#endif

AABB AABB::Transform(const glm::mat4& matrix) const noexcept {
    // Graphics Gems method for transforming AABB
    // More efficient than transforming all 8 corners

    glm::vec3 newMin(matrix[3]);
    glm::vec3 newMax(matrix[3]);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float a = matrix[j][i] * min[j];
            float b = matrix[j][i] * max[j];

            if (a < b) {
                newMin[i] += a;
                newMax[i] += b;
            } else {
                newMin[i] += b;
                newMax[i] += a;
            }
        }
    }

    return AABB(newMin, newMax);
}

AABB AABB::TransformAffine(const glm::mat3& rotation, const glm::vec3& translation) const noexcept {
    glm::vec3 newMin = translation;
    glm::vec3 newMax = translation;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float a = rotation[j][i] * min[j];
            float b = rotation[j][i] * max[j];

            if (a < b) {
                newMin[i] += a;
                newMax[i] += b;
            } else {
                newMin[i] += b;
                newMax[i] += a;
            }
        }
    }

    return AABB(newMin, newMax);
}

float AABB::DistanceSquared(const glm::vec3& point) const noexcept {
    float dist2 = 0.0f;

    for (int i = 0; i < 3; ++i) {
        float v = point[i];
        if (v < min[i]) {
            float d = min[i] - v;
            dist2 += d * d;
        } else if (v > max[i]) {
            float d = v - max[i];
            dist2 += d * d;
        }
    }

    return dist2;
}

float AABB::Distance(const glm::vec3& point) const noexcept {
    return std::sqrt(DistanceSquared(point));
}

float Sphere::RayIntersect(const Ray& ray) const noexcept {
    glm::vec3 oc = ray.origin - center;

    float a = glm::dot(ray.direction, ray.direction);
    float halfB = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;

    float discriminant = halfB * halfB - a * c;

    if (discriminant < 0.0f) {
        return -1.0f;
    }

    float sqrtD = std::sqrt(discriminant);
    float t = (-halfB - sqrtD) / a;

    if (t < 0.0f) {
        t = (-halfB + sqrtD) / a;
        if (t < 0.0f) {
            return -1.0f;
        }
    }

    return t;
}

} // namespace Nova
