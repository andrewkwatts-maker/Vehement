#pragma once

#include "CollisionBody.hpp"
#include "CollisionShape.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>

namespace Nova {

// Forward declarations
class DebugDraw;

/**
 * @brief Result of a raycast query
 */
struct RaycastHit {
    CollisionBody* body = nullptr;
    int shapeIndex = -1;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    float distance = 0.0f;

    [[nodiscard]] bool operator<(const RaycastHit& other) const {
        return distance < other.distance;
    }
};

/**
 * @brief Result of a shape cast (sweep) query
 */
struct ShapeCastHit {
    CollisionBody* body = nullptr;
    int shapeIndex = -1;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    float fraction = 0.0f;  ///< [0, 1] along the sweep path
};

/**
 * @brief Result of an overlap query
 */
struct OverlapResult {
    CollisionBody* body = nullptr;
    int shapeIndex = -1;
};

/**
 * @brief Physics world configuration
 */
struct PhysicsWorldConfig {
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    float fixedTimestep = 1.0f / 60.0f;
    int maxSubSteps = 8;
    int velocityIterations = 8;
    int positionIterations = 3;

    // Spatial hash configuration
    float cellSize = 10.0f;

    // Sleep thresholds
    float linearSleepThreshold = 0.1f;
    float angularSleepThreshold = 0.1f;
    float sleepTimeThreshold = 0.5f;

    // Collision tolerances
    float contactBreakingThreshold = 0.02f;
    float allowedPenetration = 0.01f;
    float baumgarte = 0.2f;  ///< Position correction factor
};

/**
 * @brief Spatial hash cell for broad-phase collision detection
 */
struct SpatialHashCell {
    std::vector<CollisionBody::BodyId> bodies;
};

/**
 * @brief Collision pair for tracking contacts between bodies
 */
struct CollisionPair {
    CollisionBody::BodyId bodyA;
    CollisionBody::BodyId bodyB;

    bool operator==(const CollisionPair& other) const {
        return (bodyA == other.bodyA && bodyB == other.bodyB) ||
               (bodyA == other.bodyB && bodyB == other.bodyA);
    }
};

struct CollisionPairHash {
    size_t operator()(const CollisionPair& pair) const {
        auto minId = std::min(pair.bodyA, pair.bodyB);
        auto maxId = std::max(pair.bodyA, pair.bodyB);
        return std::hash<uint64_t>{}(static_cast<uint64_t>(minId) << 32 | maxId);
    }
};

/**
 * @brief Physics simulation world
 *
 * Manages collision bodies, performs broad-phase and narrow-phase collision
 * detection, resolves collisions, and provides query interfaces.
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    explicit PhysicsWorld(const PhysicsWorldConfig& config);
    ~PhysicsWorld();

    // Non-copyable, non-movable
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    // =========================================================================
    // Simulation
    // =========================================================================

    /**
     * @brief Step the physics simulation
     * @param deltaTime Time since last frame (variable timestep)
     *
     * Internally uses fixed timestep with accumulator for deterministic simulation.
     */
    void Step(float deltaTime);

    /**
     * @brief Perform a single fixed timestep
     */
    void FixedStep();

    /**
     * @brief Clear all accumulated time (call after pause/resume)
     */
    void ResetAccumulator() { m_accumulator = 0.0f; }

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const PhysicsWorldConfig& GetConfig() const noexcept { return m_config; }
    void SetConfig(const PhysicsWorldConfig& config) { m_config = config; }

    [[nodiscard]] const glm::vec3& GetGravity() const noexcept { return m_config.gravity; }
    void SetGravity(const glm::vec3& gravity) { m_config.gravity = gravity; }

    // =========================================================================
    // Body Management
    // =========================================================================

    /**
     * @brief Add a body to the world
     * @return Pointer to the added body (owned by world)
     */
    CollisionBody* AddBody(std::unique_ptr<CollisionBody> body);

    /**
     * @brief Create and add a body with given type
     */
    CollisionBody* CreateBody(BodyType type = BodyType::Dynamic);

    /**
     * @brief Remove a body from the world
     */
    void RemoveBody(CollisionBody* body);
    void RemoveBody(CollisionBody::BodyId id);

    /**
     * @brief Get body by ID
     */
    [[nodiscard]] CollisionBody* GetBody(CollisionBody::BodyId id);
    [[nodiscard]] const CollisionBody* GetBody(CollisionBody::BodyId id) const;

    /**
     * @brief Get all bodies
     */
    [[nodiscard]] const std::vector<std::unique_ptr<CollisionBody>>& GetBodies() const { return m_bodies; }

    /**
     * @brief Get body count
     */
    [[nodiscard]] size_t GetBodyCount() const { return m_bodies.size(); }

    /**
     * @brief Clear all bodies from the world
     */
    void Clear();

    // =========================================================================
    // Raycasting
    // =========================================================================

