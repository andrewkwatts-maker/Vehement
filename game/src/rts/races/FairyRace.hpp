#pragma once

/**
 * @file FairyRace.hpp
 * @brief The Fae Court - Fairy race implementation for the RTS game
 *
 * The Fae Court is a nature-based magical civilization with illusions,
 * enchantments, and forest creatures. Features include:
 * - Night Power system with combat bonuses at night
 * - Illusion system for creating decoys and confusion
 * - Moon Well system for mana/health regeneration
 * - Living buildings that can uproot and move
 */

#include "../Culture.hpp"
#include "../TechTree.hpp"
#include "../Ability.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class Entity;
class Building;
class Unit;

// ============================================================================
// Fairy Race Constants
// ============================================================================

namespace FairyConstants {
    // Night Power mechanics
    constexpr float NIGHT_DAMAGE_BONUS = 0.15f;           // +15% damage at night
    constexpr float NIGHT_HEALING_BONUS = 0.20f;          // +20% healing at night
    constexpr float NIGHT_SPEED_BONUS = 0.10f;            // +10% movement at night
    constexpr float NIGHT_REGEN_BONUS = 0.25f;            // +25% health regen at night
    constexpr float DAY_PENALTY = 0.10f;                  // -10% damage during day

    // Day/Night cycle (in seconds)
    constexpr float DAY_DURATION = 300.0f;                // 5 minutes
    constexpr float NIGHT_DURATION = 300.0f;              // 5 minutes
    constexpr float DUSK_DAWN_DURATION = 30.0f;           // 30 second transition

    // Illusion mechanics
    constexpr float ILLUSION_DAMAGE_DEALT = 0.35f;        // Illusions deal 35% damage
    constexpr float ILLUSION_DAMAGE_TAKEN = 3.0f;         // Illusions take 300% damage
    constexpr float ILLUSION_DEFAULT_DURATION = 60.0f;    // 60 second default duration
    constexpr int MAX_ILLUSIONS_PER_UNIT = 2;
    constexpr float ILLUSION_MANA_DRAIN = 0.5f;           // Mana drained per second

    // Moon Well mechanics
    constexpr float MOON_WELL_MAX_MANA = 200.0f;
    constexpr float MOON_WELL_REGEN_RATE = 0.75f;         // Mana per second (faster at night)
    constexpr float MOON_WELL_NIGHT_REGEN_BONUS = 2.0f;   // 2x regen at night
    constexpr float MOON_WELL_HEAL_RATE = 10.0f;          // Health restored per mana
    constexpr float MOON_WELL_MANA_RESTORE_RATE = 8.0f;   // Mana restored to units per mana
    constexpr float MOON_WELL_RADIUS = 10.0f;

    // Living building mechanics
    constexpr float UPROOT_TIME = 3.0f;                   // Seconds to uproot
    constexpr float ROOT_TIME = 3.0f;                     // Seconds to root
    constexpr float UPROOTED_SPEED = 2.0f;                // Movement speed when uprooted
    constexpr float UPROOTED_ARMOR_PENALTY = 2.0f;        // Armor reduction when uprooted
    constexpr float ROOTED_REGEN_BONUS = 2.0f;            // Health regen when rooted

    // Fairy Ring teleportation
    constexpr float FAIRY_RING_COOLDOWN = 60.0f;
    constexpr int FAIRY_RING_MAX_UNITS = 12;
    constexpr float FAIRY_RING_CHANNEL_TIME = 3.0f;

    // Nature Bond mechanics
    constexpr float TREE_PROXIMITY_BONUS_RADIUS = 8.0f;
    constexpr float TREE_PROXIMITY_REGEN_BONUS = 0.5f;    // +50% regen near trees
    constexpr float FOREST_DAMAGE_BONUS = 0.10f;          // +10% damage in forests

