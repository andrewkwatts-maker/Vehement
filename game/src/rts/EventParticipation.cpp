#include "EventParticipation.hpp"
#include "EventScheduler.hpp"
#include "../network/FirebaseManager.hpp"
#include <chrono>
#include <algorithm>
#include <numeric>

// Logging macros
#if __has_include("../../../engine/core/Logger.hpp")
#include "../../../engine/core/Logger.hpp"
#define PART_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define PART_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define PART_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define PART_LOG_INFO(...) std::cout << "[EventParticipation] " << __VA_ARGS__ << std::endl
#define PART_LOG_WARN(...) std::cerr << "[EventParticipation WARN] " << __VA_ARGS__ << std::endl
#define PART_LOG_ERROR(...) std::cerr << "[EventParticipation ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ===========================================================================
// EventReward
// ===========================================================================

nlohmann::json EventReward::ToJson() const {
    nlohmann::json j;

    nlohmann::json resourcesJson;
    for (const auto& [type, amount] : resources) {
        resourcesJson[ResourceTypeToString(type)] = amount;
    }
    j["resources"] = resourcesJson;
    j["experience"] = experience;
    j["items"] = items;
    j["achievements"] = achievements;
    j["score"] = score;
    j["multiplier"] = multiplier;

    return j;
}

EventReward EventReward::FromJson(const nlohmann::json& j) {
    EventReward reward;

    if (j.contains("resources") && j["resources"].is_object()) {
        for (const auto& [key, value] : j["resources"].items()) {
            for (int i = 0; i < static_cast<int>(ResourceType::Count); ++i) {
                auto type = static_cast<ResourceType>(i);
                if (key == ResourceTypeToString(type)) {
                    reward.resources[type] = value.get<int32_t>();
                    break;
                }
            }
        }
    }

    reward.experience = j.value("experience", 0);
    reward.score = j.value("score", 0);
    reward.multiplier = j.value("multiplier", 1.0f);

    if (j.contains("items")) {
        reward.items = j["items"].get<std::vector<std::string>>();
    }
    if (j.contains("achievements")) {
        reward.achievements = j["achievements"].get<std::vector<std::string>>();
    }

    return reward;
}

// ===========================================================================
// PlayerContribution
// ===========================================================================

nlohmann::json PlayerContribution::ToJson() const {
    nlohmann::json j;
    j["playerId"] = playerId;
    j["eventId"] = eventId;

    nlohmann::json contribJson;
    for (const auto& [type, value] : contributions) {
        contribJson[std::to_string(static_cast<int>(type))] = value;
    }
    j["contributions"] = contribJson;

    j["joinedAt"] = joinedAt;
    j["leftAt"] = leftAt;
    j["activeTime"] = activeTime;
    j["status"] = static_cast<int>(status);
    j["rewardClaimed"] = rewardClaimed;
    j["contributionPercentage"] = contributionPercentage;
    j["rank"] = rank;

    return j;
}

PlayerContribution PlayerContribution::FromJson(const nlohmann::json& j) {
    PlayerContribution contrib;

    contrib.playerId = j.value("playerId", "");
    contrib.eventId = j.value("eventId", "");

    if (j.contains("contributions") && j["contributions"].is_object()) {
        for (const auto& [key, value] : j["contributions"].items()) {
            auto type = static_cast<ContributionType>(std::stoi(key));
            contrib.contributions[type] = value.get<float>();
        }
    }

    contrib.joinedAt = j.value("joinedAt", int64_t(0));
    contrib.leftAt = j.value("leftAt", int64_t(0));
    contrib.activeTime = j.value("activeTime", 0.0f);
    contrib.status = static_cast<ParticipationStatus>(j.value("status", 0));
    contrib.rewardClaimed = j.value("rewardClaimed", false);
    contrib.contributionPercentage = j.value("contributionPercentage", 0.0f);
    contrib.rank = j.value("rank", 0);

    return contrib;
}

// ===========================================================================
// LeaderboardEntry
// ===========================================================================

nlohmann::json LeaderboardEntry::ToJson() const {
    return {
        {"playerId", playerId},
        {"playerName", playerName},
        {"eventId", eventId},
        {"score", score},
        {"rank", rank},
        {"contributionPercentage", contributionPercentage},
        {"reward", reward.ToJson()},
        {"timestamp", timestamp}
    };
}

