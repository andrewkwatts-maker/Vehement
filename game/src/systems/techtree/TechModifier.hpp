#pragma once

/**
 * @file TechModifier.hpp
 * @brief Stat modifier system for technology effects
 *
 * Provides a comprehensive modifier system that applies stat changes
 * from researched technologies to units, buildings, and global stats.
 *
 * Supports:
 * - Flat, Percent, and Multiplicative modifier types
 * - Global, UnitType, BuildingType, and Specific scopes
 * - Conditional modifiers (when_in_combat, when_near_building, etc.)
 * - Stacking rules for combining multiple modifiers
 * - JSON serialization for config-driven modifiers
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace TechTree {

// ============================================================================
// Modifier Types
// ============================================================================

/**
 * @brief How the modifier value is applied to the base stat
 */
enum class ModifierType : uint8_t {
    Flat,           ///< Add a flat value: base + value (e.g., +50 health)
    Percent,        ///< Add percentage of base: base * (1 + value/100) (e.g., +15% damage)
    Multiplicative, ///< Multiply base value: base * value (e.g., 1.2x speed)
    Override,       ///< Replace the stat entirely with this value
    Max,            ///< Set a maximum cap for the stat
    Min,            ///< Set a minimum floor for the stat

    COUNT
};

/**
 * @brief Convert ModifierType to string
 */
[[nodiscard]] inline const char* ModifierTypeToString(ModifierType type) {
    switch (type) {
        case ModifierType::Flat:           return "flat";
        case ModifierType::Percent:        return "percent";
        case ModifierType::Multiplicative: return "multiplicative";
        case ModifierType::Override:       return "override";
        case ModifierType::Max:            return "max";
        case ModifierType::Min:            return "min";
        default:                           return "unknown";
    }
}

/**
 * @brief Parse ModifierType from string
 */
[[nodiscard]] inline ModifierType StringToModifierType(const std::string& str) {
    if (str == "flat")           return ModifierType::Flat;
    if (str == "percent")        return ModifierType::Percent;
    if (str == "multiplicative") return ModifierType::Multiplicative;
    if (str == "override")       return ModifierType::Override;
    if (str == "max")            return ModifierType::Max;
    if (str == "min")            return ModifierType::Min;
    return ModifierType::Flat;
}

// ============================================================================
// Target Scope
// ============================================================================

/**
 * @brief Scope of what the modifier affects
 */
enum class TargetScopeType : uint8_t {
    Global,         ///< Affects all entities of any type
    UnitType,       ///< Affects units of a specific type (e.g., "melee", "ranged")
    BuildingType,   ///< Affects buildings of a specific type
    Specific,       ///< Affects a specific entity by ID
    Category,       ///< Affects entities with a specific tag/category
    Faction,        ///< Affects all entities of a faction
    Self,           ///< Affects only the source entity

    COUNT
};

[[nodiscard]] inline const char* TargetScopeTypeToString(TargetScopeType scope) {
    switch (scope) {
        case TargetScopeType::Global:       return "global";
        case TargetScopeType::UnitType:     return "unit_type";
        case TargetScopeType::BuildingType: return "building_type";
        case TargetScopeType::Specific:     return "specific";
        case TargetScopeType::Category:     return "category";
        case TargetScopeType::Faction:      return "faction";
        case TargetScopeType::Self:         return "self";
        default:                            return "unknown";
    }
}

[[nodiscard]] inline TargetScopeType StringToTargetScopeType(const std::string& str) {
    if (str == "global")        return TargetScopeType::Global;
    if (str == "unit_type")     return TargetScopeType::UnitType;
    if (str == "building_type") return TargetScopeType::BuildingType;
    if (str == "specific")      return TargetScopeType::Specific;
    if (str == "category")      return TargetScopeType::Category;
    if (str == "faction")       return TargetScopeType::Faction;
    if (str == "self")          return TargetScopeType::Self;
    return TargetScopeType::Global;
}

/**
 * @brief Complete target scope specification
 */
struct TargetScope {
    TargetScopeType type = TargetScopeType::Global;
    std::string target;             ///< Type name, ID, category, or faction name
    std::vector<std::string> tags;  ///< Additional tags to match (AND logic)
    bool excludeHeroes = false;     ///< Exclude hero units from this modifier

