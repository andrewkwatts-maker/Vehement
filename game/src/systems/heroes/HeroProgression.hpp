#pragma once

#include "HeroDefinition.hpp"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Vehement {
namespace Heroes {

// Forward declarations
class HeroInstance;

// ============================================================================
// Experience Sources
// ============================================================================

/**
 * @brief Sources of experience gain
 */
enum class ExperienceSource : uint8_t {
    HeroKill,       // Killing enemy hero
    CreepKill,      // Killing creeps/minions
    BossKill,       // Killing boss monsters
    BuildingKill,   // Destroying buildings
    Assist,         // Assisting in a kill
    Quest,          // Completing quests
    Objective,      // Taking objectives
    Passive,        // Passive XP over time
    Item,           // From items/consumables
    Script          // From scripted events
};

// ============================================================================
// XP Curve Configuration
// ============================================================================

/**
 * @brief Configuration for XP curve
 */
struct XPCurveConfig {
    // Base XP required for level 2
    int baseXP = 100;

    // XP growth per level (multiplier or additive)
    float growthRate = 1.2f;

    // True = exponential, False = linear growth
    bool exponential = false;

    // Maximum level
    int maxLevel = 30;

    // XP reduction for high level difference (when killing lower level enemies)
    float levelDifferenceReduction = 0.1f;

    // Minimum XP percentage from kills
    float minimumXPPercent = 0.1f;
};

/**
 * @brief XP gained from different sources
 */
struct XPRewardConfig {
    // Hero kill base XP (scaled by target level)
    int heroKillBase = 200;
    float heroKillPerLevel = 50.0f;

    // Creep kill XP
    int creepKillBase = 50;

    // Boss kill XP
    int bossKillBase = 500;

    // Building kill XP
    int buildingKillBase = 100;

    // Assist XP (percentage of kill XP)
    float assistPercent = 0.5f;

    // Passive XP per second
    float passiveXPPerSecond = 1.0f;

    // XP sharing radius for nearby allies
    float xpShareRadius = 15.0f;

    // XP sharing percentage for allies
    float xpSharePercent = 0.35f;
};

// ============================================================================
// Level Up Bonus
// ============================================================================

/**
 * @brief Bonuses gained when leveling up
 */
struct LevelUpBonus {
    int level = 0;

    // Ability points gained
    int abilityPoints = 1;

    // Stat points gained
    int statPoints = 0;

    // Attribute bonuses (from stat growth)
    float strengthGain = 0.0f;
    float agilityGain = 0.0f;
    float intelligenceGain = 0.0f;

    // Resource bonuses
    float maxHealthGain = 0.0f;
    float maxManaGain = 0.0f;

    // Talent unlock (0 = none)
    int talentTierUnlock = 0;

    // Ultimate ability unlock
    bool ultimateUnlock = false;
};

// ============================================================================
// Ability Point Distribution
// ============================================================================

/**
 * @brief Rules for ability point distribution
 */
struct AbilityPointRules {
    // Points gained per level
    int pointsPerLevel = 1;

    // Bonus points at certain levels
    std::vector<std::pair<int, int>> bonusPointLevels; // {level, bonus_points}

    // Maximum points in a single ability
    int maxPointsPerAbility = 4;

    // Ultimate ability restrictions
    int ultimateUnlockLevel = 6;
    int ultimateMaxLevel = 3;
    std::vector<int> ultimateLevelUpLevels; // Levels where ultimate can be upgraded

    // Level requirements for ability levels
    // ability_level -> required_hero_level
    std::unordered_map<int, int> abilityLevelRequirements;
};

// ============================================================================
// Attribute Gain Configuration
// ============================================================================

/**
 * @brief Configuration for attribute gains per level
 */
struct AttributeGainConfig {
    // Base gain per level (from hero definition)
    float strengthPerLevel = 2.5f;
    float agilityPerLevel = 1.5f;
    float intelligencePerLevel = 1.5f;

