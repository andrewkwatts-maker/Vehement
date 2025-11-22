#pragma once

#include "SpellDefinition.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <functional>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellInstance;
class SpellCaster;

// ============================================================================
// Effect Types
// ============================================================================

/**
 * @brief All supported spell effect types
 */
enum class EffectType : uint8_t {
    // Damage/Healing
    Damage,             // Direct damage
    Heal,               // Direct healing
    DamageOverTime,     // Periodic damage (DoT)
    HealOverTime,       // Periodic healing (HoT)
    AbsorbDamage,       // Damage shield

    // Crowd Control
    Stun,               // Prevents all actions
    Root,               // Prevents movement
    Silence,            // Prevents spell casting
    Disarm,             // Prevents attacks
    Slow,               // Reduces movement speed
    Fear,               // Causes fleeing
    Charm,              // Mind control
    Sleep,              // Disabled until damaged
    Knockback,          // Pushes target away
    Pull,               // Pulls target toward caster
    Grip,               // Holds target in place

    // Buffs/Debuffs
    Buff,               // Generic positive effect
    Debuff,             // Generic negative effect
    StatModifier,       // Modifies a stat
    Aura,               // Passive area effect

    // Movement
    Teleport,           // Instant position change
    Dash,               // Quick movement in direction
    Leap,               // Jump to location

    // Summoning
    Summon,             // Create entity
    Transform,          // Change form

    // Utility
    Dispel,             // Remove effects
    Interrupt,          // Stop casting
    Resurrect,          // Revive dead target
    ResourceRestore,    // Restore mana/energy
    ResourceDrain,      // Steal resources

    // Special
    Script,             // Custom Python script effect
    Trigger,            // Trigger another effect conditionally
    Chain,              // Chain to another spell

    COUNT
};

// ============================================================================
// Effect Timing
// ============================================================================

/**
 * @brief When the effect is applied
 */
enum class EffectTiming : uint8_t {
    Instant,            // Applied immediately
    OverTime,           // Applied over duration (DoT/HoT)
    Delayed,            // Applied after delay
    OnInterval,         // Applied at regular intervals
    OnExpire,           // Applied when effect expires
    OnRemove,           // Applied when effect is dispelled
    OnStack,            // Applied when new stack is added
    Channeled           // Applied during channel
};

// ============================================================================
// Stacking Rules
// ============================================================================

/**
 * @brief How multiple applications of the effect interact
 */
enum class StackingRule : uint8_t {
    Refresh,            // Reset duration, keep stacks
    Stack,              // Add new stack
    Replace,            // Replace with new application
    Ignore,             // Don't apply if already present
    Pandemic,           // Add remaining time to new duration (up to cap)
    Highest,            // Keep highest value only
    Lowest,             // Keep lowest value only
    Separate            // Each application is independent
};

// ============================================================================
// Conditional Triggers
// ============================================================================

/**
 * @brief Conditions that can trigger bonus effects
 */
enum class TriggerCondition : uint8_t {
    None,               // Always applies
    OnCrit,             // When effect crits
    OnKill,             // When effect kills target
    OnLowHealth,        // When target health below threshold
    OnHighHealth,       // When target health above threshold
    OnMiss,             // When effect misses
    OnResist,           // When target resists
    OnDispel,           // When effect is dispelled
    OnExpire,           // When effect expires naturally
    OnTargetCast,       // When target casts a spell
    OnTargetMove,       // When target moves
    OnTargetAttack,     // When target attacks
    OnDamageTaken,      // When caster takes damage
    OnHealReceived,     // When caster receives healing
    OnResourceSpent,    // When caster spends resources
    OnCombo,            // When combo point requirement met
    CustomScript        // Custom Python condition
};

// ============================================================================
// Damage Type
// ============================================================================

/**
 * @brief Damage element/type
 */
enum class DamageType : uint8_t {
    Physical,
    Fire,
    Frost,
    Nature,
    Arcane,
    Shadow,
    Holy,
    Lightning,
    Poison,
    Bleed,
    Pure,               // Ignores all mitigation

    COUNT
};

// ============================================================================
// Stat Modifier Type
// ============================================================================

/**
 * @brief Type of stat modification
 */
enum class StatModType : uint8_t {
    Flat,               // Add flat value
    Percent,            // Add percentage
    Multiplicative      // Multiply by value
};

// ============================================================================
// Effect Scaling
// ============================================================================

/**
 * @brief How an effect scales with stats
 */
struct EffectScaling {
    std::string stat;               // Stat name to scale with
    float coefficient = 0.0f;       // Scaling coefficient
    float levelBonus = 0.0f;        // Bonus per caster level
    float targetLevelPenalty = 0.0f; // Reduction per target level difference