LeaderboardEntry LeaderboardEntry::FromJson(const nlohmann::json& j) {
    LeaderboardEntry entry;
    entry.playerId = j.value("playerId", "");
    entry.playerName = j.value("playerName", "");
    entry.eventId = j.value("eventId", "");
    entry.score = j.value("score", 0);
    entry.rank = j.value("rank", 0);
    entry.contributionPercentage = j.value("contributionPercentage", 0.0f);
    if (j.contains("reward")) {
        entry.reward = EventReward::FromJson(j["reward"]);
    }
    entry.timestamp = j.value("timestamp", int64_t(0));
    return entry;
}

// ===========================================================================
// EventParticipationRecord
// ===========================================================================

nlohmann::json EventParticipationRecord::ToJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = EventTypeToString(eventType);
    j["eventName"] = eventName;
    j["totalParticipants"] = totalParticipants;
    j["wasSuccessful"] = wasSuccessful;
    j["completedAt"] = completedAt;
    j["wasCooperative"] = wasCooperative;
    j["cooperationBonus"] = cooperationBonus;

    nlohmann::json participantsJson;
    for (const auto& [playerId, contrib] : participants) {
        participantsJson[playerId] = contrib.ToJson();
    }
    j["participants"] = participantsJson;

    nlohmann::json leaderboardJson = nlohmann::json::array();
    for (const auto& entry : leaderboard) {
        leaderboardJson.push_back(entry.ToJson());
    }
    j["leaderboard"] = leaderboardJson;

    j["totalRewardsDistributed"] = totalRewardsDistributed.ToJson();

    return j;
}

EventParticipationRecord EventParticipationRecord::FromJson(const nlohmann::json& j) {
    EventParticipationRecord record;
    record.eventId = j.value("eventId", "");
    if (auto typeOpt = StringToEventType(j.value("eventType", ""))) {
        record.eventType = *typeOpt;
    }
    record.eventName = j.value("eventName", "");
    record.totalParticipants = j.value("totalParticipants", 0);
    record.wasSuccessful = j.value("wasSuccessful", false);
    record.completedAt = j.value("completedAt", int64_t(0));
    record.wasCooperative = j.value("wasCooperative", false);
    record.cooperationBonus = j.value("cooperationBonus", 0.0f);

    if (j.contains("participants") && j["participants"].is_object()) {
        for (const auto& [playerId, contribJson] : j["participants"].items()) {
            record.participants[playerId] = PlayerContribution::FromJson(contribJson);
        }
    }

    if (j.contains("leaderboard") && j["leaderboard"].is_array()) {
        for (const auto& entryJson : j["leaderboard"]) {
            record.leaderboard.push_back(LeaderboardEntry::FromJson(entryJson));
        }
    }

    if (j.contains("totalRewardsDistributed")) {
        record.totalRewardsDistributed = EventReward::FromJson(j["totalRewardsDistributed"]);
    }

    return record;
}

// ===========================================================================
// EventParticipationManager
// ===========================================================================

EventParticipationManager::EventParticipationManager() {
    // Initialize default reward config
    m_rewardConfig.contributionWeights[ContributionType::DamageDealt] = 1.0f;
    m_rewardConfig.contributionWeights[ContributionType::KillCount] = 10.0f;
    m_rewardConfig.contributionWeights[ContributionType::ResourcesCollected] = 2.0f;
    m_rewardConfig.contributionWeights[ContributionType::ObjectivesCompleted] = 50.0f;
    m_rewardConfig.contributionWeights[ContributionType::TimeSpent] = 0.1f;
    m_rewardConfig.contributionWeights[ContributionType::BuildingsDefended] = 25.0f;
    m_rewardConfig.contributionWeights[ContributionType::PlayersAssisted] = 15.0f;
}

EventParticipationManager::~EventParticipationManager() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventParticipationManager::Initialize() {
    if (m_initialized) {
        PART_LOG_WARN("EventParticipationManager already initialized");
        return true;
    }

    // Start Firebase listener
    if (FirebaseManager::Instance().IsInitialized()) {
        m_firebaseListenerId = FirebaseManager::Instance().ListenToPath(
            m_firebasePath,
            [this](const nlohmann::json& data) {
                OnFirebaseUpdate(data);
            }
        );
    }

    m_initialized = true;
    PART_LOG_INFO("EventParticipationManager initialized");
    return true;
}

