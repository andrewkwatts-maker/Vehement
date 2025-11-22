#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <cstdint>

namespace Vehement {
namespace Effects {

// Forward declarations
class EffectInstance;
class EffectContainer;

// ============================================================================
// Trigger Condition Type
// ============================================================================

/**
 * @brief Conditions that can trigger an effect
 */
enum class TriggerCondition : uint8_t {
    // Combat events
    OnHit,              // When dealing damage
    OnCrit,             // When landing a critical hit
    OnKill,             // When killing an enemy
    OnAssist,           // When assisting a kill
    OnDamageTaken,      // When receiving damage
    OnCritTaken,        // When receiving a critical hit
    OnHeal,             // When healing
    OnHealed,           // When being healed
    OnBlock,            // When blocking an attack
    OnDodge,            // When dodging an attack
    OnParry,            // When parrying

    // Ability events
    OnAbilityUse,       // When using any ability
    OnAbilityCast,      // When starting to cast
    OnAbilityComplete,  // When ability finishes casting
    OnSpellCast,        // When casting a spell (mana cost)
    OnMeleeAttack,      // When performing melee attack
    OnRangedAttack,     // When performing ranged attack

    // Health/Resource events
    OnHealthBelow,      // When health drops below threshold
    OnHealthAbove,      // When health rises above threshold
    OnManaBelow,        // When mana drops below threshold
    OnLowHealth,        // When entering "low health" state
    OnFullHealth,       // When reaching full health

    // Movement events
    OnMove,             // Each movement tick
    OnStand,            // When standing still
    OnJump,             // When jumping
    OnDash,             // When dashing
    OnTeleport,         // When teleporting

    // State events
    OnEnterCombat,      // When entering combat
    OnLeaveCombat,      // When leaving combat
    OnDeath,            // When dying
    OnRespawn,          // When respawning
    OnLevelUp,          // When gaining a level
    OnEquipChange,      // When changing equipment

    // Effect events
    OnBuffApplied,      // When gaining a buff
    OnDebuffApplied,    // When gaining a debuff
    OnEffectRemoved,    // When any effect is removed
    OnDispel,           // When being dispelled

    // Environmental
    OnInterval,         // Periodic interval trigger
    OnZoneEnter,        // When entering a zone
    OnZoneExit,         // When exiting a zone

    // Custom
    OnCustomEvent       // Script-defined event
};

/**
 * @brief Convert trigger condition to string
 */
const char* TriggerConditionToString(TriggerCondition cond);

/**
 * @brief Parse trigger condition from string
 */
std::optional<TriggerCondition> TriggerConditionFromString(const std::string& str);

// ============================================================================
// Trigger Action Type
// ============================================================================

/**
 * @brief Actions that can be performed when triggered
 */
enum class TriggerAction : uint8_t {
    ApplyEffect,        // Apply an effect to target
    RemoveEffect,       // Remove an effect from target
    ExtendDuration,     // Extend effect duration
    ReduceDuration,     // Reduce effect duration
    AddStacks,          // Add stacks to effect
    RemoveStacks,       // Remove stacks from effect
    RefreshEffect,      // Refresh effect duration
    DealDamage,         // Deal direct damage
    HealTarget,         // Heal target
    RestoreMana,        // Restore mana
    ModifyStat,         // Temporarily modify stat
    SpawnProjectile,    // Spawn a projectile
    CreateAura,         // Create an aura
    TeleportTarget,     // Teleport the target
    KnockbackTarget,    // Apply knockback
    StunTarget,         // Apply stun
    ExecuteScript,      // Run custom script
    ChainTrigger        // Trigger another effect
};

/**
 * @brief Convert trigger action to string
 */
const char* TriggerActionToString(TriggerAction action);

/**
 * @brief Parse trigger action from string
 */
std::optional<TriggerAction> TriggerActionFromString(const std::string& str);

// ============================================================================
// Trigger Target
// ============================================================================

/**
 * @brief Target of the triggered effect
 */
enum class TriggerTarget : uint8_t {
    Self,               // Effect holder
    Source,             // Who applied the parent effect
    AttackTarget,       // Current attack target
    DamageSource,       // Who dealt damage
    HealSource,         // Who provided healing
    NearestEnemy,       // Closest enemy
    NearestAlly,        // Closest ally
    AllNearbyEnemies,   // All enemies in range
    AllNearbyAllies,    // All allies in range
    RandomEnemy,        // Random enemy in range
    RandomAlly,         // Random ally in range
    KillTarget,         // Target that was killed
    Custom              // Script-determined
};

/**
 * @brief Convert trigger target to string
 */
const char* TriggerTargetToString(TriggerTarget target);

/**
 * @brief Parse trigger target from string
 */
std::optional<TriggerTarget> TriggerTargetFromString(const std::string& str);

// ============================================================================
// Effect Trigger
// ============================================================================

/**
 * @brief Definition of a triggered effect
 */
struct EffectTrigger {
    // Trigger condition
    TriggerCondition condition = TriggerCondition::OnHit;
    std::string eventFilter;         // Filter specific events (e.g., ability name)
    float threshold = 0.0f;          // Threshold for health-based triggers

