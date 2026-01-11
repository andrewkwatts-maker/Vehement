#pragma once

#include "../sdf/SDFPrimitive.hpp"
#include "../sdf/SDFModel.hpp"
#include "CollisionShape.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace Nova {

// Forward declarations
class SDFModel;
class SDFPrimitive;

/**
 * @brief Result of an SDF collision test
 */
struct SDFCollisionResult {
    bool hit = false;                       ///< Whether collision occurred
    glm::vec3 point{0.0f};                  ///< Contact point in world space
    glm::vec3 normal{0.0f, 1.0f, 0.0f};     ///< Contact normal (pointing away from SDF surface)
    float penetrationDepth = 0.0f;          ///< Depth of penetration (positive when inside)
    float distance = 0.0f;                  ///< Signed distance to surface
    uint32_t primitiveId = 0;               ///< ID of the primitive hit (if available)

    [[nodiscard]] explicit operator bool() const noexcept { return hit; }

    /**
     * @brief Check if result indicates penetration
     */
    [[nodiscard]] bool IsPenetrating() const noexcept { return hit && penetrationDepth > 0.0f; }
};

/**
 * @brief Result of continuous collision detection
 */
struct SDFCCDResult {
    bool hit = false;                       ///< Whether collision will occur
    float timeOfImpact = 1.0f;              ///< Time of impact [0, 1] along trajectory
    glm::vec3 point{0.0f};                  ///< Contact point at time of impact
    glm::vec3 normal{0.0f, 1.0f, 0.0f};     ///< Contact normal at time of impact
    glm::vec3 impactPosition{0.0f};         ///< Collider position at time of impact

    [[nodiscard]] explicit operator bool() const noexcept { return hit; }
};

/**
 * @brief Multiple contact points from a collision
 */
struct SDFContactManifold {
    static constexpr size_t MAX_CONTACTS = 8;

    struct Contact {
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float penetration = 0.0f;
    };

    std::array<Contact, MAX_CONTACTS> contacts;
    size_t contactCount = 0;
    glm::vec3 averageNormal{0.0f, 1.0f, 0.0f};
    float maxPenetration = 0.0f;

    void AddContact(const glm::vec3& point, const glm::vec3& normal, float penetration);
    void ComputeAverages();
    void Clear() { contactCount = 0; maxPenetration = 0.0f; }
};

/**
 * @brief Configuration for SDF collision queries
 */
struct SDFCollisionConfig {
    float epsilon = 0.0001f;                ///< Tolerance for surface detection
    float normalEpsilon = 0.001f;           ///< Step size for gradient calculation
    int maxIterations = 64;                 ///< Max iterations for iterative methods
    float surfaceOffset = 0.0f;             ///< Offset from SDF surface (for skin width)
    bool generateContacts = true;           ///< Whether to generate contact points
    int maxContacts = 4;                    ///< Maximum contacts to generate

    // CCD settings
    int ccdIterations = 32;                 ///< Iterations for continuous collision detection
    float ccdTolerance = 0.001f;            ///< CCD convergence tolerance

    // GJK/EPA settings
    int gjkMaxIterations = 64;              ///< Max GJK iterations
    int epaMaxIterations = 64;              ///< Max EPA iterations
    float epaTolerance = 0.0001f;           ///< EPA convergence tolerance
};

/**
 * @brief Interface for objects that can collide with SDF scenes
 */
class ISDFCollider {
public:
    virtual ~ISDFCollider() = default;

    /**
     * @brief Get signed distance from collider to SDF at given point
     */
    [[nodiscard]] virtual float Distance(const glm::vec3& point) const = 0;

    /**
     * @brief Get support point in given direction (for GJK)
     */
    [[nodiscard]] virtual glm::vec3 Support(const glm::vec3& direction) const = 0;

    /**
     * @brief Get collider center in world space
     */
    [[nodiscard]] virtual glm::vec3 GetCenter() const = 0;

    /**
     * @brief Get bounding sphere radius
     */
    [[nodiscard]] virtual float GetBoundingRadius() const = 0;

    /**
     * @brief Get axis-aligned bounding box
     */
    [[nodiscard]] virtual AABB GetAABB() const = 0;

