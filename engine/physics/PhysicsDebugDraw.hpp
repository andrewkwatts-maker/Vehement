#pragma once

#include "CollisionShape.hpp"
#include "CollisionBody.hpp"
#include "PhysicsWorld.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Nova {

// Forward declarations
class DebugDraw;

/**
 * @brief Debug draw options for physics visualization
 */
struct PhysicsDebugDrawOptions {
    bool drawShapes = true;
    bool drawAABBs = false;
    bool drawOBBs = true;
    bool drawContactPoints = true;
    bool drawContactNormals = true;
    bool drawVelocities = false;
    bool drawCenterOfMass = false;
    bool drawSleepState = true;
    bool drawBodyType = true;
    bool drawTriggers = true;

    // Colors
    glm::vec4 staticColor{0.0f, 0.5f, 1.0f, 0.8f};
    glm::vec4 kinematicColor{1.0f, 0.5f, 0.0f, 0.8f};
    glm::vec4 dynamicColor{0.0f, 1.0f, 0.0f, 0.8f};
    glm::vec4 sleepingColor{0.5f, 0.5f, 0.5f, 0.5f};
    glm::vec4 triggerColor{1.0f, 1.0f, 0.0f, 0.4f};
    glm::vec4 contactPointColor{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 contactNormalColor{1.0f, 0.5f, 0.0f, 1.0f};
    glm::vec4 velocityColor{0.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 aabbColor{1.0f, 1.0f, 0.0f, 0.3f};

    // Scaling
    float contactPointSize = 0.05f;
    float normalLength = 0.3f;
    float velocityScale = 0.1f;
};

/**
 * @brief Recorded raycast for debug visualization
 */
struct DebugRaycast {
    glm::vec3 origin;
    glm::vec3 direction;
    float maxDistance;
    bool hit;
    glm::vec3 hitPoint;
    glm::vec3 hitNormal;
    float lifetime;
};

/**
 * @brief Recorded query for debug visualization
 */
struct DebugQuery {
    enum class Type { Sphere, Box, AABB };

    Type type;
    glm::vec3 center;
    glm::vec3 halfExtents;
    glm::quat orientation;
    float radius;
    bool hadResults;
    float lifetime;
};

/**
 * @brief Physics debug visualization system
 *
 * Provides comprehensive debug drawing for physics simulation including:
 * - Collision shapes (boxes, spheres, capsules, etc.)
 * - Contact points and normals
 * - Raycasts and queries
 * - Body state visualization (sleeping, body type)
 *
 * Integrates with the engine's DebugDraw system.
 */
class PhysicsDebugDraw {
public:
    PhysicsDebugDraw();
    explicit PhysicsDebugDraw(DebugDraw* debugDraw);
    ~PhysicsDebugDraw() = default;

    // Non-copyable, non-movable (references external DebugDraw)
    PhysicsDebugDraw(const PhysicsDebugDraw&) = delete;
    PhysicsDebugDraw& operator=(const PhysicsDebugDraw&) = delete;
    PhysicsDebugDraw(PhysicsDebugDraw&&) = delete;
    PhysicsDebugDraw& operator=(PhysicsDebugDraw&&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /**
     * @brief Set the underlying debug draw system
     */
    void SetDebugDraw(DebugDraw* debugDraw) { m_debugDraw = debugDraw; }

    /**
     * @brief Get current debug draw system
     */
    [[nodiscard]] DebugDraw* GetDebugDraw() const { return m_debugDraw; }

    /**
     * @brief Get/set draw options
     */
    [[nodiscard]] PhysicsDebugDrawOptions& GetOptions() { return m_options; }
    [[nodiscard]] const PhysicsDebugDrawOptions& GetOptions() const { return m_options; }
    void SetOptions(const PhysicsDebugDrawOptions& options) { m_options = options; }

    // =========================================================================
    // World Drawing
    // =========================================================================

    /**
     * @brief Draw all physics bodies in a world
     */
    void DrawWorld(const PhysicsWorld& world);

    /**
     * @brief Draw a single collision body
     */
    void DrawBody(const CollisionBody& body);

    /**
     * @brief Draw a collision body with custom transform
     */
    void DrawBody(const CollisionBody& body, const glm::mat4& transform);

    // =========================================================================
    // Shape Drawing
    // =========================================================================

    /**
     * @brief Draw a collision shape
     */
    void DrawShape(const CollisionShape& shape, const glm::mat4& worldTransform,
                   const glm::vec4& color);

    /**
     * @brief Draw a box shape
     */
    void DrawBox(const glm::vec3& center, const glm::vec3& halfExtents,
                 const glm::quat& orientation, const glm::vec4& color);

    /**
     * @brief Draw a sphere shape
     */
    void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color);

    /**
     * @brief Draw a capsule shape
     */
    void DrawCapsule(const glm::vec3& center, float radius, float height,
                     const glm::quat& orientation, const glm::vec4& color);

    /**
     * @brief Draw a cylinder shape
     */
    void DrawCylinder(const glm::vec3& center, float radius, float height,
                      const glm::quat& orientation, const glm::vec4& color);

    /**
     * @brief Draw a convex hull shape
     */
    void DrawConvexHull(const std::vector<glm::vec3>& vertices,
                        const glm::mat4& transform, const glm::vec4& color);

    /**
     * @brief Draw an AABB
     */
    void DrawAABB(const AABB& aabb, const glm::vec4& color);

    /**
     * @brief Draw an OBB
     */
    void DrawOBB(const OBB& obb, const glm::vec4& color);

    // =========================================================================
    // Contact Drawing
    // =========================================================================

    /**
     * @brief Draw a contact point
     */
    void DrawContactPoint(const glm::vec3& point, const glm::vec3& normal,
                          float penetration);

    /**
     * @brief Draw all contacts between two bodies
     */
    void DrawContacts(const ContactInfo& contact);

    // =========================================================================
    // Query Visualization
    // =========================================================================

    /**
     * @brief Record a raycast for visualization
     */
    void RecordRaycast(const glm::vec3& origin, const glm::vec3& direction,
                       float maxDistance, bool hit,
                       const glm::vec3& hitPoint = glm::vec3(0),
                       const glm::vec3& hitNormal = glm::vec3(0),
                       float lifetime = 1.0f);

    /**
     * @brief Record a sphere query for visualization
     */
    void RecordSphereQuery(const glm::vec3& center, float radius,
                           bool hadResults, float lifetime = 1.0f);

    /**
     * @brief Record a box query for visualization
     */
    void RecordBoxQuery(const glm::vec3& center, const glm::vec3& halfExtents,
                        const glm::quat& orientation, bool hadResults,
                        float lifetime = 1.0f);

    /**
     * @brief Draw all recorded raycasts
     */
    void DrawRaycasts();

    /**
     * @brief Draw all recorded queries
     */
    void DrawQueries();

    /**
     * @brief Update and expire recorded visualizations
     */
    void Update(float deltaTime);

    /**
     * @brief Clear all recorded visualizations
     */
    void ClearRecorded();

    // =========================================================================
    // Utility Drawing
    // =========================================================================

    /**
     * @brief Draw a velocity vector
     */
    void DrawVelocity(const glm::vec3& position, const glm::vec3& velocity);

    /**
     * @brief Draw center of mass indicator
     */
    void DrawCenterOfMass(const glm::vec3& position);

    /**
     * @brief Draw a transform gizmo
     */
    void DrawTransform(const glm::mat4& transform, float size = 0.5f);

private:
    /**
     * @brief Get color for a body based on its state
     */
    [[nodiscard]] glm::vec4 GetBodyColor(const CollisionBody& body) const;

    DebugDraw* m_debugDraw = nullptr;
    PhysicsDebugDrawOptions m_options;

    // Recorded visualizations
    std::vector<DebugRaycast> m_raycasts;
    std::vector<DebugQuery> m_queries;
};

/**
 * @brief RAII helper to temporarily enable physics debug drawing
 */
class ScopedPhysicsDebug {
public:
    ScopedPhysicsDebug(PhysicsWorld& world, DebugDraw& debugDraw)
        : m_world(world), m_wasEnabled(world.IsDebugDrawEnabled())
    {
        world.SetDebugDraw(&debugDraw);
        world.SetDebugDrawEnabled(true);
    }

    ~ScopedPhysicsDebug() {
        m_world.SetDebugDrawEnabled(m_wasEnabled);
        if (!m_wasEnabled) {
            m_world.SetDebugDraw(nullptr);
        }
    }

    ScopedPhysicsDebug(const ScopedPhysicsDebug&) = delete;
    ScopedPhysicsDebug& operator=(const ScopedPhysicsDebug&) = delete;

private:
    PhysicsWorld& m_world;
    bool m_wasEnabled;
};

} // namespace Nova