    // Resource gathering
    constexpr float WISP_HARVEST_RATE = 1.0f;             // Standard harvest rate
    constexpr float ENTANGLE_GOLD_RATE = 0.8f;            // From entangled mine
    constexpr float WOOD_GATHER_BONUS = 0.15f;            // +15% wood gathering (nature affinity)

    // Iron vulnerability
    constexpr float IRON_DAMAGE_VULNERABILITY = 0.25f;    // +25% damage from iron weapons
}

// ============================================================================
// Time of Day System
// ============================================================================

/**
 * @brief Current time of day state
 */
enum class TimeOfDay : uint8_t {
    Day = 0,
    Dusk,
    Night,
    Dawn
};

/**
 * @brief Day/Night cycle manager for Fairy bonuses
 */
class DayNightCycle {
public:
    static DayNightCycle& Instance() {
        static DayNightCycle instance;
        return instance;
    }

    void Update(float deltaTime);

    [[nodiscard]] TimeOfDay GetTimeOfDay() const { return m_currentTime; }
    [[nodiscard]] bool IsNight() const { return m_currentTime == TimeOfDay::Night; }
    [[nodiscard]] bool IsDay() const { return m_currentTime == TimeOfDay::Day; }
    [[nodiscard]] bool IsTransition() const {
        return m_currentTime == TimeOfDay::Dusk || m_currentTime == TimeOfDay::Dawn;
    }

    /**
     * @brief Get night bonus multiplier (0.0-1.0)
     * Gradually transitions during dusk/dawn
     */
    [[nodiscard]] float GetNightIntensity() const;

    /**
     * @brief Get total elapsed time in current cycle
     */
    [[nodiscard]] float GetCycleTime() const { return m_cycleTime; }

    /**
     * @brief Force time of day (for abilities/debug)
     */
    void SetTimeOfDay(TimeOfDay time);

    /**
     * @brief Reset cycle
     */
    void Reset();

private:
    DayNightCycle() = default;

    TimeOfDay m_currentTime = TimeOfDay::Day;
    float m_cycleTime = 0.0f;
    float m_transitionProgress = 0.0f;
};

// ============================================================================
// Night Power System
// ============================================================================

/**
 * @brief Night power bonuses for a Fairy unit
 */
struct NightPowerComponent {
    float baseDamageBonus = FairyConstants::NIGHT_DAMAGE_BONUS;
    float baseHealingBonus = FairyConstants::NIGHT_HEALING_BONUS;
    float baseSpeedBonus = FairyConstants::NIGHT_SPEED_BONUS;
    float baseRegenBonus = FairyConstants::NIGHT_REGEN_BONUS;
    float dayPenalty = FairyConstants::DAY_PENALTY;

    // Talent modifiers
    float talentDamageBonus = 0.0f;
    float talentDayBonus = 0.0f;  // Eternal Twilight talent

    /**
     * @brief Get current damage multiplier based on time of day
     */
    [[nodiscard]] float GetDamageMultiplier() const;

    /**
     * @brief Get current healing multiplier
     */
    [[nodiscard]] float GetHealingMultiplier() const;

    /**
     * @brief Get current speed multiplier
     */
    [[nodiscard]] float GetSpeedMultiplier() const;

    /**
     * @brief Get current health regen multiplier
     */
    [[nodiscard]] float GetRegenMultiplier() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static NightPowerComponent FromJson(const nlohmann::json& j);
};

// ============================================================================
// Illusion System
// ============================================================================

/**
 * @brief Illusion instance data
 */
struct IllusionInstance {
    uint32_t illusionId = 0;          // Entity ID of the illusion
    uint32_t sourceUnitId = 0;        // Entity ID of the original unit
    std::string unitType;
    float remainingDuration = 0.0f;
    float damageDealtMultiplier = FairyConstants::ILLUSION_DAMAGE_DEALT;
    float damageTakenMultiplier = FairyConstants::ILLUSION_DAMAGE_TAKEN;
    bool isRevealed = false;          // True if detected as illusion
    float manaDrainPerSecond = FairyConstants::ILLUSION_MANA_DRAIN;

