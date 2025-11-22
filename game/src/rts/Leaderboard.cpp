#include "Leaderboard.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define LB_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define LB_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#else
#include <iostream>
#define LB_LOG_INFO(...) std::cout << "[Leaderboard] " << __VA_ARGS__ << std::endl
#define LB_LOG_WARN(...) std::cerr << "[Leaderboard WARN] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// Helper Functions
// ============================================================================

const char* LeaderboardCategoryToString(LeaderboardCategory category) {
    switch (category) {
        case LeaderboardCategory::TerritoryControlled: return "territory";
        case LeaderboardCategory::ZombiesKilled: return "zombies_killed";
        case LeaderboardCategory::Population: return "population";
        case LeaderboardCategory::Wealth: return "wealth";
        case LeaderboardCategory::SurvivalTime: return "survival_time";
        case LeaderboardCategory::BuildingsConstructed: return "buildings";
        case LeaderboardCategory::AttacksSurvived: return "attacks_survived";
        case LeaderboardCategory::DailyZombieKills: return "daily_kills";
        case LeaderboardCategory::WeeklyTerritory: return "weekly_territory";
        default: return "unknown";
    }
}

// ============================================================================
// LeaderboardEntry Implementation
// ============================================================================

nlohmann::json LeaderboardEntry::ToJson() const {
    return {
        {"rank", rank},
        {"playerId", playerId},
        {"playerName", playerName},
        {"score", score},
        {"previousScore", previousScore},
        {"previousRank", previousRank},
        {"lastUpdated", lastUpdated},
        {"region", region},
        {"metadata", metadata}
    };
}

LeaderboardEntry LeaderboardEntry::FromJson(const nlohmann::json& j) {
    LeaderboardEntry e;
    e.rank = j.value("rank", 0);
    e.playerId = j.value("playerId", "");
    e.playerName = j.value("playerName", "Unknown");
    e.score = j.value("score", 0LL);
    e.previousScore = j.value("previousScore", 0LL);
    e.previousRank = j.value("previousRank", 0);
    e.lastUpdated = j.value("lastUpdated", 0LL);
    e.region = j.value("region", "");
    if (j.contains("metadata")) {
        e.metadata = j["metadata"];
    }
    return e;
}

// ============================================================================
// Leaderboard Implementation
// ============================================================================

nlohmann::json Leaderboard::ToJson() const {
    nlohmann::json j;
    j["category"] = static_cast<int>(category);
    j["lastUpdated"] = lastUpdated;
    j["totalPlayers"] = totalPlayers;
    j["highestScore"] = highestScore;
    j["averageScore"] = averageScore;

    j["entries"] = nlohmann::json::array();
    for (const auto& entry : entries) {
        j["entries"].push_back(entry.ToJson());
    }

    return j;
}

Leaderboard Leaderboard::FromJson(const nlohmann::json& j) {
    Leaderboard lb;
    lb.category = static_cast<LeaderboardCategory>(j.value("category", 0));
    lb.lastUpdated = j.value("lastUpdated", 0LL);
    lb.totalPlayers = j.value("totalPlayers", 0);
    lb.highestScore = j.value("highestScore", 0LL);
    lb.averageScore = j.value("averageScore", 0.0f);

    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& entryJson : j["entries"]) {
            lb.entries.push_back(LeaderboardEntry::FromJson(entryJson));
        }
    }

    return lb;
}

const LeaderboardEntry* Leaderboard::GetEntry(const std::string& playerId) const {
    auto it = std::find_if(entries.begin(), entries.end(),
        [&playerId](const LeaderboardEntry& e) { return e.playerId == playerId; });

    return it != entries.end() ? &(*it) : nullptr;
}

std::vector<LeaderboardEntry> Leaderboard::GetRange(int startRank, int count) const {
    std::vector<LeaderboardEntry> result;

    for (const auto& entry : entries) {
        if (entry.rank >= startRank && entry.rank < startRank + count) {
            result.push_back(entry);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
            return a.rank < b.rank;
        });

    return result;
}

std::vector<LeaderboardEntry> Leaderboard::GetTop(int count) const {
    return GetRange(1, count);
}