    [[nodiscard]] bool IsGlobal() const { return type == TargetScopeType::Global; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TargetScope FromJson(const nlohmann::json& j);

    // Factory methods
    [[nodiscard]] static TargetScope Global() {
        return TargetScope{TargetScopeType::Global, "", {}, false};
    }

    [[nodiscard]] static TargetScope UnitType(const std::string& unitType) {
        return TargetScope{TargetScopeType::UnitType, unitType, {}, false};
    }

    [[nodiscard]] static TargetScope BuildingType(const std::string& buildingType) {
        return TargetScope{TargetScopeType::BuildingType, buildingType, {}, false};
    }

    [[nodiscard]] static TargetScope Specific(const std::string& entityId) {
        return TargetScope{TargetScopeType::Specific, entityId, {}, false};
    }

    [[nodiscard]] static TargetScope Category(const std::string& category) {
        return TargetScope{TargetScopeType::Category, category, {}, false};
    }
};

// ============================================================================
// Conditions
// ============================================================================

/**
 * @brief Types of conditions that can activate/deactivate modifiers
 */
enum class ConditionType : uint8_t {
    Always,             ///< Always active
    WhenInCombat,       ///< Active when entity is in combat
    WhenNotInCombat,    ///< Active when entity is not in combat
    WhenNearBuilding,   ///< Active when near a specific building type
    WhenNearUnit,       ///< Active when near a specific unit type
    WhenHealthBelow,    ///< Active when health is below threshold (0.0-1.0)
    WhenHealthAbove,    ///< Active when health is above threshold
    WhenDay,            ///< Active during daytime
    WhenNight,          ///< Active during nighttime
    WhenInTerritory,    ///< Active in owned territory
    WhenInEnemyTerritory, ///< Active in enemy territory
    WhenGarrisoned,     ///< Active when unit is garrisoned
    WhenMoving,         ///< Active when entity is moving
    WhenStationary,     ///< Active when entity is not moving
    WhenBuffed,         ///< Active when entity has a specific buff
    WhenDebuffed,       ///< Active when entity has a specific debuff
    Custom,             ///< Custom script-based condition

    COUNT
};

[[nodiscard]] inline const char* ConditionTypeToString(ConditionType condition) {
    switch (condition) {
        case ConditionType::Always:              return "always";
        case ConditionType::WhenInCombat:        return "when_in_combat";
        case ConditionType::WhenNotInCombat:     return "when_not_in_combat";
        case ConditionType::WhenNearBuilding:    return "when_near_building";
        case ConditionType::WhenNearUnit:        return "when_near_unit";
        case ConditionType::WhenHealthBelow:     return "when_health_below";
        case ConditionType::WhenHealthAbove:     return "when_health_above";
        case ConditionType::WhenDay:             return "when_day";
        case ConditionType::WhenNight:           return "when_night";
        case ConditionType::WhenInTerritory:     return "when_in_territory";
        case ConditionType::WhenInEnemyTerritory: return "when_in_enemy_territory";
        case ConditionType::WhenGarrisoned:      return "when_garrisoned";
        case ConditionType::WhenMoving:          return "when_moving";
        case ConditionType::WhenStationary:      return "when_stationary";
        case ConditionType::WhenBuffed:          return "when_buffed";
        case ConditionType::WhenDebuffed:        return "when_debuffed";
        case ConditionType::Custom:              return "custom";
        default:                                 return "unknown";
    }
}

[[nodiscard]] inline ConditionType StringToConditionType(const std::string& str) {
    if (str == "always")               return ConditionType::Always;
    if (str == "when_in_combat")       return ConditionType::WhenInCombat;
    if (str == "when_not_in_combat")   return ConditionType::WhenNotInCombat;
    if (str == "when_near_building")   return ConditionType::WhenNearBuilding;
    if (str == "when_near_unit")       return ConditionType::WhenNearUnit;
    if (str == "when_health_below")    return ConditionType::WhenHealthBelow;
    if (str == "when_health_above")    return ConditionType::WhenHealthAbove;
    if (str == "when_day")             return ConditionType::WhenDay;
    if (str == "when_night")           return ConditionType::WhenNight;
    if (str == "when_in_territory")    return ConditionType::WhenInTerritory;
    if (str == "when_in_enemy_territory") return ConditionType::WhenInEnemyTerritory;
    if (str == "when_garrisoned")      return ConditionType::WhenGarrisoned;
    if (str == "when_moving")          return ConditionType::WhenMoving;
    if (str == "when_stationary")      return ConditionType::WhenStationary;
    if (str == "when_buffed")          return ConditionType::WhenBuffed;
    if (str == "when_debuffed")        return ConditionType::WhenDebuffed;
    if (str == "custom")               return ConditionType::Custom;
    return ConditionType::Always;
}

/**
 * @brief Complete condition specification
 */
struct ModifierCondition {
    ConditionType type = ConditionType::Always;
    float threshold = 0.0f;         ///< Threshold value for health/distance conditions
    std::string target;             ///< Building/unit type for proximity conditions
    float radius = 10.0f;           ///< Radius for proximity checks
    std::string scriptPath;         ///< Script path for custom conditions
    std::string buffDebuffId;       ///< Buff/debuff ID for when_buffed/when_debuffed
    bool invert = false;            ///< Invert the condition result

