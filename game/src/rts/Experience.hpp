#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace Vehement {
namespace RTS {

/**
 * @brief Experience sources for tracking XP gains
 */
enum class ExperienceSource : uint8_t {
    ZombieKill,         // Killing zombies
    EliteKill,          // Killing elite enemies
    BossKill,           // Killing boss enemies
    ObjectiveComplete,  // Completing objectives/quests
    BuildingConstruct,  // Constructing buildings
    BuildingUpgrade,    // Upgrading buildings
    UnitTrain,          // Training units
    ResourceGather,     // Gathering resources (small XP)
    Exploration,        // Discovering new areas
    Rescue,             // Rescuing NPCs
    Quest,              // Quest completion bonus

    COUNT
};

/**
 * @brief Experience modifier types for bonuses/penalties
 */
enum class ExperienceModifier : uint8_t {
    Base,               // Base XP (1.0x)
    Bonus,              // Temporary bonus multiplier
    ClassBonus,         // Hero class passive bonus
    ItemBonus,          // Equipment bonus
    AuraBonus,          // Aura effect bonus
    Penalty,            // Level difference penalty

    COUNT
};

/**
 * @brief Level thresholds and configuration
 */
struct LevelConfig {
    static constexpr int MIN_LEVEL = 1;
    static constexpr int MAX_LEVEL = 20;

    // XP required to reach each level (index = level - 1)
    // Follows exponential curve: 100 * (level ^ 1.8)
    static constexpr std::array<int, MAX_LEVEL> XP_THRESHOLDS = {
        0,          // Level 1 (start)
        100,        // Level 2
        270,        // Level 3
        520,        // Level 4
        860,        // Level 5
        1300,       // Level 6
        1850,       // Level 7
        2520,       // Level 8
        3300,       // Level 9
        4220,       // Level 10
        5280,       // Level 11
        6490,       // Level 12
        7860,       // Level 13
        9400,       // Level 14
        11120,      // Level 15
        13030,      // Level 16
        15140,      // Level 17
        17460,      // Level 18
        20000,      // Level 19
        22770       // Level 20
    };

    // Stat points gained per level
    static constexpr int STAT_POINTS_PER_LEVEL = 3;

    // Ability points gained per level (extra at certain levels)
    static constexpr int ABILITY_POINTS_BASE = 1;
    static constexpr std::array<int, MAX_LEVEL> ABILITY_POINT_BONUSES = {
        0, 0, 0, 0, 1,    // Bonus at level 5
        0, 0, 0, 0, 1,    // Bonus at level 10
        0, 0, 0, 0, 1,    // Bonus at level 15
        0, 0, 0, 0, 2     // Double bonus at level 20
    };

    /**
     * @brief Get XP required for a specific level
     */
    static int GetXPForLevel(int level) {
        if (level < MIN_LEVEL) return 0;
        if (level > MAX_LEVEL) return XP_THRESHOLDS[MAX_LEVEL - 1];
        return XP_THRESHOLDS[level - 1];
    }

    /**
     * @brief Calculate level from total XP
     */
    static int CalculateLevelFromXP(int totalXP) {
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            if (totalXP >= XP_THRESHOLDS[i]) {
                return i + 1;
            }
        }
        return MIN_LEVEL;
    }

    /**
     * @brief Get ability points for reaching a level
     */
    static int GetAbilityPointsForLevel(int level) {
        if (level < MIN_LEVEL || level > MAX_LEVEL) return 0;
        return ABILITY_POINTS_BASE + ABILITY_POINT_BONUSES[level - 1];
    }
};

/**
 * @brief XP gain event for tracking and display
 */
struct ExperienceGain {
    int amount = 0;
    ExperienceSource source = ExperienceSource::ZombieKill;
    float modifier = 1.0f;
    glm::vec3 position{0.0f};    // For floating text display
    bool showPopup = true;
};

/**
 * @brief Experience and leveling system for heroes
 *
 * Handles XP accumulation, level progression, stat allocation,
 * and ability point distribution. Supports modifiers for class
 * bonuses, items, and diminishing returns.
 */
class ExperienceSystem {
public:
    using LevelUpCallback = std::function<void(int newLevel, int statPoints, int abilityPoints)>;
    using XPGainCallback = std::function<void(const ExperienceGain& gain)>;

    ExperienceSystem();
    ~ExperienceSystem() = default;

    // =========================================================================
    // XP Management
    // =========================================================================

    /**
     * @brief Add experience from a source
     * @param baseAmount Base XP amount before modifiers
     * @param source Source of the XP
     * @param enemyLevel Enemy level for scaling (0 for non-combat)
     * @return Actual XP gained after modifiers
     */
    int AddExperience(int baseAmount, ExperienceSource source, int enemyLevel = 0);

    /**
     * @brief Add experience with custom modifier
     */
    int AddExperienceRaw(int amount, ExperienceSource source);

