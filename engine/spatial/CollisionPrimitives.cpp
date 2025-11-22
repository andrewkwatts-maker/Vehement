#include "CollisionPrimitives.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

// =========================================================================
// Capsule Implementation
// =========================================================================

glm::vec3 Capsule::ClosestPointOnAxis(const glm::vec3& point) const noexcept {
    glm::vec3 axis = end - start;
    float axisLength2 = glm::dot(axis, axis);

    if (axisLength2 < 1e-6f) {
        return start; // Degenerate capsule
    }

    float t = glm::dot(point - start, axis) / axisLength2;
    t = glm::clamp(t, 0.0f, 1.0f);

    return start + axis * t;
}

glm::vec3 Capsule::GetSupport(const glm::vec3& direction) const noexcept {
    glm::vec3 dir = glm::normalize(direction);

    // Choose the hemisphere that's most in the direction
    float dotStart = glm::dot(start, direction);
    float dotEnd = glm::dot(end, direction);

    glm::vec3 base = (dotEnd >= dotStart) ? end : start;
    return base + dir * radius;
}

bool Capsule::Contains(const glm::vec3& point) const noexcept {
    glm::vec3 closest = ClosestPointOnAxis(point);
    float dist2 = glm::dot(point - closest, point - closest);
    return dist2 <= radius * radius;
}

// =========================================================================
// Cylinder Implementation
// =========================================================================

AABB Cylinder::GetBounds() const noexcept {
    // Project radius perpendicular to axis
    glm::vec3 absAxis(std::abs(axis.x), std::abs(axis.y), std::abs(axis.z));

    glm::vec3 extent;
    extent.x = std::sqrt(1.0f - axis.x * axis.x) * radius + absAxis.x * halfHeight;
    extent.y = std::sqrt(1.0f - axis.y * axis.y) * radius + absAxis.y * halfHeight;
    extent.z = std::sqrt(1.0f - axis.z * axis.z) * radius + absAxis.z * halfHeight;

    return AABB(center - extent, center + extent);
}

glm::vec3 Cylinder::GetSupport(const glm::vec3& direction) const noexcept {
    // Project direction onto plane perpendicular to axis
    float axisProj = glm::dot(direction, axis);
    glm::vec3 perpDir = direction - axis * axisProj;

    float perpLen = glm::length(perpDir);

    glm::vec3 result = center;

    // Move along axis
    result += axis * (axisProj >= 0.0f ? halfHeight : -halfHeight);

    // Move perpendicular to axis by radius
    if (perpLen > 1e-6f) {
        result += (perpDir / perpLen) * radius;
    }

    return result;
}

bool Cylinder::Contains(const glm::vec3& point) const noexcept {
    glm::vec3 toPoint = point - center;

    // Project onto axis
    float axisProj = glm::dot(toPoint, axis);
    if (std::abs(axisProj) > halfHeight) {
        return false;
    }

    // Check radial distance
    glm::vec3 perpendicular = toPoint - axis * axisProj;
    float perpDist2 = glm::dot(perpendicular, perpendicular);

    return perpDist2 <= radius * radius;
}

// =========================================================================
// ConvexHull Implementation
// =========================================================================

ConvexHull::ConvexHull(std::span<const glm::vec3> vertices)
    : m_vertices(vertices.begin(), vertices.end()) {

    if (m_vertices.empty()) return;

    // Calculate bounds and center
    m_bounds = AABB::Invalid();
    m_center = glm::vec3(0.0f);

    for (const auto& v : m_vertices) {
        m_bounds.Expand(v);
        m_center += v;
    }

    m_center /= static_cast<float>(m_vertices.size());
}

ConvexHull ConvexHull::FromOBB(const OBB& obb) {
    auto corners = obb.GetCorners();
    return ConvexHull(std::span(corners.data(), corners.size()));
}

glm::vec3 ConvexHull::GetSupport(const glm::vec3& direction) const noexcept {
    if (m_vertices.empty()) return glm::vec3(0.0f);

    float maxDot = glm::dot(m_vertices[0], direction);
    glm::vec3 result = m_vertices[0];

    for (size_t i = 1; i < m_vertices.size(); ++i) {
        float d = glm::dot(m_vertices[i], direction);
        if (d > maxDot) {
            maxDot = d;
            result = m_vertices[i];
        }
    }

    return result;
}