void EventParticipationManager::Shutdown() {
    if (!m_initialized) return;

    PART_LOG_INFO("Shutting down EventParticipationManager");

    // Stop Firebase listener
    if (!m_firebaseListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_firebaseListenerId);
        m_firebaseListenerId.clear();
    }

    // Clear data
    {
        std::lock_guard<std::mutex> lock(m_participationMutex);
        m_activeRecords.clear();
        m_playerActiveEvents.clear();
        m_completedRecords.clear();
        m_unclaimedRewards.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_joinCallbacks.clear();
        m_leaveCallbacks.clear();
        m_rewardCallbacks.clear();
        m_leaderboardCallbacks.clear();
    }

    m_initialized = false;
}

void EventParticipationManager::SetEventScheduler(EventScheduler* scheduler) {
    m_scheduler = scheduler;

    if (scheduler) {
        // Register for event callbacks
        scheduler->OnEventStarted([this](const WorldEvent& event) {
            StartTrackingEvent(event);
        });

        scheduler->OnEventEnded([this](const WorldEvent& event) {
            DistributeRewards(event, true);
            StopTrackingEvent(event.id);
        });

        scheduler->OnEventCancelled([this](const WorldEvent& event) {
            StopTrackingEvent(event.id);
        });
    }
}

void EventParticipationManager::LoadRewardConfig(const RewardConfig& config) {
    m_rewardConfig = config;
}

void EventParticipationManager::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateActiveTime(deltaTime);
}

bool EventParticipationManager::JoinEvent(const std::string& eventId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    // Check if event exists
    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) {
        PART_LOG_WARN("Cannot join event " + eventId + ": event not found");
        return false;
    }

    // Check if already participating
    auto& participants = recordIt->second.participants;
    if (participants.find(playerId) != participants.end()) {
        auto& existing = participants[playerId];
        if (existing.status == ParticipationStatus::Active) {
            PART_LOG_WARN("Player " + playerId + " already participating in event " + eventId);
            return false;
        }
    }

    // Create or update contribution record
    PlayerContribution contrib;
    contrib.playerId = playerId;
    contrib.eventId = eventId;
    contrib.joinedAt = GetCurrentTimeMs();
    contrib.status = ParticipationStatus::Active;

    participants[playerId] = contrib;
    recordIt->second.totalParticipants++;

    // Track player's active events
    m_playerActiveEvents[playerId].insert(eventId);

    PART_LOG_INFO("Player " + playerId + " joined event " + eventId);

    // Publish to Firebase
    PublishContribution(eventId, contrib);

    // Invoke callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& callback : m_joinCallbacks) {
            if (callback) callback(eventId, playerId);
        }
    }

    return true;
}

void EventParticipationManager::LeaveEvent(const std::string& eventId,
                                            const std::string& playerId,
                                            bool abandoned) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return;

    auto& participants = recordIt->second.participants;
    auto contribIt = participants.find(playerId);
    if (contribIt == participants.end()) return;

    contribIt->second.leftAt = GetCurrentTimeMs();
    contribIt->second.status = abandoned ? ParticipationStatus::Abandoned
                                         : ParticipationStatus::Completed;

    // Remove from active events
    auto playerEventsIt = m_playerActiveEvents.find(playerId);
    if (playerEventsIt != m_playerActiveEvents.end()) {
        playerEventsIt->second.erase(eventId);
    }

    PART_LOG_INFO("Player " + playerId + " left event " + eventId +
                  (abandoned ? " (abandoned)" : ""));

    // Publish to Firebase
    PublishContribution(eventId, contribIt->second);

    // Invoke callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& callback : m_leaveCallbacks) {
            if (callback) callback(eventId, playerId, abandoned);
        }
    }
}

bool EventParticipationManager::IsParticipating(const std::string& eventId,
                                                 const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return false;

    auto contribIt = recordIt->second.participants.find(playerId);
    if (contribIt == recordIt->second.participants.end()) return false;

    return contribIt->second.status == ParticipationStatus::Active;
}

