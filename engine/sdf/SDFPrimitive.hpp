#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Nova {

/**
 * @brief SDF primitive types
 */
enum class SDFPrimitiveType : uint8_t {
    Sphere,
    Box,
    Cylinder,
    Capsule,
    Cone,
    Torus,
    Plane,
    RoundedBox,
    Ellipsoid,
    Pyramid,
    Prism,
    Custom
};

/**
 * @brief CSG (Constructive Solid Geometry) operations
 */
enum class CSGOperation : uint8_t {
    Union,           // Combine shapes
    Subtraction,     // Carve out shape
    Intersection,    // Keep overlapping region
    SmoothUnion,     // Smooth blend union
    SmoothSubtraction,
    SmoothIntersection
};

/**
 * @brief Transform for SDF components
 */
struct SDFTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 ToMatrix() const;
    [[nodiscard]] glm::mat4 ToInverseMatrix() const;
    [[nodiscard]] glm::vec3 TransformPoint(const glm::vec3& point) const;
    [[nodiscard]] glm::vec3 InverseTransformPoint(const glm::vec3& point) const;

    static SDFTransform Identity() { return SDFTransform{}; }
    static SDFTransform Lerp(const SDFTransform& a, const SDFTransform& b, float t);
};

/**
 * @brief Material properties for SDF rendering
 */
struct SDFMaterial {
    glm::vec4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissive = 0.0f;
    glm::vec3 emissiveColor{0.0f};

    // Texture painting data
    uint32_t textureAtlasIndex = 0;
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};

    // Per-vertex color painting
    std::vector<glm::vec4> vertexColors;

    std::string texturePath;
    std::string normalMapPath;
};

/**
 * @brief Parameters for different SDF primitive types
 */
struct SDFParameters {
    // Sphere
    float radius = 0.5f;

    // Box
    glm::vec3 dimensions{1.0f};
    float cornerRadius = 0.0f;  // For rounded box

    // Cylinder/Capsule/Cone
    float height = 1.0f;
    float topRadius = 0.5f;
    float bottomRadius = 0.5f;

    // Torus
    float majorRadius = 0.5f;
    float minorRadius = 0.1f;

    // Ellipsoid
    glm::vec3 radii{0.5f, 0.3f, 0.4f};

    // Prism
    int sides = 6;

    // Smooth blend factor for CSG operations
    float smoothness = 0.1f;

    // Onion shell parameters (for clothing layers)
    float onionThickness = 0.0f;  // 0 = disabled, >0 = shell thickness
    float shellMinY = -1e10f;     // Lower Y cutoff for bounded shell
    float shellMaxY = 1e10f;      // Upper Y cutoff for bounded shell
    uint32_t flags = 0;           // Bit flags for SDF options

    // Custom SDF function ID
    std::string customFunctionId;
};

/**
 * @brief Single SDF primitive component
 */
class SDFPrimitive {
public:
    SDFPrimitive() = default;
    explicit SDFPrimitive(SDFPrimitiveType type);
    SDFPrimitive(const std::string& name, SDFPrimitiveType type);

