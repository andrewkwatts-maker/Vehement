#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

/**
 * @brief Asset type enumeration
 * All game data derives from Asset - the base type for all JSON-defined content
 */
enum class AssetType {
    Asset,          // Base type - generic asset
    Texture,        // Texture resource
    Material,       // Material definition
    SDFModel,       // SDF model definition
    Skeleton,       // Skeleton/bone hierarchy
    Animation,      // Single animation
    AnimationSet,   // Collection of animations
    Effect,         // Visual/audio effect
    Sound,          // Sound/audio asset
    Entity,         // Placeable world object (base)
    Unit,           // Combat unit (Entity specialization)
    Hero,           // Hero unit (Unit specialization)
    Building,       // Structure (Entity specialization)
    ResourceNode,   // Harvestable resource (Entity specialization)
    Projectile,     // Projectile (Entity specialization)
    Decoration,     // Non-interactive decoration (Entity specialization)
    Ability,        // Ability definition
    Behavior,       // AI behavior definition
    Race,           // Race/faction definition
    TechTree,       // Technology tree
    Upgrade,        // Research upgrade
    Campaign,       // Campaign definition
    Mission,        // Mission/scenario definition
    Map,            // Map/level definition
    UI              // UI element definition
};

/**
 * @brief Convert AssetType to string
 */
inline std::string AssetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::Asset: return "asset";
        case AssetType::Texture: return "texture";
        case AssetType::Material: return "material";
        case AssetType::SDFModel: return "sdf_model";
        case AssetType::Skeleton: return "skeleton";
        case AssetType::Animation: return "animation";
        case AssetType::AnimationSet: return "animation_set";
        case AssetType::Effect: return "effect";
        case AssetType::Sound: return "sound";
        case AssetType::Entity: return "entity";
        case AssetType::Unit: return "unit";
        case AssetType::Hero: return "hero";
        case AssetType::Building: return "building";
        case AssetType::ResourceNode: return "resource_node";
        case AssetType::Projectile: return "projectile";
        case AssetType::Decoration: return "decoration";
        case AssetType::Ability: return "ability";
        case AssetType::Behavior: return "behavior";
        case AssetType::Race: return "race";
        case AssetType::TechTree: return "tech_tree";
        case AssetType::Upgrade: return "upgrade";
        case AssetType::Campaign: return "campaign";
        case AssetType::Mission: return "mission";
        case AssetType::Map: return "map";
        case AssetType::UI: return "ui";
        default: return "unknown";
    }
}

/**
 * @brief Parse AssetType from string
 */
