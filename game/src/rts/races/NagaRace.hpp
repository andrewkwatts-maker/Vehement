#pragma once

/**
 * @file NagaRace.hpp
 * @brief Depths of Nazjatar - Naga race implementation for the RTS game
 *
 * The Naga are an aquatic serpentine civilization with water magic.
 * Features include:
 * - Amphibious units that traverse water freely
 * - Tidal Power mechanic (bonuses near water)
 * - Venom system (DOT effects and debuffs)
 * - Water regeneration (healing in water)
 * - Water/Frost/Poison magic schools
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
// Naga Race Constants
// ============================================================================

namespace NagaConstants {
    // Tidal Power mechanics
    constexpr float TIDAL_POWER_RADIUS = 10.0f;           // Tiles from water
    constexpr float TIDAL_DAMAGE_BONUS = 0.15f;           // +15% damage near water
    constexpr float TIDAL_ABILITY_POWER_BONUS = 0.25f;    // +25% ability power near water

    // Water regeneration
    constexpr float WATER_HEALTH_REGEN_PERCENT = 2.0f;    // % max HP per second in water
    constexpr float NEAR_WATER_REGEN_BONUS = 1.0f;        // Flat bonus near water

    // Venom mechanics
    constexpr float BASE_VENOM_DAMAGE = 4.0f;             // Damage per tick
    constexpr float VENOM_TICK_INTERVAL = 1.0f;           // Seconds between ticks
    constexpr float VENOM_DURATION = 6.0f;                // Base duration
    constexpr int VENOM_MAX_STACKS = 3;                   // Maximum stacks
    constexpr float VENOM_HEALING_REDUCTION = 0.3f;       // 30% healing reduction

    // Amphibious movement
    constexpr float WATER_SPEED_BONUS = 0.3f;             // +30% speed in water
    constexpr float DEEP_WATER_SPEED_BONUS = 0.4f;        // +40% speed in deep water
    constexpr float DESERT_SPEED_PENALTY = -0.3f;         // -30% speed in desert

    // Fire vulnerability
    constexpr float FIRE_DAMAGE_MULTIPLIER = 1.25f;       // +25% fire damage taken

    // Building costs
    constexpr float BUILDING_COST_MULTIPLIER = 1.15f;     // 15% more expensive
    constexpr float WATER_ADJACENT_BUILDING_BONUS = 0.2f; // 20% bonus when built near water

    // Resource gathering
    constexpr float CORAL_GATHER_RATE = 1.0f;
    constexpr float CORAL_WATER_BONUS = 1.5f;             // +50% near water
    constexpr float PEARL_GATHER_RATE = 0.5f;
    constexpr float PEARL_WATER_BONUS = 2.0f;             // +100% in water
}

// ============================================================================
// Venom System
// ============================================================================

/**
 * @brief Venom effect on a target
 */
struct VenomEffect {
    uint32_t targetId = 0;
    uint32_t sourceId = 0;
    float damagePerTick = NagaConstants::BASE_VENOM_DAMAGE;
    float tickInterval = NagaConstants::VENOM_TICK_INTERVAL;
    float remainingDuration = NagaConstants::VENOM_DURATION;
    float timeSinceLastTick = 0.0f;
    int stacks = 1;
    int maxStacks = NagaConstants::VENOM_MAX_STACKS;
    float healingReduction = NagaConstants::VENOM_HEALING_REDUCTION;
    bool appliesSlowEffect = false;
    float slowAmount = 0.0f;

    /**
     * @brief Update venom effect
     * @return Damage dealt this frame
     */
    float Update(float deltaTime);

    /**
     * @brief Add a stack of venom
     */
    void AddStack(float damage, float duration);

    /**
     * @brief Check if venom has expired
     */
    [[nodiscard]] bool IsExpired() const { return remainingDuration <= 0.0f; }

    /**
     * @brief Get total damage per tick (accounting for stacks)
     */
    [[nodiscard]] float GetTotalDamagePerTick() const { return damagePerTick * stacks; }

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static VenomEffect FromJson(const nlohmann::json& j);
};

/**
 * @brief Venom system manager
 */
class VenomManager {
public:
    static VenomManager& Instance() {
        static VenomManager instance;
        return instance;
    }

    /**
     * @brief Apply venom to a target
     */
    void ApplyVenom(uint32_t targetId, uint32_t sourceId, float damage, float duration, int maxStacks = 3);

