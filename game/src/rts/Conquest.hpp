#pragma once

/**
 * @file Conquest.hpp
 * @brief Player conquest and base attack system
 *
 * Handles:
 * - Attacking other players' bases
 * - Conquest progress tracking
 * - Rewards from conquest (resources, techs, territory)
 * - Defense mechanics
 * - Firebase synchronization for multiplayer
 */

#include "TechTree.hpp"
#include "TechLoss.hpp"
#include "Resource.hpp"
#include "Territory.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class TechTree;
class TechLoss;

// ============================================================================
// Conquest Types
// ============================================================================

/**
 * @brief State of a conquest attempt
 */
enum class ConquestState : uint8_t {
    Preparing,          ///< Attacker is gathering forces
    InProgress,         ///< Battle is ongoing
    Successful,         ///< Attacker won
    Failed,             ///< Defender won
    Retreated,          ///< Attacker retreated
    Cancelled,          ///< Conquest was cancelled
    Timeout             ///< Conquest timed out
};

[[nodiscard]] inline const char* ConquestStateToString(ConquestState state) {
    switch (state) {
        case ConquestState::Preparing:   return "Preparing";
        case ConquestState::InProgress:  return "In Progress";
        case ConquestState::Successful:  return "Successful";
        case ConquestState::Failed:      return "Failed";
        case ConquestState::Retreated:   return "Retreated";
        case ConquestState::Cancelled:   return "Cancelled";
        case ConquestState::Timeout:     return "Timeout";
        default:                         return "Unknown";
    }
}

/**
 * @brief Type of conquest/attack
 */
enum class ConquestType : uint8_t {
    Raid,               ///< Quick attack for resources, no territory change
    Siege,              ///< Full siege, can destroy buildings
    Conquest,           ///< Take over territory permanently
    Assassination,      ///< Kill hero, no territory/building damage
    Sabotage            ///< Damage production, no direct combat
};

[[nodiscard]] inline const char* ConquestTypeToString(ConquestType type) {
    switch (type) {
        case ConquestType::Raid:            return "Raid";
        case ConquestType::Siege:           return "Siege";
        case ConquestType::Conquest:        return "Conquest";
        case ConquestType::Assassination:   return "Assassination";
        case ConquestType::Sabotage:        return "Sabotage";
        default:                            return "Unknown";
    }
}

// ============================================================================
// Conquest Reward
// ============================================================================

/**
 * @brief Rewards gained from a successful conquest
 */
struct ConquestReward {
    // Resources looted
    std::map<ResourceType, int> resources;

    // Technologies stolen
    std::vector<std::string> techs;

    // Workers captured
    int workers = 0;

    // Territory gained (percentage of defender's territory)
    float territoryGained = 0.0f;
    std::vector<glm::ivec2> capturedTiles;

    // Buildings destroyed/captured
    int buildingsDestroyed = 0;
    int buildingsCaptured = 0;
    std::vector<std::string> capturedBuildingIds;

    // Experience gained
    int experienceGained = 0;

    // Fame/reputation gained
    int fameGained = 0;

    // Special loot
    std::vector<std::string> specialItems;

    /**
     * @brief Calculate total value of rewards
     */
    [[nodiscard]] int GetTotalValue() const;

    /**
     * @brief Check if rewards are empty
     */
    [[nodiscard]] bool IsEmpty() const;

    /**
     * @brief Generate summary message
     */
    [[nodiscard]] std::string GetSummaryMessage() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ConquestReward FromJson(const nlohmann::json& j);
};

/**
 * @brief Losses suffered by the defender
 */
struct ConquestLoss {
    std::map<ResourceType, int> resourcesLost;
    std::vector<std::string> techsLost;
    int workersLost = 0;
    float territoryLost = 0.0f;
    int buildingsDestroyed = 0;
    int unitsLost = 0;
    Age previousAge = Age::Stone;
    Age newAge = Age::Stone;
    bool heroKilled = false;

    [[nodiscard]] int GetTotalLossValue() const;
    [[nodiscard]] std::string GetSummaryMessage() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ConquestLoss FromJson(const nlohmann::json& j);
};

