#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <expected>

namespace Nova {

/**
 * @brief Types of collision shapes supported by the physics system
 */
enum class ShapeType : uint8_t {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    ConvexHull,
    TriangleMesh,
    Compound
};

/**
 * @brief Convert shape type to string for debugging/serialization
 */
[[nodiscard]] constexpr const char* ShapeTypeToString(ShapeType type) noexcept {
    switch (type) {
        case ShapeType::Box:          return "box";
        case ShapeType::Sphere:       return "sphere";
        case ShapeType::Capsule:      return "capsule";
        case ShapeType::Cylinder:     return "cylinder";
        case ShapeType::ConvexHull:   return "convex_hull";
        case ShapeType::TriangleMesh: return "triangle_mesh";
        case ShapeType::Compound:     return "compound";
        default:                      return "unknown";
    }
}

/**
 * @brief Parse shape type from string
 */
[[nodiscard]] std::optional<ShapeType> ShapeTypeFromString(const std::string& str) noexcept;

/**
 * @brief Physical material properties for collision response
 */
struct PhysicsMaterial {
    float friction = 0.5f;      ///< Coulomb friction coefficient [0, 1+]
    float restitution = 0.3f;   ///< Bounciness [0, 1]
    float density = 1.0f;       ///< Density in kg/m^3 (for mass calculation)

    [[nodiscard]] bool operator==(const PhysicsMaterial&) const = default;

    // JSON serialization
    [[nodiscard]] nlohmann::json ToJson() const;
    static PhysicsMaterial FromJson(const nlohmann::json& j);
};

/**
 * @brief Local transform offset from entity origin
 */
struct ShapeTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] glm::mat4 ToMatrix() const;
    [[nodiscard]] glm::vec3 TransformPoint(const glm::vec3& point) const;
    [[nodiscard]] glm::vec3 TransformDirection(const glm::vec3& dir) const;
    [[nodiscard]] glm::vec3 InverseTransformPoint(const glm::vec3& point) const;

    // JSON serialization
    [[nodiscard]] nlohmann::json ToJson() const;
    static ShapeTransform FromJson(const nlohmann::json& j);
};

// Forward declaration for compound shapes
class CollisionShape;

/**
 * @brief Shape-specific parameters
 */
namespace ShapeParams {

struct Box {
    glm::vec3 halfExtents{0.5f};

    [[nodiscard]] float GetVolume() const {
        return 8.0f * halfExtents.x * halfExtents.y * halfExtents.z;
    }

    [[nodiscard]] glm::mat3 GetInertiaTensor(float mass) const;
};

struct Sphere {
    float radius = 0.5f;

    [[nodiscard]] float GetVolume() const {
        constexpr float k4PiOver3 = 4.18879032f;
        return k4PiOver3 * radius * radius * radius;
    }

    [[nodiscard]] glm::mat3 GetInertiaTensor(float mass) const;
};

struct Capsule {
    float radius = 0.3f;
    float height = 1.0f;  ///< Height of the cylinder portion (total = height + 2*radius)

    [[nodiscard]] float GetTotalHeight() const { return height + 2.0f * radius; }
    [[nodiscard]] float GetVolume() const;
    [[nodiscard]] glm::mat3 GetInertiaTensor(float mass) const;
};

struct Cylinder {
    float radius = 0.5f;
    float height = 1.0f;

    [[nodiscard]] float GetVolume() const {
        constexpr float kPi = 3.14159265f;
        return kPi * radius * radius * height;
    }

    [[nodiscard]] glm::mat3 GetInertiaTensor(float mass) const;
};

struct ConvexHull {
    std::vector<glm::vec3> vertices;

    [[nodiscard]] float GetVolume() const;
    [[nodiscard]] glm::vec3 GetCentroid() const;
    [[nodiscard]] glm::mat3 GetInertiaTensor(float mass) const;
};

struct TriangleMesh {
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    std::string meshFilePath;  ///< Optional external mesh file reference

    [[nodiscard]] size_t GetTriangleCount() const { return indices.size() / 3; }
};

struct Compound {
    std::vector<std::shared_ptr<CollisionShape>> children;
};

} // namespace ShapeParams

/**
 * @brief Variant containing all shape parameter types
 */
using ShapeParamsVariant = std::variant<
    ShapeParams::Box,
    ShapeParams::Sphere,
    ShapeParams::Capsule,
    ShapeParams::Cylinder,
    ShapeParams::ConvexHull,
    ShapeParams::TriangleMesh,
    ShapeParams::Compound
>;

/**
 * @brief Axis-Aligned Bounding Box for broad-phase collision
 */
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    [[nodiscard]] glm::vec3 GetCenter() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 GetExtents() const { return (max - min) * 0.5f; }
    [[nodiscard]] glm::vec3 GetSize() const { return max - min; }
    [[nodiscard]] float GetVolume() const {
        glm::vec3 size = GetSize();
        return size.x * size.y * size.z;
    }

    [[nodiscard]] bool Contains(const glm::vec3& point) const;
    [[nodiscard]] bool Intersects(const AABB& other) const;
    void Expand(const glm::vec3& point);
    void Expand(const AABB& other);

    [[nodiscard]] static AABB FromCenterExtents(const glm::vec3& center, const glm::vec3& extents);
};

