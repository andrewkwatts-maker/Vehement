#pragma once

#include "AssetConfig.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <variant>

namespace Engine {

/**
 * @brief Result type for generic asset loading
 * Uses std::variant to hold any asset configuration type
 */
using AssetConfigVariant = std::variant<
    AssetConfig,
    TextureConfig,
    MaterialConfig,
    SDFModelConfig,
    SkeletonConfig,
    AnimationConfig,
    AnimationSetConfig,
    EffectConfig,
    AbilityConfig,
    BehaviorConfig,
    EntityConfig,
    UnitConfig,
    HeroConfig,
    BuildingConfig,
    ResourceNodeConfig,
    ProjectileConfig,
    DecorationConfig
>;

/**
 * @brief Loader class for asset configurations from JSON
 *
 * This loader supports the unified AssetConfig system and can load
 * any asset type based on the "type" field in the JSON file.
 */
class SDFConfigLoader {
public:
    SDFConfigLoader() = default;
    ~SDFConfigLoader() = default;

    // =========================================================================
    // Generic Asset Loading
    // =========================================================================

    /**
     * @brief Load any asset configuration from a JSON file
     * @param filepath Path to the JSON file
     * @return Variant containing the appropriate config type
     * @throws std::runtime_error if loading fails
     */
    AssetConfigVariant LoadAssetFromFile(const std::filesystem::path& filepath);

    /**
     * @brief Load any asset configuration from a JSON string
     * @param jsonString JSON string to parse
     * @return Variant containing the appropriate config type
     * @throws std::runtime_error if parsing fails
     */
    AssetConfigVariant LoadAssetFromString(const std::string& jsonString);

    /**
     * @brief Load all asset configurations from a directory
     * @param directory Path to the directory containing JSON files
     * @param recursive Whether to search recursively
     * @return Map of asset ID to configuration variant
     */
    std::unordered_map<std::string, AssetConfigVariant> LoadAssetsFromDirectory(
        const std::filesystem::path& directory,
        bool recursive = true);

    // =========================================================================
    // Legacy Entity Loading (for backward compatibility)
    // =========================================================================

    /**
     * @brief Load an entity configuration from a JSON file
     * @param filepath Path to the JSON file
     * @return The loaded entity configuration
     * @throws std::runtime_error if loading fails
     */
    EntityConfig LoadFromFile(const std::filesystem::path& filepath);

    /**
     * @brief Load an entity configuration from a JSON string
     * @param jsonString JSON string to parse
     * @return The loaded entity configuration
     * @throws std::runtime_error if parsing fails
     */
    EntityConfig LoadFromString(const std::string& jsonString);

    /**
     * @brief Load all entity configurations from a directory
     * @param directory Path to the directory containing JSON files
     * @param recursive Whether to search recursively
     * @return Map of entity ID to configuration
     */
    std::unordered_map<std::string, EntityConfig> LoadFromDirectory(
        const std::filesystem::path& directory,
        bool recursive = true);

    // =========================================================================
    // Type-Specific Loading
    // =========================================================================

    /**
     * @brief Load an SDF model configuration from JSON
     */
    SDFModelConfig LoadSDFModel(const nlohmann::json& json);

    /**
     * @brief Load a skeleton configuration from JSON
     */
    SkeletonConfig LoadSkeleton(const nlohmann::json& json);

    /**
     * @brief Load an animation configuration from JSON
     */
    AnimationConfig LoadAnimation(const nlohmann::json& json);

    /**
     * @brief Load an animation set configuration from JSON
     */
    AnimationSetConfig LoadAnimationSet(const nlohmann::json& json);

    /**
     * @brief Load an ability configuration from JSON
     */
    AbilityConfig LoadAbility(const nlohmann::json& json);

    /**
     * @brief Load a behavior configuration from JSON
     */
    BehaviorConfig LoadBehavior(const nlohmann::json& json);

    /**
     * @brief Load an effect configuration from JSON
     */
    EffectConfig LoadEffect(const nlohmann::json& json);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate an entity configuration
     * @param config The configuration to validate
     * @return List of validation errors (empty if valid)
     */
    std::vector<std::string> Validate(const EntityConfig& config);

    /**
     * @brief Validate an SDF model configuration
     * @param config The configuration to validate
     * @param skeleton Optional skeleton for bone reference validation
     * @return List of validation errors (empty if valid)
     */
    std::vector<std::string> ValidateSDFModel(
        const SDFModelConfig& config,
        const SkeletonConfig* skeleton = nullptr);

    /**
     * @brief Get the last error message
     */
    const std::string& GetLastError() const { return m_lastError; }

private:
    // =========================================================================
    // Core Parsing Helpers
    // =========================================================================

    // Base asset parsing
    void ParseBaseAsset(const nlohmann::json& json, AssetConfig& config);

    // Entity parsing
    EntityConfig ParseEntity(const nlohmann::json& json);
    UnitConfig ParseUnit(const nlohmann::json& json);
    HeroConfig ParseHero(const nlohmann::json& json);
    BuildingConfig ParseBuilding(const nlohmann::json& json);
    ResourceNodeConfig ParseResourceNode(const nlohmann::json& json);
    ProjectileConfig ParseProjectile(const nlohmann::json& json);
    DecorationConfig ParseDecoration(const nlohmann::json& json);

    // Component parsing
    StatsConfig ParseStats(const nlohmann::json& json);
    CostConfig ParseCosts(const nlohmann::json& json);
    SDFModelConfig ParseSDFModel(const nlohmann::json& json);
    SDFPrimitiveConfig ParsePrimitive(const nlohmann::json& json);
    SkeletonConfig ParseSkeleton(const nlohmann::json& json);
    BoneConfig ParseBone(const nlohmann::json& json);
    AnimationConfig ParseAnimation(const std::string& name, const nlohmann::json& json);
    KeyframeConfig ParseKeyframe(const nlohmann::json& json);
    AnimationStateMachineConfig ParseAnimationStateMachine(const nlohmann::json& json);
    AnimationSetConfig ParseAnimationSet(const nlohmann::json& json);
    AbilityConfig ParseAbility(const nlohmann::json& json);
    BehaviorConfig ParseBehaviors(const nlohmann::json& json);
    BehaviorTriggerConfig ParseBehaviorTrigger(const nlohmann::json& json);
    EffectConfig ParseEffect(const std::string& name, const nlohmann::json& json);

    // Resource parsing
    TextureConfig ParseTexture(const nlohmann::json& json);
    MaterialConfig ParseMaterial(const nlohmann::json& json);

    // =========================================================================
    // Utility Helpers
    // =========================================================================

    glm::vec2 ParseVec2(const nlohmann::json& json, const glm::vec2& defaultValue = glm::vec2(0.0f));
    glm::vec3 ParseVec3(const nlohmann::json& json, const glm::vec3& defaultValue = glm::vec3(0.0f));
    glm::vec4 ParseVec4(const nlohmann::json& json, const glm::vec4& defaultValue = glm::vec4(1.0f));
    glm::quat ParseQuat(const nlohmann::json& json, const glm::quat& defaultValue = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    std::string m_lastError;
};

} // namespace Engine
