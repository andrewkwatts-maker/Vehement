#include "SDFCollision.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Nova {

// =============================================================================
// SDFContactManifold Implementation
// =============================================================================

void SDFContactManifold::AddContact(const glm::vec3& point, const glm::vec3& normal, float penetration) {
    if (contactCount >= MAX_CONTACTS) {
        // Replace the contact with smallest penetration if new one is deeper
        size_t minIndex = 0;
        float minPen = contacts[0].penetration;
        for (size_t i = 1; i < contactCount; ++i) {
            if (contacts[i].penetration < minPen) {
                minPen = contacts[i].penetration;
                minIndex = i;
            }
        }
        if (penetration > minPen) {
            contacts[minIndex] = {point, normal, penetration};
        }
    } else {
        contacts[contactCount++] = {point, normal, penetration};
    }

    if (penetration > maxPenetration) {
        maxPenetration = penetration;
    }
}

void SDFContactManifold::ComputeAverages() {
    if (contactCount == 0) {
        averageNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        return;
    }

    averageNormal = glm::vec3(0.0f);
    for (size_t i = 0; i < contactCount; ++i) {
        averageNormal += contacts[i].normal;
    }
    averageNormal = glm::normalize(averageNormal);
}

// =============================================================================
// SDFPointCollider Implementation
// =============================================================================

SDFPointCollider::SDFPointCollider(const glm::vec3& position)
    : m_position(position) {}

float SDFPointCollider::Distance(const glm::vec3& point) const {
    return glm::length(point - m_position);
}

glm::vec3 SDFPointCollider::Support(const glm::vec3& /*direction*/) const {
    return m_position;
}

AABB SDFPointCollider::GetAABB() const {
    AABB aabb;
    aabb.min = m_position;
    aabb.max = m_position;
    return aabb;
}

// =============================================================================
// SDFSphereCollider Implementation
// =============================================================================

SDFSphereCollider::SDFSphereCollider(const glm::vec3& center, float radius)
    : m_center(center), m_radius(radius) {}

float SDFSphereCollider::Distance(const glm::vec3& point) const {
    return glm::length(point - m_center) - m_radius;
}

glm::vec3 SDFSphereCollider::Support(const glm::vec3& direction) const {
    return m_center + glm::normalize(direction) * m_radius;
}

AABB SDFSphereCollider::GetAABB() const {
    AABB aabb;
    aabb.min = m_center - glm::vec3(m_radius);
    aabb.max = m_center + glm::vec3(m_radius);
    return aabb;
}

// =============================================================================
// SDFCapsuleCollider Implementation
// =============================================================================

SDFCapsuleCollider::SDFCapsuleCollider(const glm::vec3& start, const glm::vec3& end, float radius)
    : m_start(start), m_end(end), m_radius(radius) {}

float SDFCapsuleCollider::Distance(const glm::vec3& point) const {
    glm::vec3 closestPoint = SDFCollisionUtil::ClosestPointOnSegment(point, m_start, m_end);
    return glm::length(point - closestPoint) - m_radius;
}

glm::vec3 SDFCapsuleCollider::Support(const glm::vec3& direction) const {
    glm::vec3 normalizedDir = glm::normalize(direction);

    // Find which endpoint is furthest in the given direction
    float dotStart = glm::dot(m_start, normalizedDir);
    float dotEnd = glm::dot(m_end, normalizedDir);

    glm::vec3 furthestPoint = (dotStart > dotEnd) ? m_start : m_end;
    return furthestPoint + normalizedDir * m_radius;
}

glm::vec3 SDFCapsuleCollider::GetCenter() const {
    return (m_start + m_end) * 0.5f;
}

float SDFCapsuleCollider::GetBoundingRadius() const {
    return glm::length(m_end - m_start) * 0.5f + m_radius;
}

AABB SDFCapsuleCollider::GetAABB() const {
    AABB aabb;
    aabb.min = glm::min(m_start, m_end) - glm::vec3(m_radius);
    aabb.max = glm::max(m_start, m_end) + glm::vec3(m_radius);
    return aabb;
}

void SDFCapsuleCollider::SetEndpoints(const glm::vec3& start, const glm::vec3& end) {
    m_start = start;
    m_end = end;
}

// =============================================================================
// SDFBoxCollider Implementation
// =============================================================================

SDFBoxCollider::SDFBoxCollider(const glm::vec3& center, const glm::vec3& halfExtents,
                               const glm::quat& orientation)
    : m_center(center), m_halfExtents(halfExtents), m_orientation(orientation) {
    SetOrientation(orientation);
}

void SDFBoxCollider::SetOrientation(const glm::quat& orientation) {
    m_orientation = orientation;
    m_rotationMatrix = glm::toMat3(orientation);
    m_inverseRotation = glm::transpose(m_rotationMatrix);
}

float SDFBoxCollider::Distance(const glm::vec3& point) const {
    // Transform point to local space
    glm::vec3 localPoint = WorldToLocal(point);

    // SDF for box in local space
    glm::vec3 q = glm::abs(localPoint) - m_halfExtents;
    return glm::length(glm::max(q, glm::vec3(0.0f))) +
           glm::min(glm::max(q.x, glm::max(q.y, q.z)), 0.0f);
}

glm::vec3 SDFBoxCollider::Support(const glm::vec3& direction) const {
    glm::vec3 localDir = m_inverseRotation * direction;

    glm::vec3 localSupport(
        (localDir.x > 0.0f) ? m_halfExtents.x : -m_halfExtents.x,
        (localDir.y > 0.0f) ? m_halfExtents.y : -m_halfExtents.y,
        (localDir.z > 0.0f) ? m_halfExtents.z : -m_halfExtents.z
    );

    return LocalToWorld(localSupport);
}