    /**
     * @brief Test if collider intersects with SDF (negative distance)
     */
    [[nodiscard]] virtual bool Intersects(float sdfDistance) const {
        return sdfDistance < 0.0f;
    }

    /**
     * @brief Get collider type name for debugging
     */
    [[nodiscard]] virtual const char* GetTypeName() const = 0;
};

/**
 * @brief Point collider (simplest case)
 */
class SDFPointCollider : public ISDFCollider {
public:
    explicit SDFPointCollider(const glm::vec3& position);

    [[nodiscard]] float Distance(const glm::vec3& point) const override;
    [[nodiscard]] glm::vec3 Support(const glm::vec3& direction) const override;
    [[nodiscard]] glm::vec3 GetCenter() const override { return m_position; }
    [[nodiscard]] float GetBoundingRadius() const override { return 0.0f; }
    [[nodiscard]] AABB GetAABB() const override;
    [[nodiscard]] const char* GetTypeName() const override { return "Point"; }

    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }

private:
    glm::vec3 m_position;
};

/**
 * @brief Sphere collider
 */
class SDFSphereCollider : public ISDFCollider {
public:
    SDFSphereCollider(const glm::vec3& center, float radius);

    [[nodiscard]] float Distance(const glm::vec3& point) const override;
    [[nodiscard]] glm::vec3 Support(const glm::vec3& direction) const override;
    [[nodiscard]] glm::vec3 GetCenter() const override { return m_center; }
    [[nodiscard]] float GetBoundingRadius() const override { return m_radius; }
    [[nodiscard]] AABB GetAABB() const override;
    [[nodiscard]] const char* GetTypeName() const override { return "Sphere"; }

    void SetCenter(const glm::vec3& center) { m_center = center; }
    void SetRadius(float radius) { m_radius = radius; }
    [[nodiscard]] float GetRadius() const { return m_radius; }

private:
    glm::vec3 m_center;
    float m_radius;
};

/**
 * @brief Capsule collider (line segment with radius)
 */
class SDFCapsuleCollider : public ISDFCollider {
public:
    SDFCapsuleCollider(const glm::vec3& start, const glm::vec3& end, float radius);

    [[nodiscard]] float Distance(const glm::vec3& point) const override;
    [[nodiscard]] glm::vec3 Support(const glm::vec3& direction) const override;
    [[nodiscard]] glm::vec3 GetCenter() const override;
    [[nodiscard]] float GetBoundingRadius() const override;
    [[nodiscard]] AABB GetAABB() const override;
    [[nodiscard]] const char* GetTypeName() const override { return "Capsule"; }

    void SetEndpoints(const glm::vec3& start, const glm::vec3& end);
    void SetRadius(float radius) { m_radius = radius; }
    [[nodiscard]] const glm::vec3& GetStart() const { return m_start; }
    [[nodiscard]] const glm::vec3& GetEnd() const { return m_end; }
    [[nodiscard]] float GetRadius() const { return m_radius; }
    [[nodiscard]] float GetLength() const { return glm::length(m_end - m_start); }

private:
    glm::vec3 m_start;
    glm::vec3 m_end;
    float m_radius;
};

/**
 * @brief Oriented Bounding Box (OBB) collider
 */