std::vector<LeaderboardEntry> Leaderboard::GetAroundPlayer(
    const std::string& playerId, int count) const {

    const LeaderboardEntry* myEntry = GetEntry(playerId);
    if (!myEntry) {
        return GetTop(count * 2 + 1);
    }

    int startRank = std::max(1, myEntry->rank - count);
    return GetRange(startRank, count * 2 + 1);
}

// ============================================================================
// PlayerLeaderboardStats Implementation
// ============================================================================

nlohmann::json PlayerLeaderboardStats::ToJson() const {
    return {
        {"playerId", playerId},
        {"playerName", playerName},
        {"region", region},
        {"territoryTiles", territoryTiles},
        {"zombiesKilled", zombiesKilled},
        {"population", population},
        {"wealth", wealth},
        {"survivalHours", survivalHours},
        {"buildingsBuilt", buildingsBuilt},
        {"attacksSurvived", attacksSurvived},
        {"zombiesKilled24h", zombiesKilled24h},
        {"territoryGained7d", territoryGained7d}
    };
}

PlayerLeaderboardStats PlayerLeaderboardStats::FromJson(const nlohmann::json& j) {
    PlayerLeaderboardStats s;
    s.playerId = j.value("playerId", "");
    s.playerName = j.value("playerName", "Unknown");
    s.region = j.value("region", "");
    s.territoryTiles = j.value("territoryTiles", 0);
    s.zombiesKilled = j.value("zombiesKilled", 0);
    s.population = j.value("population", 0);
    s.wealth = j.value("wealth", 0LL);
    s.survivalHours = j.value("survivalHours", 0.0f);
    s.buildingsBuilt = j.value("buildingsBuilt", 0);
    s.attacksSurvived = j.value("attacksSurvived", 0);
    s.zombiesKilled24h = j.value("zombiesKilled24h", 0);
    s.territoryGained7d = j.value("territoryGained7d", 0);
    return s;
}

int64_t PlayerLeaderboardStats::CalculateWealth(int food, int wood, int stone, int metal,
                                                int fuel, int medicine, int ammo) {
    // Resource values for wealth calculation
    return static_cast<int64_t>(food) * 1 +
           static_cast<int64_t>(wood) * 2 +
           static_cast<int64_t>(stone) * 3 +
           static_cast<int64_t>(metal) * 5 +
           static_cast<int64_t>(fuel) * 4 +
           static_cast<int64_t>(medicine) * 6 +
           static_cast<int64_t>(ammo) * 3;
}

// ============================================================================
// LeaderboardAchievement Implementation
// ============================================================================

nlohmann::json LeaderboardAchievement::ToJson() const {
    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"category", static_cast<int>(category)},
        {"requiredRank", requiredRank},
        {"requiredScore", requiredScore},
        {"earned", earned},
        {"earnedTimestamp", earnedTimestamp}
    };
}

LeaderboardAchievement LeaderboardAchievement::FromJson(const nlohmann::json& j) {
    LeaderboardAchievement a;
    a.id = j.value("id", "");
    a.name = j.value("name", "");
    a.description = j.value("description", "");
    a.category = static_cast<LeaderboardCategory>(j.value("category", 0));
    a.requiredRank = j.value("requiredRank", 0);
    a.requiredScore = j.value("requiredScore", 0LL);
    a.earned = j.value("earned", false);
    a.earnedTimestamp = j.value("earnedTimestamp", 0LL);
    return a;
}

// ============================================================================
// LeaderboardManager Implementation
// ============================================================================

LeaderboardManager& LeaderboardManager::Instance() {
    static LeaderboardManager instance;
    return instance;
}

bool LeaderboardManager::Initialize(const LeaderboardConfig& config) {
    if (m_initialized) {
        LB_LOG_WARN("LeaderboardManager already initialized");
        return true;
    }

    m_config = config;
    InitializeAchievements();

    m_initialized = true;
    LB_LOG_INFO("LeaderboardManager initialized");
    return true;
}

void LeaderboardManager::Shutdown() {
    if (!m_initialized) return;

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cachedLeaderboards.clear();
        m_cacheTimestamps.clear();
    }

    m_initialized = false;
    LB_LOG_INFO("LeaderboardManager shutdown complete");
}

void LeaderboardManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Score submission timer
    m_submitTimer += deltaTime;
    if (m_submitTimer >= m_config.scoreSubmitInterval) {
        m_submitTimer = 0.0f;
        ProcessScoreSubmission();
    }

    // Auto-refresh timer
    m_updateTimer += deltaTime;
    if (m_updateTimer >= m_config.updateIntervalSeconds) {
        m_updateTimer = 0.0f;
        // Could trigger background refresh here
    }
}

// ==================== Score Submission ====================

void LeaderboardManager::SubmitStats(const PlayerLeaderboardStats& stats) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingStats = stats;
    m_hasPendingStats = true;
}

void LeaderboardManager::SubmitScore(LeaderboardCategory category, int64_t score,
                                     const nlohmann::json& metadata) {
    if (m_localPlayerId.empty()) {
        LB_LOG_WARN("Cannot submit score: no local player ID");
        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    LeaderboardEntry entry;
    entry.playerId = m_localPlayerId;
    entry.playerName = m_localPlayerName;
    entry.region = m_localRegion;
    entry.score = score;
    entry.lastUpdated = now;
    entry.metadata = metadata;

    // Update local cache
    {
        std::lock_guard<std::mutex> lock(m_myEntriesMutex);
        if (m_myEntries.count(category)) {
            entry.previousScore = m_myEntries[category].score;
            entry.previousRank = m_myEntries[category].rank;
        }
        m_myEntries[category] = entry;
    }

    // Submit to Firebase
    std::string path = GetGlobalLeaderboardPath(category) + "/" + m_localPlayerId;
    FirebaseManager::Instance().SetValue(path, entry.ToJson());

    // Also submit to regional leaderboard if region is set
    if (!m_localRegion.empty()) {
        std::string regionalPath = GetRegionalLeaderboardPath(category, m_localRegion) +
                                   "/" + m_localPlayerId;
        FirebaseManager::Instance().SetValue(regionalPath, entry.ToJson());
    }
}

void LeaderboardManager::SetLocalPlayer(const std::string& playerId, const std::string& playerName,
                                        const std::string& region) {
    m_localPlayerId = playerId;
    m_localPlayerName = playerName;
    m_localRegion = region;
}

// ==================== Leaderboard Queries ====================

void LeaderboardManager::GetLeaderboard(LeaderboardCategory category, LeaderboardCallback callback,
                                        bool forceRefresh) {
    if (!forceRefresh && m_config.enableLocalCache) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);

        auto cacheIt = m_cacheTimestamps.find(category);
        if (cacheIt != m_cacheTimestamps.end()) {
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (now - cacheIt->second < static_cast<int64_t>(m_config.cacheLifetimeSeconds)) {
                auto lbIt = m_cachedLeaderboards.find(category);
                if (lbIt != m_cachedLeaderboards.end()) {
                    if (callback) {
                        callback(lbIt->second);
                    }
                    return;
                }
            }
        }
    }

    // Fetch from Firebase
    FetchLeaderboard(category, GetGlobalLeaderboardPath(category), callback);
}

void LeaderboardManager::GetTopEntries(LeaderboardCategory category, int count,
                                       LeaderboardCallback callback) {
    GetLeaderboard(category, [count, callback](const Leaderboard& lb) {
        Leaderboard filtered = lb;
        filtered.entries = lb.GetTop(count);
        if (callback) {
            callback(filtered);
        }
    });
}

void LeaderboardManager::GetAroundMe(LeaderboardCategory category, LeaderboardCallback callback) {
    if (m_localPlayerId.empty()) {
        GetTopEntries(category, m_config.aroundPlayerCount * 2 + 1, callback);
        return;
    }

    GetLeaderboard(category, [this, callback](const Leaderboard& lb) {
        Leaderboard filtered = lb;
        filtered.entries = lb.GetAroundPlayer(m_localPlayerId, m_config.aroundPlayerCount);
        if (callback) {
            callback(filtered);
        }
    });
}

void LeaderboardManager::GetMyRank(LeaderboardCategory category, RankCallback callback) {
    GetLeaderboard(category, [this, category, callback](const Leaderboard& lb) {
        const LeaderboardEntry* entry = lb.GetEntry(m_localPlayerId);
        int rank = entry ? entry->rank : 0;
        if (callback) {
            callback(category, rank);
        }
    });
}

