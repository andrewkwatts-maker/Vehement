/**
 * @file RayPicker.cpp
 * @brief Implementation of ray casting and object picking system
 */

#include "RayPicker.hpp"
#include "../scene/Camera.hpp"
#include "../graphics/Mesh.hpp"
#include "../sdf/SDFModel.hpp"
#include "../spatial/AABB.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// Constructor / Destructor
// ============================================================================

RayPicker::RayPicker() = default;

RayPicker::~RayPicker() = default;

// ============================================================================
// Ray Generation
// ============================================================================

PickRay RayPicker::ScreenToRay(
    const glm::vec2& screenPos,
    const glm::vec2& screenSize,
    const Camera& camera) const
{
    // Convert screen coordinates to NDC (-1 to 1)
    // Screen origin is top-left, NDC origin is center
    glm::vec2 ndc;
    ndc.x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    ndc.y = 1.0f - (2.0f * screenPos.y) / screenSize.y; // Flip Y

    return NDCToRay(ndc, camera);
}

PickRay RayPicker::NDCToRay(
    const glm::vec2& ndcPos,
    const Camera& camera) const
{
    // Get inverse view-projection matrix
    const glm::mat4& invProjView = camera.GetInverseProjectionView();

    // Create points at near and far planes in clip space
    glm::vec4 nearPoint = glm::vec4(ndcPos.x, ndcPos.y, -1.0f, 1.0f);
    glm::vec4 farPoint = glm::vec4(ndcPos.x, ndcPos.y, 1.0f, 1.0f);

    // Transform to world space
    glm::vec4 nearWorld = invProjView * nearPoint;
    glm::vec4 farWorld = invProjView * farPoint;

    // Perspective divide
    nearWorld /= nearWorld.w;
    farWorld /= farWorld.w;

    // Create ray from near to far
    glm::vec3 origin = glm::vec3(nearWorld);
    glm::vec3 direction = glm::normalize(glm::vec3(farWorld) - glm::vec3(nearWorld));

    return PickRay(origin, direction);
}

// ============================================================================
// Single Object Picking
// ============================================================================

PickResult RayPicker::Pick(
    const glm::vec2& screenPos,
    const glm::vec2& screenSize,
    const Camera& camera,
    const std::vector<IPickable*>& objects) const
{
    PickRay ray = ScreenToRay(screenPos, screenSize, camera);
    return Pick(ray, objects);
}

PickResult RayPicker::Pick(
    const PickRay& ray,
    const std::vector<IPickable*>& objects) const
{
    PickResult closest;
    closest.distance = m_config.maxPickDistance;

    for (IPickable* pickable : objects) {
        if (!pickable) continue;

        PickResult result = TestPickable(ray, pickable);

        if (result.IsValid() && result.distance < closest.distance) {
            closest = result;
        }
    }

    // Invalidate if no hit found within max distance
    if (closest.objectId == 0) {
        closest.distance = std::numeric_limits<float>::max();
    }

    return closest;
}

// ============================================================================
// Multi-Object Picking
// ============================================================================

std::vector<PickResult> RayPicker::PickAll(
    const glm::vec2& screenPos,
    const glm::vec2& screenSize,
    const Camera& camera,
    const std::vector<IPickable*>& objects) const
{
    PickRay ray = ScreenToRay(screenPos, screenSize, camera);
    return PickAll(ray, objects);
}

std::vector<PickResult> RayPicker::PickAll(
    const PickRay& ray,
    const std::vector<IPickable*>& objects) const
{
    std::vector<PickResult> results;
    results.reserve(objects.size() / 4); // Reasonable estimate

    for (IPickable* pickable : objects) {
        if (!pickable) continue;

        PickResult result = TestPickable(ray, pickable);

        if (result.IsValid() && result.distance <= m_config.maxPickDistance) {
            results.push_back(result);
        }
    }

    // Sort by distance
    if (m_config.sortByDistance) {
        std::sort(results.begin(), results.end());
    }

    return results;
}

// ============================================================================
// Marquee Selection
// ============================================================================

