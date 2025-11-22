#include "HeroProgression.hpp"
#include "HeroInstance.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Vehement {
namespace Heroes {

// ============================================================================
// HeroProgression Implementation
// ============================================================================

HeroProgression::HeroProgression() {
    // Initialize default ability level requirements
    // Level 1 ability = hero level 1
    // Level 2 ability = hero level 3
    // Level 3 ability = hero level 5
    // Level 4 ability = hero level 7
    m_abilityRules.abilityLevelRequirements = {
        {1, 1},
        {2, 3},
        {3, 5},
        {4, 7}
    };

    // Ultimate can be upgraded at levels 6, 12, 18
    m_abilityRules.ultimateLevelUpLevels = {6, 12, 18};
}

HeroProgression::~HeroProgression() = default;

HeroProgression& HeroProgression::Instance() {
    static HeroProgression instance;
    return instance;
}

bool HeroProgression::LoadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Parse configuration (simplified - would use proper JSON parser)
    // For now, use default values
    return true;
}

// ============================================================================
// XP Calculation
// ============================================================================

int HeroProgression::CalculateXPForLevel(int level) const {
    if (level <= 1) return 0;
    if (level > m_xpCurve.maxLevel) level = m_xpCurve.maxLevel;

    if (m_xpCurve.exponential) {
        // Exponential curve: XP = base * growth^(level-1)
        return static_cast<int>(m_xpCurve.baseXP * std::pow(m_xpCurve.growthRate, level - 2));
    } else {
        // Linear curve: XP = base * (level-1) * growth
        // Total XP is sum of all levels
        int totalXP = 0;
        for (int l = 2; l <= level; l++) {
            totalXP += static_cast<int>(m_xpCurve.baseXP * (1.0f + (l - 2) * (m_xpCurve.growthRate - 1.0f)));
        }
        return totalXP;
    }
}

int HeroProgression::CalculateXPToNextLevel(int currentLevel) const {
    if (currentLevel >= m_xpCurve.maxLevel) return 0;
    return CalculateXPForLevel(currentLevel + 1) - CalculateXPForLevel(currentLevel);
}

int HeroProgression::CalculateKillXP(int killerLevel, int targetLevel, ExperienceSource source) const {
    int baseXP = 0;

    switch (source) {
        case ExperienceSource::HeroKill:
            baseXP = m_xpRewards.heroKillBase +
                     static_cast<int>(targetLevel * m_xpRewards.heroKillPerLevel);
            break;
        case ExperienceSource::CreepKill:
            baseXP = m_xpRewards.creepKillBase;
            break;
        case ExperienceSource::BossKill:
            baseXP = m_xpRewards.bossKillBase;
            break;
        case ExperienceSource::BuildingKill:
            baseXP = m_xpRewards.buildingKillBase;
            break;
        case ExperienceSource::Assist:
            baseXP = static_cast<int>(
                (m_xpRewards.heroKillBase + targetLevel * m_xpRewards.heroKillPerLevel) *
                m_xpRewards.assistPercent);
            break;
        default:
            return 0;
    }

    // Apply level difference penalty
    int levelDiff = killerLevel - targetLevel;
    if (levelDiff > 0) {
        float reduction = levelDiff * m_xpCurve.levelDifferenceReduction;
        float multiplier = std::max(m_xpCurve.minimumXPPercent, 1.0f - reduction);
        baseXP = static_cast<int>(baseXP * multiplier);
    } else if (levelDiff < 0) {
        // Bonus XP for killing higher level enemies
        float bonus = -levelDiff * 0.05f; // 5% bonus per level
        baseXP = static_cast<int>(baseXP * (1.0f + bonus));
    }

    return std::max(1, baseXP);
}

int HeroProgression::CalculateSharedXP(int baseXP, int allyCount) const {
    if (allyCount <= 0) return baseXP;

    // Main killer gets full XP, allies split shared portion
    float sharedPortion = baseXP * m_xpRewards.xpSharePercent;
    return static_cast<int>(sharedPortion / allyCount);
}

int HeroProgression::CalculateLevelFromXP(int totalXP) const {
    int level = 1;
    while (level < m_xpCurve.maxLevel && CalculateXPForLevel(level + 1) <= totalXP) {
        level++;
    }
    return level;
}

// ============================================================================
// Level Up Processing
// ============================================================================