float SDFBoxCollider::GetBoundingRadius() const {
    return glm::length(m_halfExtents);
}

AABB SDFBoxCollider::GetAABB() const {
    AABB aabb;
    aabb.min = glm::vec3(std::numeric_limits<float>::max());
    aabb.max = glm::vec3(std::numeric_limits<float>::lowest());

    // Transform all 8 corners and expand AABB
    auto corners = GetCorners();
    for (const auto& corner : corners) {
        aabb.min = glm::min(aabb.min, corner);
        aabb.max = glm::max(aabb.max, corner);
    }

    return aabb;
}

glm::vec3 SDFBoxCollider::GetAxis(int index) const {
    return m_rotationMatrix[index];
}

std::array<glm::vec3, 8> SDFBoxCollider::GetCorners() const {
    std::array<glm::vec3, 8> corners;
    int idx = 0;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                glm::vec3 localCorner(
                    static_cast<float>(x) * m_halfExtents.x,
                    static_cast<float>(y) * m_halfExtents.y,
                    static_cast<float>(z) * m_halfExtents.z
                );
                corners[idx++] = LocalToWorld(localCorner);
            }
        }
    }
    return corners;
}

glm::vec3 SDFBoxCollider::WorldToLocal(const glm::vec3& worldPoint) const {
    return m_inverseRotation * (worldPoint - m_center);
}

glm::vec3 SDFBoxCollider::LocalToWorld(const glm::vec3& localPoint) const {
    return m_center + m_rotationMatrix * localPoint;
}

// =============================================================================
// SDFMeshCollider Implementation
// =============================================================================

SDFMeshCollider::SDFMeshCollider(const std::vector<glm::vec3>& vertices, const glm::mat4& transform)
    : m_localVertices(vertices), m_transform(transform) {
    UpdateTransformedVertices();
    ComputeBounds();
}

float SDFMeshCollider::Distance(const glm::vec3& point) const {
    // For mesh collider, we return the minimum distance to any vertex
    // This is an approximation; true mesh SDF would require more complex computation
    float minDist = std::numeric_limits<float>::max();
    for (const auto& vertex : m_worldVertices) {
        float dist = glm::length(point - vertex);
        minDist = glm::min(minDist, dist);
    }
    return minDist;
}

glm::vec3 SDFMeshCollider::Support(const glm::vec3& direction) const {
    glm::vec3 result = m_worldVertices.empty() ? glm::vec3(0.0f) : m_worldVertices[0];
    float maxDot = -std::numeric_limits<float>::max();

    for (const auto& vertex : m_worldVertices) {
        float d = glm::dot(vertex, direction);
        if (d > maxDot) {
            maxDot = d;
            result = vertex;
        }
    }

    return result;
}

void SDFMeshCollider::SetTransform(const glm::mat4& transform) {
    m_transform = transform;
    UpdateTransformedVertices();
    ComputeBounds();
}

void SDFMeshCollider::UpdateTransformedVertices() {
    m_worldVertices.resize(m_localVertices.size());
    for (size_t i = 0; i < m_localVertices.size(); ++i) {
        glm::vec4 transformed = m_transform * glm::vec4(m_localVertices[i], 1.0f);
        m_worldVertices[i] = glm::vec3(transformed);
    }
}

void SDFMeshCollider::ComputeBounds() {
    if (m_worldVertices.empty()) {
        m_center = glm::vec3(0.0f);
        m_boundingRadius = 0.0f;
        m_aabb.min = glm::vec3(0.0f);
        m_aabb.max = glm::vec3(0.0f);
        return;
    }

    // Compute center
    m_center = glm::vec3(0.0f);
    for (const auto& v : m_worldVertices) {
        m_center += v;
    }
    m_center /= static_cast<float>(m_worldVertices.size());

    // Compute bounding radius and AABB
    m_boundingRadius = 0.0f;
    m_aabb.min = m_worldVertices[0];
    m_aabb.max = m_worldVertices[0];

    for (const auto& v : m_worldVertices) {
        m_boundingRadius = glm::max(m_boundingRadius, glm::length(v - m_center));
        m_aabb.min = glm::min(m_aabb.min, v);
        m_aabb.max = glm::max(m_aabb.max, v);
    }
}

// =============================================================================
// SDFCollisionScene Implementation
// =============================================================================

SDFCollisionScene::SDFCollisionScene(const SDFModel* model)
    : m_model(model) {}

float SDFCollisionScene::EvaluateSDF(const glm::vec3& point) const {
    if (m_sdfFunction) {
        return m_sdfFunction(point);
    }
    if (m_model) {
        return m_model->EvaluateSDF(point);
    }
    return std::numeric_limits<float>::max();
}

glm::vec3 SDFCollisionScene::CalculateGradient(const glm::vec3& point, float epsilon) const {
    return SDFCollisionUtil::ComputeSDFGradient(
        [this](const glm::vec3& p) { return EvaluateSDF(p); },
        point, epsilon);
}

