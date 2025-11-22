#pragma once

#include "WorldEvent.hpp"
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <optional>

namespace Vehement {

// Forward declarations
class FirebaseManager;
class EventScheduler;

/**
 * @brief Participation status for a player in an event
 */
enum class ParticipationStatus : uint8_t {
    None,           ///< Not participating
    Eligible,       ///< In range but not yet participating
    Active,         ///< Currently participating
    Completed,      ///< Finished participating (success)
    Failed,         ///< Finished participating (failure)
    Abandoned       ///< Left before completion
};

/**
 * @brief Contribution types for tracking participation
 */
enum class ContributionType : uint8_t {
    DamageDealt,
    KillCount,
    ResourcesCollected,
    ObjectivesCompleted,
    TimeSpent,
    UnitsLost,
    BuildingsDefended,
    PlayersAssisted
};

/**
 * @brief Reward structure for event participation
 */
struct EventReward {
    std::map<ResourceType, int32_t> resources;
    int32_t experience = 0;
    std::vector<std::string> items;
    std::vector<std::string> achievements;
    int32_t score = 0;
    float multiplier = 1.0f;            ///< Bonus multiplier applied

    /**
     * @brief Add resources to reward
     */
    void AddResources(ResourceType type, int32_t amount) {
        resources[type] += amount;
    }

    /**
     * @brief Add item to reward
     */
    void AddItem(const std::string& itemId) {
        items.push_back(itemId);
    }

    /**
     * @brief Scale all rewards by multiplier
     */
    void ApplyMultiplier(float mult) {
        multiplier *= mult;
        for (auto& [type, amount] : resources) {
            amount = static_cast<int32_t>(amount * mult);
        }
        experience = static_cast<int32_t>(experience * mult);
        score = static_cast<int32_t>(score * mult);
    }

    /**
     * @brief Combine two rewards
     */
    EventReward& operator+=(const EventReward& other) {
        for (const auto& [type, amount] : other.resources) {
            resources[type] += amount;
        }
        experience += other.experience;
        score += other.score;
        items.insert(items.end(), other.items.begin(), other.items.end());
        achievements.insert(achievements.end(), other.achievements.begin(), other.achievements.end());
        return *this;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventReward FromJson(const nlohmann::json& json);
};

/**
 * @brief Player's contribution record for an event
 */
struct PlayerContribution {
    std::string playerId;
    std::string eventId;

    // Contribution metrics
    std::map<ContributionType, float> contributions;

    // Timing
    int64_t joinedAt = 0;
    int64_t leftAt = 0;
    float activeTime = 0.0f;

    // State
    ParticipationStatus status = ParticipationStatus::None;
    bool rewardClaimed = false;

    // Calculated values
    float contributionPercentage = 0.0f;
    int32_t rank = 0;

    [[nodiscard]] float GetContribution(ContributionType type) const {
        auto it = contributions.find(type);
        return it != contributions.end() ? it->second : 0.0f;
    }

    [[nodiscard]] float GetTotalContribution() const {
        float total = 0.0f;
        for (const auto& [type, value] : contributions) {
            total += value;
        }
        return total;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerContribution FromJson(const nlohmann::json& json);
};

/**
 * @brief Leaderboard entry for event performance
 */
struct LeaderboardEntry {
    std::string playerId;
    std::string playerName;
    std::string eventId;
    int32_t score = 0;
    int32_t rank = 0;
    float contributionPercentage = 0.0f;
    EventReward reward;
    int64_t timestamp = 0;

    bool operator<(const LeaderboardEntry& other) const {
        return score > other.score; // Higher score = better rank
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static LeaderboardEntry FromJson(const nlohmann::json& json);
};

/**
 * @brief Complete event participation record
 */
struct EventParticipationRecord {
    std::string eventId;
    EventType eventType;
    std::string eventName;

    // Participants
    std::map<std::string, PlayerContribution> participants;
    int32_t totalParticipants = 0;

    // Event outcome
    bool wasSuccessful = false;
    int64_t completedAt = 0;

    // Leaderboard
    std::vector<LeaderboardEntry> leaderboard;

    // Total rewards distributed
    EventReward totalRewardsDistributed;

    // Cooperative metrics
    bool wasCooperative = false;
    float cooperationBonus = 0.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventParticipationRecord FromJson(const nlohmann::json& json);
};

/**
 * @brief Configuration for reward calculation
 */
struct RewardConfig {
    // Base rewards per event type
    std::map<EventType, EventReward> baseRewards;

    // Contribution weights for score calculation
    std::map<ContributionType, float> contributionWeights;

    // Bonus multipliers
    float firstPlaceBonus = 1.5f;
    float secondPlaceBonus = 1.25f;
    float thirdPlaceBonus = 1.1f;
    float participationBonus = 0.1f;    ///< Minimum bonus for participating
    float cooperationMultiplier = 1.2f; ///< Bonus for cooperative events
    float completionBonus = 0.25f;      ///< Bonus for event success