int HeroProgression::ProcessXPGain(HeroInstance& hero, int amount, ExperienceSource source) {
    if (amount <= 0) return 0;

    int oldLevel = hero.GetLevel();
    int levelsGained = hero.AddExperience(amount);

    // Fire XP gain callback
    if (m_onXPGain) {
        m_onXPGain(hero, amount, source);
    }

    // Process each level gained
    for (int l = oldLevel + 1; l <= hero.GetLevel(); l++) {
        LevelUpBonus bonus = CalculateLevelUpBonus(hero, l);
        ApplyLevelUpBonus(hero, bonus);

        if (m_onLevelUp) {
            m_onLevelUp(hero, bonus);
        }
    }

    return levelsGained;
}

LevelUpBonus HeroProgression::CalculateLevelUpBonus(const HeroInstance& hero, int level) const {
    LevelUpBonus bonus;
    bonus.level = level;

    auto definition = hero.GetDefinition();
    if (!definition) return bonus;

    const auto& growth = definition->GetStatGrowth();

    // Ability points
    bonus.abilityPoints = m_abilityRules.pointsPerLevel;

    // Check for bonus ability points at specific levels
    for (const auto& [bonusLevel, bonusPoints] : m_abilityRules.bonusPointLevels) {
        if (bonusLevel == level) {
            bonus.abilityPoints += bonusPoints;
        }
    }

    // Attribute gains
    bonus.strengthGain = growth.strengthPerLevel;
    bonus.agilityGain = growth.agilityPerLevel;
    bonus.intelligenceGain = growth.intelligencePerLevel;

    // Bonus attributes every N levels
    if (level % m_attrGainConfig.bonusEveryNLevels == 0) {
        bonus.strengthGain += m_attrGainConfig.bonusStrength;
        bonus.agilityGain += m_attrGainConfig.bonusAgility;
        bonus.intelligenceGain += m_attrGainConfig.bonusIntelligence;
    }

    // Resource gains
    bonus.maxHealthGain = growth.healthPerLevel;
    bonus.maxManaGain = growth.manaPerLevel;

    // Check talent unlock
    for (size_t i = 0; i < m_talentUnlockLevels.size(); i++) {
        if (m_talentUnlockLevels[i] == level) {
            bonus.talentTierUnlock = static_cast<int>(i + 1);
            break;
        }
    }

    // Check ultimate unlock
    if (level == m_abilityRules.ultimateUnlockLevel) {
        bonus.ultimateUnlock = true;
    }

    return bonus;
}

void HeroProgression::ApplyLevelUpBonus(HeroInstance& hero, const LevelUpBonus& bonus) {
    // Ability points are automatically added by HeroInstance::AddExperience
    // Attribute gains are handled by stat calculation from level

    // If talent tier unlocked, could trigger UI notification
    // If ultimate unlocked, could trigger UI notification
}

// ============================================================================
// Ability Point Distribution
// ============================================================================

bool HeroProgression::CanLevelAbility(const HeroInstance& hero, int abilitySlot) const {
    if (abilitySlot < 0 || abilitySlot >= HeroInstance::ABILITY_SLOT_COUNT) {
        return false;
    }

    // Check ability points
    if (hero.GetAbilityPoints() <= 0) {
        return false;
    }

    const auto& ability = hero.GetAbility(abilitySlot);

    // Check if ability exists
    if (ability.abilityId.empty()) {
        return false;
    }

    // Check max level
    if (ability.currentLevel >= ability.maxLevel) {
        return false;
    }

    // Check if this is ultimate (slot 3, index 3)
    bool isUltimate = false;
    auto definition = hero.GetDefinition();
    if (definition) {
        const auto* binding = definition->GetAbilitySlot(abilitySlot + 1);
        if (binding) {
            isUltimate = binding->isUltimate;
        }
    }

    int requiredLevel = GetRequiredLevelForAbility(ability.currentLevel + 1, isUltimate);
    if (hero.GetLevel() < requiredLevel) {
        return false;
    }

    return true;
}