glm::vec3 SDFCollisionScene::CalculateNormal(const glm::vec3& point, float epsilon) const {
    glm::vec3 gradient = CalculateGradient(point, epsilon);
    float len = glm::length(gradient);
    return (len > 0.0001f) ? gradient / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

std::optional<glm::vec3> SDFCollisionScene::FindClosestSurfacePoint(
    const glm::vec3& startPoint, int maxIterations, float tolerance) const {

    glm::vec3 currentPoint = startPoint;

    for (int i = 0; i < maxIterations; ++i) {
        float distance = EvaluateSDF(currentPoint);

        if (std::abs(distance) < tolerance) {
            return currentPoint;
        }

        glm::vec3 gradient = CalculateGradient(currentPoint);
        if (glm::length(gradient) < 0.0001f) {
            // Stuck at a local extremum
            return std::nullopt;
        }

        // Move towards surface
        currentPoint -= glm::normalize(gradient) * distance;
    }

    // Didn't converge
    return std::nullopt;
}

std::pair<glm::vec3, glm::vec3> SDFCollisionScene::GetBounds() const {
    if (m_model) {
        return m_model->GetBounds();
    }
    // Default bounds for custom SDF functions
    return {glm::vec3(-100.0f), glm::vec3(100.0f)};
}

// =============================================================================
// SDFCollisionSystem Implementation
// =============================================================================

SDFCollisionSystem::SDFCollisionSystem() = default;

SDFCollisionSystem::SDFCollisionSystem(const SDFCollisionConfig& config)
    : m_config(config) {}

// -----------------------------------------------------------------------------
// Point Queries
// -----------------------------------------------------------------------------

float SDFCollisionSystem::QueryDistance(const SDFCollisionScene& scene, const glm::vec3& point) const {
    m_stats.queryCount++;
    return scene.EvaluateSDF(point);
}

glm::vec3 SDFCollisionSystem::QueryNormal(const SDFCollisionScene& scene, const glm::vec3& point) const {
    m_stats.queryCount++;
    return scene.CalculateNormal(point, m_config.normalEpsilon);
}

bool SDFCollisionSystem::IsPointInside(const SDFCollisionScene& scene, const glm::vec3& point) const {
    m_stats.queryCount++;
    return scene.EvaluateSDF(point) < m_config.epsilon;
}

// -----------------------------------------------------------------------------
// Discrete Collision Detection
// -----------------------------------------------------------------------------

SDFCollisionResult SDFCollisionSystem::TestCollision(
    const ISDFCollider& collider,
    const SDFCollisionScene& scene) const {

    m_stats.collisionTests++;

    // Get collider center and evaluate SDF there first
    glm::vec3 center = collider.GetCenter();
    float boundingRadius = collider.GetBoundingRadius();

    // Quick reject: if center is far from surface, skip detailed test
    float centerDist = scene.EvaluateSDF(center);
    if (centerDist > boundingRadius + m_config.surfaceOffset) {
        return SDFCollisionResult{};
    }

    // Detailed test based on collider type
    const char* typeName = collider.GetTypeName();

    if (std::strcmp(typeName, "Sphere") == 0) {
        const auto& sphere = static_cast<const SDFSphereCollider&>(collider);
        return TestSphereCollision(sphere.GetCenter(), sphere.GetRadius(), scene);
    } else if (std::strcmp(typeName, "Capsule") == 0) {
        const auto& capsule = static_cast<const SDFCapsuleCollider&>(collider);
        return TestCapsuleCollision(capsule.GetStart(), capsule.GetEnd(),
                                    capsule.GetRadius(), scene);
    } else if (std::strcmp(typeName, "Box") == 0) {
        const auto& box = static_cast<const SDFBoxCollider&>(collider);
        return TestBoxCollision(box.GetCenter(), box.GetHalfExtents(),
                               box.GetOrientation(), scene);
    } else if (std::strcmp(typeName, "Mesh") == 0) {
        const auto& mesh = static_cast<const SDFMeshCollider&>(collider);
        return TestMeshCollision(mesh.GetVertices(), glm::mat4(1.0f), scene);
    }

    // Generic point-sampling approach for unknown types
    return TestSphereCollision(center, boundingRadius, scene);
}

SDFCollisionResult SDFCollisionSystem::TestSphereCollision(
    const glm::vec3& center, float radius,
    const SDFCollisionScene& scene) const {

    m_stats.collisionTests++;
    SDFCollisionResult result;

    // Evaluate SDF at sphere center
    float distance = scene.EvaluateSDF(center);

    // Check if sphere surface intersects SDF surface
    float effectiveRadius = radius + m_config.surfaceOffset;
    if (distance < effectiveRadius) {
        result.hit = true;
        result.distance = distance;
        result.penetrationDepth = effectiveRadius - distance;

        // Calculate contact normal
        result.normal = scene.CalculateNormal(center, m_config.normalEpsilon);

        // Contact point is on sphere surface in direction of SDF gradient
        result.point = center - result.normal * radius;
    }

    return result;
}

SDFCollisionResult SDFCollisionSystem::TestCapsuleCollision(
    const glm::vec3& start, const glm::vec3& end, float radius,
    const SDFCollisionScene& scene) const {

    m_stats.collisionTests++;
    SDFCollisionResult result;

    float effectiveRadius = radius + m_config.surfaceOffset;
    float minDistance = std::numeric_limits<float>::max();
    glm::vec3 closestPoint;

    // Sample along the capsule axis
    constexpr int SAMPLES = 8;
    for (int i = 0; i <= SAMPLES; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(SAMPLES);
        glm::vec3 samplePoint = glm::mix(start, end, t);

        float distance = scene.EvaluateSDF(samplePoint);
        if (distance < minDistance) {
            minDistance = distance;
            closestPoint = samplePoint;
        }
    }

    // Also check additional radial samples around the minimum point
    glm::vec3 axis = glm::normalize(end - start);
    glm::vec3 perpX, perpY;

    // Build perpendicular vectors
    if (std::abs(axis.x) < 0.9f) {
        perpX = glm::normalize(glm::cross(axis, glm::vec3(1, 0, 0)));
    } else {
        perpX = glm::normalize(glm::cross(axis, glm::vec3(0, 1, 0)));
    }
    perpY = glm::cross(axis, perpX);

    constexpr int RADIAL_SAMPLES = 4;
    for (int i = 0; i < RADIAL_SAMPLES; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(RADIAL_SAMPLES) * 6.28318f;
        glm::vec3 offset = (std::cos(angle) * perpX + std::sin(angle) * perpY) * radius;

        for (int j = 0; j <= 2; ++j) {
            float t = static_cast<float>(j) / 2.0f;
            glm::vec3 samplePoint = glm::mix(start, end, t) + offset;

            float distance = scene.EvaluateSDF(samplePoint);
            if (distance < minDistance) {
                minDistance = distance;
                closestPoint = samplePoint - offset; // Store axis point
            }
        }
    }

    if (minDistance < effectiveRadius) {
        result.hit = true;
        result.distance = minDistance;
        result.penetrationDepth = effectiveRadius - minDistance;
        result.normal = scene.CalculateNormal(closestPoint, m_config.normalEpsilon);
        result.point = closestPoint - result.normal * radius;
    }

    return result;
}

SDFCollisionResult SDFCollisionSystem::TestBoxCollision(
    const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::quat& orientation,
    const SDFCollisionScene& scene) const {

    m_stats.collisionTests++;
    SDFCollisionResult result;

    // Create temporary box collider for sample generation
    SDFBoxCollider box(center, halfExtents, orientation);

    // Sample corners and face centers
    auto corners = box.GetCorners();
    float minDistance = std::numeric_limits<float>::max();
    glm::vec3 closestPoint;

    for (const auto& corner : corners) {
        float distance = scene.EvaluateSDF(corner);
        if (distance < minDistance) {
            minDistance = distance;
            closestPoint = corner;
        }
    }

    // Sample face centers
    glm::mat3 rot = glm::toMat3(orientation);
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            glm::vec3 faceCenter = center + rot[axis] * halfExtents[axis] * static_cast<float>(sign);
            float distance = scene.EvaluateSDF(faceCenter);
            if (distance < minDistance) {
                minDistance = distance;
                closestPoint = faceCenter;
            }
        }
    }

    // Sample edge midpoints
    for (int axis1 = 0; axis1 < 3; ++axis1) {
        for (int axis2 = axis1 + 1; axis2 < 3; ++axis2) {
            for (int sign1 = -1; sign1 <= 1; sign1 += 2) {
                for (int sign2 = -1; sign2 <= 1; sign2 += 2) {
                    glm::vec3 edgeMid = center
                        + rot[axis1] * halfExtents[axis1] * static_cast<float>(sign1)
                        + rot[axis2] * halfExtents[axis2] * static_cast<float>(sign2);
                    float distance = scene.EvaluateSDF(edgeMid);
                    if (distance < minDistance) {
                        minDistance = distance;
                        closestPoint = edgeMid;
                    }
                }
            }
        }
    }

    float effectiveOffset = m_config.surfaceOffset;
    if (minDistance < effectiveOffset) {
        result.hit = true;
        result.distance = minDistance;
        result.penetrationDepth = effectiveOffset - minDistance;
        result.normal = scene.CalculateNormal(closestPoint, m_config.normalEpsilon);
        result.point = closestPoint;
    }

    return result;
}