// ============================================================================
// Combat Stats
// ============================================================================

/**
 * @brief Combat statistics for conquest
 */
struct CombatStats {
    // Attacker stats
    int attackerUnits = 0;
    int attackerUnitsLost = 0;
    float attackerDamageDealt = 0.0f;
    float attackerDamageTaken = 0.0f;

    // Defender stats
    int defenderUnits = 0;
    int defenderUnitsLost = 0;
    float defenderDamageDealt = 0.0f;
    float defenderDamageTaken = 0.0f;

    // Building damage
    float buildingDamageDealt = 0.0f;

    // Duration
    float combatDurationSeconds = 0.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static CombatStats FromJson(const nlohmann::json& j);
};

// ============================================================================
// Conquest Instance
// ============================================================================

/**
 * @brief A single conquest attempt between two players
 */
struct ConquestInstance {
    // Identification
    std::string id;                         ///< Unique conquest ID
    std::string attackerId;                 ///< Attacker player ID
    std::string defenderId;                 ///< Defender player ID

    // Type and state
    ConquestType type = ConquestType::Raid;
    ConquestState state = ConquestState::Preparing;

    // Timing
    int64_t initiatedTimestamp = 0;         ///< When conquest was started
    int64_t startedTimestamp = 0;           ///< When battle began
    int64_t completedTimestamp = 0;         ///< When conquest ended
    float preparationTimeSeconds = 300.0f;  ///< Time before battle starts
    float maxDurationSeconds = 600.0f;      ///< Max battle duration

    // Progress
    float conquestProgress = 0.0f;          ///< 0-100%, attacker wins at 100%
    float defenseStrength = 100.0f;         ///< Defense remaining (0 = defeated)

    // Forces
    std::vector<std::string> attackerUnitIds;
    std::vector<std::string> defenderUnitIds;

    // Location (if targeting specific area)
    glm::ivec2 targetPosition{0, 0};
    float attackRadius = 10.0f;

    // Results
    ConquestReward attackerReward;
    ConquestLoss defenderLoss;
    CombatStats combatStats;

    // Flags
    bool defenderOnline = false;            ///< Was defender online during attack?
    bool wasContested = false;              ///< Did defender actively fight back?

    /**
     * @brief Check if conquest is still active
     */
    [[nodiscard]] bool IsActive() const;

    /**
     * @brief Check if conquest is completed (success or failure)
     */
    [[nodiscard]] bool IsCompleted() const;

    /**
     * @brief Get time until battle starts
     */
    [[nodiscard]] float GetTimeUntilStart() const;

    /**
     * @brief Get remaining battle time
     */
    [[nodiscard]] float GetRemainingTime() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ConquestInstance FromJson(const nlohmann::json& j);
};

// ============================================================================
// Conquest Configuration
// ============================================================================

/**
 * @brief Configuration for conquest system
 */
struct ConquestConfig {
    // Timing
    float raidPreparationTime = 60.0f;          ///< Seconds before raid starts
    float siegePreparationTime = 300.0f;        ///< Seconds before siege starts
    float conquestPreparationTime = 600.0f;     ///< Seconds before conquest starts
    float maxRaidDuration = 180.0f;             ///< Max raid battle duration
    float maxSiegeDuration = 600.0f;            ///< Max siege duration
    float maxConquestDuration = 1200.0f;        ///< Max conquest duration

    // Cooldowns
    float attackCooldownHours = 1.0f;           ///< Hours between attacks on same target
    float defenseCooldownHours = 2.0f;          ///< Hours of protection after being attacked
    float globalAttackCooldownMinutes = 10.0f;  ///< Minutes between any attacks

    // Rewards
    float resourceLootPercent = 0.3f;           ///< % of defender's resources looted
    float techStealChance = 0.25f;              ///< Chance to steal each tech
    float workerCapturePercent = 0.1f;          ///< % of workers captured
    float territoryGainPercent = 0.15f;         ///< % of territory gained per conquest

