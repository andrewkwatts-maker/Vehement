#pragma once

#include "Entity.hpp"
#include <vector>
#include <unordered_set>
#include <functional>

namespace Vehement {

// Forward declarations
class EntityManager;
class NPC;
class Zombie;

/**
 * @brief Statistics for infection tracking
 */
struct InfectionStats {
    uint32_t totalInfected = 0;         // Total NPCs ever infected
    uint32_t totalConverted = 0;        // NPCs that turned into zombies
    uint32_t totalCured = 0;            // NPCs that were cured
    uint32_t currentlyInfected = 0;     // Currently infected NPCs
    float averageConversionTime = 0.0f; // Average time to convert

    // Per-session tracking
    uint32_t sessionInfected = 0;
    uint32_t sessionConverted = 0;
    uint32_t sessionCured = 0;

    /** @brief Get conversion rate (infected that became zombies) */
    [[nodiscard]] float GetConversionRate() const {
        return totalInfected > 0 ?
            static_cast<float>(totalConverted) / static_cast<float>(totalInfected) : 0.0f;
    }

    /** @brief Get cure rate (infected that were cured) */
    [[nodiscard]] float GetCureRate() const {
        return totalInfected > 0 ?
            static_cast<float>(totalCured) / static_cast<float>(totalInfected) : 0.0f;
    }

    /** @brief Reset session statistics */
    void ResetSession() {
        sessionInfected = 0;
        sessionConverted = 0;
        sessionCured = 0;
    }
};

/**
 * @brief Configuration for infection spread
 */
struct InfectionConfig {
    float baseInfectionChance = 0.3f;     // Base chance per zombie attack
    float infectionDuration = 30.0f;       // Time from infection to turning (seconds)
    float infectionDurationVariance = 5.0f; // Random variance in duration
    bool allowCure = true;                  // Whether NPCs can be cured
    float cureWindow = 0.1f;                // Last % of duration where cure fails
    float proximityInfectionRadius = 0.0f;  // Radius for proximity infection (0 = disabled)
    float proximityInfectionChance = 0.0f;  // Chance per second in radius

    /** @brief Get randomized infection duration */
    [[nodiscard]] float GetRandomDuration() const;
};

/**
 * @brief Infection System for Vehement2
 *
 * Manages the zombie infection mechanics:
 * - Tracks infected NPCs
 * - Updates infection timers
 * - Converts NPCs to zombies when timer expires
 * - Handles infection spread configuration
 */
class InfectionSystem {
public:
    using ConversionCallback = std::function<void(NPC& npc, glm::vec3 position)>;
    using InfectionCallback = std::function<void(NPC& npc, Entity::EntityId source)>;
    using CureCallback = std::function<void(NPC& npc)>;

    InfectionSystem();
    explicit InfectionSystem(const InfectionConfig& config);
    ~InfectionSystem() = default;

    // =========================================================================
    // Core Update
    // =========================================================================

    /**
     * @brief Update all infection timers and handle conversions
     * @param deltaTime Time since last frame
     * @param entityManager Entity manager for spawning zombies
     */
    void Update(float deltaTime, EntityManager& entityManager);

    // =========================================================================
    // Infection Management
    // =========================================================================

    /**
     * @brief Infect an NPC
     * @param npc The NPC to infect
     * @param source Entity that caused the infection (for stats)
     * @return true if NPC was infected (not already infected)
     */
    bool InfectNPC(NPC& npc, Entity::EntityId source = Entity::INVALID_ID);

    /**
     * @brief Try to cure an infected NPC
     * @param npc The NPC to cure
     * @return true if NPC was cured
     */
    bool CureNPC(NPC& npc);

    /**
     * @brief Roll for infection based on config
     * @param bonusChance Additional chance modifier
     * @return true if infection should occur
     */
    [[nodiscard]] bool RollInfection(float bonusChance = 0.0f) const;