    // Scaling
    float difficultyScaling = 0.1f;     ///< Per difficulty tier
    float playerCountScaling = 0.05f;   ///< Per additional player

    // Penalties
    float abandonPenalty = 0.5f;        ///< Multiplier for abandoning event
    float failurePenalty = 0.7f;        ///< Multiplier for event failure
};

/**
 * @brief Manages player participation in world events
 *
 * Responsibilities:
 * - Track player participation in events
 * - Calculate contributions and scores
 * - Distribute rewards
 * - Maintain leaderboards
 * - Handle cooperative events
 */
class EventParticipationManager {
public:
    EventParticipationManager();
    ~EventParticipationManager();

    // Non-copyable
    EventParticipationManager(const EventParticipationManager&) = delete;
    EventParticipationManager& operator=(const EventParticipationManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the participation manager
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown the manager
     */
    void Shutdown();

    /**
     * @brief Set event scheduler reference
     */
    void SetEventScheduler(EventScheduler* scheduler);

    /**
     * @brief Load reward configuration
     */
    void LoadRewardConfig(const RewardConfig& config);

    /**
     * @brief Set local player ID
     */
    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update participation tracking
     */
    void Update(float deltaTime);

    // =========================================================================
    // Participation Management
    // =========================================================================

    /**
     * @brief Join an event
     * @param eventId Event to join
     * @param playerId Player joining
     * @return true if successfully joined
     */
    [[nodiscard]] bool JoinEvent(const std::string& eventId, const std::string& playerId);

    /**
     * @brief Leave an event
     * @param eventId Event to leave
     * @param playerId Player leaving
     * @param abandoned If true, player abandoned (penalized)
     */
    void LeaveEvent(const std::string& eventId, const std::string& playerId, bool abandoned = false);

    /**
     * @brief Check if player is participating in event
     */
    [[nodiscard]] bool IsParticipating(const std::string& eventId,
                                        const std::string& playerId) const;

    /**
     * @brief Get participation status
     */
    [[nodiscard]] ParticipationStatus GetParticipationStatus(const std::string& eventId,
                                                              const std::string& playerId) const;

    /**
     * @brief Get events player is participating in
     */
    [[nodiscard]] std::vector<std::string> GetActiveEventIds(const std::string& playerId) const;

    // =========================================================================
    // Contribution Tracking
    // =========================================================================

    /**
     * @brief Record a contribution
     */
    void RecordContribution(const std::string& eventId,
                            const std::string& playerId,
                            ContributionType type,
                            float amount);

    /**
     * @brief Record damage dealt to event enemies
     */
    void RecordDamage(const std::string& eventId, const std::string& playerId, float damage);

    /**
     * @brief Record kill of event enemy
     */
    void RecordKill(const std::string& eventId, const std::string& playerId);

    /**
     * @brief Record objective completion
     */
    void RecordObjective(const std::string& eventId, const std::string& playerId);

    /**
     * @brief Record resources collected
     */
    void RecordResources(const std::string& eventId, const std::string& playerId, int32_t amount);

    /**
     * @brief Get player's contribution to event
     */
    [[nodiscard]] PlayerContribution GetContribution(const std::string& eventId,
                                                      const std::string& playerId) const;

    /**
     * @brief Get all contributions for an event
     */
    [[nodiscard]] std::vector<PlayerContribution> GetEventContributions(
        const std::string& eventId) const;

    // =========================================================================
    // Rewards
    // =========================================================================

    /**
     * @brief Calculate rewards for a player's participation
     */
    [[nodiscard]] EventReward CalculateReward(const std::string& eventId,
                                               const std::string& playerId) const;

    /**
     * @brief Claim rewards for completed event
     */
    [[nodiscard]] std::optional<EventReward> ClaimReward(const std::string& eventId,
                                                          const std::string& playerId);

    /**
     * @brief Check if player has unclaimed rewards
     */
    [[nodiscard]] bool HasUnclaimedRewards(const std::string& playerId) const;

    /**
     * @brief Get all unclaimed rewards for player
     */
    [[nodiscard]] std::vector<std::pair<std::string, EventReward>> GetUnclaimedRewards(
        const std::string& playerId) const;

    /**
     * @brief Distribute rewards to all participants when event ends
     */
    void DistributeRewards(const WorldEvent& event, bool wasSuccessful);

    // =========================================================================
    // Leaderboards
    // =========================================================================

    /**
     * @brief Get leaderboard for specific event
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetEventLeaderboard(
        const std::string& eventId) const;

    /**
     * @brief Get global leaderboard for event type
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetGlobalLeaderboard(
        EventType type, int32_t limit = 100) const;

    /**
     * @brief Get player's rank in event
     */
    [[nodiscard]] int32_t GetPlayerRank(const std::string& eventId,
                                         const std::string& playerId) const;

    /**
     * @brief Get player's best performances
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetPlayerBestPerformances(
        const std::string& playerId, int32_t limit = 10) const;

    // =========================================================================
    // Cooperative Events
    // =========================================================================

    /**
     * @brief Check if event is cooperative
     */
    [[nodiscard]] bool IsCooperativeEvent(const std::string& eventId) const;

    /**
     * @brief Get cooperation score for event
     */
    [[nodiscard]] float GetCooperationScore(const std::string& eventId) const;

    /**
     * @brief Record cooperative action
     */
    void RecordCooperation(const std::string& eventId,
                           const std::string& helperPlayerId,
                           const std::string& helpedPlayerId);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get player's total event statistics
     */
    struct PlayerEventStats {
        int32_t eventsParticipated = 0;
        int32_t eventsCompleted = 0;
        int32_t eventsFailed = 0;
        int32_t eventsAbandoned = 0;
        int32_t firstPlaceFinishes = 0;
        int32_t topThreeFinishes = 0;
        int64_t totalExperienceEarned = 0;
        int64_t totalScoreEarned = 0;
        std::map<EventType, int32_t> participationByType;
    };

    [[nodiscard]] PlayerEventStats GetPlayerStats(const std::string& playerId) const;

    // =========================================================================
    // Firebase Synchronization
    // =========================================================================

    /**
     * @brief Sync participation data with Firebase
     */
    void SyncWithFirebase();

    /**
     * @brief Handle Firebase update
     */
    void OnFirebaseUpdate(const nlohmann::json& data);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Callback when player joins event
     */
    using JoinCallback = std::function<void(const std::string& eventId,
                                            const std::string& playerId)>;
    void OnPlayerJoined(JoinCallback callback);

    /**
     * @brief Callback when player leaves event
     */
    using LeaveCallback = std::function<void(const std::string& eventId,
                                             const std::string& playerId,
                                             bool abandoned)>;
    void OnPlayerLeft(LeaveCallback callback);

    /**
     * @brief Callback when rewards are distributed
     */
    using RewardCallback = std::function<void(const std::string& eventId,
                                              const std::string& playerId,
                                              const EventReward& reward)>;
    void OnRewardDistributed(RewardCallback callback);

    /**
     * @brief Callback when leaderboard updates
     */
    using LeaderboardCallback = std::function<void(const std::string& eventId,
                                                   const std::vector<LeaderboardEntry>& leaderboard)>;
    void OnLeaderboardUpdated(LeaderboardCallback callback);

private:
    // Calculation helpers
    float CalculateScore(const PlayerContribution& contribution) const;
    float CalculateContributionPercentage(const std::string& eventId,
                                          const PlayerContribution& contribution) const;
    void UpdateLeaderboard(const std::string& eventId);
    void ApplyBonuses(EventReward& reward, const PlayerContribution& contribution,
                      const WorldEvent* event) const;

    // State management
    void StartTrackingEvent(const WorldEvent& event);
    void StopTrackingEvent(const std::string& eventId);
    void UpdateActiveTime(float deltaTime);

    // Firebase helpers
    void PublishContribution(const std::string& eventId, const PlayerContribution& contribution);
    void PublishLeaderboard(const std::string& eventId);

    // Time utilities
    [[nodiscard]] int64_t GetCurrentTimeMs() const;

    // State
    bool m_initialized = false;
    std::string m_localPlayerId;
    EventScheduler* m_scheduler = nullptr;
    RewardConfig m_rewardConfig;

    // Active participation tracking
    std::map<std::string, EventParticipationRecord> m_activeRecords;
    std::map<std::string, std::set<std::string>> m_playerActiveEvents; // playerId -> eventIds
    mutable std::mutex m_participationMutex;

    // Historical records
    std::vector<EventParticipationRecord> m_completedRecords;
    static constexpr size_t MaxCompletedRecords = 100;

    // Unclaimed rewards
    std::map<std::string, std::map<std::string, EventReward>> m_unclaimedRewards; // playerId -> eventId -> reward

    // Player statistics
    std::map<std::string, PlayerEventStats> m_playerStats;

    // Callbacks
    std::vector<JoinCallback> m_joinCallbacks;
    std::vector<LeaveCallback> m_leaveCallbacks;
    std::vector<RewardCallback> m_rewardCallbacks;
    std::vector<LeaderboardCallback> m_leaderboardCallbacks;
    std::mutex m_callbackMutex;

    // Firebase
    std::string m_firebasePath = "participation";
    std::string m_firebaseListenerId;
};

} // namespace Vehement
