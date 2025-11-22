#pragma once

/**
 * @file AlienRace.hpp
 * @brief The Collective - Alien race implementation for the RTS game
 *
 * The Collective is a technologically advanced extraterrestrial civilization
 * with energy weapons and psionic powers. Features include:
 * - Energy shields on all units
 * - Psionic abilities for casters
 * - Warp technology for rapid deployment
 * - Power grid mechanic for buildings
 */

#include "../Culture.hpp"
#include "../TechTree.hpp"
#include "../Ability.hpp"
#include <string>
#include <vector>
#include <unordered_map>
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
// Alien Race Constants
// ============================================================================

namespace AlienConstants {
    // Shield mechanics
    constexpr float SHIELD_REGEN_DELAY = 10.0f;         // Seconds before shield regen starts
    constexpr float BASE_SHIELD_REGEN_RATE = 2.0f;      // Shields per second (percent of max)
    constexpr int SHIELD_ARMOR_BASE = 0;

    // Power grid
    constexpr float PYLON_POWER_RADIUS = 6.5f;
    constexpr int PYLON_SUPPLY = 8;
    constexpr int NEXUS_SUPPLY = 15;

    // Warp mechanics
    constexpr float WARP_IN_TIME = 5.0f;
    constexpr float WARP_VULNERABILITY_DURATION = 5.0f;
    constexpr float WARP_PRISM_POWER_RADIUS = 4.0f;

    // Psionic mechanics
    constexpr float PSIONIC_DAMAGE_MULTIPLIER = 1.25f;
    constexpr float ENERGY_REGEN_PER_INT = 0.05f;

    // Resource gathering (slightly slower than standard)
    constexpr float MINERAL_GATHER_RATE = 0.9f;
    constexpr float VESPENE_GATHER_RATE = 0.85f;

    // Production modifiers
    constexpr float UNIT_COST_MULTIPLIER = 1.15f;
    constexpr float BUILDING_COST_MULTIPLIER = 1.2f;
    constexpr float PRODUCTION_TIME_MULTIPLIER = 1.1f;
}

// ============================================================================
// Shield System
// ============================================================================

/**
 * @brief Shield component for Collective units and buildings
 */
struct ShieldComponent {
    float maxShield = 0.0f;
    float currentShield = 0.0f;
    float shieldArmor = 0.0f;
    float regenRate = 0.0f;          // Shields per second
    float regenDelay = 10.0f;        // Seconds after damage before regen
    float timeSinceLastDamage = 0.0f;
    bool isRegenerating = false;

    /**
     * @brief Update shield regeneration
     */
    void Update(float deltaTime);

    /**
     * @brief Apply damage to shield
     * @return Remaining damage to apply to health
     */
    float TakeDamage(float damage);

    /**
     * @brief Get shield percentage (0.0 - 1.0)
     */
    [[nodiscard]] float GetShieldPercent() const {
        return maxShield > 0.0f ? currentShield / maxShield : 0.0f;
    }

    /**
     * @brief Check if shields are full
     */
    [[nodiscard]] bool IsFull() const {
        return currentShield >= maxShield;
    }

    /**
     * @brief Restore shields by amount
     */
    void RestoreShields(float amount);

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static ShieldComponent FromJson(const nlohmann::json& j);
};

// ============================================================================
// Power Grid System
// ============================================================================

/**
 * @brief Represents a power source (Pylon or Warp Prism in phased mode)
 */
struct PowerSource {
    uint32_t entityId = 0;
    glm::vec3 position{0.0f};
    float radius = AlienConstants::PYLON_POWER_RADIUS;
    bool isActive = true;
    bool allowsWarpIn = true;
    std::string sourceType;  // "pylon", "warp_prism", "nexus"

    /**
     * @brief Check if a position is within power range
     */
    [[nodiscard]] bool IsPowering(const glm::vec3& pos) const;
};

/**
 * @brief Power grid manager for the Collective
 */
class PowerGridManager {
public:
    static PowerGridManager& Instance() {
        static PowerGridManager instance;
        return instance;
    }

    /**
     * @brief Register a power source
     */
    void RegisterPowerSource(const PowerSource& source);

    /**
     * @brief Unregister a power source
     */
    void UnregisterPowerSource(uint32_t entityId);

    /**
     * @brief Update a power source position (for mobile sources)
     */
    void UpdatePowerSourcePosition(uint32_t entityId, const glm::vec3& position);

    /**
     * @brief Check if a position has power
     */
    [[nodiscard]] bool HasPower(const glm::vec3& position) const;

    /**
     * @brief Check if a position allows warp-in
     */
    [[nodiscard]] bool CanWarpAt(const glm::vec3& position) const;

    /**
     * @brief Get all power sources in range of a position
     */
    [[nodiscard]] std::vector<const PowerSource*> GetPowerSourcesAt(const glm::vec3& position) const;

    /**
     * @brief Get all valid warp-in locations
     */
    [[nodiscard]] std::vector<glm::vec3> GetWarpLocations() const;

