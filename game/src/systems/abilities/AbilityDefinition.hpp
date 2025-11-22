#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <glm/glm.hpp>

namespace Vehement {
namespace Abilities {

// ============================================================================
// Ability Types
// ============================================================================

/**
 * @brief Types of abilities
 */
enum class AbilityType : uint8_t {
    Active,     // Requires manual activation
    Passive,    // Always active, no activation
    Toggle,     // Can be turned on/off
    Autocast    // Can be set to auto-activate
};

/**
 * @brief Targeting modes for abilities
 */
enum class TargetingType : uint8_t {
    None,           // No target (self-cast)
    Point,          // Target ground location
    Unit,           // Target single unit
    UnitOrPoint,    // Target unit or ground
    Area,           // Area of effect
    Direction,      // Cast in direction
    Cone,           // Cone-shaped area
    Line,           // Line from caster
    Vector          // Point with direction
};

/**
 * @brief Unit target flags
 */
enum class TargetFlag : uint32_t {
    None        = 0,
    Self        = 1 << 0,
    Ally        = 1 << 1,
    Enemy       = 1 << 2,
    Hero        = 1 << 3,
    Creep       = 1 << 4,
    Building    = 1 << 5,
    Ancient     = 1 << 6,
    Mechanical  = 1 << 7,
    Organic     = 1 << 8,
    Dead        = 1 << 9,
    Invulnerable = 1 << 10,
    Invisible   = 1 << 11,
    MagicImmune = 1 << 12,