SDFCollisionResult SDFCollisionSystem::TestMeshCollision(
    const std::vector<glm::vec3>& vertices,
    const glm::mat4& transform,
    const SDFCollisionScene& scene) const {

    m_stats.collisionTests++;
    SDFCollisionResult result;

    if (vertices.empty()) {
        return result;
    }

    float minDistance = std::numeric_limits<float>::max();
    glm::vec3 closestVertex;

    for (const auto& localVertex : vertices) {
        glm::vec4 worldVertex = transform * glm::vec4(localVertex, 1.0f);
        glm::vec3 vertex = glm::vec3(worldVertex);

        float distance = scene.EvaluateSDF(vertex);
        if (distance < minDistance) {
            minDistance = distance;
            closestVertex = vertex;
        }
    }

    float effectiveOffset = m_config.surfaceOffset;
    if (minDistance < effectiveOffset) {
        result.hit = true;
        result.distance = minDistance;
        result.penetrationDepth = effectiveOffset - minDistance;
        result.normal = scene.CalculateNormal(closestVertex, m_config.normalEpsilon);
        result.point = closestVertex;
    }

    return result;
}

// -----------------------------------------------------------------------------
// Contact Manifold Generation
// -----------------------------------------------------------------------------

SDFContactManifold SDFCollisionSystem::GenerateContacts(
    const ISDFCollider& collider,
    const SDFCollisionScene& scene) const {

    const char* typeName = collider.GetTypeName();

    if (std::strcmp(typeName, "Sphere") == 0) {
        const auto& sphere = static_cast<const SDFSphereCollider&>(collider);
        return GenerateSphereContacts(sphere.GetCenter(), sphere.GetRadius(), scene);
    } else if (std::strcmp(typeName, "Box") == 0) {
        const auto& box = static_cast<const SDFBoxCollider&>(collider);
        return GenerateBoxContacts(box.GetCenter(), box.GetHalfExtents(),
                                   box.GetOrientation(), scene);
    }

    // Default: single contact from basic collision test
    SDFContactManifold manifold;
    auto result = TestCollision(collider, scene);
    if (result.hit) {
        manifold.AddContact(result.point, result.normal, result.penetrationDepth);
    }
    manifold.ComputeAverages();
    return manifold;
}

SDFContactManifold SDFCollisionSystem::GenerateSphereContacts(
    const glm::vec3& center, float radius,
    const SDFCollisionScene& scene) const {

    SDFContactManifold manifold;

    // For sphere, we only need one contact
    auto result = TestSphereCollision(center, radius, scene);
    if (result.hit) {
        manifold.AddContact(result.point, result.normal, result.penetrationDepth);
    }

    manifold.ComputeAverages();
    return manifold;
}

