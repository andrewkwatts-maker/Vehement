#pragma once

#include "AABB.hpp"
#include "OBB.hpp"
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <optional>
#include <span>

namespace Nova {

/**
 * @brief Collision shape types
 */
enum class CollisionShapeType : uint8_t {
    None = 0,
    Sphere,
    Capsule,
    Cylinder,
    Box,          // OBB
    ConvexHull,
    TriangleMesh
};

/**
 * @brief Collision contact information
 */
struct Contact {
    glm::vec3 point{0.0f};           // Contact point in world space
    glm::vec3 normal{0.0f, 1.0f, 0.0f}; // Contact normal (from A to B)
    float penetration = 0.0f;         // Penetration depth (positive = overlapping)

    [[nodiscard]] bool IsValid() const noexcept {
        return penetration > 0.0f;
    }
};

/**
 * @brief Collision manifold - multiple contact points
 */
struct ContactManifold {
    static constexpr size_t MAX_CONTACTS = 4;

    std::array<Contact, MAX_CONTACTS> contacts;
    size_t numContacts = 0;

    void AddContact(const Contact& contact) noexcept {
        if (numContacts < MAX_CONTACTS) {
            contacts[numContacts++] = contact;
        }
    }

    void Clear() noexcept { numContacts = 0; }

    [[nodiscard]] bool HasContacts() const noexcept { return numContacts > 0; }
};

// =========================================================================
// Collision Primitives
// =========================================================================

/**
 * @brief Capsule collision shape (sphere-swept line segment)
 */
struct Capsule {
    glm::vec3 start{0.0f, 0.0f, 0.0f};
    glm::vec3 end{0.0f, 1.0f, 0.0f};
    float radius = 0.5f;

    constexpr Capsule() noexcept = default;
    Capsule(const glm::vec3& s, const glm::vec3& e, float r) noexcept
        : start(s), end(e), radius(r) {}

    /**
     * @brief Create vertical capsule from base position and height
     */
    [[nodiscard]] static Capsule FromHeight(const glm::vec3& base, float height, float radius) noexcept {
        return Capsule(
            base + glm::vec3(0.0f, radius, 0.0f),
            base + glm::vec3(0.0f, height - radius, 0.0f),
            radius
        );
    }

    /**
     * @brief Get AABB bounds
     */
    [[nodiscard]] AABB GetBounds() const noexcept {
        glm::vec3 r(radius);
        return AABB(
            glm::min(start, end) - r,
            glm::max(start, end) + r
        );
    }

    /**
     * @brief Get the line segment axis
     */
    [[nodiscard]] glm::vec3 GetAxis() const noexcept {
        return end - start;
    }

    /**
     * @brief Get length of the capsule (not including hemisphere caps)
     */
    [[nodiscard]] float GetLength() const noexcept {
        return glm::length(end - start);
    }

    /**
     * @brief Get total height (including caps)
     */
    [[nodiscard]] float GetTotalHeight() const noexcept {
        return GetLength() + 2.0f * radius;
    }

    /**
     * @brief Get closest point on capsule axis to given point
     */
    [[nodiscard]] glm::vec3 ClosestPointOnAxis(const glm::vec3& point) const noexcept;

    /**
     * @brief Get support point in given direction (for GJK)
     */
    [[nodiscard]] glm::vec3 GetSupport(const glm::vec3& direction) const noexcept;

    /**
     * @brief Test if point is inside capsule
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept;
};

/**
 * @brief Cylinder collision shape
 */
struct Cylinder {
    glm::vec3 center{0.0f};
    glm::vec3 axis{0.0f, 1.0f, 0.0f};  // Normalized axis direction
    float halfHeight = 0.5f;
    float radius = 0.5f;

    constexpr Cylinder() noexcept = default;
    Cylinder(const glm::vec3& c, const glm::vec3& a, float hh, float r) noexcept
        : center(c), axis(glm::normalize(a)), halfHeight(hh), radius(r) {}

    /**
     * @brief Create vertical cylinder
     */
    [[nodiscard]] static Cylinder Vertical(const glm::vec3& center, float height, float radius) noexcept {
        return Cylinder(center, glm::vec3(0, 1, 0), height * 0.5f, radius);
    }

    /**
     * @brief Get AABB bounds
     */
    [[nodiscard]] AABB GetBounds() const noexcept;

    /**
     * @brief Get top and bottom centers
     */
    [[nodiscard]] glm::vec3 GetTop() const noexcept {
        return center + axis * halfHeight;
    }

    [[nodiscard]] glm::vec3 GetBottom() const noexcept {
        return center - axis * halfHeight;
    }