    /**
     * @brief Check if an NPC is tracked as infected
     * @param npcId Entity ID of the NPC
     */
    [[nodiscard]] bool IsTracked(Entity::EntityId npcId) const;

    /**
     * @brief Get all infected NPC IDs
     */
    [[nodiscard]] const std::unordered_set<Entity::EntityId>& GetInfectedNPCs() const noexcept {
        return m_infectedNPCs;
    }

    /**
     * @brief Get count of currently infected NPCs
     */
    [[nodiscard]] size_t GetInfectedCount() const noexcept { return m_infectedNPCs.size(); }

    // =========================================================================
    // Configuration
    // =========================================================================

    /** @brief Get infection configuration */
    [[nodiscard]] const InfectionConfig& GetConfig() const noexcept { return m_config; }

    /** @brief Set infection configuration */
    void SetConfig(const InfectionConfig& config) { m_config = config; }

    /** @brief Set base infection chance */
    void SetBaseInfectionChance(float chance) { m_config.baseInfectionChance = std::clamp(chance, 0.0f, 1.0f); }

    /** @brief Set infection duration */
    void SetInfectionDuration(float duration) { m_config.infectionDuration = std::max(1.0f, duration); }

    /** @brief Enable/disable proximity infection */
    void SetProximityInfection(float radius, float chance) {
        m_config.proximityInfectionRadius = std::max(0.0f, radius);
        m_config.proximityInfectionChance = std::clamp(chance, 0.0f, 1.0f);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /** @brief Get infection statistics */
    [[nodiscard]] const InfectionStats& GetStats() const noexcept { return m_stats; }

    /** @brief Reset all statistics */
    void ResetStats() { m_stats = InfectionStats(); }

    /** @brief Reset session statistics only */
    void ResetSessionStats() { m_stats.ResetSession(); }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when NPC converts to zombie
     * Called with NPC and position where zombie should spawn
     */
    void SetConversionCallback(ConversionCallback callback) {
        m_onConversion = std::move(callback);
    }

    /**
     * @brief Set callback for when NPC becomes infected
     */
    void SetInfectionCallback(InfectionCallback callback) {
        m_onInfection = std::move(callback);
    }

    /**
     * @brief Set callback for when NPC is cured
     */
    void SetCureCallback(CureCallback callback) {
        m_onCure = std::move(callback);
    }

    // =========================================================================
    // Zombie Spawning Integration
    // =========================================================================

    /**
     * @brief Spawn a zombie at NPC's location (when NPC turns)
     * @param entityManager Entity manager for spawning
     * @param position Position to spawn zombie
     * @param zombieType Type of zombie to spawn
     * @return Pointer to spawned zombie
     */
    Zombie* SpawnZombieFromInfection(EntityManager& entityManager,
                                     const glm::vec3& position,
                                     ZombieType zombieType = ZombieType::Standard);

private:
    /**
     * @brief Handle NPC conversion to zombie
     * @param npc The NPC that is turning
     * @param entityManager Entity manager for spawning
     */
    void HandleConversion(NPC& npc, EntityManager& entityManager);

    /**
     * @brief Process proximity infection for one frame
     * @param deltaTime Time since last frame
     * @param entityManager Entity manager for queries
     */
    void ProcessProximityInfection(float deltaTime, EntityManager& entityManager);

    /**
     * @brief Remove NPC from tracking (called on death/removal)
     */
    void StopTracking(Entity::EntityId npcId);

    // Configuration
    InfectionConfig m_config;

    // Tracking
    std::unordered_set<Entity::EntityId> m_infectedNPCs;

    // Statistics
    InfectionStats m_stats;

    // Conversion time tracking for average calculation
    std::vector<float> m_conversionTimes;

    // Callbacks
    ConversionCallback m_onConversion;
    InfectionCallback m_onInfection;
    CureCallback m_onCure;
};

} // namespace Vehement
