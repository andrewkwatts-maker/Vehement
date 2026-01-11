/**
 * @file RayPicker.hpp
 * @brief Ray casting and object picking system for the Nova3D/Vehement editor
 *
 * Provides comprehensive picking functionality including:
 * - Screen-to-world ray generation from mouse position
 * - Ray-AABB intersection for bounding boxes
 * - Ray-SDF intersection using sphere tracing
 * - Ray-mesh intersection for polygon objects
 * - Multi-select support with Ctrl/Shift modifiers
 * - Marquee selection (rectangular drag select)
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <functional>
#include <memory>
#include <limits>
#include <cstdint>
#include <cmath>

namespace Nova {

// Forward declarations
class Camera;
class Mesh;
class SDFModel;
struct AABB;

/**
 * @brief Ray structure for picking operations
 */
struct PickRay {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};

    PickRay() noexcept = default;

    PickRay(const glm::vec3& o, const glm::vec3& d) noexcept
        : origin(o), direction(glm::normalize(d)) {}

    /**
     * @brief Get point along ray at distance t
     */
    [[nodiscard]] constexpr glm::vec3 GetPoint(float t) const noexcept {
        return origin + direction * t;
    }

    /**
     * @brief Get inverse direction for optimized AABB tests
     */
    [[nodiscard]] glm::vec3 GetInverseDirection() const noexcept {
        constexpr float epsilon = 1e-10f;
        return glm::vec3(
            1.0f / (std::abs(direction.x) < epsilon ? epsilon : direction.x),
            1.0f / (std::abs(direction.y) < epsilon ? epsilon : direction.y),
            1.0f / (std::abs(direction.z) < epsilon ? epsilon : direction.z)
        );
    }
};

/**
 * @brief Result of a picking operation
 */
struct PickResult {
    uint64_t objectId = 0;                              // Unique identifier of picked object
    glm::vec3 hitPoint{0.0f};                           // World-space hit position
    glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};              // Surface normal at hit point
    float distance = std::numeric_limits<float>::max(); // Distance from ray origin
    int triangleIndex = -1;                             // Triangle index for mesh hits (-1 if N/A)
    int primitiveIndex = -1;                            // SDF primitive index (-1 if N/A)
    void* userData = nullptr;                           // Optional user data pointer

    /**
     * @brief Check if this result represents a valid hit
     */
    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return distance < std::numeric_limits<float>::max() && objectId != 0;
    }

    /**
     * @brief Comparison for sorting by distance
     */
    [[nodiscard]] constexpr bool operator<(const PickResult& other) const noexcept {
        return distance < other.distance;
    }

    /**
     * @brief Check if this hit is closer than another
     */
    [[nodiscard]] constexpr bool IsCloserThan(const PickResult& other) const noexcept {
        return distance < other.distance;
    }
};

/**
 * @brief Selection modifier keys
 */
enum class SelectionModifier : uint8_t {
    None    = 0,        // Replace selection
    Ctrl    = 1 << 0,   // Toggle selection (add/remove)
    Shift   = 1 << 1,   // Add to selection
    Alt     = 1 << 2    // Subtract from selection
};

