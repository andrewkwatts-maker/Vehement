#pragma once

/**
 * @file CryptidRace.hpp
 * @brief Cryptid Race (The Hidden Ones) implementation for the RTS game
 *
 * Features:
 * - Transformation System: Shapeshifting units can transform into various forms
 * - Fear System: Terror-based units inflict fear stacks that debuff enemies
 * - Mist System: Terrain concealment for ambush attacks
 * - Ambush System: Bonus damage from concealment and stealth attacks
 * - Mimicry System: Units can copy enemy appearances and abilities
 * - Night Bonus: Enhanced stats during nighttime
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
#include <optional>

namespace Vehement {
namespace RTS {

// Forward declarations
class Entity;
class Building;
class Unit;
class Hero;

// ============================================================================
// Cryptid Race Constants
// ============================================================================

namespace CryptidConstants {
    // Fear system
    constexpr int MAX_FEAR_STACKS = 5;
    constexpr float FEAR_STACK_DURATION = 8.0f;          // Seconds per stack
    constexpr float FEAR_DAMAGE_REDUCTION_PER_STACK = 0.05f;  // 5% per stack
    constexpr float FEAR_SPEED_REDUCTION_PER_STACK = 0.04f;   // 4% per stack
    constexpr float FEAR_FLEE_THRESHOLD = 5;             // Stacks needed to flee
    constexpr float FEAR_SPREAD_RADIUS = 200.0f;         // For spreading panic talent

    // Transformation system
    constexpr float BASE_TRANSFORM_COOLDOWN = 10.0f;     // Seconds
    constexpr int BASE_TRANSFORM_ESSENCE_COST = 25;      // Essence per transform
    constexpr float TRANSFORM_TRANSITION_TIME = 0.5f;    // Visual transition

    // Mist system
    constexpr float MIST_SPREAD_RATE = 0.3f;             // Tiles per second
    constexpr float MIST_BASE_RADIUS = 10.0f;            // Base mist radius
    constexpr float MIST_CONCEALMENT_BONUS = 0.3f;       // Detection reduction in mist
    constexpr float MIST_DECAY_TIME = 60.0f;             // Seconds before mist fades

    // Ambush system
    constexpr float AMBUSH_DAMAGE_BONUS = 0.50f;         // 50% bonus from stealth
    constexpr float AMBUSH_CRIT_CHANCE = 0.25f;          // 25% crit from ambush
    constexpr float STEALTH_FADE_TIME = 3.0f;            // Seconds to become invisible
    constexpr float STEALTH_DETECTION_RANGE = 400.0f;    // Base detection range

    // Mimicry system
    constexpr float DISGUISE_DURATION = 60.0f;           // Seconds
    constexpr float COPY_ABILITY_DURATION = 45.0f;       // Seconds
    constexpr float COPY_FORM_DURATION = 30.0f;          // Seconds
    constexpr float COPY_EFFECTIVENESS = 0.9f;           // 90% of original stats

    // Day/Night cycle
    constexpr float NIGHT_DAMAGE_BONUS = 0.15f;          // 15% more damage at night
    constexpr float NIGHT_SPEED_BONUS = 0.10f;           // 10% faster at night
    constexpr float NIGHT_STEALTH_BONUS = 0.20f;         // 20% harder to detect at night
    constexpr float DAY_DAMAGE_PENALTY = 0.10f;          // 10% less damage during day
    constexpr float DAY_VISION_PENALTY = 0.15f;          // 15% less vision during day

    // Resources
    constexpr float ESSENCE_GENERATION_RATE = 0.5f;      // Per fear stack on enemies
    constexpr float DREAD_GENERATION_RATE = 0.1f;        // Per unit killed
    constexpr int BASE_ESSENCE_CAP = 500;
    constexpr int BASE_DREAD_CAP = 200;

    // Population
    constexpr int BASE_POPULATION_CAP = 10;
    constexpr int DEN_POPULATION = 8;
    constexpr int MAX_POPULATION = 200;

    // Wendigo growth
    constexpr float WENDIGO_HP_PER_KILL = 10.0f;         // HP gained per kill
    constexpr float WENDIGO_DAMAGE_PER_KILL = 1.0f;      // Damage gained per kill
    constexpr float WENDIGO_MAX_GROWTH_MULTIPLIER = 2.0f; // Max 200% of base stats
}

// ============================================================================
// Fear System
// ============================================================================

/**
 * @brief Represents fear stacks on an entity
 */