    // Caps
    float minValue = 0.0f;
    float maxValue = std::numeric_limits<float>::max();

    /**
     * @brief Calculate scaled value
     */
    float Calculate(float base, float statValue, int casterLevel, int targetLevel) const;
};

// ============================================================================
// Conditional Effect
// ============================================================================

/**
 * @brief Effect that triggers under specific conditions
 */
struct ConditionalEffect {
    TriggerCondition condition = TriggerCondition::None;
    float threshold = 0.0f;         // Threshold for condition (e.g., health %)
    float chance = 1.0f;            // Chance to trigger (0-1)
    std::string conditionScript;    // Custom condition script

    // Effect to apply
    std::string effectId;           // Reference to another effect
    std::shared_ptr<class SpellEffect> inlineEffect; // Or inline effect
};

// ============================================================================
// Spell Effect Base Class
// ============================================================================

/**
 * @brief Base class for all spell effects
 *
 * All effect behavior is configured via JSON. The effect system applies
 * the configured effects to targets based on type and parameters.
 */
class SpellEffect {
public:
    SpellEffect() = default;
    virtual ~SpellEffect() = default;

    // Disable copy, allow move
    SpellEffect(const SpellEffect&) = delete;
    SpellEffect& operator=(const SpellEffect&) = delete;
    SpellEffect(SpellEffect&&) noexcept = default;
    SpellEffect& operator=(SpellEffect&&) noexcept = default;

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Load effect from JSON string
     */
    bool LoadFromJson(const std::string& jsonString);

    /**
     * @brief Serialize effect to JSON string
     */
    std::string ToJsonString() const;

    /**
     * @brief Validate effect configuration
     */
    bool Validate(std::vector<std::string>& errors) const;

    // =========================================================================
    // Effect Application
    // =========================================================================

    /**
     * @brief Apply this effect to a target
     * @param instance The spell instance
     * @param casterId Entity casting the spell
     * @param targetId Entity receiving the effect
     * @param casterStats Function to get caster stat values
     * @param targetStats Function to get target stat values
     * @return Amount of effect applied (damage dealt, healing done, etc.)
     */
    using StatQueryFunc = std::function<float(uint32_t entityId, const std::string& statName)>;

    struct ApplicationResult {
        bool success = false;
        float amount = 0.0f;            // Primary effect amount
        bool wasCrit = false;
        bool wasResisted = false;
        bool wasAbsorbed = false;
        float absorbedAmount = 0.0f;
        std::string failReason;
        std::vector<uint32_t> triggeredEffects;
    };

    virtual ApplicationResult Apply(
        const SpellInstance& instance,
        uint32_t casterId,
        uint32_t targetId,
        StatQueryFunc casterStats,
        StatQueryFunc targetStats
    ) const;

    /**
     * @brief Calculate the effect amount
     */
    float CalculateAmount(
        uint32_t casterId,
        uint32_t targetId,
        StatQueryFunc casterStats,
        StatQueryFunc targetStats
    ) const;

    /**
     * @brief Check if this effect should trigger conditionals
     */
    bool CheckTriggerCondition(
        TriggerCondition condition,
        const ApplicationResult& result,
        uint32_t casterId,
        uint32_t targetId,
        StatQueryFunc statQuery
    ) const;

    // =========================================================================
    // Periodic Effect Update
    // =========================================================================

    /**
     * @brief Update periodic effect (for DoT/HoT)
     * @param deltaTime Time since last update
     * @param tickCallback Called when a tick should be applied
     */
    using TickCallback = std::function<void(float amount)>;

    void UpdatePeriodic(float deltaTime, TickCallback tickCallback);

    // =========================================================================
    // Stacking
    // =========================================================================

    /**
     * @brief Handle new application with stacking rules
     * @param existingDuration Current remaining duration
     * @param existingStacks Current stack count
     * @return New duration and stack count
     */
    std::pair<float, int> HandleStacking(float existingDuration, int existingStacks) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const { return m_id; }
    [[nodiscard]] EffectType GetType() const { return m_type; }
    [[nodiscard]] EffectTiming GetTiming() const { return m_timing; }
    [[nodiscard]] StackingRule GetStackingRule() const { return m_stackingRule; }
    [[nodiscard]] DamageType GetDamageType() const { return m_damageType; }

    [[nodiscard]] float GetBaseAmount() const { return m_baseAmount; }
    [[nodiscard]] float GetDuration() const { return m_duration; }
    [[nodiscard]] float GetTickInterval() const { return m_tickInterval; }
    [[nodiscard]] float GetDelay() const { return m_delay; }
    [[nodiscard]] int GetMaxStacks() const { return m_maxStacks; }
    [[nodiscard]] float GetStackValue() const { return m_stackValue; }

