#include "Experience.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// ExperienceSystem Implementation
// ============================================================================

ExperienceSystem::ExperienceSystem() {
    Reset();
}

void ExperienceSystem::Reset() {
    m_totalXP = 0;
    m_level = LevelConfig::MIN_LEVEL;
    m_unspentStatPoints = 0;
    m_unspentAbilityPoints = 1; // Start with 1 ability point

    // Reset modifiers to 1.0 (no change)
    for (auto& mod : m_modifiers) {
        mod = 1.0f;
    }
    m_modifiers[static_cast<size_t>(ExperienceModifier::Penalty)] = 1.0f;

    // Reset XP tracking
    for (auto& xp : m_xpBySource) {
        xp = 0;
    }
}

int ExperienceSystem::AddExperience(int baseAmount, ExperienceSource source, int enemyLevel) {
    if (IsMaxLevel()) {
        return 0;
    }

    // Apply level scaling
    float levelScale = CalculateLevelScaling(m_level, enemyLevel);

    // Apply all modifiers
    float totalMod = GetTotalModifier() * levelScale;

    // Calculate final XP
    int finalXP = static_cast<int>(baseAmount * totalMod);
    finalXP = std::max(1, finalXP); // Always gain at least 1 XP

    return AddExperienceRaw(finalXP, source);
}

int ExperienceSystem::AddExperienceRaw(int amount, ExperienceSource source) {
    if (IsMaxLevel() || amount <= 0) {
        return 0;
    }

    m_totalXP += amount;
    m_xpBySource[static_cast<size_t>(source)] += amount;

    // Notify XP gain
    if (m_onXPGain) {
        ExperienceGain gain;
        gain.amount = amount;
        gain.source = source;
        gain.modifier = GetTotalModifier();
        gain.showPopup = true;
        m_onXPGain(gain);
    }

    // Check for level ups
    CheckLevelUp();

    return amount;
}

int ExperienceSystem::GetCurrentLevelXP() const noexcept {
    if (m_level <= LevelConfig::MIN_LEVEL) {
        return m_totalXP;
    }

    int previousLevelXP = LevelConfig::GetXPForLevel(m_level);
    return m_totalXP - previousLevelXP;
}

int ExperienceSystem::GetXPForNextLevel() const noexcept {
    if (IsMaxLevel()) {
        return 0;
    }

    int currentLevelXP = LevelConfig::GetXPForLevel(m_level);
    int nextLevelXP = LevelConfig::GetXPForLevel(m_level + 1);
    return nextLevelXP - currentLevelXP;
}

float ExperienceSystem::GetLevelProgress() const noexcept {
    if (IsMaxLevel()) {
        return 1.0f;
    }

    int xpForNext = GetXPForNextLevel();
    if (xpForNext <= 0) {
        return 1.0f;
    }

    int currentXP = GetCurrentLevelXP();
    return std::clamp(static_cast<float>(currentXP) / xpForNext, 0.0f, 1.0f);
}

void ExperienceSystem::SetLevel(int level) {
    level = std::clamp(level, LevelConfig::MIN_LEVEL, LevelConfig::MAX_LEVEL);

    if (level > m_level) {
        // Leveling up - grant points for each level
        for (int i = m_level + 1; i <= level; ++i) {
            ApplyLevelUp(i);
        }
    }

    m_level = level;
    m_totalXP = LevelConfig::GetXPForLevel(level);
}

bool ExperienceSystem::SpendStatPoint() {
    if (m_unspentStatPoints <= 0) {
        return false;
    }
    m_unspentStatPoints--;
    return true;
}

bool ExperienceSystem::SpendAbilityPoint() {
    if (m_unspentAbilityPoints <= 0) {
        return false;
    }
    m_unspentAbilityPoints--;
    return true;
}

void ExperienceSystem::SetModifier(ExperienceModifier type, float value) {
    size_t index = static_cast<size_t>(type);
    if (index < m_modifiers.size()) {
        m_modifiers[index] = value;
    }
}

float ExperienceSystem::GetModifier(ExperienceModifier type) const {
    size_t index = static_cast<size_t>(type);
    if (index < m_modifiers.size()) {
        return m_modifiers[index];
    }
    return 1.0f;
}

float ExperienceSystem::GetTotalModifier() const noexcept {
    float total = 1.0f;
    for (const auto& mod : m_modifiers) {
        total *= mod;
    }
    return std::max(0.1f, total); // Minimum 10% XP gain
}

void ExperienceSystem::ResetModifiers() {
    for (auto& mod : m_modifiers) {
        mod = 1.0f;
    }
}

float ExperienceSystem::CalculateLevelScaling(int heroLevel, int enemyLevel) {
    if (enemyLevel <= 0) {
        return 1.0f; // Non-combat XP source
    }

    int levelDiff = enemyLevel - heroLevel;

    if (levelDiff >= 3) {
        // Enemy is much higher level - bonus XP
        return 1.5f;
    } else if (levelDiff >= 1) {
        // Enemy is slightly higher - small bonus
        return 1.0f + (levelDiff * 0.1f);
    } else if (levelDiff >= -2) {
        // Similar level - full XP
        return 1.0f;
    } else if (levelDiff >= -5) {
        // Enemy is lower level - reduced XP
        // -3 = 80%, -4 = 60%, -5 = 40%
        return 1.0f + (levelDiff * 0.2f);
    } else {
        // Enemy is much lower level - minimal XP
        // Minimum 10%
        return 0.1f;
    }
}

int ExperienceSystem::GetKillXP(int enemyLevel, bool isElite, bool isBoss) {
    int baseXP = XPValues::ZOMBIE_BASE;

    if (isBoss) {
        baseXP = XPValues::ZOMBIE_BOSS;
    } else if (isElite) {
        baseXP = XPValues::ZOMBIE_ELITE;
    }

    // Scale XP with enemy level
    float levelMultiplier = 1.0f + (enemyLevel - 1) * 0.1f;
    return static_cast<int>(baseXP * levelMultiplier);
}

int ExperienceSystem::GetXPFromSource(ExperienceSource source) const {
    size_t index = static_cast<size_t>(source);
    if (index < m_xpBySource.size()) {
        return m_xpBySource[index];
    }
    return 0;
}

void ExperienceSystem::CheckLevelUp() {
    while (m_level < LevelConfig::MAX_LEVEL) {
        int xpNeeded = LevelConfig::GetXPForLevel(m_level + 1);
        if (m_totalXP >= xpNeeded) {
            ApplyLevelUp(m_level + 1);
            m_level++;
        } else {
            break;
        }
    }
}

void ExperienceSystem::ApplyLevelUp(int newLevel) {
    // Grant stat points
    int statPoints = LevelConfig::STAT_POINTS_PER_LEVEL;
    m_unspentStatPoints += statPoints;

    // Grant ability points
    int abilityPoints = LevelConfig::GetAbilityPointsForLevel(newLevel);
    m_unspentAbilityPoints += abilityPoints;

    // Notify
    if (m_onLevelUp) {
        m_onLevelUp(newLevel, statPoints, abilityPoints);
    }
}

} // namespace RTS
} // namespace Vehement