SDFContactManifold SDFCollisionSystem::GenerateBoxContacts(
    const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::quat& orientation,
    const SDFCollisionScene& scene) const {

    SDFContactManifold manifold;
    SDFBoxCollider box(center, halfExtents, orientation);

    float effectiveOffset = m_config.surfaceOffset;

    // Check corners
    auto corners = box.GetCorners();
    for (const auto& corner : corners) {
        float distance = scene.EvaluateSDF(corner);
        if (distance < effectiveOffset) {
            float penetration = effectiveOffset - distance;
            glm::vec3 normal = scene.CalculateNormal(corner, m_config.normalEpsilon);
            manifold.AddContact(corner, normal, penetration);
        }
    }

    // Check face centers if no corner contacts
    if (manifold.contactCount == 0) {
        glm::mat3 rot = glm::toMat3(orientation);
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign = -1; sign <= 1; sign += 2) {
                glm::vec3 faceCenter = center + rot[axis] * halfExtents[axis] * static_cast<float>(sign);
                float distance = scene.EvaluateSDF(faceCenter);
                if (distance < effectiveOffset) {
                    float penetration = effectiveOffset - distance;
                    glm::vec3 normal = scene.CalculateNormal(faceCenter, m_config.normalEpsilon);
                    manifold.AddContact(faceCenter, normal, penetration);
                }
            }
        }
    }

    manifold.ComputeAverages();
    return manifold;
}

// -----------------------------------------------------------------------------
// Continuous Collision Detection
// -----------------------------------------------------------------------------

SDFCCDResult SDFCollisionSystem::TestContinuousCollision(
    const ISDFCollider& collider,
    const glm::vec3& displacement,
    const SDFCollisionScene& scene) const {

    m_stats.ccdTests++;

    const char* typeName = collider.GetTypeName();

    if (std::strcmp(typeName, "Sphere") == 0) {
        const auto& sphere = static_cast<const SDFSphereCollider&>(collider);
        return TestSphereCCD(sphere.GetCenter(), sphere.GetRadius(), displacement, scene);
    } else if (std::strcmp(typeName, "Capsule") == 0) {
        const auto& capsule = static_cast<const SDFCapsuleCollider&>(collider);
        return TestCapsuleCCD(capsule.GetStart(), capsule.GetEnd(),
                             capsule.GetRadius(), displacement, scene);
    }

    // Generic: sphere trace with bounding radius
    return TestSphereCCD(collider.GetCenter(), collider.GetBoundingRadius(), displacement, scene);
}

SDFCCDResult SDFCollisionSystem::TestSphereCCD(
    const glm::vec3& startCenter, float radius,
    const glm::vec3& displacement,
    const SDFCollisionScene& scene) const {

    m_stats.ccdTests++;
    SDFCCDResult result;

    float totalDistance = glm::length(displacement);
    if (totalDistance < m_config.ccdTolerance) {
        return result;
    }

    glm::vec3 direction = displacement / totalDistance;
    float effectiveRadius = radius + m_config.surfaceOffset;

    // Conservative sphere tracing along the path
    float t = 0.0f;
    glm::vec3 currentPos = startCenter;

    for (int i = 0; i < m_config.ccdIterations && t < 1.0f; ++i) {
        float distance = scene.EvaluateSDF(currentPos);

        // Check for collision
        if (distance <= effectiveRadius) {
            result.hit = true;
            result.timeOfImpact = t;
            result.impactPosition = currentPos;
            result.normal = scene.CalculateNormal(currentPos, m_config.normalEpsilon);
            result.point = currentPos - result.normal * radius;
            return result;
        }

        // Advance conservatively (account for sphere radius)
        float stepSize = glm::max(distance - effectiveRadius, m_config.ccdTolerance);
        float maxStep = (1.0f - t) * totalDistance;
        stepSize = glm::min(stepSize, maxStep);

        t += stepSize / totalDistance;
        currentPos = startCenter + displacement * t;
    }

    return result;
}

SDFCCDResult SDFCollisionSystem::TestCapsuleCCD(
    const glm::vec3& start1, const glm::vec3& end1, float radius,
    const glm::vec3& displacement,
    const SDFCollisionScene& scene) const {

    m_stats.ccdTests++;
    SDFCCDResult result;

    float totalDistance = glm::length(displacement);
    if (totalDistance < m_config.ccdTolerance) {
        return result;
    }

    glm::vec3 direction = displacement / totalDistance;
    float effectiveRadius = radius + m_config.surfaceOffset;

    float t = 0.0f;

    for (int i = 0; i < m_config.ccdIterations && t < 1.0f; ++i) {
        glm::vec3 currentStart = start1 + displacement * t;
        glm::vec3 currentEnd = end1 + displacement * t;

        // Find minimum distance along capsule
        float minDistance = std::numeric_limits<float>::max();
        glm::vec3 closestPoint;

        constexpr int SAMPLES = 5;
        for (int j = 0; j <= SAMPLES; ++j) {
            float s = static_cast<float>(j) / static_cast<float>(SAMPLES);
            glm::vec3 samplePoint = glm::mix(currentStart, currentEnd, s);

            float distance = scene.EvaluateSDF(samplePoint);
            if (distance < minDistance) {
                minDistance = distance;
                closestPoint = samplePoint;
            }
        }

        if (minDistance <= effectiveRadius) {
            result.hit = true;
            result.timeOfImpact = t;
            result.impactPosition = (currentStart + currentEnd) * 0.5f;
            result.normal = scene.CalculateNormal(closestPoint, m_config.normalEpsilon);
            result.point = closestPoint - result.normal * radius;
            return result;
        }

        float stepSize = glm::max(minDistance - effectiveRadius, m_config.ccdTolerance);
        float maxStep = (1.0f - t) * totalDistance;
        stepSize = glm::min(stepSize, maxStep);

        t += stepSize / totalDistance;
    }

    return result;
}