    [[nodiscard]] bool IsAlways() const { return type == ConditionType::Always; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static ModifierCondition FromJson(const nlohmann::json& j);

    // Factory methods
    [[nodiscard]] static ModifierCondition Always() {
        return ModifierCondition{ConditionType::Always};
    }

    [[nodiscard]] static ModifierCondition WhenInCombat() {
        return ModifierCondition{ConditionType::WhenInCombat};
    }

    [[nodiscard]] static ModifierCondition WhenHealthBelow(float threshold) {
        ModifierCondition c{ConditionType::WhenHealthBelow};
        c.threshold = threshold;
        return c;
    }

    [[nodiscard]] static ModifierCondition WhenNearBuilding(const std::string& buildingType, float radius = 10.0f) {
        ModifierCondition c{ConditionType::WhenNearBuilding};
        c.target = buildingType;
        c.radius = radius;
        return c;
    }
};

// ============================================================================
// Stacking Rules
// ============================================================================

/**
 * @brief How modifiers of the same type stack with each other
 */
enum class StackingRule : uint8_t {
    Additive,       ///< Stack additively (sum all values)
    Multiplicative, ///< Stack multiplicatively (multiply all values)
    Highest,        ///< Only use highest value
    Lowest,         ///< Only use lowest value
    Average,        ///< Use average of all values
    None,           ///< Does not stack (first one wins)
    Capped,         ///< Additive but with a maximum cap

    COUNT
};

[[nodiscard]] inline const char* StackingRuleToString(StackingRule rule) {
    switch (rule) {
        case StackingRule::Additive:       return "additive";
        case StackingRule::Multiplicative: return "multiplicative";
        case StackingRule::Highest:        return "highest";
        case StackingRule::Lowest:         return "lowest";
        case StackingRule::Average:        return "average";
        case StackingRule::None:           return "none";
        case StackingRule::Capped:         return "capped";
        default:                           return "unknown";
    }
}

[[nodiscard]] inline StackingRule StringToStackingRule(const std::string& str) {
    if (str == "additive")       return StackingRule::Additive;
    if (str == "multiplicative") return StackingRule::Multiplicative;
    if (str == "highest")        return StackingRule::Highest;
    if (str == "lowest")         return StackingRule::Lowest;
    if (str == "average")        return StackingRule::Average;
    if (str == "none")           return StackingRule::None;
    if (str == "capped")         return StackingRule::Capped;
    return StackingRule::Additive;
}

// ============================================================================
// Stat Names
// ============================================================================

/**
 * @brief Common stat names for modifiers
 */
namespace Stats {
    // Health and survivability
    constexpr const char* HEALTH = "health";
    constexpr const char* MAX_HEALTH = "max_health";
    constexpr const char* ARMOR = "armor";
    constexpr const char* MAGIC_RESISTANCE = "magic_resistance";
    constexpr const char* SHIELD = "shield";
    constexpr const char* HEALTH_REGEN = "health_regen";

    // Damage
    constexpr const char* DAMAGE = "damage";
    constexpr const char* ATTACK_DAMAGE = "attack_damage";
    constexpr const char* ABILITY_DAMAGE = "ability_damage";
    constexpr const char* CRIT_CHANCE = "crit_chance";
    constexpr const char* CRIT_DAMAGE = "crit_damage";
    constexpr const char* ARMOR_PENETRATION = "armor_penetration";

