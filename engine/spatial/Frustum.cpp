#include "Frustum.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

void Frustum::ExtractPlanes(const glm::mat4& viewProjection) noexcept {
    m_viewProjection = viewProjection;
    m_inverseViewProjection = glm::inverse(viewProjection);

    // Extract frustum planes using Gribb/Hartmann method
    // Plane equations are: normal.x * x + normal.y * y + normal.z * z + distance = 0

    // Left plane
    m_planes[0].normal.x = viewProjection[0][3] + viewProjection[0][0];
    m_planes[0].normal.y = viewProjection[1][3] + viewProjection[1][0];
    m_planes[0].normal.z = viewProjection[2][3] + viewProjection[2][0];
    m_planes[0].distance = viewProjection[3][3] + viewProjection[3][0];

    // Right plane
    m_planes[1].normal.x = viewProjection[0][3] - viewProjection[0][0];
    m_planes[1].normal.y = viewProjection[1][3] - viewProjection[1][0];
    m_planes[1].normal.z = viewProjection[2][3] - viewProjection[2][0];
    m_planes[1].distance = viewProjection[3][3] - viewProjection[3][0];

    // Bottom plane
    m_planes[2].normal.x = viewProjection[0][3] + viewProjection[0][1];
    m_planes[2].normal.y = viewProjection[1][3] + viewProjection[1][1];
    m_planes[2].normal.z = viewProjection[2][3] + viewProjection[2][1];
    m_planes[2].distance = viewProjection[3][3] + viewProjection[3][1];

    // Top plane
    m_planes[3].normal.x = viewProjection[0][3] - viewProjection[0][1];
    m_planes[3].normal.y = viewProjection[1][3] - viewProjection[1][1];
    m_planes[3].normal.z = viewProjection[2][3] - viewProjection[2][1];
    m_planes[3].distance = viewProjection[3][3] - viewProjection[3][1];

    // Near plane
    m_planes[4].normal.x = viewProjection[0][3] + viewProjection[0][2];
    m_planes[4].normal.y = viewProjection[1][3] + viewProjection[1][2];
    m_planes[4].normal.z = viewProjection[2][3] + viewProjection[2][2];
    m_planes[4].distance = viewProjection[3][3] + viewProjection[3][2];

    // Far plane
    m_planes[5].normal.x = viewProjection[0][3] - viewProjection[0][2];
    m_planes[5].normal.y = viewProjection[1][3] - viewProjection[1][2];
    m_planes[5].normal.z = viewProjection[2][3] - viewProjection[2][2];
    m_planes[5].distance = viewProjection[3][3] - viewProjection[3][2];

    // Normalize all planes
    for (auto& plane : m_planes) {
        plane.Normalize();
    }

#ifdef __SSE__
    // Pack plane data for SIMD operations
    for (int i = 0; i < 6; ++i) {
        m_planeNormalsX[i] = m_planes[i].normal.x;
        m_planeNormalsY[i] = m_planes[i].normal.y;
        m_planeNormalsZ[i] = m_planes[i].normal.z;
        m_planeDistances[i] = m_planes[i].distance;
    }
    // Padding
    m_planeNormalsX[6] = m_planeNormalsX[7] = 0.0f;
    m_planeNormalsY[6] = m_planeNormalsY[7] = 0.0f;
    m_planeNormalsZ[6] = m_planeNormalsZ[7] = 0.0f;
    m_planeDistances[6] = m_planeDistances[7] = 0.0f;
#endif
}

std::array<glm::vec3, 8> Frustum::GetCorners() const noexcept {
    // NDC corners
    static constexpr glm::vec4 ndcCorners[8] = {
        {-1, -1, -1, 1}, // Near bottom-left
        { 1, -1, -1, 1}, // Near bottom-right
        { 1,  1, -1, 1}, // Near top-right
        {-1,  1, -1, 1}, // Near top-left
        {-1, -1,  1, 1}, // Far bottom-left
        { 1, -1,  1, 1}, // Far bottom-right
        { 1,  1,  1, 1}, // Far top-right
        {-1,  1,  1, 1}  // Far top-left
    };

    std::array<glm::vec3, 8> corners;
    for (int i = 0; i < 8; ++i) {
        glm::vec4 worldPos = m_inverseViewProjection * ndcCorners[i];
        corners[i] = glm::vec3(worldPos) / worldPos.w;
    }

    return corners;
}

