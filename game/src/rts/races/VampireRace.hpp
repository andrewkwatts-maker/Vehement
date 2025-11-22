#pragma once

/**
 * @file VampireRace.hpp
 * @brief Vampire Race (The Blood Court) implementation for the RTS game
 *
 * Features:
 * - Life Steal: All vampires heal when dealing damage
 * - Night Power: +50% stats at night, -25% during day
 * - Transformation: Units can change between bat, wolf, and mist forms
 * - Immortal Heroes: Heroes revive at Coffin Vault
 * - Blood Resource: Alternative resource from kills
 * - Domination: Mind control enemy units
 */

#include "../Culture.hpp"
#include "../Faction.hpp"
#include "../TechTree.hpp"
#include "../Ability.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace RTS {

// Forward declarations
class Entity;
class Building;
class Unit;
class Hero;

// ============================================================================
// Vampire Race Constants
// ============================================================================

namespace VampireConstants {
    // Day/Night cycle
    constexpr float NIGHT_BONUS_STATS = 0.50f;           // +50% at night
    constexpr float DAY_PENALTY_STATS = 0.25f;           // -25% during day
    constexpr float NIGHT_DURATION = 300.0f;             // Seconds
    constexpr float DAY_DURATION = 300.0f;               // Seconds
    constexpr float TWILIGHT_TRANSITION = 30.0f;         // Transition time
    constexpr float SUN_DAMAGE_PER_SECOND = 5.0f;        // Damage for vulnerable units

    // Life steal system
    constexpr float BASE_LIFE_STEAL = 0.15f;             // 15% base life steal
    constexpr float MAX_LIFE_STEAL = 0.40f;              // 40% maximum life steal
    constexpr float LIFE_STEAL_UPGRADE = 0.05f;          // Per upgrade level

    // Blood resource
    constexpr int BLOOD_PER_KILL = 5;
    constexpr int BLOOD_PER_WORKER_KILL = 2;
    constexpr int BLOOD_PER_HERO_KILL = 25;
    constexpr float BLOOD_DECAY_RATE = 0.5f;             // Per minute
    constexpr int MAX_BLOOD_STORAGE = 500;

    // Transformation
    constexpr float TRANSFORM_COOLDOWN = 10.0f;
    constexpr float BAT_FORM_SPEED_BONUS = 0.50f;
    constexpr float BAT_FORM_ARMOR_PENALTY = 0.50f;
    constexpr float WOLF_FORM_DAMAGE_BONUS = 0.35f;
    constexpr float WOLF_FORM_SPEED_BONUS = 0.25f;
    constexpr float MIST_FORM_DURATION = 5.0f;
    constexpr float MIST_FORM_COOLDOWN = 60.0f;

    // Domination
    constexpr float BASE_DOMINATION_CHANCE = 0.10f;
    constexpr int MAX_DOMINATED_UNITS = 20;
    constexpr float DOMINATED_STAT_PENALTY = 0.20f;

    // Damage vulnerabilities
    constexpr float HOLY_DAMAGE_MULTIPLIER = 1.75f;
    constexpr float FIRE_DAMAGE_MULTIPLIER = 1.50f;
    constexpr float ICE_DAMAGE_MULTIPLIER = 1.0f;
    constexpr float POISON_DAMAGE_MULTIPLIER = 0.0f;     // Immune

    // Population
    constexpr int BASE_POPULATION_CAP = 10;
    constexpr int DARK_OBELISK_POPULATION = 10;
    constexpr int BLOOD_FOUNTAIN_POPULATION = 5;
    constexpr int MAX_POPULATION = 200;

    // Hero revival
    constexpr float HERO_REVIVE_TIME_BASE = 30.0f;
    constexpr float HERO_REVIVE_TIME_PER_LEVEL = 5.0f;
    constexpr float HERO_REVIVE_COST_REDUCTION = 0.50f;
}

// ============================================================================
// Day/Night System
// ============================================================================

enum class TimeOfDay {
    Day,
    Twilight,
    Night
};