    // Combat
    constexpr const char* ATTACK_SPEED = "attack_speed";
    constexpr const char* ATTACK_RANGE = "attack_range";
    constexpr const char* ACCURACY = "accuracy";
    constexpr const char* EVASION = "evasion";

    // Movement
    constexpr const char* SPEED = "speed";
    constexpr const char* MOVEMENT_SPEED = "movement_speed";
    constexpr const char* TURN_RATE = "turn_rate";

    // Resources and economy
    constexpr const char* COST = "cost";
    constexpr const char* BUILD_TIME = "build_time";
    constexpr const char* TRAIN_TIME = "train_time";
    constexpr const char* RESEARCH_TIME = "research_time";
    constexpr const char* RESOURCE_COST = "resource_cost";
    constexpr const char* UPKEEP = "upkeep";

    // Production
    constexpr const char* FOOD_PRODUCTION = "food_production";
    constexpr const char* WOOD_PRODUCTION = "wood_production";
    constexpr const char* STONE_PRODUCTION = "stone_production";
    constexpr const char* METAL_PRODUCTION = "metal_production";
    constexpr const char* GOLD_PRODUCTION = "gold_production";
    constexpr const char* GATHER_SPEED = "gather_speed";
    constexpr const char* CARRY_CAPACITY = "carry_capacity";

    // Buildings
    constexpr const char* CONSTRUCTION_SPEED = "construction_speed";
    constexpr const char* REPAIR_SPEED = "repair_speed";
    constexpr const char* GARRISON_CAPACITY = "garrison_capacity";

    // Vision
    constexpr const char* VISION_RANGE = "vision_range";
    constexpr const char* DETECTION_RANGE = "detection_range";

    // Experience
    constexpr const char* XP_GAIN = "xp_gain";
    constexpr const char* XP_REQUIRED = "xp_required";

    // Special
    constexpr const char* COOLDOWN_REDUCTION = "cooldown_reduction";
    constexpr const char* POPULATION_COST = "population_cost";
    constexpr const char* SUPPLY_LIMIT = "supply_limit";
}

// ============================================================================
// Tech Modifier
// ============================================================================

/**
 * @brief Complete stat modifier definition
 *
 * A TechModifier represents a single stat change that can be applied
 * from a technology, ability, buff, or other source.
 *
 * Example JSON:
 * @code
 * {
 *   "stat": "damage",
 *   "type": "percent",
 *   "value": 15,
 *   "scope": {"type": "unit_type", "target": "melee"},
 *   "condition": {"type": "when_in_combat"},
 *   "stacking": "additive",
 *   "priority": 100
 * }
 * @endcode
 */
struct TechModifier {
    // Identity
    std::string id;                     ///< Unique identifier for this modifier
    std::string sourceId;               ///< ID of the tech/ability that grants this
    std::string description;            ///< Human-readable description

    // What stat to modify
    std::string stat;                   ///< Stat name to modify (from Stats namespace)
    ModifierType type = ModifierType::Percent;
    float value = 0.0f;                 ///< Modifier value

    // What to apply to
    TargetScope scope;

    // When to apply
    ModifierCondition condition;

    // Stacking behavior
    StackingRule stacking = StackingRule::Additive;
    float stackCap = 0.0f;              ///< Maximum stacked value (for Capped rule)
    int maxStacks = 0;                  ///< Maximum number of stacks (0 = unlimited)

    // Priority (higher = applied later in the calculation chain)
    int priority = 100;

    // Duration (0 = permanent until tech is lost)
    float duration = 0.0f;              ///< Duration in seconds (0 = permanent)
    bool isPermanent = true;            ///< Whether this is a permanent modifier

    // Tags for filtering
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    /**
     * @brief Check if this modifier applies to a given entity
     * @param entityType Type of entity (e.g., "melee", "building_barracks")
     * @param entityTags Tags on the entity
     * @param entityId Specific entity ID
     */
    [[nodiscard]] bool AppliesToEntity(const std::string& entityType,
                                        const std::vector<std::string>& entityTags,
                                        const std::string& entityId = "") const;

    /**
     * @brief Calculate the modified value
     * @param baseValue Original stat value
     * @return Modified value
     */
    [[nodiscard]] float Apply(float baseValue) const;

    /**
     * @brief Get display string for the modifier
     * @return String like "+15% damage" or "+50 health"
     */
    [[nodiscard]] std::string GetDisplayString() const;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static TechModifier FromJson(const nlohmann::json& j);

