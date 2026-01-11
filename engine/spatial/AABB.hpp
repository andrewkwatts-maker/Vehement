#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <limits>
#include <cmath>

#ifdef __SSE__
#include <xmmintrin.h>
#include <smmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

namespace Nova {

// Forward declarations
struct Ray;
struct Sphere;
class Frustum;
class OBB;

/**
 * @brief Axis-Aligned Bounding Box with SIMD-optimized operations
 *
 * Represents a 3D rectangular volume aligned with the coordinate axes.
 * Used extensively for spatial queries, culling, and collision detection.
 */
struct alignas(16) AABB {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr AABB() noexcept = default;

    constexpr AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) noexcept
        : min(minPoint), max(maxPoint) {}

    /**
     * @brief Create AABB from center and half-extents
     */
    [[nodiscard]] static constexpr AABB FromCenterExtents(
        const glm::vec3& center, const glm::vec3& halfExtents) noexcept {
        return AABB(center - halfExtents, center + halfExtents);
    }

    /**
     * @brief Create AABB that contains a single point
     */
    [[nodiscard]] static constexpr AABB FromPoint(const glm::vec3& point) noexcept {
        return AABB(point, point);
    }

    /**
     * @brief Create AABB from a set of points
     */
    template<typename Iterator>
    [[nodiscard]] static AABB FromPoints(Iterator begin, Iterator end) noexcept {
        AABB result;
        for (auto it = begin; it != end; ++it) {
            result.Expand(*it);
        }
        return result;
    }