const LeaderboardEntry* LeaderboardManager::GetMyEntry(LeaderboardCategory category) const {
    std::lock_guard<std::mutex> lock(m_myEntriesMutex);
    auto it = m_myEntries.find(category);
    return it != m_myEntries.end() ? &it->second : nullptr;
}

const Leaderboard* LeaderboardManager::GetCachedLeaderboard(LeaderboardCategory category) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cachedLeaderboards.find(category);
    return it != m_cachedLeaderboards.end() ? &it->second : nullptr;
}

// ==================== Regional Leaderboards ====================

void LeaderboardManager::GetRegionalLeaderboard(LeaderboardCategory category,
                                                const std::string& region,
                                                LeaderboardCallback callback) {
    FetchLeaderboard(category, GetRegionalLeaderboardPath(category, region), callback);
}

void LeaderboardManager::GetMyRegionLeaderboard(LeaderboardCategory category,
                                               LeaderboardCallback callback) {
    if (m_localRegion.empty()) {
        LB_LOG_WARN("No region set for local player");
        return;
    }
    GetRegionalLeaderboard(category, m_localRegion, callback);
}

// ==================== Achievements ====================

std::vector<LeaderboardAchievement> LeaderboardManager::GetAchievements() const {
    std::lock_guard<std::mutex> lock(m_achievementsMutex);
    return m_achievements;
}

std::vector<LeaderboardAchievement> LeaderboardManager::GetEarnedAchievements() const {
    std::lock_guard<std::mutex> lock(m_achievementsMutex);

    std::vector<LeaderboardAchievement> earned;
    for (const auto& a : m_achievements) {
        if (a.earned) {
            earned.push_back(a);
        }
    }
    return earned;
}

void LeaderboardManager::CheckAchievements() {
    std::lock_guard<std::mutex> lock(m_achievementsMutex);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (auto& achievement : m_achievements) {
        if (achievement.earned) continue;

        std::lock_guard<std::mutex> myLock(m_myEntriesMutex);
        auto entryIt = m_myEntries.find(achievement.category);
        if (entryIt == m_myEntries.end()) continue;

        const LeaderboardEntry& myEntry = entryIt->second;
        bool shouldEarn = false;

        if (achievement.requiredRank > 0 && myEntry.rank > 0) {
            shouldEarn = myEntry.rank <= achievement.requiredRank;
        } else if (achievement.requiredScore > 0) {
            shouldEarn = myEntry.score >= achievement.requiredScore;
        }

        if (shouldEarn) {
            achievement.earned = true;
            achievement.earnedTimestamp = now;

            LB_LOG_INFO("Achievement unlocked: " + achievement.name);

            // Notify callbacks
            std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
            for (auto& callback : m_achievementCallbacks) {
                if (callback) {
                    callback(achievement);
                }
            }
        }
    }
}

void LeaderboardManager::OnAchievementUnlocked(AchievementCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_achievementCallbacks.push_back(std::move(callback));
}

// ==================== Statistics ====================

LeaderboardManager::LeaderboardStats LeaderboardManager::GetStats(LeaderboardCategory category) const {
    LeaderboardStats stats{};

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cachedLeaderboards.find(category);
    if (it == m_cachedLeaderboards.end()) {
        return stats;
    }

    const Leaderboard& lb = it->second;
    stats.totalPlayers = lb.totalPlayers;
    stats.highestScore = lb.highestScore;
    stats.averageScore = lb.averageScore;

    if (!lb.entries.empty()) {
        stats.lowestScore = lb.entries.back().score;

        // Find median
        if (lb.entries.size() > 0) {
            size_t mid = lb.entries.size() / 2;
            stats.medianScore = lb.entries[mid].score;
        }
    }

    // Get my stats
    const LeaderboardEntry* myEntry = lb.GetEntry(m_localPlayerId);
    if (myEntry) {
        stats.myRank = myEntry->rank;
        stats.myScore = myEntry->score;
        stats.percentile = (1.0f - static_cast<float>(myEntry->rank) /
                           std::max(1, lb.totalPlayers)) * 100.0f;
    }

    return stats;
}

// ==================== Configuration ====================

void LeaderboardManager::RefreshAll() {
    for (int i = 0; i < static_cast<int>(LeaderboardCategory::Count); ++i) {
        LeaderboardCategory cat = static_cast<LeaderboardCategory>(i);
        GetLeaderboard(cat, nullptr, true);
    }
}