    // =========================================================================
    // Factory Methods
    // =========================================================================

    [[nodiscard]] static TechModifier FlatBonus(const std::string& stat, float value,
                                                 TargetScope scope = TargetScope::Global());

    [[nodiscard]] static TechModifier PercentBonus(const std::string& stat, float percent,
                                                    TargetScope scope = TargetScope::Global());

    [[nodiscard]] static TechModifier Multiplier(const std::string& stat, float multiplier,
                                                  TargetScope scope = TargetScope::Global());
};

// ============================================================================
// Modifier Stack
// ============================================================================

/**
 * @brief Manages a collection of modifiers for a single stat
 *
 * Handles proper calculation order and stacking rules for combining
 * multiple modifiers affecting the same stat.
 */
class ModifierStack {
public:
    /**
     * @brief Add a modifier to the stack
     */
    void AddModifier(const TechModifier& modifier);

    /**
     * @brief Remove a modifier by ID
     */
    bool RemoveModifier(const std::string& modifierId);

    /**
     * @brief Remove all modifiers from a source
     */
    void RemoveModifiersFromSource(const std::string& sourceId);

    /**
     * @brief Clear all modifiers
     */
    void Clear();

    /**
     * @brief Calculate the final modified value
     * @param baseValue Original stat value
     * @param entityType Entity type for scope filtering
     * @param entityTags Entity tags for scope filtering
     * @param entityId Entity ID for specific scopes
     * @return Final calculated value
     */
    [[nodiscard]] float Calculate(float baseValue,
                                   const std::string& entityType = "",
                                   const std::vector<std::string>& entityTags = {},
                                   const std::string& entityId = "") const;

    /**
     * @brief Get all active modifiers
     */
    [[nodiscard]] const std::vector<TechModifier>& GetModifiers() const { return m_modifiers; }

    /**
     * @brief Get number of modifiers
     */
    [[nodiscard]] size_t GetModifierCount() const { return m_modifiers.size(); }

    /**
     * @brief Check if stack has any modifiers
     */
    [[nodiscard]] bool IsEmpty() const { return m_modifiers.empty(); }

    /**
     * @brief Get breakdown of how the value is calculated
     */
    [[nodiscard]] std::string GetCalculationBreakdown(float baseValue) const;

private:
    std::vector<TechModifier> m_modifiers;
    mutable bool m_dirty = true;
    mutable std::vector<const TechModifier*> m_sortedModifiers;

    void SortModifiers() const;
};

// ============================================================================
// Modifier Collection
// ============================================================================

/**
 * @brief Manages all modifiers for multiple stats
 */
class ModifierCollection {
public:
    /**
     * @brief Add a modifier
     */
    void AddModifier(const TechModifier& modifier);

    /**
     * @brief Remove a modifier by ID
     */
    bool RemoveModifier(const std::string& modifierId);

    /**
     * @brief Remove all modifiers from a source (e.g., when tech is lost)
     */
    void RemoveModifiersFromSource(const std::string& sourceId);

    /**
     * @brief Clear all modifiers
     */
    void Clear();

    /**
     * @brief Get modified value for a stat
     * @param stat Stat name
     * @param baseValue Original value
     * @param entityType Entity type for scope filtering
     * @param entityTags Entity tags for scope filtering
     * @param entityId Entity ID for specific scopes
     */
    [[nodiscard]] float GetModifiedValue(const std::string& stat, float baseValue,
                                          const std::string& entityType = "",
                                          const std::vector<std::string>& entityTags = {},
                                          const std::string& entityId = "") const;

    /**
     * @brief Get total flat bonus for a stat
     */
    [[nodiscard]] float GetFlatBonus(const std::string& stat) const;

    /**
     * @brief Get total percent bonus for a stat
     */
    [[nodiscard]] float GetPercentBonus(const std::string& stat) const;

    /**
     * @brief Get total multiplier for a stat
     */
    [[nodiscard]] float GetMultiplier(const std::string& stat) const;

    /**
     * @brief Check if any modifier affects a stat
     */
    [[nodiscard]] bool HasModifiersForStat(const std::string& stat) const;

    /**
     * @brief Get all modified stats
     */
    [[nodiscard]] std::vector<std::string> GetModifiedStats() const;

    /**
     * @brief Get all modifiers
     */
    [[nodiscard]] std::vector<TechModifier> GetAllModifiers() const;