    [[nodiscard]] const EffectScaling& GetScaling() const { return m_scaling; }
    [[nodiscard]] const std::vector<ConditionalEffect>& GetConditionals() const { return m_conditionals; }

    [[nodiscard]] float GetCritChance() const { return m_critChance; }
    [[nodiscard]] float GetCritMultiplier() const { return m_critMultiplier; }

    [[nodiscard]] const std::string& GetCustomScript() const { return m_customScript; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }
    [[nodiscard]] const std::string& GetIconOverride() const { return m_iconOverride; }

    // =========================================================================
    // Mutators
    // =========================================================================

    void SetId(const std::string& id) { m_id = id; }
    void SetType(EffectType type) { m_type = type; }
    void SetTiming(EffectTiming timing) { m_timing = timing; }
    void SetStackingRule(StackingRule rule) { m_stackingRule = rule; }
    void SetDamageType(DamageType type) { m_damageType = type; }

    void SetBaseAmount(float amount) { m_baseAmount = amount; }
    void SetDuration(float duration) { m_duration = duration; }
    void SetTickInterval(float interval) { m_tickInterval = interval; }
    void SetDelay(float delay) { m_delay = delay; }
    void SetMaxStacks(int stacks) { m_maxStacks = stacks; }
    void SetStackValue(float value) { m_stackValue = value; }

    void SetScaling(const EffectScaling& scaling) { m_scaling = scaling; }
    void AddConditional(const ConditionalEffect& conditional) { m_conditionals.push_back(conditional); }

    void SetCritChance(float chance) { m_critChance = chance; }
    void SetCritMultiplier(float mult) { m_critMultiplier = mult; }

    void SetCustomScript(const std::string& script) { m_customScript = script; }
    void SetDescription(const std::string& desc) { m_description = desc; }
    void SetIconOverride(const std::string& icon) { m_iconOverride = icon; }

    // =========================================================================
    // Type-Specific Configuration
    // =========================================================================

    // Stat modifier specific
    struct StatModConfig {
        std::string statName;
        StatModType modType = StatModType::Flat;
        float value = 0.0f;
        float valuePerStack = 0.0f;
    };
    std::vector<StatModConfig> statModifiers;

    // Summon specific
    struct SummonConfig {
        std::string unitId;
        int count = 1;
        float duration = 0.0f;          // 0 = permanent
        bool inheritFaction = true;
        glm::vec3 spawnOffset{0.0f};
        float spawnRadius = 2.0f;
    };
    std::optional<SummonConfig> summonConfig;

    // Movement specific (teleport, dash, leap)
    struct MovementConfig {
        float distance = 10.0f;
        float speed = 20.0f;            // For dash/leap
        bool towardTarget = true;
        bool throughWalls = false;
        std::string arrivalEffect;
    };
    std::optional<MovementConfig> movementConfig;

    // Knockback/Pull specific
    struct DisplacementConfig {
        float distance = 5.0f;
        float speed = 15.0f;
        bool scalesWithDistance = false;
        bool knocksUp = false;
        float knockUpHeight = 2.0f;
    };
    std::optional<DisplacementConfig> displacementConfig;

    // Dispel specific
    struct DispelConfig {
        bool dispelBuffs = false;
        bool dispelDebuffs = true;
        int maxDispelled = 1;
        std::vector<std::string> specificEffects;   // Only these effects
        std::vector<std::string> excludedEffects;   // Never these effects
        bool dispelMagic = true;
        bool dispelCurse = false;
        bool dispelPoison = false;
        bool dispelDisease = false;
    };
    std::optional<DispelConfig> dispelConfig;

    // Resource specific
    struct ResourceConfig {
        std::string resourceType;       // mana, energy, rage, etc.
        float amount = 0.0f;
        bool percentage = false;        // Amount is percentage of max
        bool drain = false;             // Steal from target
        float drainEfficiency = 1.0f;   // How much caster gets from drain
    };
    std::optional<ResourceConfig> resourceConfig;

protected:
    // Identity
    std::string m_id;
    std::string m_description;
    std::string m_iconOverride;

    // Core configuration
    EffectType m_type = EffectType::Damage;
    EffectTiming m_timing = EffectTiming::Instant;
    StackingRule m_stackingRule = StackingRule::Refresh;
    DamageType m_damageType = DamageType::Physical;