struct FearStatus {
    uint32_t entityId = 0;
    int stacks = 0;
    float duration = 0.0f;              // Time until oldest stack expires
    std::vector<float> stackTimers;     // Timer for each stack
    bool isFleeing = false;
    glm::vec3 fleeDirection{0.0f};
    uint32_t fearSource = 0;            // Entity that caused the fear

    [[nodiscard]] float GetDamageReduction() const {
        return static_cast<float>(stacks) * CryptidConstants::FEAR_DAMAGE_REDUCTION_PER_STACK;
    }

    [[nodiscard]] float GetSpeedReduction() const {
        return static_cast<float>(stacks) * CryptidConstants::FEAR_SPEED_REDUCTION_PER_STACK;
    }

    [[nodiscard]] bool ShouldFlee() const {
        return stacks >= CryptidConstants::FEAR_FLEE_THRESHOLD;
    }
};

/**
 * @brief Manages fear effects on entities
 */
class FearManager {
public:
    static FearManager& Instance();

    // Non-copyable
    FearManager(const FearManager&) = delete;
    FearManager& operator=(const FearManager&) = delete;

    /**
     * @brief Update all fear statuses
     */
    void Update(float deltaTime);

    /**
     * @brief Apply fear stack to an entity
     * @return Number of stacks after application
     */
    int ApplyFear(uint32_t targetId, uint32_t sourceId, int stacks = 1);

    /**
     * @brief Remove fear stacks from an entity
     */
    void RemoveFear(uint32_t entityId, int stacks = -1);

    /**
     * @brief Get fear status for an entity
     */
    FearStatus* GetFearStatus(uint32_t entityId);

    /**
     * @brief Check if entity is feared
     */
    [[nodiscard]] bool IsFeared(uint32_t entityId) const;

    /**
     * @brief Check if entity is fleeing
     */
    [[nodiscard]] bool IsFleeing(uint32_t entityId) const;

    /**
     * @brief Get fear stacks on entity
     */
    [[nodiscard]] int GetFearStacks(uint32_t entityId) const;

    /**
     * @brief Spread fear to nearby enemies (Spreading Panic talent)
     */
    void SpreadFear(uint32_t sourceEntityId, float radius, float spreadChance);

    /**
     * @brief Check if entity is immune to fear
     */
    [[nodiscard]] bool IsFearImmune(uint32_t entityId) const;

    /**
     * @brief Register a fear-immune entity
     */
    void RegisterFearImmune(uint32_t entityId);

    /**
     * @brief Unregister fear immunity
     */
    void UnregisterFearImmune(uint32_t entityId);

    /**
     * @brief Get all feared entities
     */
    const std::unordered_map<uint32_t, FearStatus>& GetAllFearStatuses() const { return m_fearStatuses; }

    /**
     * @brief Set fear duration modifier (from talents)
     */
    void SetFearDurationModifier(float mod) { m_fearDurationModifier = mod; }

private:
    FearManager() = default;

    std::unordered_map<uint32_t, FearStatus> m_fearStatuses;
    std::unordered_set<uint32_t> m_fearImmuneEntities;
    float m_fearDurationModifier = 1.0f;
};

// ============================================================================
// Transformation System
// ============================================================================

/**
 * @brief Represents a transformation form
 */
struct TransformForm {
    std::string formId;
    std::string formName;
    std::string baseUnitType;           // Stats template
    int essenceCost = 0;
    float cooldown = 0.0f;
    float duration = 0.0f;              // 0 = permanent until changed

    // Stat modifiers relative to base
    float healthMultiplier = 1.0f;
    float damageMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float armorBonus = 0.0f;

    // Special properties
    bool canFly = false;
    bool canSwim = false;
    bool hasCloak = false;
    std::vector<std::string> grantedAbilities;
};