class SDFBoxCollider : public ISDFCollider {
public:
    SDFBoxCollider(const glm::vec3& center, const glm::vec3& halfExtents,
                   const glm::quat& orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    [[nodiscard]] float Distance(const glm::vec3& point) const override;
    [[nodiscard]] glm::vec3 Support(const glm::vec3& direction) const override;
    [[nodiscard]] glm::vec3 GetCenter() const override { return m_center; }
    [[nodiscard]] float GetBoundingRadius() const override;
    [[nodiscard]] AABB GetAABB() const override;
    [[nodiscard]] const char* GetTypeName() const override { return "Box"; }

    void SetCenter(const glm::vec3& center) { m_center = center; }
    void SetHalfExtents(const glm::vec3& halfExtents) { m_halfExtents = halfExtents; }
    void SetOrientation(const glm::quat& orientation);

    [[nodiscard]] const glm::vec3& GetHalfExtents() const { return m_halfExtents; }
    [[nodiscard]] const glm::quat& GetOrientation() const { return m_orientation; }
    [[nodiscard]] glm::vec3 GetAxis(int index) const;
    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const;

    /**
     * @brief Transform point to local space
     */
    [[nodiscard]] glm::vec3 WorldToLocal(const glm::vec3& worldPoint) const;

    /**
     * @brief Transform point to world space
     */
    [[nodiscard]] glm::vec3 LocalToWorld(const glm::vec3& localPoint) const;

private:
    glm::vec3 m_center;
    glm::vec3 m_halfExtents;
    glm::quat m_orientation;
    glm::mat3 m_rotationMatrix;
    glm::mat3 m_inverseRotation;
};

/**
 * @brief Mesh collider using vertex sampling
 */
class SDFMeshCollider : public ISDFCollider {
public:
    SDFMeshCollider(const std::vector<glm::vec3>& vertices, const glm::mat4& transform = glm::mat4(1.0f));

    [[nodiscard]] float Distance(const glm::vec3& point) const override;
    [[nodiscard]] glm::vec3 Support(const glm::vec3& direction) const override;
    [[nodiscard]] glm::vec3 GetCenter() const override { return m_center; }
    [[nodiscard]] float GetBoundingRadius() const override { return m_boundingRadius; }
    [[nodiscard]] AABB GetAABB() const override { return m_aabb; }
    [[nodiscard]] const char* GetTypeName() const override { return "Mesh"; }

    void SetTransform(const glm::mat4& transform);
    [[nodiscard]] const std::vector<glm::vec3>& GetVertices() const { return m_worldVertices; }

private:
    void UpdateTransformedVertices();
    void ComputeBounds();

    std::vector<glm::vec3> m_localVertices;
    std::vector<glm::vec3> m_worldVertices;
    glm::mat4 m_transform;
    glm::vec3 m_center;
    float m_boundingRadius;
    AABB m_aabb;
};

/**
 * @brief SDF scene wrapper for collision queries
 */
class SDFCollisionScene {
public:
    SDFCollisionScene() = default;
    explicit SDFCollisionScene(const SDFModel* model);

    /**
     * @brief Set the SDF model for collision queries
     */
    void SetModel(const SDFModel* model) { m_model = model; }
    [[nodiscard]] const SDFModel* GetModel() const { return m_model; }

    /**
     * @brief Set custom SDF evaluation function
     */
    void SetSDFFunction(std::function<float(const glm::vec3&)> func) { m_sdfFunction = std::move(func); }

    /**
     * @brief Evaluate SDF at point
     */
    [[nodiscard]] float EvaluateSDF(const glm::vec3& point) const;

    /**
     * @brief Calculate SDF gradient (normal direction)
     */
    [[nodiscard]] glm::vec3 CalculateGradient(const glm::vec3& point, float epsilon = 0.001f) const;

    /**
     * @brief Calculate surface normal at point
     */
    [[nodiscard]] glm::vec3 CalculateNormal(const glm::vec3& point, float epsilon = 0.001f) const;

    /**
     * @brief Find closest point on SDF surface using sphere tracing
     */
    [[nodiscard]] std::optional<glm::vec3> FindClosestSurfacePoint(
        const glm::vec3& startPoint, int maxIterations = 64, float tolerance = 0.0001f) const;

    /**
     * @brief Get bounds of the SDF scene
     */
    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetBounds() const;

    /**
     * @brief Check if scene is valid for queries
     */
    [[nodiscard]] bool IsValid() const { return m_model != nullptr || m_sdfFunction != nullptr; }

private:
    const SDFModel* m_model = nullptr;
    std::function<float(const glm::vec3&)> m_sdfFunction;
};

/**
 * @brief Main SDF collision detection system
 *
 * Provides collision detection between geometric colliders and SDF scenes.
 * Supports discrete and continuous collision detection, contact generation,
 * and collision resolution.
 */
class SDFCollisionSystem {
public:
    SDFCollisionSystem();
    explicit SDFCollisionSystem(const SDFCollisionConfig& config);

    // =========================================================================
    // Configuration
    // =========================================================================

