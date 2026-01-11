#pragma once

#include "AABB.hpp"
#include "OBB.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

namespace Nova {

/**
 * @brief Plane equation Ax + By + Cz + D = 0
 */
struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;

    constexpr Plane() noexcept = default;
    constexpr Plane(const glm::vec3& n, float d) noexcept
        : normal(n), distance(d) {}

    /**
     * @brief Create plane from normal and point on plane
     */
    [[nodiscard]] static Plane FromPointNormal(const glm::vec3& point, const glm::vec3& normal) noexcept {
        glm::vec3 n = glm::normalize(normal);
        return Plane(n, -glm::dot(n, point));
    }

    /**
     * @brief Create plane from three points
     */
    [[nodiscard]] static Plane FromPoints(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        return FromPointNormal(a, normal);
    }

    /**
     * @brief Get signed distance from point to plane
     * Positive = front of plane, Negative = behind plane
     */
    [[nodiscard]] constexpr float SignedDistance(const glm::vec3& point) const noexcept {
        return glm::dot(normal, point) + distance;
    }

    /**
     * @brief Get point on plane closest to given point
     */
    [[nodiscard]] constexpr glm::vec3 ClosestPoint(const glm::vec3& point) const noexcept {
        return point - normal * SignedDistance(point);
    }

    /**
     * @brief Normalize the plane equation
     */
    void Normalize() noexcept {
        float len = glm::length(normal);
        if (len > 0.0f) {
            normal /= len;
            distance /= len;
        }
    }
};

/**
 * @brief Frustum plane indices
 */
enum class FrustumPlane : uint8_t {
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3,
    Near = 4,
    Far = 5,
    Count = 6
};

/**
 * @brief Intersection result for frustum culling
 */
enum class FrustumResult : uint8_t {
    Outside = 0,    // Completely outside frustum
    Inside = 1,     // Completely inside frustum
    Intersect = 2   // Partially inside (intersects frustum boundary)
};

/**
 * @brief View frustum for culling operations
 *
 * Extracts frustum planes from view-projection matrix and provides
 * efficient intersection tests with AABB, sphere, and OBB.
 * Supports coherency culling for temporal coherence optimization.
 */
class Frustum {
public:
    Frustum() noexcept = default;

    /**
     * @brief Extract frustum planes from view-projection matrix
     * @param viewProjection Combined view * projection matrix
     */
    explicit Frustum(const glm::mat4& viewProjection) noexcept {
        ExtractPlanes(viewProjection);
    }

    /**
     * @brief Extract frustum planes from separate view and projection matrices
     */
    Frustum(const glm::mat4& view, const glm::mat4& projection) noexcept {
        ExtractPlanes(projection * view);
    }

    /**
     * @brief Update frustum from new view-projection matrix
     */
    void Update(const glm::mat4& viewProjection) noexcept {
        ExtractPlanes(viewProjection);
    }

    /**
     * @brief Update frustum from separate matrices
     */
    void Update(const glm::mat4& view, const glm::mat4& projection) noexcept {
        ExtractPlanes(projection * view);
    }

    // =========================================================================
    // Plane Access
    // =========================================================================

    /**
     * @brief Get frustum plane by index
     */
    [[nodiscard]] const Plane& GetPlane(FrustumPlane plane) const noexcept {
        return m_planes[static_cast<size_t>(plane)];
    }

    /**
     * @brief Get all frustum planes
     */
    [[nodiscard]] const std::array<Plane, 6>& GetPlanes() const noexcept {
        return m_planes;
    }

    /**
     * @brief Get frustum corners (8 corners of the view frustum)
     */
    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const noexcept;

    // =========================================================================
    // Point Tests
    // =========================================================================

    /**
     * @brief Test if point is inside frustum
     */
    [[nodiscard]] bool ContainsPoint(const glm::vec3& point) const noexcept;

    // =========================================================================
    // Sphere Tests
    // =========================================================================

    /**
     * @brief Test sphere against frustum
     */
    [[nodiscard]] FrustumResult TestSphere(const glm::vec3& center, float radius) const noexcept;

    /**
     * @brief Quick test if sphere is completely outside
     */
    [[nodiscard]] bool IsSphereOutside(const glm::vec3& center, float radius) const noexcept;