/**
 * @brief Current transformation state for a unit
 */
struct TransformState {
    uint32_t entityId = 0;
    std::string currentForm;
    std::string baseForm;               // Original form
    float transformCooldown = 0.0f;
    float formDuration = 0.0f;          // Remaining time in current form
    bool isTransforming = false;
    float transformProgress = 0.0f;

    std::unordered_map<std::string, float> formCooldowns;  // Per-form cooldowns
};

/**
 * @brief Manages shapeshifting transformations
 */
class TransformationManager {
public:
    static TransformationManager& Instance();

    // Non-copyable
    TransformationManager(const TransformationManager&) = delete;
    TransformationManager& operator=(const TransformationManager&) = delete;

    /**
     * @brief Update all transformation states
     */
    void Update(float deltaTime);

    /**
     * @brief Register available form for a unit
     */
    void RegisterForm(const std::string& formId, const TransformForm& form);

    /**
     * @brief Get a registered form
     */
    const TransformForm* GetForm(const std::string& formId) const;

    /**
     * @brief Register a transforming unit
     */
    void RegisterTransformer(uint32_t entityId, const std::string& baseForm,
                            const std::vector<std::string>& availableForms);

    /**
     * @brief Unregister a transforming unit
     */
    void UnregisterTransformer(uint32_t entityId);

    /**
     * @brief Begin transformation to a new form
     * @return True if transformation started
     */
    bool StartTransformation(uint32_t entityId, const std::string& targetForm);

    /**
     * @brief Cancel ongoing transformation
     */
    void CancelTransformation(uint32_t entityId);

    /**
     * @brief Revert to base form
     */
    void RevertToBase(uint32_t entityId);

    /**
     * @brief Get current form of entity
     */
    [[nodiscard]] std::string GetCurrentForm(uint32_t entityId) const;

    /**
     * @brief Check if entity can transform to a form
     */
    [[nodiscard]] bool CanTransform(uint32_t entityId, const std::string& targetForm) const;

    /**
     * @brief Get transformation state
     */
    TransformState* GetTransformState(uint32_t entityId);

    /**
     * @brief Get available forms for entity
     */
    std::vector<std::string> GetAvailableForms(uint32_t entityId) const;

    /**
     * @brief Check if entity is currently transforming
     */
    [[nodiscard]] bool IsTransforming(uint32_t entityId) const;

    /**
     * @brief Set cooldown reduction modifier (from talents)
     */
    void SetCooldownModifier(float mod) { m_cooldownModifier = mod; }

    /**
     * @brief Set essence cost modifier (from talents)
     */
    void SetCostModifier(float mod) { m_costModifier = mod; }

private:
    TransformationManager() = default;

    void CompleteTransformation(uint32_t entityId);

    std::unordered_map<std::string, TransformForm> m_forms;
    std::unordered_map<uint32_t, TransformState> m_transformers;
    std::unordered_map<uint32_t, std::vector<std::string>> m_availableForms;

    float m_cooldownModifier = 1.0f;
    float m_costModifier = 1.0f;
};

// ============================================================================
// Mist System
// ============================================================================

/**
 * @brief Represents a mist tile for concealment
 */
struct MistTile {
    glm::ivec2 position{0, 0};
    float intensity = 1.0f;             // 0-1, affects concealment
    float decayTimer = 0.0f;
    uint32_t sourceBuildingId = 0;
    bool isPermanent = false;           // From mist generators
};

/**
 * @brief Manages mist terrain for concealment
 */
class MistManager {
public:
    static MistManager& Instance();

    // Non-copyable
    MistManager(const MistManager&) = delete;
    MistManager& operator=(const MistManager&) = delete;

    /**
     * @brief Update mist spread and decay
     */
    void Update(float deltaTime);

    /**
     * @brief Add mist source (from mist generator building)
     */
    void AddMistSource(uint32_t buildingId, const glm::vec3& position, float radius, bool permanent = true);

    /**
     * @brief Remove mist source
     */
    void RemoveMistSource(uint32_t buildingId);

