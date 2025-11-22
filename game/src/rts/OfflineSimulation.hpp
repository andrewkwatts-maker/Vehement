#pragma once

#include "PersistentWorld.hpp"
#include <vector>
#include <map>
#include <string>
#include <random>
#include <functional>

namespace Vehement {

/**
 * @brief Report of what happened during offline time
 */
struct OfflineReport {
    float hoursOffline = 0.0f;

    // Resource changes
    std::map<ResourceType, int> resourcesGained;
    std::map<ResourceType, int> resourcesLost;

    // Population changes
    int workersGained = 0;
    int workersLost = 0;
    int workersInjured = 0;

    // Combat
    int attacksReceived = 0;
    int attacksSurvived = 0;
    int zombiesKilled = 0;
    int buildingsDestroyed = 0;
    int buildingsDamaged = 0;

    // Events
    std::vector<std::string> events;
    std::vector<WorldEvent> worldEvents;

    // Summary stats
    float totalDamageReceived = 0.0f;
    float totalResourcesProduced = 0.0f;
    float totalResourcesConsumed = 0.0f;

    /**
     * @brief Get a formatted summary string
     */
    [[nodiscard]] std::string GetSummary() const;

    /**
     * @brief Add an event to the report
     */
    void AddEvent(const std::string& description);

    /**
     * @brief Check if anything significant happened
     */
    [[nodiscard]] bool HasSignificantEvents() const;
};

/**
 * @brief Configuration for offline simulation
 */
struct OfflineSimulationConfig {
    // Time limits
    float maxSimulatedHours = 72.0f;        // Max hours to simulate at once
    float simulationTimeStep = 1.0f;        // Hours per simulation step

    // Production modifiers
    float offlineProductionMultiplier = 0.75f;  // Production is slower offline
    float offlineConsumptionMultiplier = 0.5f;  // Consumption is lower offline

    // Threat settings
    float baseAttackChancePerHour = 0.05f;  // 5% chance of attack per hour
    float maxAttacksPerDay = 3.0f;          // Maximum attacks in 24 hours
    float nightAttackMultiplier = 2.0f;     // More attacks at night

    // Defense calculations
    float defenseEffectiveness = 0.8f;      // How effective defenses are offline
    float workerDefenseBonus = 0.1f;        // Bonus per defending worker

    // Morale and efficiency
    float moraleDrainPerHour = 0.5f;        // Morale drops while offline
    float minMorale = 20.0f;                // Minimum morale level

    // Resource regeneration
    bool enableResourceRegeneration = true;
    float resourceRegenerationRate = 0.1f;  // Per hour
};

/**
 * @brief Zombie attack data during offline simulation
 */
struct OfflineAttack {
    int hour;                               // Hour of attack (0 = first hour offline)
    int zombieCount;                        // Number of zombies
    int zombieStrength;                     // Average zombie strength
    bool isNightAttack;                     // Night attacks are stronger
    bool wasRepelled;                       // Did defenses hold?
    int damageDealt;                        // Total damage to buildings
    int zombiesKilled;                      // Zombies eliminated by defenses
    std::vector<int> damagedBuildings;      // Building IDs that were damaged
    std::vector<int> destroyedBuildings;    // Building IDs that were destroyed
    std::vector<int> killedWorkers;         // Worker IDs that died
};

/**
 * @brief Simulates what happens to the world while player is offline
 *
 * Key features:
 * - Resource production continues at reduced rate
 * - Resource consumption continues (workers eat)
 * - Random zombie attacks can occur
 * - Defenses automatically respond
 * - Workers can be injured or killed
 * - Buildings can be damaged or destroyed
 * - Morale decreases over time
 * - Resource nodes may regenerate
 *
 * The simulation is deterministic based on the offline duration
 * and the world state when the player left.
 */
class OfflineSimulation {
public:
    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static OfflineSimulation& Instance();

    // Non-copyable
    OfflineSimulation(const OfflineSimulation&) = delete;
    OfflineSimulation& operator=(const OfflineSimulation&) = delete;

