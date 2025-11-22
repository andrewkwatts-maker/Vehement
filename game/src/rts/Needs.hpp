#pragma once

#include <cstdint>
#include <algorithm>

namespace Vehement {

/**
 * @brief Worker needs system - tracks physical and psychological requirements
 *
 * Workers have needs that must be met to maintain productivity and prevent
 * negative outcomes like desertion, sickness, or death.
 */
struct WorkerNeeds {
    // =========================================================================
    // Primary Needs (0-100 scale, higher is better)
    // =========================================================================

    float hunger = 100.0f;    ///< Satiation level - decreases over time, restored by eating
    float energy = 100.0f;    ///< Stamina level - decreases while working, restored by rest
    float morale = 75.0f;     ///< Happiness/mental state - affected by events, safety, conditions
    float health = 100.0f;    ///< Physical health - damaged by combat, disease, starvation

    // =========================================================================
    // Need Thresholds
    // =========================================================================

    static constexpr float CRITICAL_THRESHOLD = 10.0f;   ///< Below this: severe penalties/death risk
    static constexpr float LOW_THRESHOLD = 25.0f;        ///< Below this: significant penalties
    static constexpr float MODERATE_THRESHOLD = 50.0f;   ///< Below this: minor penalties
    static constexpr float GOOD_THRESHOLD = 75.0f;       ///< Above this: bonuses apply

    // =========================================================================
    // Decay Rates (per second under normal conditions)
    // =========================================================================

    static constexpr float HUNGER_DECAY_RATE = 0.5f;     ///< Lose 0.5 hunger per second (empty in ~3.3 min)
    static constexpr float ENERGY_DECAY_IDLE = 0.1f;     ///< Energy loss while idle
    static constexpr float ENERGY_DECAY_WORKING = 0.4f;  ///< Energy loss while working
    static constexpr float ENERGY_DECAY_MOVING = 0.2f;   ///< Energy loss while moving
    static constexpr float MORALE_DECAY_RATE = 0.05f;    ///< Natural morale drift toward neutral

    // =========================================================================
    // Recovery Rates (per second)
    // =========================================================================

    static constexpr float ENERGY_RECOVERY_RESTING = 2.0f;  ///< Energy gained while resting
    static constexpr float HEALTH_RECOVERY_RESTING = 0.5f;  ///< Health recovery when resting
    static constexpr float HEALTH_RECOVERY_MEDIC = 2.0f;    ///< Health recovery from medic treatment

    // =========================================================================
    // Hunger Effects
    // =========================================================================

    static constexpr float STARVATION_DAMAGE_RATE = 1.0f;  ///< Health loss per second when starving
    static constexpr float HUNGER_MORALE_PENALTY = 0.1f;   ///< Morale loss rate when hungry

    // =========================================================================
    // Methods
    // =========================================================================

    /**
     * @brief Update needs based on elapsed time and current activity
     * @param deltaTime Time since last update
     * @param isWorking Whether the worker is actively working
     * @param isMoving Whether the worker is moving
     * @param isResting Whether the worker is resting
     */
    void Update(float deltaTime, bool isWorking, bool isMoving, bool isResting) {
        // Hunger always decreases (slightly faster when working)
        float hungerDecay = HUNGER_DECAY_RATE;
        if (isWorking) hungerDecay *= 1.5f;
        hunger = std::max(0.0f, hunger - hungerDecay * deltaTime);

        // Energy changes based on activity
        if (isResting) {
            energy = std::min(100.0f, energy + ENERGY_RECOVERY_RESTING * deltaTime);
            health = std::min(100.0f, health + HEALTH_RECOVERY_RESTING * deltaTime);
        } else if (isWorking) {
            energy = std::max(0.0f, energy - ENERGY_DECAY_WORKING * deltaTime);
        } else if (isMoving) {
            energy = std::max(0.0f, energy - ENERGY_DECAY_MOVING * deltaTime);
        } else {
            energy = std::max(0.0f, energy - ENERGY_DECAY_IDLE * deltaTime);
        }

        // Starvation effects
        if (hunger <= CRITICAL_THRESHOLD) {
            health = std::max(0.0f, health - STARVATION_DAMAGE_RATE * deltaTime);
            morale = std::max(0.0f, morale - HUNGER_MORALE_PENALTY * 2.0f * deltaTime);
        } else if (hunger <= LOW_THRESHOLD) {
            morale = std::max(0.0f, morale - HUNGER_MORALE_PENALTY * deltaTime);
        }

        // Morale drifts toward 50 if no other factors
        if (morale > 50.0f) {
            morale = std::max(50.0f, morale - MORALE_DECAY_RATE * deltaTime);
        }
    }

    /**
     * @brief Feed the worker, restoring hunger
     * @param amount Food value (hunger restored)
     */
    void Feed(float amount) {
        hunger = std::min(100.0f, hunger + amount);
    }