bool Frustum::ContainsPoint(const glm::vec3& point) const noexcept {
    for (const auto& plane : m_planes) {
        if (plane.SignedDistance(point) < 0.0f) {
            return false;
        }
    }
    return true;
}

FrustumResult Frustum::TestSphere(const glm::vec3& center, float radius) const noexcept {
    bool allInside = true;

    for (const auto& plane : m_planes) {
        float distance = plane.SignedDistance(center);

        if (distance < -radius) {
            return FrustumResult::Outside;
        }

        if (distance < radius) {
            allInside = false;
        }
    }

    return allInside ? FrustumResult::Inside : FrustumResult::Intersect;
}

bool Frustum::IsSphereOutside(const glm::vec3& center, float radius) const noexcept {
    for (const auto& plane : m_planes) {
        if (plane.SignedDistance(center) < -radius) {
            return true;
        }
    }
    return false;
}

FrustumResult Frustum::TestAABB(const AABB& aabb) const noexcept {
    bool allInside = true;

    for (const auto& plane : m_planes) {
        // Find the point of the AABB closest to the plane (p-vertex)
        // and the point farthest from the plane (n-vertex)
        glm::vec3 pVertex = aabb.min;
        glm::vec3 nVertex = aabb.max;

        if (plane.normal.x >= 0.0f) {
            pVertex.x = aabb.max.x;
            nVertex.x = aabb.min.x;
        }
        if (plane.normal.y >= 0.0f) {
            pVertex.y = aabb.max.y;
            nVertex.y = aabb.min.y;
        }
        if (plane.normal.z >= 0.0f) {
            pVertex.z = aabb.max.z;
            nVertex.z = aabb.min.z;
        }

        // If n-vertex is outside, AABB is completely outside
        if (plane.SignedDistance(nVertex) < 0.0f) {
            return FrustumResult::Outside;
        }

        // If p-vertex is outside, AABB intersects the plane
        if (plane.SignedDistance(pVertex) < 0.0f) {
            allInside = false;
        }
    }

    return allInside ? FrustumResult::Inside : FrustumResult::Intersect;
}

bool Frustum::IsAABBOutside(const AABB& aabb) const noexcept {
    for (const auto& plane : m_planes) {
        // Find n-vertex (point most in negative direction of normal)
        glm::vec3 nVertex;
        nVertex.x = plane.normal.x >= 0.0f ? aabb.min.x : aabb.max.x;
        nVertex.y = plane.normal.y >= 0.0f ? aabb.min.y : aabb.max.y;
        nVertex.z = plane.normal.z >= 0.0f ? aabb.min.z : aabb.max.z;

        if (plane.SignedDistance(nVertex) < 0.0f) {
            return true;
        }
    }
    return false;
}

bool Frustum::TestAABBCoherent(const AABB& aabb, uint8_t& planeMask) const noexcept {
    uint8_t newMask = 0;

    for (int i = 0; i < 6; ++i) {
        if (!(planeMask & (1 << i))) {
            continue; // Skip planes masked out
        }

        const Plane& plane = m_planes[i];

        glm::vec3 nVertex;
        nVertex.x = plane.normal.x >= 0.0f ? aabb.min.x : aabb.max.x;
        nVertex.y = plane.normal.y >= 0.0f ? aabb.min.y : aabb.max.y;
        nVertex.z = plane.normal.z >= 0.0f ? aabb.min.z : aabb.max.z;

        if (plane.SignedDistance(nVertex) < 0.0f) {
            return false; // Outside this plane
        }

        // Check if we're entirely inside this plane
        glm::vec3 pVertex;
        pVertex.x = plane.normal.x >= 0.0f ? aabb.max.x : aabb.min.x;
        pVertex.y = plane.normal.y >= 0.0f ? aabb.max.y : aabb.min.y;
        pVertex.z = plane.normal.z >= 0.0f ? aabb.max.z : aabb.min.z;

        if (plane.SignedDistance(pVertex) < 0.0f) {
            // Intersecting this plane, keep testing it
            newMask |= (1 << i);
        }
        // else: completely inside this plane, don't need to test children
    }

    planeMask = newMask;
    return true;
}