    /**
     * @brief Create an invalid/empty AABB
     */
    [[nodiscard]] static constexpr AABB Invalid() noexcept {
        return AABB();
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /**
     * @brief Get center point of the AABB
     */
    [[nodiscard]] constexpr glm::vec3 GetCenter() const noexcept {
        return (min + max) * 0.5f;
    }

    /**
     * @brief Get half-extents (half the size in each dimension)
     */
    [[nodiscard]] constexpr glm::vec3 GetExtents() const noexcept {
        return (max - min) * 0.5f;
    }

    /**
     * @brief Get full size in each dimension
     */
    [[nodiscard]] constexpr glm::vec3 GetSize() const noexcept {
        return max - min;
    }

    /**
     * @brief Get the volume of the AABB
     */
    [[nodiscard]] constexpr float GetVolume() const noexcept {
        glm::vec3 size = GetSize();
        return size.x * size.y * size.z;
    }

    /**
     * @brief Get surface area (used for SAH in BVH construction)
     */
    [[nodiscard]] constexpr float GetSurfaceArea() const noexcept {
        glm::vec3 d = GetSize();
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    /**
     * @brief Get half surface area (for SAH optimization)
     */
    [[nodiscard]] constexpr float GetHalfSurfaceArea() const noexcept {
        glm::vec3 d = GetSize();
        return d.x * d.y + d.y * d.z + d.z * d.x;
    }

    /**
     * @brief Check if AABB is valid (min <= max in all dimensions)
     */
    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    /**
     * @brief Get the longest axis (0=X, 1=Y, 2=Z)
     */
    [[nodiscard]] constexpr int GetLongestAxis() const noexcept {
        glm::vec3 size = GetSize();
        if (size.x >= size.y && size.x >= size.z) return 0;
        if (size.y >= size.z) return 1;
        return 2;
    }

    /**
     * @brief Get all 8 corner vertices
     */
    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const noexcept {
        return {{
            {min.x, min.y, min.z},
            {max.x, min.y, min.z},
            {min.x, max.y, min.z},
            {max.x, max.y, min.z},
            {min.x, min.y, max.z},
            {max.x, min.y, max.z},
            {min.x, max.y, max.z},
            {max.x, max.y, max.z}
        }};
    }

    // =========================================================================
    // Modification
    // =========================================================================

    /**
     * @brief Expand AABB to include a point
     */
    constexpr void Expand(const glm::vec3& point) noexcept {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    /**
     * @brief Expand AABB to include another AABB
     */
    constexpr void Expand(const AABB& other) noexcept {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    /**
     * @brief Expand AABB by a uniform amount in all directions
     */
    constexpr void Inflate(float amount) noexcept {
        min -= glm::vec3(amount);
        max += glm::vec3(amount);
    }

    /**
     * @brief Translate the AABB
     */
    constexpr void Translate(const glm::vec3& offset) noexcept {
        min += offset;
        max += offset;
    }

    /**
     * @brief Scale the AABB from its center
     */
    constexpr void Scale(float factor) noexcept {
        glm::vec3 center = GetCenter();
        glm::vec3 extents = GetExtents() * factor;
        min = center - extents;
        max = center + extents;
    }

    // =========================================================================
    // Static Operations
    // =========================================================================

    /**
     * @brief Merge two AABBs into one that contains both
     */
    [[nodiscard]] static constexpr AABB Merge(const AABB& a, const AABB& b) noexcept {
        return AABB(glm::min(a.min, b.min), glm::max(a.max, b.max));
    }

    /**
     * @brief Get intersection of two AABBs (may be invalid if no intersection)
     */
    [[nodiscard]] static constexpr AABB Intersection(const AABB& a, const AABB& b) noexcept {
        return AABB(glm::max(a.min, b.min), glm::min(a.max, b.max));
    }

    // =========================================================================
    // Intersection Tests
    // =========================================================================

    /**
     * @brief Test if point is inside AABB
     */
    [[nodiscard]] constexpr bool Contains(const glm::vec3& point) const noexcept {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    /**
     * @brief Test if another AABB is fully contained
     */
    [[nodiscard]] constexpr bool Contains(const AABB& other) const noexcept {
        return other.min.x >= min.x && other.max.x <= max.x &&
               other.min.y >= min.y && other.max.y <= max.y &&
               other.min.z >= min.z && other.max.z <= max.z;
    }

    /**
     * @brief Test intersection with another AABB
     */
    [[nodiscard]] constexpr bool Intersects(const AABB& other) const noexcept {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    /**
     * @brief Test intersection with sphere
     */
    [[nodiscard]] bool IntersectsSphere(const glm::vec3& center, float radius) const noexcept;

    /**
     * @brief Ray intersection test
     * @param origin Ray origin
     * @param invDir Inverse of ray direction (1/dir for each component)
     * @param tMin Minimum t value (output)
     * @param tMax Maximum t value (output)
     * @return true if ray intersects AABB
     */
    [[nodiscard]] bool IntersectsRay(
        const glm::vec3& origin,
        const glm::vec3& invDir,
        float& tMin,
        float& tMax) const noexcept;

    /**
     * @brief Ray intersection returning distance (or negative if no hit)
     */
    [[nodiscard]] float RayIntersect(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = std::numeric_limits<float>::max()) const noexcept;

    // =========================================================================
    // SIMD-Optimized Operations
    // =========================================================================

#ifdef __SSE__
    /**
     * @brief SIMD-accelerated ray-AABB intersection
     */
    [[nodiscard]] bool IntersectsRaySIMD(
        __m128 origin,
        __m128 invDir,
        __m128 dirSign,
        float& tMin,
        float& tMax) const noexcept;

    /**
     * @brief Test 4 AABBs against one ray simultaneously
     */
    [[nodiscard]] static uint32_t IntersectsRay4(
        const AABB* boxes,
        const glm::vec3& origin,
        const glm::vec3& invDir,
        float maxDist) noexcept;
#endif

    // =========================================================================
    // Transform
    // =========================================================================

    /**
     * @brief Transform AABB by a 4x4 matrix
     *
     * Uses the method from Graphics Gems to efficiently compute
     * the transformed AABB without computing all 8 corners.
     */
    [[nodiscard]] AABB Transform(const glm::mat4& matrix) const noexcept;

    /**
     * @brief Transform AABB by rotation and translation only
     */
    [[nodiscard]] AABB TransformAffine(const glm::mat3& rotation, const glm::vec3& translation) const noexcept;

    // =========================================================================
    // Distance Queries
    // =========================================================================

    /**
     * @brief Get closest point on AABB surface to a given point
     */
    [[nodiscard]] constexpr glm::vec3 ClosestPoint(const glm::vec3& point) const noexcept {
        return glm::clamp(point, min, max);
    }

    /**
     * @brief Get squared distance from point to AABB
     */
    [[nodiscard]] float DistanceSquared(const glm::vec3& point) const noexcept;

    /**
     * @brief Get distance from point to AABB
     */
    [[nodiscard]] float Distance(const glm::vec3& point) const noexcept;

    // =========================================================================
    // Comparison
    // =========================================================================

    [[nodiscard]] constexpr bool operator==(const AABB& other) const noexcept {
        return min == other.min && max == other.max;
    }

    [[nodiscard]] constexpr bool operator!=(const AABB& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Ray structure for intersection tests
 */
struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};

    constexpr Ray() noexcept = default;
    constexpr Ray(const glm::vec3& o, const glm::vec3& d) noexcept
        : origin(o), direction(glm::normalize(d)) {}

    /**
     * @brief Get point along ray at distance t
     */
    [[nodiscard]] constexpr glm::vec3 GetPoint(float t) const noexcept {
        return origin + direction * t;
    }

    /**
     * @brief Get inverse direction for optimized AABB tests
     */
    [[nodiscard]] glm::vec3 GetInverseDirection() const noexcept {
        return glm::vec3(
            1.0f / direction.x,
            1.0f / direction.y,
            1.0f / direction.z
        );
    }
};

/**
 * @brief Hit result from ray intersection queries
 */
struct RayHit {
    uint64_t entityId = 0;
    float distance = std::numeric_limits<float>::max();
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return distance < std::numeric_limits<float>::max();
    }

    [[nodiscard]] constexpr bool operator<(const RayHit& other) const noexcept {
        return distance < other.distance;
    }
};

/**
 * @brief Sphere primitive for spatial queries
 */
struct Sphere {
    glm::vec3 center{0.0f};
    float radius = 1.0f;

    constexpr Sphere() noexcept = default;
    constexpr Sphere(const glm::vec3& c, float r) noexcept
        : center(c), radius(r) {}

    /**
     * @brief Get AABB that bounds this sphere
     */
    [[nodiscard]] constexpr AABB GetBounds() const noexcept {
        glm::vec3 r(radius);
        return AABB(center - r, center + r);
    }

    /**
     * @brief Test if point is inside sphere
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept {
        float dist2 = glm::dot(point - center, point - center);
        return dist2 <= radius * radius;
    }

    /**
     * @brief Test intersection with another sphere
     */
    [[nodiscard]] bool Intersects(const Sphere& other) const noexcept {
        float dist2 = glm::dot(other.center - center, other.center - center);
        float radiusSum = radius + other.radius;
        return dist2 <= radiusSum * radiusSum;
    }

    /**
     * @brief Test intersection with AABB
     */
    [[nodiscard]] bool Intersects(const AABB& box) const noexcept {
        return box.IntersectsSphere(center, radius);
    }

    /**
     * @brief Ray intersection
     * @return Distance to intersection or negative if no hit
     */
    [[nodiscard]] float RayIntersect(const Ray& ray) const noexcept;
};

} // namespace Nova