/**
 * @brief Manages the day/night cycle for vampires
 */
class DayNightManager {
public:
    static DayNightManager& Instance();

    // Non-copyable
    DayNightManager(const DayNightManager&) = delete;
    DayNightManager& operator=(const DayNightManager&) = delete;

    /**
     * @brief Update the day/night cycle
     */
    void Update(float deltaTime);

    /**
     * @brief Get current time of day
     */
    [[nodiscard]] TimeOfDay GetTimeOfDay() const { return m_currentTime; }

    /**
     * @brief Check if it's currently night
     */
    [[nodiscard]] bool IsNight() const { return m_currentTime == TimeOfDay::Night; }

    /**
     * @brief Check if it's currently day
     */
    [[nodiscard]] bool IsDay() const { return m_currentTime == TimeOfDay::Day; }

    /**
     * @brief Get the stat modifier for vampires based on time
     */
    [[nodiscard]] float GetVampireStatModifier() const;

    /**
     * @brief Get current cycle progress (0-1)
     */
    [[nodiscard]] float GetCycleProgress() const { return m_cycleTimer / m_cycleDuration; }

    /**
     * @brief Force night time (for abilities like Crimson Night)
     */
    void ForceNight(float duration);

    /**
     * @brief Check if night is forced
     */
    [[nodiscard]] bool IsNightForced() const { return m_forcedNightDuration > 0.0f; }

    /**
     * @brief Set callback for day/night changes
     */
    using TimeChangeCallback = std::function<void(TimeOfDay newTime)>;
    void SetOnTimeChange(TimeChangeCallback cb) { m_onTimeChange = std::move(cb); }

private:
    DayNightManager() = default;

    TimeOfDay m_currentTime = TimeOfDay::Day;
    float m_cycleTimer = 0.0f;
    float m_cycleDuration = VampireConstants::DAY_DURATION;
    float m_forcedNightDuration = 0.0f;
    TimeChangeCallback m_onTimeChange;
};

// ============================================================================
// Blood Resource System
// ============================================================================

/**
 * @brief Manages the Blood resource for vampires
 */
class BloodResourceManager {
public:
    static BloodResourceManager& Instance();

    // Non-copyable
    BloodResourceManager(const BloodResourceManager&) = delete;
    BloodResourceManager& operator=(const BloodResourceManager&) = delete;

    /**
     * @brief Update blood decay
     */
    void Update(float deltaTime);

    /**
     * @brief Add blood from a kill
     */
    void AddBlood(int amount);

    /**
     * @brief Spend blood for an ability or unit
     */
    bool SpendBlood(int amount);

    /**
     * @brief Check if we have enough blood
     */
    [[nodiscard]] bool HasBlood(int amount) const { return m_currentBlood >= amount; }

    /**
     * @brief Get current blood amount
     */
    [[nodiscard]] int GetBlood() const { return m_currentBlood; }

    /**
     * @brief Get maximum blood storage
     */
    [[nodiscard]] int GetMaxBlood() const { return m_maxBlood; }

    /**
     * @brief Increase maximum blood storage
     */
    void IncreaseMaxBlood(int amount) { m_maxBlood += amount; }

    /**
     * @brief Called when an enemy unit is killed
     */
    void OnUnitKilled(const std::string& unitType, bool isWorker, bool isHero);

    /**
     * @brief Set blood efficiency (cost reduction)
     */
    void SetBloodEfficiency(float efficiency) { m_bloodEfficiency = efficiency; }

private:
    BloodResourceManager() = default;

    int m_currentBlood = 0;
    int m_maxBlood = VampireConstants::MAX_BLOOD_STORAGE;
    float m_bloodDecayAccumulator = 0.0f;
    float m_bloodEfficiency = 1.0f;
};

// ============================================================================
// Transformation System
// ============================================================================

enum class VampireForm {
    Humanoid,
    Bat,
    Wolf,
    Mist,
    Swarm
};

/**
 * @brief Represents a transformation state
 */