void RayPicker::BeginMarquee(const glm::vec2& screenPos)
{
    m_marquee.startPoint = screenPos;
    m_marquee.endPoint = screenPos;
    m_marquee.isActive = true;
}

void RayPicker::UpdateMarquee(const glm::vec2& screenPos)
{
    if (m_marquee.isActive) {
        m_marquee.endPoint = screenPos;
    }
}

std::vector<uint64_t> RayPicker::EndMarquee(
    const glm::vec2& screenSize,
    const Camera& camera,
    const std::vector<IPickable*>& objects)
{
    std::vector<uint64_t> selected;

    if (m_marquee.isActive) {
        // Check minimum size threshold
        glm::vec2 size = m_marquee.GetSize();
        if (size.x >= m_config.marqueeMinSize || size.y >= m_config.marqueeMinSize) {
            selected = MarqueeSelect(screenSize, camera, objects);
        }
    }

    m_marquee.isActive = false;
    return selected;
}

void RayPicker::CancelMarquee()
{
    m_marquee.isActive = false;
}

std::vector<uint64_t> RayPicker::MarqueeSelect(
    const glm::vec2& screenSize,
    const Camera& camera,
    const std::vector<IPickable*>& objects) const
{
    std::vector<uint64_t> selected;

    if (!m_marquee.isActive) {
        return selected;
    }

    // Get normalized marquee rectangle
    glm::vec2 minPt, maxPt;
    m_marquee.GetNormalizedRect(minPt, maxPt);

    // Build frustum planes for the marquee rectangle
    glm::vec4 frustumPlanes[6];
    GetMarqueeFrustum(minPt, maxPt, screenSize, camera, frustumPlanes);

    // Test each object against frustum
    for (IPickable* pickable : objects) {
        if (!pickable) continue;

        glm::vec3 boundsMin, boundsMax;
        if (!pickable->GetPickBounds(boundsMin, boundsMax)) {
            continue;
        }

        glm::mat4 transform = pickable->GetPickTransform();

        // Transform bounds to world space
        AABB localAABB(boundsMin, boundsMax);
        AABB worldAABB = localAABB.Transform(transform);

        // Test against frustum
        bool inside = AABBInFrustum(
            worldAABB.min, worldAABB.max,
            frustumPlanes,
            !m_config.marqueeIncludePartial
        );

        if (inside) {
            selected.push_back(pickable->GetPickId());
        }
    }

    return selected;
}

// ============================================================================
// Low-Level Intersection Tests
// ============================================================================

bool RayPicker::IntersectAABB(
    const PickRay& ray,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    float& outDistance)
{
    glm::vec3 invDir = ray.GetInverseDirection();

    // Calculate intersection distances for each slab
    glm::vec3 t1 = (aabbMin - ray.origin) * invDir;
    glm::vec3 t2 = (aabbMax - ray.origin) * invDir;

    // Find the min and max t values for each axis
    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);

    // Find the largest entry point and smallest exit point
    float tEntry = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
    float tExit = glm::min(glm::min(tMax.x, tMax.y), tMax.z);

    // Check for valid intersection
    if (tEntry > tExit || tExit < 0.0f) {
        return false;
    }

    // Return entry distance (or 0 if inside AABB)
    outDistance = tEntry > 0.0f ? tEntry : 0.0f;
    return true;
}

bool RayPicker::IntersectAABBTransformed(
    const PickRay& ray,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    const glm::mat4& transform,
    float& outDistance)
{
    // Transform ray to local space
    glm::mat4 invTransform = glm::inverse(transform);

    glm::vec4 localOrigin = invTransform * glm::vec4(ray.origin, 1.0f);
    glm::vec4 localDir = invTransform * glm::vec4(ray.direction, 0.0f);

    PickRay localRay(glm::vec3(localOrigin), glm::normalize(glm::vec3(localDir)));

    // Test against local AABB
    float localDistance;
    if (!IntersectAABB(localRay, aabbMin, aabbMax, localDistance)) {
        return false;
    }

    // Transform hit point back to world space to get correct distance
    glm::vec3 localHit = localRay.GetPoint(localDistance);
    glm::vec4 worldHit = transform * glm::vec4(localHit, 1.0f);

    outDistance = glm::length(glm::vec3(worldHit) - ray.origin);
    return true;
}