    /**
     * @brief Apply neurotoxin (venom with slow)
     */
    void ApplyNeurotoxin(uint32_t targetId, uint32_t sourceId, float damage, float duration, float slowAmount);

    /**
     * @brief Remove venom from target
     */
    void RemoveVenom(uint32_t targetId);

    /**
     * @brief Update all active venom effects
     */
    void Update(float deltaTime);

    /**
     * @brief Check if target has venom
     */
    [[nodiscard]] bool HasVenom(uint32_t targetId) const;

    /**
     * @brief Get venom stacks on target
     */
    [[nodiscard]] int GetVenomStacks(uint32_t targetId) const;

    /**
     * @brief Get healing reduction on target (from venom)
     */
    [[nodiscard]] float GetHealingReduction(uint32_t targetId) const;

    /**
     * @brief Clear all venom effects
     */
    void Clear();

private:
    VenomManager() = default;
    std::unordered_map<uint32_t, VenomEffect> m_activeVenoms;
};

// ============================================================================
// Tidal Power System
// ============================================================================

/**
 * @brief Represents a water tile for tidal power calculations
 */
struct WaterTile {
    glm::vec3 position{0.0f};
    bool isDeepWater = false;
    float powerBonus = 1.0f;
};

/**
 * @brief Tidal power manager
 */
class TidalPowerManager {
public:
    static TidalPowerManager& Instance() {
        static TidalPowerManager instance;
        return instance;
    }

    /**
     * @brief Register a water tile
     */
    void RegisterWaterTile(const glm::vec3& position, bool isDeepWater = false);

    /**
     * @brief Check if position is near water
     */
    [[nodiscard]] bool IsNearWater(const glm::vec3& position, float radius = NagaConstants::TIDAL_POWER_RADIUS) const;

    /**
     * @brief Check if position is in water
     */
    [[nodiscard]] bool IsInWater(const glm::vec3& position) const;

    /**
     * @brief Check if position is in deep water
     */
    [[nodiscard]] bool IsInDeepWater(const glm::vec3& position) const;

    /**
     * @brief Get tidal power bonus at position
     */
    [[nodiscard]] float GetTidalPowerBonus(const glm::vec3& position) const;

    /**
     * @brief Get damage bonus at position
     */
    [[nodiscard]] float GetDamageBonus(const glm::vec3& position) const;

    /**
     * @brief Get ability power bonus at position
     */
    [[nodiscard]] float GetAbilityPowerBonus(const glm::vec3& position) const;

    /**
     * @brief Get movement speed modifier at position
     */
    [[nodiscard]] float GetMovementModifier(const glm::vec3& position, bool isAmphibious) const;

    /**
     * @brief Get health regeneration rate at position
     */
    [[nodiscard]] float GetHealthRegenRate(const glm::vec3& position) const;

    /**
     * @brief Clear all water tiles
     */
    void Clear();

    /**
     * @brief Load water tiles from map data
     */
    void LoadFromMapData(const nlohmann::json& mapData);

private:
    TidalPowerManager() = default;
    std::vector<WaterTile> m_waterTiles;
    std::unordered_set<uint64_t> m_waterPositionHashes;

    uint64_t HashPosition(const glm::vec3& pos) const;
};

// ============================================================================
// Amphibious Component
// ============================================================================

/**
 * @brief Amphibious movement component for Naga units
 */
struct AmphibiousComponent {
    bool canSwim = true;
    bool canDive = false;                     // Can go invisible in water
    float swimSpeed = 1.3f;                   // Multiplier for water movement
    float landSpeed = 1.0f;                   // Multiplier for land movement
    float desertPenalty = 0.7f;               // Multiplier for desert movement
    bool isSubmerged = false;                 // Currently underwater (invisible)
    float submergeDuration = 0.0f;
    float maxSubmergeDuration = 10.0f;

    /**
     * @brief Update submerge state
     */
    void Update(float deltaTime, bool inWater);

    /**
     * @brief Toggle submerge state
     */
    bool ToggleSubmerge(bool inWater);

    /**
     * @brief Get current movement multiplier
     */
    [[nodiscard]] float GetMovementMultiplier(bool inWater, bool inDeepWater, bool inDesert) const;
};

// ============================================================================
// Multi-Head System (for Hydras)
// ============================================================================