    /**
     * @brief Create temporary mist at position (from abilities)
     */
    void CreateTemporaryMist(const glm::vec3& position, float radius, float duration);

    /**
     * @brief Check if position is in mist
     */
    [[nodiscard]] bool IsInMist(const glm::vec3& position) const;

    /**
     * @brief Get mist intensity at position
     */
    [[nodiscard]] float GetMistIntensity(const glm::vec3& position) const;

    /**
     * @brief Get concealment bonus at position
     */
    [[nodiscard]] float GetConcealmentBonus(const glm::vec3& position) const;

    /**
     * @brief Get all misted tiles
     */
    const std::unordered_set<uint64_t>& GetMistedTiles() const { return m_mistedTiles; }

    /**
     * @brief Clear all temporary mist
     */
    void ClearTemporaryMist();

private:
    MistManager() = default;

    void SpreadMist(const glm::ivec2& center, float radius, uint32_t sourceId, bool permanent);
    uint64_t TileKey(int x, int y) const { return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y); }

    struct MistSource {
        uint32_t buildingId;
        glm::vec3 position;
        float radius;
        bool permanent;
    };

    std::vector<MistSource> m_mistSources;
    std::unordered_set<uint64_t> m_mistedTiles;
    std::unordered_map<uint64_t, MistTile> m_mistData;
    std::vector<std::pair<uint64_t, float>> m_temporaryMist;  // Tile key and decay timer
};

// ============================================================================
// Mimicry System
// ============================================================================

/**
 * @brief Represents a disguise state
 */
struct DisguiseState {
    uint32_t entityId = 0;
    uint32_t copiedEntityId = 0;        // Entity being mimicked
    std::string copiedUnitType;
    float duration = 0.0f;
    bool isActive = false;
    bool appearsAsEnemy = false;        // For identity theft

    // Copied stats (at copy effectiveness)
    int copiedHealth = 0;
    int copiedDamage = 0;
    float copiedSpeed = 0.0f;
    std::vector<std::string> copiedAbilities;
};

/**
 * @brief Manages mimicry and disguise mechanics
 */
class MimicryManager {
public:
    static MimicryManager& Instance();

    // Non-copyable
    MimicryManager(const MimicryManager&) = delete;
    MimicryManager& operator=(const MimicryManager&) = delete;

    /**
     * @brief Update all disguise states
     */
    void Update(float deltaTime);

    /**
     * @brief Start disguise as another unit
     */
    bool StartDisguise(uint32_t entityId, uint32_t targetId);

    /**
     * @brief End disguise
     */
    void EndDisguise(uint32_t entityId, bool explosive = false);

    /**
     * @brief Copy abilities from target
     */
    bool CopyAbilities(uint32_t entityId, uint32_t targetId, int abilityCount = 1);

    /**
     * @brief Make target appear as enemy to allies (identity theft)
     */
    bool ApplyIdentityTheft(uint32_t targetId, float duration);

    /**
     * @brief Get disguise state for entity
     */
    DisguiseState* GetDisguiseState(uint32_t entityId);

    /**
     * @brief Check if entity is disguised
     */
    [[nodiscard]] bool IsDisguised(uint32_t entityId) const;

    /**
     * @brief Check if entity appears as enemy (identity theft victim)
     */
    [[nodiscard]] bool AppearsAsEnemy(uint32_t entityId) const;

    /**
     * @brief Get the apparent unit type of an entity
     */
    [[nodiscard]] std::string GetApparentUnitType(uint32_t entityId) const;

    /**
     * @brief Reveal a disguised entity (from detection or attack)
     */
    void RevealDisguise(uint32_t entityId);

    /**
     * @brief Set copy effectiveness modifier (from talents)
     */
    void SetCopyEffectiveness(float eff) { m_copyEffectiveness = eff; }

private:
    MimicryManager() = default;

    std::unordered_map<uint32_t, DisguiseState> m_disguises;
    std::unordered_set<uint32_t> m_identityTheftVictims;
    float m_copyEffectiveness = CryptidConstants::COPY_EFFECTIVENESS;
};

// ============================================================================
// Ambush System
// ============================================================================