bool RayPicker::IntersectSphere(
    const PickRay& ray,
    const glm::vec3& center,
    float radius,
    float& outDistance)
{
    glm::vec3 oc = ray.origin - center;

    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return false;
    }

    float sqrtD = std::sqrt(discriminant);

    // Find nearest positive intersection
    float t1 = (-b - sqrtD) / (2.0f * a);
    float t2 = (-b + sqrtD) / (2.0f * a);

    if (t1 > 0.0f) {
        outDistance = t1;
        return true;
    } else if (t2 > 0.0f) {
        outDistance = t2;
        return true;
    }

    return false;
}

bool RayPicker::IntersectPlane(
    const PickRay& ray,
    const glm::vec3& planePoint,
    const glm::vec3& planeNormal,
    float& outDistance)
{
    float denom = glm::dot(planeNormal, ray.direction);

    // Check if ray is parallel to plane
    if (std::abs(denom) < 1e-6f) {
        return false;
    }

    float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;

    if (t < 0.0f) {
        return false;
    }

    outDistance = t;
    return true;
}

bool RayPicker::IntersectTriangle(
    const PickRay& ray,
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2,
    float& outDistance,
    glm::vec2* outBarycentrics)
{
    constexpr float epsilon = 1e-8f;

    // Moller-Trumbore algorithm
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;

    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    // Check if ray is parallel to triangle
    if (a > -epsilon && a < epsilon) {
        return false;
    }

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);

    // Check u bounds
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);

    // Check v bounds and sum
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    // Calculate distance
    float t = f * glm::dot(edge2, q);

    if (t < epsilon) {
        return false;
    }

    outDistance = t;

    if (outBarycentrics) {
        *outBarycentrics = glm::vec2(u, v);
    }

    return true;
}

bool RayPicker::IntersectMesh(
    const PickRay& ray,
    const Mesh& mesh,
    const glm::mat4& transform,
    PickResult& result) const
{
    // First check AABB for early rejection
    float aabbDist;
    if (!IntersectAABBTransformed(ray, mesh.GetBoundsMin(), mesh.GetBoundsMax(), transform, aabbDist)) {
        return false;
    }

    // If not using detailed intersection, return AABB hit
    if (!m_config.useDetailedIntersection) {
        result.distance = aabbDist;
        result.hitPoint = ray.GetPoint(aabbDist);
        // Approximate normal from AABB
        result.hitNormal = glm::normalize(result.hitPoint - (mesh.GetBoundsMin() + mesh.GetBoundsMax()) * 0.5f);
        return true;
    }

    // Transform ray to local space for mesh testing
    glm::mat4 invTransform = glm::inverse(transform);
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(invTransform));

    glm::vec4 localOrigin4 = invTransform * glm::vec4(ray.origin, 1.0f);
    glm::vec4 localDir4 = invTransform * glm::vec4(ray.direction, 0.0f);

    PickRay localRay(glm::vec3(localOrigin4), glm::normalize(glm::vec3(localDir4)));

    // Note: For full mesh intersection, we would need access to the mesh's
    // vertex and index data. Since the Mesh class doesn't expose this,
    // we fall back to AABB intersection.
    // In a production implementation, the Mesh class would expose
    // GetVertices() and GetIndices() methods, or provide a RayIntersect method.

    // For now, return the AABB intersection as the hit
    result.distance = aabbDist;
    result.hitPoint = ray.GetPoint(aabbDist);

    // Calculate approximate normal from the AABB face that was hit
    glm::vec3 center = (mesh.GetBoundsMin() + mesh.GetBoundsMax()) * 0.5f;
    glm::vec3 localHit = localRay.GetPoint(aabbDist);
    glm::vec3 localNormal = glm::vec3(0.0f, 1.0f, 0.0f);

    // Determine which face was hit by finding axis with largest extent to hit point
    glm::vec3 toHit = localHit - center;
    glm::vec3 extents = (mesh.GetBoundsMax() - mesh.GetBoundsMin()) * 0.5f;
    glm::vec3 scaled = toHit / (extents + glm::vec3(1e-6f));

    if (std::abs(scaled.x) > std::abs(scaled.y) && std::abs(scaled.x) > std::abs(scaled.z)) {
        localNormal = glm::vec3(glm::sign(scaled.x), 0.0f, 0.0f);
    } else if (std::abs(scaled.y) > std::abs(scaled.z)) {
        localNormal = glm::vec3(0.0f, glm::sign(scaled.y), 0.0f);
    } else {
        localNormal = glm::vec3(0.0f, 0.0f, glm::sign(scaled.z));
    }

    // Transform normal to world space
    result.hitNormal = glm::normalize(normalMatrix * localNormal);

    return true;
}