/**
 * @brief Head component for multi-headed creatures
 */
struct HeadComponent {
    int headCount = 3;
    int maxHeads = 5;
    float damagePerHead = 18.0f;
    float regenTimePerHead = 30.0f;
    float currentRegenProgress = 0.0f;
    bool twoHeadsPerLost = false;             // Ancient Hydra ability
    std::vector<bool> headActive;

    /**
     * @brief Initialize heads
     */
    void Initialize(int count, int max, float damage);

    /**
     * @brief Lose a head
     * @return true if head was lost
     */
    bool LoseHead();

    /**
     * @brief Update head regeneration
     */
    void Update(float deltaTime);

    /**
     * @brief Get total attack damage
     */
    [[nodiscard]] float GetTotalDamage() const { return damagePerHead * headCount; }

    /**
     * @brief Get number of active heads
     */
    [[nodiscard]] int GetActiveHeads() const { return headCount; }
};

// ============================================================================
// Naga Race Class
// ============================================================================

/**
 * @brief Main class for the Naga race
 *
 * Manages race-specific mechanics including:
 * - Venom system for DOT and debuffs
 * - Tidal Power for water proximity bonuses
 * - Amphibious movement
 * - Water regeneration
 * - Multi-head attacks (Hydras)
 */
class NagaRace {
public:
    // Callbacks
    using VenomAppliedCallback = std::function<void(uint32_t targetId, int stacks)>;
    using TidalPowerActivatedCallback = std::function<void(uint32_t entityId, float bonus)>;
    using HeadRegeneratedCallback = std::function<void(uint32_t unitId, int newHeadCount)>;

    /**
     * @brief Get singleton instance
     */
    static NagaRace& Instance();