/**
 * @brief Stealth state for ambush units
 */
struct StealthState {
    uint32_t entityId = 0;
    bool isStealthed = false;
    bool isStationary = false;
    float fadeProgress = 0.0f;          // 0-1, time spent fading
    float bonusDamage = 0.0f;           // Accumulated from staying hidden
    bool canAttackWithoutBreaking = false;  // One with Darkness talent
    int freeAttacksRemaining = 0;
};

/**
 * @brief Manages ambush and stealth mechanics
 */
class AmbushManager {
public:
    static AmbushManager& Instance();

    // Non-copyable
    AmbushManager(const AmbushManager&) = delete;
    AmbushManager& operator=(const AmbushManager&) = delete;

    /**
     * @brief Update stealth states
     */
    void Update(float deltaTime);

    /**
     * @brief Register a stealth-capable unit
     */
    void RegisterStealthUnit(uint32_t entityId);

    /**
     * @brief Unregister a stealth unit
     */
    void UnregisterStealthUnit(uint32_t entityId);

    /**
     * @brief Enter stealth mode
     */
    bool EnterStealth(uint32_t entityId, bool instant = false);

    /**
     * @brief Exit stealth mode
     */
    void ExitStealth(uint32_t entityId);

    /**
     * @brief Get stealth state
     */
    StealthState* GetStealthState(uint32_t entityId);

    /**
     * @brief Check if entity is stealthed
     */
    [[nodiscard]] bool IsStealthed(uint32_t entityId) const;

    /**
     * @brief Calculate ambush damage bonus
     */
    [[nodiscard]] float GetAmbushDamageBonus(uint32_t entityId) const;

    /**
     * @brief Mark entity as stationary (for passive stealth)
     */
    void SetStationary(uint32_t entityId, bool stationary);

    /**
     * @brief Calculate detection chance for stealthed unit
     * @return 0-1 probability of detection
     */
    [[nodiscard]] float CalculateDetectionChance(uint32_t stealthedId, uint32_t detectorId,
                                                  float detectionRange) const;

    /**
     * @brief Notify that an attack from stealth occurred
     */
    void OnStealthAttack(uint32_t entityId);

    /**
     * @brief Set ambush damage modifier (from talents)
     */
    void SetAmbushDamageModifier(float mod) { m_ambushDamageModifier = mod; }

    /**
     * @brief Set detection difficulty modifier (from talents)
     */
    void SetDetectionModifier(float mod) { m_detectionModifier = mod; }

private:
    AmbushManager() = default;

    std::unordered_map<uint32_t, StealthState> m_stealthStates;
    float m_ambushDamageModifier = 1.0f;
    float m_detectionModifier = 1.0f;
};

// ============================================================================
// Wendigo Growth System
// ============================================================================

/**
 * @brief Tracks wendigo growth from kills
 */
struct WendigoGrowth {
    uint32_t entityId = 0;
    int kills = 0;
    float bonusHealth = 0.0f;
    float bonusDamage = 0.0f;
    float currentMultiplier = 1.0f;

    void AddKill() {
        ++kills;
        bonusHealth += CryptidConstants::WENDIGO_HP_PER_KILL;
        bonusDamage += CryptidConstants::WENDIGO_DAMAGE_PER_KILL;
        currentMultiplier = std::min(
            1.0f + (static_cast<float>(kills) * 0.1f),
            CryptidConstants::WENDIGO_MAX_GROWTH_MULTIPLIER
        );
    }
};

// ============================================================================
// Cryptid Race Manager
// ============================================================================

/**
 * @brief Main manager for the Cryptid race mechanics
 */
class CryptidRace {
public:
    static CryptidRace& Instance();

    // Non-copyable
    CryptidRace(const CryptidRace&) = delete;
    CryptidRace& operator=(const CryptidRace&) = delete;

    /**
     * @brief Initialize the Cryptid race
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
     * @brief Check if a unit is a cryptid
     */
    [[nodiscard]] bool IsCryptidUnit(uint32_t entityId) const;

    /**
     * @brief Register a cryptid unit
     */
    void RegisterCryptidUnit(uint32_t entityId, const std::string& unitType);