FrustumResult Frustum::TestOBB(const OBB& obb) const noexcept {
    bool allInside = true;

    for (const auto& plane : m_planes) {
        // Project OBB onto plane normal
        const glm::vec3* axes = obb.GetAxes();
        float projectedRadius = 0.0f;

        for (int i = 0; i < 3; ++i) {
            projectedRadius += std::abs(glm::dot(axes[i], plane.normal)) * obb.halfExtents[i];
        }

        float centerDistance = plane.SignedDistance(obb.center);

        if (centerDistance < -projectedRadius) {
            return FrustumResult::Outside;
        }

        if (centerDistance < projectedRadius) {
            allInside = false;
        }
    }

    return allInside ? FrustumResult::Inside : FrustumResult::Intersect;
}

bool Frustum::IsOBBOutside(const OBB& obb) const noexcept {
    for (const auto& plane : m_planes) {
        const glm::vec3* axes = obb.GetAxes();
        float projectedRadius = 0.0f;

        for (int i = 0; i < 3; ++i) {
            projectedRadius += std::abs(glm::dot(axes[i], plane.normal)) * obb.halfExtents[i];
        }

        if (plane.SignedDistance(obb.center) < -projectedRadius) {
            return true;
        }
    }
    return false;
}

#ifdef __SSE__
uint32_t Frustum::TestAABB4(const AABB* boxes) const noexcept {
    uint32_t result = 0xF; // Assume all visible

    for (int p = 0; p < 6; ++p) {
        __m128 planeNx = _mm_set1_ps(m_planeNormalsX[p]);
        __m128 planeNy = _mm_set1_ps(m_planeNormalsY[p]);
        __m128 planeNz = _mm_set1_ps(m_planeNormalsZ[p]);
        __m128 planeD = _mm_set1_ps(m_planeDistances[p]);

        // Load n-vertices for all 4 boxes
        __m128 nvX, nvY, nvZ;

        float nv_x[4], nv_y[4], nv_z[4];
        for (int i = 0; i < 4; ++i) {
            nv_x[i] = m_planeNormalsX[p] >= 0.0f ? boxes[i].min.x : boxes[i].max.x;
            nv_y[i] = m_planeNormalsY[p] >= 0.0f ? boxes[i].min.y : boxes[i].max.y;
            nv_z[i] = m_planeNormalsZ[p] >= 0.0f ? boxes[i].min.z : boxes[i].max.z;
        }

        nvX = _mm_loadu_ps(nv_x);
        nvY = _mm_loadu_ps(nv_y);
        nvZ = _mm_loadu_ps(nv_z);

        // Calculate signed distance for n-vertices
        __m128 dist = _mm_add_ps(
            _mm_add_ps(
                _mm_mul_ps(planeNx, nvX),
                _mm_mul_ps(planeNy, nvY)
            ),
            _mm_add_ps(
                _mm_mul_ps(planeNz, nvZ),
                planeD
            )
        );

        // Check which boxes are outside (distance < 0)
        __m128 zero = _mm_setzero_ps();
        __m128 outside = _mm_cmplt_ps(dist, zero);
        uint32_t outsideMask = static_cast<uint32_t>(_mm_movemask_ps(outside));

        result &= ~outsideMask;

        if (result == 0) {
            break; // All boxes culled
        }
    }

    return result;
}