    // Delete copy/move
    NagaRace(const NagaRace&) = delete;
    NagaRace& operator=(const NagaRace&) = delete;
    NagaRace(NagaRace&&) = delete;
    NagaRace& operator=(NagaRace&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the Naga race
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
    // Venom Management
    // =========================================================================

    /**
     * @brief Apply venom to target
     */
    void ApplyVenom(uint32_t targetId, uint32_t sourceId, float damage = NagaConstants::BASE_VENOM_DAMAGE);

    /**
     * @brief Apply enhanced venom (upgraded)
     */
    void ApplyEnhancedVenom(uint32_t targetId, uint32_t sourceId);

    /**
     * @brief Apply neurotoxin (with slow)
     */
    void ApplyNeurotoxin(uint32_t targetId, uint32_t sourceId, float slowAmount = 0.3f);

    /**
     * @brief Get venom manager
     */
    [[nodiscard]] VenomManager& GetVenomManager() { return VenomManager::Instance(); }

    // =========================================================================
    // Tidal Power
    // =========================================================================

    /**
     * @brief Get tidal power manager
     */
    [[nodiscard]] TidalPowerManager& GetTidalPowerManager() { return TidalPowerManager::Instance(); }

    /**
     * @brief Calculate damage with tidal bonus
     */
    [[nodiscard]] float CalculateDamage(uint32_t attackerId, float baseDamage) const;

    /**
     * @brief Calculate ability power with tidal bonus
     */
    [[nodiscard]] float CalculateAbilityPower(uint32_t casterId, float baseAbilityPower) const;

    /**
     * @brief Apply water regeneration to unit
     */
    void ApplyWaterRegeneration(uint32_t unitId, float deltaTime);

    // =========================================================================
    // Amphibious Management
    // =========================================================================

    /**
     * @brief Register amphibious component
     */
    void RegisterAmphibious(uint32_t unitId, const AmphibiousComponent& component);

    /**
     * @brief Unregister amphibious component
     */
    void UnregisterAmphibious(uint32_t unitId);

    /**
     * @brief Get amphibious component
     */
    [[nodiscard]] AmphibiousComponent* GetAmphibious(uint32_t unitId);

    /**
     * @brief Toggle submerge for unit
     */
    bool ToggleSubmerge(uint32_t unitId);

    // =========================================================================
    // Multi-Head Management (Hydras)
    // =========================================================================

    /**
     * @brief Register head component
     */
    void RegisterHeadComponent(uint32_t unitId, const HeadComponent& component);

    /**
     * @brief Unregister head component
     */
    void UnregisterHeadComponent(uint32_t unitId);

    /**
     * @brief Get head component
     */
    [[nodiscard]] HeadComponent* GetHeadComponent(uint32_t unitId);

    /**
     * @brief Handle head lost event
     */
    void OnHeadLost(uint32_t unitId);

    /**
     * @brief Handle kill event (for head growth)
     */
    void OnKill(uint32_t killerUnitId);

    // =========================================================================
    // Unit/Building Creation
    // =========================================================================

    /**
     * @brief Create a unit with Naga-specific components
     */
    uint32_t CreateUnit(const std::string& unitType, const glm::vec3& position, uint32_t ownerId);

    /**
     * @brief Create a building with water bonuses
     */
    uint32_t CreateBuilding(const std::string& buildingType, const glm::vec3& position, uint32_t ownerId);

    // =========================================================================
    // Damage Modifiers
    // =========================================================================

    /**
     * @brief Apply fire vulnerability
     * @return Modified damage
     */
    [[nodiscard]] float ApplyFireVulnerability(uint32_t targetId, float damage, const std::string& damageType) const;

    /**
     * @brief Get building cost with modifiers
     */
    [[nodiscard]] float GetBuildingCost(const std::string& buildingType, const glm::vec3& position) const;

    // =========================================================================
    // Resource Modifiers
    // =========================================================================

    /**
     * @brief Get gathering rate for resource
     */
    [[nodiscard]] float GetGatherRate(const std::string& resourceType, const glm::vec3& position) const;

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

    void SetOnVenomApplied(VenomAppliedCallback callback) {
        m_onVenomApplied = std::move(callback);
    }

    void SetOnTidalPowerActivated(TidalPowerActivatedCallback callback) {
        m_onTidalPowerActivated = std::move(callback);
    }

    void SetOnHeadRegenerated(HeadRegeneratedCallback callback) {
        m_onHeadRegenerated = std::move(callback);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total venom damage dealt
     */
    [[nodiscard]] float GetTotalVenomDamageDealt() const { return m_totalVenomDamage; }

    /**
     * @brief Get number of units with tidal power bonus
     */
    [[nodiscard]] int GetUnitsWithTidalPower() const;

    /**
     * @brief Get total active hydra heads
     */
    [[nodiscard]] int GetTotalActiveHeads() const;

private:
    NagaRace() = default;
    ~NagaRace() = default;

    void UpdateVenom(float deltaTime);
    void UpdateAmphibious(float deltaTime);
    void UpdateHeads(float deltaTime);
    void UpdateWaterRegeneration(float deltaTime);
    void UpdateTidalPowerBonuses();

    void LoadConfiguration(const std::string& configPath);
    void InitializeDefaultConfigs();

    bool m_initialized = false;
    std::string m_configBasePath;

    // Amphibious system
    std::unordered_map<uint32_t, AmphibiousComponent> m_amphibiousComponents;

    // Multi-head system
    std::unordered_map<uint32_t, HeadComponent> m_headComponents;

    // Unit positions (for tidal power)
    std::unordered_map<uint32_t, glm::vec3> m_unitPositions;

    // Statistics
    float m_totalVenomDamage = 0.0f;

    // Callbacks
    VenomAppliedCallback m_onVenomApplied;
    TidalPowerActivatedCallback m_onTidalPowerActivated;
    HeadRegeneratedCallback m_onHeadRegenerated;

    // Configuration cache
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
};

// ============================================================================
// Naga-specific Ability Behaviors
// ============================================================================

/**
 * @brief Tidal Wave ability implementation
 */
class TidalWaveAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Frost Nova ability implementation
 */
class NagaFrostNovaAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Whirlpool ability implementation
 */
class WhirlpoolAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    struct WhirlpoolInstance {
        glm::vec3 position;
        float remainingDuration;
        float pullStrength;
        float damagePerSecond;
        float tickTimer;
    };
    std::vector<WhirlpoolInstance> m_activeWhirlpools;
};

/**
 * @brief Song of the Siren ability implementation
 */
class SongOfSirenAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Ravage ability implementation (Tidehunter ultimate)
 */
class RavageAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
};

/**
 * @brief Mass Charm ability implementation
 */
class MassCharmAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    std::vector<uint32_t> m_charmedUnits;
};

/**
 * @brief Kraken Wrath ability implementation
 */
class KrakenWrathAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

} // namespace RTS
} // namespace Vehement