    // Bonus gain every X levels
    int bonusEveryNLevels = 5;
    float bonusStrength = 1.0f;
    float bonusAgility = 1.0f;
    float bonusIntelligence = 1.0f;

    // Maximum attributes
    float maxStrength = 200.0f;
    float maxAgility = 200.0f;
    float maxIntelligence = 200.0f;
};

// ============================================================================
// Hero Progression System
// ============================================================================

/**
 * @brief Manages hero leveling and experience
 *
 * Handles:
 * - XP curve calculation
 * - Level up processing
 * - Ability point distribution rules
 * - Attribute gains per level
 * - Talent unlock tracking
 */
class HeroProgression {
public:
    using LevelUpCallback = std::function<void(HeroInstance&, const LevelUpBonus&)>;
    using XPGainCallback = std::function<void(HeroInstance&, int amount, ExperienceSource source)>;

    HeroProgression();
    ~HeroProgression();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Load progression config from JSON
     */
    bool LoadConfig(const std::string& configPath);

    /**
     * @brief Set XP curve configuration
     */
    void SetXPCurve(const XPCurveConfig& config) { m_xpCurve = config; }
    [[nodiscard]] const XPCurveConfig& GetXPCurve() const { return m_xpCurve; }

    /**
     * @brief Set XP reward configuration
     */
    void SetXPRewards(const XPRewardConfig& config) { m_xpRewards = config; }
    [[nodiscard]] const XPRewardConfig& GetXPRewards() const { return m_xpRewards; }

    /**
     * @brief Set ability point rules
     */
    void SetAbilityPointRules(const AbilityPointRules& rules) { m_abilityRules = rules; }
    [[nodiscard]] const AbilityPointRules& GetAbilityPointRules() const { return m_abilityRules; }

    /**
     * @brief Set attribute gain configuration
     */
    void SetAttributeGainConfig(const AttributeGainConfig& config) { m_attrGainConfig = config; }
    [[nodiscard]] const AttributeGainConfig& GetAttributeGainConfig() const { return m_attrGainConfig; }

    // =========================================================================
    // XP Calculation
    // =========================================================================

    /**
     * @brief Calculate total XP required for a level
     * @param level Target level
     * @return Total XP needed to reach that level
     */
    [[nodiscard]] int CalculateXPForLevel(int level) const;

    /**
     * @brief Calculate XP needed from current level to next
     * @param currentLevel Current hero level
     * @return XP needed to level up
     */
    [[nodiscard]] int CalculateXPToNextLevel(int currentLevel) const;

    /**
     * @brief Calculate XP reward for a kill
     * @param killerLevel Level of killer
     * @param targetLevel Level of target
     * @param source Type of kill
     * @return XP reward
     */
    [[nodiscard]] int CalculateKillXP(int killerLevel, int targetLevel, ExperienceSource source) const;

    /**
     * @brief Calculate shared XP for nearby allies
     * @param baseXP Original XP amount
     * @param allyCount Number of allies sharing
     * @return XP per ally
     */
    [[nodiscard]] int CalculateSharedXP(int baseXP, int allyCount) const;

    /**
     * @brief Calculate level from total XP
     * @param totalXP Total experience
     * @return Level achieved
     */
    [[nodiscard]] int CalculateLevelFromXP(int totalXP) const;

    // =========================================================================
    // Level Up Processing
    // =========================================================================

    /**
     * @brief Process XP gain for hero
     * @param hero Hero instance
     * @param amount XP amount
     * @param source XP source
     * @return Number of levels gained
     */
    int ProcessXPGain(HeroInstance& hero, int amount, ExperienceSource source);

    /**
     * @brief Calculate level up bonus for a level
     * @param hero Hero instance
     * @param level New level
     * @return Level up bonus
     */
    [[nodiscard]] LevelUpBonus CalculateLevelUpBonus(const HeroInstance& hero, int level) const;