    AllUnits    = Self | Ally | Enemy | Hero | Creep,
    AllAllies   = Self | Ally,
    AllEnemies  = Enemy
};

inline TargetFlag operator|(TargetFlag a, TargetFlag b) {
    return static_cast<TargetFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(TargetFlag a, TargetFlag b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief Damage types
 */
enum class DamageType : uint8_t {
    Physical,   // Reduced by armor
    Magical,    // Reduced by magic resist
    Pure,       // Not reduced
    HP_Removal  // Ignores everything
};

/**
 * @brief Ability behavior flags
 */
enum class AbilityBehavior : uint32_t {
    None            = 0,
    Hidden          = 1 << 0,   // Not shown in UI
    Passive         = 1 << 1,   // Passive ability
    NoTarget        = 1 << 2,   // No target required
    UnitTarget      = 1 << 3,   // Requires unit target
    PointTarget     = 1 << 4,   // Requires point target
    AOE             = 1 << 5,   // Area of effect
    Channeled       = 1 << 6,   // Must channel
    Toggle          = 1 << 7,   // Toggle on/off
    Autocast        = 1 << 8,   // Can be autocast
    NotLearnable    = 1 << 9,   // Cannot be learned
    Aura            = 1 << 10,  // Is an aura
    AttackModifier  = 1 << 11,  // Modifies attacks
    Immediate       = 1 << 12,  // Instant cast
    Directional     = 1 << 13,  // Cast in direction
    Unrestricted    = 1 << 14,  // Ignore disable states
    IgnoreBackswing = 1 << 15,  // No backswing
    RootDisables    = 1 << 16,  // Disabled by root
    DontProceedCast = 1 << 17,  // Don't auto-proceed
    IgnoreChannel   = 1 << 18,  // Doesn't interrupt channel
    DontCancelChannel = 1 << 19,// Can be used while channeling
    DontCancelMovement = 1 << 20 // Doesn't stop movement
};

inline AbilityBehavior operator|(AbilityBehavior a, AbilityBehavior b) {
    return static_cast<AbilityBehavior>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(AbilityBehavior a, AbilityBehavior b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// ============================================================================
// Per-Level Scaling Data
// ============================================================================

/**
 * @brief Per-level ability values
 */
struct AbilityLevelData {
    // Damage/Healing
    float damage = 0.0f;
    float healing = 0.0f;
    DamageType damageType = DamageType::Magical;

    // Costs
    float manaCost = 0.0f;
    float healthCost = 0.0f;

    // Timing
    float cooldown = 10.0f;
    float duration = 0.0f;
    float castTime = 0.0f;
    float channelTime = 0.0f;

    // Range/Area
    float castRange = 0.0f;
    float effectRadius = 0.0f;
    float width = 0.0f;         // For line abilities
    float travelDistance = 0.0f; // For projectiles

    // Status effects
    float slowPercent = 0.0f;
    float stunDuration = 0.0f;
    float silenceDuration = 0.0f;
    float disarmDuration = 0.0f;

    // Buff values
    float bonusDamage = 0.0f;
    float bonusArmor = 0.0f;
    float bonusMoveSpeed = 0.0f;
    float bonusAttackSpeed = 0.0f;
    float bonusHealthRegen = 0.0f;
    float bonusManaRegen = 0.0f;

    // Special values
    float specialValue1 = 0.0f;  // Custom ability-specific
    float specialValue2 = 0.0f;
    float specialValue3 = 0.0f;
    float specialValue4 = 0.0f;

    // Scaling
    float strengthScaling = 0.0f;   // % of strength added
    float agilityScaling = 0.0f;
    float intelligenceScaling = 0.0f;
    float attackDamageScaling = 0.0f;

    // Charges
    int charges = 0;
    float chargeRestoreTime = 0.0f;

    // Targets
    int maxTargets = 1;
    int bounces = 0;
};

// ============================================================================
// Ability Trigger
// ============================================================================

/**
 * @brief Trigger events for passive abilities
 */
enum class TriggerEvent : uint8_t {
    None,
    OnAttackHit,        // When attack lands
    OnAttackStart,      // When attack begins
    OnDamageTaken,      // When taking damage
    OnDamageDealt,      // When dealing damage
    OnKill,             // When killing target
    OnDeath,            // When dying
    OnCast,             // When casting ability
    OnAbilityHit,       // When ability hits
    OnHealthLow,        // When health drops below threshold
    OnManaLow,          // When mana drops below threshold
    OnInterval,         // On timed interval
    OnProximity,        // When enemy is near
    OnMove,             // When moving
    OnStop,             // When stopping
    OnTakeMagicDamage,  // When taking magic damage
    OnTakePhysicalDamage, // When taking physical damage
    OnCriticalHit,      // When landing critical hit
    OnEvasion           // When evading attack
};

/**
 * @brief Trigger configuration for passive abilities
 */
struct AbilityTrigger {
    TriggerEvent event = TriggerEvent::None;
    std::string effectId;           // Effect to apply
    float chance = 1.0f;            // Proc chance (0-1)
    float cooldown = 0.0f;          // Internal cooldown
    float threshold = 0.0f;         // For threshold-based triggers
    std::string condition;          // Optional condition script
};

// ============================================================================
// Visual/Audio Configuration
// ============================================================================

/**
 * @brief Visual and audio effects for abilities
 */
struct AbilityEffects {
    // Cast effects
    std::string castAnimation;
    std::string castSound;
    std::string castParticle;

    // Impact effects
    std::string impactSound;
    std::string impactParticle;

    // Projectile (if applicable)
    std::string projectileModel;
    std::string projectileTrail;
    float projectileSpeed = 0.0f;

    // Buff/Debuff effects
    std::string buffParticle;
    std::string debuffParticle;

    // Channel effects
    std::string channelAnimation;
    std::string channelParticle;

    // Icon
    std::string iconPath;
};

// ============================================================================
// Script Event Bindings
// ============================================================================

/**
 * @brief Script bindings for ability events
 */
struct AbilityScriptBindings {
    std::string onLearn;            // When ability is learned
    std::string onUpgrade;          // When ability level increases
    std::string onCastStart;        // When cast begins
    std::string onCastComplete;     // When cast completes
    std::string onChannelTick;      // Each channel tick
    std::string onChannelEnd;       // When channel ends
    std::string onHit;              // When ability hits target
    std::string onKill;             // When ability kills target
    std::string onToggleOn;         // When toggled on
    std::string onToggleOff;        // When toggled off
    std::string onCreate;           // When definition is created
    std::string onDestroy;          // When definition is destroyed
};

// ============================================================================
// Ability Definition
// ============================================================================

/**
 * @brief Complete ability definition loaded from JSON
 *
 * Supports:
 * - Active, Passive, Toggle, Autocast types
 * - Multiple levels with per-level scaling
 * - Various targeting modes
 * - Trigger system for passives
 * - Visual and audio effects
 * - Script event bindings
 * - Create/Tick/Destroy lifecycle
 */
class AbilityDefinition {
public:
    using CreateCallback = std::function<void(const AbilityDefinition&)>;
    using TickCallback = std::function<void(const AbilityDefinition&, float)>;
    using DestroyCallback = std::function<void(const AbilityDefinition&)>;

    static constexpr int MAX_ABILITY_LEVEL = 7;

    AbilityDefinition();
    ~AbilityDefinition();

    // =========================================================================
    // Loading and Serialization
    // =========================================================================

    bool LoadFromFile(const std::string& filePath);
    bool LoadFromString(const std::string& jsonString);
    bool SaveToFile(const std::string& filePath) const;
    std::string ToJsonString() const;
    bool Validate(std::vector<std::string>& outErrors) const;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    void Create();
    void Tick(float deltaTime);
    void Destroy();
    [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const noexcept { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] const std::string& GetLore() const noexcept { return m_lore; }
    void SetLore(const std::string& lore) { m_lore = lore; }

    // =========================================================================
    // Type and Behavior
    // =========================================================================

    [[nodiscard]] AbilityType GetType() const noexcept { return m_type; }
    void SetType(AbilityType type) { m_type = type; }

    [[nodiscard]] AbilityBehavior GetBehavior() const noexcept { return m_behavior; }
    void SetBehavior(AbilityBehavior behavior) { m_behavior = behavior; }
    [[nodiscard]] bool HasBehavior(AbilityBehavior behavior) const {
        return m_behavior & behavior;
    }

    [[nodiscard]] TargetingType GetTargetingType() const noexcept { return m_targetingType; }
    void SetTargetingType(TargetingType type) { m_targetingType = type; }

    [[nodiscard]] TargetFlag GetTargetFlags() const noexcept { return m_targetFlags; }
    void SetTargetFlags(TargetFlag flags) { m_targetFlags = flags; }
    [[nodiscard]] bool CanTarget(TargetFlag flag) const {
        return m_targetFlags & flag;
    }

    // =========================================================================
    // Levels
    // =========================================================================

    [[nodiscard]] int GetMaxLevel() const noexcept { return m_maxLevel; }
    void SetMaxLevel(int level) { m_maxLevel = level; }

    [[nodiscard]] const std::vector<AbilityLevelData>& GetLevelData() const noexcept {
        return m_levelData;
    }
    void SetLevelData(const std::vector<AbilityLevelData>& data) { m_levelData = data; }

    /**
     * @brief Get data for a specific level (1-indexed)
     */
    [[nodiscard]] const AbilityLevelData& GetDataForLevel(int level) const;

    /**
     * @brief Get interpolated value between levels
     */
    [[nodiscard]] float GetInterpolatedValue(int level, float AbilityLevelData::*member) const;

    // =========================================================================
    // Triggers (for passives)
    // =========================================================================

    [[nodiscard]] const std::vector<AbilityTrigger>& GetTriggers() const noexcept {
        return m_triggers;
    }
    void SetTriggers(const std::vector<AbilityTrigger>& triggers) { m_triggers = triggers; }
    void AddTrigger(const AbilityTrigger& trigger) { m_triggers.push_back(trigger); }

    // =========================================================================
    // Effects
    // =========================================================================

    [[nodiscard]] const AbilityEffects& GetEffects() const noexcept { return m_effects; }
    void SetEffects(const AbilityEffects& effects) { m_effects = effects; }

    // =========================================================================
    // Scripts
    // =========================================================================

    [[nodiscard]] const AbilityScriptBindings& GetScripts() const noexcept { return m_scripts; }
    void SetScripts(const AbilityScriptBindings& scripts) { m_scripts = scripts; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnCreate(CreateCallback callback) { m_onCreate = std::move(callback); }
    void SetOnTick(TickCallback callback) { m_onTick = std::move(callback); }
    void SetOnDestroy(DestroyCallback callback) { m_onDestroy = std::move(callback); }

    // =========================================================================
    // Tags
    // =========================================================================

    [[nodiscard]] const std::vector<std::string>& GetTags() const noexcept { return m_tags; }
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }
    [[nodiscard]] bool HasTag(const std::string& tag) const;

    // =========================================================================
    // Source Info
    // =========================================================================

    [[nodiscard]] const std::string& GetSourcePath() const noexcept { return m_sourcePath; }

private:
    void ParseJson(const std::string& json);
    void ParseLevels(const std::string& json);
    void ParseTriggers(const std::string& json);
    void ParseEffects(const std::string& json);
    void ParseScripts(const std::string& json);

    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_lore;

    // Type and behavior
    AbilityType m_type = AbilityType::Active;
    AbilityBehavior m_behavior = AbilityBehavior::None;
    TargetingType m_targetingType = TargetingType::None;
    TargetFlag m_targetFlags = TargetFlag::None;

    // Levels
    int m_maxLevel = 4;
    std::vector<AbilityLevelData> m_levelData;

    // Triggers
    std::vector<AbilityTrigger> m_triggers;

    // Effects
    AbilityEffects m_effects;

    // Scripts
    AbilityScriptBindings m_scripts;

    // Tags
    std::vector<std::string> m_tags;

    // Source info
    std::string m_sourcePath;
    int64_t m_lastModified = 0;

    // Lifecycle
    bool m_isActive = false;

    // Callbacks
    CreateCallback m_onCreate;
    TickCallback m_onTick;
    DestroyCallback m_onDestroy;
};

// ============================================================================
// Ability Definition Registry
// ============================================================================

class AbilityDefinitionRegistry {
public:
    static AbilityDefinitionRegistry& Instance();

    int LoadFromDirectory(const std::string& configPath);
    void Register(std::shared_ptr<AbilityDefinition> definition);
    [[nodiscard]] std::shared_ptr<AbilityDefinition> Get(const std::string& id) const;
    [[nodiscard]] std::vector<std::shared_ptr<AbilityDefinition>> GetAll() const;
    [[nodiscard]] std::vector<std::shared_ptr<AbilityDefinition>> GetByType(AbilityType type) const;
    [[nodiscard]] std::vector<std::shared_ptr<AbilityDefinition>> GetByTag(const std::string& tag) const;
    [[nodiscard]] bool Exists(const std::string& id) const;
    [[nodiscard]] size_t Count() const { return m_definitions.size(); }
    void Clear();
    void Reload();
    void Tick(float deltaTime);

private:
    AbilityDefinitionRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<AbilityDefinition>> m_definitions;
    std::string m_configPath;
};

// ============================================================================
// Helper Functions
// ============================================================================

[[nodiscard]] std::string AbilityTypeToString(AbilityType type);
[[nodiscard]] AbilityType StringToAbilityType(const std::string& str);
[[nodiscard]] std::string TargetingTypeToString(TargetingType type);
[[nodiscard]] TargetingType StringToTargetingType(const std::string& str);
[[nodiscard]] std::string TriggerEventToString(TriggerEvent event);
[[nodiscard]] TriggerEvent StringToTriggerEvent(const std::string& str);
[[nodiscard]] std::string DamageTypeToString(DamageType type);
[[nodiscard]] DamageType StringToDamageType(const std::string& str);

} // namespace Abilities
} // namespace Vehement