    [[nodiscard]] bool IsExpired() const { return remainingDuration <= 0.0f; }

    void Update(float deltaTime) {
        remainingDuration -= deltaTime;
    }
};

/**
 * @brief Manages all active illusions for the Fairy race
 */
class IllusionManager {
public:
    static IllusionManager& Instance() {
        static IllusionManager instance;
        return instance;
    }

    /**
     * @brief Create an illusion of a unit
     * @return Entity ID of the created illusion, or 0 if failed
     */
    uint32_t CreateIllusion(uint32_t sourceUnitId, const std::string& unitType,
                           const glm::vec3& position, float duration,
                           float damageDealt = FairyConstants::ILLUSION_DAMAGE_DEALT,
                           float damageTaken = FairyConstants::ILLUSION_DAMAGE_TAKEN);

    /**
     * @brief Create multiple illusions (Mirror Image)
     */
    std::vector<uint32_t> CreateMirrorImages(uint32_t sourceUnitId, int count, float duration);

    /**
     * @brief Mass Illusion - create illusions of all units in area
     */
    std::vector<uint32_t> CreateMassIllusion(const glm::vec3& center, float radius,
                                              float duration, int copiesPerUnit = 1);

    /**
     * @brief Destroy an illusion
     */
    void DestroyIllusion(uint32_t illusionId);

    /**
     * @brief Destroy all illusions from a source unit
     */
    void DestroyIllusionsFromSource(uint32_t sourceUnitId);

    /**
     * @brief Check if entity is an illusion
     */
    [[nodiscard]] bool IsIllusion(uint32_t entityId) const;

    /**
     * @brief Get illusion data
     */
    [[nodiscard]] const IllusionInstance* GetIllusion(uint32_t illusionId) const;
    [[nodiscard]] IllusionInstance* GetIllusion(uint32_t illusionId);

    /**
     * @brief Get all illusions of a source unit
     */
    [[nodiscard]] std::vector<uint32_t> GetIllusionsOfUnit(uint32_t sourceUnitId) const;

    /**
     * @brief Reveal an illusion (detection abilities)
     */
    void RevealIllusion(uint32_t illusionId);

    /**
     * @brief Reveal all illusions in radius
     */
    void RevealIllusionsInRadius(const glm::vec3& center, float radius);

    /**
     * @brief Update all illusions
     */
    void Update(float deltaTime);

    /**
     * @brief Clear all illusions
     */
    void Clear();

    /**
     * @brief Get count of active illusions
     */
    [[nodiscard]] int GetActiveIllusionCount() const { return static_cast<int>(m_illusions.size()); }

private:
    IllusionManager() = default;

    std::unordered_map<uint32_t, IllusionInstance> m_illusions;
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_sourceToIllusions;
    uint32_t m_nextIllusionId = 500000;  // Start from high ID to avoid conflicts
};

// ============================================================================
// Moon Well System
// ============================================================================

/**
 * @brief Moon Well state and functionality
 */
struct MoonWellState {
    uint32_t buildingId = 0;
    glm::vec3 position{0.0f};
    float currentMana = FairyConstants::MOON_WELL_MAX_MANA;
    float maxMana = FairyConstants::MOON_WELL_MAX_MANA;
    float regenRate = FairyConstants::MOON_WELL_REGEN_RATE;
    float radius = FairyConstants::MOON_WELL_RADIUS;
    bool isActive = true;
    bool autoHeal = true;
    bool autoMana = true;

    // Talent modifiers
    float capacityBonus = 0.0f;
    float regenBonus = 0.0f;

    /**
     * @brief Update mana regeneration
     */
    void Update(float deltaTime);

    /**
     * @brief Use mana from the well
     * @return Amount of mana actually used
     */
    float UseMana(float amount);