    /**
     * @brief Get modifiers for a specific stat
     */
    [[nodiscard]] const ModifierStack* GetStackForStat(const std::string& stat) const;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    void FromJson(const nlohmann::json& j);

private:
    std::unordered_map<std::string, ModifierStack> m_stacks;
};

// ============================================================================
// Condition Evaluator
// ============================================================================

/**
 * @brief Interface for evaluating modifier conditions at runtime
 */
class IConditionEvaluator {
public:
    virtual ~IConditionEvaluator() = default;

    /**
     * @brief Evaluate if a condition is met
     * @param condition The condition to evaluate
     * @param entityId Entity to check the condition for
     * @return true if condition is met
     */
    [[nodiscard]] virtual bool Evaluate(const ModifierCondition& condition,
                                        const std::string& entityId) const = 0;

    /**
     * @brief Check if entity is in combat
     */
    [[nodiscard]] virtual bool IsInCombat(const std::string& entityId) const = 0;

    /**
     * @brief Get entity health percentage (0.0-1.0)
     */
    [[nodiscard]] virtual float GetHealthPercent(const std::string& entityId) const = 0;

    /**
     * @brief Check if entity is near a building type
     */
    [[nodiscard]] virtual bool IsNearBuilding(const std::string& entityId,
                                              const std::string& buildingType,
                                              float radius) const = 0;

    /**
     * @brief Check if entity is near a unit type
     */
    [[nodiscard]] virtual bool IsNearUnit(const std::string& entityId,
                                          const std::string& unitType,
                                          float radius) const = 0;

    /**
     * @brief Check if it's daytime
     */
    [[nodiscard]] virtual bool IsDaytime() const = 0;

    /**
     * @brief Check if entity is in owned territory
     */
    [[nodiscard]] virtual bool IsInOwnTerritory(const std::string& entityId) const = 0;

    /**
     * @brief Check if entity is garrisoned
     */
    [[nodiscard]] virtual bool IsGarrisoned(const std::string& entityId) const = 0;

    /**
     * @brief Check if entity is moving
     */
    [[nodiscard]] virtual bool IsMoving(const std::string& entityId) const = 0;

    /**
     * @brief Check if entity has a buff
     */
    [[nodiscard]] virtual bool HasBuff(const std::string& entityId,
                                       const std::string& buffId) const = 0;

    /**
     * @brief Check if entity has a debuff
     */
    [[nodiscard]] virtual bool HasDebuff(const std::string& entityId,
                                         const std::string& debuffId) const = 0;

    /**
     * @brief Evaluate a custom script condition
     */
    [[nodiscard]] virtual bool EvaluateCustomCondition(const std::string& scriptPath,
                                                        const std::string& entityId) const = 0;
};

/**
 * @brief Default condition evaluator implementation
 *
 * Provides stub implementations that can be overridden for game-specific logic.
 */
class DefaultConditionEvaluator : public IConditionEvaluator {
public:
    [[nodiscard]] bool Evaluate(const ModifierCondition& condition,
                                const std::string& entityId) const override;

    [[nodiscard]] bool IsInCombat(const std::string& entityId) const override { return false; }
    [[nodiscard]] float GetHealthPercent(const std::string& entityId) const override { return 1.0f; }
    [[nodiscard]] bool IsNearBuilding(const std::string& entityId,
                                      const std::string& buildingType,
                                      float radius) const override { return false; }
    [[nodiscard]] bool IsNearUnit(const std::string& entityId,
                                  const std::string& unitType,
                                  float radius) const override { return false; }
    [[nodiscard]] bool IsDaytime() const override { return true; }
    [[nodiscard]] bool IsInOwnTerritory(const std::string& entityId) const override { return true; }
    [[nodiscard]] bool IsGarrisoned(const std::string& entityId) const override { return false; }
    [[nodiscard]] bool IsMoving(const std::string& entityId) const override { return false; }
    [[nodiscard]] bool HasBuff(const std::string& entityId,
                               const std::string& buffId) const override { return false; }
    [[nodiscard]] bool HasDebuff(const std::string& entityId,
                                 const std::string& debuffId) const override { return false; }
    [[nodiscard]] bool EvaluateCustomCondition(const std::string& scriptPath,
                                                const std::string& entityId) const override { return true; }
};

} // namespace TechTree
} // namespace Vehement