    // Amount configuration
    float m_baseAmount = 0.0f;
    float m_duration = 0.0f;
    float m_tickInterval = 1.0f;
    float m_delay = 0.0f;
    int m_maxStacks = 1;
    float m_stackValue = 0.0f;          // Additional value per stack

    // Scaling
    EffectScaling m_scaling;

    // Critical hits
    float m_critChance = 0.0f;          // Additional crit chance (added to base)
    float m_critMultiplier = 2.0f;      // Crit damage multiplier

    // Conditional effects
    std::vector<ConditionalEffect> m_conditionals;

    // Custom script for Script effect type
    std::string m_customScript;

    // Runtime state for periodic effects
    float m_periodicAccumulator = 0.0f;
    int m_currentStacks = 0;
};

// ============================================================================
// Active Effect Instance
// ============================================================================

/**
 * @brief Runtime instance of an applied effect (buff/debuff)
 */
class ActiveEffect {
public:
    ActiveEffect(const SpellEffect* effect, uint32_t casterId, uint32_t targetId);

    /**
     * @brief Update the effect each frame
     * @param deltaTime Time since last frame
     * @return true if effect is still active, false if expired
     */
    bool Update(float deltaTime);

    /**
     * @brief Add a stack to this effect
     * @return New stack count
     */
    int AddStack();

    /**
     * @brief Remove a stack from this effect
     * @return New stack count
     */
    int RemoveStack();

    /**
     * @brief Refresh the effect duration
     */
    void Refresh();

    /**
     * @brief Apply pandemic (add remaining time to new duration)
     */
    void ApplyPandemic(float newDuration, float maxPandemicBonus);

    // Accessors
    [[nodiscard]] const SpellEffect* GetEffect() const { return m_effect; }
    [[nodiscard]] uint32_t GetCasterId() const { return m_casterId; }
    [[nodiscard]] uint32_t GetTargetId() const { return m_targetId; }
    [[nodiscard]] float GetRemainingDuration() const { return m_remainingDuration; }
    [[nodiscard]] float GetTotalDuration() const { return m_totalDuration; }
    [[nodiscard]] int GetStacks() const { return m_stacks; }
    [[nodiscard]] bool IsExpired() const { return m_remainingDuration <= 0.0f; }
    [[nodiscard]] float GetProgress() const { return m_remainingDuration / m_totalDuration; }

    // Callbacks
    using TickCallback = std::function<void(ActiveEffect&, float amount)>;
    using ExpirationCallback = std::function<void(ActiveEffect&)>;

    void SetOnTick(TickCallback callback) { m_onTick = std::move(callback); }
    void SetOnExpire(ExpirationCallback callback) { m_onExpire = std::move(callback); }

private:
    const SpellEffect* m_effect = nullptr;
    uint32_t m_casterId = 0;
    uint32_t m_targetId = 0;

    float m_remainingDuration = 0.0f;
    float m_totalDuration = 0.0f;
    float m_tickAccumulator = 0.0f;
    int m_stacks = 1;

    TickCallback m_onTick;
    ExpirationCallback m_onExpire;
};

// ============================================================================
// Effect Factory
// ============================================================================

/**
 * @brief Factory for creating effect instances from JSON
 */
class SpellEffectFactory {
public:
    static SpellEffectFactory& Instance();

    /**
     * @brief Create effect from JSON
     */
    std::shared_ptr<SpellEffect> CreateFromJson(const std::string& jsonString);

    /**
     * @brief Create effect by type
     */
    std::shared_ptr<SpellEffect> Create(EffectType type);

private:
    SpellEffectFactory() = default;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert EffectType to string
 */
const char* EffectTypeToString(EffectType type);

/**
 * @brief Parse EffectType from string
 */
EffectType StringToEffectType(const std::string& str);

/**
 * @brief Convert EffectTiming to string
 */
const char* EffectTimingToString(EffectTiming timing);

/**
 * @brief Parse EffectTiming from string
 */
EffectTiming StringToEffectTiming(const std::string& str);

/**
 * @brief Convert StackingRule to string
 */
const char* StackingRuleToString(StackingRule rule);

/**
 * @brief Parse StackingRule from string
 */
StackingRule StringToStackingRule(const std::string& str);

/**
 * @brief Convert DamageType to string
 */
const char* DamageTypeToString(DamageType type);

/**
 * @brief Parse DamageType from string
 */
DamageType StringToDamageType(const std::string& str);

/**
 * @brief Convert TriggerCondition to string
 */
const char* TriggerConditionToString(TriggerCondition condition);

/**
 * @brief Parse TriggerCondition from string
 */
TriggerCondition StringToTriggerCondition(const std::string& str);

} // namespace Spells
} // namespace Vehement