    /**
     * @brief Get current mana percentage (0.0-1.0)
     */
    [[nodiscard]] float GetManaPercent() const {
        return maxMana > 0.0f ? currentMana / maxMana : 0.0f;
    }

    /**
     * @brief Check if position is in well radius
     */
    [[nodiscard]] bool IsInRange(const glm::vec3& pos) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static MoonWellState FromJson(const nlohmann::json& j);
};

/**
 * @brief Manages all Moon Wells for the Fairy race
 */
class MoonWellManager {
public:
    static MoonWellManager& Instance() {
        static MoonWellManager instance;
        return instance;
    }

    /**
     * @brief Register a Moon Well
     */
    void RegisterMoonWell(const MoonWellState& well);

    /**
     * @brief Unregister a Moon Well
     */
    void UnregisterMoonWell(uint32_t buildingId);

    /**
     * @brief Get Moon Well state
     */
    [[nodiscard]] MoonWellState* GetMoonWell(uint32_t buildingId);
    [[nodiscard]] const MoonWellState* GetMoonWell(uint32_t buildingId) const;

    /**
     * @brief Find nearest Moon Well to position
     */
    [[nodiscard]] MoonWellState* GetNearestMoonWell(const glm::vec3& position);

    /**
     * @brief Get all Moon Wells in range of position
     */
    [[nodiscard]] std::vector<MoonWellState*> GetMoonWellsInRange(const glm::vec3& position);

    /**
     * @brief Update all Moon Wells
     */
    void Update(float deltaTime);

    /**
     * @brief Process automatic healing/mana restore for units
     */
    void ProcessAutoRestore(float deltaTime);

    /**
     * @brief Get total mana in all wells
     */
    [[nodiscard]] float GetTotalMana() const;

    /**
     * @brief Clear all Moon Wells
     */
    void Clear();

private:
    MoonWellManager() = default;

    std::unordered_map<uint32_t, MoonWellState> m_moonWells;
};

// ============================================================================
// Living Building System
// ============================================================================

/**
 * @brief State of a living (ancients/trees) building
 */
enum class LivingBuildingState : uint8_t {
    Rooted = 0,
    Uprooting,
    Uprooted,
    Rooting
};

/**
 * @brief Living building component for trees and ancients
 */
struct LivingBuildingComponent {
    uint32_t buildingId = 0;
    LivingBuildingState state = LivingBuildingState::Rooted;
    float transitionProgress = 0.0f;
    float transitionTime = FairyConstants::UPROOT_TIME;
    float uprootedSpeed = FairyConstants::UPROOTED_SPEED;
    float armorPenalty = FairyConstants::UPROOTED_ARMOR_PENALTY;
    float rootedRegenBonus = FairyConstants::ROOTED_REGEN_BONUS;
    bool canAttackWhileUprooted = false;
    bool canGatherWhileUprooted = false;  // For wisps on Tree of Life

    // Combat stats when uprooted
    float uprootedDamage = 0.0f;
    float uprootedAttackRange = 0.0f;
    float uprootedAttackSpeed = 0.0f;

    /**
     * @brief Start uprooting process
     */
    void StartUproot();

    /**
     * @brief Start rooting process
     */
    void StartRoot();

    /**
     * @brief Update transition progress
     */
    void Update(float deltaTime);

    /**
     * @brief Check if building can move
     */
    [[nodiscard]] bool CanMove() const { return state == LivingBuildingState::Uprooted; }

    /**
     * @brief Check if building can produce
     */
    [[nodiscard]] bool CanProduce() const { return state == LivingBuildingState::Rooted; }

    /**
     * @brief Check if in transition
     */
    [[nodiscard]] bool IsTransitioning() const {
        return state == LivingBuildingState::Uprooting || state == LivingBuildingState::Rooting;
    }

    /**
     * @brief Get current health regen multiplier
     */
    [[nodiscard]] float GetRegenMultiplier() const;