// Enable bitwise operations for SelectionModifier
inline SelectionModifier operator|(SelectionModifier a, SelectionModifier b) {
    return static_cast<SelectionModifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SelectionModifier operator&(SelectionModifier a, SelectionModifier b) {
    return static_cast<SelectionModifier>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasModifier(SelectionModifier flags, SelectionModifier check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

/**
 * @brief Marquee (rectangular) selection state
 */
struct MarqueeSelection {
    glm::vec2 startPoint{0.0f};     // Screen-space start position
    glm::vec2 endPoint{0.0f};       // Screen-space end position
    bool isActive = false;          // Whether marquee selection is in progress

    /**
     * @brief Get normalized rectangle (min/max corners)
     */
    [[nodiscard]] void GetNormalizedRect(glm::vec2& outMin, glm::vec2& outMax) const noexcept {
        outMin.x = glm::min(startPoint.x, endPoint.x);
        outMin.y = glm::min(startPoint.y, endPoint.y);
        outMax.x = glm::max(startPoint.x, endPoint.x);
        outMax.y = glm::max(startPoint.y, endPoint.y);
    }

    /**
     * @brief Get width and height of selection rectangle
     */
    [[nodiscard]] glm::vec2 GetSize() const noexcept {
        return glm::abs(endPoint - startPoint);
    }

    /**
     * @brief Check if a point is inside the marquee rectangle
     */
    [[nodiscard]] bool Contains(const glm::vec2& point) const noexcept {
        glm::vec2 minPt, maxPt;
        GetNormalizedRect(minPt, maxPt);
        return point.x >= minPt.x && point.x <= maxPt.x &&
               point.y >= minPt.y && point.y <= maxPt.y;
    }
};

/**
 * @brief Pickable object interface for custom object types
 */
struct IPickable {
    virtual ~IPickable() = default;

    /**
     * @brief Get object's unique identifier
     */
    [[nodiscard]] virtual uint64_t GetPickId() const = 0;

    /**
     * @brief Get object's axis-aligned bounding box
     */
    [[nodiscard]] virtual bool GetPickBounds(glm::vec3& outMin, glm::vec3& outMax) const = 0;

    /**
     * @brief Get object's world transform matrix
     */
    [[nodiscard]] virtual glm::mat4 GetPickTransform() const = 0;

    /**
     * @brief Optional: Get mesh for detailed intersection (return nullptr if not available)
     */
    [[nodiscard]] virtual const Mesh* GetPickMesh() const { return nullptr; }

    /**
     * @brief Optional: Get SDF model for detailed intersection (return nullptr if not available)
     */
    [[nodiscard]] virtual const SDFModel* GetPickSDFModel() const { return nullptr; }
};

/**
 * @brief Configuration for ray picking behavior
 */
struct RayPickerConfig {
    // Ray marching settings for SDF
    int sdfMaxSteps = 64;                   // Maximum sphere tracing steps
    float sdfMaxDistance = 1000.0f;         // Maximum ray travel distance
    float sdfHitThreshold = 0.001f;         // Surface hit threshold

    // General settings
    float maxPickDistance = 10000.0f;       // Maximum picking distance
    bool useDetailedIntersection = true;    // Use mesh/SDF intersection vs AABB only
    bool sortByDistance = true;             // Sort results by distance

    // Marquee settings
    float marqueeMinSize = 5.0f;            // Minimum marquee size to register (pixels)
    bool marqueeIncludePartial = false;     // Include objects partially inside marquee
};

/**
 * @brief Ray Picker - Object selection via raycasting
 *
 * Features:
 * - Screen-to-world ray generation using camera projection
 * - Ray-AABB intersection for fast culling
 * - Ray-SDF sphere tracing for precise SDF object hits
 * - Ray-triangle intersection for mesh objects
 * - Marquee (rectangular drag) selection
 * - Multi-select with keyboard modifiers
 */
class RayPicker {
public:
    RayPicker();
    ~RayPicker();

    // Non-copyable, movable
    RayPicker(const RayPicker&) = delete;
    RayPicker& operator=(const RayPicker&) = delete;
    RayPicker(RayPicker&&) noexcept = default;
    RayPicker& operator=(RayPicker&&) noexcept = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get configuration
     */
    [[nodiscard]] RayPickerConfig& GetConfig() noexcept { return m_config; }
    [[nodiscard]] const RayPickerConfig& GetConfig() const noexcept { return m_config; }

    /**
     * @brief Set configuration
     */
    void SetConfig(const RayPickerConfig& config) { m_config = config; }

    // =========================================================================
    // Ray Generation
    // =========================================================================

    /**
     * @brief Convert screen position to world-space ray
     * @param screenPos Screen position in pixels (origin at top-left)
     * @param screenSize Screen dimensions in pixels
     * @param camera Camera for projection/view matrices
     * @return Ray in world space
     */
    [[nodiscard]] PickRay ScreenToRay(
        const glm::vec2& screenPos,
        const glm::vec2& screenSize,
        const Camera& camera) const;

    /**
     * @brief Convert normalized device coordinates to world-space ray
     * @param ndcPos NDC position (-1 to 1 range)
     * @param camera Camera for projection/view matrices
     * @return Ray in world space
     */
    [[nodiscard]] PickRay NDCToRay(
        const glm::vec2& ndcPos,
        const Camera& camera) const;

    // =========================================================================
    // Single Object Picking
    // =========================================================================

    /**
     * @brief Pick the closest object at screen position
     * @param screenPos Screen position in pixels
     * @param screenSize Screen dimensions in pixels
     * @param camera Camera for ray generation
     * @param objects Objects to test against
     * @return Pick result (check IsValid() for hit)
     */
    [[nodiscard]] PickResult Pick(
        const glm::vec2& screenPos,
        const glm::vec2& screenSize,
        const Camera& camera,
        const std::vector<IPickable*>& objects) const;

    /**
     * @brief Pick using a pre-computed ray
     * @param ray World-space ray
     * @param objects Objects to test against
     * @return Pick result (check IsValid() for hit)
     */
    [[nodiscard]] PickResult Pick(
        const PickRay& ray,
        const std::vector<IPickable*>& objects) const;

    // =========================================================================
    // Multi-Object Picking
    // =========================================================================

    /**
     * @brief Pick all objects along the ray
     * @param screenPos Screen position in pixels
     * @param screenSize Screen dimensions in pixels
     * @param camera Camera for ray generation
     * @param objects Objects to test against
     * @return All pick results sorted by distance
     */
    [[nodiscard]] std::vector<PickResult> PickAll(
        const glm::vec2& screenPos,
        const glm::vec2& screenSize,
        const Camera& camera,
        const std::vector<IPickable*>& objects) const;

    /**
     * @brief Pick all objects along a pre-computed ray
     * @param ray World-space ray
     * @param objects Objects to test against
     * @return All pick results sorted by distance
     */
    [[nodiscard]] std::vector<PickResult> PickAll(
        const PickRay& ray,
        const std::vector<IPickable*>& objects) const;

    // =========================================================================
    // Marquee Selection
    // =========================================================================

    /**
     * @brief Start marquee selection
     * @param screenPos Starting screen position
     */
    void BeginMarquee(const glm::vec2& screenPos);

    /**
     * @brief Update marquee selection
     * @param screenPos Current screen position
     */
    void UpdateMarquee(const glm::vec2& screenPos);

    /**
     * @brief End marquee selection and get selected objects
     * @param screenSize Screen dimensions
     * @param camera Camera for projection
     * @param objects Objects to test against
     * @return IDs of objects within the marquee
     */
    [[nodiscard]] std::vector<uint64_t> EndMarquee(
        const glm::vec2& screenSize,
        const Camera& camera,
        const std::vector<IPickable*>& objects);

    /**
     * @brief Cancel marquee selection without selecting
     */
    void CancelMarquee();

    /**
     * @brief Get objects within marquee (without ending selection)
     * @param screenSize Screen dimensions
     * @param camera Camera for projection
     * @param objects Objects to test against
     * @return IDs of objects within the marquee
     */
    [[nodiscard]] std::vector<uint64_t> MarqueeSelect(
        const glm::vec2& screenSize,
        const Camera& camera,
        const std::vector<IPickable*>& objects) const;

    /**
     * @brief Get current marquee state
     */
    [[nodiscard]] const MarqueeSelection& GetMarquee() const noexcept { return m_marquee; }

    /**
     * @brief Check if marquee selection is active
     */
    [[nodiscard]] bool IsMarqueeActive() const noexcept { return m_marquee.isActive; }

    // =========================================================================
    // Low-Level Intersection Tests
    // =========================================================================

    /**
     * @brief Ray-AABB intersection test
     * @param ray World-space ray
     * @param aabbMin AABB minimum corner
     * @param aabbMax AABB maximum corner
     * @param outDistance Distance to hit point (if hit)
     * @return true if ray intersects AABB
     */
    [[nodiscard]] static bool IntersectAABB(
        const PickRay& ray,
        const glm::vec3& aabbMin,
        const glm::vec3& aabbMax,
        float& outDistance);

    /**
     * @brief Ray-AABB intersection test with transform
     * @param ray World-space ray
     * @param aabbMin AABB minimum corner (local space)
     * @param aabbMax AABB maximum corner (local space)
     * @param transform Object world transform
     * @param outDistance Distance to hit point (if hit)
     * @return true if ray intersects transformed AABB
     */
    [[nodiscard]] static bool IntersectAABBTransformed(
        const PickRay& ray,
        const glm::vec3& aabbMin,
        const glm::vec3& aabbMax,
        const glm::mat4& transform,
        float& outDistance);

    /**
     * @brief Ray-sphere intersection test
     * @param ray World-space ray
     * @param center Sphere center
     * @param radius Sphere radius
     * @param outDistance Distance to hit point (if hit)
     * @return true if ray intersects sphere
     */
    [[nodiscard]] static bool IntersectSphere(
        const PickRay& ray,
        const glm::vec3& center,
        float radius,
        float& outDistance);

    /**
     * @brief Ray-plane intersection test
     * @param ray World-space ray
     * @param planePoint Point on plane
     * @param planeNormal Plane normal
     * @param outDistance Distance to hit point (if hit)
     * @return true if ray intersects plane
     */
    [[nodiscard]] static bool IntersectPlane(
        const PickRay& ray,
        const glm::vec3& planePoint,
        const glm::vec3& planeNormal,
        float& outDistance);

    /**
     * @brief Ray-triangle intersection test (Moller-Trumbore algorithm)
     * @param ray World-space ray
     * @param v0, v1, v2 Triangle vertices
     * @param outDistance Distance to hit point (if hit)
     * @param outBarycentrics Barycentric coordinates of hit (optional)
     * @return true if ray intersects triangle
     */
    [[nodiscard]] static bool IntersectTriangle(
        const PickRay& ray,
        const glm::vec3& v0,
        const glm::vec3& v1,
        const glm::vec3& v2,
        float& outDistance,
        glm::vec2* outBarycentrics = nullptr);

    /**
     * @brief Ray-mesh intersection test
     * @param ray World-space ray
     * @param mesh Mesh to test
     * @param transform Mesh world transform
     * @param result Output pick result with hit details
     * @return true if ray intersects mesh
     */
    [[nodiscard]] bool IntersectMesh(
        const PickRay& ray,
        const Mesh& mesh,
        const glm::mat4& transform,
        PickResult& result) const;

    /**
     * @brief Ray-SDF intersection using sphere tracing
     * @param ray World-space ray
     * @param sdfModel SDF model to trace
     * @param transform Model world transform
     * @param result Output pick result with hit details
     * @return true if ray hits SDF surface
     */
    [[nodiscard]] bool IntersectSDF(
        const PickRay& ray,
        const SDFModel& sdfModel,
        const glm::mat4& transform,
        PickResult& result) const;

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * @brief Project world point to screen coordinates
     * @param worldPos World-space position
     * @param screenSize Screen dimensions
     * @param camera Camera for projection
     * @return Screen coordinates (or negative if behind camera)
     */
    [[nodiscard]] static glm::vec2 WorldToScreen(
        const glm::vec3& worldPos,
        const glm::vec2& screenSize,
        const Camera& camera);

    /**
     * @brief Check if world point is visible on screen
     * @param worldPos World-space position
     * @param camera Camera for frustum check
     * @return true if point is in front of camera
     */
    [[nodiscard]] static bool IsPointVisible(
        const glm::vec3& worldPos,
        const Camera& camera);

    /**
     * @brief Get frustum planes for marquee selection
     * @param screenMin Minimum screen corner of marquee
     * @param screenMax Maximum screen corner of marquee
     * @param screenSize Screen dimensions
     * @param camera Camera for projection
     * @param outPlanes Output array of 6 frustum planes (left, right, bottom, top, near, far)
     */
    static void GetMarqueeFrustum(
        const glm::vec2& screenMin,
        const glm::vec2& screenMax,
        const glm::vec2& screenSize,
        const Camera& camera,
        glm::vec4 outPlanes[6]);

private:
    /**
     * @brief Test single pickable object against ray
     */
    [[nodiscard]] PickResult TestPickable(
        const PickRay& ray,
        IPickable* pickable) const;

    /**
     * @brief Check if object is inside marquee frustum
     */
    [[nodiscard]] bool IsInMarqueeFrustum(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const glm::mat4& transform,
        const glm::vec4 frustumPlanes[6]) const;

    /**
     * @brief Check if AABB is inside or intersects frustum
     */
    [[nodiscard]] static bool AABBInFrustum(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const glm::vec4 frustumPlanes[6],
        bool requireFullyInside);

    RayPickerConfig m_config;
    MarqueeSelection m_marquee;
};

} // namespace Nova