    // Probability
    float chance = 1.0f;             // Probability to trigger (0-1)

    // Cooldown
    float cooldown = 0.0f;           // Minimum time between triggers
    int maxTriggersPerCombat = -1;   // Max triggers per combat (-1 = unlimited)
    int maxTriggersPerEffect = -1;   // Max triggers per effect duration

    // Action
    TriggerAction action = TriggerAction::ApplyEffect;
    TriggerTarget target = TriggerTarget::Self;

    // Action parameters
    std::string effectId;            // Effect to apply/remove
    float amount = 0.0f;             // Amount for damage/heal/duration
    int stacks = 1;                  // Stacks to add/remove
    float range = 10.0f;             // Range for area targets

    // Custom script
    std::string scriptPath;
    std::string functionName;

    // Runtime tracking (not serialized)
    mutable float lastTriggerTime = 0.0f;
    mutable int triggerCount = 0;
    mutable int combatTriggerCount = 0;

    // -------------------------------------------------------------------------
    // Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Check if trigger can fire (cooldown, max triggers)
     */
    bool CanTrigger(float currentTime) const;

    /**
     * @brief Mark trigger as fired
     */
    void OnTriggered(float currentTime);

    /**
     * @brief Reset combat trigger count
     */
    void ResetCombatTriggers();

    /**
     * @brief Reset all trigger counts
     */
    void Reset();

    /**
     * @brief Check if this trigger matches the given event
     */
    bool MatchesEvent(TriggerCondition eventType, const std::string& eventData = "") const;

    /**
     * @brief Roll for trigger chance
     */
    bool RollChance() const;

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Trigger Event Data
// ============================================================================

/**
 * @brief Data passed to trigger evaluation
 */
struct TriggerEventData {
    TriggerCondition eventType = TriggerCondition::OnHit;
    uint32_t sourceId = 0;           // Who caused the event
    uint32_t targetId = 0;           // Who was affected
    float amount = 0.0f;             // Damage/heal amount
    bool isCritical = false;
    bool isKill = false;
    std::string abilityId;           // Ability that caused event
    std::string effectId;            // Effect that caused event
    float currentHealth = 0.0f;
    float maxHealth = 0.0f;
    float currentTime = 0.0f;

    [[nodiscard]] float GetHealthPercent() const {
        return maxHealth > 0.0f ? (currentHealth / maxHealth * 100.0f) : 0.0f;
    }
};

// ============================================================================
// Trigger System Interface
// ============================================================================

/**
 * @brief Interface for systems that can handle trigger events
 */
class ITriggerHandler {
public:
    virtual ~ITriggerHandler() = default;

    /**
     * @brief Process a trigger activation
     */
    virtual void HandleTrigger(
        const EffectTrigger& trigger,
        const TriggerEventData& eventData,
        EffectInstance* sourceEffect
    ) = 0;

    /**
     * @brief Get entity by ID for targeting
     */
    virtual uint32_t ResolveTarget(TriggerTarget targetType, const TriggerEventData& eventData) = 0;
};

// ============================================================================
// Trigger Builder (Fluent API)
// ============================================================================

/**
 * @brief Builder for creating triggers with fluent syntax
 */
class TriggerBuilder {
public:
    TriggerBuilder& When(TriggerCondition condition);
    TriggerBuilder& Filter(const std::string& filter);
    TriggerBuilder& Threshold(float value);
    TriggerBuilder& Chance(float probability);
    TriggerBuilder& Cooldown(float seconds);
    TriggerBuilder& MaxPerCombat(int count);
    TriggerBuilder& MaxTotal(int count);
    TriggerBuilder& Action(TriggerAction action);
    TriggerBuilder& Target(TriggerTarget target);
    TriggerBuilder& ApplyEffect(const std::string& effectId);
    TriggerBuilder& RemoveEffect(const std::string& effectId);
    TriggerBuilder& Amount(float value);
    TriggerBuilder& Stacks(int count);
    TriggerBuilder& Range(float radius);
    TriggerBuilder& Script(const std::string& path, const std::string& func = "on_trigger");

    EffectTrigger Build() const { return m_trigger; }
    operator EffectTrigger() const { return m_trigger; }

private:
    EffectTrigger m_trigger;
};

// Convenience function
inline TriggerBuilder Trigger() { return TriggerBuilder(); }

} // namespace Effects
} // namespace Vehement