int HeroProgression::GetRequiredLevelForAbility(int abilityLevel, bool isUltimate) const {
    if (isUltimate) {
        // Ultimate requirements: level 6, 12, 18 for levels 1, 2, 3
        if (abilityLevel <= 0 || abilityLevel > static_cast<int>(m_abilityRules.ultimateLevelUpLevels.size())) {
            return 99;
        }
        return m_abilityRules.ultimateLevelUpLevels[abilityLevel - 1];
    }

    // Normal ability requirements
    auto it = m_abilityRules.abilityLevelRequirements.find(abilityLevel);
    if (it != m_abilityRules.abilityLevelRequirements.end()) {
        return it->second;
    }

    // Default: 2 levels per ability level
    return 1 + (abilityLevel - 1) * 2;
}

int HeroProgression::GetTotalAbilityPointsAtLevel(int level) const {
    int points = level; // 1 point per level

    // Add bonus points
    for (const auto& [bonusLevel, bonusPoints] : m_abilityRules.bonusPointLevels) {
        if (bonusLevel <= level) {
            points += bonusPoints;
        }
    }

    return points;
}

// ============================================================================
// Attribute Gains
// ============================================================================

std::tuple<float, float, float> HeroProgression::CalculateAttributesAtLevel(
    const HeroInstance& hero, int level) const {

    auto definition = hero.GetDefinition();
    if (!definition) {
        return {20.0f, 15.0f, 15.0f};
    }

    const auto& baseStats = definition->GetBaseStats();
    auto [strGain, agiGain, intGain] = GetAttributeGains(1, level, *definition);

    return {
        baseStats.strength + strGain,
        baseStats.agility + agiGain,
        baseStats.intelligence + intGain
    };
}

std::tuple<float, float, float> HeroProgression::GetAttributeGains(
    int fromLevel, int toLevel, const HeroDefinition& heroDefinition) const {

    if (toLevel <= fromLevel) {
        return {0.0f, 0.0f, 0.0f};
    }

    const auto& growth = heroDefinition.GetStatGrowth();
    int levelsGained = toLevel - fromLevel;

    float strGain = growth.strengthPerLevel * levelsGained;
    float agiGain = growth.agilityPerLevel * levelsGained;
    float intGain = growth.intelligencePerLevel * levelsGained;

    // Add bonus attributes
    for (int l = fromLevel + 1; l <= toLevel; l++) {
        if (l % m_attrGainConfig.bonusEveryNLevels == 0) {
            strGain += m_attrGainConfig.bonusStrength;
            agiGain += m_attrGainConfig.bonusAgility;
            intGain += m_attrGainConfig.bonusIntelligence;
        }
    }

    return {strGain, agiGain, intGain};
}

// ============================================================================
// Talent Unlocks
// ============================================================================

int HeroProgression::GetTalentTierAtLevel(int level) const {
    for (int i = static_cast<int>(m_talentUnlockLevels.size()) - 1; i >= 0; i--) {
        if (level >= m_talentUnlockLevels[i]) {
            return i;
        }
    }
    return -1;
}

bool HeroProgression::LevelUnlocksTalent(int level) const {
    for (int unlockLevel : m_talentUnlockLevels) {
        if (unlockLevel == level) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string ExperienceSourceToString(ExperienceSource source) {
    switch (source) {
        case ExperienceSource::HeroKill: return "hero_kill";
        case ExperienceSource::CreepKill: return "creep_kill";
        case ExperienceSource::BossKill: return "boss_kill";
        case ExperienceSource::BuildingKill: return "building_kill";
        case ExperienceSource::Assist: return "assist";
        case ExperienceSource::Quest: return "quest";
        case ExperienceSource::Objective: return "objective";
        case ExperienceSource::Passive: return "passive";
        case ExperienceSource::Item: return "item";
        case ExperienceSource::Script: return "script";
    }
    return "unknown";
}

ExperienceSource StringToExperienceSource(const std::string& str) {
    if (str == "hero_kill") return ExperienceSource::HeroKill;
    if (str == "creep_kill") return ExperienceSource::CreepKill;
    if (str == "boss_kill") return ExperienceSource::BossKill;
    if (str == "building_kill") return ExperienceSource::BuildingKill;
    if (str == "assist") return ExperienceSource::Assist;
    if (str == "quest") return ExperienceSource::Quest;
    if (str == "objective") return ExperienceSource::Objective;
    if (str == "passive") return ExperienceSource::Passive;
    if (str == "item") return ExperienceSource::Item;
    if (str == "script") return ExperienceSource::Script;
    return ExperienceSource::Passive;
}

} // namespace Heroes
} // namespace Vehement