    /**
     * @brief Get current total XP
     */
    [[nodiscard]] int GetTotalXP() const noexcept { return m_totalXP; }

    /**
     * @brief Get XP within current level
     */
    [[nodiscard]] int GetCurrentLevelXP() const noexcept;

    /**
     * @brief Get XP needed for next level
     */
    [[nodiscard]] int GetXPForNextLevel() const noexcept;

    /**
     * @brief Get progress to next level (0.0 to 1.0)
     */
    [[nodiscard]] float GetLevelProgress() const noexcept;

    // =========================================================================
    // Level Management
    // =========================================================================

    /**
     * @brief Get current level
     */
    [[nodiscard]] int GetLevel() const noexcept { return m_level; }

    /**
     * @brief Check if at max level
     */
    [[nodiscard]] bool IsMaxLevel() const noexcept { return m_level >= LevelConfig::MAX_LEVEL; }

    /**
     * @brief Force set level (for testing/cheats)
     */
    void SetLevel(int level);

    // =========================================================================
    // Stat Points
    // =========================================================================

    /**
     * @brief Get unspent stat points
     */
    [[nodiscard]] int GetUnspentStatPoints() const noexcept { return m_unspentStatPoints; }

    /**
     * @brief Spend a stat point
     * @return true if point was available to spend
     */
    bool SpendStatPoint();

    /**
     * @brief Add stat points (from items, bonuses)
     */
    void AddStatPoints(int amount) { m_unspentStatPoints += amount; }

    // =========================================================================
    // Ability Points
    // =========================================================================

    /**
     * @brief Get unspent ability points
     */
    [[nodiscard]] int GetUnspentAbilityPoints() const noexcept { return m_unspentAbilityPoints; }

    /**
     * @brief Spend an ability point
     * @return true if point was available to spend
     */
    bool SpendAbilityPoint();

    /**
     * @brief Add ability points (from quests, items)
     */
    void AddAbilityPoints(int amount) { m_unspentAbilityPoints += amount; }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Set an XP modifier
     * @param type Modifier type
     * @param value Multiplier value (1.0 = normal)
     */
    void SetModifier(ExperienceModifier type, float value);

    /**
     * @brief Get current modifier value
     */
    [[nodiscard]] float GetModifier(ExperienceModifier type) const;

    /**
     * @brief Get total XP multiplier from all modifiers
     */
    [[nodiscard]] float GetTotalModifier() const noexcept;

    /**
     * @brief Reset all modifiers to default
     */
    void ResetModifiers();

    // =========================================================================
    // XP Scaling
    // =========================================================================

    /**
     * @brief Calculate XP scaling based on level difference
     * @param heroLevel Current hero level
     * @param enemyLevel Enemy level
     * @return Multiplier (0.1 to 1.5)
     */
    static float CalculateLevelScaling(int heroLevel, int enemyLevel);

    /**
     * @brief Get base XP for killing an enemy
     * @param enemyLevel Enemy level
     * @param isElite Whether enemy is elite
     * @param isBoss Whether enemy is boss
     */
    static int GetKillXP(int enemyLevel, bool isElite = false, bool isBoss = false);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnLevelUp(LevelUpCallback callback) { m_onLevelUp = std::move(callback); }
    void SetOnXPGain(XPGainCallback callback) { m_onXPGain = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Reset to starting state
     */
    void Reset();

    /**
     * @brief Get XP breakdown by source
     */
    [[nodiscard]] int GetXPFromSource(ExperienceSource source) const;

private:
    void CheckLevelUp();
    void ApplyLevelUp(int newLevel);

    // Core state
    int m_totalXP = 0;
    int m_level = LevelConfig::MIN_LEVEL;
    int m_unspentStatPoints = 0;
    int m_unspentAbilityPoints = 0;

    // Modifiers
    std::array<float, static_cast<size_t>(ExperienceModifier::COUNT)> m_modifiers;

    // XP tracking by source
    std::array<int, static_cast<size_t>(ExperienceSource::COUNT)> m_xpBySource;

    // Callbacks
    LevelUpCallback m_onLevelUp;
    XPGainCallback m_onXPGain;
};

// ============================================================================
// XP Value Constants
// ============================================================================

namespace XPValues {
    // Combat XP
    constexpr int ZOMBIE_BASE = 10;
    constexpr int ZOMBIE_ELITE = 50;
    constexpr int ZOMBIE_BOSS = 200;

    // Building XP
    constexpr int BUILDING_CONSTRUCT_BASE = 25;
    constexpr int BUILDING_UPGRADE = 15;

    // Objective XP
    constexpr int OBJECTIVE_MINOR = 50;
    constexpr int OBJECTIVE_MAJOR = 150;
    constexpr int OBJECTIVE_CRITICAL = 500;

    // Misc XP
    constexpr int NPC_RESCUE = 30;
    constexpr int AREA_DISCOVER = 20;
    constexpr int RESOURCE_GATHER = 1;  // Per batch
}

} // namespace RTS
} // namespace Vehement