// -----------------------------------------------------------------------------
// Collision Resolution
// -----------------------------------------------------------------------------

glm::vec3 SDFCollisionSystem::ResolveCollision(
    const ISDFCollider& /*collider*/,
    const SDFCollisionResult& result) const {

    if (!result.hit || result.penetrationDepth <= 0.0f) {
        return glm::vec3(0.0f);
    }

    // Push out along the contact normal
    return result.normal * (result.penetrationDepth + m_config.epsilon);
}

glm::mat4 SDFCollisionSystem::ResolveCollisionTransform(
    const glm::mat4& currentTransform,
    const SDFCollisionResult& result) const {

    if (!result.hit) {
        return currentTransform;
    }

    glm::vec3 correction = result.normal * (result.penetrationDepth + m_config.epsilon);
    glm::mat4 corrected = currentTransform;
    corrected[3] += glm::vec4(correction, 0.0f);

    return corrected;
}

glm::vec3 SDFCollisionSystem::ResolveDeepPenetration(
    const ISDFCollider& collider,
    const SDFCollisionScene& scene,
    int maxIterations) const {

    glm::vec3 totalCorrection(0.0f);

    // Create a mutable copy of the collider position for iterative resolution
    glm::vec3 currentCenter = collider.GetCenter();

    for (int i = 0; i < maxIterations; ++i) {
        float distance = scene.EvaluateSDF(currentCenter);
        float effectiveRadius = collider.GetBoundingRadius() + m_config.surfaceOffset;

        if (distance >= effectiveRadius) {
            break; // No longer penetrating
        }

        float penetration = effectiveRadius - distance;
        glm::vec3 normal = scene.CalculateNormal(currentCenter, m_config.normalEpsilon);

        glm::vec3 correction = normal * (penetration + m_config.epsilon);
        totalCorrection += correction;
        currentCenter += correction;
    }

    return totalCorrection;
}

// -----------------------------------------------------------------------------
// Raycast
// -----------------------------------------------------------------------------

SDFCollisionResult SDFCollisionSystem::Raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const SDFCollisionScene& scene) const {

    m_stats.queryCount++;
    SDFCollisionResult result;

    glm::vec3 dir = glm::normalize(direction);
    float t = 0.0f;
    glm::vec3 currentPos = origin;

    for (int i = 0; i < m_config.maxIterations && t < maxDistance; ++i) {
        float distance = scene.EvaluateSDF(currentPos);

        if (distance < m_config.epsilon) {
            result.hit = true;
            result.point = currentPos;
            result.distance = t;
            result.normal = scene.CalculateNormal(currentPos, m_config.normalEpsilon);
            result.penetrationDepth = -distance;
            return result;
        }

        // Conservative step
        t += distance;
        currentPos = origin + dir * t;
    }

    return result;
}

// -----------------------------------------------------------------------------
// GJK/EPA Implementation
// -----------------------------------------------------------------------------

void SDFCollisionSystem::GJKSimplex::Push(const glm::vec3& point) {
    // Shift existing points
    for (int i = glm::min(count, 3); i > 0; --i) {
        points[i] = points[i - 1];
    }
    points[0] = point;
    count = glm::min(count + 1, 4);
}

bool SDFCollisionSystem::GJKIntersection(
    const ISDFCollider& colliderA,
    const ISDFCollider& colliderB) const {

    m_stats.gjkIterations = 0;

    // Initial direction from A to B
    glm::vec3 direction = colliderB.GetCenter() - colliderA.GetCenter();
    if (glm::length(direction) < 0.0001f) {
        direction = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Initial support point (Minkowski difference)
    glm::vec3 support = colliderA.Support(direction) - colliderB.Support(-direction);

    GJKSimplex simplex;
    simplex.Push(support);

    direction = -support;

    for (int i = 0; i < m_config.gjkMaxIterations; ++i) {
        m_stats.gjkIterations++;

        support = colliderA.Support(direction) - colliderB.Support(-direction);

        if (glm::dot(support, direction) <= 0.0f) {
            return false; // No intersection
        }

        simplex.Push(support);

        if (ProcessSimplex(simplex, direction)) {
            return true; // Origin is enclosed
        }
    }

    return false;
}

bool SDFCollisionSystem::ProcessSimplex(GJKSimplex& simplex, glm::vec3& direction) const {
    switch (simplex.count) {
        case 2:
            return ProcessLine(simplex, direction);
        case 3:
            return ProcessTriangle(simplex, direction);
        case 4:
            return ProcessTetrahedron(simplex, direction);
        default:
            return false;
    }
}

bool SDFCollisionSystem::ProcessLine(GJKSimplex& simplex, glm::vec3& direction) const {
    const glm::vec3& a = simplex[0];
    const glm::vec3& b = simplex[1];

    glm::vec3 ab = b - a;
    glm::vec3 ao = -a;

    if (SDFCollisionUtil::SameDirection(ab, ao)) {
        direction = SDFCollisionUtil::TripleProduct(ab, ao, ab);
    } else {
        simplex.count = 1;
        direction = ao;
    }

    return false;
}

bool SDFCollisionSystem::ProcessTriangle(GJKSimplex& simplex, glm::vec3& direction) const {
    const glm::vec3& a = simplex[0];
    const glm::vec3& b = simplex[1];
    const glm::vec3& c = simplex[2];

    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);

    if (SDFCollisionUtil::SameDirection(glm::cross(abc, ac), ao)) {
        if (SDFCollisionUtil::SameDirection(ac, ao)) {
            simplex.count = 2;
            simplex[1] = c;
            direction = SDFCollisionUtil::TripleProduct(ac, ao, ac);
        } else {
            simplex.count = 2;
            return ProcessLine(simplex, direction);
        }
    } else {
        if (SDFCollisionUtil::SameDirection(glm::cross(ab, abc), ao)) {
            simplex.count = 2;
            return ProcessLine(simplex, direction);
        } else {
            if (SDFCollisionUtil::SameDirection(abc, ao)) {
                direction = abc;
            } else {
                simplex[1] = c;
                simplex[2] = b;
                direction = -abc;
            }
        }
    }

    return false;
}