bool RayPicker::IntersectSDF(
    const PickRay& ray,
    const SDFModel& sdfModel,
    const glm::mat4& transform,
    PickResult& result) const
{
    // First check AABB for early rejection
    auto [boundsMin, boundsMax] = sdfModel.GetBounds();

    float aabbDist;
    if (!IntersectAABBTransformed(ray, boundsMin, boundsMax, transform, aabbDist)) {
        return false;
    }

    // If not using detailed intersection, return AABB hit
    if (!m_config.useDetailedIntersection) {
        result.distance = aabbDist;
        result.hitPoint = ray.GetPoint(aabbDist);
        result.hitNormal = glm::normalize(result.hitPoint - (boundsMin + boundsMax) * 0.5f);
        return true;
    }

    // Transform ray to local space
    glm::mat4 invTransform = glm::inverse(transform);
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(invTransform));

    glm::vec4 localOrigin4 = invTransform * glm::vec4(ray.origin, 1.0f);
    glm::vec4 localDir4 = invTransform * glm::vec4(ray.direction, 0.0f);

    glm::vec3 localOrigin = glm::vec3(localOrigin4);
    glm::vec3 localDir = glm::normalize(glm::vec3(localDir4));

    // Sphere tracing
    float t = glm::max(0.0f, aabbDist - 0.1f); // Start slightly before AABB hit
    float totalDistance = 0.0f;

    for (int i = 0; i < m_config.sdfMaxSteps; ++i) {
        glm::vec3 p = localOrigin + localDir * t;

        // Check if we've gone beyond bounds
        if (t > m_config.sdfMaxDistance) {
            return false;
        }

        // Evaluate SDF at current point
        float d = sdfModel.EvaluateSDF(p);

        // Check for hit
        if (d < m_config.sdfHitThreshold) {
            // Calculate normal at hit point
            glm::vec3 localNormal = sdfModel.CalculateNormal(p);

            // Transform hit point and normal to world space
            glm::vec4 worldHit4 = transform * glm::vec4(p, 1.0f);
            result.hitPoint = glm::vec3(worldHit4);
            result.hitNormal = glm::normalize(normalMatrix * localNormal);
            result.distance = glm::length(result.hitPoint - ray.origin);

            return true;
        }

        // March forward by SDF distance (clamped for safety)
        t += glm::max(d, m_config.sdfHitThreshold * 0.1f);
        totalDistance += d;
    }

    return false;
}

// ============================================================================
// Utility Functions
// ============================================================================

glm::vec2 RayPicker::WorldToScreen(
    const glm::vec3& worldPos,
    const glm::vec2& screenSize,
    const Camera& camera)
{
    // Project to clip space
    glm::vec4 clipPos = camera.GetProjectionView() * glm::vec4(worldPos, 1.0f);

    // Check if behind camera
    if (clipPos.w <= 0.0f) {
        return glm::vec2(-1.0f);
    }

    // Perspective divide to NDC
    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

    // Check if outside normalized device coordinates
    if (ndc.x < -1.0f || ndc.x > 1.0f ||
        ndc.y < -1.0f || ndc.y > 1.0f ||
        ndc.z < -1.0f || ndc.z > 1.0f) {
        return glm::vec2(-1.0f);
    }

    // Convert to screen coordinates (flip Y)
    glm::vec2 screen;
    screen.x = (ndc.x + 1.0f) * 0.5f * screenSize.x;
    screen.y = (1.0f - ndc.y) * 0.5f * screenSize.y;

    return screen;
}