struct TransformationState {
    VampireForm currentForm = VampireForm::Humanoid;
    float transformCooldown = 0.0f;
    float formDuration = 0.0f;
    bool isTemporary = false;
};

/**
 * @brief Manages vampire transformations
 */
class TransformationManager {
public:
    static TransformationManager& Instance();

    // Non-copyable
    TransformationManager(const TransformationManager&) = delete;
    TransformationManager& operator=(const TransformationManager&) = delete;

    /**
     * @brief Update transformation timers
     */
    void Update(float deltaTime);

    /**
     * @brief Transform an entity to a new form
     */
    bool Transform(uint32_t entityId, VampireForm newForm, float duration = -1.0f);

    /**
     * @brief Revert to humanoid form
     */
    bool RevertForm(uint32_t entityId);

    /**
     * @brief Get current form of an entity
     */
    [[nodiscard]] VampireForm GetForm(uint32_t entityId) const;

    /**
     * @brief Check if entity can transform
     */
    [[nodiscard]] bool CanTransform(uint32_t entityId) const;

    /**
     * @brief Get transformation state
     */
    [[nodiscard]] const TransformationState* GetState(uint32_t entityId) const;

    /**
     * @brief Register a transformable entity
     */
    void RegisterEntity(uint32_t entityId);

    /**
     * @brief Unregister an entity
     */
    void UnregisterEntity(uint32_t entityId);

    /**
     * @brief Get stat modifiers for a form
     */
    struct FormModifiers {
        float speedBonus = 0.0f;
        float damageBonus = 0.0f;
        float armorBonus = 0.0f;
        bool isFlying = false;
        bool isInvulnerable = false;
        bool canAttack = true;
        bool canPassThroughUnits = false;
    };
    [[nodiscard]] FormModifiers GetFormModifiers(VampireForm form) const;

private:
    TransformationManager() = default;

    std::unordered_map<uint32_t, TransformationState> m_transformStates;
};

// ============================================================================
// Domination System
// ============================================================================

/**
 * @brief Represents a dominated unit
 */
struct DominatedUnit {
    uint32_t unitId = 0;
    uint32_t dominatorId = 0;
    float duration = -1.0f;  // -1 = permanent
    std::string originalRace;
    float statPenalty = VampireConstants::DOMINATED_STAT_PENALTY;
};

/**
 * @brief Manages mind control and domination
 */
class DominationManager {
public:
    static DominationManager& Instance();

    // Non-copyable
    DominationManager(const DominationManager&) = delete;
    DominationManager& operator=(const DominationManager&) = delete;

    /**
     * @brief Update dominated units
     */
    void Update(float deltaTime);

    /**
     * @brief Dominate an enemy unit
     */
    bool Dominate(uint32_t targetId, uint32_t dominatorId, float duration = -1.0f);

    /**
     * @brief Release a dominated unit
     */
    void Release(uint32_t unitId);

    /**
     * @brief Check if unit is dominated
     */
    [[nodiscard]] bool IsDominated(uint32_t unitId) const;

    /**
     * @brief Get domination info
     */
    [[nodiscard]] const DominatedUnit* GetDominationInfo(uint32_t unitId) const;

    /**
     * @brief Get number of dominated units
     */
    [[nodiscard]] size_t GetDominatedCount() const { return m_dominatedUnits.size(); }

    /**
     * @brief Check if can dominate more units
     */
    [[nodiscard]] bool CanDominateMore() const;

private:
    DominationManager() = default;

    std::unordered_map<uint32_t, DominatedUnit> m_dominatedUnits;
};

// ============================================================================
// Hero Revival System
// ============================================================================

/**
 * @brief Manages vampire hero revival at Coffin Vault
 */
class HeroRevivalManager {
public:
    static HeroRevivalManager& Instance();

    // Non-copyable
    HeroRevivalManager(const HeroRevivalManager&) = delete;
    HeroRevivalManager& operator=(const HeroRevivalManager&) = delete;