    /**
     * @brief Heal the worker
     * @param amount Health to restore
     */
    void Heal(float amount) {
        health = std::min(100.0f, health + amount);
    }

    /**
     * @brief Modify morale (positive or negative)
     * @param amount Morale change
     */
    void ModifyMorale(float amount) {
        morale = std::clamp(morale + amount, 0.0f, 100.0f);
    }

    /**
     * @brief Apply damage to health
     * @param amount Damage to apply
     */
    void TakeDamage(float amount) {
        health = std::max(0.0f, health - amount);
    }

    // =========================================================================
    // Status Queries
    // =========================================================================

    /** @brief Check if worker is starving (critical hunger) */
    [[nodiscard]] bool IsStarving() const noexcept { return hunger <= CRITICAL_THRESHOLD; }

    /** @brief Check if worker is hungry */
    [[nodiscard]] bool IsHungry() const noexcept { return hunger <= LOW_THRESHOLD; }

    /** @brief Check if worker is exhausted (critical energy) */
    [[nodiscard]] bool IsExhausted() const noexcept { return energy <= CRITICAL_THRESHOLD; }

    /** @brief Check if worker is tired */
    [[nodiscard]] bool IsTired() const noexcept { return energy <= LOW_THRESHOLD; }

    /** @brief Check if worker has low morale */
    [[nodiscard]] bool HasLowMorale() const noexcept { return morale <= LOW_THRESHOLD; }

    /** @brief Check if worker might desert (critical morale) */
    [[nodiscard]] bool MightDesert() const noexcept { return morale <= CRITICAL_THRESHOLD; }

    /** @brief Check if worker is critically injured */
    [[nodiscard]] bool IsCriticallyInjured() const noexcept { return health <= CRITICAL_THRESHOLD; }

    /** @brief Check if worker is injured */
    [[nodiscard]] bool IsInjured() const noexcept { return health <= LOW_THRESHOLD; }

    /** @brief Check if worker is dead */
    [[nodiscard]] bool IsDead() const noexcept { return health <= 0.0f; }

    /** @brief Check if worker needs rest urgently */
    [[nodiscard]] bool NeedsRest() const noexcept { return IsTired() || IsExhausted(); }

    /** @brief Check if worker needs food urgently */
    [[nodiscard]] bool NeedsFood() const noexcept { return IsHungry() || IsStarving(); }

    /** @brief Check if worker needs medical attention */
    [[nodiscard]] bool NeedsMedical() const noexcept { return IsInjured() || IsCriticallyInjured(); }

    // =========================================================================
    // Productivity Calculation
    // =========================================================================

    /**
     * @brief Calculate productivity modifier based on needs (0.0 - 1.5)
     *
     * - Well-fed, rested, happy workers can exceed 100% productivity
     * - Hungry, tired, or unhappy workers have reduced productivity
     * - Critical needs severely impact productivity
     *
     * @return Productivity multiplier
     */
    [[nodiscard]] float GetProductivityModifier() const noexcept {
        float modifier = 1.0f;

        // Health impact (most severe)
        if (health <= CRITICAL_THRESHOLD) {
            modifier *= 0.1f;
        } else if (health <= LOW_THRESHOLD) {
            modifier *= 0.5f;
        } else if (health <= MODERATE_THRESHOLD) {
            modifier *= 0.8f;
        }

        // Hunger impact
        if (hunger <= CRITICAL_THRESHOLD) {
            modifier *= 0.2f;
        } else if (hunger <= LOW_THRESHOLD) {
            modifier *= 0.6f;
        } else if (hunger <= MODERATE_THRESHOLD) {
            modifier *= 0.85f;
        } else if (hunger >= GOOD_THRESHOLD) {
            modifier *= 1.1f;  // Well-fed bonus
        }

        // Energy impact
        if (energy <= CRITICAL_THRESHOLD) {
            modifier *= 0.3f;
        } else if (energy <= LOW_THRESHOLD) {
            modifier *= 0.6f;
        } else if (energy <= MODERATE_THRESHOLD) {
            modifier *= 0.8f;
        } else if (energy >= GOOD_THRESHOLD) {
            modifier *= 1.1f;  // Well-rested bonus
        }

        // Morale impact
        if (morale <= CRITICAL_THRESHOLD) {
            modifier *= 0.4f;
        } else if (morale <= LOW_THRESHOLD) {
            modifier *= 0.7f;
        } else if (morale <= MODERATE_THRESHOLD) {
            modifier *= 0.9f;
        } else if (morale >= GOOD_THRESHOLD) {
            modifier *= 1.15f;  // Happy worker bonus
        }

        return std::clamp(modifier, 0.0f, 1.5f);
    }

