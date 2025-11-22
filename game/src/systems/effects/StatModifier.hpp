#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>

namespace Vehement {
namespace Effects {

// ============================================================================
// Stat Type Enumeration
// ============================================================================

/**
 * @brief All modifiable stats in the game
 */
enum class StatType : uint8_t {
    // Primary Stats
    Health,
    MaxHealth,
    Mana,
    MaxMana,
    Stamina,
    MaxStamina,

    // Combat Stats
    Damage,
    PhysicalDamage,
    MagicDamage,
    FireDamage,
    IceDamage,
    LightningDamage,
    PoisonDamage,
    HolyDamage,
    DarkDamage,

    // Defense Stats
    Armor,
    PhysicalResist,
    MagicResist,
    FireResist,
    IceResist,
    LightningResist,
    PoisonResist,

    // Attack Stats
    AttackSpeed,
    CastSpeed,
    CritChance,
    CritMultiplier,
    Accuracy,
    ArmorPenetration,

    // Movement Stats
    MoveSpeed,
    JumpHeight,
    Dodge,

    // Resource Stats
    HealthRegen,
    ManaRegen,
    StaminaRegen,
    Lifesteal,
    ManaLeech,

    // Utility Stats
    CooldownReduction,
    Range,
    AreaOfEffect,
    Duration,
    ThreatModifier,

    // Experience/Resources
    ExperienceGain,
    GoldFind,
    LootChance,

    Count  // Total count for iteration
};

/**
 * @brief Convert stat type to string for serialization/display
 */
const char* StatTypeToString(StatType type);

/**
 * @brief Parse stat type from string
 */
std::optional<StatType> StatTypeFromString(const std::string& str);

// ============================================================================
// Modifier Operation
// ============================================================================

/**
 * @brief Type of mathematical operation for the modifier
 */
enum class ModifierOp : uint8_t {
    Add,        // +value (flat addition)
    Multiply,   // *value (multiplicative, 1.0 = no change)
    Percent,    // +(base * value/100) (percentage of base)
    Set,        // =value (override to specific value)
    Min,        // =max(current, value) (ensure minimum)
    Max         // =min(current, value) (ensure maximum)
};

/**
 * @brief Convert modifier op to string
 */
const char* ModifierOpToString(ModifierOp op);

/**
 * @brief Parse modifier op from string
 */
std::optional<ModifierOp> ModifierOpFromString(const std::string& str);

// ============================================================================
// Condition Type
// ============================================================================

/**
 * @brief Conditions for conditional modifiers
 */
enum class ConditionType : uint8_t {
    None,               // Always active
    HealthBelow,        // Health < X%
    HealthAbove,        // Health > X%
    ManaBelow,          // Mana < X%
    ManaAbove,          // Mana > X%
    StaminaBelow,       // Stamina < X%
    InCombat,           // Currently in combat
    OutOfCombat,        // Not in combat
    MovingSpeed,        // Moving faster than X
    Stationary,         // Not moving
    HasBuff,            // Has specific buff
    HasDebuff,          // Has specific debuff
    TargetHealthBelow,  // Target health < X%
    EnemiesNearby,      // X or more enemies nearby
    AlliesNearby,       // X or more allies nearby
    TimeOfDay,          // Specific time range
    IsStealthed,        // In stealth
    IsMounted,          // On mount
    HasFullHealth,      // At max health
    RecentlyDamaged,    // Took damage in last X seconds
    KilledRecently      // Killed enemy in last X seconds
};

/**
 * @brief Convert condition type to string
 */
const char* ConditionTypeToString(ConditionType type);

/**
 * @brief Parse condition type from string
 */
std::optional<ConditionType> ConditionTypeFromString(const std::string& str);

// ============================================================================
// Modifier Condition
// ============================================================================

/**
 * @brief A condition that must be met for a modifier to apply
 */
struct ModifierCondition {
    ConditionType type = ConditionType::None;
    float threshold = 0.0f;          // Threshold value (e.g., 50 for "below 50%")
    std::string parameter;           // Additional parameter (e.g., buff name)
    bool inverted = false;           // Invert the condition result

    /**
     * @brief Check if condition is met (requires context from entity)
     */
    bool Evaluate(const std::unordered_map<std::string, float>& context) const;