    /**
     * @brief Get current armor modifier
     */
    [[nodiscard]] float GetArmorModifier() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static LivingBuildingComponent FromJson(const nlohmann::json& j);
};

// ============================================================================
// Fairy Ring Teleportation
// ============================================================================

/**
 * @brief Fairy Ring network for teleportation
 */
struct FairyRingNode {
    uint32_t buildingId = 0;
    glm::vec3 position{0.0f};
    float cooldownRemaining = 0.0f;
    bool isActive = true;
    std::string ringName;

    void Update(float deltaTime);
    [[nodiscard]] bool IsReady() const { return cooldownRemaining <= 0.0f && isActive; }
    void StartCooldown() { cooldownRemaining = FairyConstants::FAIRY_RING_COOLDOWN; }
};

/**
 * @brief Manages Fairy Ring teleportation network
 */
class FairyRingNetwork {
public:
    static FairyRingNetwork& Instance() {
        static FairyRingNetwork instance;
        return instance;
    }

    /**
     * @brief Register a Fairy Ring
     */
    void RegisterRing(const FairyRingNode& ring);

    /**
     * @brief Unregister a Fairy Ring
     */
    void UnregisterRing(uint32_t buildingId);

    /**
     * @brief Get all available destination rings from source
     */
    [[nodiscard]] std::vector<const FairyRingNode*> GetAvailableDestinations(uint32_t sourceRingId) const;

    /**
     * @brief Teleport units from one ring to another
     * @return true if teleport succeeded
     */
    bool TeleportUnits(uint32_t sourceRingId, uint32_t destRingId,
                       const std::vector<uint32_t>& unitIds);

    /**
     * @brief Update all rings
     */
    void Update(float deltaTime);

    /**
     * @brief Clear network
     */
    void Clear();

private:
    FairyRingNetwork() = default;

    std::unordered_map<uint32_t, FairyRingNode> m_rings;
};

// ============================================================================
// Nature Bond System
// ============================================================================

/**
 * @brief Nature bond component for tree proximity bonuses
 */
struct NatureBondComponent {
    uint32_t unitId = 0;
    float treeProximityBonus = FairyConstants::TREE_PROXIMITY_REGEN_BONUS;
    float forestDamageBonus = FairyConstants::FOREST_DAMAGE_BONUS;
    bool isNearTree = false;
    bool isInForest = false;
    int nearbyTreeCount = 0;

    /**
     * @brief Update proximity to trees
     */
    void UpdateTreeProximity(const glm::vec3& position);

    /**
     * @brief Get current regen bonus from trees
     */
    [[nodiscard]] float GetTreeRegenBonus() const;

    /**
     * @brief Get current damage bonus from forest
     */
    [[nodiscard]] float GetForestDamageBonus() const;
};

// ============================================================================
// Fairy Race Class
// ============================================================================

/**
 * @brief Main class for the Fairy (Fae Court) race
 *
 * Manages race-specific mechanics including:
 * - Night Power system for time-of-day bonuses
 * - Illusion system for decoys and confusion
 * - Moon Well system for area healing/mana
 * - Living building mechanics
 * - Fairy Ring teleportation network
 */
class FairyRace {
public:
    // Callbacks
    using IllusionDestroyedCallback = std::function<void(uint32_t illusionId, uint32_t sourceId)>;
    using MoonWellEmptyCallback = std::function<void(uint32_t wellId)>;
    using BuildingUprootedCallback = std::function<void(uint32_t buildingId)>;
    using TeleportCompleteCallback = std::function<void(uint32_t ringId, const std::vector<uint32_t>& unitIds)>;
    using NightFallCallback = std::function<void()>;
    using DaybreakCallback = std::function<void()>;

    /**
     * @brief Get singleton instance
     */
    static FairyRace& Instance();

    // Delete copy/move
    FairyRace(const FairyRace&) = delete;
    FairyRace& operator=(const FairyRace&) = delete;
    FairyRace(FairyRace&&) = delete;
    FairyRace& operator=(FairyRace&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the Fairy race
     * @param configPath Path to race configuration
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const std::string& configPath = "");

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update all race-specific systems
     */
    void Update(float deltaTime);

