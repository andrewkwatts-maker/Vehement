#pragma once

#include "ConfigSchema.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <variant>

namespace Vehement {
namespace Config {

// ============================================================================
// Forward Declarations
// ============================================================================

class ConfigRegistry;

// ============================================================================
// Collision Shape Configuration
// ============================================================================

/**
 * @brief Box collision shape parameters
 */
struct BoxShapeParams {
    glm::vec3 halfExtents{0.5f, 0.5f, 0.5f};
    glm::vec3 offset{0.0f};
};

/**
 * @brief Sphere collision shape parameters
 */
struct SphereShapeParams {
    float radius = 0.5f;
    glm::vec3 offset{0.0f};
};

/**
 * @brief Capsule collision shape parameters
 */
struct CapsuleShapeParams {
    float radius = 0.3f;
    float height = 1.0f;
    glm::vec3 offset{0.0f};
};

/**
 * @brief Mesh collision shape parameters
 */
struct MeshShapeParams {
    std::string meshPath;
    glm::vec3 scale{1.0f};
    bool convex = true;
};

/**
 * @brief Compound collision shape (multiple sub-shapes)
 */
struct CompoundShapeParams {
    struct SubShape {
        CollisionShapeType type;
        std::variant<BoxShapeParams, SphereShapeParams, CapsuleShapeParams> params;
        glm::vec3 localPosition{0.0f};
        glm::vec3 localRotation{0.0f};  // Euler angles in degrees
    };
    std::vector<SubShape> shapes;
};

/**
 * @brief Complete collision configuration
 */
struct CollisionConfig {
    CollisionShapeType type = CollisionShapeType::None;
    std::variant<BoxShapeParams, SphereShapeParams, CapsuleShapeParams,
                 MeshShapeParams, CompoundShapeParams> params;

    // Physics properties
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.0f;
    bool isStatic = false;
    bool isTrigger = false;

    // Collision filtering
    uint32_t collisionGroup = 1;
    uint32_t collisionMask = 0xFFFFFFFF;
};

// ============================================================================
// Material Configuration
// ============================================================================

/**
 * @brief Material/texture configuration for entities
 */
struct MaterialConfig {
    std::string diffusePath;
    std::string normalPath;
    std::string specularPath;
    std::string emissivePath;

    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissiveStrength = 0.0f;

    glm::vec2 uvScale{1.0f};
    glm::vec2 uvOffset{0.0f};

    bool transparent = false;
    bool doubleSided = false;
};

// ============================================================================
// Event Handler Configuration
// ============================================================================

/**
 * @brief Script event handler binding
 */
struct EventHandler {
    std::string eventName;
    std::string scriptPath;        // Path to Python script
    std::string functionName;      // Function to call in script
    bool async = false;            // Run asynchronously
    int priority = 0;              // Execution order (higher = first)
};

// ============================================================================
// Property Bag
// ============================================================================

/**
 * @brief Dynamic property value type
 */
using PropertyValue = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    std::vector<std::string>,
    std::vector<double>
>;

/**
 * @brief Custom properties container
 */
class PropertyBag {
public:
    void Set(const std::string& key, PropertyValue value) {
        m_properties[key] = std::move(value);
    }

    template<typename T>
    std::optional<T> Get(const std::string& key) const {
        auto it = m_properties.find(key);
        if (it != m_properties.end()) {
            if (auto* val = std::get_if<T>(&it->second)) {
                return *val;
            }
        }
        return std::nullopt;
    }

    template<typename T>
    T GetOr(const std::string& key, T defaultValue) const {
        auto result = Get<T>(key);
        return result.value_or(defaultValue);
    }

    bool Has(const std::string& key) const {
        return m_properties.find(key) != m_properties.end();
    }

    void Remove(const std::string& key) {
        m_properties.erase(key);
    }

    void Clear() {
        m_properties.clear();
    }

    const std::unordered_map<std::string, PropertyValue>& GetAll() const {
        return m_properties;
    }

private:
    std::unordered_map<std::string, PropertyValue> m_properties;
};

// ============================================================================
// Base Entity Configuration
// ============================================================================

/**
 * @brief Base configuration for all entity types
 *
 * Supports:
 * - Model and texture paths
 * - Material configuration
 * - Physics collision shapes
 * - Event handlers mapped to Python scripts
 * - Custom properties
 * - Inheritance from base configs
 */
class EntityConfig {
public:
    EntityConfig() = default;
    virtual ~EntityConfig() = default;

    // =========================================================================
    // Loading and Serialization
    // =========================================================================

    /**
     * @brief Load configuration from JSON file
     * @param filePath Path to the JSON config file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Load configuration from JSON string
     * @param jsonString JSON content as string
     * @return true if parsed successfully
     */
    bool LoadFromString(const std::string& jsonString);

    /**
     * @brief Save configuration to JSON file
     * @param filePath Output file path
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Serialize configuration to JSON string
     * @return JSON string representation
     */
    std::string ToJsonString() const;

    /**
     * @brief Validate configuration against schema
     * @return Validation result with errors/warnings
     */
    virtual ValidationResult Validate() const;

    // =========================================================================
    // Identity
    // =========================================================================

    /** @brief Get unique config ID */
    [[nodiscard]] const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    /** @brief Get display name */
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    /** @brief Get description */
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    /** @brief Get config type (e.g., "unit", "building", "tile") */
    [[nodiscard]] virtual std::string GetConfigType() const { return "entity"; }

    /** @brief Get tags for filtering/querying */
    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }
    void AddTag(const std::string& tag) { m_tags.push_back(tag); }
    bool HasTag(const std::string& tag) const;

    // =========================================================================
    // Inheritance
    // =========================================================================