    /**
     * @brief Set simulation configuration
     * @param config Configuration settings
     */
    void SetConfig(const OfflineSimulationConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const OfflineSimulationConfig& GetConfig() const { return m_config; }

    /**
     * @brief Run full offline simulation
     * @param state World state to modify (in place)
     * @param hoursOffline Hours the player was offline
     * @return Report of what happened
     */
    OfflineReport Simulate(WorldState& state, float hoursOffline);

    /**
     * @brief Simulate resource production
     * @param state World state to modify
     * @param hours Hours to simulate
     */
    void SimulateProduction(WorldState& state, float hours);

    /**
     * @brief Simulate resource consumption
     * @param state World state to modify
     * @param hours Hours to simulate
     */
    void SimulateConsumption(WorldState& state, float hours);

    /**
     * @brief Simulate zombie threats/attacks
     * @param state World state to modify
     * @param hours Hours to simulate
     * @return Attacks that occurred
     */
    std::vector<OfflineAttack> SimulateThreats(WorldState& state, float hours);

    /**
     * @brief Simulate worker activity and morale
     * @param state World state to modify
     * @param hours Hours to simulate
     */
    void SimulateWorkers(WorldState& state, float hours);

    /**
     * @brief Simulate construction progress
     * @param state World state to modify
     * @param hours Hours to simulate
     */
    void SimulateConstruction(WorldState& state, float hours);

    /**
     * @brief Simulate resource node regeneration
     * @param state World state to modify
     * @param hours Hours to simulate
     */
    void SimulateResourceRegeneration(WorldState& state, float hours);

    /**
     * @brief Apply world events that occurred during offline time
     * @param state World state to modify
     * @param events Events to apply
     */
    void ApplyWorldEvents(WorldState& state, const std::vector<WorldEvent>& events);

    /**
     * @brief Calculate total defense strength
     * @param state World state to evaluate
     * @return Defense power value
     */
    [[nodiscard]] float CalculateDefenseStrength(const WorldState& state) const;

    /**
     * @brief Calculate attack strength for a wave
     * @param zombieCount Number of zombies
     * @param zombieStrength Per-zombie strength
     * @param isNight Is it a night attack
     * @return Attack power value
     */
    [[nodiscard]] float CalculateAttackStrength(int zombieCount, int zombieStrength, bool isNight) const;

    /**
     * @brief Set callback for attack events
     */
    using AttackCallback = std::function<void(const OfflineAttack& attack)>;
    void SetAttackCallback(AttackCallback callback) { m_attackCallback = callback; }

    /**
     * @brief Set callback for production events
     */
    using ProductionCallback = std::function<void(ResourceType type, int amount)>;
    void SetProductionCallback(ProductionCallback callback) { m_productionCallback = callback; }

private:
    OfflineSimulation();
    ~OfflineSimulation() = default;

    /**
     * @brief Generate attack for a specific hour
     * @param state World state
     * @param hour Hour number in simulation
     * @param isNight Is it night time
     * @return Attack data (zombieCount = 0 if no attack)
     */
    OfflineAttack GenerateAttack(const WorldState& state, int hour, bool isNight);

    /**
     * @brief Resolve an attack against defenses
     * @param state World state to modify
     * @param attack Attack to resolve
     */
    void ResolveAttack(WorldState& state, OfflineAttack& attack);

    /**
     * @brief Check if an hour is during night time
     * @param hour Hour (0-23)
     * @return true if night time
     */
    [[nodiscard]] bool IsNightHour(int hour) const;

    /**
     * @brief Select random building to damage
     * @param state World state
     * @return Building ID or -1 if none available
     */
    int SelectBuildingToDamage(const WorldState& state);

    /**
     * @brief Select random worker that might be killed
     * @param state World state
     * @return Worker ID or -1 if none available
     */
    int SelectWorkerAtRisk(const WorldState& state);

    OfflineSimulationConfig m_config;
    std::mt19937 m_rng;

    // Callbacks
    AttackCallback m_attackCallback;
    ProductionCallback m_productionCallback;

    // Running totals for current simulation
    OfflineReport m_currentReport;
};

/**
 * @brief Helper to format resource amounts for display
 */
class ResourceFormatter {
public:
    /**
     * @brief Format resource amount with type name
     */
    [[nodiscard]] static std::string Format(ResourceType type, int amount);

    /**
     * @brief Format multiple resources as a list
     */
    [[nodiscard]] static std::string FormatList(const std::map<ResourceType, int>& resources);

    /**
     * @brief Format time duration in human-readable form
     */
    [[nodiscard]] static std::string FormatDuration(float hours);
};

} // namespace Vehement