bool SDFCollisionSystem::ProcessTetrahedron(GJKSimplex& simplex, glm::vec3& direction) const {
    const glm::vec3& a = simplex[0];
    const glm::vec3& b = simplex[1];
    const glm::vec3& c = simplex[2];
    const glm::vec3& d = simplex[3];

    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ad = d - a;
    glm::vec3 ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);
    glm::vec3 acd = glm::cross(ac, ad);
    glm::vec3 adb = glm::cross(ad, ab);

    if (SDFCollisionUtil::SameDirection(abc, ao)) {
        simplex.count = 3;
        simplex[2] = c;
        return ProcessTriangle(simplex, direction);
    }

    if (SDFCollisionUtil::SameDirection(acd, ao)) {
        simplex.count = 3;
        simplex[1] = c;
        simplex[2] = d;
        return ProcessTriangle(simplex, direction);
    }

    if (SDFCollisionUtil::SameDirection(adb, ao)) {
        simplex.count = 3;
        simplex[1] = d;
        simplex[2] = b;
        return ProcessTriangle(simplex, direction);
    }

    return true; // Origin is inside tetrahedron
}

SDFCollisionResult SDFCollisionSystem::EPAPenetration(
    const ISDFCollider& colliderA,
    const ISDFCollider& colliderB) const {

    m_stats.epaIterations = 0;
    SDFCollisionResult result;

    // First verify intersection with GJK
    if (!GJKIntersection(colliderA, colliderB)) {
        return result;
    }

    // Build initial polytope from GJK simplex
    // This is a simplified EPA implementation
    // A full implementation would expand the polytope iteratively

    glm::vec3 centerA = colliderA.GetCenter();
    glm::vec3 centerB = colliderB.GetCenter();
    glm::vec3 direction = centerB - centerA;

    if (glm::length(direction) < 0.0001f) {
        direction = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Sample in multiple directions to find penetration
    float minDepth = std::numeric_limits<float>::max();
    glm::vec3 bestNormal;

    constexpr int SAMPLES = 26; // 6 axis + 12 edge + 8 corner directions
    glm::vec3 directions[SAMPLES] = {
        glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
        glm::normalize(glm::vec3(1, 1, 0)), glm::normalize(glm::vec3(-1, 1, 0)),
        glm::normalize(glm::vec3(1, -1, 0)), glm::normalize(glm::vec3(-1, -1, 0)),
        glm::normalize(glm::vec3(1, 0, 1)), glm::normalize(glm::vec3(-1, 0, 1)),
        glm::normalize(glm::vec3(1, 0, -1)), glm::normalize(glm::vec3(-1, 0, -1)),
        glm::normalize(glm::vec3(0, 1, 1)), glm::normalize(glm::vec3(0, -1, 1)),
        glm::normalize(glm::vec3(0, 1, -1)), glm::normalize(glm::vec3(0, -1, -1)),
        glm::normalize(glm::vec3(1, 1, 1)), glm::normalize(glm::vec3(-1, 1, 1)),
        glm::normalize(glm::vec3(1, -1, 1)), glm::normalize(glm::vec3(-1, -1, 1)),
        glm::normalize(glm::vec3(1, 1, -1)), glm::normalize(glm::vec3(-1, 1, -1)),
        glm::normalize(glm::vec3(1, -1, -1)), glm::normalize(glm::vec3(-1, -1, -1))
    };

    for (int i = 0; i < SAMPLES; ++i) {
        m_stats.epaIterations++;

        glm::vec3 supportA = colliderA.Support(directions[i]);
        glm::vec3 supportB = colliderB.Support(-directions[i]);
        glm::vec3 support = supportA - supportB;

        float depth = glm::dot(support, directions[i]);

        if (depth < minDepth) {
            minDepth = depth;
            bestNormal = directions[i];
        }
    }

    result.hit = true;
    result.penetrationDepth = minDepth;
    result.normal = bestNormal;
    result.point = centerA + bestNormal * (colliderA.GetBoundingRadius() - minDepth * 0.5f);

    return result;
}

// -----------------------------------------------------------------------------
// Sample Point Generation
// -----------------------------------------------------------------------------

SDFCollisionResult SDFCollisionSystem::TestColliderSampling(
    const ISDFCollider& /*collider*/,
    const SDFCollisionScene& scene,
    const std::vector<glm::vec3>& samplePoints) const {

    SDFCollisionResult result;
    float minDistance = std::numeric_limits<float>::max();
    glm::vec3 closestPoint;

    for (const auto& point : samplePoints) {
        float distance = scene.EvaluateSDF(point);
        if (distance < minDistance) {
            minDistance = distance;
            closestPoint = point;
        }
    }

    if (minDistance < m_config.surfaceOffset) {
        result.hit = true;
        result.distance = minDistance;
        result.penetrationDepth = m_config.surfaceOffset - minDistance;
        result.normal = scene.CalculateNormal(closestPoint, m_config.normalEpsilon);
        result.point = closestPoint;
    }

    return result;
}

std::vector<glm::vec3> SDFCollisionSystem::GenerateSphereSamplePoints(
    const glm::vec3& center, float radius, int samples) const {

    std::vector<glm::vec3> points;
    points.reserve(samples + 1);

    points.push_back(center);

    // Fibonacci sphere distribution
    float goldenRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;
    float angleIncrement = 6.28318f * goldenRatio;

    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        float phi = std::acos(1.0f - 2.0f * t);
        float theta = angleIncrement * static_cast<float>(i);

        glm::vec3 dir(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );

        points.push_back(center + dir * radius);
    }

    return points;
}