    /**
     * @brief Clear all power sources (on game end)
     */
    void Clear();

private:
    PowerGridManager() = default;
    std::unordered_map<uint32_t, PowerSource> m_powerSources;
};

// ============================================================================
// Warp System
// ============================================================================

/**
 * @brief Unit warp-in state
 */
struct WarpInState {
    uint32_t unitId = 0;
    std::string unitType;
    glm::vec3 warpPosition{0.0f};
    float warpProgress = 0.0f;
    float warpTime = AlienConstants::WARP_IN_TIME;
    bool isVulnerable = true;
    uint32_t sourceGateId = 0;

    [[nodiscard]] float GetProgress() const {
        return warpTime > 0.0f ? warpProgress / warpTime : 1.0f;
    }

    [[nodiscard]] bool IsComplete() const {
        return warpProgress >= warpTime;
    }
};

/**
 * @brief Warp Gate state
 */
struct WarpGateState {
    uint32_t buildingId = 0;
    float cooldownRemaining = 0.0f;
    bool isReady = true;
    std::vector<std::string> availableUnits;

    void Update(float deltaTime);
    void StartCooldown(float duration);
};

// ============================================================================
// Psionic System
// ============================================================================

/**
 * @brief Psionic unit types
 */
enum class PsionicRank : uint8_t {
    None = 0,       // Non-psionic unit
    Latent,         // Basic psionic (Zealot, Stalker)
    Adept,          // Trained psionic (Psi Adept)
    Templar,        // High Templar
    Archon,         // Merged psionic entity
    Master          // Hero-level psionic
};

/**
 * @brief Psionic component for units
 */
struct PsionicComponent {
    PsionicRank rank = PsionicRank::None;
    float energy = 0.0f;
    float maxEnergy = 200.0f;
    float energyRegen = 0.5625f;
    bool isChanneling = false;
    int channelingAbilityId = -1;

    void Update(float deltaTime);
    bool ConsumeEnergy(float amount);
    void RestoreEnergy(float amount);

    [[nodiscard]] float GetEnergyPercent() const {
        return maxEnergy > 0.0f ? energy / maxEnergy : 0.0f;
    }
};

// ============================================================================
// Alien Race Class
// ============================================================================

/**
 * @brief Main class for the Alien (Collective) race
 *
 * Manages race-specific mechanics including:
 * - Shield system for all units/buildings
 * - Power grid for building functionality
 * - Warp system for rapid unit deployment
 * - Psionic abilities and energy management
 */
class AlienRace {
public:
    // Callbacks
    using ShieldDepletedCallback = std::function<void(uint32_t entityId)>;
    using PowerLostCallback = std::function<void(uint32_t buildingId)>;
    using WarpCompleteCallback = std::function<void(uint32_t unitId, const glm::vec3& position)>;

    /**
     * @brief Get singleton instance
     */
    static AlienRace& Instance();