// ==================== Callbacks ====================

void LeaderboardManager::OnRankChanged(RankCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_rankCallbacks.push_back(std::move(callback));
}

// ==================== Private Methods ====================

std::string LeaderboardManager::GetGlobalLeaderboardPath(LeaderboardCategory category) const {
    return "rts/leaderboards/global/" + std::string(LeaderboardCategoryToString(category));
}

std::string LeaderboardManager::GetRegionalLeaderboardPath(LeaderboardCategory category,
                                                          const std::string& region) const {
    return "rts/leaderboards/regional/" + region + "/" +
           std::string(LeaderboardCategoryToString(category));
}

std::string LeaderboardManager::GetPlayerScorePath(const std::string& playerId) const {
    return "rts/players/" + playerId + "/scores";
}

void LeaderboardManager::FetchLeaderboard(LeaderboardCategory category, const std::string& path,
                                         LeaderboardCallback callback) {
    FirebaseManager::Instance().GetValue(path,
        [this, category, callback](const nlohmann::json& data) {
            Leaderboard lb;
            lb.category = category;
            lb.lastUpdated = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (!data.is_null() && !data.empty()) {
                // Parse entries from Firebase data
                for (auto& [key, value] : data.items()) {
                    LeaderboardEntry entry = LeaderboardEntry::FromJson(value);
                    entry.playerId = key;
                    lb.entries.push_back(entry);
                }

                // Sort by score (descending)
                std::sort(lb.entries.begin(), lb.entries.end(),
                    [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
                        return a.score > b.score;
                    });

                // Assign ranks
                for (size_t i = 0; i < lb.entries.size(); ++i) {
                    lb.entries[i].rank = static_cast<int>(i + 1);
                }

                // Calculate stats
                lb.totalPlayers = static_cast<int>(lb.entries.size());
                if (!lb.entries.empty()) {
                    lb.highestScore = lb.entries.front().score;

                    int64_t totalScore = 0;
                    for (const auto& e : lb.entries) {
                        totalScore += e.score;
                    }
                    lb.averageScore = static_cast<float>(totalScore) / lb.entries.size();
                }
            }

            // Cache result
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_cachedLeaderboards[category] = lb;
                m_cacheTimestamps[category] = lb.lastUpdated;
            }

            // Update local player entry
            UpdateLocalRanks();

            if (callback) {
                callback(lb);
            }
        });
}

void LeaderboardManager::ProcessScoreSubmission() {
    PlayerLeaderboardStats stats;
    bool hasStats = false;

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_hasPendingStats) {
            stats = m_pendingStats;
            hasStats = true;
            m_hasPendingStats = false;
        }
    }

    if (!hasStats) return;

    // Submit to each relevant leaderboard
    for (int i = 0; i < static_cast<int>(LeaderboardCategory::Count); ++i) {
        LeaderboardCategory cat = static_cast<LeaderboardCategory>(i);
        int64_t score = GetScoreForCategory(stats, cat);

        if (score > 0) {
            nlohmann::json metadata = stats.ToJson();
            SubmitScore(cat, score, metadata);
        }
    }

    // Check achievements after submission
    CheckAchievements();
}

void LeaderboardManager::UpdateLocalRanks() {
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
    std::lock_guard<std::mutex> myLock(m_myEntriesMutex);

    for (const auto& [category, lb] : m_cachedLeaderboards) {
        const LeaderboardEntry* myEntry = lb.GetEntry(m_localPlayerId);
        if (myEntry) {
            // Check for rank change
            auto it = m_myEntries.find(category);
            if (it != m_myEntries.end()) {
                int oldRank = it->second.rank;
                if (oldRank != myEntry->rank && oldRank > 0) {
                    // Notify rank change
                    std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
                    for (auto& callback : m_rankCallbacks) {
                        if (callback) {
                            callback(category, myEntry->rank);
                        }
                    }
                }
            }

            m_myEntries[category] = *myEntry;
        }
    }
}

