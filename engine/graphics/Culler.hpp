#pragma once

#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <cstdint>
#include <limits>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Camera;
class Mesh;
class SceneNode;

/**
 * @brief Axis-Aligned Bounding Box for culling
 */
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    AABB() = default;
    AABB(const glm::vec3& minPt, const glm::vec3& maxPt)
        : min(minPt), max(maxPt) {}

    glm::vec3 GetCenter() const { return (min + max) * 0.5f; }
    glm::vec3 GetExtents() const { return (max - min) * 0.5f; }
    glm::vec3 GetSize() const { return max - min; }
    float GetRadius() const { return glm::length(GetExtents()); }

    bool Contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool Intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    void Expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void Expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    inline AABB Transform(const glm::mat4& matrix) const {
        // Transform all 8 corners and compute new AABB
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };

        AABB result;
        result.min = glm::vec3(std::numeric_limits<float>::max());
        result.max = glm::vec3(std::numeric_limits<float>::lowest());

        for (int i = 0; i < 8; i++) {
            glm::vec4 transformed = matrix * glm::vec4(corners[i], 1.0f);
            glm::vec3 point = glm::vec3(transformed) / transformed.w;
            result.min = glm::min(result.min, point);
            result.max = glm::max(result.max, point);
        }

        return result;
    }

    static inline AABB Merge(const AABB& a, const AABB& b) {
        return AABB(glm::min(a.min, b.min), glm::max(a.max, b.max));
    }
};

/**
 * @brief Bounding sphere for fast culling checks
 */
struct BoundingSphere {
    glm::vec3 center{0.0f};
    float radius = 0.0f;

    BoundingSphere() = default;
    BoundingSphere(const glm::vec3& c, float r) : center(c), radius(r) {}

    static BoundingSphere FromAABB(const AABB& aabb) {
        return BoundingSphere(aabb.GetCenter(), aabb.GetRadius());
    }
};

/**
 * @brief Frustum plane representation
 */
struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;

    Plane() = default;
    Plane(const glm::vec3& n, float d) : normal(n), distance(d) {}
    Plane(const glm::vec3& n, const glm::vec3& point)
        : normal(n), distance(-glm::dot(n, point)) {}

    float DistanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }

    void Normalize() {
        float length = glm::length(normal);
        if (length > 0.0001f) {
            normal /= length;
            distance /= length;
        }
    }
};

/**
 * @brief View frustum for culling
 */
struct Frustum {
    enum FrustumPlane {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        PlaneCount
    };

    std::array<Plane, PlaneCount> planes;

    inline void ExtractFromMatrix(const glm::mat4& vp) {
        // Extract frustum planes from view-projection matrix
        planes[Left].normal.x = vp[0][3] + vp[0][0];
        planes[Left].normal.y = vp[1][3] + vp[1][0];
        planes[Left].normal.z = vp[2][3] + vp[2][0];
        planes[Left].distance = vp[3][3] + vp[3][0];

        planes[Right].normal.x = vp[0][3] - vp[0][0];
        planes[Right].normal.y = vp[1][3] - vp[1][0];
        planes[Right].normal.z = vp[2][3] - vp[2][0];
        planes[Right].distance = vp[3][3] - vp[3][0];

        planes[Bottom].normal.x = vp[0][3] + vp[0][1];
        planes[Bottom].normal.y = vp[1][3] + vp[1][1];
        planes[Bottom].normal.z = vp[2][3] + vp[2][1];
        planes[Bottom].distance = vp[3][3] + vp[3][1];

        planes[Top].normal.x = vp[0][3] - vp[0][1];
        planes[Top].normal.y = vp[1][3] - vp[1][1];
        planes[Top].normal.z = vp[2][3] - vp[2][1];
        planes[Top].distance = vp[3][3] - vp[3][1];

        planes[Near].normal.x = vp[0][3] + vp[0][2];
        planes[Near].normal.y = vp[1][3] + vp[1][2];
        planes[Near].normal.z = vp[2][3] + vp[2][2];
        planes[Near].distance = vp[3][3] + vp[3][2];

        planes[Far].normal.x = vp[0][3] - vp[0][2];
        planes[Far].normal.y = vp[1][3] - vp[1][2];
        planes[Far].normal.z = vp[2][3] - vp[2][2];
        planes[Far].distance = vp[3][3] - vp[3][2];

        for (auto& plane : planes) {
            plane.Normalize();
        }
    }