    // Limits
    int maxActiveConquests = 1;                 ///< Max concurrent conquests
    int maxDailyAttacks = 5;                    ///< Max attacks per day
    int minAgeDifference = -2;                  ///< Min age difference to attack
    int maxAgeDifference = 2;                   ///< Max age difference to attack

    // Balance
    float offlineDefenseBonus = 0.5f;           ///< +50% defense when offline
    float fortificationBonus = 1.5f;            ///< Defense bonus per fortification level
    float homeTerritoryBonus = 1.3f;            ///< Defender bonus in own territory
    float surpriseAttackBonus = 1.2f;           ///< Attacker bonus for surprise attack

    // Protection
    float newPlayerProtectionHours = 24.0f;     ///< Hours of protection for new players
    float afterDefeatProtectionHours = 4.0f;    ///< Hours of protection after defeat

    [[nodiscard]] nlohmann::json ToJson() const;
    static ConquestConfig FromJson(const nlohmann::json& j);

    static ConquestConfig Default() { return {}; }
    static ConquestConfig Casual() {
        ConquestConfig c;
        c.resourceLootPercent = 0.15f;
        c.techStealChance = 0.1f;
        c.defenseCooldownHours = 6.0f;
        c.afterDefeatProtectionHours = 8.0f;
        return c;
    }
    static ConquestConfig Hardcore() {
        ConquestConfig c;
        c.resourceLootPercent = 0.5f;
        c.techStealChance = 0.4f;
        c.territoryGainPercent = 0.25f;
        c.defenseCooldownHours = 1.0f;
        c.afterDefeatProtectionHours = 1.0f;
        return c;
    }
};

// ============================================================================
// Conquest Manager
// ============================================================================

/**
 * @brief Manages all conquest-related functionality
 *
 * Features:
 * - Initiating attacks on other players
 * - Tracking conquest progress
 * - Calculating rewards and losses
 * - Firebase synchronization
 * - Notifications and callbacks
 *
 * Example usage:
 * @code
 * auto& conquest = ConquestManager::Instance();
 * conquest.Initialize("my_player_id");
 *
 * // Check if can attack
 * if (conquest.CanAttack("enemy_id")) {
 *     conquest.InitiateConquest("my_id", "enemy_id", ConquestType::Raid);
 * }
 *
 * // Update each frame
 * conquest.Update(deltaTime);
 *
 * // Handle results
 * conquest.SetOnConquestComplete([](const ConquestInstance& c) {
 *     if (c.state == ConquestState::Successful) {
 *         // Show victory!
 *     }
 * });
 * @endcode
 */
class ConquestManager {
public:
    // Callback types
    using ConquestStartCallback = std::function<void(const ConquestInstance& conquest)>;
    using ConquestUpdateCallback = std::function<void(const ConquestInstance& conquest)>;
    using ConquestCompleteCallback = std::function<void(const ConquestInstance& conquest)>;
    using UnderAttackCallback = std::function<void(const ConquestInstance& conquest)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static ConquestManager& Instance();

    // Non-copyable
    ConquestManager(const ConquestManager&) = delete;
    ConquestManager& operator=(const ConquestManager&) = delete;

    /**
     * @brief Initialize conquest system
     * @param localPlayerId Local player's ID
     * @param config Configuration settings
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const std::string& localPlayerId,
                                   const ConquestConfig& config = ConquestConfig::Default());

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update all active conquests
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Conquest Initiation
    // =========================================================================

    /**
     * @brief Check if player can attack a target
     * @param targetId Target player ID
     * @return true if attack is allowed
     */
    [[nodiscard]] bool CanAttack(const std::string& targetId) const;

    /**
     * @brief Get reason why attack is not allowed
     */
    [[nodiscard]] std::string GetAttackBlockedReason(const std::string& targetId) const;

    /**
     * @brief Initiate a conquest attempt
     * @param attackerId Attacker player ID
     * @param defenderId Defender player ID
     * @param type Type of conquest
     * @return Conquest ID or empty string on failure
     */
    std::string InitiateConquest(const std::string& attackerId,
                                  const std::string& defenderId,
                                  ConquestType type = ConquestType::Raid);

