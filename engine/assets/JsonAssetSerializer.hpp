#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Material;
class Texture;
class Mesh;
class Animation;

/**
 * @brief Asset type enumeration
 */
enum class AssetType {
    Unknown,
    Material,
    Texture,
    Mesh,
    Model,
    Animation,
    Shader,
    Audio,
    Particles,
    Physics,
    VisualScript,
    Light,
    Prefab
};

/**
 * @brief Asset version information
 */
struct AssetVersion {
    int major = 1;
    int minor = 0;
    int patch = 0;

    std::string ToString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    static AssetVersion FromString(const std::string& str);
    bool IsCompatible(const AssetVersion& other) const;
};

/**
 * @brief Asset metadata
 */
struct AssetMetadata {
    AssetType type = AssetType::Unknown;
    AssetVersion version;
    std::string name;
    std::string uuid;
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> dependencies;
    std::string author;
    std::string createdDate;
    std::string modifiedDate;
    std::unordered_map<std::string, std::string> customProperties;

    nlohmann::json ToJson() const;
    static AssetMetadata FromJson(const nlohmann::json& json);
};

/**
 * @brief Asset schema validation result
 */
struct ValidationResult {
    bool isValid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void AddError(const std::string& error) {
        errors.push_back(error);
        isValid = false;
    }

    void AddWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

/**
 * @brief Generic asset container
 */
struct JsonAsset {
    AssetMetadata metadata;
    nlohmann::json data;
    std::string sourceFile;

    bool IsValid() const;
};

/**
 * @brief Asset migration interface
 */
class IAssetMigration {
public:
    virtual ~IAssetMigration() = default;

    virtual AssetVersion GetFromVersion() const = 0;
    virtual AssetVersion GetToVersion() const = 0;
    virtual bool Migrate(nlohmann::json& data) = 0;
    virtual std::string GetDescription() const = 0;
};

/**
 * @brief Universal JSON asset serialization system
 *
 * Features:
 * - Generic asset serialization/deserialization
 * - Schema validation with error reporting
 * - Asset versioning and migration
 * - Hot-reloading support
 * - Asset dependency tracking
 * - AI-friendly JSON format
 */
class JsonAssetSerializer {
public:
    JsonAssetSerializer();
    ~JsonAssetSerializer();

    /**
     * @brief Load asset from JSON file
     */
    std::shared_ptr<JsonAsset> LoadFromFile(const std::string& filePath);

    /**
     * @brief Load asset from JSON
     */
    std::shared_ptr<JsonAsset> LoadFromJson(const nlohmann::json& json);

    /**
     * @brief Save asset to JSON file
     */
    bool SaveToFile(const JsonAsset& asset, const std::string& filePath);

    /**
     * @brief Save asset to JSON
     */
    nlohmann::json SaveToJson(const JsonAsset& asset);

    /**
     * @brief Validate asset against schema
     */
    ValidationResult Validate(const JsonAsset& asset) const;

    /**
     * @brief Validate JSON against schema
     */
    ValidationResult ValidateJson(const nlohmann::json& json, AssetType type) const;

    /**
     * @brief Register asset type
     */
    void RegisterAssetType(AssetType type, const std::string& typeName);

    /**
     * @brief Register schema for asset type
     */
    void RegisterSchema(AssetType type, const nlohmann::json& schema);

    /**
     * @brief Register migration
     */
    void RegisterMigration(AssetType type, std::shared_ptr<IAssetMigration> migration);

    /**
     * @brief Migrate asset to latest version
     */
    bool MigrateToLatest(JsonAsset& asset);

    /**
     * @brief Get asset type from JSON
     */
    AssetType GetAssetType(const nlohmann::json& json) const;

    /**
     * @brief Get asset type from string
     */
    AssetType GetAssetTypeFromString(const std::string& typeStr) const;

    /**
     * @brief Get asset type string
     */
    std::string GetAssetTypeString(AssetType type) const;

    /**
     * @brief Strip comments from JSON file
     */
    static std::string StripComments(const std::string& jsonWithComments);

    /**
     * @brief Generate UUID for asset
     */
    static std::string GenerateUUID();

    /**
     * @brief Extract dependencies from asset
     */
    std::vector<std::string> ExtractDependencies(const JsonAsset& asset) const;

    /**
     * @brief Resolve asset path
     */
    std::string ResolveAssetPath(const std::string& path, const std::string& basePath) const;

    /**
     * @brief Enable/disable schema validation
     */
    void SetValidationEnabled(bool enabled) { m_validationEnabled = enabled; }
    bool IsValidationEnabled() const { return m_validationEnabled; }

    /**
     * @brief Enable/disable automatic migration
     */
    void SetAutoMigrationEnabled(bool enabled) { m_autoMigrationEnabled = enabled; }
    bool IsAutoMigrationEnabled() const { return m_autoMigrationEnabled; }

private:
    std::unordered_map<AssetType, std::string> m_typeNames;
    std::unordered_map<std::string, AssetType> m_stringToType;
    std::unordered_map<AssetType, nlohmann::json> m_schemas;
    std::unordered_map<AssetType, std::vector<std::shared_ptr<IAssetMigration>>> m_migrations;