    /**
     * @brief Quick test if sphere intersects or is inside
     */
    [[nodiscard]] bool IsSphereVisible(const glm::vec3& center, float radius) const noexcept {
        return !IsSphereOutside(center, radius);
    }

    // =========================================================================
    // AABB Tests
    // =========================================================================

    /**
     * @brief Test AABB against frustum
     */
    [[nodiscard]] FrustumResult TestAABB(const AABB& aabb) const noexcept;

    /**
     * @brief Quick test if AABB is completely outside
     */
    [[nodiscard]] bool IsAABBOutside(const AABB& aabb) const noexcept;

    /**
     * @brief Quick test if AABB intersects or is inside
     */
    [[nodiscard]] bool IsAABBVisible(const AABB& aabb) const noexcept {
        return !IsAABBOutside(aabb);
    }

    /**
     * @brief Test AABB with plane masking for coherency culling
     * @param aabb The AABB to test
     * @param planeMask In/out mask of planes to test (1 = test, 0 = skip)
     * @return true if AABB is potentially visible
     */
    [[nodiscard]] bool TestAABBCoherent(const AABB& aabb, uint8_t& planeMask) const noexcept;

    // =========================================================================
    // OBB Tests
    // =========================================================================

    /**
     * @brief Test OBB against frustum
     */
    [[nodiscard]] FrustumResult TestOBB(const OBB& obb) const noexcept;

    /**
     * @brief Quick test if OBB is completely outside
     */
    [[nodiscard]] bool IsOBBOutside(const OBB& obb) const noexcept;

    // =========================================================================
    // SIMD Batch Tests
    // =========================================================================

#ifdef __SSE__
    /**
     * @brief Test 4 AABBs simultaneously using SIMD
     * @param boxes Array of 4 AABBs to test
     * @return Bitmask where bit i is set if box i is visible
     */
    [[nodiscard]] uint32_t TestAABB4(const AABB* boxes) const noexcept;

    /**
     * @brief Test 4 spheres simultaneously using SIMD
     * @param centers Array of 4 sphere centers
     * @param radii Array of 4 radii
     * @return Bitmask where bit i is set if sphere i is visible
     */
    [[nodiscard]] uint32_t TestSphere4(const glm::vec3* centers, const float* radii) const noexcept;
#endif

    // =========================================================================
    // Coherency Culling Support
    // =========================================================================

    /**
     * @brief Temporal coherency data for an object
     */
    struct CoherencyData {
        uint8_t lastPlane = 0;      // Index of last failing plane
        uint8_t planeMask = 0x3F;   // Mask of planes that need testing
        bool wasVisible = true;     // Was visible last frame
    };

    /**
     * @brief Test AABB with temporal coherency optimization
     * @param aabb The AABB to test
     * @param coherency Coherency data (updated by this call)
     * @return true if AABB is visible
     */
    [[nodiscard]] bool TestAABBCoherent(const AABB& aabb, CoherencyData& coherency) const noexcept;

private:
    void ExtractPlanes(const glm::mat4& viewProjection) noexcept;

    std::array<Plane, 6> m_planes;
    glm::mat4 m_viewProjection{1.0f};
    glm::mat4 m_inverseViewProjection{1.0f};

    // Cached for SIMD operations
#ifdef __SSE__
    alignas(16) float m_planeNormalsX[8];
    alignas(16) float m_planeNormalsY[8];
    alignas(16) float m_planeNormalsZ[8];
    alignas(16) float m_planeDistances[8];
#endif
};

/**
 * @brief Batch frustum culling helper
 *
 * Efficiently culls many objects against a frustum using
 * SIMD and temporal coherency optimizations.
 */
class FrustumCuller {
public:
    /**
     * @brief Initialize with frustum for this frame
     */
    void SetFrustum(const Frustum& frustum) noexcept {
        m_frustum = &frustum;
    }

    /**
     * @brief Cull a batch of AABBs
     * @param boxes Array of AABBs to cull
     * @param count Number of boxes
     * @param outVisible Output array of visibility flags
     */
    void CullAABBs(const AABB* boxes, size_t count, bool* outVisible) const noexcept;

    /**
     * @brief Cull with coherency data
     */
    void CullAABBsCoherent(
        const AABB* boxes,
        Frustum::CoherencyData* coherency,
        size_t count,
        bool* outVisible) const noexcept;

private:
    const Frustum* m_frustum = nullptr;
};

} // namespace Nova