    /**
     * @brief Get desertion chance per day (0.0 - 1.0)
     *
     * Workers with very low morale or in dire conditions may leave.
     *
     * @return Probability of desertion per day
     */
    [[nodiscard]] float GetDesertionChance() const noexcept {
        if (morale >= MODERATE_THRESHOLD && hunger >= LOW_THRESHOLD) {
            return 0.0f;  // Content workers don't desert
        }

        float chance = 0.0f;

        // Morale-based desertion
        if (morale <= CRITICAL_THRESHOLD) {
            chance += 0.3f;  // 30% base chance when morale is critical
        } else if (morale <= LOW_THRESHOLD) {
            chance += 0.1f;  // 10% base chance when morale is low
        }

        // Starvation increases desertion chance
        if (hunger <= CRITICAL_THRESHOLD) {
            chance += 0.2f;
        } else if (hunger <= LOW_THRESHOLD) {
            chance += 0.05f;
        }

        return std::clamp(chance, 0.0f, 0.5f);  // Cap at 50% per day
    }

    /**
     * @brief Get overall wellbeing score (0-100)
     *
     * Weighted average of all needs for quick status assessment.
     */
    [[nodiscard]] float GetOverallWellbeing() const noexcept {
        return (health * 0.3f + hunger * 0.25f + energy * 0.25f + morale * 0.2f);
    }
};

/**
 * @brief Skill levels for workers (0-100 scale)
 *
 * Skills improve with practice and affect work efficiency.
 */
struct WorkerSkills {
    float gathering = 10.0f;    ///< Resource gathering speed
    float building = 10.0f;     ///< Construction speed
    float farming = 10.0f;      ///< Farming efficiency
    float combat = 5.0f;        ///< Fighting ability
    float crafting = 10.0f;     ///< Item crafting quality/speed
    float medical = 5.0f;       ///< Healing effectiveness
    float scouting = 10.0f;     ///< Exploration/detection range
    float trading = 10.0f;      ///< Trade value bonuses

    static constexpr float MAX_SKILL = 100.0f;
    static constexpr float SKILL_GAIN_RATE = 0.01f;  ///< Skill points per second of work

    /**
     * @brief Improve a skill through practice
     * @param skill Pointer to skill value
     * @param amount XP gained
     */
    static void ImproveSkill(float& skill, float amount) {
        // Diminishing returns - harder to improve at higher levels
        float learningRate = 1.0f - (skill / MAX_SKILL) * 0.8f;
        skill = std::min(MAX_SKILL, skill + amount * learningRate);
    }

    /**
     * @brief Get skill modifier for productivity (0.5 - 2.0)
     * @param skillLevel The skill level to evaluate
     * @return Productivity multiplier based on skill
     */
    [[nodiscard]] static float GetSkillModifier(float skillLevel) noexcept {
        // Linear scaling: 0 skill = 0.5x, 50 skill = 1.0x, 100 skill = 2.0x
        return 0.5f + (skillLevel / MAX_SKILL) * 1.5f;
    }
};

/**
 * @brief Personality traits affecting worker behavior and stats
 */
struct WorkerPersonality {
    // Trait values from -1.0 (negative) to +1.0 (positive)
    float bravery = 0.0f;       ///< -1 = cowardly, +1 = fearless (affects flee distance)
    float diligence = 0.0f;     ///< -1 = lazy, +1 = hardworking (affects work speed)
    float sociability = 0.0f;   ///< -1 = loner, +1 = social (affects morale from group size)
    float optimism = 0.0f;      ///< -1 = pessimistic, +1 = optimistic (affects morale recovery)
    float loyalty = 0.0f;       ///< -1 = mercenary, +1 = devoted (affects desertion)

    /**
     * @brief Generate random personality
     */
    static WorkerPersonality GenerateRandom();

    /**
     * @brief Get effective flee trigger distance
     * @param baseDistance Base distance for fleeing
     */
    [[nodiscard]] float GetFleeDistance(float baseDistance) const noexcept {
        // Brave workers flee at shorter distances
        return baseDistance * (1.0f - bravery * 0.3f);
    }

    /**
     * @brief Get work speed modifier from personality
     */
    [[nodiscard]] float GetWorkSpeedModifier() const noexcept {
        return 1.0f + diligence * 0.2f;
    }

    /**
     * @brief Get morale modifier from being around others
     * @param nearbyWorkerCount Number of workers nearby
     */
    [[nodiscard]] float GetGroupMoraleModifier(int nearbyWorkerCount) const noexcept {
        if (nearbyWorkerCount == 0) {
            // Alone - social workers lose morale, loners gain it
            return -sociability * 0.1f;
        }
        // In group - opposite effect
        return sociability * 0.05f * std::min(nearbyWorkerCount, 5);
    }

    /**
     * @brief Get desertion resistance modifier
     */
    [[nodiscard]] float GetLoyaltyModifier() const noexcept {
        // High loyalty reduces desertion chance
        return 1.0f - loyalty * 0.5f;
    }

    /**
     * @brief Get morale recovery rate modifier
     */
    [[nodiscard]] float GetMoraleRecoveryModifier() const noexcept {
        return 1.0f + optimism * 0.3f;
    }
};

} // namespace Vehement