    /**
     * @brief Cast a ray and return the closest hit
     * @param origin Ray origin
     * @param direction Ray direction (will be normalized)
     * @param maxDistance Maximum distance to check
     * @param layerMask Only hit bodies with these layers
     * @return Hit result if found
     */
    [[nodiscard]] std::optional<RaycastHit> Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 1000.0f,
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Cast a ray and return all hits
     */
    [[nodiscard]] std::vector<RaycastHit> RaycastAll(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 1000.0f,
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Check if a ray hits anything (fast early-out)
     */
    [[nodiscard]] bool RaycastAny(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 1000.0f,
        uint32_t layerMask = CollisionLayer::All) const;

    // =========================================================================
    // Shape Casting
    // =========================================================================

    /**
     * @brief Sweep a sphere along a path
     */
    [[nodiscard]] std::optional<ShapeCastHit> SphereCast(
        const glm::vec3& origin,
        float radius,
        const glm::vec3& direction,
        float maxDistance,
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Sweep a box along a path
     */
    [[nodiscard]] std::optional<ShapeCastHit> BoxCast(
        const glm::vec3& origin,
        const glm::vec3& halfExtents,
        const glm::quat& orientation,
        const glm::vec3& direction,
        float maxDistance,
        uint32_t layerMask = CollisionLayer::All) const;

    // =========================================================================
    // Overlap Queries
    // =========================================================================

    /**
     * @brief Find all bodies overlapping a sphere
     */
    [[nodiscard]] std::vector<OverlapResult> OverlapSphere(
        const glm::vec3& center,
        float radius,
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Find all bodies overlapping a box
     */
    [[nodiscard]] std::vector<OverlapResult> OverlapBox(
        const glm::vec3& center,
        const glm::vec3& halfExtents,
        const glm::quat& orientation = glm::quat(1, 0, 0, 0),
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Find all bodies overlapping an AABB
     */
    [[nodiscard]] std::vector<OverlapResult> OverlapAABB(
        const AABB& aabb,
        uint32_t layerMask = CollisionLayer::All) const;

    /**
     * @brief Check if a point is inside any body
     */
    [[nodiscard]] CollisionBody* PointQuery(
        const glm::vec3& point,
        uint32_t layerMask = CollisionLayer::All) const;

    // =========================================================================
    // Debug Visualization
    // =========================================================================

    /**
     * @brief Enable/disable debug drawing
     */
    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }

    /**
     * @brief Set debug draw interface
     */
    void SetDebugDraw(DebugDraw* debugDraw) { m_debugDraw = debugDraw; }

    /**
     * @brief Draw debug visualization
     */
    void DebugRender();

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        size_t bodyCount = 0;
        size_t activeBodyCount = 0;
        size_t broadPhasePairs = 0;
        size_t narrowPhaseTests = 0;
        size_t contactCount = 0;
        float stepTime = 0.0f;
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }

private:
    // Simulation steps
    void IntegrateForces(float dt);
    void BroadPhase();
    void NarrowPhase();
    void ResolveCollisions(float dt);
    void IntegrateVelocities(float dt);
    void UpdateSleepStates(float dt);

    // Spatial hash operations
    void RebuildSpatialHash();
    void GetCellsForAABB(const AABB& aabb, std::vector<glm::ivec3>& cells) const;
    [[nodiscard]] glm::ivec3 GetCellCoord(const glm::vec3& pos) const;
    [[nodiscard]] size_t HashCell(const glm::ivec3& cell) const;

    // Collision detection
    bool TestCollision(CollisionBody& bodyA, CollisionBody& bodyB, ContactInfo& contact) const;
    bool TestShapeCollision(
        const CollisionShape& shapeA, const glm::mat4& transformA,
        const CollisionShape& shapeB, const glm::mat4& transformB,
        std::vector<ContactPoint>& contacts) const;

    // Shape-specific collision tests
    bool TestSphereSphere(
        const glm::vec3& centerA, float radiusA,
        const glm::vec3& centerB, float radiusB,
        ContactPoint& contact) const;

    bool TestSphereBox(
        const glm::vec3& sphereCenter, float sphereRadius,
        const OBB& box,
        ContactPoint& contact) const;

    bool TestBoxBox(
        const OBB& boxA, const OBB& boxB,
        std::vector<ContactPoint>& contacts) const;

    bool TestCapsuleCapsule(
        const glm::vec3& startA, const glm::vec3& endA, float radiusA,
        const glm::vec3& startB, const glm::vec3& endB, float radiusB,
        ContactPoint& contact) const;

    bool TestSphereCapsule(
        const glm::vec3& sphereCenter, float sphereRadius,
        const glm::vec3& capsuleStart, const glm::vec3& capsuleEnd, float capsuleRadius,
        ContactPoint& contact) const;

    // Collision resolution
    void ResolveContact(CollisionBody& bodyA, CollisionBody& bodyB,
                        const ContactPoint& contact, float dt);

    // Ray intersection helpers
    bool RaycastBody(
        const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
        const CollisionBody& body, RaycastHit& hit) const;

    bool RaycastShape(
        const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
        const CollisionShape& shape, const glm::mat4& transform,
        float& hitDistance, glm::vec3& hitNormal) const;

    // Configuration
    PhysicsWorldConfig m_config;

    // Bodies
    std::vector<std::unique_ptr<CollisionBody>> m_bodies;
    std::unordered_map<CollisionBody::BodyId, CollisionBody*> m_bodyMap;

    // Spatial hash for broad phase
    std::unordered_map<size_t, SpatialHashCell> m_spatialHash;

    // Collision pairs from broad phase
    std::vector<CollisionPair> m_broadPhasePairs;

    // Active contacts
    std::unordered_set<CollisionPair, CollisionPairHash> m_activeContacts;
    std::unordered_set<CollisionPair, CollisionPairHash> m_previousContacts;

    // Time accumulator for fixed timestep
    float m_accumulator = 0.0f;

    // Debug
    bool m_debugDrawEnabled = false;
    DebugDraw* m_debugDraw = nullptr;

    // Statistics
    Stats m_stats;
};

} // namespace Nova