inline AssetType StringToAssetType(const std::string& str) {
    static const std::unordered_map<std::string, AssetType> map = {
        {"asset", AssetType::Asset},
        {"texture", AssetType::Texture},
        {"material", AssetType::Material},
        {"sdf_model", AssetType::SDFModel},
        {"skeleton", AssetType::Skeleton},
        {"animation", AssetType::Animation},
        {"animation_set", AssetType::AnimationSet},
        {"effect", AssetType::Effect},
        {"sound", AssetType::Sound},
        {"entity", AssetType::Entity},
        {"unit", AssetType::Unit},
        {"hero", AssetType::Hero},
        {"building", AssetType::Building},
        {"resource_node", AssetType::ResourceNode},
        {"projectile", AssetType::Projectile},
        {"decoration", AssetType::Decoration},
        {"ability", AssetType::Ability},
        {"behavior", AssetType::Behavior},
        {"race", AssetType::Race},
        {"tech_tree", AssetType::TechTree},
        {"upgrade", AssetType::Upgrade},
        {"campaign", AssetType::Campaign},
        {"mission", AssetType::Mission},
        {"map", AssetType::Map},
        {"ui", AssetType::UI}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : AssetType::Asset;
}

// =============================================================================
// Base Asset Config - All configs inherit from this
// =============================================================================

/**
 * @brief Base configuration for all assets
 * This is the root of the config hierarchy - everything is an Asset
 */
struct AssetConfig {
    std::string id;                 // Unique identifier (e.g., "humans.units.footman")
    std::string name;               // Display name
    std::string description;        // Description text
    AssetType type = AssetType::Asset;
    std::vector<std::string> tags;  // Searchable tags
    nlohmann::json metadata;        // Additional arbitrary data
};

// =============================================================================
// Resource Configs (non-placeable)
// =============================================================================

/**
 * @brief Texture configuration
 */
struct TextureConfig : AssetConfig {
    std::string path;               // File path to texture
    std::string format;             // png, jpg, dds, etc.
    bool generateMipmaps = true;
    bool sRGB = true;
};

/**
 * @brief Material configuration
 */
struct MaterialConfig : AssetConfig {
    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emissive{0.0f};
    std::string albedoTexture;      // Reference to texture ID
    std::string normalTexture;
    std::string metallicTexture;
    std::string roughnessTexture;
    std::string aoTexture;
    std::string emissiveTexture;
};

// =============================================================================
// SDF Model Config (separate modular file)
// =============================================================================

/**
 * @brief SDF primitive configuration
 */
struct SDFPrimitiveConfig {
    std::string id;
    std::string type;               // Sphere, Box, Cylinder, etc.
    nlohmann::json params;          // Type-specific parameters

    // Transform
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    // Material (can override or reference)
    std::optional<std::string> materialRef;  // Reference to material ID
    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emissive{0.0f};

    // CSG operation
    std::string operation = "Union";
    float smoothness = 0.0f;

    // Bone attachment (optional)
    std::string bone;
};

/**
 * @brief SDF Model configuration (can be in separate file)
 */
struct SDFModelConfig : AssetConfig {
    glm::vec3 boundsMin{-1.0f};
    glm::vec3 boundsMax{1.0f};
    std::vector<SDFPrimitiveConfig> primitives;

    // LOD variants (optional)
    std::vector<std::string> lodModels;  // References to lower detail models
};

// =============================================================================
// Skeleton Config (separate modular file)
// =============================================================================

/**
 * @brief Single bone configuration
 */
struct BoneConfig {
    std::string name;
    std::string parent;             // Empty or "null" for root
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

/**
 * @brief Skeleton configuration (can be in separate file)
 */
struct SkeletonConfig : AssetConfig {
    std::vector<BoneConfig> bones;
};

// =============================================================================
// Animation Config (separate modular file)
// =============================================================================

/**
 * @brief Animation keyframe
 */
struct KeyframeConfig {
    float time = 0.0f;
    std::unordered_map<std::string, nlohmann::json> boneTransforms;
    std::vector<std::string> events;
    std::optional<float> constructionProgress;
};

/**
 * @brief Single animation configuration (can be in separate file)
 */
struct AnimationConfig : AssetConfig {
    float duration = 1.0f;
    bool loop = false;
    std::vector<KeyframeConfig> keyframes;
    std::string skeletonRef;        // Reference to skeleton this animation is for
};

/**
 * @brief Animation state transition
 */
struct StateTransitionConfig {
    std::string to;
    std::string condition;
    float blendTime = 0.2f;
};

/**
 * @brief Animation state
 */
struct AnimationStateConfig {
    std::string animationRef;       // Reference to animation ID
    std::vector<StateTransitionConfig> transitions;
    float playbackSpeed = 1.0f;
};

/**
 * @brief Animation state machine configuration
 */
struct AnimationStateMachineConfig {
    std::string initialState;
    std::unordered_map<std::string, AnimationStateConfig> states;
};

/**
 * @brief Animation set - collection of animations (can be in separate file)
 */
struct AnimationSetConfig : AssetConfig {
    std::string skeletonRef;        // Reference to skeleton
    std::vector<std::string> animationRefs;  // References to animation IDs
    AnimationStateMachineConfig stateMachine;
};

// =============================================================================
// Effect Config (separate modular file)
// =============================================================================

/**
 * @brief Effect configuration (can be in separate file)
 */
struct EffectConfig : AssetConfig {
    std::string effectType;         // particle, sound, light, etc.
    nlohmann::json params;
    float duration = 0.0f;          // 0 = instant, -1 = looping
    std::string attachBone;         // Optional bone to attach to
    glm::vec3 offset{0.0f};
};

// =============================================================================
// Ability Config (separate modular file)
// =============================================================================

/**
 * @brief Ability configuration (can be in separate file)
 */
struct AbilityConfig : AssetConfig {
    std::string hotkey;
    std::string targetType;         // none, unit, point, unit_or_point
    std::string icon;               // Reference to texture

    float cooldown = 0.0f;
    int manaCost = 0;
    float range = 0.0f;
    float castTime = 0.0f;
    float duration = 0.0f;
    float radius = 0.0f;

    std::vector<std::string> effectRefs;  // Effects to play
    nlohmann::json params;          // Ability-specific parameters
};

// =============================================================================
// Behavior Config (separate modular file)
// =============================================================================

/**
 * @brief Behavior action
 */
struct BehaviorActionConfig {
    std::string type;
    nlohmann::json params;
};

/**
 * @brief Behavior condition
 */
struct BehaviorConditionConfig {
    std::string type;
    nlohmann::json params;
};

/**
 * @brief Behavior trigger
 */
struct BehaviorTriggerConfig {
    std::vector<BehaviorConditionConfig> conditions;
    std::vector<BehaviorActionConfig> actions;
};

/**
 * @brief Behavior configuration (can be in separate file)
 */
struct BehaviorConfig : AssetConfig {
    std::unordered_map<std::string, BehaviorTriggerConfig> triggers;
};

// =============================================================================
// Entity Stats and Costs
// =============================================================================

/**
 * @brief Combat and movement stats
 */
struct StatsConfig {
    int health = 100;
    int maxHealth = 100;
    int mana = 0;
    int maxMana = 0;
    int armor = 0;
    int damage = 10;
    float attackSpeed = 1.0f;
    float moveSpeed = 200.0f;
    float attackRange = 100.0f;
    float healthRegen = 0.0f;
    float manaRegen = 0.0f;
    float sightRange = 800.0f;
    bool flying = false;

    // Building-specific
    int foodProvided = 0;
    float buildTime = 0.0f;

    // Hero-specific
    int level = 1;
    int maxLevel = 10;
    int experience = 0;
    int strength = 0;
    int agility = 0;
    int intelligence = 0;
};

/**
 * @brief Resource costs
 */
struct CostConfig {
    int gold = 0;
    int lumber = 0;
    int food = 0;
    int mana = 0;
    float buildTime = 0.0f;
};

// =============================================================================
// Entity Config (placeable world objects)
// =============================================================================

/**
 * @brief Base entity configuration - placeable objects in the world
 * This is the main config for units, heroes, buildings, etc.
 */
struct EntityConfig : AssetConfig {
    std::string race;               // Faction/race this belongs to
    std::string category;           // Unit category (infantry, cavalry, etc.)

    // Modular references (can point to separate JSON files)
    std::string sdfModelRef;        // Reference to SDF model JSON
    std::string skeletonRef;        // Reference to skeleton JSON
    std::string animationSetRef;    // Reference to animation set JSON
    std::string behaviorRef;        // Reference to behavior JSON

    // Inline data (if not using references)
    std::optional<SDFModelConfig> sdfModel;
    std::optional<SkeletonConfig> skeleton;
    std::optional<AnimationSetConfig> animationSet;
    std::optional<BehaviorConfig> behavior;

    // Stats and costs
    StatsConfig stats;
    CostConfig costs;

    // Requirements to build/train
    std::vector<std::string> requirements;

    // Ability references
    std::vector<std::string> abilityRefs;

    // Inline abilities (if not using references)
    std::vector<AbilityConfig> abilities;

    // Effect references
    std::vector<std::string> effectRefs;
    std::unordered_map<std::string, EffectConfig> effects;

    // Transform defaults
    glm::vec3 spawnOffset{0.0f};
    float collisionRadius = 0.5f;
    float selectionRadius = 1.0f;
};

// =============================================================================
// Entity Type Specializations
// =============================================================================

/**
 * @brief Unit configuration (combat unit)
 */
struct UnitConfig : EntityConfig {
    std::string unitClass;          // melee, ranged, caster, siege
    std::string armorType;          // light, medium, heavy, fortified
    std::string attackType;         // normal, pierce, magic, siege
    int squadSize = 1;              // Number of units in squad
};

/**
 * @brief Hero configuration (special powerful unit)
 */
struct HeroConfig : UnitConfig {
    std::string heroClass;          // warrior, mage, support
    int startingLevel = 1;

    // Per-level stat growth
    int healthPerLevel = 50;
    int manaPerLevel = 25;
    int damagePerLevel = 3;
    float strPerLevel = 2.0f;
    float agiPerLevel = 1.5f;
    float intPerLevel = 2.0f;

    // Ability slots (Q, W, E, R typically)
    std::vector<std::string> heroAbilityRefs;
    std::string ultimateAbilityRef;
};

/**
 * @brief Building configuration
 */
struct BuildingConfig : EntityConfig {
    std::vector<std::string> trains;    // Units this building can train
    std::vector<std::string> upgrades;  // What this building upgrades to
    std::vector<std::string> researches; // Upgrades that can be researched here

    bool isDefensive = false;           // Can attack
    bool isMainBuilding = false;        // Town Hall, etc.
    bool providesDropOff = false;       // Resource drop-off point

    glm::vec2 footprint{2.0f, 2.0f};    // Building size in tiles
};

/**
 * @brief Resource node configuration (gold mine, tree, etc.)
 */
struct ResourceConfig : EntityConfig {
    std::string resourceType;       // gold, lumber, stone, food
    int resourceAmount = 1000;      // Total harvestable amount
    int harvestRate = 10;           // Amount per harvest
    float harvestTime = 1.0f;       // Time per harvest
    bool depletes = true;           // Does it run out?
    bool respawns = false;          // Does it come back?
    float respawnTime = 0.0f;
};

/**
 * @brief Projectile configuration
 */
struct ProjectileConfig : EntityConfig {
    float speed = 500.0f;
    float arcHeight = 0.0f;         // 0 = straight line
    bool homing = false;
    float turnRate = 0.0f;
    std::string impactEffectRef;
    int damage = 0;
    float splashRadius = 0.0f;
};

/**
 * @brief Decoration configuration (non-interactive)
 */
struct DecorationConfig : EntityConfig {
    bool blocksPathing = false;
    bool blocksBuilding = true;
    float fadeDistance = 100.0f;
};

} // namespace Engine