    // Delete copy/move
    AlienRace(const AlienRace&) = delete;
    AlienRace& operator=(const AlienRace&) = delete;
    AlienRace(AlienRace&&) = delete;
    AlienRace& operator=(AlienRace&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the Alien race
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
    // Shield Management
    // =========================================================================

    /**
     * @brief Register a shield component for an entity
     */
    void RegisterShield(uint32_t entityId, const ShieldComponent& shield);

    /**
     * @brief Unregister shield when entity is destroyed
     */
    void UnregisterShield(uint32_t entityId);

    /**
     * @brief Get shield component for an entity
     */
    [[nodiscard]] ShieldComponent* GetShield(uint32_t entityId);
    [[nodiscard]] const ShieldComponent* GetShield(uint32_t entityId) const;

    /**
     * @brief Apply damage to entity (shields first)
     * @return Damage applied to health (after shield absorption)
     */
    float ApplyDamage(uint32_t entityId, float damage);

    /**
     * @brief Restore shields on entity
     */
    void RestoreShields(uint32_t entityId, float amount);

    // =========================================================================
    // Power Grid
    // =========================================================================

    /**
     * @brief Check if building has power
     */
    [[nodiscard]] bool BuildingHasPower(uint32_t buildingId) const;

    /**
     * @brief Check if position can build (has power for non-power buildings)
     */
    [[nodiscard]] bool CanBuildAt(const glm::vec3& position, const std::string& buildingType) const;

    /**
     * @brief Get power grid manager
     */
    [[nodiscard]] PowerGridManager& GetPowerGrid() { return PowerGridManager::Instance(); }

    // =========================================================================
    // Warp System
    // =========================================================================

    /**
     * @brief Start warping in a unit
     * @return true if warp started successfully
     */
    bool StartWarpIn(uint32_t gateId, const std::string& unitType, const glm::vec3& position);

    /**
     * @brief Cancel a warp-in in progress
     */
    bool CancelWarpIn(uint32_t unitId);

    /**
     * @brief Get warp gate state
     */
    [[nodiscard]] WarpGateState* GetWarpGateState(uint32_t gateId);

    /**
     * @brief Register a warp gate
     */
    void RegisterWarpGate(uint32_t buildingId, const std::vector<std::string>& units);

    /**
     * @brief Unregister a warp gate
     */
    void UnregisterWarpGate(uint32_t buildingId);

    // =========================================================================
    // Psionic System
    // =========================================================================

    /**
     * @brief Register a psionic component for a unit
     */
    void RegisterPsionic(uint32_t unitId, const PsionicComponent& psionic);

    /**
     * @brief Unregister psionic component
     */
    void UnregisterPsionic(uint32_t unitId);

    /**
     * @brief Get psionic component
     */
    [[nodiscard]] PsionicComponent* GetPsionic(uint32_t unitId);

    /**
     * @brief Calculate psionic damage multiplier based on rank
     */
    [[nodiscard]] float GetPsionicDamageMultiplier(PsionicRank rank) const;

    // =========================================================================
    // Unit Creation
    // =========================================================================

    /**
     * @brief Create a unit with Collective-specific components
     */
    uint32_t CreateUnit(const std::string& unitType, const glm::vec3& position, uint32_t ownerId);

    /**
     * @brief Create a building with power requirements
     */
    uint32_t CreateBuilding(const std::string& buildingType, const glm::vec3& position, uint32_t ownerId);

    // =========================================================================
    // Special Abilities
    // =========================================================================

    /**
     * @brief Execute Archon merge between two High Templars
     * @return ID of created Archon or 0 if failed
     */
    uint32_t MergeArchon(uint32_t templar1Id, uint32_t templar2Id);

    /**
     * @brief Execute Mass Recall ability
     */
    bool ExecuteMassRecall(uint32_t sourceId, const glm::vec3& targetPosition, float radius);

    /**
     * @brief Execute Chrono Boost on a structure
     */
    bool ExecuteChronoBoost(uint32_t nexusId, uint32_t targetBuildingId);

    // =========================================================================
    // Resource Modifiers
    // =========================================================================

    /**
     * @brief Get gathering rate modifier for drones
     */
    [[nodiscard]] float GetGatherRateModifier(const std::string& resourceType) const;

    /**
     * @brief Get cost modifier for units/buildings
     */
    [[nodiscard]] float GetCostModifier(const std::string& entityType) const;

    /**
     * @brief Get production time modifier
     */
    [[nodiscard]] float GetProductionTimeModifier() const;

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

    void SetOnShieldDepleted(ShieldDepletedCallback callback) {
        m_onShieldDepleted = std::move(callback);
    }

    void SetOnPowerLost(PowerLostCallback callback) {
        m_onPowerLost = std::move(callback);
    }

    void SetOnWarpComplete(WarpCompleteCallback callback) {
        m_onWarpComplete = std::move(callback);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total shields currently active
     */
    [[nodiscard]] float GetTotalActiveShields() const;

    /**
     * @brief Get number of powered buildings
     */
    [[nodiscard]] int GetPoweredBuildingCount() const;

    /**
     * @brief Get number of active warp gates
     */
    [[nodiscard]] int GetActiveWarpGateCount() const;

private:
    AlienRace() = default;
    ~AlienRace() = default;

    void UpdateShields(float deltaTime);
    void UpdateWarpIns(float deltaTime);
    void UpdatePsionics(float deltaTime);
    void UpdatePowerStatus();

    void LoadConfiguration(const std::string& configPath);
    void InitializeDefaultConfigs();

    bool m_initialized = false;
    std::string m_configBasePath;

    // Shield system
    std::unordered_map<uint32_t, ShieldComponent> m_shields;

    // Warp system
    std::unordered_map<uint32_t, WarpGateState> m_warpGates;
    std::unordered_map<uint32_t, WarpInState> m_activeWarpIns;

    // Psionic system
    std::unordered_map<uint32_t, PsionicComponent> m_psionics;

    // Building power status
    std::unordered_map<uint32_t, bool> m_buildingPowerStatus;
    std::unordered_map<uint32_t, glm::vec3> m_buildingPositions;

    // Callbacks
    ShieldDepletedCallback m_onShieldDepleted;
    PowerLostCallback m_onPowerLost;
    WarpCompleteCallback m_onWarpComplete;

    // Configuration cache
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
};

// ============================================================================
// Alien-specific Ability Behaviors
// ============================================================================

/**
 * @brief Blink ability implementation
 */
class BlinkAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Psionic Storm ability implementation
 */
class PsionicStormAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;

private:
    struct StormInstance {
        glm::vec3 position;
        float remainingDuration;
        float tickTimer;
        std::vector<uint32_t> affectedEntities;
    };
    std::vector<StormInstance> m_activeStorms;
};

/**
 * @brief Feedback ability implementation
 */
class FeedbackAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Chrono Boost ability implementation
 */
class ChronoBoostAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Mass Recall ability implementation
 */
class MassRecallAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Graviton Beam ability implementation
 */
class GravitonBeamAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;
};

} // namespace RTS
} // namespace Vehement