    /**
     * @brief Get support point for GJK
     */
    [[nodiscard]] glm::vec3 GetSupport(const glm::vec3& direction) const noexcept;

    /**
     * @brief Test if point is inside cylinder
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept;
};

/**
 * @brief Convex hull collision shape
 */
class ConvexHull {
public:
    ConvexHull() noexcept = default;

    /**
     * @brief Create convex hull from vertices
     */
    explicit ConvexHull(std::span<const glm::vec3> vertices);

    /**
     * @brief Create convex hull from OBB
     */
    [[nodiscard]] static ConvexHull FromOBB(const OBB& obb);

    /**
     * @brief Get AABB bounds
     */
    [[nodiscard]] AABB GetBounds() const noexcept { return m_bounds; }

    /**
     * @brief Get support point in given direction (for GJK/EPA)
     */
    [[nodiscard]] glm::vec3 GetSupport(const glm::vec3& direction) const noexcept;

    /**
     * @brief Get all vertices
     */
    [[nodiscard]] std::span<const glm::vec3> GetVertices() const noexcept {
        return m_vertices;
    }

    /**
     * @brief Get center of mass
     */
    [[nodiscard]] glm::vec3 GetCenter() const noexcept { return m_center; }

    /**
     * @brief Test if point is inside convex hull
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept;

private:
    std::vector<glm::vec3> m_vertices;
    AABB m_bounds;
    glm::vec3 m_center{0.0f};
};

// =========================================================================
// GJK Algorithm (Gilbert-Johnson-Keerthi)
// =========================================================================

namespace GJK {

/**
 * @brief Simplex structure for GJK algorithm
 */
struct Simplex {
    std::array<glm::vec3, 4> points;
    size_t size = 0;

    void PushFront(const glm::vec3& point) noexcept {
        for (size_t i = std::min(size, size_t(3)); i > 0; --i) {
            points[i] = points[i - 1];
        }
        points[0] = point;
        size = std::min(size + 1, size_t(4));
    }

    glm::vec3& operator[](size_t i) { return points[i]; }
    const glm::vec3& operator[](size_t i) const { return points[i]; }
};

/**
 * @brief Support point in Minkowski difference
 */
struct SupportPoint {
    glm::vec3 point;     // Point in Minkowski difference
    glm::vec3 pointA;    // Support on shape A
    glm::vec3 pointB;    // Support on shape B
};

/**
 * @brief Get support point on Minkowski difference A - B
 */
template<typename ShapeA, typename ShapeB>
[[nodiscard]] SupportPoint GetSupport(
    const ShapeA& a, const ShapeB& b, const glm::vec3& direction) noexcept {
    SupportPoint result;
    result.pointA = a.GetSupport(direction);
    result.pointB = b.GetSupport(-direction);
    result.point = result.pointA - result.pointB;
    return result;
}

/**
 * @brief GJK intersection test
 * @return true if shapes intersect
 */
template<typename ShapeA, typename ShapeB>
[[nodiscard]] bool Intersects(const ShapeA& a, const ShapeB& b) noexcept;

/**
 * @brief GJK with penetration depth via EPA
 * @return Contact information if shapes intersect
 */
template<typename ShapeA, typename ShapeB>
[[nodiscard]] std::optional<Contact> GetContact(const ShapeA& a, const ShapeB& b) noexcept;

} // namespace GJK

// =========================================================================
// EPA Algorithm (Expanding Polytope Algorithm)
// =========================================================================

namespace EPA {

constexpr float EPA_TOLERANCE = 1e-4f;
constexpr size_t EPA_MAX_ITERATIONS = 64;

/**
 * @brief Calculate penetration depth and normal using EPA
 * @param simplex Initial simplex from GJK (must be a tetrahedron)
 * @param shapeA First shape
 * @param shapeB Second shape
 * @return Contact with penetration info
 */
template<typename ShapeA, typename ShapeB>
[[nodiscard]] Contact ExpandPolytope(
    const std::vector<GJK::SupportPoint>& simplex,
    const ShapeA& shapeA,
    const ShapeB& shapeB) noexcept;

} // namespace EPA

// =========================================================================
// Specialized Collision Tests (Optimized paths)
// =========================================================================

namespace Collision {

// Sphere-Sphere
[[nodiscard]] bool TestSphereSphere(
    const Sphere& a, const Sphere& b) noexcept;

[[nodiscard]] std::optional<Contact> GetContactSphereSphere(
    const Sphere& a, const Sphere& b) noexcept;

// Sphere-Capsule
[[nodiscard]] bool TestSphereCapsule(
    const Sphere& sphere, const Capsule& capsule) noexcept;

[[nodiscard]] std::optional<Contact> GetContactSphereCapsule(
    const Sphere& sphere, const Capsule& capsule) noexcept;

// Sphere-OBB
[[nodiscard]] bool TestSphereOBB(
    const Sphere& sphere, const OBB& obb) noexcept;

[[nodiscard]] std::optional<Contact> GetContactSphereOBB(
    const Sphere& sphere, const OBB& obb) noexcept;

// Capsule-Capsule
[[nodiscard]] bool TestCapsuleCapsule(
    const Capsule& a, const Capsule& b) noexcept;

[[nodiscard]] std::optional<Contact> GetContactCapsuleCapsule(
    const Capsule& a, const Capsule& b) noexcept;

// Capsule-OBB
[[nodiscard]] bool TestCapsuleOBB(
    const Capsule& capsule, const OBB& obb) noexcept;

// OBB-OBB (uses SAT from OBB.hpp)
[[nodiscard]] bool TestOBBOBB(const OBB& a, const OBB& b) noexcept;

[[nodiscard]] std::optional<Contact> GetContactOBBOBB(
    const OBB& a, const OBB& b) noexcept;

// Ray tests
[[nodiscard]] std::optional<float> RayCapsule(
    const Ray& ray, const Capsule& capsule) noexcept;

[[nodiscard]] std::optional<float> RayCylinder(
    const Ray& ray, const Cylinder& cylinder) noexcept;

// Closest point queries
[[nodiscard]] glm::vec3 ClosestPointOnSegment(
    const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept;

[[nodiscard]] std::pair<glm::vec3, glm::vec3> ClosestPointsOnSegments(
    const glm::vec3& a1, const glm::vec3& a2,
    const glm::vec3& b1, const glm::vec3& b2) noexcept;

} // namespace Collision

// =========================================================================
// Collision Shape Interface (Type-erased)
// =========================================================================

/**
 * @brief Abstract collision shape interface
 */
class ICollisionShape {
public:
    virtual ~ICollisionShape() = default;