    /**
     * @brief Update revival timers
     */
    void Update(float deltaTime);

    /**
     * @brief Called when a vampire hero dies
     */
    void OnHeroDeath(uint32_t heroId, int level, const glm::vec3& deathPosition);

    /**
     * @brief Get time remaining for hero revival
     */
    [[nodiscard]] float GetRevivalTimeRemaining(uint32_t heroId) const;

    /**
     * @brief Instantly revive a hero (for a cost)
     */
    bool InstantRevive(uint32_t heroId);

    /**
     * @brief Set the coffin vault building
     */
    void SetCoffinVault(uint32_t buildingId, const glm::vec3& position);

    /**
     * @brief Check if coffin vault exists
     */
    [[nodiscard]] bool HasCoffinVault() const { return m_coffinVaultId != 0; }

    /**
     * @brief Set revival time reduction (from upgrades)
     */
    void SetRevivalTimeReduction(float reduction) { m_revivalTimeReduction = reduction; }

    /**
     * @brief Set revival cost reduction (from upgrades)
     */
    void SetRevivalCostReduction(float reduction) { m_revivalCostReduction = reduction; }

private:
    HeroRevivalManager() = default;

    struct RevivingHero {
        uint32_t heroId;
        int level;
        float timeRemaining;
        glm::vec3 deathPosition;
    };

    std::vector<RevivingHero> m_revivingHeroes;
    uint32_t m_coffinVaultId = 0;
    glm::vec3 m_coffinVaultPosition{0.0f};
    float m_revivalTimeReduction = 0.0f;
    float m_revivalCostReduction = VampireConstants::HERO_REVIVE_COST_REDUCTION;
};

// ============================================================================
// Vampire Race Manager
// ============================================================================

/**
 * @brief Main manager for the Vampire race mechanics
 */
class VampireRace {
public:
    static VampireRace& Instance();

    // Non-copyable
    VampireRace(const VampireRace&) = delete;
    VampireRace& operator=(const VampireRace&) = delete;

    /**
     * @brief Initialize the Vampire race
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update race mechanics
     */
    void Update(float deltaTime);

    // =========================================================================
    // Unit Management
    // =========================================================================

    /**
     * @brief Check if a unit is a vampire
     */
    [[nodiscard]] bool IsVampireUnit(uint32_t entityId) const;

    /**
     * @brief Register a vampire unit
     */
    void RegisterVampireUnit(uint32_t entityId, const std::string& unitType);

    /**
     * @brief Unregister a vampire unit
     */
    void UnregisterVampireUnit(uint32_t entityId);

    /**
     * @brief Apply vampire bonuses to a unit
     */
    void ApplyVampireBonuses(uint32_t entityId);

    /**
     * @brief Get damage multiplier for damage type
     */
    [[nodiscard]] float GetDamageMultiplier(const std::string& damageType) const;

    /**
     * @brief Calculate life steal amount
     */
    [[nodiscard]] float CalculateLifeSteal(uint32_t entityId, float damageDealt) const;

    // =========================================================================
    // Day/Night Effects
    // =========================================================================

    /**
     * @brief Apply day/night stat modifiers
     */
    void ApplyTimeOfDayEffects();

    /**
     * @brief Apply sunlight damage to vulnerable units
     */
    void ApplySunlightDamage(float deltaTime);

    /**
     * @brief Check if unit is vulnerable to sunlight
     */
    [[nodiscard]] bool IsVulnerableToSunlight(uint32_t entityId) const;

    // =========================================================================
    // Blood Resource
    // =========================================================================

    /**
     * @brief Get blood manager
     */
    BloodResourceManager& GetBloodManager() { return BloodResourceManager::Instance(); }

    /**
     * @brief Called when a unit is killed
     */
    void OnUnitKilled(uint32_t killerId, uint32_t victimId, const std::string& victimType);

    // =========================================================================
    // Transformation
    // =========================================================================

    /**
     * @brief Get transformation manager
     */
    TransformationManager& GetTransformationManager() { return TransformationManager::Instance(); }

