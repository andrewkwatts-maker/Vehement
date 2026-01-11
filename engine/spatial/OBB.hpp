#pragma once

#include "AABB.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>

namespace Nova {

/**
 * @brief Oriented Bounding Box with full rotation support
 *
 * Represents a 3D box that can be oriented in any direction.
 * Uses center, half-extents, and orientation quaternion representation.
 * Supports SAT (Separating Axis Theorem) intersection tests.
 */
struct OBB {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion

    // Cached axes (recomputed when orientation changes)
    mutable glm::vec3 axes[3]{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
    mutable bool axesDirty = true;

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr OBB() noexcept = default;

    OBB(const glm::vec3& c, const glm::vec3& extents, const glm::quat& orient = glm::quat(1, 0, 0, 0)) noexcept
        : center(c), halfExtents(extents), orientation(orient), axesDirty(true) {}

    OBB(const glm::vec3& c, const glm::vec3& extents, const glm::mat3& rotation) noexcept
        : center(c), halfExtents(extents), orientation(glm::quat_cast(rotation)), axesDirty(true) {}

    /**
     * @brief Create OBB from AABB (axis-aligned)
     */
    [[nodiscard]] static OBB FromAABB(const AABB& aabb) noexcept {
        return OBB(aabb.GetCenter(), aabb.GetExtents());
    }

    /**
     * @brief Create OBB that bounds a set of points
     */
    template<typename Iterator>
    [[nodiscard]] static OBB FromPoints(Iterator begin, Iterator end);

    // =========================================================================
    // Axis Access
    // =========================================================================

    /**
     * @brief Get local X axis (right) in world space
     */
    [[nodiscard]] const glm::vec3& GetAxisX() const noexcept {
        UpdateAxes();
        return axes[0];
    }

    /**
     * @brief Get local Y axis (up) in world space
     */
    [[nodiscard]] const glm::vec3& GetAxisY() const noexcept {
        UpdateAxes();
        return axes[1];
    }

    /**
     * @brief Get local Z axis (forward) in world space
     */
    [[nodiscard]] const glm::vec3& GetAxisZ() const noexcept {
        UpdateAxes();
        return axes[2];
    }

    /**
     * @brief Get all three axes
     */
    [[nodiscard]] const glm::vec3* GetAxes() const noexcept {
        UpdateAxes();
        return axes;
    }

    /**
     * @brief Get rotation matrix
     */
    [[nodiscard]] glm::mat3 GetRotationMatrix() const noexcept {
        return glm::mat3_cast(orientation);
    }

    /**
     * @brief Set orientation from Euler angles (degrees)
     */
    void SetEulerAngles(const glm::vec3& eulerDegrees) noexcept {
        orientation = glm::quat(glm::radians(eulerDegrees));
        axesDirty = true;
    }

    /**
     * @brief Set orientation from rotation matrix
     */
    void SetRotation(const glm::mat3& rotation) noexcept {
        orientation = glm::quat_cast(rotation);
        axesDirty = true;
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /**
     * @brief Get all 8 corner vertices
     */
    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const noexcept;

    /**
     * @brief Get bounding AABB that contains this OBB
     */
    [[nodiscard]] AABB GetBoundingAABB() const noexcept;

    /**
     * @brief Get volume
     */
    [[nodiscard]] float GetVolume() const noexcept {
        return 8.0f * halfExtents.x * halfExtents.y * halfExtents.z;
    }

    /**
     * @brief Get surface area
     */
    [[nodiscard]] float GetSurfaceArea() const noexcept {
        return 8.0f * (halfExtents.x * halfExtents.y +
                       halfExtents.y * halfExtents.z +
                       halfExtents.z * halfExtents.x);
    }

    // =========================================================================
    // Point Queries
    // =========================================================================

    /**
     * @brief Test if point is inside OBB
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept;

    /**
     * @brief Get closest point on OBB surface to given point
     */
    [[nodiscard]] glm::vec3 ClosestPoint(const glm::vec3& point) const noexcept;

    /**
     * @brief Get squared distance from point to OBB
     */
    [[nodiscard]] float DistanceSquared(const glm::vec3& point) const noexcept;

    /**
     * @brief Get distance from point to OBB
     */
    [[nodiscard]] float Distance(const glm::vec3& point) const noexcept;

    /**
     * @brief Transform point from world space to OBB local space
     */
    [[nodiscard]] glm::vec3 WorldToLocal(const glm::vec3& worldPoint) const noexcept;

    /**
     * @brief Transform point from OBB local space to world space
     */
    [[nodiscard]] glm::vec3 LocalToWorld(const glm::vec3& localPoint) const noexcept;

    // =========================================================================
    // Intersection Tests (SAT)
    // =========================================================================

    /**
     * @brief Test intersection with another OBB using SAT
     */
    [[nodiscard]] bool Intersects(const OBB& other) const noexcept;

    /**
     * @brief Test intersection with AABB using SAT
     */
    [[nodiscard]] bool Intersects(const AABB& aabb) const noexcept;

    /**
     * @brief Test intersection with sphere
     */
    [[nodiscard]] bool IntersectsSphere(const glm::vec3& sphereCenter, float radius) const noexcept;

    /**
     * @brief Ray intersection test
     * @return Distance to intersection or negative if no hit
     */
    [[nodiscard]] float RayIntersect(const Ray& ray) const noexcept;

    /**
     * @brief Ray intersection with normal
     */
    [[nodiscard]] bool RayIntersect(const Ray& ray, float& outT, glm::vec3& outNormal) const noexcept;

    // =========================================================================
    // Collision Response
    // =========================================================================

    /**
     * @brief Get penetration depth and normal for OBB-OBB collision
     * @return true if collision, with penetration data
     */
    [[nodiscard]] bool GetPenetration(const OBB& other, float& depth, glm::vec3& normal) const noexcept;

    /**
     * @brief Get support point in given direction (for GJK/EPA)
     */
    [[nodiscard]] glm::vec3 GetSupport(const glm::vec3& direction) const noexcept;

    // =========================================================================
    // Transform
    // =========================================================================

    /**
     * @brief Transform OBB by translation and rotation
     */
    [[nodiscard]] OBB Transform(const glm::vec3& translation, const glm::quat& rotation) const noexcept;

    /**
     * @brief Transform OBB by a 4x4 matrix
     */
    [[nodiscard]] OBB Transform(const glm::mat4& matrix) const noexcept;

private:
    void UpdateAxes() const noexcept {
        if (axesDirty) {
            glm::mat3 rot = glm::mat3_cast(orientation);
            axes[0] = rot[0];
            axes[1] = rot[1];
            axes[2] = rot[2];
            axesDirty = false;
        }
    }
};

// =========================================================================
// SAT Helper Functions
// =========================================================================

namespace SAT {

/**
 * @brief Project OBB onto axis and get min/max
 */
void ProjectOBB(const OBB& obb, const glm::vec3& axis, float& outMin, float& outMax) noexcept;

/**
 * @brief Project AABB onto axis and get min/max
 */
void ProjectAABB(const AABB& aabb, const glm::vec3& axis, float& outMin, float& outMax) noexcept;

/**
 * @brief Test if projections overlap
 */
[[nodiscard]] bool Overlaps(float minA, float maxA, float minB, float maxB) noexcept;

/**
 * @brief Get overlap amount (positive if overlapping)
 */
[[nodiscard]] float GetOverlap(float minA, float maxA, float minB, float maxB) noexcept;

} // namespace SAT

} // namespace Nova
