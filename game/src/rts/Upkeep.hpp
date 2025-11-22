#pragma once

#include "Resource.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Vehement {
namespace RTS {

// ============================================================================
// Upkeep Source
// ============================================================================

/**
 * @brief Types of entities that consume resources
 */
enum class UpkeepSourceType : uint8_t {
    Worker,             ///< Workers consume food
    Building,           ///< Buildings may consume fuel
    DefenseStructure,   ///< Defense turrets consume ammo when firing
    Vehicle,            ///< Vehicles consume fuel
    Unit                ///< Combat units consume food and ammo
};

/**
 * @brief An entity that contributes to resource upkeep
 */
struct UpkeepSource {
    /// Unique identifier
    uint32_t id = 0;

    /// Type of source
    UpkeepSourceType type = UpkeepSourceType::Worker;

    /// Resource type consumed
    ResourceType resourceType = ResourceType::Food;

    /// Consumption rate per second
    float consumptionRate = 0.1f;

    /// Whether this source is currently active
    bool active = true;

    /// Name for display
    std::string name;

    /// Position (for UI/locating)
    glm::vec2 position{0.0f};

    /// Health (affected by starvation)
    float* healthRef = nullptr;

    /**
     * @brief Get consumption per tick
     */
    [[nodiscard]] float GetConsumptionPerSecond() const {
        return active ? consumptionRate : 0.0f;
    }
};

// ============================================================================
// Upkeep Status
// ============================================================================

/**
 * @brief Status of resource availability for upkeep
 */
enum class UpkeepStatus : uint8_t {
    Healthy,        ///< Resources are plentiful
    Adequate,       ///< Resources are sufficient but not abundant
    Low,            ///< Resources are getting low
    Critical,       ///< Resources critically low
    Depleted        ///< Resources exhausted - negative effects active
};

/**
 * @brief Get status name for display
 */
[[nodiscard]] const char* GetUpkeepStatusName(UpkeepStatus status);

/**
 * @brief Get color for upkeep status (for UI)
 */
[[nodiscard]] uint32_t GetUpkeepStatusColor(UpkeepStatus status);

// ============================================================================
// Starvation Effect
// ============================================================================

/**
 * @brief Effect applied when resources are depleted
 */
struct StarvationEffect {
    ResourceType resourceType = ResourceType::Food;

    /// Whether starvation is currently active
    bool active = false;

    /// Duration in starvation state
    float duration = 0.0f;

    /// Damage per second during starvation
    float damagePerSecond = 1.0f;

    /// Speed reduction multiplier (1.0 = normal)
    float speedMultiplier = 1.0f;

    /// Production efficiency during starvation
    float productionMultiplier = 1.0f;

    /// Morale effect (affects various things)
    float moraleMultiplier = 1.0f;
};

// ============================================================================
// Upkeep Warning
// ============================================================================

/**
 * @brief A warning about upcoming resource shortage
 */
struct UpkeepWarning {
    ResourceType resourceType = ResourceType::Food;

    /// Current status
    UpkeepStatus status = UpkeepStatus::Healthy;

    /// Time until depletion at current rate (seconds)
    float timeUntilDepletion = 0.0f;

    /// Current net rate (income - expense)
    float netRate = 0.0f;

    /// Message for display
    std::string message;

    /// Whether this warning has been acknowledged
    bool acknowledged = false;
};

// ============================================================================
// Upkeep Configuration
// ============================================================================

/**
 * @brief Configuration for the upkeep system
 */
struct UpkeepConfig {
    /// Food consumption per worker per second
    float workerFoodConsumption = 0.05f;

    /// Fuel consumption per building per second (if applicable)
    float buildingFuelConsumption = 0.02f;

    /// Ammo consumption per shot for defense structures
    int defenseAmmoPerShot = 1;

    /// Time between starvation damage ticks
    float starvationDamageInterval = 5.0f;

    /// Damage dealt per starvation tick
    float starvationDamageAmount = 5.0f;

    /// Speed penalty during starvation (multiplier)
    float starvationSpeedPenalty = 0.5f;

    /// Production penalty during starvation (multiplier)
    float starvationProductionPenalty = 0.3f;

    /// Thresholds for warning levels (as percentage of capacity)
    float lowThreshold = 0.25f;
    float criticalThreshold = 0.10f;
    float adequateThreshold = 0.50f;