    /**
     * @brief Cancel a conquest (during preparation phase)
     * @param conquestId Conquest to cancel
     * @return true if cancelled
     */
    bool CancelConquest(const std::string& conquestId);

    /**
     * @brief Retreat from an active conquest
     * @param conquestId Conquest to retreat from
     * @return true if retreat successful
     */
    bool Retreat(const std::string& conquestId);

    // =========================================================================
    // Conquest Progress
    // =========================================================================

    /**
     * @brief Update conquest progress (battle simulation)
     * @param conquestId Conquest to update
     * @param deltaTime Time since last update
     */
    void UpdateConquest(const std::string& conquestId, float deltaTime);

    /**
     * @brief Get conquest by ID
     */
    [[nodiscard]] const ConquestInstance* GetConquest(const std::string& conquestId) const;

    /**
     * @brief Get all active conquests
     */
    [[nodiscard]] std::vector<const ConquestInstance*> GetActiveConquests() const;

    /**
     * @brief Get conquests where local player is attacking
     */
    [[nodiscard]] std::vector<const ConquestInstance*> GetMyAttacks() const;

    /**
     * @brief Get conquests where local player is defending
     */
    [[nodiscard]] std::vector<const ConquestInstance*> GetMyDefenses() const;

    // =========================================================================
    // Conquest Completion
    // =========================================================================

    /**
     * @brief Complete a conquest (apply results)
     * @param conquestId Conquest to complete
     * @return Final conquest result
     */
    ConquestInstance CompleteConquest(const std::string& conquestId);

    /**
     * @brief Calculate rewards for a successful conquest
     */
    [[nodiscard]] ConquestReward CalculateReward(const ConquestInstance& conquest,
                                                  const TechTree& defenderTech) const;

    /**
     * @brief Calculate losses for a defeated defender
     */
    [[nodiscard]] ConquestLoss CalculateLoss(const ConquestInstance& conquest,
                                              const TechTree& defenderTech) const;

    /**
     * @brief Apply conquest results to tech trees
     */
    void ApplyConquestResults(TechTree& attackerTech, TechTree& defenderTech,
                               const ConquestInstance& conquest,
                               TechLoss& techLoss);

    // =========================================================================
    // Defense
    // =========================================================================

    /**
     * @brief Check if player has protection
     */
    [[nodiscard]] bool HasProtection(const std::string& playerId) const;

    /**
     * @brief Get remaining protection time
     */
    [[nodiscard]] float GetProtectionTimeRemaining(const std::string& playerId) const;

    /**
     * @brief Grant protection to a player
     */
    void GrantProtection(const std::string& playerId, float durationHours);

    /**
     * @brief Calculate defense strength for a player
     */
    [[nodiscard]] float CalculateDefenseStrength(const std::string& playerId) const;

    /**
     * @brief Defend against an attack (active defense)
     * @param conquestId Conquest to defend against
     * @param defenseUnits Units committed to defense
     */
    void CommitDefense(const std::string& conquestId,
                        const std::vector<std::string>& defenseUnits);

    // =========================================================================
    // History & Statistics
    // =========================================================================

    /**
     * @brief Get recent conquest history for a player
     */
    [[nodiscard]] std::vector<ConquestInstance> GetConquestHistory(
        const std::string& playerId, int limit = 10) const;

    /**
     * @brief Get attack statistics for a player
     */
    struct AttackStats {
        int totalAttacks = 0;
        int successfulAttacks = 0;
        int failedAttacks = 0;
        int totalResourcesLooted = 0;
        int totalTechsStolen = 0;
        float totalTerritoryGained = 0.0f;
    };
    [[nodiscard]] AttackStats GetAttackStats(const std::string& playerId) const;

    /**
     * @brief Get defense statistics for a player
     */
    struct DefenseStats {
        int totalDefenses = 0;
        int successfulDefenses = 0;
        int failedDefenses = 0;
        int totalResourcesLost = 0;
        int totalTechsLost = 0;
        float totalTerritoryLost = 0.0f;
    };
    [[nodiscard]] DefenseStats GetDefenseStats(const std::string& playerId) const;

