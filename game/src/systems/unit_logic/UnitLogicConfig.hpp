#pragma once

#include "UnitAnimationController.hpp"
#include "UnitEventHandler.hpp"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {

using Nova::json;

/**
 * @brief Animation mapping entry
 */
struct AnimationMapping {
    std::string id;                 // Logical animation name (e.g., "idle", "walk")
    std::string clipPath;           // Actual clip path
    float speed = 1.0f;
    bool loop = true;
    bool mirror = false;
    std::vector<std::string> variants;  // Alternative clips for variation

    [[nodiscard]] json ToJson() const;
    static AnimationMapping FromJson(const json& j);
};

/**
 * @brief Sound mapping entry
 */
struct SoundMapping {
    std::string id;
    std::string soundPath;
    float volume = 1.0f;
    float pitchMin = 1.0f;
    float pitchMax = 1.0f;
    std::vector<std::string> variants;

    [[nodiscard]] json ToJson() const;
    static SoundMapping FromJson(const json& j);
};

/**
 * @brief VFX mapping entry
 */
struct VFXMapping {
    std::string id;
    std::string vfxPath;
    std::string attachBone;
    glm::vec3 offset{0.0f};
    float scale = 1.0f;
    bool attachToUnit = true;

    [[nodiscard]] json ToJson() const;
    static VFXMapping FromJson(const json& j);
};

/**
 * @brief Complete unit logic configuration
 *
 * Defines all animation, event, and behavior configuration for a unit type.
 * Can be loaded from JSON and hot-reloaded at runtime.
 */
struct UnitLogicConfig {
    // Identification
    std::string id;
    std::string name;
    std::string type;               // "humanoid", "creature", "vehicle"
    std::string basedOn;            // Config to inherit from

    // State machine configuration
    std::string stateMachineConfig;
    std::string locomotionBlendTreeConfig;
    std::string combatStateMachineConfig;
    std::string abilityStateMachineConfig;

    // Animation mappings
    std::vector<AnimationMapping> animationMappings;

    // Sound mappings
    std::vector<SoundMapping> soundMappings;

    // VFX mappings
    std::vector<VFXMapping> vfxMappings;

    // Event bindings config path
    std::string eventBindingsConfig;

    // Blend masks
    struct {
        std::string upperBody;
        std::string lowerBody;
        std::string fullBody;
        std::string headOnly;
        std::string handsOnly;
    } masks;

    // Timing settings
    struct {
        float locomotionBlendSpeed = 5.0f;
        float combatBlendSpeed = 8.0f;
        float transitionBlendTime = 0.2f;
        float hitReactionDuration = 0.3f;
        float stunRecoveryTime = 0.5f;
    } timing;

    // Flags
    struct {
        bool useRootMotion = false;
        bool useFootIK = false;
        bool useLookAt = false;
        bool useLayeredAnimation = true;
    } features;

    [[nodiscard]] json ToJson() const;
    static UnitLogicConfig FromJson(const json& j);
};

/**
 * @brief Manager for unit logic configurations
 */
class UnitLogicConfigManager {
public:
    UnitLogicConfigManager() = default;

    /**
     * @brief Load configuration from file
     */
    UnitLogicConfig* Load(const std::string& filepath);

    /**
     * @brief Load all configs from directory
     */
    void LoadDirectory(const std::string& directory, bool recursive = true);

    /**
     * @brief Get config by ID
     */
    [[nodiscard]] UnitLogicConfig* Get(const std::string& id);
    [[nodiscard]] const UnitLogicConfig* Get(const std::string& id) const;

    /**
     * @brief Create config from JSON
     */
    UnitLogicConfig* CreateFromJson(const std::string& id, const json& config);

    /**
     * @brief Remove config
     */
    bool Remove(const std::string& id);

    /**
     * @brief Clear all configs
     */
    void Clear();

    /**
     * @brief Get all config IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllIds() const;

    /**
     * @brief Apply inheritance for all configs
     */
    void ApplyInheritance();

    /**
     * @brief Reload all configs
     */
    void ReloadAll();

    /**
     * @brief Create animation controller from config
     */
    [[nodiscard]] std::unique_ptr<UnitAnimationController> CreateAnimationController(
        const std::string& configId) const;

    /**
     * @brief Create event handler from config
     */
    [[nodiscard]] std::unique_ptr<UnitEventHandler> CreateEventHandler(
        const std::string& configId,
        Nova::AnimationEventSystem* eventSystem) const;

private:
    void ApplyInheritance(UnitLogicConfig& config);
    void MergeConfig(UnitLogicConfig& target, const UnitLogicConfig& base);

    std::unordered_map<std::string, std::unique_ptr<UnitLogicConfig>> m_configs;
    std::unordered_map<std::string, std::string> m_pathToId;
};

/**
 * @brief Builder for creating unit logic configs programmatically
 */
class UnitLogicConfigBuilder {
public:
    UnitLogicConfigBuilder() = default;

    UnitLogicConfigBuilder& SetId(const std::string& id);
    UnitLogicConfigBuilder& SetName(const std::string& name);
    UnitLogicConfigBuilder& SetType(const std::string& type);
    UnitLogicConfigBuilder& SetBasedOn(const std::string& basedOn);

    UnitLogicConfigBuilder& SetStateMachine(const std::string& path);
    UnitLogicConfigBuilder& SetLocomotionBlendTree(const std::string& path);
    UnitLogicConfigBuilder& SetCombatStateMachine(const std::string& path);

    UnitLogicConfigBuilder& AddAnimation(const std::string& id, const std::string& clipPath,
                                          float speed = 1.0f, bool loop = true);
    UnitLogicConfigBuilder& AddSound(const std::string& id, const std::string& soundPath,
                                      float volume = 1.0f);
    UnitLogicConfigBuilder& AddVFX(const std::string& id, const std::string& vfxPath,
                                    const std::string& bone = "");

    UnitLogicConfigBuilder& SetMasks(const std::string& upperBody, const std::string& lowerBody,
                                      const std::string& fullBody);
    UnitLogicConfigBuilder& SetTiming(float locomotionBlendSpeed, float combatBlendSpeed,
                                       float transitionTime);
    UnitLogicConfigBuilder& EnableRootMotion(bool enable);
    UnitLogicConfigBuilder& EnableFootIK(bool enable);
    UnitLogicConfigBuilder& EnableLookAt(bool enable);

    [[nodiscard]] UnitLogicConfig Build() const;
    [[nodiscard]] json ToJson() const;

private:
    UnitLogicConfig m_config;
};

} // namespace Vehement