/**
 * @brief Oriented Bounding Box for tighter collision bounds
 */
struct OBB {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] glm::vec3 GetAxis(int index) const;
    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const;
    [[nodiscard]] AABB GetAABB() const;
    [[nodiscard]] bool Contains(const glm::vec3& point) const;
    [[nodiscard]] bool Intersects(const OBB& other) const;
    [[nodiscard]] glm::vec3 ClosestPoint(const glm::vec3& point) const;
};

/**
 * @brief Collision shape definition
 *
 * Represents a single collision primitive with local transform, material,
 * and shape-specific parameters. Can be combined in compound shapes.
 */
class CollisionShape {
public:
    CollisionShape() = default;
    explicit CollisionShape(ShapeType type);
    CollisionShape(ShapeType type, const ShapeParamsVariant& params);

    ~CollisionShape() = default;

    // Copy and move
    CollisionShape(const CollisionShape&) = default;
    CollisionShape& operator=(const CollisionShape&) = default;
    CollisionShape(CollisionShape&&) noexcept = default;
    CollisionShape& operator=(CollisionShape&&) noexcept = default;

    // =========================================================================
    // Shape Type and Parameters
    // =========================================================================

    [[nodiscard]] ShapeType GetType() const noexcept { return m_type; }
    void SetType(ShapeType type);

    template<typename T>
    [[nodiscard]] T* GetParams() {
        return std::get_if<T>(&m_params);
    }

    template<typename T>
    [[nodiscard]] const T* GetParams() const {
        return std::get_if<T>(&m_params);
    }

    template<typename T>
    void SetParams(const T& params) {
        m_params = params;
        m_boundsDirty = true;
    }

    [[nodiscard]] const ShapeParamsVariant& GetParamsVariant() const { return m_params; }

    // =========================================================================
    // Transform
    // =========================================================================

    [[nodiscard]] const ShapeTransform& GetLocalTransform() const noexcept { return m_localTransform; }
    void SetLocalTransform(const ShapeTransform& transform);
    void SetLocalPosition(const glm::vec3& pos);
    void SetLocalRotation(const glm::quat& rot);

    // =========================================================================
    // Material
    // =========================================================================

    [[nodiscard]] const PhysicsMaterial& GetMaterial() const noexcept { return m_material; }
    void SetMaterial(const PhysicsMaterial& material) { m_material = material; }

    // =========================================================================
    // Trigger Support
    // =========================================================================

    [[nodiscard]] bool IsTrigger() const noexcept { return m_isTrigger; }
    void SetTrigger(bool trigger) { m_isTrigger = trigger; }

    [[nodiscard]] const std::string& GetTriggerEvent() const noexcept { return m_triggerEvent; }
    void SetTriggerEvent(const std::string& event) { m_triggerEvent = event; }

    // =========================================================================
    // Bounds Computation
    // =========================================================================

    /**
     * @brief Compute AABB in local space (with local transform applied)
     */
    [[nodiscard]] AABB ComputeLocalAABB() const;

    /**
     * @brief Compute AABB in world space given entity transform
     */
    [[nodiscard]] AABB ComputeWorldAABB(const glm::mat4& worldTransform) const;

    /**
     * @brief Compute OBB in local space
     */
    [[nodiscard]] OBB ComputeLocalOBB() const;

    /**
     * @brief Compute OBB in world space given entity transform
     */
    [[nodiscard]] OBB ComputeWorldOBB(const glm::mat4& worldTransform) const;

    // =========================================================================
    // Mass Properties
    // =========================================================================

    /**
     * @brief Calculate volume of the shape
     */
    [[nodiscard]] float GetVolume() const;

    /**
     * @brief Calculate mass based on material density and volume
     */
    [[nodiscard]] float CalculateMass() const;

    /**
     * @brief Calculate inertia tensor for a given mass
     */
    [[nodiscard]] glm::mat3 CalculateInertiaTensor(float mass) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize shape to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize shape from JSON
     */
    static std::expected<CollisionShape, std::string> FromJson(const nlohmann::json& j);

    /**
     * @brief Create default shape of given type
     */
    static CollisionShape CreateDefault(ShapeType type);

    // =========================================================================
    // Factory Methods
    // =========================================================================

    static CollisionShape CreateBox(const glm::vec3& halfExtents);
    static CollisionShape CreateSphere(float radius);
    static CollisionShape CreateCapsule(float radius, float height);
    static CollisionShape CreateCylinder(float radius, float height);

private:
    void InitDefaultParams();
    void MarkBoundsDirty() { m_boundsDirty = true; }

    ShapeType m_type = ShapeType::Box;
    ShapeParamsVariant m_params;
    ShapeTransform m_localTransform;
    PhysicsMaterial m_material;

    bool m_isTrigger = false;
    std::string m_triggerEvent;

    // Cached bounds (mutable for lazy computation)
    mutable bool m_boundsDirty = true;
    mutable AABB m_cachedLocalAABB;
};

} // namespace Nova