bool ConvexHull::Contains(const glm::vec3& point) const noexcept {
    // Simple test using support function
    // Point is inside if it's "behind" all faces
    // This is a basic implementation; a proper one would use face planes
    if (!m_bounds.Contains(point)) return false;

    // Use GJK with a point (sphere of radius 0)
    Sphere pointSphere(point, 0.0f);
    return GJK::Intersects(*this, pointSphere);
}

// =========================================================================
// Collision Namespace - Specialized Tests
// =========================================================================

namespace Collision {

glm::vec3 ClosestPointOnSegment(
    const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept {

    glm::vec3 ab = b - a;
    float t = glm::dot(point - a, ab);

    if (t <= 0.0f) return a;

    float denom = glm::dot(ab, ab);
    if (t >= denom) return b;

    return a + ab * (t / denom);
}

std::pair<glm::vec3, glm::vec3> ClosestPointsOnSegments(
    const glm::vec3& a1, const glm::vec3& a2,
    const glm::vec3& b1, const glm::vec3& b2) noexcept {

    glm::vec3 d1 = a2 - a1;
    glm::vec3 d2 = b2 - b1;
    glm::vec3 r = a1 - b1;

    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s, t;

    if (a < 1e-6f && e < 1e-6f) {
        // Both segments degenerate to points
        return {a1, b1};
    }

    if (a < 1e-6f) {
        // First segment degenerates
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(d1, r);
        if (e < 1e-6f) {
            // Second segment degenerates
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;

            if (denom != 0.0f) {
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }

            t = (b * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    return {a1 + d1 * s, b1 + d2 * t};
}

bool TestSphereSphere(const Sphere& a, const Sphere& b) noexcept {
    float dist2 = glm::dot(b.center - a.center, b.center - a.center);
    float radiusSum = a.radius + b.radius;
    return dist2 <= radiusSum * radiusSum;
}

std::optional<Contact> GetContactSphereSphere(const Sphere& a, const Sphere& b) noexcept {
    glm::vec3 diff = b.center - a.center;
    float dist2 = glm::dot(diff, diff);
    float radiusSum = a.radius + b.radius;

    if (dist2 > radiusSum * radiusSum) {
        return std::nullopt;
    }

    Contact contact;
    float dist = std::sqrt(dist2);

    if (dist > 1e-6f) {
        contact.normal = diff / dist;
    } else {
        contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    contact.penetration = radiusSum - dist;
    contact.point = a.center + contact.normal * (a.radius - contact.penetration * 0.5f);

    return contact;
}

bool TestSphereCapsule(const Sphere& sphere, const Capsule& capsule) noexcept {
    glm::vec3 closest = capsule.ClosestPointOnAxis(sphere.center);
    float dist2 = glm::dot(sphere.center - closest, sphere.center - closest);
    float radiusSum = sphere.radius + capsule.radius;
    return dist2 <= radiusSum * radiusSum;
}

std::optional<Contact> GetContactSphereCapsule(
    const Sphere& sphere, const Capsule& capsule) noexcept {

    glm::vec3 closest = capsule.ClosestPointOnAxis(sphere.center);
    glm::vec3 diff = sphere.center - closest;
    float dist2 = glm::dot(diff, diff);
    float radiusSum = sphere.radius + capsule.radius;

    if (dist2 > radiusSum * radiusSum) {
        return std::nullopt;
    }

    Contact contact;
    float dist = std::sqrt(dist2);

    if (dist > 1e-6f) {
        contact.normal = diff / dist;
    } else {
        contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    contact.penetration = radiusSum - dist;
    contact.point = closest + contact.normal * capsule.radius;

    return contact;
}

bool TestSphereOBB(const Sphere& sphere, const OBB& obb) noexcept {
    return obb.IntersectsSphere(sphere.center, sphere.radius);
}

std::optional<Contact> GetContactSphereOBB(const Sphere& sphere, const OBB& obb) noexcept {
    glm::vec3 closest = obb.ClosestPoint(sphere.center);
    glm::vec3 diff = sphere.center - closest;
    float dist2 = glm::dot(diff, diff);

    if (dist2 > sphere.radius * sphere.radius) {
        return std::nullopt;
    }

    Contact contact;
    float dist = std::sqrt(dist2);

    if (dist > 1e-6f) {
        contact.normal = diff / dist;
        contact.penetration = sphere.radius - dist;
        contact.point = closest;
    } else {
        // Sphere center is inside OBB - find closest face
        glm::vec3 local = obb.WorldToLocal(sphere.center);
        float minDist = obb.halfExtents.x - std::abs(local.x);
        int minAxis = 0;

        for (int i = 1; i < 3; ++i) {
            float d = obb.halfExtents[i] - std::abs(local[i]);
            if (d < minDist) {
                minDist = d;
                minAxis = i;
            }
        }

        contact.normal = obb.GetAxes()[minAxis] * (local[minAxis] >= 0.0f ? 1.0f : -1.0f);
        contact.penetration = sphere.radius + minDist;
        contact.point = sphere.center - contact.normal * sphere.radius;
    }

    return contact;
}

bool TestCapsuleCapsule(const Capsule& a, const Capsule& b) noexcept {
    auto [closestA, closestB] = ClosestPointsOnSegments(a.start, a.end, b.start, b.end);
    float dist2 = glm::dot(closestB - closestA, closestB - closestA);
    float radiusSum = a.radius + b.radius;
    return dist2 <= radiusSum * radiusSum;
}

std::optional<Contact> GetContactCapsuleCapsule(const Capsule& a, const Capsule& b) noexcept {
    auto [closestA, closestB] = ClosestPointsOnSegments(a.start, a.end, b.start, b.end);
    glm::vec3 diff = closestB - closestA;
    float dist2 = glm::dot(diff, diff);
    float radiusSum = a.radius + b.radius;

    if (dist2 > radiusSum * radiusSum) {
        return std::nullopt;
    }

    Contact contact;
    float dist = std::sqrt(dist2);

    if (dist > 1e-6f) {
        contact.normal = diff / dist;
    } else {
        contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    contact.penetration = radiusSum - dist;
    contact.point = closestA + contact.normal * a.radius;

    return contact;
}

bool TestCapsuleOBB(const Capsule& capsule, const OBB& obb) noexcept {
    // Test capsule as thick line segment against OBB
    // First check if capsule axis intersects inflated OBB
    OBB inflated = obb;
    inflated.halfExtents += glm::vec3(capsule.radius);

    // Then test endpoints
    if (obb.IntersectsSphere(capsule.start, capsule.radius)) return true;
    if (obb.IntersectsSphere(capsule.end, capsule.radius)) return true;

    // Test capsule segment against OBB edges (simplified)
    glm::vec3 closest = obb.ClosestPoint(capsule.ClosestPointOnAxis(obb.center));
    glm::vec3 capsulePoint = capsule.ClosestPointOnAxis(closest);
    float dist2 = glm::dot(closest - capsulePoint, closest - capsulePoint);

    return dist2 <= capsule.radius * capsule.radius;
}

bool TestOBBOBB(const OBB& a, const OBB& b) noexcept {
    return a.Intersects(b);
}

std::optional<Contact> GetContactOBBOBB(const OBB& a, const OBB& b) noexcept {
    float depth;
    glm::vec3 normal;

    if (!a.GetPenetration(b, depth, normal)) {
        return std::nullopt;
    }

    Contact contact;
    contact.normal = normal;
    contact.penetration = depth;

    // Approximate contact point (center of overlap region)
    contact.point = (a.center + b.center) * 0.5f;

    return contact;
}

std::optional<float> RayCapsule(const Ray& ray, const Capsule& capsule) noexcept {
    glm::vec3 ab = capsule.end - capsule.start;
    glm::vec3 ao = ray.origin - capsule.start;

    float abab = glm::dot(ab, ab);
    float abao = glm::dot(ab, ao);
    float abrd = glm::dot(ab, ray.direction);
    float aord = glm::dot(ao, ray.direction);
    float aoao = glm::dot(ao, ao);
    float rdrd = glm::dot(ray.direction, ray.direction);

    float a = abab * rdrd - abrd * abrd;
    float b = 2.0f * (abab * aord - abao * abrd);
    float c = abab * (aoao - capsule.radius * capsule.radius) - abao * abao;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        // Check sphere caps
        Sphere startCap(capsule.start, capsule.radius);
        Sphere endCap(capsule.end, capsule.radius);

        float t1 = startCap.RayIntersect(ray);
        float t2 = endCap.RayIntersect(ray);

        if (t1 >= 0.0f && t2 >= 0.0f) return std::min(t1, t2);
        if (t1 >= 0.0f) return t1;
        if (t2 >= 0.0f) return t2;
        return std::nullopt;
    }

    float t = (-b - std::sqrt(discriminant)) / (2.0f * a);

    // Check if hit is on cylinder part
    float s = (abao + t * abrd) / abab;

    if (s >= 0.0f && s <= 1.0f && t >= 0.0f) {
        return t;
    }

    // Check sphere caps
    Sphere cap = (s < 0.0f) ? Sphere(capsule.start, capsule.radius)
                            : Sphere(capsule.end, capsule.radius);
    float tCap = cap.RayIntersect(ray);

    return tCap >= 0.0f ? std::optional(tCap) : std::nullopt;
}

std::optional<float> RayCylinder(const Ray& ray, const Cylinder& cylinder) noexcept {
    // Transform ray to cylinder local space
    glm::vec3 localOrigin = ray.origin - cylinder.center;
    glm::vec3 localDir = ray.direction;

    // Project out the axis component
    float originAxis = glm::dot(localOrigin, cylinder.axis);
    float dirAxis = glm::dot(localDir, cylinder.axis);

    glm::vec3 originPerp = localOrigin - cylinder.axis * originAxis;
    glm::vec3 dirPerp = localDir - cylinder.axis * dirAxis;

    // Solve quadratic for infinite cylinder
    float a = glm::dot(dirPerp, dirPerp);
    float b = 2.0f * glm::dot(originPerp, dirPerp);
    float c = glm::dot(originPerp, originPerp) - cylinder.radius * cylinder.radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return std::nullopt;

    float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (t < 0.0f) {
        t = (-b + std::sqrt(discriminant)) / (2.0f * a);
        if (t < 0.0f) return std::nullopt;
    }

    // Check if hit is within cylinder height
    float hitAxis = originAxis + t * dirAxis;
    if (std::abs(hitAxis) <= cylinder.halfHeight) {
        return t;
    }

    // Check end caps
    // ... (simplified - just return no hit for now)
    return std::nullopt;
}

} // namespace Collision

// =========================================================================
// GJK Implementation
// =========================================================================

namespace GJK {

namespace {

bool DoSimplex2(Simplex& simplex, glm::vec3& direction) {
    glm::vec3 a = simplex[0];
    glm::vec3 b = simplex[1];

    glm::vec3 ab = b - a;
    glm::vec3 ao = -a;

    if (glm::dot(ab, ao) > 0.0f) {
        direction = glm::cross(glm::cross(ab, ao), ab);
    } else {
        simplex.size = 1;
        direction = ao;
    }

    return false;
}

bool DoSimplex3(Simplex& simplex, glm::vec3& direction) {
    glm::vec3 a = simplex[0];
    glm::vec3 b = simplex[1];
    glm::vec3 c = simplex[2];

    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);

    if (glm::dot(glm::cross(abc, ac), ao) > 0.0f) {
        if (glm::dot(ac, ao) > 0.0f) {
            simplex.points[1] = c;
            simplex.size = 2;
            direction = glm::cross(glm::cross(ac, ao), ac);
        } else {
            simplex.size = 2;
            return DoSimplex2(simplex, direction);
        }
    } else {
        if (glm::dot(glm::cross(ab, abc), ao) > 0.0f) {
            simplex.size = 2;
            return DoSimplex2(simplex, direction);
        } else {
            if (glm::dot(abc, ao) > 0.0f) {
                direction = abc;
            } else {
                simplex.points[1] = c;
                simplex.points[2] = b;
                direction = -abc;
            }
        }
    }

    return false;
}

bool DoSimplex4(Simplex& simplex, glm::vec3& direction) {
    glm::vec3 a = simplex[0];
    glm::vec3 b = simplex[1];
    glm::vec3 c = simplex[2];
    glm::vec3 d = simplex[3];

    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ad = d - a;
    glm::vec3 ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);
    glm::vec3 acd = glm::cross(ac, ad);
    glm::vec3 adb = glm::cross(ad, ab);

    if (glm::dot(abc, ao) > 0.0f) {
        simplex.points[0] = a;
        simplex.points[1] = b;
        simplex.points[2] = c;
        simplex.size = 3;
        return DoSimplex3(simplex, direction);
    }

    if (glm::dot(acd, ao) > 0.0f) {
        simplex.points[0] = a;
        simplex.points[1] = c;
        simplex.points[2] = d;
        simplex.size = 3;
        return DoSimplex3(simplex, direction);
    }

    if (glm::dot(adb, ao) > 0.0f) {
        simplex.points[0] = a;
        simplex.points[1] = d;
        simplex.points[2] = b;
        simplex.size = 3;
        return DoSimplex3(simplex, direction);
    }

    return true; // Origin is inside tetrahedron
}

bool DoSimplex(Simplex& simplex, glm::vec3& direction) {
    switch (simplex.size) {
        case 2: return DoSimplex2(simplex, direction);
        case 3: return DoSimplex3(simplex, direction);
        case 4: return DoSimplex4(simplex, direction);
    }
    return false;
}

} // anonymous namespace

// Template instantiations for common shape pairs
template<>
bool Intersects(const Sphere& a, const Sphere& b) noexcept {
    return Collision::TestSphereSphere(a, b);
}

template<>
bool Intersects(const OBB& a, const OBB& b) noexcept {
    return a.Intersects(b);
}

template<>
bool Intersects(const ConvexHull& a, const ConvexHull& b) noexcept {
    glm::vec3 direction = b.GetCenter() - a.GetCenter();
    if (glm::dot(direction, direction) < 1e-6f) {
        direction = glm::vec3(1, 0, 0);
    }

    Simplex simplex;
    SupportPoint support = GetSupport(a, b, direction);
    simplex.PushFront(support.point);

    direction = -support.point;

    constexpr int maxIterations = 64;
    for (int i = 0; i < maxIterations; ++i) {
        support = GetSupport(a, b, direction);

        if (glm::dot(support.point, direction) <= 0.0f) {
            return false; // No intersection
        }

        simplex.PushFront(support.point);

        if (DoSimplex(simplex, direction)) {
            return true; // Intersection found
        }
    }

    return false;
}

} // namespace GJK

// =========================================================================
// CollisionShape Implementation
// =========================================================================

CollisionShape::CollisionShape(const Sphere& sphere) noexcept
    : m_type(CollisionShapeType::Sphere) {
    m_data.sphere = sphere;
}

CollisionShape::CollisionShape(const Capsule& capsule) noexcept
    : m_type(CollisionShapeType::Capsule) {
    m_data.capsule = capsule;
}

CollisionShape::CollisionShape(const Cylinder& cylinder) noexcept
    : m_type(CollisionShapeType::Cylinder) {
    m_data.cylinder = cylinder;
}

CollisionShape::CollisionShape(const OBB& obb) noexcept
    : m_type(CollisionShapeType::Box) {
    m_data.obb = obb;
}

CollisionShape::CollisionShape(ConvexHull hull) noexcept
    : m_type(CollisionShapeType::ConvexHull)
    , m_hull(std::make_unique<ConvexHull>(std::move(hull))) {}

CollisionShapeType CollisionShape::GetType() const noexcept {
    return m_type;
}

AABB CollisionShape::GetBounds() const noexcept {
    switch (m_type) {
        case CollisionShapeType::Sphere:
            return m_data.sphere.GetBounds();
        case CollisionShapeType::Capsule:
            return m_data.capsule.GetBounds();
        case CollisionShapeType::Cylinder:
            return m_data.cylinder.GetBounds();
        case CollisionShapeType::Box:
            return m_data.obb.GetBoundingAABB();
        case CollisionShapeType::ConvexHull:
            return m_hull ? m_hull->GetBounds() : AABB();
        default:
            return AABB();
    }
}

glm::vec3 CollisionShape::GetSupport(const glm::vec3& direction) const noexcept {
    switch (m_type) {
        case CollisionShapeType::Sphere:
            return m_data.sphere.center + glm::normalize(direction) * m_data.sphere.radius;
        case CollisionShapeType::Capsule:
            return m_data.capsule.GetSupport(direction);
        case CollisionShapeType::Cylinder:
            return m_data.cylinder.GetSupport(direction);
        case CollisionShapeType::Box:
            return m_data.obb.GetSupport(direction);
        case CollisionShapeType::ConvexHull:
            return m_hull ? m_hull->GetSupport(direction) : glm::vec3(0);
        default:
            return glm::vec3(0);
    }
}

glm::vec3 CollisionShape::GetCenter() const noexcept {
    switch (m_type) {
        case CollisionShapeType::Sphere:
            return m_data.sphere.center;
        case CollisionShapeType::Capsule:
            return (m_data.capsule.start + m_data.capsule.end) * 0.5f;
        case CollisionShapeType::Cylinder:
            return m_data.cylinder.center;
        case CollisionShapeType::Box:
            return m_data.obb.center;
        case CollisionShapeType::ConvexHull:
            return m_hull ? m_hull->GetCenter() : glm::vec3(0);
        default:
            return glm::vec3(0);
    }
}

bool CollisionShape::Contains(const glm::vec3& point) const noexcept {
    switch (m_type) {
        case CollisionShapeType::Sphere:
            return m_data.sphere.Contains(point);
        case CollisionShapeType::Capsule:
            return m_data.capsule.Contains(point);
        case CollisionShapeType::Cylinder:
            return m_data.cylinder.Contains(point);
        case CollisionShapeType::Box:
            return m_data.obb.Contains(point);
        case CollisionShapeType::ConvexHull:
            return m_hull ? m_hull->Contains(point) : false;
        default:
            return false;
    }
}

const Sphere* CollisionShape::AsSphere() const noexcept {
    return m_type == CollisionShapeType::Sphere ? &m_data.sphere : nullptr;
}

const Capsule* CollisionShape::AsCapsule() const noexcept {
    return m_type == CollisionShapeType::Capsule ? &m_data.capsule : nullptr;
}

const Cylinder* CollisionShape::AsCylinder() const noexcept {
    return m_type == CollisionShapeType::Cylinder ? &m_data.cylinder : nullptr;
}

const OBB* CollisionShape::AsOBB() const noexcept {
    return m_type == CollisionShapeType::Box ? &m_data.obb : nullptr;
}

const ConvexHull* CollisionShape::AsConvexHull() const noexcept {
    return m_type == CollisionShapeType::ConvexHull ? m_hull.get() : nullptr;
}

bool TestCollision(const CollisionShape& a, const CollisionShape& b) noexcept {
    // Fast AABB pre-test
    if (!a.GetBounds().Intersects(b.GetBounds())) {
        return false;
    }

    // Specialized tests for common pairs
    if (a.GetType() == CollisionShapeType::Sphere) {
        if (b.GetType() == CollisionShapeType::Sphere) {
            return Collision::TestSphereSphere(*a.AsSphere(), *b.AsSphere());
        }
        if (b.GetType() == CollisionShapeType::Capsule) {
            return Collision::TestSphereCapsule(*a.AsSphere(), *b.AsCapsule());
        }
        if (b.GetType() == CollisionShapeType::Box) {
            return Collision::TestSphereOBB(*a.AsSphere(), *b.AsOBB());
        }
    }

    if (a.GetType() == CollisionShapeType::Capsule) {
        if (b.GetType() == CollisionShapeType::Sphere) {
            return Collision::TestSphereCapsule(*b.AsSphere(), *a.AsCapsule());
        }
        if (b.GetType() == CollisionShapeType::Capsule) {
            return Collision::TestCapsuleCapsule(*a.AsCapsule(), *b.AsCapsule());
        }
        if (b.GetType() == CollisionShapeType::Box) {
            return Collision::TestCapsuleOBB(*a.AsCapsule(), *b.AsOBB());
        }
    }

    if (a.GetType() == CollisionShapeType::Box && b.GetType() == CollisionShapeType::Box) {
        return Collision::TestOBBOBB(*a.AsOBB(), *b.AsOBB());
    }

    // Fall back to GJK for other combinations
    // (simplified - would need proper implementation)
    return false;
}

std::optional<Contact> GetContact(const CollisionShape& a, const CollisionShape& b) noexcept {
    if (!a.GetBounds().Intersects(b.GetBounds())) {
        return std::nullopt;
    }

    // Specialized contact generation
    if (a.GetType() == CollisionShapeType::Sphere) {
        if (b.GetType() == CollisionShapeType::Sphere) {
            return Collision::GetContactSphereSphere(*a.AsSphere(), *b.AsSphere());
        }
        if (b.GetType() == CollisionShapeType::Capsule) {
            return Collision::GetContactSphereCapsule(*a.AsSphere(), *b.AsCapsule());
        }
        if (b.GetType() == CollisionShapeType::Box) {
            return Collision::GetContactSphereOBB(*a.AsSphere(), *b.AsOBB());
        }
    }

    if (a.GetType() == CollisionShapeType::Capsule) {
        if (b.GetType() == CollisionShapeType::Capsule) {
            return Collision::GetContactCapsuleCapsule(*a.AsCapsule(), *b.AsCapsule());
        }
    }

    if (a.GetType() == CollisionShapeType::Box && b.GetType() == CollisionShapeType::Box) {
        return Collision::GetContactOBBOBB(*a.AsOBB(), *b.AsOBB());
    }

    return std::nullopt;
}

} // namespace Nova