    void SetConfig(const SDFCollisionConfig& config) { m_config = config; }
    [[nodiscard]] const SDFCollisionConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Point Queries
    // =========================================================================

    /**
     * @brief Query SDF distance at a point
     */
    [[nodiscard]] float QueryDistance(const SDFCollisionScene& scene, const glm::vec3& point) const;

    /**
     * @brief Query SDF normal at a point
     */
    [[nodiscard]] glm::vec3 QueryNormal(const SDFCollisionScene& scene, const glm::vec3& point) const;

    /**
     * @brief Check if point is inside SDF
     */
    [[nodiscard]] bool IsPointInside(const SDFCollisionScene& scene, const glm::vec3& point) const;

    // =========================================================================
    // Discrete Collision Detection
    // =========================================================================

    /**
     * @brief Test collision between collider and SDF scene
     */
    [[nodiscard]] SDFCollisionResult TestCollision(
        const ISDFCollider& collider,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Test sphere collision with SDF
     */
    [[nodiscard]] SDFCollisionResult TestSphereCollision(
        const glm::vec3& center, float radius,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Test capsule collision with SDF
     */
    [[nodiscard]] SDFCollisionResult TestCapsuleCollision(
        const glm::vec3& start, const glm::vec3& end, float radius,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Test OBB collision with SDF
     */
    [[nodiscard]] SDFCollisionResult TestBoxCollision(
        const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Test mesh collision with SDF (vertex sampling)
     */
    [[nodiscard]] SDFCollisionResult TestMeshCollision(
        const std::vector<glm::vec3>& vertices,
        const glm::mat4& transform,
        const SDFCollisionScene& scene) const;

    // =========================================================================
    // Contact Manifold Generation
    // =========================================================================

    /**
     * @brief Generate contact manifold for collision
     */
    [[nodiscard]] SDFContactManifold GenerateContacts(
        const ISDFCollider& collider,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Generate sphere contact manifold
     */
    [[nodiscard]] SDFContactManifold GenerateSphereContacts(
        const glm::vec3& center, float radius,
        const SDFCollisionScene& scene) const;

    /**
     * @brief Generate box contact manifold
     */
    [[nodiscard]] SDFContactManifold GenerateBoxContacts(
        const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation,
        const SDFCollisionScene& scene) const;

    // =========================================================================
    // Continuous Collision Detection (CCD)
    // =========================================================================

    /**
     * @brief Perform continuous collision detection
     */
    [[nodiscard]] SDFCCDResult TestContinuousCollision(
        const ISDFCollider& collider,
        const glm::vec3& displacement,
        const SDFCollisionScene& scene) const;

    /**
     * @brief CCD for sphere
     */
    [[nodiscard]] SDFCCDResult TestSphereCCD(
        const glm::vec3& startCenter, float radius,
        const glm::vec3& displacement,
        const SDFCollisionScene& scene) const;

    /**
     * @brief CCD for capsule
     */
    [[nodiscard]] SDFCCDResult TestCapsuleCCD(
        const glm::vec3& start1, const glm::vec3& end1, float radius,
        const glm::vec3& displacement,
        const SDFCollisionScene& scene) const;

    // =========================================================================
    // Collision Resolution
    // =========================================================================

    /**
     * @brief Resolve collision by computing position correction
     */
    [[nodiscard]] glm::vec3 ResolveCollision(
        const ISDFCollider& collider,
        const SDFCollisionResult& result) const;

    /**
     * @brief Resolve collision and return corrected transform
     */
    [[nodiscard]] glm::mat4 ResolveCollisionTransform(
        const glm::mat4& currentTransform,
        const SDFCollisionResult& result) const;

    /**
     * @brief Iteratively resolve deep penetrations
     */
    [[nodiscard]] glm::vec3 ResolveDeepPenetration(
        const ISDFCollider& collider,
        const SDFCollisionScene& scene,
        int maxIterations = 8) const;

    // =========================================================================
    // Raycast Against SDF
    // =========================================================================

    /**
     * @brief Raycast against SDF using sphere tracing
     */
    [[nodiscard]] SDFCollisionResult Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        const SDFCollisionScene& scene) const;

    // =========================================================================
    // GJK/EPA for Convex Colliders
    // =========================================================================

    /**
     * @brief Test intersection using GJK algorithm
     */
    [[nodiscard]] bool GJKIntersection(
        const ISDFCollider& colliderA,
        const ISDFCollider& colliderB) const;

    /**
     * @brief Get penetration depth and normal using EPA
     */
    [[nodiscard]] SDFCollisionResult EPAPenetration(
        const ISDFCollider& colliderA,
        const ISDFCollider& colliderB) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        size_t queryCount = 0;
        size_t collisionTests = 0;
        size_t ccdTests = 0;
        size_t gjkIterations = 0;
        size_t epaIterations = 0;
        float totalQueryTime = 0.0f;
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = Stats(); }

private:
    // Internal helper methods
    [[nodiscard]] SDFCollisionResult TestColliderSampling(
        const ISDFCollider& collider,
        const SDFCollisionScene& scene,
        const std::vector<glm::vec3>& samplePoints) const;

    [[nodiscard]] std::vector<glm::vec3> GenerateSphereSamplePoints(
        const glm::vec3& center, float radius, int samples) const;

    [[nodiscard]] std::vector<glm::vec3> GenerateCapsuleSamplePoints(
        const glm::vec3& start, const glm::vec3& end, float radius, int samples) const;

    [[nodiscard]] std::vector<glm::vec3> GenerateBoxSamplePoints(
        const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation, int samples) const;

    // GJK support functions
    struct GJKSimplex {
        std::array<glm::vec3, 4> points;
        int count = 0;

        void Push(const glm::vec3& point);
        glm::vec3& operator[](int i) { return points[i]; }
        const glm::vec3& operator[](int i) const { return points[i]; }
    };

    [[nodiscard]] bool GJKIteration(
        const ISDFCollider& colliderA,
        const ISDFCollider& colliderB,
        GJKSimplex& simplex,
        glm::vec3& direction) const;

    [[nodiscard]] bool ProcessSimplex(GJKSimplex& simplex, glm::vec3& direction) const;
    [[nodiscard]] bool ProcessLine(GJKSimplex& simplex, glm::vec3& direction) const;
    [[nodiscard]] bool ProcessTriangle(GJKSimplex& simplex, glm::vec3& direction) const;
    [[nodiscard]] bool ProcessTetrahedron(GJKSimplex& simplex, glm::vec3& direction) const;

    // EPA helper structures
    struct EPAFace {
        std::array<int, 3> indices;
        glm::vec3 normal;
        float distance;
    };

    SDFCollisionConfig m_config;
    mutable Stats m_stats;
};

/**
 * @brief Utility functions for SDF collision
 */
namespace SDFCollisionUtil {
    /**
     * @brief Find closest point on line segment to point
     */
    [[nodiscard]] glm::vec3 ClosestPointOnSegment(
        const glm::vec3& point,
        const glm::vec3& segmentStart,
        const glm::vec3& segmentEnd);

    /**
     * @brief Find closest point on triangle to point
     */
    [[nodiscard]] glm::vec3 ClosestPointOnTriangle(
        const glm::vec3& point,
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    /**
     * @brief Compute SDF gradient using central differences
     */
    [[nodiscard]] glm::vec3 ComputeSDFGradient(
        const std::function<float(const glm::vec3&)>& sdf,
        const glm::vec3& point,
        float epsilon = 0.001f);

    /**
     * @brief Signed distance to box (local space)
     */
    [[nodiscard]] float SDFBox(const glm::vec3& point, const glm::vec3& halfExtents);

    /**
     * @brief Signed distance to sphere
     */
    [[nodiscard]] float SDFSphere(const glm::vec3& point, float radius);

    /**
     * @brief Signed distance to capsule
     */
    [[nodiscard]] float SDFCapsule(
        const glm::vec3& point,
        const glm::vec3& a, const glm::vec3& b,
        float radius);

    /**
     * @brief Triple product for GJK
     */
    [[nodiscard]] glm::vec3 TripleProduct(
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    /**
     * @brief Check if origin is enclosed by simplex
     */
    [[nodiscard]] bool SameDirection(const glm::vec3& a, const glm::vec3& b);
}

} // namespace Nova