    // =========================================================================
    // Cooldowns
    // =========================================================================

    /**
     * @brief Check if player can attack (not on cooldown)
     */
    [[nodiscard]] bool IsOffAttackCooldown() const;

    /**
     * @brief Get time until can attack again
     */
    [[nodiscard]] float GetAttackCooldownRemaining() const;

    /**
     * @brief Get remaining daily attacks
     */
    [[nodiscard]] int GetRemainingDailyAttacks() const;

    /**
     * @brief Check if can attack specific target (not on cooldown)
     */
    [[nodiscard]] bool CanAttackTarget(const std::string& targetId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnConquestStart(ConquestStartCallback callback) {
        m_onConquestStart = std::move(callback);
    }

    void SetOnConquestUpdate(ConquestUpdateCallback callback) {
        m_onConquestUpdate = std::move(callback);
    }

    void SetOnConquestComplete(ConquestCompleteCallback callback) {
        m_onConquestComplete = std::move(callback);
    }

    void SetOnUnderAttack(UnderAttackCallback callback) {
        m_onUnderAttack = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ConquestConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const ConquestConfig& config) { m_config = config; }

    // =========================================================================
    // Firebase Sync
    // =========================================================================

    /**
     * @brief Save conquest state to Firebase
     */
    void SaveToFirebase();

    /**
     * @brief Load conquest state from Firebase
     */
    void LoadFromFirebase(std::function<void(bool success)> callback);

    /**
     * @brief Enable real-time sync for conquests
     */
    void EnableFirebaseSync();

    /**
     * @brief Disable Firebase sync
     */
    void DisableFirebaseSync();

private:
    ConquestManager() = default;
    ~ConquestManager() = default;

    // Helper functions
    std::string GenerateConquestId();
    float CalculatePrepTime(ConquestType type) const;
    float CalculateMaxDuration(ConquestType type) const;
    void SimulateBattle(ConquestInstance& conquest, float deltaTime);
    void FinalizeConquest(ConquestInstance& conquest);
    int64_t GetCurrentTimestamp() const;
    std::string GetFirebasePath() const;

    bool m_initialized = false;
    std::string m_localPlayerId;
    ConquestConfig m_config;

    // Active conquests
    std::unordered_map<std::string, ConquestInstance> m_activeConquests;
    mutable std::mutex m_conquestMutex;

    // Protection tracking
    std::unordered_map<std::string, int64_t> m_protectionExpiry;

    // Cooldowns
    int64_t m_lastAttackTimestamp = 0;
    std::unordered_map<std::string, int64_t> m_targetCooldowns;
    int m_dailyAttacksUsed = 0;
    int m_dailyAttacksDay = 0;  // Day of year for daily reset

    // History
    std::vector<ConquestInstance> m_conquestHistory;

    // Statistics
    std::unordered_map<std::string, AttackStats> m_attackStats;
    std::unordered_map<std::string, DefenseStats> m_defenseStats;

    // Firebase
    std::string m_firebaseListenerId;
    bool m_firebaseSyncEnabled = false;

    // ID generation
    uint64_t m_nextConquestId = 1;

    // Callbacks
    ConquestStartCallback m_onConquestStart;
    ConquestUpdateCallback m_onConquestUpdate;
    ConquestCompleteCallback m_onConquestComplete;
    UnderAttackCallback m_onUnderAttack;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate a notification message for conquest event
 */
[[nodiscard]] std::string GenerateConquestNotification(
    const ConquestInstance& conquest,
    bool forAttacker);

/**
 * @brief Calculate recommended units for an attack
 */
[[nodiscard]] int CalculateRecommendedAttackForce(
    float targetDefenseStrength,
    ConquestType type);

/**
 * @brief Check if conquest type is aggressive (affects reputation)
 */
[[nodiscard]] inline bool IsAggressiveConquest(ConquestType type) {
    return type == ConquestType::Siege || type == ConquestType::Conquest;
}

} // namespace RTS
} // namespace Vehement