    [[nodiscard]] virtual CollisionShapeType GetType() const noexcept = 0;
    [[nodiscard]] virtual AABB GetBounds() const noexcept = 0;
    [[nodiscard]] virtual glm::vec3 GetSupport(const glm::vec3& direction) const noexcept = 0;
    [[nodiscard]] virtual glm::vec3 GetCenter() const noexcept = 0;
    [[nodiscard]] virtual bool Contains(const glm::vec3& point) const noexcept = 0;
};

/**
 * @brief Type-erased collision shape wrapper
 */
class CollisionShape {
public:
    CollisionShape() noexcept = default;

    explicit CollisionShape(const Sphere& sphere) noexcept;
    explicit CollisionShape(const Capsule& capsule) noexcept;
    explicit CollisionShape(const Cylinder& cylinder) noexcept;
    explicit CollisionShape(const OBB& obb) noexcept;
    explicit CollisionShape(ConvexHull hull) noexcept;

    [[nodiscard]] CollisionShapeType GetType() const noexcept;
    [[nodiscard]] AABB GetBounds() const noexcept;
    [[nodiscard]] glm::vec3 GetSupport(const glm::vec3& direction) const noexcept;
    [[nodiscard]] glm::vec3 GetCenter() const noexcept;
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept;

    [[nodiscard]] bool IsValid() const noexcept { return m_type != CollisionShapeType::None; }

    // Access to underlying shapes
    [[nodiscard]] const Sphere* AsSphere() const noexcept;
    [[nodiscard]] const Capsule* AsCapsule() const noexcept;
    [[nodiscard]] const Cylinder* AsCylinder() const noexcept;
    [[nodiscard]] const OBB* AsOBB() const noexcept;
    [[nodiscard]] const ConvexHull* AsConvexHull() const noexcept;

private:
    CollisionShapeType m_type = CollisionShapeType::None;

    // Storage for small shapes (union-like)
    union ShapeData {
        Sphere sphere;
        Capsule capsule;
        Cylinder cylinder;
        OBB obb;

        ShapeData() : sphere() {}
    } m_data;

    // For convex hull (heap allocated)
    std::unique_ptr<ConvexHull> m_hull;
};

/**
 * @brief Test collision between two type-erased shapes
 */
[[nodiscard]] bool TestCollision(
    const CollisionShape& a, const CollisionShape& b) noexcept;

/**
 * @brief Get contact between two type-erased shapes
 */
[[nodiscard]] std::optional<Contact> GetContact(
    const CollisionShape& a, const CollisionShape& b) noexcept;

} // namespace Nova