    // =========================================================================
    // Domination
    // =========================================================================

    /**
     * @brief Get domination manager
     */
    DominationManager& GetDominationManager() { return DominationManager::Instance(); }

    // =========================================================================
    // Hero Revival
    // =========================================================================

    /**
     * @brief Get hero revival manager
     */
    HeroRevivalManager& GetHeroRevivalManager() { return HeroRevivalManager::Instance(); }

    // =========================================================================
    // Population
    // =========================================================================

    /**
     * @brief Get current population cap
     */
    [[nodiscard]] int GetPopulationCap() const;

    /**
     * @brief Get current population used
     */
    [[nodiscard]] int GetPopulationUsed() const { return m_populationUsed; }

    /**
     * @brief Add to population used
     */
    void AddPopulation(int amount) { m_populationUsed += amount; }

    /**
     * @brief Remove from population used
     */
    void RemovePopulation(int amount) { m_populationUsed = std::max(0, m_populationUsed - amount); }

    // =========================================================================
    // Building Tracking
    // =========================================================================

    /**
     * @brief Called when a vampire building is constructed
     */
    void OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType, const glm::vec3& position);

    /**
     * @brief Called when a vampire building is destroyed
     */
    void OnBuildingDestroyed(uint32_t buildingId);

    // =========================================================================
    // Configuration Loading
    // =========================================================================

    /**
     * @brief Load race configuration from JSON
     */
    bool LoadConfiguration(const std::string& configPath);

    /**
     * @brief Get unit configuration
     */
    [[nodiscard]] const nlohmann::json* GetUnitConfig(const std::string& unitType) const;

    /**
     * @brief Get building configuration
     */
    [[nodiscard]] const nlohmann::json* GetBuildingConfig(const std::string& buildingType) const;

    /**
     * @brief Get hero configuration
     */
    [[nodiscard]] const nlohmann::json* GetHeroConfig(const std::string& heroType) const;

private:
    VampireRace() = default;

    // Configuration data
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
    std::unordered_map<std::string, nlohmann::json> m_heroConfigs;

    // Unit tracking
    std::unordered_set<uint32_t> m_vampireUnits;
    std::unordered_map<uint32_t, std::string> m_unitTypes;
    std::unordered_set<uint32_t> m_sunlightVulnerableUnits;

    // Building tracking
    std::unordered_map<uint32_t, std::string> m_buildings;
    int m_darkObeliskCount = 0;
    int m_bloodFountainCount = 0;

    // Population
    int m_populationUsed = 0;

    // Life steal modifiers
    float m_bonusLifeSteal = 0.0f;

    // State
    bool m_initialized = false;
};

// ============================================================================
// Vampire Ability Implementations
// ============================================================================

/**
 * @brief Life Drain ability - channels to drain life from target
 */
class LifeDrainAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    uint32_t m_targetId = 0;
    float m_channelTime = 0.0f;
};

/**
 * @brief Transform ability - changes vampire form
 */
class TransformAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Dominate ability - mind controls enemy unit
 */
class DominateAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Blood Storm ability - AOE damage and heal
 */
class BloodStormAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;

private:
    int m_wavesRemaining = 0;
    float m_waveTimer = 0.0f;
};

/**
 * @brief Shadow Step ability - teleport behind target
 */
class ShadowStepAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Crimson Night ability - AOE night zone
 */
class CrimsonNightAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    float m_remainingDuration = 0.0f;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Register all vampire abilities with the ability manager
 */
void RegisterVampireAbilities();

/**
 * @brief Check if a unit type can transform
 */
[[nodiscard]] bool UnitCanTransform(const std::string& unitType);

/**
 * @brief Check if a unit type is vulnerable to sunlight
 */
[[nodiscard]] bool UnitVulnerableToSunlight(const std::string& unitType);

/**
 * @brief Get base life steal for a unit type
 */
[[nodiscard]] float GetBaseLifeSteal(const std::string& unitType);

} // namespace RTS
} // namespace Vehement