void LeaderboardManager::InitializeAchievements() {
    std::lock_guard<std::mutex> lock(m_achievementsMutex);
    m_achievements.clear();

    // Territory achievements
    m_achievements.push_back({"territory_10", "Small Landowner", "Control 10 tiles",
        LeaderboardCategory::TerritoryControlled, 0, 10, false, 0});
    m_achievements.push_back({"territory_100", "Land Baron", "Control 100 tiles",
        LeaderboardCategory::TerritoryControlled, 0, 100, false, 0});
    m_achievements.push_back({"territory_top10", "Territory Elite", "Reach top 10 in territory",
        LeaderboardCategory::TerritoryControlled, 10, 0, false, 0});

    // Zombie killing achievements
    m_achievements.push_back({"kills_100", "Zombie Hunter", "Kill 100 zombies",
        LeaderboardCategory::ZombiesKilled, 0, 100, false, 0});
    m_achievements.push_back({"kills_1000", "Zombie Slayer", "Kill 1000 zombies",
        LeaderboardCategory::ZombiesKilled, 0, 1000, false, 0});
    m_achievements.push_back({"kills_top10", "Kill Leader", "Reach top 10 in zombie kills",
        LeaderboardCategory::ZombiesKilled, 10, 0, false, 0});

    // Population achievements
    m_achievements.push_back({"pop_10", "Small Settlement", "Have 10 workers",
        LeaderboardCategory::Population, 0, 10, false, 0});
    m_achievements.push_back({"pop_50", "Thriving Town", "Have 50 workers",
        LeaderboardCategory::Population, 0, 50, false, 0});

    // Wealth achievements
    m_achievements.push_back({"wealth_1000", "Getting By", "Accumulate 1000 wealth",
        LeaderboardCategory::Wealth, 0, 1000, false, 0});
    m_achievements.push_back({"wealth_10000", "Wealthy", "Accumulate 10000 wealth",
        LeaderboardCategory::Wealth, 0, 10000, false, 0});
    m_achievements.push_back({"wealth_top10", "Economic Elite", "Reach top 10 in wealth",
        LeaderboardCategory::Wealth, 10, 0, false, 0});

    // Survival achievements
    m_achievements.push_back({"survive_24", "Day Survivor", "Survive 24 hours",
        LeaderboardCategory::SurvivalTime, 0, 24, false, 0});
    m_achievements.push_back({"survive_168", "Week Survivor", "Survive a week",
        LeaderboardCategory::SurvivalTime, 0, 168, false, 0});

    // Defense achievements
    m_achievements.push_back({"defense_10", "Defender", "Survive 10 attacks",
        LeaderboardCategory::AttacksSurvived, 0, 10, false, 0});
    m_achievements.push_back({"defense_100", "Fortress", "Survive 100 attacks",
        LeaderboardCategory::AttacksSurvived, 0, 100, false, 0});

    // Rank 1 achievements
    m_achievements.push_back({"rank1_territory", "Territory Champion", "Reach #1 in territory",
        LeaderboardCategory::TerritoryControlled, 1, 0, false, 0});
    m_achievements.push_back({"rank1_kills", "Kill Champion", "Reach #1 in zombie kills",
        LeaderboardCategory::ZombiesKilled, 1, 0, false, 0});
    m_achievements.push_back({"rank1_wealth", "Wealth Champion", "Reach #1 in wealth",
        LeaderboardCategory::Wealth, 1, 0, false, 0});
}

int64_t LeaderboardManager::GetScoreForCategory(const PlayerLeaderboardStats& stats,
                                                LeaderboardCategory category) const {
    switch (category) {
        case LeaderboardCategory::TerritoryControlled:
            return stats.territoryTiles;
        case LeaderboardCategory::ZombiesKilled:
            return stats.zombiesKilled;
        case LeaderboardCategory::Population:
            return stats.population;
        case LeaderboardCategory::Wealth:
            return stats.wealth;
        case LeaderboardCategory::SurvivalTime:
            return static_cast<int64_t>(stats.survivalHours);
        case LeaderboardCategory::BuildingsConstructed:
            return stats.buildingsBuilt;
        case LeaderboardCategory::AttacksSurvived:
            return stats.attacksSurvived;
        case LeaderboardCategory::DailyZombieKills:
            return stats.zombiesKilled24h;
        case LeaderboardCategory::WeeklyTerritory:
            return stats.territoryGained7d;
        default:
            return 0;
    }
}

} // namespace Vehement