    /**
     * @brief Unregister a cryptid unit
     */
    void UnregisterCryptidUnit(uint32_t entityId);

    /**
     * @brief Apply cryptid bonuses to a unit
     */
    void ApplyCryptidBonuses(uint32_t entityId);

    /**
     * @brief Get damage multiplier based on time of day
     */
    [[nodiscard]] float GetTimeOfDayMultiplier() const;

    // =========================================================================
    // Fear System Interface
    // =========================================================================

    /**
     * @brief Apply fear to a target
     */
    int ApplyFearToTarget(uint32_t targetId, uint32_t sourceId, int stacks = 1);

    /**
     * @brief Get fear bonus damage against target
     */
    [[nodiscard]] float GetFearBonusDamage(uint32_t targetId) const;

    /**
     * @brief Check if target should flee
     */
    [[nodiscard]] bool ShouldTargetFlee(uint32_t targetId) const;

    // =========================================================================
    // Transformation Interface
    // =========================================================================

    /**
     * @brief Transform unit to new form
     */
    bool TransformUnit(uint32_t entityId, const std::string& targetForm);

    /**
     * @brief Revert unit to base form
     */
    void RevertUnitForm(uint32_t entityId);

    /**
     * @brief Get current essence cost for transformation
     */
    [[nodiscard]] int GetTransformEssenceCost(uint32_t entityId, const std::string& targetForm) const;

    // =========================================================================
    // Ambush Interface
    // =========================================================================

    /**
     * @brief Calculate total ambush damage for an attack
     */
    [[nodiscard]] float CalculateAmbushDamage(uint32_t attackerId, float baseDamage) const;

    /**
     * @brief Check if entity is concealed (in mist or stealthed)
     */
    [[nodiscard]] bool IsConcealed(uint32_t entityId) const;

    // =========================================================================
    // Mimicry Interface
    // =========================================================================

    /**
     * @brief Copy an enemy unit
     */
    bool CopyEnemyUnit(uint32_t entityId, uint32_t targetId);

    /**
     * @brief Get the visual appearance of a unit (may be disguised)
     */
    [[nodiscard]] std::string GetUnitAppearance(uint32_t entityId) const;

    // =========================================================================
    // Resource Management
    // =========================================================================

    /**
     * @brief Get current essence
     */
    [[nodiscard]] int GetEssence() const { return m_essence; }

    /**
     * @brief Get current dread
     */
    [[nodiscard]] int GetDread() const { return m_dread; }

    /**
     * @brief Add essence
     */
    void AddEssence(int amount);

    /**
     * @brief Spend essence
     */
    bool SpendEssence(int amount);

    /**
     * @brief Add dread
     */
    void AddDread(int amount);

    /**
     * @brief Spend dread
     */
    bool SpendDread(int amount);

    /**
     * @brief Generate essence from feared enemies
     */
    void GenerateEssenceFromFear();

    // =========================================================================
    // Building Management
    // =========================================================================

    /**
     * @brief Check if building can be placed
     */
    [[nodiscard]] bool CanPlaceBuilding(const std::string& buildingType, const glm::vec3& position) const;