    /**
     * @brief Apply level up bonus to hero
     */
    void ApplyLevelUpBonus(HeroInstance& hero, const LevelUpBonus& bonus);

    // =========================================================================
    // Ability Point Distribution
    // =========================================================================

    /**
     * @brief Check if ability can be leveled
     * @param hero Hero instance
     * @param abilitySlot Ability slot (0-3)
     * @return true if can level up
     */
    [[nodiscard]] bool CanLevelAbility(const HeroInstance& hero, int abilitySlot) const;

    /**
     * @brief Get required hero level for ability level
     * @param abilityLevel Target ability level
     * @param isUltimate Whether this is ultimate ability
     * @return Required hero level
     */
    [[nodiscard]] int GetRequiredLevelForAbility(int abilityLevel, bool isUltimate) const;

    /**
     * @brief Calculate total ability points at level
     */
    [[nodiscard]] int GetTotalAbilityPointsAtLevel(int level) const;

    // =========================================================================
    // Attribute Gains
    // =========================================================================

    /**
     * @brief Calculate attribute gains at level
     * @param hero Hero instance
     * @param level Target level
     * @return Attribute values at that level
     */
    [[nodiscard]] std::tuple<float, float, float> CalculateAttributesAtLevel(
        const HeroInstance& hero, int level) const;

    /**
     * @brief Get bonus attributes from leveling
     * @param fromLevel Starting level
     * @param toLevel Target level
     * @param heroDefinition Hero definition for base growth
     * @return Tuple of (str, agi, int) gains
     */
    [[nodiscard]] std::tuple<float, float, float> GetAttributeGains(
        int fromLevel, int toLevel, const HeroDefinition& heroDefinition) const;

    // =========================================================================
    // Talent Unlocks
    // =========================================================================

    /**
     * @brief Get talent tier unlocked at level
     * @param level Hero level
     * @return Talent tier (0-3) or -1 if none
     */
    [[nodiscard]] int GetTalentTierAtLevel(int level) const;

    /**
     * @brief Check if level unlocks a talent tier
     */
    [[nodiscard]] bool LevelUnlocksTalent(int level) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnLevelUp(LevelUpCallback callback) { m_onLevelUp = std::move(callback); }
    void SetOnXPGain(XPGainCallback callback) { m_onXPGain = std::move(callback); }

    // =========================================================================
    // Static Instance
    // =========================================================================

    static HeroProgression& Instance();

private:
    XPCurveConfig m_xpCurve;
    XPRewardConfig m_xpRewards;
    AbilityPointRules m_abilityRules;
    AttributeGainConfig m_attrGainConfig;

    // Talent unlock levels
    std::vector<int> m_talentUnlockLevels = {10, 15, 20, 25};

    // Callbacks
    LevelUpCallback m_onLevelUp;
    XPGainCallback m_onXPGain;
};

// ============================================================================
// XP Event
// ============================================================================

/**
 * @brief Event representing XP gained
 */
struct XPGainEvent {
    uint32_t heroInstanceId = 0;
    int amount = 0;
    ExperienceSource source = ExperienceSource::Passive;
    uint32_t sourceEntityId = 0;
    glm::vec3 position{0.0f};
    float gameTime = 0.0f;

    // Sharing info
    bool wasShared = false;
    int originalAmount = 0;
    int shareCount = 0;
};

// ============================================================================
// Level Up Event
// ============================================================================

/**
 * @brief Event representing a level up
 */
struct LevelUpEvent {
    uint32_t heroInstanceId = 0;
    int oldLevel = 0;
    int newLevel = 0;
    LevelUpBonus bonus;
    float gameTime = 0.0f;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert experience source to string
 */
[[nodiscard]] std::string ExperienceSourceToString(ExperienceSource source);

/**
 * @brief Parse experience source from string
 */
[[nodiscard]] ExperienceSource StringToExperienceSource(const std::string& str);

} // namespace Heroes
} // namespace Vehement