ParticipationStatus EventParticipationManager::GetParticipationStatus(
    const std::string& eventId, const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return ParticipationStatus::None;

    auto contribIt = recordIt->second.participants.find(playerId);
    if (contribIt == recordIt->second.participants.end()) return ParticipationStatus::None;

    return contribIt->second.status;
}

std::vector<std::string> EventParticipationManager::GetActiveEventIds(
    const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto it = m_playerActiveEvents.find(playerId);
    if (it == m_playerActiveEvents.end()) return {};

    return std::vector<std::string>(it->second.begin(), it->second.end());
}

void EventParticipationManager::RecordContribution(const std::string& eventId,
                                                    const std::string& playerId,
                                                    ContributionType type,
                                                    float amount) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return;

    auto contribIt = recordIt->second.participants.find(playerId);
    if (contribIt == recordIt->second.participants.end()) return;
    if (contribIt->second.status != ParticipationStatus::Active) return;

    contribIt->second.contributions[type] += amount;

    // Update leaderboard periodically (not every contribution)
    static int updateCounter = 0;
    if (++updateCounter % 10 == 0) {
        UpdateLeaderboard(eventId);
    }
}

void EventParticipationManager::RecordDamage(const std::string& eventId,
                                              const std::string& playerId,
                                              float damage) {
    RecordContribution(eventId, playerId, ContributionType::DamageDealt, damage);
}

void EventParticipationManager::RecordKill(const std::string& eventId,
                                            const std::string& playerId) {
    RecordContribution(eventId, playerId, ContributionType::KillCount, 1.0f);
}

void EventParticipationManager::RecordObjective(const std::string& eventId,
                                                 const std::string& playerId) {
    RecordContribution(eventId, playerId, ContributionType::ObjectivesCompleted, 1.0f);
}

void EventParticipationManager::RecordResources(const std::string& eventId,
                                                 const std::string& playerId,
                                                 int32_t amount) {
    RecordContribution(eventId, playerId, ContributionType::ResourcesCollected,
                       static_cast<float>(amount));
}

PlayerContribution EventParticipationManager::GetContribution(
    const std::string& eventId, const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return {};

    auto contribIt = recordIt->second.participants.find(playerId);
    if (contribIt == recordIt->second.participants.end()) return {};

    return contribIt->second;
}

std::vector<PlayerContribution> EventParticipationManager::GetEventContributions(
    const std::string& eventId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return {};

    std::vector<PlayerContribution> contributions;
    for (const auto& [playerId, contrib] : recordIt->second.participants) {
        contributions.push_back(contrib);
    }

    // Sort by contribution
    std::sort(contributions.begin(), contributions.end(),
              [](const PlayerContribution& a, const PlayerContribution& b) {
                  return a.GetTotalContribution() > b.GetTotalContribution();
              });

    return contributions;
}

EventReward EventParticipationManager::CalculateReward(const std::string& eventId,
                                                        const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return {};

    auto contribIt = recordIt->second.participants.find(playerId);
    if (contribIt == recordIt->second.participants.end()) return {};

    const auto& record = recordIt->second;
    const auto& contribution = contribIt->second;

    // Start with base reward for event type
    EventReward reward;
    auto baseIt = m_rewardConfig.baseRewards.find(record.eventType);
    if (baseIt != m_rewardConfig.baseRewards.end()) {
        reward = baseIt->second;
    } else {
        // Default base rewards
        reward.experience = 100;
        reward.score = 100;
        reward.resources[ResourceType::Food] = 50;
    }

    // Calculate contribution percentage
    float contribPct = CalculateContributionPercentage(eventId, contribution);

    // Scale reward by contribution
    reward.ApplyMultiplier(0.5f + contribPct * 0.5f); // 50% base + up to 50% from contribution

    // Apply rank bonuses
    int32_t rank = contribution.rank;
    if (rank == 1) {
        reward.ApplyMultiplier(m_rewardConfig.firstPlaceBonus);
    } else if (rank == 2) {
        reward.ApplyMultiplier(m_rewardConfig.secondPlaceBonus);
    } else if (rank == 3) {
        reward.ApplyMultiplier(m_rewardConfig.thirdPlaceBonus);
    }

    // Apply participation bonus
    reward.ApplyMultiplier(1.0f + m_rewardConfig.participationBonus);

    // Apply cooperation bonus
    if (record.wasCooperative) {
        reward.ApplyMultiplier(m_rewardConfig.cooperationMultiplier);
    }

    // Apply completion bonus if event was successful
    if (record.wasSuccessful) {
        reward.ApplyMultiplier(1.0f + m_rewardConfig.completionBonus);
    }

    // Apply penalties
    if (contribution.status == ParticipationStatus::Abandoned) {
        reward.ApplyMultiplier(m_rewardConfig.abandonPenalty);
    } else if (contribution.status == ParticipationStatus::Failed) {
        reward.ApplyMultiplier(m_rewardConfig.failurePenalty);
    }

    return reward;
}