    /**
     * @brief Called when building is constructed
     */
    void OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType, const glm::vec3& position);

    /**
     * @brief Called when building is destroyed
     */
    void OnBuildingDestroyed(uint32_t buildingId);

    // =========================================================================
    // Wendigo Growth
    // =========================================================================

    /**
     * @brief Register a wendigo for growth tracking
     */
    void RegisterWendigo(uint32_t entityId);

    /**
     * @brief Notify wendigo kill for growth
     */
    void OnWendigoKill(uint32_t wendigoId);

    /**
     * @brief Get wendigo growth data
     */
    const WendigoGrowth* GetWendigoGrowth(uint32_t entityId) const;

    // =========================================================================
    // Day/Night Cycle
    // =========================================================================

    /**
     * @brief Set current time of day (0-1, 0 = midnight, 0.5 = noon)
     */
    void SetTimeOfDay(float time) { m_timeOfDay = time; }

    /**
     * @brief Get current time of day
     */
    [[nodiscard]] float GetTimeOfDay() const { return m_timeOfDay; }

    /**
     * @brief Check if it's currently night
     */
    [[nodiscard]] bool IsNight() const { return m_timeOfDay < 0.25f || m_timeOfDay > 0.75f; }

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
     * @brief Add to population
     */
    void AddPopulation(int amount) { m_populationUsed += amount; }

    /**
     * @brief Remove from population
     */
    void RemovePopulation(int amount) { m_populationUsed = std::max(0, m_populationUsed - amount); }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using FearAppliedCallback = std::function<void(uint32_t targetId, int stacks)>;
    using TransformCallback = std::function<void(uint32_t entityId, const std::string& newForm)>;
    using AmbushCallback = std::function<void(uint32_t attackerId, float bonusDamage)>;

    void SetOnFearApplied(FearAppliedCallback cb) { m_onFearApplied = std::move(cb); }
    void SetOnTransform(TransformCallback cb) { m_onTransform = std::move(cb); }
    void SetOnAmbush(AmbushCallback cb) { m_onAmbush = std::move(cb); }

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
    CryptidRace() = default;

    // Configuration data
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
    std::unordered_map<std::string, nlohmann::json> m_heroConfigs;

    // Unit tracking
    std::unordered_set<uint32_t> m_cryptidUnits;
    std::unordered_map<uint32_t, std::string> m_unitTypes;

    // Wendigo growth tracking
    std::unordered_map<uint32_t, WendigoGrowth> m_wendigoGrowth;

    // Building tracking
    std::unordered_map<uint32_t, std::string> m_buildings;
    int m_denCount = 0;

    // Resources
    int m_essence = 0;
    int m_dread = 0;
    int m_essenceCap = CryptidConstants::BASE_ESSENCE_CAP;
    int m_dreadCap = CryptidConstants::BASE_DREAD_CAP;

    // Population
    int m_populationUsed = 0;

    // Time of day (0-1)
    float m_timeOfDay = 0.0f;

    // State
    bool m_initialized = false;

    // Callbacks
    FearAppliedCallback m_onFearApplied;
    TransformCallback m_onTransform;
    AmbushCallback m_onAmbush;
};

// ============================================================================
// Cryptid Ability Implementations
// ============================================================================

/**
 * @brief Transform ability - shapeshifting between forms
 */
class TransformAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Terrify ability - applies fear stacks in area
 */
class TerrifyAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Paralyzing Gaze ability - stuns and fears a single target
 */
class ParalyzingGazeAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Shadow Meld ability - enter stealth with bonus damage
 */
class ShadowMeldAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Pounce ability - leap to target with damage and stun
 */
class PounceAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Copy Form ability - mimic disguises as target
 */
class CopyFormAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Reveal True Form ability - explosive reveal from disguise
 */
class RevealTrueFormAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Life Drain ability - damage that heals attacker
 */
class LifeDrainAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    uint32_t m_targetId = 0;
    float m_channelTime = 0.0f;
};

/**
 * @brief Mind Shatter ability - massive fear and damage
 */
class MindShatterAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Nightmare Walk ability - teleport with fear on arrival
 */
class NightmareWalkAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Prophecy of Doom ability - marks target for death
 */
class ProphecyOfDoomAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;

private:
    uint32_t m_targetId = 0;
    float m_doomTimer = 0.0f;
};

/**
 * @brief Consume ability - wendigo devours target for growth
 */
class ConsumeAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Eldritch Blast ability - reality-warping damage
 */
class EldritchBlastAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Register all cryptid abilities with the ability manager
 */
void RegisterCryptidAbilities();

/**
 * @brief Get the fear resistance of a unit type
 */
[[nodiscard]] float GetFearResistance(const std::string& unitType);

/**
 * @brief Check if a unit type can be mimicked
 */
[[nodiscard]] bool CanBeMimicked(const std::string& unitType);

/**
 * @brief Get transformation forms available to a unit type
 */
[[nodiscard]] std::vector<std::string> GetTransformForms(const std::string& unitType);

} // namespace RTS
} // namespace Vehement