    // =========================================================================
    // Night Power Management
    // =========================================================================

    /**
     * @brief Register Night Power component for a unit
     */
    void RegisterNightPower(uint32_t unitId, const NightPowerComponent& nightPower);

    /**
     * @brief Unregister Night Power component
     */
    void UnregisterNightPower(uint32_t unitId);

    /**
     * @brief Get Night Power component
     */
    [[nodiscard]] NightPowerComponent* GetNightPower(uint32_t unitId);
    [[nodiscard]] const NightPowerComponent* GetNightPower(uint32_t unitId) const;

    /**
     * @brief Get current time of day
     */
    [[nodiscard]] TimeOfDay GetTimeOfDay() const { return DayNightCycle::Instance().GetTimeOfDay(); }

    /**
     * @brief Check if it's currently night
     */
    [[nodiscard]] bool IsNightTime() const { return DayNightCycle::Instance().IsNight(); }

    /**
     * @brief Get night intensity (0.0-1.0)
     */
    [[nodiscard]] float GetNightIntensity() const { return DayNightCycle::Instance().GetNightIntensity(); }

    // =========================================================================
    // Illusion Management
    // =========================================================================

    /**
     * @brief Create an illusion
     */
    uint32_t CreateIllusion(uint32_t sourceUnitId, const glm::vec3& position, float duration);

    /**
     * @brief Create mirror images
     */
    std::vector<uint32_t> CreateMirrorImages(uint32_t sourceUnitId, int count, float duration);

    /**
     * @brief Create mass illusion in area
     */
    std::vector<uint32_t> CreateMassIllusion(const glm::vec3& center, float radius,
                                              float duration, int copiesPerUnit = 1);

    /**
     * @brief Destroy an illusion
     */
    void DestroyIllusion(uint32_t illusionId);

    /**
     * @brief Check if entity is an illusion
     */
    [[nodiscard]] bool IsIllusion(uint32_t entityId) const;

    /**
     * @brief Apply damage to entity (with illusion multiplier)
     */
    float ApplyDamage(uint32_t entityId, float damage, bool isIronWeapon = false);

    /**
     * @brief Get illusion manager
     */
    [[nodiscard]] IllusionManager& GetIllusionManager() { return IllusionManager::Instance(); }

    // =========================================================================
    // Moon Well Management
    // =========================================================================

    /**
     * @brief Register a Moon Well
     */
    void RegisterMoonWell(uint32_t buildingId, const glm::vec3& position);

    /**
     * @brief Unregister a Moon Well
     */
    void UnregisterMoonWell(uint32_t buildingId);

    /**
     * @brief Get Moon Well manager
     */
    [[nodiscard]] MoonWellManager& GetMoonWellManager() { return MoonWellManager::Instance(); }

    /**
     * @brief Heal unit from Moon Well
     */
    float HealFromMoonWell(uint32_t unitId, float amount);

    /**
     * @brief Restore mana from Moon Well
     */
    float RestoreManaFromMoonWell(uint32_t unitId, float amount);

    // =========================================================================
    // Living Building Management
    // =========================================================================

    /**
     * @brief Register a living building
     */
    void RegisterLivingBuilding(uint32_t buildingId, const LivingBuildingComponent& component);

    /**
     * @brief Unregister a living building
     */
    void UnregisterLivingBuilding(uint32_t buildingId);

    /**
     * @brief Get living building component
     */
    [[nodiscard]] LivingBuildingComponent* GetLivingBuilding(uint32_t buildingId);
    [[nodiscard]] const LivingBuildingComponent* GetLivingBuilding(uint32_t buildingId) const;

    /**
     * @brief Uproot a building
     */
    bool UprootBuilding(uint32_t buildingId);