bool RayPicker::IsPointVisible(
    const glm::vec3& worldPos,
    const Camera& camera)
{
    glm::vec4 clipPos = camera.GetProjectionView() * glm::vec4(worldPos, 1.0f);
    return clipPos.w > 0.0f;
}

void RayPicker::GetMarqueeFrustum(
    const glm::vec2& screenMin,
    const glm::vec2& screenMax,
    const glm::vec2& screenSize,
    const Camera& camera,
    glm::vec4 outPlanes[6])
{
    // Convert screen corners to NDC
    glm::vec2 ndcMin, ndcMax;
    ndcMin.x = (2.0f * screenMin.x) / screenSize.x - 1.0f;
    ndcMin.y = 1.0f - (2.0f * screenMax.y) / screenSize.y; // Note: max Y for min NDC Y
    ndcMax.x = (2.0f * screenMax.x) / screenSize.x - 1.0f;
    ndcMax.y = 1.0f - (2.0f * screenMin.y) / screenSize.y; // Note: min Y for max NDC Y

    // Get inverse view-projection matrix
    const glm::mat4& invProjView = camera.GetInverseProjectionView();
    const glm::mat4& projView = camera.GetProjectionView();

    // Calculate frustum corners at near and far planes
    glm::vec4 corners[8] = {
        // Near plane
        invProjView * glm::vec4(ndcMin.x, ndcMin.y, -1.0f, 1.0f),
        invProjView * glm::vec4(ndcMax.x, ndcMin.y, -1.0f, 1.0f),
        invProjView * glm::vec4(ndcMax.x, ndcMax.y, -1.0f, 1.0f),
        invProjView * glm::vec4(ndcMin.x, ndcMax.y, -1.0f, 1.0f),
        // Far plane
        invProjView * glm::vec4(ndcMin.x, ndcMin.y, 1.0f, 1.0f),
        invProjView * glm::vec4(ndcMax.x, ndcMin.y, 1.0f, 1.0f),
        invProjView * glm::vec4(ndcMax.x, ndcMax.y, 1.0f, 1.0f),
        invProjView * glm::vec4(ndcMin.x, ndcMax.y, 1.0f, 1.0f)
    };

    // Perspective divide
    for (int i = 0; i < 8; ++i) {
        corners[i] /= corners[i].w;
    }

    // Build frustum planes from corners
    // Left plane: corners 0, 3, 4, 7
    glm::vec3 n = glm::normalize(glm::cross(
        glm::vec3(corners[3]) - glm::vec3(corners[0]),
        glm::vec3(corners[4]) - glm::vec3(corners[0])
    ));
    outPlanes[0] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[0])));

    // Right plane: corners 1, 2, 5, 6
    n = glm::normalize(glm::cross(
        glm::vec3(corners[5]) - glm::vec3(corners[1]),
        glm::vec3(corners[2]) - glm::vec3(corners[1])
    ));
    outPlanes[1] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[1])));

    // Bottom plane: corners 0, 1, 4, 5
    n = glm::normalize(glm::cross(
        glm::vec3(corners[4]) - glm::vec3(corners[0]),
        glm::vec3(corners[1]) - glm::vec3(corners[0])
    ));
    outPlanes[2] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[0])));

    // Top plane: corners 2, 3, 6, 7
    n = glm::normalize(glm::cross(
        glm::vec3(corners[3]) - glm::vec3(corners[2]),
        glm::vec3(corners[6]) - glm::vec3(corners[2])
    ));
    outPlanes[3] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[2])));

    // Near plane: corners 0, 1, 2, 3
    n = glm::normalize(glm::cross(
        glm::vec3(corners[1]) - glm::vec3(corners[0]),
        glm::vec3(corners[3]) - glm::vec3(corners[0])
    ));
    outPlanes[4] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[0])));

    // Far plane: corners 4, 5, 6, 7
    n = glm::normalize(glm::cross(
        glm::vec3(corners[7]) - glm::vec3(corners[4]),
        glm::vec3(corners[5]) - glm::vec3(corners[4])
    ));
    outPlanes[5] = glm::vec4(n, -glm::dot(n, glm::vec3(corners[4])));
}