std::optional<EventReward> EventParticipationManager::ClaimReward(const std::string& eventId,
                                                                   const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    // Check unclaimed rewards
    auto playerIt = m_unclaimedRewards.find(playerId);
    if (playerIt == m_unclaimedRewards.end()) return std::nullopt;

    auto rewardIt = playerIt->second.find(eventId);
    if (rewardIt == playerIt->second.end()) return std::nullopt;

    EventReward reward = rewardIt->second;
    playerIt->second.erase(rewardIt);

    PART_LOG_INFO("Player " + playerId + " claimed reward for event " + eventId);

    return reward;
}

bool EventParticipationManager::HasUnclaimedRewards(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto it = m_unclaimedRewards.find(playerId);
    return it != m_unclaimedRewards.end() && !it->second.empty();
}

std::vector<std::pair<std::string, EventReward>> EventParticipationManager::GetUnclaimedRewards(
    const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    std::vector<std::pair<std::string, EventReward>> rewards;

    auto it = m_unclaimedRewards.find(playerId);
    if (it != m_unclaimedRewards.end()) {
        for (const auto& [eventId, reward] : it->second) {
            rewards.emplace_back(eventId, reward);
        }
    }

    return rewards;
}

void EventParticipationManager::DistributeRewards(const WorldEvent& event, bool wasSuccessful) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(event.id);
    if (recordIt == m_activeRecords.end()) return;

    recordIt->second.wasSuccessful = wasSuccessful;
    recordIt->second.completedAt = GetCurrentTimeMs();

    // Update final leaderboard
    UpdateLeaderboard(event.id);

    // Calculate and distribute rewards
    for (auto& [playerId, contribution] : recordIt->second.participants) {
        if (contribution.status == ParticipationStatus::Active) {
            contribution.status = wasSuccessful ? ParticipationStatus::Completed
                                                : ParticipationStatus::Failed;
        }

        // Calculate reward
        EventReward reward = CalculateReward(event.id, playerId);

        // Store as unclaimed
        m_unclaimedRewards[playerId][event.id] = reward;

        // Update total distributed
        recordIt->second.totalRewardsDistributed += reward;

        // Update player stats
        auto& stats = m_playerStats[playerId];
        stats.eventsParticipated++;
        if (wasSuccessful) stats.eventsCompleted++;
        else stats.eventsFailed++;
        if (contribution.status == ParticipationStatus::Abandoned) stats.eventsAbandoned++;
        if (contribution.rank == 1) stats.firstPlaceFinishes++;
        if (contribution.rank <= 3) stats.topThreeFinishes++;
        stats.totalExperienceEarned += reward.experience;
        stats.totalScoreEarned += reward.score;
        stats.participationByType[event.type]++;

        PART_LOG_INFO("Distributed reward to player " + playerId +
                      " for event " + event.id);

        // Invoke callbacks
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            for (const auto& callback : m_rewardCallbacks) {
                if (callback) callback(event.id, playerId, reward);
            }
        }
    }

    // Move to completed records
    m_completedRecords.push_back(recordIt->second);
    if (m_completedRecords.size() > MaxCompletedRecords) {
        m_completedRecords.erase(m_completedRecords.begin());
    }
}

std::vector<LeaderboardEntry> EventParticipationManager::GetEventLeaderboard(
    const std::string& eventId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt != m_activeRecords.end()) {
        return recordIt->second.leaderboard;
    }

    // Check completed records
    for (const auto& record : m_completedRecords) {
        if (record.eventId == eventId) {
            return record.leaderboard;
        }
    }

    return {};
}