    /**
     * @brief Root a building
     */
    bool RootBuilding(uint32_t buildingId);

    /**
     * @brief Check if building can produce
     */
    [[nodiscard]] bool CanBuildingProduce(uint32_t buildingId) const;

    // =========================================================================
    // Fairy Ring Network
    // =========================================================================

    /**
     * @brief Register a Fairy Ring
     */
    void RegisterFairyRing(uint32_t buildingId, const glm::vec3& position, const std::string& name);

    /**
     * @brief Unregister a Fairy Ring
     */
    void UnregisterFairyRing(uint32_t buildingId);

    /**
     * @brief Teleport units between rings
     */
    bool TeleportViaFairyRing(uint32_t sourceRingId, uint32_t destRingId,
                               const std::vector<uint32_t>& unitIds);

    /**
     * @brief Get Fairy Ring network
     */
    [[nodiscard]] FairyRingNetwork& GetFairyRingNetwork() { return FairyRingNetwork::Instance(); }

    // =========================================================================
    // Nature Bond
    // =========================================================================

    /**
     * @brief Register Nature Bond component
     */
    void RegisterNatureBond(uint32_t unitId, const NatureBondComponent& component);

    /**
     * @brief Unregister Nature Bond component
     */
    void UnregisterNatureBond(uint32_t unitId);

    /**
     * @brief Get Nature Bond component
     */
    [[nodiscard]] NatureBondComponent* GetNatureBond(uint32_t unitId);

    /**
     * @brief Update all nature bonds
     */
    void UpdateNatureBonds();

    // =========================================================================
    // Unit/Building Creation
    // =========================================================================

    /**
     * @brief Create a Fairy unit with all race-specific components
     */
    uint32_t CreateUnit(const std::string& unitType, const glm::vec3& position, uint32_t ownerId);

    /**
     * @brief Create a Fairy building
     */
    uint32_t CreateBuilding(const std::string& buildingType, const glm::vec3& position, uint32_t ownerId);

    // =========================================================================
    // Resource Modifiers
    // =========================================================================

    /**
     * @brief Get gathering rate modifier
     */
    [[nodiscard]] float GetGatherRateModifier(const std::string& resourceType) const;

    /**
     * @brief Get cost modifier
     */
    [[nodiscard]] float GetCostModifier(const std::string& entityType) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Load unit configuration
     */
    [[nodiscard]] nlohmann::json LoadUnitConfig(const std::string& unitId) const;

    /**
     * @brief Load building configuration
     */
    [[nodiscard]] nlohmann::json LoadBuildingConfig(const std::string& buildingId) const;