    // =========================================================================
    // Properties
    // =========================================================================

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] uint32_t GetId() const { return m_id; }

    [[nodiscard]] SDFPrimitiveType GetType() const { return m_type; }
    void SetType(SDFPrimitiveType type) { m_type = type; }

    [[nodiscard]] const SDFTransform& GetLocalTransform() const { return m_localTransform; }
    void SetLocalTransform(const SDFTransform& transform) { m_localTransform = transform; }

    [[nodiscard]] const SDFParameters& GetParameters() const { return m_parameters; }
    SDFParameters& GetParameters() { return m_parameters; }
    void SetParameters(const SDFParameters& params) { m_parameters = params; }

    [[nodiscard]] const SDFMaterial& GetMaterial() const { return m_material; }
    SDFMaterial& GetMaterial() { return m_material; }
    void SetMaterial(const SDFMaterial& material) { m_material = material; }

    [[nodiscard]] bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }

    [[nodiscard]] bool IsLocked() const { return m_locked; }
    void SetLocked(bool locked) { m_locked = locked; }

    // =========================================================================
    // SDF Evaluation
    // =========================================================================

    /**
     * @brief Evaluate signed distance at a point (in local space)
     */
    [[nodiscard]] float EvaluateSDF(const glm::vec3& point) const;

    /**
     * @brief Calculate gradient/normal at a point
     */
    [[nodiscard]] glm::vec3 CalculateNormal(const glm::vec3& point, float epsilon = 0.001f) const;

    /**
     * @brief Get bounding box in local space
     */
    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetLocalBounds() const;

    // =========================================================================
    // Hierarchy
    // =========================================================================

    [[nodiscard]] SDFPrimitive* GetParent() const { return m_parent; }
    [[nodiscard]] const std::vector<std::unique_ptr<SDFPrimitive>>& GetChildren() const { return m_children; }
    [[nodiscard]] std::vector<std::unique_ptr<SDFPrimitive>>& GetChildren() { return m_children; }

    /**
     * @brief Add child primitive
     */
    SDFPrimitive* AddChild(std::unique_ptr<SDFPrimitive> child);

    /**
     * @brief Remove child by pointer
     */
    bool RemoveChild(SDFPrimitive* child);

    /**
     * @brief Remove child by index
     */
    bool RemoveChild(size_t index);

    /**
     * @brief Find child by name (recursive)
     */
    [[nodiscard]] SDFPrimitive* FindChild(const std::string& name);

    /**
     * @brief Find child by ID (recursive)
     */
    [[nodiscard]] SDFPrimitive* FindChildById(uint32_t id);

    /**
     * @brief Get world transform (accumulated from parents)
     */
    [[nodiscard]] SDFTransform GetWorldTransform() const;

    /**
     * @brief Get CSG operation for combining with siblings
     */
    [[nodiscard]] CSGOperation GetCSGOperation() const { return m_csgOperation; }
    void SetCSGOperation(CSGOperation op) { m_csgOperation = op; }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Clone this primitive (deep copy)
     */
    [[nodiscard]] std::unique_ptr<SDFPrimitive> Clone() const;

    /**
     * @brief Traverse hierarchy
     */
    void ForEach(const std::function<void(SDFPrimitive&)>& callback);
    void ForEach(const std::function<void(const SDFPrimitive&)>& callback) const;

private:
    static uint32_t s_nextId;

    uint32_t m_id;
    std::string m_name;
    SDFPrimitiveType m_type = SDFPrimitiveType::Sphere;
    SDFTransform m_localTransform;
    SDFParameters m_parameters;
    SDFMaterial m_material;
    CSGOperation m_csgOperation = CSGOperation::Union;

    bool m_visible = true;
    bool m_locked = false;

    SDFPrimitive* m_parent = nullptr;
    std::vector<std::unique_ptr<SDFPrimitive>> m_children;
};

/**
 * @brief SDF evaluation functions
 */
namespace SDFEval {
    [[nodiscard]] float Sphere(const glm::vec3& p, float radius);
    [[nodiscard]] float Box(const glm::vec3& p, const glm::vec3& dimensions);
    [[nodiscard]] float RoundedBox(const glm::vec3& p, const glm::vec3& dimensions, float radius);
    [[nodiscard]] float Cylinder(const glm::vec3& p, float height, float radius);
    [[nodiscard]] float Capsule(const glm::vec3& p, float height, float radius);
    [[nodiscard]] float Cone(const glm::vec3& p, float height, float radius);
    [[nodiscard]] float Torus(const glm::vec3& p, float majorRadius, float minorRadius);
    [[nodiscard]] float Plane(const glm::vec3& p, const glm::vec3& normal, float offset);
    [[nodiscard]] float Ellipsoid(const glm::vec3& p, const glm::vec3& radii);
    [[nodiscard]] float Pyramid(const glm::vec3& p, float height, float baseSize);
    [[nodiscard]] float Prism(const glm::vec3& p, int sides, float radius, float height);

    // CSG operations
    [[nodiscard]] float Union(float d1, float d2);
    [[nodiscard]] float Subtraction(float d1, float d2);
    [[nodiscard]] float Intersection(float d1, float d2);
    [[nodiscard]] float SmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float SmoothSubtraction(float d1, float d2, float k);
    [[nodiscard]] float SmoothIntersection(float d1, float d2, float k);

    // Advanced smooth blending operations (more organic)
    [[nodiscard]] float ExponentialSmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float PowerSmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float CubicSmoothUnion(float d1, float d2, float k);

    // Distance-aware blending (prevents unwanted self-smoothing during animation)
    [[nodiscard]] float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist);
}

} // namespace Nova