    inline bool ContainsPoint(const glm::vec3& point) const {
        for (const auto& plane : planes) {
            if (plane.DistanceToPoint(point) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    inline bool ContainsSphere(const glm::vec3& center, float radius) const {
        for (const auto& plane : planes) {
            if (plane.DistanceToPoint(center) < -radius) {
                return false;
            }
        }
        return true;
    }

    inline bool ContainsAABB(const AABB& aabb) const {
        for (const auto& plane : planes) {
            glm::vec3 positive = aabb.min;
            if (plane.normal.x >= 0) positive.x = aabb.max.x;
            if (plane.normal.y >= 0) positive.y = aabb.max.y;
            if (plane.normal.z >= 0) positive.z = aabb.max.z;

            if (plane.DistanceToPoint(positive) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // Returns: -1 = outside, 0 = intersecting, 1 = inside
    inline int TestAABB(const AABB& aabb) const {
        int result = 1;

        for (const auto& plane : planes) {
            glm::vec3 positive = aabb.min;
            glm::vec3 negative = aabb.max;

            if (plane.normal.x >= 0) {
                positive.x = aabb.max.x;
                negative.x = aabb.min.x;
            }
            if (plane.normal.y >= 0) {
                positive.y = aabb.max.y;
                negative.y = aabb.min.y;
            }
            if (plane.normal.z >= 0) {
                positive.z = aabb.max.z;
                negative.z = aabb.min.z;
            }

            if (plane.DistanceToPoint(positive) < 0.0f) {
                return -1;
            }

            if (plane.DistanceToPoint(negative) < 0.0f) {
                result = 0;
            }
        }

        return result;
    }

    inline int TestSphere(const glm::vec3& center, float radius) const {
        int result = 1;

        for (const auto& plane : planes) {
            float distance = plane.DistanceToPoint(center);

            if (distance < -radius) {
                return -1;
            }

            if (distance < radius) {
                result = 0;
            }
        }

        return result;
    }
};

/**
 * @brief Cullable object information
 */
struct CullableObject {
    uint32_t id = 0;
    AABB worldBounds;
    BoundingSphere boundingSphere;
    float distanceToCamera = 0.0f;
    float screenSize = 0.0f;
    void* userData = nullptr;
    bool visible = true;
    bool occluded = false;
    int lodLevel = 0;
};

/**
 * @brief Configuration for the culling system
 */
struct CullingConfig {
    bool frustumCullingEnabled = true;
    bool occlusionCullingEnabled = true;
    bool distanceCullingEnabled = true;
    bool smallObjectCullingEnabled = true;

    float maxRenderDistance = 500.0f;
    float smallObjectThreshold = 0.01f;  // Screen-space ratio
    float occlusionQueryDelay = 0.1f;    // Seconds between occlusion queries

    // Hierarchical occlusion culling
    int occlusionHierarchyDepth = 4;
    int occlusionResolution = 256;       // Hi-Z buffer resolution

    // Multi-threaded culling
    bool useMultiThreading = false;
    int numCullingThreads = 4;
};

/**
 * @brief Occlusion query result
 */
struct OcclusionQuery {
    uint32_t queryID = 0;
    uint32_t objectID = 0;
    bool resultReady = false;
    bool visible = true;
    int frameDelay = 0;
};

/**
 * @brief Hierarchical Z-Buffer for occlusion culling
 */
class HiZBuffer {
public:
    HiZBuffer();
    ~HiZBuffer();

    bool Initialize(int width, int height, int levels = 8);
    void Shutdown();

    void Update(uint32_t depthTexture);
    bool TestAABB(const AABB& aabb, const glm::mat4& viewProjection) const;

    uint32_t GetTexture() const { return m_texture; }
    int GetLevelCount() const { return m_levelCount; }

private:
    uint32_t m_texture = 0;
    uint32_t m_framebuffer = 0;
    uint32_t m_downsampleShader = 0;
    int m_width = 0;
    int m_height = 0;
    int m_levelCount = 0;
    bool m_initialized = false;
};

/**
 * @brief Culling system for visibility determination
 *
 * Implements frustum culling, hierarchical occlusion culling,
 * distance-based culling, and small object culling.
 */
class Culler {
public:
    Culler();
    ~Culler();

    // Non-copyable
    Culler(const Culler&) = delete;
    Culler& operator=(const Culler&) = delete;

    /**
     * @brief Initialize the culling system
     */
    bool Initialize(const CullingConfig& config = CullingConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Begin culling for a new frame
     * @param camera The camera to cull against
     */
    void BeginFrame(const Camera& camera);

    /**
     * @brief End frame processing
     */
    void EndFrame();

    /**
     * @brief Register a cullable object
     * @return Object ID for future reference
     */
    uint32_t RegisterObject(const AABB& bounds, void* userData = nullptr);

    /**
     * @brief Update object bounds
     */
    void UpdateObjectBounds(uint32_t objectID, const AABB& newBounds);

    /**
     * @brief Remove a cullable object
     */
    void RemoveObject(uint32_t objectID);

    /**
     * @brief Clear all registered objects
     */
    void ClearObjects();

    /**
     * @brief Perform culling on all registered objects
     * @return Vector of visible object IDs
     */
    std::vector<uint32_t> Cull();

    /**
     * @brief Test if a single bounding box is visible
     * @param bounds World-space bounding box
     * @param transform Optional additional transform
     * @return true if visible
     */
    bool IsVisible(const AABB& bounds, const glm::mat4& transform = glm::mat4(1.0f)) const;

    /**
     * @brief Test if a sphere is visible
     */
    bool IsVisible(const glm::vec3& center, float radius) const;

    /**
     * @brief Test if a point is visible
     */
    bool IsVisible(const glm::vec3& point) const;

    /**
     * @brief Get distance from camera to point
     */
    float GetDistanceToCamera(const glm::vec3& point) const;

    /**
     * @brief Calculate screen-space size of an object
     * @param worldSize Object's world-space diameter
     * @param distance Distance from camera
     * @return Approximate screen-space size (0-1 ratio)
     */
    float CalculateScreenSize(float worldSize, float distance) const;

    /**
     * @brief Update configuration
     */
    void SetConfig(const CullingConfig& config);
    const CullingConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get current frustum
     */
    const Frustum& GetFrustum() const { return m_frustum; }

    /**
     * @brief Get culling statistics
     */
    struct Stats {
        uint32_t totalObjects = 0;
        uint32_t frustumCulled = 0;
        uint32_t occlusionCulled = 0;
        uint32_t distanceCulled = 0;
        uint32_t smallObjectCulled = 0;
        uint32_t visibleObjects = 0;
        float cullingEfficiency = 0.0f;

        void Reset() {
            totalObjects = frustumCulled = occlusionCulled = 0;
            distanceCulled = smallObjectCulled = visibleObjects = 0;
            cullingEfficiency = 0.0f;
        }
    };

    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Update Hi-Z buffer from depth texture
     */
    void UpdateOcclusionBuffer(uint32_t depthTexture);

    /**
     * @brief Get Hi-Z buffer for debugging
     */
    const HiZBuffer* GetHiZBuffer() const { return m_hiZBuffer.get(); }

private:
    // Frustum culling
    bool FrustumCull(const CullableObject& object) const;

    // Occlusion culling
    bool OcclusionCull(const CullableObject& object) const;
    void ProcessOcclusionQueries();
    void IssueOcclusionQuery(CullableObject& object);

    // Distance culling
    bool DistanceCull(const CullableObject& object) const;

    // Small object culling
    bool SmallObjectCull(const CullableObject& object) const;

    // Object management
    std::vector<CullableObject> m_objects;
    std::vector<uint32_t> m_freeIndices;
    uint32_t m_nextObjectID = 1;

    // Occlusion queries
    std::vector<OcclusionQuery> m_occlusionQueries;
    std::unique_ptr<HiZBuffer> m_hiZBuffer;

    // Camera data
    Frustum m_frustum;
    glm::vec3 m_cameraPosition{0.0f};
    glm::mat4 m_viewProjection{1.0f};
    float m_fov = 45.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    CullingConfig m_config;
    Stats m_stats;
    bool m_initialized = false;
};

/**
 * @brief BVH Node for spatial acceleration
 */
struct BVHNode {
    AABB bounds;
    int leftChild = -1;   // -1 = leaf
    int rightChild = -1;
    int objectIndex = -1; // Only for leaves

    bool IsLeaf() const { return leftChild == -1 && rightChild == -1; }
};

/**
 * @brief Bounding Volume Hierarchy for accelerated culling
 */
class BVH {
public:
    BVH() = default;
    ~BVH() = default;

    /**
     * @brief Build BVH from objects
     */
    void Build(const std::vector<CullableObject>& objects);

    /**
     * @brief Query visible objects using frustum
     */
    void QueryFrustum(const Frustum& frustum,
                      std::vector<uint32_t>& visibleIndices) const;

    /**
     * @brief Query objects within distance
     */
    void QuerySphere(const glm::vec3& center, float radius,
                     std::vector<uint32_t>& indices) const;

    /**
     * @brief Clear the BVH
     */
    void Clear() { m_nodes.clear(); }

    /**
     * @brief Check if BVH is empty
     */
    bool IsEmpty() const { return m_nodes.empty(); }

private:
    int BuildRecursive(std::vector<int>& indices, int start, int end,
                       const std::vector<CullableObject>& objects);

    void QueryFrustumRecursive(int nodeIndex, const Frustum& frustum,
                               std::vector<uint32_t>& results) const;

    void QuerySphereRecursive(int nodeIndex, const glm::vec3& center,
                              float radius, std::vector<uint32_t>& results) const;

    std::vector<BVHNode> m_nodes;
};

} // namespace Nova
