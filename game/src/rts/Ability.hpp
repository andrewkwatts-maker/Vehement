#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <cstdint>

namespace Vehement {

// Forward declarations
class Entity;

namespace RTS {

// Forward declarations
class Hero;

/**
 * @brief Types of abilities
 */
enum class AbilityType : uint8_t {
    Passive,    // Always active, no activation needed
    Active,     // Requires activation, instant effect
    Toggle,     // Can be turned on/off, drains mana while active
    Channeled   // Must hold to maintain, interrupted by damage
};

/**
 * @brief Targeting modes for abilities
 */
enum class TargetType : uint8_t {
    None,       // No target needed (self-cast or aura)
    Point,      // Target ground location
    Unit,       // Target single unit
    Area,       // Area of effect at point
    Direction,  // Cast in a direction (line abilities)
    Cone        // Cone in direction (cleave, breath)
};

/**
 * @brief Effect types for abilities
 */
enum class AbilityEffect : uint8_t {
    Damage,         // Deal damage
    Heal,           // Restore health
    Buff,           // Apply positive status
    Debuff,         // Apply negative status
    Summon,         // Create units
    Teleport,       // Move instantly
    Knockback,      // Push targets
    Stun,           // Prevent actions
    Slow,           // Reduce speed
    Silence,        // Prevent abilities
    Shield,         // Absorb damage
    Stealth,        // Become invisible
    Detection,      // Reveal hidden units
    ResourceGain,   // Generate resources

    COUNT
};

/**
 * @brief Status effects that can be applied by abilities
 */
enum class StatusEffect : uint8_t {
    None,

    // Positive (Buffs)
    Haste,          // Increased move speed
    Might,          // Increased damage
    Fortified,      // Increased armor
    Regeneration,   // Health over time
    Shield,         // Damage absorption
    Inspired,       // All stats boost
    Invisible,      // Cannot be seen

    // Negative (Debuffs)
    Slowed,         // Reduced move speed
    Weakened,       // Reduced damage
    Vulnerable,     // Reduced armor
    Burning,        // Damage over time
    Frozen,         // Cannot move
    Stunned,        // Cannot act
    Silenced,       // Cannot use abilities
    Revealed,       // Cannot stealth

    COUNT
};

/**
 * @brief Data structure for ability statistics per level
 */
struct AbilityLevelData {
    float damage = 0.0f;            // Base damage/healing
    float duration = 0.0f;          // Effect duration in seconds
    float radius = 0.0f;            // AoE radius (if applicable)
    float manaCost = 0.0f;          // Mana cost
    float cooldown = 0.0f;          // Cooldown in seconds
    float range = 0.0f;             // Cast range
    float effectStrength = 0.0f;    // Status effect strength (slow %, etc.)
    int summonCount = 0;            // Number of summons (if applicable)
};

/**
 * @brief Complete ability definition
 */
struct AbilityData {
    // Identification
    int id = -1;
    std::string name;
    std::string description;
    std::string iconPath;

    // Type and targeting
    AbilityType type = AbilityType::Active;
    TargetType targetType = TargetType::None;
    std::vector<AbilityEffect> effects;
    StatusEffect appliesStatus = StatusEffect::None;

    // Requirements
    int requiredLevel = 1;          // Hero level to unlock
    int maxLevel = 4;               // Maximum ability level
    bool requiresTarget = false;    // Must have valid target
    bool canTargetSelf = true;      // Can target caster
    bool canTargetAlly = true;      // Can target friendly units
    bool canTargetEnemy = true;     // Can target enemy units
    bool canTargetGround = false;   // Can target terrain

    // Stats per level (index 0 = level 1, etc.)
    std::vector<AbilityLevelData> levelData;

    // Audio/Visual
    std::string castSound;
    std::string impactSound;
    std::string castEffect;
    std::string impactEffect;

    /**
     * @brief Get data for a specific ability level
     */
    const AbilityLevelData& GetLevelData(int level) const {
        int idx = std::min(level - 1, static_cast<int>(levelData.size()) - 1);
        return levelData[std::max(0, idx)];
    }
};

/**
 * @brief Runtime state for an ability instance
 */
struct AbilityState {
    int abilityId = -1;
    int currentLevel = 0;           // 0 = not learned
    float cooldownRemaining = 0.0f;
    bool isToggled = false;         // For toggle abilities
    bool isChanneling = false;      // For channeled abilities
    float channelTimeRemaining = 0.0f;