std::vector<glm::vec3> SDFCollisionSystem::GenerateCapsuleSamplePoints(
    const glm::vec3& start, const glm::vec3& end, float radius, int samples) const {

    std::vector<glm::vec3> points;

    // Points along axis
    int axisCount = samples / 3;
    for (int i = 0; i <= axisCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(axisCount);
        points.push_back(glm::mix(start, end, t));
    }

    // Radial points
    glm::vec3 axis = glm::normalize(end - start);
    glm::vec3 perpX, perpY;

    if (std::abs(axis.x) < 0.9f) {
        perpX = glm::normalize(glm::cross(axis, glm::vec3(1, 0, 0)));
    } else {
        perpX = glm::normalize(glm::cross(axis, glm::vec3(0, 1, 0)));
    }
    perpY = glm::cross(axis, perpX);

    int radialCount = (samples - axisCount) / 2;
    for (int i = 0; i < radialCount; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(radialCount) * 6.28318f;
        glm::vec3 offset = (std::cos(angle) * perpX + std::sin(angle) * perpY) * radius;

        points.push_back(start + offset);
        points.push_back(end + offset);
        points.push_back((start + end) * 0.5f + offset);
    }

    return points;
}

std::vector<glm::vec3> SDFCollisionSystem::GenerateBoxSamplePoints(
    const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::quat& orientation, int /*samples*/) const {

    std::vector<glm::vec3> points;
    SDFBoxCollider box(center, halfExtents, orientation);

    // Corners
    auto corners = box.GetCorners();
    for (const auto& corner : corners) {
        points.push_back(corner);
    }

    // Face centers
    glm::mat3 rot = glm::toMat3(orientation);
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            points.push_back(center + rot[axis] * halfExtents[axis] * static_cast<float>(sign));
        }
    }

    // Edge midpoints
    for (int axis1 = 0; axis1 < 3; ++axis1) {
        for (int axis2 = axis1 + 1; axis2 < 3; ++axis2) {
            for (int sign1 = -1; sign1 <= 1; sign1 += 2) {
                for (int sign2 = -1; sign2 <= 1; sign2 += 2) {
                    points.push_back(center
                        + rot[axis1] * halfExtents[axis1] * static_cast<float>(sign1)
                        + rot[axis2] * halfExtents[axis2] * static_cast<float>(sign2));
                }
            }
        }
    }

    // Center
    points.push_back(center);

    return points;
}

// =============================================================================
// SDFCollisionUtil Implementation
// =============================================================================

namespace SDFCollisionUtil {

glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& segmentStart, const glm::vec3& segmentEnd) {
    glm::vec3 ab = segmentEnd - segmentStart;
    float t = glm::dot(point - segmentStart, ab);

    if (t <= 0.0f) {
        return segmentStart;
    }

    float denom = glm::dot(ab, ab);
    if (t >= denom) {
        return segmentEnd;
    }

    return segmentStart + ab * (t / denom);
}

glm::vec3 ClosestPointOnTriangle(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    // Check if P in vertex region outside A
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = point - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);

    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }

    // Check if P in vertex region outside B
    glm::vec3 bp = point - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);

    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }

    // Check if P in edge region of AB
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    // Check if P in vertex region outside C
    glm::vec3 cp = point - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);

    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }

    // Check if P in edge region of AC
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    // Check if P in edge region of BC
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    // P inside face region
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;

    return a + ab * v + ac * w;
}

glm::vec3 ComputeSDFGradient(const std::function<float(const glm::vec3&)>& sdf, const glm::vec3& point, float epsilon) {
    return glm::vec3(
        sdf(point + glm::vec3(epsilon, 0, 0)) - sdf(point - glm::vec3(epsilon, 0, 0)),
        sdf(point + glm::vec3(0, epsilon, 0)) - sdf(point - glm::vec3(0, epsilon, 0)),
        sdf(point + glm::vec3(0, 0, epsilon)) - sdf(point - glm::vec3(0, 0, epsilon))
    ) / (2.0f * epsilon);
}

float SDFBox(const glm::vec3& point, const glm::vec3& halfExtents) {
    glm::vec3 q = glm::abs(point) - halfExtents;
    return glm::length(glm::max(q, glm::vec3(0.0f))) + glm::min(glm::max(q.x, glm::max(q.y, q.z)), 0.0f);
}

float SDFSphere(const glm::vec3& point, float radius) {
    return glm::length(point) - radius;
}

float SDFCapsule(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b, float radius) {
    glm::vec3 closest = ClosestPointOnSegment(point, a, b);
    return glm::length(point - closest) - radius;
}

glm::vec3 TripleProduct(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    return glm::cross(glm::cross(a, b), c);
}

bool SameDirection(const glm::vec3& a, const glm::vec3& b) {
    return glm::dot(a, b) > 0.0f;
}

} // namespace SDFCollisionUtil

} // namespace Nova