uint32_t Frustum::TestSphere4(const glm::vec3* centers, const float* radii) const noexcept {
    uint32_t result = 0xF;

    __m128 cx = _mm_set_ps(centers[3].x, centers[2].x, centers[1].x, centers[0].x);
    __m128 cy = _mm_set_ps(centers[3].y, centers[2].y, centers[1].y, centers[0].y);
    __m128 cz = _mm_set_ps(centers[3].z, centers[2].z, centers[1].z, centers[0].z);
    __m128 r = _mm_loadu_ps(radii);
    __m128 negR = _mm_sub_ps(_mm_setzero_ps(), r);

    for (int p = 0; p < 6; ++p) {
        __m128 planeNx = _mm_set1_ps(m_planeNormalsX[p]);
        __m128 planeNy = _mm_set1_ps(m_planeNormalsY[p]);
        __m128 planeNz = _mm_set1_ps(m_planeNormalsZ[p]);
        __m128 planeD = _mm_set1_ps(m_planeDistances[p]);

        // dist = nx*cx + ny*cy + nz*cz + d
        __m128 dist = _mm_add_ps(
            _mm_add_ps(
                _mm_mul_ps(planeNx, cx),
                _mm_mul_ps(planeNy, cy)
            ),
            _mm_add_ps(
                _mm_mul_ps(planeNz, cz),
                planeD
            )
        );

        // outside if dist < -radius
        __m128 outside = _mm_cmplt_ps(dist, negR);
        uint32_t outsideMask = static_cast<uint32_t>(_mm_movemask_ps(outside));

        result &= ~outsideMask;

        if (result == 0) break;
    }

    return result;
}
#endif

bool Frustum::TestAABBCoherent(const AABB& aabb, CoherencyData& coherency) const noexcept {
    // First test the plane that failed last time
    if (coherency.lastPlane < 6) {
        const Plane& plane = m_planes[coherency.lastPlane];

        glm::vec3 nVertex;
        nVertex.x = plane.normal.x >= 0.0f ? aabb.min.x : aabb.max.x;
        nVertex.y = plane.normal.y >= 0.0f ? aabb.min.y : aabb.max.y;
        nVertex.z = plane.normal.z >= 0.0f ? aabb.min.z : aabb.max.z;

        if (plane.SignedDistance(nVertex) < 0.0f) {
            coherency.wasVisible = false;
            return false;
        }
    }

    // Test remaining planes
    for (int i = 0; i < 6; ++i) {
        if (i == coherency.lastPlane) continue;

        const Plane& plane = m_planes[i];

        glm::vec3 nVertex;
        nVertex.x = plane.normal.x >= 0.0f ? aabb.min.x : aabb.max.x;
        nVertex.y = plane.normal.y >= 0.0f ? aabb.min.y : aabb.max.y;
        nVertex.z = plane.normal.z >= 0.0f ? aabb.min.z : aabb.max.z;

        if (plane.SignedDistance(nVertex) < 0.0f) {
            coherency.lastPlane = static_cast<uint8_t>(i);
            coherency.wasVisible = false;
            return false;
        }
    }

    coherency.wasVisible = true;
    return true;
}

void FrustumCuller::CullAABBs(const AABB* boxes, size_t count, bool* outVisible) const noexcept {
    if (!m_frustum) return;

#ifdef __SSE__
    // Process 4 at a time with SIMD
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        uint32_t mask = m_frustum->TestAABB4(boxes + i);
        outVisible[i + 0] = (mask & 0x1) != 0;
        outVisible[i + 1] = (mask & 0x2) != 0;
        outVisible[i + 2] = (mask & 0x4) != 0;
        outVisible[i + 3] = (mask & 0x8) != 0;
    }

    // Handle remaining
    for (; i < count; ++i) {
        outVisible[i] = !m_frustum->IsAABBOutside(boxes[i]);
    }
#else
    for (size_t i = 0; i < count; ++i) {
        outVisible[i] = !m_frustum->IsAABBOutside(boxes[i]);
    }
#endif
}

void FrustumCuller::CullAABBsCoherent(
    const AABB* boxes,
    Frustum::CoherencyData* coherency,
    size_t count,
    bool* outVisible) const noexcept {

    if (!m_frustum) return;

    for (size_t i = 0; i < count; ++i) {
        outVisible[i] = m_frustum->TestAABBCoherent(boxes[i], coherency[i]);
    }
}

} // namespace Nova