// ============================================================================
// Private Implementation
// ============================================================================

PickResult RayPicker::TestPickable(
    const PickRay& ray,
    IPickable* pickable) const
{
    PickResult result;
    result.objectId = pickable->GetPickId();

    glm::vec3 boundsMin, boundsMax;
    if (!pickable->GetPickBounds(boundsMin, boundsMax)) {
        result.objectId = 0;
        return result;
    }

    glm::mat4 transform = pickable->GetPickTransform();

    // Quick AABB test first
    float aabbDist;
    if (!IntersectAABBTransformed(ray, boundsMin, boundsMax, transform, aabbDist)) {
        result.objectId = 0;
        return result;
    }

    // Try detailed intersection tests if available
    if (m_config.useDetailedIntersection) {
        // Try SDF intersection first (more accurate for SDF models)
        const SDFModel* sdfModel = pickable->GetPickSDFModel();
        if (sdfModel) {
            if (IntersectSDF(ray, *sdfModel, transform, result)) {
                return result;
            }
        }

        // Try mesh intersection
        const Mesh* mesh = pickable->GetPickMesh();
        if (mesh) {
            if (IntersectMesh(ray, *mesh, transform, result)) {
                return result;
            }
        }
    }

    // Fall back to AABB intersection
    result.distance = aabbDist;
    result.hitPoint = ray.GetPoint(aabbDist);

    // Calculate approximate normal
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec4 localHit4 = invTransform * glm::vec4(result.hitPoint, 1.0f);
    glm::vec3 localHit = glm::vec3(localHit4);
    glm::vec3 toHit = localHit - center;
    glm::vec3 extents = (boundsMax - boundsMin) * 0.5f + glm::vec3(1e-6f);
    glm::vec3 scaled = toHit / extents;

    glm::vec3 localNormal;
    if (std::abs(scaled.x) > std::abs(scaled.y) && std::abs(scaled.x) > std::abs(scaled.z)) {
        localNormal = glm::vec3(glm::sign(scaled.x), 0.0f, 0.0f);
    } else if (std::abs(scaled.y) > std::abs(scaled.z)) {
        localNormal = glm::vec3(0.0f, glm::sign(scaled.y), 0.0f);
    } else {
        localNormal = glm::vec3(0.0f, 0.0f, glm::sign(scaled.z));
    }

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(invTransform));
    result.hitNormal = glm::normalize(normalMatrix * localNormal);

    return result;
}

bool RayPicker::IsInMarqueeFrustum(
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    const glm::mat4& transform,
    const glm::vec4 frustumPlanes[6]) const
{
    // Transform AABB to world space
    AABB localAABB(boundsMin, boundsMax);
    AABB worldAABB = localAABB.Transform(transform);

    return AABBInFrustum(
        worldAABB.min, worldAABB.max,
        frustumPlanes,
        !m_config.marqueeIncludePartial
    );
}

bool RayPicker::AABBInFrustum(
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    const glm::vec4 frustumPlanes[6],
    bool requireFullyInside)
{
    // Get AABB corners
    glm::vec3 corners[8] = {
        glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z)
    };

    // Test each frustum plane
    for (int p = 0; p < 6; ++p) {
        const glm::vec4& plane = frustumPlanes[p];
        glm::vec3 planeNormal(plane.x, plane.y, plane.z);
        float planeD = plane.w;

        int outsideCount = 0;
        int insideCount = 0;

        for (int c = 0; c < 8; ++c) {
            float dist = glm::dot(planeNormal, corners[c]) + planeD;

            if (dist < 0.0f) {
                outsideCount++;
            } else {
                insideCount++;
            }
        }

        // All corners outside this plane - completely outside frustum
        if (outsideCount == 8) {
            return false;
        }

        // If requiring fully inside and some corners are outside
        if (requireFullyInside && outsideCount > 0) {
            return false;
        }
    }

    return true;
}

} // namespace Nova