    /**
     * @brief Load ability configuration
     */
    [[nodiscard]] nlohmann::json LoadAbilityConfig(const std::string& abilityId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnIllusionDestroyed(IllusionDestroyedCallback callback) {
        m_onIllusionDestroyed = std::move(callback);
    }

    void SetOnMoonWellEmpty(MoonWellEmptyCallback callback) {
        m_onMoonWellEmpty = std::move(callback);
    }

    void SetOnBuildingUprooted(BuildingUprootedCallback callback) {
        m_onBuildingUprooted = std::move(callback);
    }

    void SetOnTeleportComplete(TeleportCompleteCallback callback) {
        m_onTeleportComplete = std::move(callback);
    }

    void SetOnNightFall(NightFallCallback callback) {
        m_onNightFall = std::move(callback);
    }

    void SetOnDaybreak(DaybreakCallback callback) {
        m_onDaybreak = std::move(callback);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total active illusions
     */
    [[nodiscard]] int GetActiveIllusionCount() const;

    /**
     * @brief Get total Moon Well mana
     */
    [[nodiscard]] float GetTotalMoonWellMana() const;

    /**
     * @brief Get number of living buildings
     */
    [[nodiscard]] int GetLivingBuildingCount() const;

    /**
     * @brief Get number of uprooted buildings
     */
    [[nodiscard]] int GetUprootedBuildingCount() const;

private:
    FairyRace() = default;
    ~FairyRace() = default;

    void UpdateNightPower(float deltaTime);
    void UpdateIllusions(float deltaTime);
    void UpdateMoonWells(float deltaTime);
    void UpdateLivingBuildings(float deltaTime);
    void UpdateFairyRings(float deltaTime);

    void LoadConfiguration(const std::string& configPath);
    void InitializeDefaultConfigs();

    void CheckTimeOfDayTransition();

    bool m_initialized = false;
    std::string m_configBasePath;
    TimeOfDay m_previousTimeOfDay = TimeOfDay::Day;

    // Night Power system
    std::unordered_map<uint32_t, NightPowerComponent> m_nightPowers;

    // Living buildings
    std::unordered_map<uint32_t, LivingBuildingComponent> m_livingBuildings;

    // Nature Bond
    std::unordered_map<uint32_t, NatureBondComponent> m_natureBonds;

    // Callbacks
    IllusionDestroyedCallback m_onIllusionDestroyed;
    MoonWellEmptyCallback m_onMoonWellEmpty;
    BuildingUprootedCallback m_onBuildingUprooted;
    TeleportCompleteCallback m_onTeleportComplete;
    NightFallCallback m_onNightFall;
    DaybreakCallback m_onDaybreak;

    // Configuration cache
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
};

// ============================================================================
// Fairy-specific Ability Behaviors
// ============================================================================

/**
 * @brief Entangling Roots ability implementation
 */
class EntanglingRootsAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    struct RootInstance {
        uint32_t targetId;
        glm::vec3 position;
        float remainingDuration;
        float damagePerSecond;
        float tickTimer;
    };
    std::vector<RootInstance> m_activeRoots;
};

/**
 * @brief Force of Nature ability - summon Treants
 */
class ForceOfNatureAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;

private:
    int m_baseTreantCount = 2;
    int m_talentBonusTreants = 0;
};

/**
 * @brief Mirror Image ability - create illusions
 */
class MirrorImageAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Charm ability - mind control enemy
 */
class CharmAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    std::unordered_map<uint32_t, uint32_t> m_charmedUnits; // charmed -> original owner
    std::unordered_map<uint32_t, float> m_charmDurations;
};

/**
 * @brief Tranquility ability - area heal over time
 */
class TranquilityAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    struct TranquilityInstance {
        uint32_t casterId;
        glm::vec3 position;
        float remainingDuration;
        float healPerSecond;
        float radius;
        float tickTimer;
    };
    std::vector<TranquilityInstance> m_activeInstances;
};

/**
 * @brief Starfall ability - area damage from sky
 */
class StarfallAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    struct StarfallInstance {
        uint32_t casterId;
        glm::vec3 position;
        float remainingDuration;
        float damagePerWave;
        float radius;
        float waveTimer;
        float waveInterval;
    };
    std::vector<StarfallInstance> m_activeInstances;
};

/**
 * @brief Mass Illusion ability - create illusions of all nearby units
 */
class MassIllusionAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;

private:
    int m_copiesPerUnit = 1;
    int m_talentBonusCopies = 0;
};

/**
 * @brief Phase Shift ability - become untargetable
 */
class PhaseShiftAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    std::unordered_set<uint32_t> m_phasedUnits;
    std::unordered_map<uint32_t, float> m_phaseDurations;
};

/**
 * @brief Rebirth ability - Phoenix resurrection
 */
class RebirthAbility : public AbilityBehavior {
public:
    void OnDeath(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;

private:
    struct RebirthState {
        uint32_t phoenixId;
        glm::vec3 deathPosition;
        float respawnTimer;
        float healthPercent;
    };
    std::unordered_map<uint32_t, RebirthState> m_pendingRebirths;
    std::unordered_map<uint32_t, float> m_rebirthCooldowns;
};

} // namespace RTS
} // namespace Vehement