    /** @brief Get base config ID this extends */
    [[nodiscard]] const std::string& GetBaseConfigId() const { return m_baseConfigId; }
    void SetBaseConfigId(const std::string& id) { m_baseConfigId = id; }

    /** @brief Check if this config extends another */
    [[nodiscard]] bool HasBaseConfig() const { return !m_baseConfigId.empty(); }

    /**
     * @brief Apply base config values (called during loading)
     * @param baseConfig The base configuration to inherit from
     */
    virtual void ApplyBaseConfig(const EntityConfig& baseConfig);

    // =========================================================================
    // Model and Rendering
    // =========================================================================

    /** @brief Get model path */
    [[nodiscard]] const std::string& GetModelPath() const { return m_modelPath; }
    void SetModelPath(const std::string& path) { m_modelPath = path; }

    /** @brief Get model scale */
    [[nodiscard]] const glm::vec3& GetModelScale() const { return m_modelScale; }
    void SetModelScale(const glm::vec3& scale) { m_modelScale = scale; }

    /** @brief Get model rotation offset (degrees) */
    [[nodiscard]] const glm::vec3& GetModelRotation() const { return m_modelRotation; }
    void SetModelRotation(const glm::vec3& rotation) { m_modelRotation = rotation; }

    /** @brief Get model position offset */
    [[nodiscard]] const glm::vec3& GetModelOffset() const { return m_modelOffset; }
    void SetModelOffset(const glm::vec3& offset) { m_modelOffset = offset; }

    // =========================================================================
    // Textures and Materials
    // =========================================================================

    /** @brief Get primary texture path */
    [[nodiscard]] const std::string& GetTexturePath() const { return m_texturePath; }
    void SetTexturePath(const std::string& path) { m_texturePath = path; }

    /** @brief Get material configuration */
    [[nodiscard]] const MaterialConfig& GetMaterial() const { return m_material; }
    void SetMaterial(const MaterialConfig& material) { m_material = material; }

    /** @brief Get named texture paths */
    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetTextures() const {
        return m_textures;
    }
    void SetTexture(const std::string& name, const std::string& path) {
        m_textures[name] = path;
    }
    std::string GetTexture(const std::string& name) const;

    // =========================================================================
    // Physics and Collision
    // =========================================================================

    /** @brief Get collision configuration */
    [[nodiscard]] const CollisionConfig& GetCollision() const { return m_collision; }
    void SetCollision(const CollisionConfig& collision) { m_collision = collision; }

    // =========================================================================
    // Event Handlers
    // =========================================================================

    /** @brief Get all event handlers */
    [[nodiscard]] const std::vector<EventHandler>& GetEventHandlers() const {
        return m_eventHandlers;
    }

    /** @brief Add an event handler */
    void AddEventHandler(const EventHandler& handler) {
        m_eventHandlers.push_back(handler);
    }

    /** @brief Get handlers for a specific event */
    std::vector<EventHandler> GetHandlersForEvent(const std::string& eventName) const;

    /** @brief Check if event has any handlers */
    bool HasEventHandler(const std::string& eventName) const;

    // =========================================================================
    // Custom Properties
    // =========================================================================

    /** @brief Get property bag */
    [[nodiscard]] PropertyBag& GetProperties() { return m_properties; }
    [[nodiscard]] const PropertyBag& GetProperties() const { return m_properties; }

    // =========================================================================
    // Source File Info
    // =========================================================================

    /** @brief Get source file path */
    [[nodiscard]] const std::string& GetSourcePath() const { return m_sourcePath; }

    /** @brief Get last modification time */
    [[nodiscard]] int64_t GetLastModified() const { return m_lastModified; }

protected:
    // Parse helpers for derived classes
    virtual void ParseTypeSpecificFields(const std::string& jsonContent);
    virtual std::string SerializeTypeSpecificFields() const;

    // Strip comments from JSON (JSON5 support)
    static std::string StripComments(const std::string& json);

    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::vector<std::string> m_tags;

    // Inheritance
    std::string m_baseConfigId;

    // Model
    std::string m_modelPath;
    glm::vec3 m_modelScale{1.0f};
    glm::vec3 m_modelRotation{0.0f};
    glm::vec3 m_modelOffset{0.0f};

    // Textures/Materials
    std::string m_texturePath;
    MaterialConfig m_material;
    std::unordered_map<std::string, std::string> m_textures;

    // Physics
    CollisionConfig m_collision;

    // Events
    std::vector<EventHandler> m_eventHandlers;

    // Custom properties
    PropertyBag m_properties;

    // Source info
    std::string m_sourcePath;
    int64_t m_lastModified = 0;
};

// ============================================================================
// Config Factory
// ============================================================================

/**
 * @brief Factory for creating config objects by type
 */
class EntityConfigFactory {
public:
    using ConfigCreator = std::function<std::unique_ptr<EntityConfig>()>;

    static EntityConfigFactory& Instance();

    void RegisterType(const std::string& typeName, ConfigCreator creator);

    std::unique_ptr<EntityConfig> Create(const std::string& typeName) const;

    bool HasType(const std::string& typeName) const;

    std::vector<std::string> GetRegisteredTypes() const;

private:
    EntityConfigFactory() = default;
    std::unordered_map<std::string, ConfigCreator> m_creators;
};

// Registration macro for config types
#define REGISTER_CONFIG_TYPE(TypeName, ConfigClass) \
    namespace { \
        struct ConfigClass##Registrar { \
            ConfigClass##Registrar() { \
                EntityConfigFactory::Instance().RegisterType(TypeName, \
                    []() -> std::unique_ptr<EntityConfig> { \
                        return std::make_unique<ConfigClass>(); \
                    }); \
            } \
        }; \
        static ConfigClass##Registrar g_##ConfigClass##Registrar; \
    }

} // namespace Config
} // namespace Vehement