std::vector<LeaderboardEntry> EventParticipationManager::GetGlobalLeaderboard(
    EventType type, int32_t limit) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    std::vector<LeaderboardEntry> globalLeaderboard;

    // Aggregate from completed records
    for (const auto& record : m_completedRecords) {
        if (record.eventType == type) {
            globalLeaderboard.insert(globalLeaderboard.end(),
                                     record.leaderboard.begin(),
                                     record.leaderboard.end());
        }
    }

    // Sort by score
    std::sort(globalLeaderboard.begin(), globalLeaderboard.end());

    // Limit results
    if (static_cast<int32_t>(globalLeaderboard.size()) > limit) {
        globalLeaderboard.resize(limit);
    }

    // Update ranks
    for (int32_t i = 0; i < static_cast<int32_t>(globalLeaderboard.size()); ++i) {
        globalLeaderboard[i].rank = i + 1;
    }

    return globalLeaderboard;
}

int32_t EventParticipationManager::GetPlayerRank(const std::string& eventId,
                                                  const std::string& playerId) const {
    auto leaderboard = GetEventLeaderboard(eventId);

    for (const auto& entry : leaderboard) {
        if (entry.playerId == playerId) {
            return entry.rank;
        }
    }

    return 0;
}

std::vector<LeaderboardEntry> EventParticipationManager::GetPlayerBestPerformances(
    const std::string& playerId, int32_t limit) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    std::vector<LeaderboardEntry> performances;

    for (const auto& record : m_completedRecords) {
        for (const auto& entry : record.leaderboard) {
            if (entry.playerId == playerId) {
                performances.push_back(entry);
                break;
            }
        }
    }

    // Sort by score
    std::sort(performances.begin(), performances.end());

    // Limit results
    if (static_cast<int32_t>(performances.size()) > limit) {
        performances.resize(limit);
    }

    return performances;
}

bool EventParticipationManager::IsCooperativeEvent(const std::string& eventId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return false;

    return recordIt->second.wasCooperative;
}

float EventParticipationManager::GetCooperationScore(const std::string& eventId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return 0.0f;

    // Calculate based on player assists
    float totalAssists = 0.0f;
    for (const auto& [playerId, contrib] : recordIt->second.participants) {
        totalAssists += contrib.GetContribution(ContributionType::PlayersAssisted);
    }

    return totalAssists;
}

void EventParticipationManager::RecordCooperation(const std::string& eventId,
                                                   const std::string& helperPlayerId,
                                                   const std::string& helpedPlayerId) {
    RecordContribution(eventId, helperPlayerId, ContributionType::PlayersAssisted, 1.0f);

    // Mark event as cooperative if multiple players are helping each other
    std::lock_guard<std::mutex> lock(m_participationMutex);
    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt != m_activeRecords.end()) {
        if (GetCooperationScore(eventId) >= 5.0f) {
            recordIt->second.wasCooperative = true;
        }
    }
}

EventParticipationManager::PlayerEventStats EventParticipationManager::GetPlayerStats(
    const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    auto it = m_playerStats.find(playerId);
    if (it != m_playerStats.end()) {
        return it->second;
    }

    return {};
}

void EventParticipationManager::SyncWithFirebase() {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    // Request current participation data
    FirebaseManager::Instance().GetValue(m_firebasePath, [this](const nlohmann::json& data) {
        OnFirebaseUpdate(data);
    });
}

void EventParticipationManager::OnFirebaseUpdate(const nlohmann::json& data) {
    if (data.is_null()) return;

    // Parse participation records from Firebase
    // This would sync contributions from other players
}

void EventParticipationManager::OnPlayerJoined(JoinCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_joinCallbacks.push_back(std::move(callback));
}

void EventParticipationManager::OnPlayerLeft(LeaveCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_leaveCallbacks.push_back(std::move(callback));
}

void EventParticipationManager::OnRewardDistributed(RewardCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_rewardCallbacks.push_back(std::move(callback));
}

void EventParticipationManager::OnLeaderboardUpdated(LeaderboardCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_leaderboardCallbacks.push_back(std::move(callback));
}

// ===========================================================================
// Private Helpers
// ===========================================================================