    [[nodiscard]] bool IsReady() const { return cooldownRemaining <= 0.0f && currentLevel > 0; }
    [[nodiscard]] bool IsLearned() const { return currentLevel > 0; }
    [[nodiscard]] bool IsMaxLevel(const AbilityData& data) const { return currentLevel >= data.maxLevel; }
};

/**
 * @brief Result of an ability cast
 */
struct AbilityCastResult {
    bool success = false;
    std::string failReason;
    float damageDealt = 0.0f;
    float healingDone = 0.0f;
    int unitsAffected = 0;
    std::vector<uint32_t> affectedEntities;
};

/**
 * @brief Context passed to ability execution
 */
struct AbilityCastContext {
    Hero* caster = nullptr;
    glm::vec3 targetPoint{0.0f};
    Entity* targetUnit = nullptr;
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    int abilityLevel = 1;
    float deltaTime = 0.0f;         // For channeled/toggle abilities
};

/**
 * @brief Base class for ability behavior implementation
 */
class AbilityBehavior {
public:
    virtual ~AbilityBehavior() = default;

    /**
     * @brief Check if ability can be cast in current context
     */
    [[nodiscard]] virtual bool CanCast(const AbilityCastContext& context, const AbilityData& data) const;

    /**
     * @brief Execute the ability
     */
    virtual AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) = 0;

    /**
     * @brief Update for channeled/toggle abilities
     */
    virtual void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {}

    /**
     * @brief Called when ability ends (channel interrupted, toggle off)
     */
    virtual void OnEnd(const AbilityCastContext& context, const AbilityData& data) {}
};

/**
 * @brief Manages all ability definitions and instances
 */
class AbilityManager {
public:
    static AbilityManager& Instance() {
        static AbilityManager instance;
        return instance;
    }

    /**
     * @brief Initialize ability database
     */
    void Initialize();

    /**
     * @brief Get ability data by ID
     */
    const AbilityData* GetAbility(int id) const;

    /**
     * @brief Get ability behavior by ID
     */
    AbilityBehavior* GetBehavior(int id);

    /**
     * @brief Register a custom ability behavior
     */
    void RegisterBehavior(int abilityId, std::unique_ptr<AbilityBehavior> behavior);

    /**
     * @brief Get all abilities for a hero class
     */
    std::vector<const AbilityData*> GetAbilitiesForClass(int heroClassId) const;

    /**
     * @brief Get number of registered abilities
     */
    [[nodiscard]] size_t GetAbilityCount() const { return m_abilities.size(); }

private:
    AbilityManager() = default;
    void RegisterDefaultAbilities();

    std::vector<AbilityData> m_abilities;
    std::unordered_map<int, std::unique_ptr<AbilityBehavior>> m_behaviors;
};

// ============================================================================
// Built-in Ability Implementations
// ============================================================================

/**
 * @brief Rally - Warlord ability
 * Increases damage and attack speed of nearby allies
 */
class RallyAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Inspire - Commander ability
 * Grants movement speed and reduces ability cooldowns for allies
 */
class InspireAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Fortify - Engineer ability
 * Increases armor and health of targeted building/unit
 */
class FortifyAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Shadowstep - Scout ability
 * Teleport to target location, gain stealth briefly
 */
class ShadowstepAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Market Mastery - Merchant ability
 * Passive that increases gold from all sources
 */
class MarketMasteryAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Warcry - Ultimate area damage and stun
 */
class WarcryAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

// ============================================================================
// Ability Data Definitions (IDs)
// ============================================================================

namespace AbilityId {
    constexpr int RALLY = 0;
    constexpr int INSPIRE = 1;
    constexpr int FORTIFY = 2;
    constexpr int SHADOWSTEP = 3;
    constexpr int MARKET_MASTERY = 4;
    constexpr int WARCRY = 5;
    constexpr int REPAIR_AURA = 6;
    constexpr int DETECTION_WARD = 7;
    constexpr int TRADE_CARAVAN = 8;
    constexpr int BATTLE_STANDARD = 9;
}

} // namespace RTS
} // namespace Vehement