    bool m_validationEnabled = true;
    bool m_autoMigrationEnabled = true;

    /**
     * @brief Validate against JSON schema
     */
    ValidationResult ValidateAgainstSchema(const nlohmann::json& json,
                                          const nlohmann::json& schema) const;

    /**
     * @brief Find migration path
     */
    std::vector<std::shared_ptr<IAssetMigration>> FindMigrationPath(
        AssetType type, const AssetVersion& from, const AssetVersion& to) const;
};

/**
 * @brief Material asset serializer
 */
namespace MaterialSerializer {
    nlohmann::json Serialize(const Material& material);
    std::shared_ptr<Material> Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Light asset serializer
 */
namespace LightSerializer {
    struct LightData {
        std::string type; // "directional", "point", "spot", "area"
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;
        float temperature = 6500.0f; // Kelvin
        float radius = 10.0f;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
        float spotAngle = 45.0f;
        bool castsShadows = true;
        std::string iesProfile;
        std::string materialFunction; // For flickering, etc.
    };

    nlohmann::json Serialize(const LightData& light);
    LightData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Model asset serializer
 */
namespace ModelSerializer {
    struct ModelData {
        std::string meshPath;
        std::vector<std::string> materialPaths;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        std::string physicsMaterial;
        std::vector<std::string> animations;
    };

    nlohmann::json Serialize(const ModelData& model);
    ModelData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Animation asset serializer
 */
namespace AnimationSerializer {
    struct AnimationData {
        std::string name;
        std::string clipPath;
        float duration = 1.0f;
        bool looping = false;
        float speed = 1.0f;
        std::vector<std::string> events;
    };

    nlohmann::json Serialize(const AnimationData& animation);
    AnimationData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Visual script asset serializer
 */
namespace VisualScriptSerializer {
    nlohmann::json Serialize(const nlohmann::json& script);
    nlohmann::json Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Shader asset serializer
 */
namespace ShaderSerializer {
    struct ShaderData {
        std::string vertexSource;
        std::string fragmentSource;
        std::string geometrySource;
        std::string computeSource;
        std::unordered_map<std::string, std::string> parameters;
        std::vector<std::string> defines;
    };

    nlohmann::json Serialize(const ShaderData& shader);
    ShaderData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Audio asset serializer
 */
namespace AudioSerializer {
    struct AudioData {
        std::string audioPath;
        float volume = 1.0f;
        float pitch = 1.0f;
        bool looping = false;
        bool spatial = false;
        float minDistance = 1.0f;
        float maxDistance = 100.0f;
        float rolloffFactor = 1.0f;
    };

    nlohmann::json Serialize(const AudioData& audio);
    AudioData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Particle system asset serializer
 */
namespace ParticleSerializer {
    struct ParticleData {
        std::string emitterShape; // "point", "box", "sphere", "cone"
        glm::vec3 emitterSize = glm::vec3(1.0f);
        int maxParticles = 1000;
        float emissionRate = 100.0f;
        float lifetime = 1.0f;
        glm::vec3 velocity = glm::vec3(0.0f, 1.0f, 0.0f);
        float velocityVariation = 0.1f;
        glm::vec4 startColor = glm::vec4(1.0f);
        glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        float startSize = 1.0f;
        float endSize = 0.0f;
        std::string material;
    };

    nlohmann::json Serialize(const ParticleData& particles);
    ParticleData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Physics asset serializer
 */
namespace PhysicsSerializer {
    struct PhysicsData {
        std::string collisionShape; // "box", "sphere", "capsule", "mesh"
        glm::vec3 shapeSize = glm::vec3(1.0f);
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
        bool isKinematic = false;
        bool isTrigger = false;
    };

    nlohmann::json Serialize(const PhysicsData& physics);
    PhysicsData Deserialize(const nlohmann::json& json);
    ValidationResult Validate(const nlohmann::json& json);
    nlohmann::json GetSchema();
}

/**
 * @brief Helper functions for JSON serialization
 */
namespace JsonHelpers {
    // GLM types
    nlohmann::json SerializeVec2(const glm::vec2& v);
    nlohmann::json SerializeVec3(const glm::vec3& v);
    nlohmann::json SerializeVec4(const glm::vec4& v);
    nlohmann::json SerializeMat4(const glm::mat4& m);

    glm::vec2 DeserializeVec2(const nlohmann::json& json);
    glm::vec3 DeserializeVec3(const nlohmann::json& json);
    glm::vec4 DeserializeVec4(const nlohmann::json& json);
    glm::mat4 DeserializeMat4(const nlohmann::json& json);

    // Validation helpers
    bool HasField(const nlohmann::json& json, const std::string& field);
    bool HasFieldOfType(const nlohmann::json& json, const std::string& field, nlohmann::json::value_t type);
    bool IsInRange(float value, float min, float max);
}

} // namespace Nova