float EventParticipationManager::CalculateScore(const PlayerContribution& contribution) const {
    float score = 0.0f;

    for (const auto& [type, value] : contribution.contributions) {
        auto weightIt = m_rewardConfig.contributionWeights.find(type);
        float weight = weightIt != m_rewardConfig.contributionWeights.end() ? weightIt->second : 1.0f;
        score += value * weight;
    }

    return score;
}

float EventParticipationManager::CalculateContributionPercentage(
    const std::string& eventId, const PlayerContribution& contribution) const {
    // Note: Must be called with mutex held

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return 0.0f;

    float totalContribution = 0.0f;
    for (const auto& [playerId, contrib] : recordIt->second.participants) {
        totalContribution += CalculateScore(contrib);
    }

    if (totalContribution <= 0.0f) return 0.0f;

    return CalculateScore(contribution) / totalContribution;
}

void EventParticipationManager::UpdateLeaderboard(const std::string& eventId) {
    // Note: Must be called with mutex held

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return;

    auto& record = recordIt->second;
    record.leaderboard.clear();

    // Build leaderboard entries
    for (auto& [playerId, contrib] : record.participants) {
        LeaderboardEntry entry;
        entry.playerId = playerId;
        entry.eventId = eventId;
        entry.score = static_cast<int32_t>(CalculateScore(contrib));
        entry.contributionPercentage = CalculateContributionPercentage(eventId, contrib);
        entry.timestamp = GetCurrentTimeMs();

        record.leaderboard.push_back(entry);
    }

    // Sort by score
    std::sort(record.leaderboard.begin(), record.leaderboard.end());

    // Assign ranks
    for (int32_t i = 0; i < static_cast<int32_t>(record.leaderboard.size()); ++i) {
        record.leaderboard[i].rank = i + 1;

        // Update contribution record with rank
        auto contribIt = record.participants.find(record.leaderboard[i].playerId);
        if (contribIt != record.participants.end()) {
            contribIt->second.rank = i + 1;
            contribIt->second.contributionPercentage = record.leaderboard[i].contributionPercentage;
        }
    }

    // Invoke callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& callback : m_leaderboardCallbacks) {
            if (callback) callback(eventId, record.leaderboard);
        }
    }

    // Publish to Firebase
    PublishLeaderboard(eventId);
}

void EventParticipationManager::StartTrackingEvent(const WorldEvent& event) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    EventParticipationRecord record;
    record.eventId = event.id;
    record.eventType = event.type;
    record.eventName = event.name;
    record.wasCooperative = false;
    record.cooperationBonus = 0.0f;

    m_activeRecords[event.id] = record;

    PART_LOG_INFO("Started tracking participation for event: " + event.name);
}

void EventParticipationManager::StopTrackingEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    // Remove from active events for all players
    for (auto& [playerId, activeEvents] : m_playerActiveEvents) {
        activeEvents.erase(eventId);
    }

    // Record is moved to completed in DistributeRewards
    m_activeRecords.erase(eventId);

    PART_LOG_INFO("Stopped tracking participation for event: " + eventId);
}

void EventParticipationManager::UpdateActiveTime(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_participationMutex);

    for (auto& [eventId, record] : m_activeRecords) {
        for (auto& [playerId, contrib] : record.participants) {
            if (contrib.status == ParticipationStatus::Active) {
                contrib.activeTime += deltaTime;
                contrib.contributions[ContributionType::TimeSpent] = contrib.activeTime;
            }
        }
    }
}

void EventParticipationManager::PublishContribution(const std::string& eventId,
                                                     const PlayerContribution& contribution) {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    std::string path = m_firebasePath + "/" + eventId + "/" + contribution.playerId;
    FirebaseManager::Instance().SetValue(path, contribution.ToJson());
}

void EventParticipationManager::PublishLeaderboard(const std::string& eventId) {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    auto recordIt = m_activeRecords.find(eventId);
    if (recordIt == m_activeRecords.end()) return;

    nlohmann::json leaderboardJson = nlohmann::json::array();
    for (const auto& entry : recordIt->second.leaderboard) {
        leaderboardJson.push_back(entry.ToJson());
    }

    std::string path = m_firebasePath + "/" + eventId + "/leaderboard";
    FirebaseManager::Instance().SetValue(path, leaderboardJson);
}

int64_t EventParticipationManager::GetCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace Vehement