    /**
     * @brief Load from JSON object
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Stat Modifier
// ============================================================================

/**
 * @brief A single stat modification
 */
struct StatModifier {
    // Core properties
    StatType stat = StatType::Damage;
    ModifierOp operation = ModifierOp::Add;
    float value = 0.0f;

    // Priority for calculation order (higher = applied later)
    int priority = 0;

    // Source tracking
    uint32_t sourceId = 0;           // Effect instance that created this
    std::string sourceTag;           // Tag for grouping/filtering

    // Conditional
    std::optional<ModifierCondition> condition;

    // -------------------------------------------------------------------------
    // Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Apply this modifier to a value
     * @param baseValue The original base value
     * @param currentValue The current modified value
     * @return New value after applying this modifier
     */
    float Apply(float baseValue, float currentValue) const;

    /**
     * @brief Check if modifier should be applied (condition check)
     */
    bool ShouldApply(const std::unordered_map<std::string, float>& context) const;

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;

    // Comparison for sorting by priority
    bool operator<(const StatModifier& other) const {
        return priority < other.priority;
    }
};

// ============================================================================
// Stat Block
// ============================================================================

/**
 * @brief Container for all base stats of an entity
 */
class StatBlock {
public:
    StatBlock();
    ~StatBlock() = default;

    // -------------------------------------------------------------------------
    // Base Stats
    // -------------------------------------------------------------------------

    /**
     * @brief Get base value of a stat
     */
    float GetBaseStat(StatType stat) const;

    /**
     * @brief Set base value of a stat
     */
    void SetBaseStat(StatType stat, float value);

    /**
     * @brief Get all base stats as a map
     */
    const std::unordered_map<StatType, float>& GetAllBaseStats() const { return m_baseStats; }

    // -------------------------------------------------------------------------
    // Modified Stats
    // -------------------------------------------------------------------------

    /**
     * @brief Get final modified value of a stat
     */
    float GetFinalStat(StatType stat) const;

    /**
     * @brief Add a modifier to this stat block
     */
    void AddModifier(const StatModifier& modifier);

    /**
     * @brief Remove modifiers by source ID
     */
    void RemoveModifiersBySource(uint32_t sourceId);

    /**
     * @brief Remove modifiers by tag
     */
    void RemoveModifiersByTag(const std::string& tag);

    /**
     * @brief Remove all modifiers
     */
    void ClearAllModifiers();

    /**
     * @brief Get all active modifiers
     */
    const std::vector<StatModifier>& GetModifiers() const { return m_modifiers; }

    // -------------------------------------------------------------------------
    // Calculation
    // -------------------------------------------------------------------------

    /**
     * @brief Recalculate all final stats
     * @param context Context for conditional modifiers
     */
    void Recalculate(const std::unordered_map<std::string, float>& context = {});

    /**
     * @brief Mark stats as dirty (need recalculation)
     */
    void MarkDirty() { m_dirty = true; }

    /**
     * @brief Check if stats need recalculation
     */
    bool IsDirty() const { return m_dirty; }

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /**
     * @brief Load base stats from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize base stats to JSON
     */
    std::string ToJson() const;

private:
    void CalculateStat(StatType stat, const std::unordered_map<std::string, float>& context);

    std::unordered_map<StatType, float> m_baseStats;
    std::unordered_map<StatType, float> m_finalStats;
    std::vector<StatModifier> m_modifiers;
    bool m_dirty = true;
};

// ============================================================================
// Stat Modifier Builder (Fluent API)
// ============================================================================

/**
 * @brief Builder for creating stat modifiers with fluent syntax
 */
class StatModifierBuilder {
public:
    StatModifierBuilder& Stat(StatType stat);
    StatModifierBuilder& Operation(ModifierOp op);
    StatModifierBuilder& Value(float val);
    StatModifierBuilder& Priority(int pri);
    StatModifierBuilder& Source(uint32_t id);
    StatModifierBuilder& Tag(const std::string& tag);
    StatModifierBuilder& When(ConditionType condition, float threshold = 0.0f);
    StatModifierBuilder& WithParam(const std::string& param);
    StatModifierBuilder& Inverted(bool inv = true);

    StatModifier Build() const { return m_modifier; }
    operator StatModifier() const { return m_modifier; }

private:
    StatModifier m_modifier;
};

// Convenience function
inline StatModifierBuilder Modify() { return StatModifierBuilder(); }

} // namespace Effects
} // namespace Vehement