    /// How often to recalculate upkeep (seconds)
    float updateInterval = 1.0f;
};

// ============================================================================
// Upkeep System
// ============================================================================

/**
 * @brief Manages resource consumption and shortage effects
 *
 * This system handles:
 * - Worker food consumption
 * - Building fuel consumption
 * - Defense structure ammunition
 * - Starvation/shortage effects
 * - Warnings for low resources
 */
class UpkeepSystem {
public:
    UpkeepSystem();
    ~UpkeepSystem();

    // Delete copy, allow move
    UpkeepSystem(const UpkeepSystem&) = delete;
    UpkeepSystem& operator=(const UpkeepSystem&) = delete;
    UpkeepSystem(UpkeepSystem&&) noexcept = default;
    UpkeepSystem& operator=(UpkeepSystem&&) noexcept = default;

    /**
     * @brief Initialize the upkeep system
     */
    void Initialize(const UpkeepConfig& config = UpkeepConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update upkeep calculations
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Upkeep Source Management
    // -------------------------------------------------------------------------

    /**
     * @brief Register a worker for food upkeep
     * @param position Worker position
     * @param healthRef Pointer to worker health (for starvation damage)
     * @return Source ID
     */
    uint32_t RegisterWorker(const glm::vec2& position, float* healthRef = nullptr);

    /**
     * @brief Register a building for fuel upkeep
     */
    uint32_t RegisterBuilding(const std::string& name, const glm::vec2& position, float consumptionRate);

    /**
     * @brief Register a defense structure for ammo upkeep
     */
    uint32_t RegisterDefense(const std::string& name, const glm::vec2& position);

    /**
     * @brief Register a custom upkeep source
     */
    uint32_t RegisterSource(const UpkeepSource& source);

    /**
     * @brief Unregister an upkeep source
     */
    void UnregisterSource(uint32_t sourceId);

    /**
     * @brief Set source active state
     */
    void SetSourceActive(uint32_t sourceId, bool active);

    /**
     * @brief Get an upkeep source by ID
     */
    [[nodiscard]] const UpkeepSource* GetSource(uint32_t sourceId) const;

    /**
     * @brief Get all upkeep sources
     */
    [[nodiscard]] const std::vector<UpkeepSource>& GetSources() const { return m_sources; }

    // -------------------------------------------------------------------------
    // Upkeep Calculation
    // -------------------------------------------------------------------------

    /**
     * @brief Get total consumption rate for a resource type (per second)
     */
    [[nodiscard]] float GetTotalConsumption(ResourceType type) const;

    /**
     * @brief Get total consumption from a specific source type
     */
    [[nodiscard]] float GetConsumptionByType(UpkeepSourceType sourceType) const;

    /**
     * @brief Get number of active sources of a type
     */
    [[nodiscard]] int GetActiveSourceCount(UpkeepSourceType type) const;

    /**
     * @brief Get estimated time until resource depletion
     * @param type Resource type
     * @return Time in seconds, or -1 if not depleting
     */
    [[nodiscard]] float GetTimeUntilDepletion(ResourceType type) const;

    // -------------------------------------------------------------------------
    // Upkeep Status
    // -------------------------------------------------------------------------

    /**
     * @brief Get upkeep status for a resource type
     */
    [[nodiscard]] UpkeepStatus GetStatus(ResourceType type) const;

    /**
     * @brief Get all current warnings
     */
    [[nodiscard]] const std::vector<UpkeepWarning>& GetWarnings() const { return m_warnings; }

    /**
     * @brief Acknowledge a warning
     */
    void AcknowledgeWarning(ResourceType type);

    /**
     * @brief Check if starvation is active for a resource
     */
    [[nodiscard]] bool IsStarving(ResourceType type) const;

    /**
     * @brief Get starvation effect for a resource
     */
    [[nodiscard]] const StarvationEffect& GetStarvationEffect(ResourceType type) const;

    // -------------------------------------------------------------------------
    // Defense Ammunition
    // -------------------------------------------------------------------------

    /**
     * @brief Consume ammunition for a defense structure shot
     * @param defenseId Defense source ID
     * @return true if ammo was available
     */
    bool ConsumeDefenseAmmo(uint32_t defenseId);

    /**
     * @brief Check if defense has ammunition
     */
    [[nodiscard]] bool HasDefenseAmmo() const;

    // -------------------------------------------------------------------------
    // Resource Stock
    // -------------------------------------------------------------------------

    /**
     * @brief Set the resource stock to consume from
     */
    void SetResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Get current resource stock
     */
    [[nodiscard]] ResourceStock* GetResourceStock() const { return m_resourceStock; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Apply scarcity settings
     */
    void ApplyScarcitySettings(const ScarcitySettings& settings);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const UpkeepConfig& GetConfig() const { return m_config; }

    /**
     * @brief Modify configuration
     */
    void SetConfig(const UpkeepConfig& config) { m_config = config; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using StarvationCallback = std::function<void(ResourceType, bool started)>;
    using WarningCallback = std::function<void(const UpkeepWarning&)>;
    using SourceDiedCallback = std::function<void(const UpkeepSource&)>;

    void SetOnStarvation(StarvationCallback cb) { m_onStarvation = std::move(cb); }
    void SetOnWarning(WarningCallback cb) { m_onWarning = std::move(cb); }
    void SetOnSourceDied(SourceDiedCallback cb) { m_onSourceDied = std::move(cb); }

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    /**
     * @brief Get total resources consumed (lifetime)
     */
    [[nodiscard]] int GetTotalConsumed(ResourceType type) const;

    /**
     * @brief Get total starvation time
     */
    [[nodiscard]] float GetTotalStarvationTime(ResourceType type) const;

    /**
     * @brief Get workers lost to starvation
     */
    [[nodiscard]] int GetWorkersLostToStarvation() const { return m_workersLostToStarvation; }

private:
    void UpdateConsumption(float deltaTime);
    void UpdateStarvation(float deltaTime);
    void UpdateWarnings();

    void ApplyStarvationDamage(float deltaTime);
    void CheckSourceDeath(UpkeepSource& source);

    uint32_t GenerateSourceId();

    UpkeepConfig m_config;
    ScarcitySettings m_scarcitySettings;

    std::vector<UpkeepSource> m_sources;
    std::vector<UpkeepWarning> m_warnings;

    std::unordered_map<ResourceType, StarvationEffect> m_starvationEffects;
    std::unordered_map<ResourceType, float> m_consumptionAccumulators;
    std::unordered_map<ResourceType, int> m_totalConsumed;
    std::unordered_map<ResourceType, float> m_totalStarvationTime;

    ResourceStock* m_resourceStock = nullptr;

    uint32_t m_nextSourceId = 1;

    float m_updateTimer = 0.0f;
    float m_starvationDamageTimer = 0.0f;

    int m_workersLostToStarvation = 0;

    // Callbacks
    StarvationCallback m_onStarvation;
    WarningCallback m_onWarning;
    SourceDiedCallback m_onSourceDied;

    bool m_initialized = false;
};

// ============================================================================
// Upkeep Calculator Helper
// ============================================================================

/**
 * @brief Helper class to calculate projected upkeep
 */
class UpkeepCalculator {
public:
    /**
     * @brief Calculate daily food requirement
     * @param workerCount Number of workers
     * @param config Upkeep configuration
     * @return Food units needed per day (86400 seconds)
     */
    [[nodiscard]] static int CalculateDailyFoodNeed(int workerCount, const UpkeepConfig& config);

    /**
     * @brief Calculate fuel requirement for buildings
     * @param buildingConsumption Total building fuel consumption per second
     * @param hours Time period in hours
     * @return Fuel units needed
     */
    [[nodiscard]] static int CalculateFuelNeed(float buildingConsumption, float hours);

    /**
     * @brief Estimate ammo needed for defense
     * @param defenseCount Number of defense structures
     * @param expectedAttacksPerHour Expected attack frequency
     * @param shotsPerAttack Average shots per attack
     * @param config Upkeep configuration
     * @return Ammo units needed per hour
     */
    [[nodiscard]] static int EstimateAmmoNeed(
        int defenseCount,
        float expectedAttacksPerHour,
        int shotsPerAttack,
        const UpkeepConfig& config
    );

    /**
     * @brief Calculate if current stock can sustain workers for given duration
     * @param stock Current resource stock
     * @param workerCount Number of workers
     * @param hours Duration in hours
     * @param config Upkeep configuration
     * @return true if sustainable
     */
    [[nodiscard]] static bool CanSustainWorkers(
        const ResourceStock& stock,
        int workerCount,
        float hours,
        const UpkeepConfig& config
    );
};

} // namespace RTS
} // namespace Vehement
